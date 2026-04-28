// load_test.cpp
// g++ -O2 -std=c++17 -pthread load_test.cpp -o load_test
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

#include "third_party/json.hpp"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

struct Metrics {
    std::atomic<uint64_t> sent{0}, ok{0}, err{0}, dropped{0};
    std::mutex latMu;
    std::vector<double> latMs;
} g_metrics;

static bool sendAll(int fd, const void* data, size_t len) {
    const char* p = static_cast<const char*>(data);
    while (len > 0) {
        ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
        if (n > 0) { p += n; len -= (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

static bool recvAll(int fd, void* data, size_t len) {
    char* p = static_cast<char*>(data);
    while (len > 0) {
        ssize_t n = ::recv(fd, p, len, 0);
        if (n > 0) { p += n; len -= (size_t)n; continue; }
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

static bool sendFrame(int fd, const std::string& payload) {
    uint32_t n = htonl((uint32_t)payload.size());
    return sendAll(fd, &n, 4) && sendAll(fd, payload.data(), payload.size());
}

static bool recvFrame(int fd, std::string& out) {
    uint32_t n = 0;
    if (!recvAll(fd, &n, 4)) return false;
    n = ntohl(n);
    if (n == 0 || n > 8 * 1024 * 1024) return false;
    out.resize(n);
    return recvAll(fd, out.data(), n);
}

struct Args {
    std::string host = "127.0.0.1";
    int port = 8080;
    int clients = 20;
    int durationSec = 60;
    double ratePerClient = 5.0;
    std::string groupId = "Group1";
    std::string out = "result.json";
};

static int connectTo(const std::string& host, int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) { ::close(fd); return -1; }
    if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) { ::close(fd); return -1; }
    return fd;
}

static std::string makeReq(int type, uint64_t reqId, uint64_t seq, const std::string& from,
                           const std::string& to, const json& body) {
    json j{
        {"ver",1}, {"type",type}, {"req_id",reqId}, {"seq",seq},
        {"from",from}, {"to",to}
    };
    for (auto& [k,v] : body.items()) j[k] = v;
    return j.dump();
}

static void clientWorker(int idx, const Args& a, bool creator) {
    int fd = connectTo(a.host, a.port);
    if (fd < 0) { g_metrics.err++; return; }

    std::string user = "u" + std::to_string(idx);
    uint64_t reqId = 1, seq = 1;

    auto sendReq = [&](int type, const json& body, const std::string& to = "") -> bool {
        std::string p = makeReq(type, reqId++, seq++, user, to, body);
        return sendFrame(fd, p);
    };

    std::string resp;
    // AUTH_REQ=1
    if (!sendReq(1, json{{"user",user}}) || !recvFrame(fd, resp)) { g_metrics.err++; ::close(fd); return; }

    // CREATE_GROUP_REQ=10 (only creator)
    if (creator) {
        if (!sendReq(10, json{{"name", a.groupId}}) || !recvFrame(fd, resp)) { g_metrics.err++; ::close(fd); return; }
    }

    // JOIN_GROUP_REQ=12
    if (!sendReq(12, json{{"groupId", a.groupId}}) || !recvFrame(fd, resp)) { g_metrics.err++; ::close(fd); return; }

    const auto endAt = Clock::now() + std::chrono::seconds(a.durationSec);
    const auto interval = std::chrono::duration<double>(1.0 / std::max(0.1, a.ratePerClient));

    std::mt19937_64 rng((uint64_t)idx * 1315423911ULL);
    while (Clock::now() < endAt) {
        uint64_t myReq = reqId;
        std::string content = "hello-" + user + "-" + std::to_string(rng() % 100000);
        std::string p = makeReq(21, reqId++, seq++, user, "", json{{"groupId", a.groupId}, {"content", content}});

        auto t0 = Clock::now();
        if (!sendFrame(fd, p)) { g_metrics.err++; break; }
        g_metrics.sent++;

        // 读到自己的 GROUP_MSG_RESP(type=22, req_id=myReq)
        bool got = false;
        for (int guard = 0; guard < 32; ++guard) {
            if (!recvFrame(fd, resp)) { g_metrics.err++; goto done; }

            if (resp == "PING") { sendFrame(fd, "PONG"); continue; }

            json jr;
            try { jr = json::parse(resp); } catch (...) { continue; }

            int t = jr.value("type", -1);
            uint64_t rid = jr.value("req_id", 0ULL);

            if (t == 22 && rid == myReq) {
                bool ok = jr.value("ok", false);
                auto t1 = Clock::now();
                double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                if (ok) {
                    g_metrics.ok++;
                    auto data = jr.value("data", json::object());
                    g_metrics.dropped += (uint64_t)data.value("dropped", 0);
                    std::lock_guard<std::mutex> lk(g_metrics.latMu);
                    g_metrics.latMs.push_back(ms);
                } else {
                    g_metrics.err++;
                }
                got = true;
                break;
            }
        }
        if (!got) g_metrics.err++;

        std::this_thread::sleep_for(interval);
    }

done:
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
}

static double pct(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t idx = (size_t)(p * (v.size() - 1));
    return v[idx];
}

int main(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        if (k == "--host" && i + 1 < argc) a.host = argv[++i];
        else if (k == "--port" && i + 1 < argc) a.port = std::stoi(argv[++i]);
        else if (k == "--clients" && i + 1 < argc) a.clients = std::stoi(argv[++i]);
        else if (k == "--duration" && i + 1 < argc) a.durationSec = std::stoi(argv[++i]);
        else if (k == "--rate" && i + 1 < argc) a.ratePerClient = std::stod(argv[++i]);
        else if (k == "--group" && i + 1 < argc) a.groupId = argv[++i];
        else if (k == "--out" && i + 1 < argc) a.out = argv[++i];
    }

    auto t0 = Clock::now();
    std::vector<std::thread> ts;
    ts.reserve(a.clients);

    for (int i = 0; i < a.clients; ++i) {
        ts.emplace_back(clientWorker, i + 1, std::cref(a), i == 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    for (auto& t : ts) t.join();

    auto elapsed = std::chrono::duration<double>(Clock::now() - t0).count();

    std::vector<double> lats;
    {
        std::lock_guard<std::mutex> lk(g_metrics.latMu);
        lats = g_metrics.latMs;
    }

    double p50 = pct(lats, 0.50), p95 = pct(lats, 0.95), p99 = pct(lats, 0.99);
    double qps = elapsed > 0 ? (double)g_metrics.ok.load() / elapsed : 0.0;

    json out{
        {"elapsed_s", elapsed},
        {"sent", g_metrics.sent.load()},
        {"ok", g_metrics.ok.load()},
        {"err", g_metrics.err.load()},
        {"dropped", g_metrics.dropped.load()},
        {"qps_ok", qps},
        {"p50_ms", p50},
        {"p95_ms", p95},
        {"p99_ms", p99},
        {"clients", a.clients},
        {"duration_s", a.durationSec},
        {"rate_per_client", a.ratePerClient},
        {"group", a.groupId}
    };

    std::ofstream ofs(a.out);
    ofs << out.dump(2) << "\n";

    std::cout << out.dump(2) << std::endl;
    return 0;
}
