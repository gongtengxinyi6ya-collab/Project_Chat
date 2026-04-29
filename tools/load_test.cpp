#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "third_party/json.hpp"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

enum MsgType {
    AUTH_REQ = 1,
    AUTH_RESP = 2,

    CREATE_GROUP_REQ = 10,
    CREATE_GROUP_RESP = 11,
    JOIN_GROUP_REQ = 12,
    JOIN_GROUP_RESP = 13,

    GROUP_EVENT_PUSH = 20,
    GROUP_MSG_REQ = 21,
    GROUP_MSG_RESP = 22,
    GROUP_MSG_PUSH = 23,

    ERR = 255
};

struct Args {
    std::string host = "127.0.0.1";
    int port = 8080;
    int clients = 20;
    int durationSec = 60;
    double ratePerClient = 5.0;
    int timeoutMs = 10000;
    int drainMs = 3000;
    std::string groupName = "Group1";
    std::string groupId;
};

struct Metrics {
    std::atomic<uint64_t> sent{0};
    std::atomic<uint64_t> ok{0};
    std::atomic<uint64_t> timeout{0};
    std::atomic<uint64_t> lateResp{0};
    std::atomic<uint64_t> pushRecv{0};
    std::atomic<uint64_t> parseFail{0};
    std::atomic<uint64_t> serverErrResp{0};
    std::atomic<uint64_t> connectFail{0};
    std::atomic<uint64_t> authFail{0};
    std::atomic<uint64_t> joinFail{0};
    std::atomic<uint64_t> recvFail{0};
    std::atomic<uint64_t> sendFail{0};
    std::atomic<uint64_t> droppedByServer{0};

    std::mutex latMu;
    std::vector<double> latenciesMs;
};

bool sendAll(int fd, const void* data, size_t len) {
    const char* p = static_cast<const char*>(data);

    while (len > 0) {
        ssize_t n = ::send(fd, p, len, 0);

        if (n > 0) {
            p += n;
            len -= static_cast<size_t>(n);
            continue;
        }

        if (n < 0 && errno == EINTR) {
            continue;
        }

        return false;
    }

    return true;
}

bool recvAll(int fd, void* data, size_t len) {
    char* p = static_cast<char*>(data);

    while (len > 0) {
        ssize_t n = ::recv(fd, p, len, 0);

        if (n > 0) {
            p += n;
            len -= static_cast<size_t>(n);
            continue;
        }

        if (n < 0 && errno == EINTR) {
            continue;
        }

        return false;
    }

    return true;
}

bool sendFrame(int fd, const std::string& payload) {
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    return sendAll(fd, &len, sizeof(len)) && sendAll(fd, payload.data(), payload.size());
}

bool recvFrame(int fd, std::string& payload) {
    uint32_t netLen = 0;

    if (!recvAll(fd, &netLen, sizeof(netLen))) {
        return false;
    }

    uint32_t len = ntohl(netLen);

    if (len == 0 || len > 1024 * 1024) {
        return false;
    }

    payload.assign(len, '\0');
    return recvAll(fd, payload.data(), len);
}

int connectTo(const Args& args) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(args.port);

    if (::inet_pton(AF_INET, args.host.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        return -1;
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }

    return fd;
}

json makeReq(int type, uint64_t reqId, const std::string& user) {
    return json{
        {"ver", 1},
        {"type", type},
        {"req_id", reqId},
        {"seq", reqId},
        {"from", user},
        {"user", user},
        {"to", ""}
    };
}

bool sendReq(int fd, const json& req) {
    return sendFrame(fd, req.dump());
}

bool waitResp(int fd, int expectedType, uint64_t expectedReqId, json& out, int timeoutMs) {
    auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);

    while (Clock::now() < deadline) {
        std::string frame;

        if (!recvFrame(fd, frame)) {
            return false;
        }

        if (frame == "PING") {
            sendFrame(fd, "PONG");
            continue;
        }

        json msg = json::parse(frame, nullptr, false);

        if (msg.is_discarded()) {
            continue;
        }

        int type = msg.value("type", 0);
        uint64_t reqId = msg.value("req_id", 0ULL);

        if (type == expectedType && reqId == expectedReqId) {
            out = std::move(msg);
            return true;
        }

        if (type == ERR && reqId == expectedReqId) {
            out = std::move(msg);
            return true;
        }
    }

    return false;
}

bool setupGroup(Args& args) {
    int fd = connectTo(args);

    if (fd < 0) {
        std::cerr << "setup connect failed\n";
        return false;
    }

    std::string user = "bench_setup";
    uint64_t reqId = 1;

    json auth = makeReq(AUTH_REQ, reqId++, user);
    auth["user"] = user;

    if (!sendReq(fd, auth)) {
        std::cerr << "setup auth send failed\n";
        ::close(fd);
        return false;
    }

    json authResp;

    if (!waitResp(fd, AUTH_RESP, auth["req_id"].get<uint64_t>(), authResp, args.timeoutMs) ||
        !authResp.value("ok", false)) {
        std::cerr << "setup auth failed: " << authResp.dump() << "\n";
        ::close(fd);
        return false;
    }

    json create = makeReq(CREATE_GROUP_REQ, reqId++, user);
    create["name"] = args.groupName;

    if (!sendReq(fd, create)) {
        std::cerr << "setup create send failed\n";
        ::close(fd);
        return false;
    }

    json createResp;

    if (!waitResp(fd, CREATE_GROUP_RESP, create["req_id"].get<uint64_t>(), createResp, args.timeoutMs) ||
        !createResp.value("ok", false)) {
        std::cerr << "setup create failed: " << createResp.dump() << "\n";
        ::close(fd);
        return false;
    }

    if (createResp.contains("data") && createResp["data"].contains("groupId")) {
        args.groupId = createResp["data"]["groupId"].get<std::string>();
    } else {
        std::cerr << "setup create response missing groupId: " << createResp.dump() << "\n";
        ::close(fd);
        return false;
    }

    ::close(fd);
    return true;
}

struct Worker {
    int id;
    Args args;
    Metrics& metrics;

    int fd = -1;
    std::string user;
    std::atomic<bool> running{false};
    std::atomic<bool> closing{false};
    std::atomic<uint64_t> nextReqId{1};

    std::mutex pendingMu;
    std::map<uint64_t, Clock::time_point> pending;

    std::thread recvThread;
    std::thread timeoutThread;

    bool handshake() {
        user = "bench_user_" + std::to_string(id);
        fd = connectTo(args);

        if (fd < 0) {
            metrics.connectFail.fetch_add(1);
            return false;
        }

        uint64_t authReqId = nextReqId.fetch_add(1);
        json auth = makeReq(AUTH_REQ, authReqId, user);
        auth["user"] = user;

        if (!sendReq(fd, auth)) {
            metrics.authFail.fetch_add(1);
            return false;
        }

        json authResp;

        if (!waitResp(fd, AUTH_RESP, authReqId, authResp, args.timeoutMs) ||
            !authResp.value("ok", false)) {
            metrics.authFail.fetch_add(1);
            return false;
        }

        uint64_t joinReqId = nextReqId.fetch_add(1);
        json join = makeReq(JOIN_GROUP_REQ, joinReqId, user);
        join["groupId"] = args.groupId;

        if (!sendReq(fd, join)) {
            metrics.joinFail.fetch_add(1);
            return false;
        }

        json joinResp;

        if (!waitResp(fd, JOIN_GROUP_RESP, joinReqId, joinResp, args.timeoutMs) ||
            !joinResp.value("ok", false)) {
            metrics.joinFail.fetch_add(1);
            return false;
        }

        return true;
    }

    void recvLoop() {
        while (running.load()) {
            std::string frame;

            if (!recvFrame(fd, frame)) {
                if (!closing.load()) {
                    metrics.recvFail.fetch_add(1);
                }
                break;
            }

            if (frame == "PING") {
                if (!sendFrame(fd, "PONG") && !closing.load()) {
                    metrics.sendFail.fetch_add(1);
                }
                continue;
            }

            json msg = json::parse(frame, nullptr, false);

            if (msg.is_discarded()) {
                metrics.parseFail.fetch_add(1);
                continue;
            }

            int type = msg.value("type", 0);

            if (type == GROUP_MSG_PUSH || type == GROUP_EVENT_PUSH) {
                metrics.pushRecv.fetch_add(1);
                continue;
            }

            if (type != GROUP_MSG_RESP && type != ERR) {
                continue;
            }

            uint64_t reqId = msg.value("req_id", 0ULL);
            Clock::time_point begin;
            bool matched = false;

            {
                std::lock_guard<std::mutex> lock(pendingMu);
                auto it = pending.find(reqId);

                if (it != pending.end()) {
                    begin = it->second;
                    pending.erase(it);
                    matched = true;
                }
            }

            if (!matched) {
                metrics.lateResp.fetch_add(1);
                continue;
            }

            if (type == ERR || !msg.value("ok", false)) {
                metrics.serverErrResp.fetch_add(1);
                continue;
            }

            if (msg.contains("data") && msg["data"].contains("dropped")) {
                metrics.droppedByServer.fetch_add(msg["data"]["dropped"].get<uint64_t>());
            }

            auto end = Clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end - begin).count();

            {
                std::lock_guard<std::mutex> lock(metrics.latMu);
                metrics.latenciesMs.push_back(latencyMs);
            }

            metrics.ok.fetch_add(1);
        }
    }

    void timeoutLoop() {
        auto timeout = std::chrono::milliseconds(args.timeoutMs);

        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto now = Clock::now();
            uint64_t expired = 0;

            {
                std::lock_guard<std::mutex> lock(pendingMu);

                for (auto it = pending.begin(); it != pending.end();) {
                    if (now - it->second > timeout) {
                        it = pending.erase(it);
                        ++expired;
                    } else {
                        ++it;
                    }
                }
            }

            if (expired > 0) {
                metrics.timeout.fetch_add(expired);
            }
        }
    }

    void run() {
        if (!handshake()) {
            cleanup();
            return;
        }

        running = true;
        recvThread = std::thread(&Worker::recvLoop, this);
        timeoutThread = std::thread(&Worker::timeoutLoop, this);

        auto endAt = Clock::now() + std::chrono::seconds(args.durationSec);
        auto interval = std::chrono::duration<double>(1.0 / std::max(args.ratePerClient, 0.1));
        auto nextSend = Clock::now();

        while (Clock::now() < endAt && running.load()) {
            auto now = Clock::now();

            if (now < nextSend) {
                std::this_thread::sleep_for(nextSend - now);
                continue;
            }

            uint64_t reqId = nextReqId.fetch_add(1);

            json req = makeReq(GROUP_MSG_REQ, reqId, user);
            req["groupId"] = args.groupId;
            req["content"] = "hello-" + std::to_string(id) + "-" + std::to_string(reqId);

            {
                std::lock_guard<std::mutex> lock(pendingMu);
                pending[reqId] = Clock::now();
            }

            if (!sendReq(fd, req)) {
                {
                    std::lock_guard<std::mutex> lock(pendingMu);
                    pending.erase(reqId);
                }

                if (!closing.load()) {
                    metrics.sendFail.fetch_add(1);
                }

                break;
            }

            metrics.sent.fetch_add(1);
            nextSend += std::chrono::duration_cast<Clock::duration>(interval);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(args.drainMs));

        closing = true;
        running = false;

        if (fd >= 0) {
            ::shutdown(fd, SHUT_RDWR);
        }

        if (recvThread.joinable()) {
            recvThread.join();
        }

        if (timeoutThread.joinable()) {
            timeoutThread.join();
        }

        cleanup();
    }

    void cleanup() {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }
};

double percentile(std::vector<double>& values, double p) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    size_t index = static_cast<size_t>((values.size() - 1) * p);
    return values[index];
}

bool parseArgs(int argc, char** argv, Args& args) {
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];

        auto hasValue = [&]() {
            return i + 1 < argc;
        };

        if (key == "--host" && hasValue()) {
            args.host = argv[++i];
        } else if (key == "--port" && hasValue()) {
            args.port = std::stoi(argv[++i]);
        } else if (key == "--clients" && hasValue()) {
            args.clients = std::stoi(argv[++i]);
        } else if (key == "--duration" && hasValue()) {
            args.durationSec = std::stoi(argv[++i]);
        } else if (key == "--rate" && hasValue()) {
            args.ratePerClient = std::stod(argv[++i]);
        } else if (key == "--group" && hasValue()) {
            args.groupName = argv[++i];
        } else if (key == "--timeout_ms" && hasValue()) {
            args.timeoutMs = std::stoi(argv[++i]);
        } else if (key == "--drain_ms" && hasValue()) {
            args.drainMs = std::stoi(argv[++i]);
        } else {
            std::cerr << "invalid arg: " << key << "\n";
            return false;
        }
    }

    return true;
}

int main(int argc, char** argv) {
    Args args;

    if (!parseArgs(argc, argv, args)) {
        return 1;
    }

    if (!setupGroup(args)) {
        std::cerr << "setup group failed\n";
        return 2;
    }

    Metrics metrics;
    std::vector<std::thread> workers;

    auto begin = Clock::now();

    for (int i = 0; i < args.clients; ++i) {
        workers.emplace_back([&, i] {
            Worker worker{i, args, metrics};
            worker.run();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    for (auto& worker : workers) {
        worker.join();
    }

    auto end = Clock::now();
    double elapsedSec = std::chrono::duration<double>(end - begin).count();

    std::vector<double> latencies;

    {
        std::lock_guard<std::mutex> lock(metrics.latMu);
        latencies = metrics.latenciesMs;
    }

    uint64_t errTotal =
        metrics.connectFail.load() +
        metrics.authFail.load() +
        metrics.joinFail.load() +
        metrics.recvFail.load() +
        metrics.sendFail.load() +
        metrics.timeout.load() +
        metrics.parseFail.load() +
        metrics.serverErrResp.load();

    json result{
        {"clients", args.clients},
        {"duration_s", args.durationSec},
        {"elapsed_s", elapsedSec},
        {"rate_per_client", args.ratePerClient},
        {"group_name", args.groupName},
        {"group_id", args.groupId},
        {"sent", metrics.sent.load()},
        {"ok", metrics.ok.load()},
        {"qps_ok", elapsedSec > 0 ? metrics.ok.load() / elapsedSec : 0.0},
        {"p50_ms", percentile(latencies, 0.50)},
        {"p95_ms", percentile(latencies, 0.95)},
        {"p99_ms", percentile(latencies, 0.99)},
        {"err_total", errTotal},
        {"connect_fail", metrics.connectFail.load()},
        {"auth_fail", metrics.authFail.load()},
        {"join_fail", metrics.joinFail.load()},
        {"recv_fail", metrics.recvFail.load()},
        {"send_fail", metrics.sendFail.load()},
        {"timeout", metrics.timeout.load()},
        {"parse_fail", metrics.parseFail.load()},
        {"server_err_resp", metrics.serverErrResp.load()},
        {"late_resp", metrics.lateResp.load()},
        {"push_recv", metrics.pushRecv.load()},
        {"dropped_by_server", metrics.droppedByServer.load()}
    };

    std::cout << result.dump(2) << std::endl;
    return 0;
}
