// tools/load_test.cpp
// build: g++ -O2 -std=c++17 -pthread -Iinclude tools/load_test.cpp -o build/load_test
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
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
    int timeoutMs = 10000;
    int drainMs = 3000;
};

struct Metrics {
    std::atomic<uint64_t> sent{0}, ok{0}, dropped{0};
    std::atomic<uint64_t> timeout{0}, sendFail{0}, recvFail{0};
    std::atomic<uint64_t> parseFail{0}, serverErrResp{0}, lateResp{0}, pushRecv{0};
    std::mutex latMu;
    std::vector<double> latMs;
} g;

static bool sendAll(int fd, const void* data, size_t len) {
    const char* p = static_cast<const char*>(data);
    while (len > 0) {
        ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
        if (n > 0) { p += n; len -= static_cast<size_t>(n); continue; }
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

static bool recvAll(int fd, void* data, size_t len) {
    char* p = static_cast<char*>(data);
    while (len > 0) {
        ssize_t n = ::recv(fd, p, len, 0);
        if (n > 0) { p += n; len -= static_cast<size_t>(n); continue; }
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

static bool sendFrame(int fd, const std::string& payload) {
    uint32_t n = htonl(static_cast<uint32_t>(payload.size()));
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

static int connectTo(const Args& a) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(a.port));
    if (::inet_pton(AF_INET, a.host.c_str(), &addr.sin_addr) != 1) { ::close(fd); return -1; }
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) { ::close(fd); return -1; }
    return fd;
}

static std::string makeReq(int type, uint64_t reqId, uint64_t seq,
                           const std::string& from, const std::string& to, const json& body) {
    json j{{"ver",1},{"type",type},{"req_id",reqId},{"seq",seq},{"from",from},{"to",to}};
    for (auto& item : body.items()) j[item.key()] = item.value();
    return j.dump();
}

struct Worker {
    int idx;
    Args args;
    int fd{-1};
    std::string user;
    std::atomic<bool> running{true};
    std::atomic<bool> sendingDone{false};
    uint64_t reqId{1};
    uint64_t seq{1};

    std::mutex pendingMu;
    std::unordered_map<uint64_t, Clock::time_point> pending;

    Worker(int i, const Args& a) : idx(i), args(a), user("u" + std::to_string(i)) {}

    uint64_t nextReq() { return reqId++; }
    uint64_t nextSeq() { return seq++; }

    bool sendJson(int type, const json& body) {
        return sendFrame(fd, makeReq(type, nextReq(), nextSeq(), user, "", body));
    }

    bool waitResp(int expectedType) {
        std::string frame;
        auto deadline = Clock::now() + std::chrono::seconds(5);
        while (Clock::now() < deadline) {
            if (!recvFrame(fd, frame)) return false;
            if (frame == "PING") { sendFrame(fd, "PONG"); continue; }
            json j;
            try { j = json::parse(frame); } catch (...) { continue; }
            if (j.value("type", -1) == expectedType && j.value("ok", false)) return true;
            if (j.value("type", -1) == 255) return false;
        }
        return false;
    }

    bool handshake(bool creator) {
        if (!sendJson(1, json{{"user", user}}) || !waitResp(2)) return false;
        if (creator && (!sendJson(10, json{{"name", args.groupId}}) || !waitResp(11))) return false;
        if (!sendJson(12, json{{"groupId", args.groupId}}) || !waitResp(13)) return false;
        return true;
    }

    void recvLoop() {
        std::string frame;
        while (running.load()) {
            if (!recvFrame(fd, frame)) {
                if (running.load()) g.recvFail++;
                break;
            }
            if (frame == "PING") {
                if (!sendFrame(fd, "PONG")) g.sendFail++;
                continue;
            }

            json j;
            try { j = json::parse(frame); } catch (...) { g.parseFail++; continue; }

            int type = j.value("type", -1);
            uint64_t rid = j.value("req_id", 0ULL);

            if (type == 23) {
                g.pushRecv++;
                continue;
            }
            if (type == 255) {
                g.serverErrResp++;
                continue;
            }
            if (type != 22) {
                continue;
            }

            Clock::time_point start;
            bool matched = false;
            {
                std::lock_guard<std::mutex> lk(pendingMu);
                auto it = pending.find(rid);
                if (it != pending.end()) {
                    start = it->second;
                    pending.erase(it);
                    matched = true;
                }
            }

            if (!matched) {
                g.lateResp++;
                continue;
            }

            if (!j.value("ok", false)) {
                g.serverErrResp++;
                continue;
            }

            auto data = j.value("data", json::object());
            g.ok++;
            g.dropped += static_cast<uint64_t>(data.value("dropped", 0));

            double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            std::lock_guard<std::mutex> lk(g.latMu);
            g.latMs.push_back(ms);
        }
    }

    void timeoutLoop() {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto now = Clock::now();
            uint64_t expired = 0;
            {
                std::lock_guard<std::mutex> lk(pendingMu);
                for (auto it = pending.begin(); it != pending.end();) {
                    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
                    if (age > args.timeoutMs) {
                        it = pending.erase(it);
                        expired++;
                    } else {
                        ++it;
                    }
                }
            }
            g.timeout += expired;

            if (sendingDone.load()) {
                std::lock_guard<std::mutex> lk(pendingMu);
                if (pending.empty()) break;
            }
        }
    }

    void sendLoop() {
        std::mt19937_64 rng(static_cast<uint64_t>(idx) * 1315423911ULL);
        auto interval = std::chrono::duration<double>(1.0 / std::max(0.1, args.ratePerClient));
        auto end = Clock::now() + std::chrono::seconds(args.durationSec);

        while (running.load() && Clock::now() < end) {
            uint64_t rid = nextReq();
            std::string content = "hello-" + user + "-" + std::to_string(rng() % 100000);
            std::string payload = makeReq(21, rid, nextSeq(), user, "",
                                          json{{"groupId", args.groupId}, {"content", content}});
            {
                std::lock_guard<std::mutex> lk(pendingMu);
                pending[rid] = Clock::now();
            }

            if (!sendFrame(fd, payload)) {
                g.sendFail++;
                std::lock_guard<std::mutex> lk(pendingMu);
                pending.erase(rid);
                break;
            }

            g.sent++;
            std::this_thread::sleep_for(interval);
        }
        sendingDone.store(true);
    }

    void run(bool creator) {
        fd = connectTo(args);
        if (fd < 0) { g.sendFail++; return; }

        if (!handshake(creator)) {
            g.recvFail++;
            ::close(fd);
            return;
        }

        std::thread receiver(&Worker::recvLoop, this);
        std::thread timeoutSweeper(&Worker::timeoutLoop, this);

        sendLoop();

        std::this_thread::sleep_for(std::chrono::milliseconds(args.drainMs));
        running.store(false);
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);

        if (receiver.joinable()) receiver.join();
        if (timeoutSweeper.joinable()) timeoutSweeper.join();
    }
};

static double pct(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[static_cast<size_t>(p * (v.size() - 1))];
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
        else if (k == "--timeout_ms" && i + 1 < argc) a.timeoutMs = std::stoi(argv[++i]);
        else if (k == "--drain_ms" && i + 1 < argc) a.drainMs = std::stoi(argv[++i]);
    }

    auto t0 = Clock::now();
    std::vector<std::thread> threads;
    threads.reserve(a.clients);

    for (int i = 0; i < a.clients; ++i) {
        threads.emplace_back([&, i] {
            Worker w(i + 1, a);
            w.run(i == 0);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (auto& t : threads) t.join();

    double elapsed = std::chrono::duration<double>(Clock::now() - t0).count();

    std::vector<double> lat;
    {
        std::lock_guard<std::mutex> lk(g.latMu);
        lat = g.latMs;
    }

    uint64_t errTotal = g.timeout + g.sendFail + g.recvFail + g.parseFail + g.serverErrResp;
    double qps = elapsed > 0 ? static_cast<double>(g.ok.load()) / elapsed : 0.0;

    json out{
        {"clients", a.clients},
        {"duration_s", a.durationSec},
        {"rate_per_client", a.ratePerClient},
        {"group", a.groupId},
        {"elapsed_s", elapsed},
        {"sent", g.sent.load()},
        {"ok", g.ok.load()},
        {"dropped", g.dropped.load()},
        {"qps_ok", qps},
        {"timeout", g.timeout.load()},
        {"send_fail", g.sendFail.load()},
        {"recv_fail", g.recvFail.load()},
        {"parse_fail", g.parseFail.load()},
        {"server_err_resp", g.serverErrResp.load()},
        {"late_resp", g.lateResp.load()},
        {"push_recv", g.pushRecv.load()},
        {"err_total", errTotal},
        {"p50_ms", pct(lat, 0.50)},
        {"p95_ms", pct(lat, 0.95)},
        {"p99_ms", pct(lat, 0.99)}
    };

    std::ofstream ofs(a.out);
    ofs << out.dump(2) << "\n";
    std::cout << out.dump(2) << std::endl;
    return 0;
}
