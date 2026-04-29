#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <iomanip>
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
    GROUP_MSG_REQ = 14,
    GROUP_MSG_RESP = 15,
    CREATE_GROUP_REQ = 16,
    CREATE_GROUP_RESP = 17,
    JOIN_GROUP_REQ = 18,
    JOIN_GROUP_RESP = 19,
    GROUP_EVENT_PUSH = 20,
    GROUP_MSG_PUSH = 21,
    PING = 100,
    PONG = 101,
    ERR = 999
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
    std::atomic<uint64_t> runtimeRecvFail{0};
    std::atomic<uint64_t> runtimeSendFail{0};
    std::atomic<uint64_t> droppedByServer{0};

    std::mutex latMu;
    std::vector<double> latenciesMs;
};

static bool sendAll(int fd, const void* data, size_t len) {
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

static bool recvAll(int fd, void* data, size_t len) {
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

static bool sendFrame(int fd, const std::string& payload) {
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    return sendAll(fd, &len, 4) && sendAll(fd, payload.data(), payload.size());
}

static bool recvFrame(int fd, std::string& payload) {
    uint32_t netLen = 0;
    if (!recvAll(fd, &netLen, 4)) {
        return false;
    }

    uint32_t len = ntohl(netLen);
    if (len == 0 || len > 1024 * 1024) {
        return false;
    }

    payload.assign(len, '\0');
    return recvAll(fd, payload.data(), len);
}

static int connectTo(const Args& args) {
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

static json makeReq(int type, uint64_t reqId, const std::string& user) {
    return json{
        {"ver", 1},
        {"type", type},
        {"req_id", reqId},
        {"seq", reqId},
        {"from", user},
        {"user", user},
        {"to", ""},
        {"content", ""}
    };
}

static bool sendReq(int fd, const json& req) {
    return sendFrame(fd, req.dump());
}

static bool waitResp(
    int fd,
    int expectedType,
    uint64_t expectedReqId,
    json& out,
    int timeoutMs
) {
    auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);

    while (Clock::now() < deadline) {
        std::string frame;
        if (!recvFrame(fd, frame)) {
            return false;
        }

        json msg = json::parse(frame, nullptr, false);
        if (msg.is_discarded()) {
            continue;
        }

        int type = msg.value("type", 0);
        if (type == PING) {
            json pong{{"ver", 1}, {"type", PONG}, {"req_id", 0}, {"seq", 0}};
            sendFrame(fd, pong.dump());
            continue;
        }

        if (type == expectedType && msg.value("req_id", 0ULL) == expectedReqId) {
            out = std::move(msg);
            return true;
        }

        if (type == ERR && msg.value("req_id", 0ULL) == expectedReqId) {
            out = std::move(msg);
            return true;
        }
    }

    return false;
}

static bool setupGroup(Args& args) {
    int fd = connectTo(args);
    if (fd < 0) {
        return false;
    }

    std::string user = "bench_setup";
    uint64_t reqId = 1;

    json auth = makeReq(AUTH_REQ, reqId++, user);
    auth["user"] = user;
    if (!sendReq(fd, auth)) {
        ::close(fd);
        return false;
    }

    json authResp;
    if (!waitResp(fd, AUTH_RESP, auth["req_id"].get<uint64_t>(), authResp, args.timeoutMs)) {
        ::close(fd);
        return false;
    }

    json create = makeReq(CREATE_GROUP_REQ, reqId++, user);
    create["groupName"] = args.groupName;
    if (!sendReq(fd, create)) {
        ::close(fd);
        return false;
    }

    json createResp;
    if (!waitResp(fd, CREATE_GROUP_RESP, create["req_id"].get<uint64_t>(), createResp, args.timeoutMs)) {
        ::close(fd);
        return false;
    }

    if (!createResp.value("ok", false)) {
        ::close(fd);
        return false;
    }

    if (createResp.contains("data") && createResp["data"].contains("groupId")) {
        args.groupId = createResp["data"]["groupId"].get<std::string>();
    } else {
        args.groupId = args.groupName;
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
    std::atomic<uint64_t> reqId{1};

    std::mutex mu;
    std::map<uint64_t, Clock::time_point> pending;

    std::thread recvThread;
    std::thread timeoutThread;

    bool handshake() {
        user = "bench_user_" + std::to_string(id);
        fd = connectTo(args);
        if (fd < 0) {
            metrics.connectFail++;
            return false;
        }

        uint64_t authReqId = reqId++;
        json auth = makeReq(AUTH_REQ, authReqId, user);
        auth["user"] = user;
        if (!sendReq(fd, auth)) {
            metrics.authFail++;
            return false;
        }

        json authResp;
        if (!waitResp(fd, AUTH_RESP, authReqId, authResp, args.timeoutMs) ||
            !authResp.value("ok", false)) {
            metrics.authFail++;
            return false;
        }

        uint64_t joinReqId = reqId++;
        json join = makeReq(JOIN_GROUP_REQ, joinReqId, user);
        join["groupId"] = args.groupId;
        if (!sendReq(fd, join)) {
            metrics.joinFail++;
            return false;
        }

        json joinResp;
        if (!waitResp(fd, JOIN_GROUP_RESP, joinReqId, joinResp, args.timeoutMs) ||
            !joinResp.value("ok", false)) {
            metrics.joinFail++;
            return false;
        }

        return true;
    }

    void recvLoop() {
        while (running.load()) {
            std::string frame;
            if (!recvFrame(fd, frame)) {
                if (!closing.load()) {
                    metrics.runtimeRecvFail++;
                }
                break;
            }

            json msg = json::parse(frame, nullptr, false);
            if (msg.is_discarded()) {
                metrics.parseFail++;
                continue;
            }

            int type = msg.value("type", 0);

            if (type == PING) {
                json pong{{"ver", 1}, {"type", PONG}, {"req_id", 0}, {"seq", 0}};
                if (!sendFrame(fd, pong.dump()) && !closing.load()) {
                    metrics.runtimeSendFail++;
                }
                continue;
            }

            if (type == GROUP_MSG_PUSH || type == GROUP_EVENT_PUSH) {
                metrics.pushRecv++;
                continue;
            }

            if (type == GROUP_MSG_RESP || type == ERR) {
                uint64_t respReqId = msg.value("req_id", 0ULL);
                Clock::time_point start;
                bool found = false;

                {
                    std::lock_guard<std::mutex> lock(mu);
                    auto it = pending.find(respReqId);
                    if (it != pending.end()) {
                        start = it->second;
                        pending.erase(it);
                        found = true;
                    }
                }

                if (!found) {
                    metrics.lateResp++;
                    continue;
                }

                if (type == ERR || !msg.value("ok", false)) {
                    metrics.serverErrResp++;
                    continue;
                }

                if (msg.contains("data") && msg["data"].contains("dropped")) {
                    metrics.droppedByServer += msg["data"]["dropped"].get<uint64_t>();
                }

                auto end = Clock::now();
                double ms = std::chrono::duration<double, std::milli>(end - start).count();

                {
                    std::lock_guard<std::mutex> lock(metrics.latMu);
                    metrics.latenciesMs.push_back(ms);
                }

                metrics.ok++;
            }
        }
    }

    void timeoutLoop() {
        auto timeout = std::chrono::milliseconds(args.timeoutMs);

        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto now = Clock::now();
            uint64_t expired = 0;

            {
                std::lock_guard<std::mutex> lock(mu);
                for (auto it = pending.begin(); it != pending.end();) {
                    if (now - it->second > timeout) {
                        it = pending.erase(it);
                        expired++;
                    } else {
                        ++it;
                    }
                }
            }

            metrics.timeout += expired;
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

            uint64_t rid = reqId++;
            json req = makeReq(GROUP_MSG_REQ, rid, user);
            req["groupId"] = args.groupId;
            req["content"] = "hello-" + std::to_string(rid);

            {
                std::lock_guard<std::mutex> lock(mu);
                pending[rid] = Clock::now();
            }

            if (!sendReq(fd, req)) {
                {
                    std::lock_guard<std::mutex> lock(mu);
                    pending.erase(rid);
                }

                if (!closing.load()) {
                    metrics.runtimeSendFail++;
                }
                break;
            }

            metrics.sent++;
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

static double percentile(std::vector<double>& values, double p) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    size_t idx = static_cast<size_t>((values.size() - 1) * p);
    return values[idx];
}

static void printUsage(const char* name) {
    std::cerr
        << "Usage: " << name
        << " [--host 127.0.0.1] [--port 8080] [--clients 20]"
        << " [--duration 60] [--rate 5] [--group Group1]"
        << " [--timeout_ms 10000] [--drain_ms 3000]\n";
}

static bool parseArgs(int argc, char** argv, Args& args) {
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];

        auto needValue = [&](const std::string& k) {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << k << "\n";
                return false;
            }
            return true;
        };

        if (key == "--host" && needValue(key)) {
            args.host = argv[++i];
        } else if (key == "--port" && needValue(key)) {
            args.port = std::stoi(argv[++i]);
        } else if (key == "--clients" && needValue(key)) {
            args.clients = std::stoi(argv[++i]);
        } else if (key == "--duration" && needValue(key)) {
            args.durationSec = std::stoi(argv[++i]);
        } else if (key == "--rate" && needValue(key)) {
            args.ratePerClient = std::stod(argv[++i]);
        } else if (key == "--group" && needValue(key)) {
            args.groupName = argv[++i];
        } else if (key == "--timeout_ms" && needValue(key)) {
            args.timeoutMs = std::stoi(argv[++i]);
        } else if (key == "--drain_ms" && needValue(key)) {
            args.drainMs = std::stoi(argv[++i]);
        } else {
            printUsage(argv[0]);
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
    std::vector<std::thread> threads;

    auto start = Clock::now();

    for (int i = 0; i < args.clients; ++i) {
        threads.emplace_back([&, i] {
            Worker worker{i, args, metrics};
            worker.run();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = Clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    std::vector<double> latencies;
    {
        std::lock_guard<std::mutex> lock(metrics.latMu);
        latencies = metrics.latenciesMs;
    }

    double p50 = percentile(latencies, 0.50);
    double p95 = percentile(latencies, 0.95);
    double p99 = percentile(latencies, 0.99);

    uint64_t errTotal =
        metrics.connectFail.load() +
        metrics.authFail.load() +
        metrics.joinFail.load() +
        metrics.runtimeRecvFail.load() +
        metrics.runtimeSendFail.load() +
        metrics.timeout.load() +
        metrics.parseFail.load() +
        metrics.serverErrResp.load();

    json result{
        {"clients", args.clients},
        {"duration_s", args.durationSec},
        {"elapsed_s", elapsed},
        {"rate_per_client", args.ratePerClient},
        {"group_name", args.groupName},
        {"group_id", args.groupId},
        {"sent", metrics.sent.load()},
        {"ok", metrics.ok.load()},
        {"qps_ok", elapsed > 0 ? metrics.ok.load() / elapsed : 0.0},
        {"p50_ms", p50},
        {"p95_ms", p95},
        {"p99_ms", p99},
        {"err_total", errTotal},
        {"connect_fail", metrics.connectFail.load()},
        {"auth_fail", metrics.authFail.load()},
        {"join_fail", metrics.joinFail.load()},
        {"recv_fail", metrics.runtimeRecvFail.load()},
        {"send_fail", metrics.runtimeSendFail.load()},
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
