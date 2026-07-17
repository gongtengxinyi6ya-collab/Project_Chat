#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "im/MsgType.h"
#include "third_party/json.hpp"

namespace {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using json = nlohmann::json;

constexpr uint32_t kMaxFrameLength = 1024U * 1024U;
constexpr int kMaxEpollEvents = 1024;
volatile sig_atomic_t gStopRequested = 0;

void requestStop(int) {
    gStopRequested = 1;
}

enum class TestMode {
    Transport,
    Authenticated
};

enum class ConnectionState {
    Connecting,
    LoginWriting,
    LoginWaiting,
    Ready
};

struct Credential {
    std::string accountId;
    std::string password;
};

struct Args {
    std::string host{"127.0.0.1"};
    int port{8080};
    size_t clients{100};
    double connectRatePerSecond{100.0};
    int holdSec{300};
    int connectTimeoutMs{3000};
    int loginTimeoutMs{5000};
    int progressSec{5};
    int serverPid{-1};
    TestMode mode{TestMode::Transport};
    std::string accountsFile;
    bool allowAccountReuse{false};
    std::string jsonOutput{"connection_load_test_result.json"};
    std::string csvOutput;
};

struct ProcessSnapshot {
    bool valid{false};
    uint64_t cpuTicks{0};
    uint64_t rssKb{0};
    uint64_t highWaterKb{0};
    uint64_t fdCount{0};
    uint64_t threadCount{0};
};

struct LatencyStats {
    size_t samples{0};
    double minimumMs{0.0};
    double averageMs{0.0};
    double p50Ms{0.0};
    double p95Ms{0.0};
    double p99Ms{0.0};
    double maximumMs{0.0};
};

struct Metrics {
    uint64_t attempted{0};
    uint64_t connectOk{0};
    uint64_t connectFail{0};
    uint64_t connectTimeout{0};
    uint64_t loginOk{0};
    uint64_t loginFail{0};
    uint64_t loginTimeout{0};
    uint64_t readyCurrent{0};
    uint64_t readyPeak{0};
    uint64_t readyAtHoldStart{0};
    uint64_t readyAtHoldEnd{0};
    uint64_t unexpectedClose{0};
    uint64_t socketErrors{0};
    uint64_t parseFail{0};
    uint64_t otherFrames{0};
    uint64_t heartbeatPingRecv{0};
    uint64_t heartbeatPongQueued{0};
    uint64_t heartbeatPongSent{0};
    uint64_t heartbeatPongFail{0};
    std::map<int, uint64_t> loginErrorCodes;
};

struct ConnectionContext {
    int fd{-1};
    size_t index{0};
    ConnectionState state{ConnectionState::Connecting};
    Credential credential;
    TimePoint connectBegin{};
    TimePoint loginBegin{};
    TimePoint deadline{};
    uint64_t loginRequestId{1};
    std::string input;
    size_t inputOffset{0};
    std::string output;
    size_t outputOffset{0};
    uint64_t pendingPongs{0};
};

std::string_view modeToString(TestMode mode) {
    return mode == TestMode::Transport ? "transport" : "authenticated";
}

std::optional<TestMode> parseMode(std::string_view value) {
    if (value == "transport") {
        return TestMode::Transport;
    }
    if (value == "authenticated") {
        return TestMode::Authenticated;
    }
    return std::nullopt;
}

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "Connection options:\n"
        << "  --host HOST                  Default 127.0.0.1\n"
        << "  --port PORT                  Default 8080\n"
        << "  --clients N                  Default 100\n"
        << "  --connect-rate N             Connections opened per second, default 100\n"
        << "  --hold SEC                   Hold duration after setup, default 300\n"
        << "  --connect-timeout-ms N       Default 3000\n"
        << "  --login-timeout-ms N         Default 5000\n"
        << "  --mode MODE                  transport (default) or authenticated\n"
        << "  --accounts-file PATH         Required in authenticated mode\n"
        << "  --allow-account-reuse        Reuse credentials when clients exceed accounts\n"
        << "  --progress-sec N             Default 5; 0 disables progress output\n"
        << "  --server-pid PID             Sample server CPU/RSS/fd/thread metrics\n"
        << "  --json-out PATH              Default connection_load_test_result.json\n"
        << "  --csv-out PATH               Append a summary CSV row\n"
        << "  --help\n";
}

bool parseInteger(const std::string& value, int& output) {
    try {
        size_t consumed = 0;
        const long parsed = std::stol(value, &consumed);
        if (consumed != value.size() || parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) {
            return false;
        }
        output = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseSize(const std::string& value, size_t& output) {
    try {
        size_t consumed = 0;
        const unsigned long long parsed = std::stoull(value, &consumed);
        if (consumed != value.size()) {
            return false;
        }
        output = static_cast<size_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseDouble(const std::string& value, double& output) {
    try {
        size_t consumed = 0;
        output = std::stod(value, &consumed);
        return consumed == value.size() && std::isfinite(output);
    } catch (...) {
        return false;
    }
}

bool parseArgs(int argc, char** argv, Args& args) {
    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--help") {
            printUsage(argv[0]);
            return false;
        }
        if (option == "--allow-account-reuse") {
            args.allowAccountReuse = true;
            continue;
        }
        if (index + 1 >= argc) {
            std::cerr << "missing value for " << option << '\n';
            return false;
        }
        const std::string value = argv[++index];
        if (option == "--host") {
            args.host = value;
        } else if (option == "--port") {
            if (!parseInteger(value, args.port)) return false;
        } else if (option == "--clients") {
            if (!parseSize(value, args.clients)) return false;
        } else if (option == "--connect-rate") {
            if (!parseDouble(value, args.connectRatePerSecond)) return false;
        } else if (option == "--hold") {
            if (!parseInteger(value, args.holdSec)) return false;
        } else if (option == "--connect-timeout-ms") {
            if (!parseInteger(value, args.connectTimeoutMs)) return false;
        } else if (option == "--login-timeout-ms") {
            if (!parseInteger(value, args.loginTimeoutMs)) return false;
        } else if (option == "--progress-sec") {
            if (!parseInteger(value, args.progressSec)) return false;
        } else if (option == "--server-pid") {
            if (!parseInteger(value, args.serverPid)) return false;
        } else if (option == "--mode") {
            const auto mode = parseMode(value);
            if (!mode) {
                std::cerr << "invalid mode: " << value << '\n';
                return false;
            }
            args.mode = *mode;
        } else if (option == "--accounts-file") {
            args.accountsFile = value;
        } else if (option == "--json-out") {
            args.jsonOutput = value;
        } else if (option == "--csv-out") {
            args.csvOutput = value;
        } else {
            std::cerr << "unknown option: " << option << '\n';
            return false;
        }
    }

    if (args.port <= 0 || args.port > 65535 || args.clients == 0 ||
        args.connectRatePerSecond <= 0.0 || args.holdSec <= 0 ||
        args.connectTimeoutMs <= 0 || args.loginTimeoutMs <= 0 || args.progressSec < 0) {
        std::cerr << "invalid numeric argument\n";
        return false;
    }
    if (args.mode == TestMode::Authenticated && args.accountsFile.empty()) {
        std::cerr << "authenticated mode requires --accounts-file\n";
        return false;
    }
    return true;
}

std::optional<std::vector<Credential>> loadCredentials(const Args& args) {
    if (args.mode == TestMode::Transport) {
        return std::vector<Credential>{};
    }
    std::ifstream input(args.accountsFile);
    if (!input) {
        std::cerr << "cannot open accounts file: " << args.accountsFile << '\n';
        return std::nullopt;
    }
    json document;
    try {
        input >> document;
    } catch (const std::exception& exception) {
        std::cerr << "invalid accounts JSON: " << exception.what() << '\n';
        return std::nullopt;
    }
    const json* accounts = &document;
    if (document.is_object() && document.contains("accounts")) {
        accounts = &document.at("accounts");
    }
    if (!accounts->is_array() || accounts->empty()) {
        std::cerr << "accounts file must contain a non-empty accounts array\n";
        return std::nullopt;
    }

    std::vector<Credential> credentials;
    credentials.reserve(accounts->size());
    std::unordered_set<std::string> uniqueAccountIds;
    for (const auto& entry : *accounts) {
        if (!entry.is_object() || !entry.contains("accountId") ||
            !entry.contains("password") || !entry.at("accountId").is_string() ||
            !entry.at("password").is_string()) {
            std::cerr << "each account requires string accountId and password\n";
            return std::nullopt;
        }
        Credential credential{entry.at("accountId").get<std::string>(),
                              entry.at("password").get<std::string>()};
        if (credential.accountId.empty() || credential.password.empty()) {
            std::cerr << "accountId and password cannot be empty\n";
            return std::nullopt;
        }
        if (!uniqueAccountIds.insert(credential.accountId).second &&
            !args.allowAccountReuse) {
            std::cerr << "duplicate accountId in accounts file: " << credential.accountId << '\n';
            return std::nullopt;
        }
        credentials.push_back(std::move(credential));
    }
    if (!args.allowAccountReuse && credentials.size() < args.clients) {
        std::cerr << "authenticated mode requires at least " << args.clients
                  << " credentials; available=" << credentials.size()
                  << ". Use --allow-account-reuse only for transport-oriented tests.\n";
        return std::nullopt;
    }
    return credentials;
}

std::string makeLoginRequest(const Credential& credential, uint64_t requestId) {
    return json{{"ver", 1},
                {"type", im::msgTypeToInt(im::MsgType::LOGIN_REQ)},
                {"req_id", requestId},
                {"from", credential.accountId},
                {"to", ""},
                {"seq", requestId},
                {"accountId", credential.accountId},
                {"password", credential.password}}
        .dump();
}

std::string encodeFrame(std::string_view payload) {
    const uint32_t networkLength = htonl(static_cast<uint32_t>(payload.size()));
    std::string frame(sizeof(networkLength), '\0');
    std::memcpy(frame.data(), &networkLength, sizeof(networkLength));
    frame.append(payload.data(), payload.size());
    return frame;
}

double elapsedMs(TimePoint begin, TimePoint end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

double percentile(const std::vector<double>& sorted, double fraction) {
    if (sorted.empty()) {
        return 0.0;
    }
    const double position = fraction * static_cast<double>(sorted.size() - 1);
    const size_t lower = static_cast<size_t>(position);
    const size_t upper = std::min(lower + 1, sorted.size() - 1);
    const double weight = position - static_cast<double>(lower);
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

LatencyStats calculateLatencyStats(std::vector<double> values) {
    LatencyStats stats;
    if (values.empty()) {
        return stats;
    }
    std::sort(values.begin(), values.end());
    stats.samples = values.size();
    stats.minimumMs = values.front();
    stats.maximumMs = values.back();
    stats.averageMs = std::accumulate(values.begin(), values.end(), 0.0) /
                      static_cast<double>(values.size());
    stats.p50Ms = percentile(values, 0.50);
    stats.p95Ms = percentile(values, 0.95);
    stats.p99Ms = percentile(values, 0.99);
    return stats;
}

json latencyToJson(const LatencyStats& stats) {
    return json{{"samples", stats.samples},
                {"min", stats.minimumMs},
                {"avg", stats.averageMs},
                {"p50", stats.p50Ms},
                {"p95", stats.p95Ms},
                {"p99", stats.p99Ms},
                {"max", stats.maximumMs}};
}

ProcessSnapshot readProcessSnapshot(int pid) {
    ProcessSnapshot snapshot;
    if (pid < 0) {
        return snapshot;
    }
    const std::string base = pid > 0 ? "/proc/" + std::to_string(pid) : "/proc/self";
    std::ifstream statFile(base + "/stat");
    std::string statLine;
    if (!statFile || !std::getline(statFile, statLine)) {
        return snapshot;
    }
    const size_t closingParenthesis = statLine.rfind(')');
    if (closingParenthesis == std::string::npos || closingParenthesis + 2 >= statLine.size()) {
        return snapshot;
    }
    std::istringstream fields(statLine.substr(closingParenthesis + 2));
    std::vector<std::string> tokens;
    std::string token;
    while (fields >> token) {
        tokens.push_back(token);
    }
    if (tokens.size() <= 21) {
        return snapshot;
    }
    try {
        snapshot.cpuTicks = std::stoull(tokens[11]) + std::stoull(tokens[12]);
        const long pageSize = ::sysconf(_SC_PAGESIZE);
        snapshot.rssKb = std::stoull(tokens[21]) * static_cast<uint64_t>(pageSize) / 1024ULL;
    } catch (...) {
        return {};
    }

    std::ifstream statusFile(base + "/status");
    std::string line;
    while (std::getline(statusFile, line)) {
        if (line.starts_with("VmHWM:")) {
            std::istringstream value(line.substr(6));
            value >> snapshot.highWaterKb;
        } else if (line.starts_with("Threads:")) {
            std::istringstream value(line.substr(8));
            value >> snapshot.threadCount;
        }
    }
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(base + "/fd", error)) {
        static_cast<void>(entry);
        ++snapshot.fdCount;
    }
    snapshot.valid = true;
    return snapshot;
}

double calculateCpuPercent(const ProcessSnapshot& begin, const ProcessSnapshot& end,
                           double elapsedSeconds) {
    if (!begin.valid || !end.valid || elapsedSeconds <= 0.0 || end.cpuTicks < begin.cpuTicks) {
        return 0.0;
    }
    const long ticksPerSecond = ::sysconf(_SC_CLK_TCK);
    const double cpuSeconds = static_cast<double>(end.cpuTicks - begin.cpuTicks) /
                              static_cast<double>(ticksPerSecond);
    return cpuSeconds / elapsedSeconds * 100.0;
}

std::string currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&time, &local);
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

void ensureParentDirectory(const std::string& path) {
    if (path.empty()) {
        return;
    }
    const std::filesystem::path output(path);
    if (!output.has_parent_path()) {
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(output.parent_path(), error);
    if (error) {
        throw std::runtime_error("failed to create output directory: " + error.message());
    }
}

class ConnectionLoadRunner {
public:
    ConnectionLoadRunner(Args args, std::vector<Credential> credentials)
        : args_(std::move(args)), credentials_(std::move(credentials)) {
        epollFd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epollFd_ < 0) {
            throw std::runtime_error("epoll_create1 failed: " + std::string(std::strerror(errno)));
        }
        address_.sin_family = AF_INET;
        address_.sin_port = htons(static_cast<uint16_t>(args_.port));
        if (::inet_pton(AF_INET, args_.host.c_str(), &address_.sin_addr) != 1) {
            throw std::runtime_error("invalid IPv4 address: " + args_.host);
        }
    }

    ~ConnectionLoadRunner() {
        closeAll(false);
        if (epollFd_ >= 0) {
            ::close(epollFd_);
        }
    }

    int run() {
        warnFileDescriptorLimit();
        begin_ = Clock::now();
        nextConnectAt_ = begin_;
        nextProgressAt_ = begin_ + std::chrono::seconds(std::max(1, args_.progressSec));
        nextSampleAt_ = begin_;
        loadBegin_ = readProcessSnapshot(0);
        serverBegin_ = readProcessSnapshot(args_.serverPid);
        loadPeakRss_ = loadBegin_.rssKb;
        loadPeakFd_ = loadBegin_.fdCount;
        serverPeakRss_ = serverBegin_.rssKb;
        serverPeakFd_ = serverBegin_.fdCount;
        const auto connectInterval = std::chrono::duration<double>(1.0 / args_.connectRatePerSecond);
        connectInterval_ = std::chrono::duration_cast<Clock::duration>(connectInterval);

        std::vector<epoll_event> events(kMaxEpollEvents);
        while (!finished_) {
            const TimePoint now = Clock::now();
            if (gStopRequested != 0) {
                interrupted_ = true;
                metrics_.readyAtHoldEnd = metrics_.readyCurrent;
                break;
            }
            launchDueConnections(now);
            expireSetupConnections(now);
            startHoldWhenSetupSettles(now);
            if (finished_) {
                break;
            }
            sampleProcesses(now);
            printProgress(now);

            if (holdDeadline_ && now >= *holdDeadline_) {
                metrics_.readyAtHoldEnd = metrics_.readyCurrent;
                finished_ = true;
                break;
            }
            if (metrics_.attempted == args_.clients && contexts_.empty() && !holdStart_) {
                metrics_.readyAtHoldStart = 0;
                metrics_.readyAtHoldEnd = 0;
                finished_ = true;
                break;
            }

            const int timeoutMs = epollTimeoutMs(now);
            int eventCount;
            do {
                eventCount = ::epoll_wait(epollFd_, events.data(),
                                          static_cast<int>(events.size()), timeoutMs);
            } while (eventCount < 0 && errno == EINTR);
            if (eventCount < 0) {
                throw std::runtime_error("epoll_wait failed: " + std::string(std::strerror(errno)));
            }
            for (int index = 0; index < eventCount; ++index) {
                handleEvent(events[static_cast<size_t>(index)]);
            }
        }

        const TimePoint end = Clock::now();
        if (!metrics_.readyAtHoldEnd && holdStart_) {
            metrics_.readyAtHoldEnd = metrics_.readyCurrent;
        }
        closeAll(false);
        loadEnd_ = readProcessSnapshot(0);
        serverEnd_ = readProcessSnapshot(args_.serverPid);
        loadPeakRss_ = std::max({loadPeakRss_, loadEnd_.rssKb, loadEnd_.highWaterKb});
        loadPeakFd_ = std::max(loadPeakFd_, loadEnd_.fdCount);
        serverPeakRss_ = std::max({serverPeakRss_, serverEnd_.rssKb, serverEnd_.highWaterKb});
        serverPeakFd_ = std::max(serverPeakFd_, serverEnd_.fdCount);
        const double elapsedSeconds = std::chrono::duration<double>(end - begin_).count();
        const json result = buildResult(elapsedSeconds);
        writeResult(result);
        std::cout << result.dump(2) << '\n';

        const bool passed = metrics_.readyAtHoldStart == args_.clients &&
                            metrics_.readyAtHoldEnd == metrics_.readyAtHoldStart &&
                            metrics_.unexpectedClose == 0 && metrics_.heartbeatPongFail == 0 &&
                            metrics_.heartbeatPongQueued == metrics_.heartbeatPongSent &&
                            !interrupted_;
        return passed ? 0 : 4;
    }

private:
    void warnFileDescriptorLimit() const {
        rlimit limit{};
        if (::getrlimit(RLIMIT_NOFILE, &limit) != 0) {
            return;
        }
        const uint64_t required = static_cast<uint64_t>(args_.clients) + 128ULL;
        if (limit.rlim_cur < required) {
            std::cerr << "warning: RLIMIT_NOFILE=" << limit.rlim_cur
                      << " is below recommended " << required << '\n';
        }
    }

    void launchDueConnections(TimePoint now) {
        while (metrics_.attempted < args_.clients && now >= nextConnectAt_) {
            openConnection(static_cast<size_t>(metrics_.attempted), now);
            ++metrics_.attempted;
            nextConnectAt_ += connectInterval_;
            if (connectInterval_ <= Clock::duration::zero()) {
                nextConnectAt_ = now;
            }
        }
    }

    void openConnection(size_t index, TimePoint now) {
        const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            ++metrics_.connectFail;
            ++metrics_.socketErrors;
            return;
        }
        int enabled = 1;
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
        ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled));

        auto context = std::make_unique<ConnectionContext>();
        context->fd = fd;
        context->index = index;
        context->connectBegin = now;
        context->deadline = now + std::chrono::milliseconds(args_.connectTimeoutMs);
        if (args_.mode == TestMode::Authenticated) {
            context->credential = credentials_[index % credentials_.size()];
        }

        const int result = ::connect(fd, reinterpret_cast<sockaddr*>(&address_), sizeof(address_));
        if (result < 0 && errno != EINPROGRESS) {
            ++metrics_.connectFail;
            ++metrics_.socketErrors;
            ::close(fd);
            return;
        }

        epoll_event event{};
        event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        event.data.fd = fd;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event) < 0) {
            ++metrics_.connectFail;
            ++metrics_.socketErrors;
            ::close(fd);
            return;
        }
        contexts_.emplace(fd, std::move(context));
        ++connecting_;
        if (result == 0) {
            completeConnect(fd, now);
        }
    }

    void completeConnect(int fd, TimePoint now) {
        auto* context = findContext(fd);
        if (!context || context->state != ConnectionState::Connecting) {
            return;
        }
        int socketError = 0;
        socklen_t errorLength = sizeof(socketError);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &errorLength) < 0 ||
            socketError != 0) {
            ++metrics_.connectFail;
            ++metrics_.socketErrors;
            closeConnection(fd, false);
            return;
        }
        --connecting_;
        ++metrics_.connectOk;
        connectLatencies_.push_back(elapsedMs(context->connectBegin, now));
        if (args_.mode == TestMode::Transport) {
            markReady(*context);
            updateInterest(*context);
            return;
        }

        context->state = ConnectionState::LoginWriting;
        context->loginBegin = now;
        context->deadline = now + std::chrono::milliseconds(args_.loginTimeoutMs);
        context->output = encodeFrame(makeLoginRequest(context->credential,
                                                       context->loginRequestId));
        context->outputOffset = 0;
        ++loggingIn_;
        updateInterest(*context);
    }

    void markReady(ConnectionContext& context) {
        context.state = ConnectionState::Ready;
        ++metrics_.readyCurrent;
        metrics_.readyPeak = std::max(metrics_.readyPeak, metrics_.readyCurrent);
    }

    void handleEvent(const epoll_event& event) {
        const int fd = event.data.fd;
        auto* context = findContext(fd);
        if (!context) {
            return;
        }
        if (context->state == ConnectionState::Connecting &&
            (event.events & (EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
            completeConnect(fd, Clock::now());
            context = findContext(fd);
            if (!context) {
                return;
            }
        }
        if ((event.events & EPOLLIN) != 0) {
            handleRead(fd);
            context = findContext(fd);
            if (!context) {
                return;
            }
        }
        if ((event.events & EPOLLOUT) != 0) {
            handleWrite(fd);
            context = findContext(fd);
            if (!context) {
                return;
            }
        }
        if ((event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
            handlePeerFailure(fd);
        }
    }

    void handleRead(int fd) {
        auto* context = findContext(fd);
        if (!context) {
            return;
        }
        char buffer[16384];
        while (true) {
            const ssize_t received = ::recv(fd, buffer, sizeof(buffer), 0);
            if (received > 0) {
                context->input.append(buffer, static_cast<size_t>(received));
                if (!processFrames(fd)) {
                    return;
                }
                context = findContext(fd);
                if (!context) {
                    return;
                }
                continue;
            }
            if (received == 0) {
                handlePeerFailure(fd);
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            ++metrics_.socketErrors;
            handlePeerFailure(fd);
            return;
        }
    }

    bool processFrames(int fd) {
        while (true) {
            auto* context = findContext(fd);
            if (!context) {
                return false;
            }
            const size_t readable = context->input.size() - context->inputOffset;
            if (readable < sizeof(uint32_t)) {
                compactInput(*context);
                return true;
            }
            uint32_t networkLength = 0;
            std::memcpy(&networkLength, context->input.data() + context->inputOffset,
                        sizeof(networkLength));
            const uint32_t length = ntohl(networkLength);
            if (length == 0 || length > kMaxFrameLength) {
                ++metrics_.parseFail;
                failSetupIfNeeded(*context);
                closeConnection(fd, context->state == ConnectionState::Ready);
                return false;
            }
            if (readable < sizeof(uint32_t) + length) {
                compactInput(*context);
                return true;
            }
            context->inputOffset += sizeof(uint32_t);
            std::string payload = context->input.substr(context->inputOffset, length);
            context->inputOffset += length;
            if (!handleFrame(fd, payload)) {
                return false;
            }
        }
    }

    bool handleFrame(int fd, const std::string& payload) {
        auto* context = findContext(fd);
        if (!context) {
            return false;
        }
        if (payload == "PING") {
            ++metrics_.heartbeatPingRecv;
            context->output.append(encodeFrame("PONG"));
            ++context->pendingPongs;
            ++metrics_.heartbeatPongQueued;
            updateInterest(*context);
            return true;
        }

        if (context->state == ConnectionState::LoginWaiting ||
            context->state == ConnectionState::LoginWriting) {
            json response;
            try {
                response = json::parse(payload);
            } catch (...) {
                ++metrics_.parseFail;
                ++metrics_.loginFail;
                closeConnection(fd, false);
                return false;
            }
            if (response.value("req_id", uint64_t{0}) != context->loginRequestId) {
                ++metrics_.otherFrames;
                return true;
            }
            if (!response.value("ok", false)) {
                ++metrics_.loginFail;
                ++metrics_.loginErrorCodes[response.value("code", -1)];
                closeConnection(fd, false);
                return false;
            }
            --loggingIn_;
            ++metrics_.loginOk;
            loginLatencies_.push_back(elapsedMs(context->loginBegin, Clock::now()));
            markReady(*context);
            updateInterest(*context);
            return true;
        }

        ++metrics_.otherFrames;
        return true;
    }

    void handleWrite(int fd) {
        auto* context = findContext(fd);
        if (!context) {
            return;
        }
        while (context->outputOffset < context->output.size()) {
            const ssize_t written = ::send(fd, context->output.data() + context->outputOffset,
                                           context->output.size() - context->outputOffset,
                                           MSG_NOSIGNAL);
            if (written > 0) {
                context->outputOffset += static_cast<size_t>(written);
                continue;
            }
            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                updateInterest(*context);
                return;
            }
            ++metrics_.socketErrors;
            metrics_.heartbeatPongFail += context->pendingPongs;
            failSetupIfNeeded(*context);
            closeConnection(fd, context->state == ConnectionState::Ready);
            return;
        }

        context->output.clear();
        context->outputOffset = 0;
        metrics_.heartbeatPongSent += context->pendingPongs;
        context->pendingPongs = 0;
        if (context->state == ConnectionState::LoginWriting) {
            context->state = ConnectionState::LoginWaiting;
        }
        updateInterest(*context);
    }

    void handlePeerFailure(int fd) {
        auto* context = findContext(fd);
        if (!context) {
            return;
        }
        const bool unexpected = context->state == ConnectionState::Ready;
        failSetupIfNeeded(*context);
        metrics_.heartbeatPongFail += context->pendingPongs;
        closeConnection(fd, unexpected);
    }

    void failSetupIfNeeded(const ConnectionContext& context) {
        if (context.state == ConnectionState::Connecting) {
            ++metrics_.connectFail;
        } else if (context.state == ConnectionState::LoginWriting ||
                   context.state == ConnectionState::LoginWaiting) {
            ++metrics_.loginFail;
        }
    }

    void expireSetupConnections(TimePoint now) {
        std::vector<int> expired;
        for (const auto& [fd, context] : contexts_) {
            if (context->state != ConnectionState::Ready && now >= context->deadline) {
                expired.push_back(fd);
            }
        }
        for (const int fd : expired) {
            auto* context = findContext(fd);
            if (!context) {
                continue;
            }
            if (context->state == ConnectionState::Connecting) {
                ++metrics_.connectTimeout;
                ++metrics_.connectFail;
            } else {
                ++metrics_.loginTimeout;
                ++metrics_.loginFail;
            }
            metrics_.heartbeatPongFail += context->pendingPongs;
            closeConnection(fd, false);
        }
    }

    void startHoldWhenSetupSettles(TimePoint now) {
        if (holdStart_ || metrics_.attempted != args_.clients || connecting_ != 0 ||
            loggingIn_ != 0) {
            return;
        }
        holdStart_ = now;
        metrics_.readyAtHoldStart = metrics_.readyCurrent;
        if (metrics_.readyAtHoldStart == 0) {
            metrics_.readyAtHoldEnd = 0;
            finished_ = true;
            return;
        }
        holdDeadline_ = now + std::chrono::seconds(args_.holdSec);
        std::cerr << "setup settled: ready=" << metrics_.readyAtHoldStart << '/'
                  << args_.clients << ", holding for " << args_.holdSec << " seconds\n";
    }

    void compactInput(ConnectionContext& context) {
        if (context.inputOffset == 0) {
            return;
        }
        if (context.inputOffset == context.input.size()) {
            context.input.clear();
            context.inputOffset = 0;
        } else if (context.inputOffset >= 4096) {
            context.input.erase(0, context.inputOffset);
            context.inputOffset = 0;
        }
    }

    void updateInterest(const ConnectionContext& context) {
        epoll_event event{};
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        if (context.state == ConnectionState::Connecting ||
            context.outputOffset < context.output.size()) {
            event.events |= EPOLLOUT;
        }
        event.data.fd = context.fd;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_MOD, context.fd, &event) < 0 && errno != ENOENT) {
            throw std::runtime_error("epoll_ctl mod failed: " + std::string(std::strerror(errno)));
        }
    }

    ConnectionContext* findContext(int fd) {
        const auto iterator = contexts_.find(fd);
        return iterator == contexts_.end() ? nullptr : iterator->second.get();
    }

    void closeConnection(int fd, bool unexpected) {
        const auto iterator = contexts_.find(fd);
        if (iterator == contexts_.end()) {
            return;
        }
        const ConnectionState state = iterator->second->state;
        if (state == ConnectionState::Connecting && connecting_ > 0) {
            --connecting_;
        } else if ((state == ConnectionState::LoginWriting ||
                    state == ConnectionState::LoginWaiting) && loggingIn_ > 0) {
            --loggingIn_;
        } else if (state == ConnectionState::Ready && metrics_.readyCurrent > 0) {
            --metrics_.readyCurrent;
            if (unexpected) {
                ++metrics_.unexpectedClose;
            }
        }
        ::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
        ::close(fd);
        contexts_.erase(iterator);
    }

    void closeAll(bool unexpected) {
        std::vector<int> fds;
        fds.reserve(contexts_.size());
        for (const auto& [fd, context] : contexts_) {
            static_cast<void>(context);
            fds.push_back(fd);
        }
        for (const int fd : fds) {
            closeConnection(fd, unexpected);
        }
    }

    int epollTimeoutMs(TimePoint now) const {
        auto deadline = now + std::chrono::milliseconds(100);
        if (metrics_.attempted < args_.clients) {
            deadline = std::min(deadline, nextConnectAt_);
        }
        if (holdDeadline_) {
            deadline = std::min(deadline, *holdDeadline_);
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        return static_cast<int>(std::clamp<int64_t>(remaining.count(), 0, 100));
    }

    void sampleProcesses(TimePoint now) {
        if (now < nextSampleAt_) {
            return;
        }
        const ProcessSnapshot load = readProcessSnapshot(0);
        const ProcessSnapshot server = readProcessSnapshot(args_.serverPid);
        loadPeakRss_ = std::max({loadPeakRss_, load.rssKb, load.highWaterKb});
        loadPeakFd_ = std::max(loadPeakFd_, load.fdCount);
        serverPeakRss_ = std::max({serverPeakRss_, server.rssKb, server.highWaterKb});
        serverPeakFd_ = std::max(serverPeakFd_, server.fdCount);
        nextSampleAt_ = now + std::chrono::seconds(1);
    }

    void printProgress(TimePoint now) {
        if (args_.progressSec == 0 || now < nextProgressAt_) {
            return;
        }
        const double elapsed = std::chrono::duration<double>(now - begin_).count();
        std::cerr << "elapsed=" << std::fixed << std::setprecision(1) << elapsed
                  << "s attempted=" << metrics_.attempted << '/' << args_.clients
                  << " connected=" << metrics_.connectOk
                  << " ready=" << metrics_.readyCurrent
                  << " connect_fail=" << metrics_.connectFail
                  << " login_fail=" << metrics_.loginFail
                  << " unexpected_close=" << metrics_.unexpectedClose
                  << " ping=" << metrics_.heartbeatPingRecv
                  << " pong=" << metrics_.heartbeatPongSent << '\n';
        nextProgressAt_ = now + std::chrono::seconds(args_.progressSec);
    }

    json processJson(const ProcessSnapshot& begin, const ProcessSnapshot& end,
                     uint64_t peakRss, uint64_t peakFd, double elapsedSeconds) const {
        return json{{"sampled", begin.valid && end.valid},
                    {"cpu_percent", calculateCpuPercent(begin, end, elapsedSeconds)},
                    {"rss_start_kb", begin.rssKb},
                    {"rss_end_kb", end.rssKb},
                    {"rss_peak_kb", peakRss},
                    {"fd_start", begin.fdCount},
                    {"fd_end", end.fdCount},
                    {"fd_peak", peakFd},
                    {"threads_start", begin.threadCount},
                    {"threads_end", end.threadCount}};
    }

    json buildResult(double elapsedSeconds) const {
        json loginErrors = json::object();
        for (const auto& [code, count] : metrics_.loginErrorCodes) {
            loginErrors[std::to_string(code)] = count;
        }
        const double setupSuccessRate = args_.clients == 0
                                            ? 0.0
                                            : static_cast<double>(metrics_.readyAtHoldStart) /
                                                  static_cast<double>(args_.clients);
        const double survivalRate = metrics_.readyAtHoldStart == 0
                                        ? 0.0
                                        : static_cast<double>(metrics_.readyAtHoldEnd) /
                                              static_cast<double>(metrics_.readyAtHoldStart);
        const auto connectStats = calculateLatencyStats(connectLatencies_);
        const auto loginStats = calculateLatencyStats(loginLatencies_);
        const bool passed = metrics_.readyAtHoldStart == args_.clients &&
                            metrics_.readyAtHoldEnd == metrics_.readyAtHoldStart &&
                            metrics_.unexpectedClose == 0 && metrics_.heartbeatPongFail == 0 &&
                            metrics_.heartbeatPongQueued == metrics_.heartbeatPongSent &&
                            !interrupted_;
        return json{
            {"timestamp", currentTimestamp()},
            {"config",
             {{"host", args_.host},
              {"port", args_.port},
              {"mode", std::string(modeToString(args_.mode))},
              {"clients_requested", args_.clients},
              {"connect_rate_per_s", args_.connectRatePerSecond},
              {"hold_s", args_.holdSec},
              {"connect_timeout_ms", args_.connectTimeoutMs},
              {"login_timeout_ms", args_.loginTimeoutMs},
              {"elapsed_s", elapsedSeconds},
              {"credential_count", credentials_.size()},
              {"account_reuse", args_.allowAccountReuse}}},
            {"connections",
             {{"attempted", metrics_.attempted},
              {"connect_ok", metrics_.connectOk},
              {"connect_fail", metrics_.connectFail},
              {"connect_timeout", metrics_.connectTimeout},
              {"login_ok", metrics_.loginOk},
              {"login_fail", metrics_.loginFail},
              {"login_timeout", metrics_.loginTimeout},
              {"ready_peak", metrics_.readyPeak},
              {"ready_at_hold_start", metrics_.readyAtHoldStart},
              {"ready_at_hold_end", metrics_.readyAtHoldEnd},
              {"setup_success_rate", setupSuccessRate},
              {"survival_rate", survivalRate},
              {"unexpected_close", metrics_.unexpectedClose},
              {"socket_errors", metrics_.socketErrors},
              {"parse_fail", metrics_.parseFail},
              {"other_frames", metrics_.otherFrames},
              {"login_error_codes", loginErrors}}},
            {"heartbeat",
             {{"ping_recv", metrics_.heartbeatPingRecv},
              {"pong_queued", metrics_.heartbeatPongQueued},
              {"pong_sent", metrics_.heartbeatPongSent},
              {"pong_fail", metrics_.heartbeatPongFail}}},
            {"latency_ms",
             {{"connect", latencyToJson(connectStats)}, {"login", latencyToJson(loginStats)}}},
            {"process",
             {{"load_generator",
               processJson(loadBegin_, loadEnd_, loadPeakRss_, loadPeakFd_,
                           elapsedSeconds)},
              {"server",
               processJson(serverBegin_, serverEnd_, serverPeakRss_, serverPeakFd_,
                           elapsedSeconds)}}},
            {"interrupted", interrupted_},
            {"passed", passed}};
    }

    void writeResult(const json& result) const {
        ensureParentDirectory(args_.jsonOutput);
        std::ofstream jsonFile(args_.jsonOutput, std::ios::trunc);
        if (!jsonFile) {
            throw std::runtime_error("cannot open JSON output: " + args_.jsonOutput);
        }
        jsonFile << result.dump(2) << '\n';

        if (args_.csvOutput.empty()) {
            return;
        }
        ensureParentDirectory(args_.csvOutput);
        const bool writeHeader = !std::filesystem::exists(args_.csvOutput) ||
                                 std::filesystem::file_size(args_.csvOutput) == 0;
        std::ofstream csv(args_.csvOutput, std::ios::app);
        if (!csv) {
            throw std::runtime_error("cannot open CSV output: " + args_.csvOutput);
        }
        if (writeHeader) {
            csv << "timestamp,mode,clients_requested,ready_start,ready_end,setup_success_rate,"
                   "survival_rate,unexpected_close,ping_recv,pong_sent,pong_fail,connect_p95_ms,"
                   "login_p95_ms,server_cpu_pct,server_rss_peak_kb,server_fd_peak,passed\n";
        }
        csv << result.at("timestamp").get<std::string>() << ','
            << result.at("config").at("mode").get<std::string>() << ','
            << result.at("config").at("clients_requested") << ','
            << result.at("connections").at("ready_at_hold_start") << ','
            << result.at("connections").at("ready_at_hold_end") << ','
            << result.at("connections").at("setup_success_rate") << ','
            << result.at("connections").at("survival_rate") << ','
            << result.at("connections").at("unexpected_close") << ','
            << result.at("heartbeat").at("ping_recv") << ','
            << result.at("heartbeat").at("pong_sent") << ','
            << result.at("heartbeat").at("pong_fail") << ','
            << result.at("latency_ms").at("connect").at("p95") << ','
            << result.at("latency_ms").at("login").at("p95") << ','
            << result.at("process").at("server").at("cpu_percent") << ','
            << result.at("process").at("server").at("rss_peak_kb") << ','
            << result.at("process").at("server").at("fd_peak") << ','
            << (result.at("passed").get<bool>() ? 1 : 0) << '\n';
    }

    Args args_;
    std::vector<Credential> credentials_;
    int epollFd_{-1};
    sockaddr_in address_{};
    std::unordered_map<int, std::unique_ptr<ConnectionContext>> contexts_;
    Metrics metrics_;
    uint64_t connecting_{0};
    uint64_t loggingIn_{0};
    std::vector<double> connectLatencies_;
    std::vector<double> loginLatencies_;
    TimePoint begin_{};
    TimePoint nextConnectAt_{};
    Clock::duration connectInterval_{};
    TimePoint nextProgressAt_{};
    TimePoint nextSampleAt_{};
    std::optional<TimePoint> holdStart_;
    std::optional<TimePoint> holdDeadline_;
    bool finished_{false};
    bool interrupted_{false};
    ProcessSnapshot loadBegin_;
    ProcessSnapshot loadEnd_;
    ProcessSnapshot serverBegin_;
    ProcessSnapshot serverEnd_;
    uint64_t loadPeakRss_{0};
    uint64_t loadPeakFd_{0};
    uint64_t serverPeakRss_{0};
    uint64_t serverPeakFd_{0};
};

}

int main(int argc, char** argv) {
    ::signal(SIGPIPE, SIG_IGN);
    ::signal(SIGINT, requestStop);
    ::signal(SIGTERM, requestStop);
    Args args;
    if (!parseArgs(argc, argv, args)) {
        return argc >= 2 && std::string(argv[1]) == "--help" ? 0 : 2;
    }
    const auto credentials = loadCredentials(args);
    if (!credentials) {
        return 3;
    }
    try {
        ConnectionLoadRunner runner(std::move(args), *credentials);
        return runner.run();
    } catch (const std::exception& exception) {
        std::cerr << "connection load test failed: " << exception.what() << '\n';
        return 5;
    }
}
