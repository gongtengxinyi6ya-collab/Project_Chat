// tools/load_test_async.cpp
// g++ -O2 -std=c++17 -pthread tools/load_test_async.cpp -o build/load_test_async
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "third_party/json.hpp"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

struct Args {
    std::string host = "127.0.0.1";
    int port = 8080;
    int clients = 20;
    int durationSec = 60;
    double ratePerClient = 5.0;
    std::string groupId = "Group1";
    std::string out = "result.json";
    int timeoutMs = 3000;
};

struct Metrics {
    std::atomic<uint64_t> sent{0}, ok{0}, dropped{0};
    std::atomic<uint64_t> timeout{0}, sendFail{0}, recvFail{0}, parseFail{0}, serverErrResp{0}, unmatchedResp{0};

    std::mutex latMu;
    std::vector<double> latMs;
} gM;

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

static std::string makeReq(int type, uint64_t reqId, uint64_t seq,
                           const std::string& from, const std::string& to, const json& body) {
    json j{{"ver",1},{"type",type},{"req_id",reqId},{"seq",seq},{"from",from},{"to",to}};
    for (auto& [k,v] : body.items()) j[k] = v;
    return j.dump();
}

struct Worker {
    int idx;
    Args a;
    bool creator;
    int fd{-1};
    std::string user;
    std::atomic<bool> running{true};
    std::atomic<bool> sendDone{false};

    std::mutex pmu;
    std::unordered_map<uint64_t, Clock::time_point> pending; // req_id -> send time

    uint64_t reqId{1}, seq{1};

    Worker(int i, const Args& args, bool c): idx(i), a(args), creator(c), user("u"+std::to_string(i)) {}

    uint64_t nextReq() { return reqId++; }
    uint64_t nextSeq() { return seq++; }

    bool sendJson(int type, const json& body, const std::string& to = "") {
        std::string p = makeReq(type, nextReq(), nextSeq(), user, to, body);
        return sendFrame(fd, p);
    }

    void recvLoop() {
        std::string resp;
        while (running.load()) {
            if (!recvFrame(fd, resp)) { gM.recvFail++; break; }

            if (resp == "PING") {
                if (!sendFrame(fd, "PONG")) gM.sendFail++;
                continue;
            }

            json jr;
            try { jr = json::parse(resp); }
            catch (...) { gM.parseFail++; continue; }

            int type = jr.value("type", -1);
            uint64_t rid = jr.value("req_id", 0ULL);

            if (type == 22) { // GROUP_MSG_RESP
                Clock::time_point t0;
                bool matched = false;
                {
                    std::lock_guard<std::mutex> lk(pmu);
                    auto it = pending.find(rid);
                    if (it != pending.end()) {
                        t0 = it->second;
                        pending.erase(it);
                        matched = true;
                    }
                }
                if (!matched) { gM.unmatchedResp++; continue; }

                bool ok = jr.value("ok", false);
                auto data = jr.value("data", json::object());
                uint64_t dropped = data.value("dropped", 0);

                if (ok) {
                    gM.ok++;
                    gM.dropped += dropped;
                    double lat = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
                    std::lock_guard<std::mutex> lk(gM.latMu);
                    gM.latMs.push_back(lat);
                } else {
                    gM.serverErrResp++;
                }
                continue;
            }

            if (type == 23) { // GROUP_MSG_PUSH
                continue; // 正常广播推送，不算错误
            }

            if (type == 255) {
                gM.serverErrResp++;
            }
        }
        running.store(false);
    }

    void timeoutSweepLoop() {
        using namespace std::chrono_literals;
        while (running.load()) {
            std::this_thread::sleep_for(100ms);
            auto now = Clock::now();
            std::vector<uint64_t> expired;
            {
                std::lock_guard<std::mutex> lk(pmu);
                for (auto& kv : pending) {
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - kv.second).count();
                    if (ms > a.timeoutMs) expired.push_back(kv.first);
                }
                for (auto rid : expired) pending.erase(rid);
            }
            gM.timeout += expired.size();

            if (sendDone.load()) {
                std::lock_guard<std::mutex> lk(pmu);
                if (pending.empty()) break;
            }
        }
    }

    bool handshake() {
        // AUTH_REQ=1
        if (!sendJson(1, json{{"user",user}})) return false;
        std::string r;
        if (!recvFrame(fd, r)) return false;

        // CREATE_GROUP_REQ=10
        if (creator) {
            if (!sendJson(10, json{{"name", a.groupId}})) return false;
            if (!recvFrame(fd, r)) return false;
        }

        // JOIN_GROUP_REQ=12
        if (!sendJson(12, json{{"groupId", a.groupId}})) return false;
        if (!recvFrame(fd, r)) return false;
        return true;
    }

    void sendLoop() {
        std::mt19937_64 rng((uint64_t)idx * 1145141ULL);
        auto interval = std::chrono::duration<double>(1.0 / std::max(0.1, a.ratePerClient));
        auto endAt = Clock::now() + std::chrono::seconds(a.durationSec);

        while (running.load() && Clock::now() < endAt) {
            uint64_t rid = nextReq();
            std::string content = "hello-" + user + "-" + std::to_string(rng() % 100000);

            std::string p = makeReq(
                21, rid, nextSeq(), user, "",
                json{{"groupId", a.groupId}, {"content", content}}
            );

            {
                std::lock_guard<std::mutex> lk(pmu);
                pending[rid] = Clock::now();
            }

            if (!sendFrame(fd, p)) {
                gM.sendFail++;
                std::lock_guard<std::mutex> lk(pmu);
                pending.erase(rid);
                break;
            }

            gM.sent++;
            std::this_thread::sleep_for(interval);
        }
        sendDone.store(true);
    }

    void run() {
        fd = connectTo(a.host, a.port);
        if (fd < 0) { gM.sendFail++; return; }

        if (!handshake()) {
            gM.recvFail++;
            ::close(fd);
            return;
        }

        std::thread tr(&Worker::recvLoop, this);
        std::thread tt(&Worker::timeoutSweepLoop, this);

        sendLoop();

        // drain 1.5s
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        running.store(false);

        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);

        if (tr.joinable()) tr.join();
        if (tt.joinable()) tt.join();
    }
};

static double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t idx = (size_t)(p * (v.size() - 1));
    return v[idx];
}

int main(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        if (k == "--host" && i+1<argc) a.host = argv[++i];
        else if (k == "--port" && i+1<argc) a.port = std::stoi(argv[++i]);
        else if (k == "--clients" && i+1<argc) a.clients = std::stoi(argv[++i]);
        else if (k == "--duration" && i+1<argc) a.durationSec = std::stoi(argv[++i]);
        else if (k == "--rate" && i+1<argc) a.ratePerClient = std::stod(argv[++i]);
        else if (k == "--group" && i+1<argc) a.groupId = argv[++i];
        else if (k == "--out" && i+1<argc) a.out = argv[++i];
        else if (k == "--timeout_ms" && i+1<argc) a.timeoutMs = std::stoi(argv[++i]);
    }

    auto t0 = Clock::now();
    std::vector<std::thread> ts;
    ts.reserve(a.clients);

    for (int i = 0; i < a.clients; ++i) {
        ts.emplace_back([&, i] {
            Worker w(i+1, a, i==0);
            w.run();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    for (auto& t : ts) t.join();

    double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();

    std::vector<double> lats;
    {
        std::lock_guard<std::mutex> lk(gM.latMu);
        lats = gM.latMs;
    }

    uint64_t errTotal = gM.timeout + gM.sendFail + gM.recvFail + gM.parseFail + gM.serverErrResp + gM.unmatchedResp;
    double qps = elapsed > 0 ? (double)gM.ok.load() / elapsed : 0.0;

    json out{
        {"clients", a.clients},
        {"duration_s", a.durationSec},
        {"rate_per_client", a.ratePerClient},
        {"group", a.groupId},
        {"elapsed_s", elapsed},

        {"sent", gM.sent.load()},
        {"ok", gM.ok.load()},
        {"dropped", gM.dropped.load()},
        {"qps_ok", qps},

        {"timeout", gM.timeout.load()},
        {"send_fail", gM.sendFail.load()},
        {"recv_fail", gM.recvFail.load()},
        {"parse_fail", gM.parseFail.load()},
        {"server_err_resp", gM.serverErrResp.load()},
        {"unmatched_resp", gM.unmatchedResp.load()},
        {"err_total", errTotal},

        {"p50_ms", percentile(lats, 0.50)},
        {"p95_ms", percentile(lats, 0.95)},
        {"p99_ms", percentile(lats, 0.99)}
    };

    std::ofstream ofs(a.out);
    ofs << out.dump(2) << "\n";
    std::cout << out.dump(2) << std::endl;
    return 0;
}
