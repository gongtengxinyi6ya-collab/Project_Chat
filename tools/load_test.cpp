#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "im/MsgType.h"
#include "third_party/json.hpp"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

namespace {

constexpr uint32_t kMaxFrameLength = 1024U * 1024U;
constexpr int kDefaultAccountRateLimit = 20;

enum class PacingMode {
    Staggered,
    Synchronized
};

std::string_view pacingModeToString(PacingMode mode) {
    switch (mode) {
        case PacingMode::Staggered:
            return "staggered";
        case PacingMode::Synchronized:
            return "synchronized";
    }
    return "unknown";
}

std::optional<PacingMode> parsePacingMode(std::string_view value) {
    if (value == "staggered") {
        return PacingMode::Staggered;
    }
    if (value == "synchronized") {
        return PacingMode::Synchronized;
    }
    return std::nullopt;
}

struct Credential {
    std::string accountId;
    std::string password;
};

struct Args {
    std::string host{"127.0.0.1"};
    int port{8080};
    int clients{20};
    int warmupSec{5};
    int durationSec{60};
    double ratePerClient{5.0};
    int connectTimeoutMs{3000};
    int requestTimeoutMs{10000};
    int drainMs{15000};
    int fanoutSettleMs{2000};
    int progressSec{5};
    size_t maxInflight{128};
    size_t payloadBytes{64};
    bool verifyGroup{true};
    bool allowPartialReady{false};
    bool allowAccountReuse{false};
    PacingMode pacingMode{PacingMode::Staggered};
    int accountRateLimit{kDefaultAccountRateLimit};
    int serverPid{-1};

    std::string accountId;
    std::string password;
    std::string accountsFile;
    std::string groupId;
    std::string createGroupName;
    std::string jsonOutput{"load_test_result.json"};
    std::string csvOutput;
    std::string runId;
};

struct PendingRequest {
    TimePoint begin;
    bool measured{false};
};

struct LatencyStats {
    uint64_t samples{0};
    double minimumMs{0.0};
    double maximumMs{0.0};
    double averageMs{0.0};
    double p50Ms{0.0};
    double p90Ms{0.0};
    double p95Ms{0.0};
    double p99Ms{0.0};
    double p999Ms{0.0};
};

struct ProcessSnapshot {
    bool valid{false};
    uint64_t cpuTicks{0};
    uint64_t rssKb{0};
    uint64_t highWaterKb{0};
};

struct Metrics {
    std::atomic<uint64_t> connectOk{0};
    std::atomic<uint64_t> connectFail{0};
    std::atomic<uint64_t> loginOk{0};
    std::atomic<uint64_t> loginFail{0};
    std::atomic<uint64_t> groupCheckOk{0};
    std::atomic<uint64_t> groupCheckFail{0};
    std::atomic<uint64_t> workerExceptions{0};
    std::atomic<uint64_t> activeConnections{0};

    std::atomic<uint64_t> attempted{0};
    std::atomic<uint64_t> sent{0};
    std::atomic<uint64_t> responseOk{0};
    std::atomic<uint64_t> serverErrorResponses{0};
    std::atomic<uint64_t> timeout{0};
    std::atomic<uint64_t> lateResponses{0};
    std::atomic<uint64_t> unmatchedResponses{0};
    std::atomic<uint64_t> sendFail{0};
    std::atomic<uint64_t> recvFail{0};
    std::atomic<uint64_t> parseFail{0};
    std::atomic<uint64_t> inflightThrottled{0};
    std::atomic<uint64_t> scheduleLagEvents{0};
    std::atomic<uint64_t> peakInflight{0};

    std::atomic<uint64_t> warmupSent{0};
    std::atomic<uint64_t> warmupOk{0};
    std::atomic<uint64_t> warmupError{0};
    std::atomic<uint64_t> warmupTimeout{0};
    std::atomic<uint64_t> warmupPushRecv{0};

    std::atomic<uint64_t> pushRecv{0};
    std::atomic<uint64_t> otherPushRecv{0};
    std::atomic<uint64_t> serverFanoutSent{0};
    std::atomic<uint64_t> serverDropped{0};
    std::atomic<uint64_t> serverOverloaded{0};
    std::atomic<uint64_t> serverClosed{0};
    std::atomic<uint64_t> serverNoSuchConnection{0};
    std::atomic<uint64_t> serverFanoutFailed{0};

    std::atomic<uint64_t> appBytesSent{0};
    std::atomic<uint64_t> appBytesRecv{0};
    std::atomic<uint64_t> measuredBytesSent{0};
    std::atomic<uint64_t> measuredBytesRecv{0};

    std::mutex latencyMutex;
    std::vector<double> successLatenciesMs;

    std::mutex persistenceMutex;
    std::vector<double> queueWaitMs;
    std::vector<double> persistMs;

    std::mutex errorMutex;
    std::map<int, uint64_t> errorCodes;
};

class StartGate {
public:
    explicit StartGate(int total) : total_(total) {}

    void report(bool ready) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++reported_;
        if (ready) {
            ++ready_;
        }
        condition_.notify_all();
    }

    int waitUntilReported() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return reported_ == total_; });
        return ready_;
    }

    void open(TimePoint startTime, bool abort) {
        std::lock_guard<std::mutex> lock(mutex_);
        startTime_ = startTime;
        abort_ = abort;
        opened_ = true;
        condition_.notify_all();
    }

    std::optional<TimePoint> waitForStart() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return opened_; });
        if (abort_) {
            return std::nullopt;
        }
        return startTime_;
    }

private:
    int total_{0};
    int reported_{0};
    int ready_{0};
    bool opened_{false};
    bool abort_{false};
    TimePoint startTime_{};
    std::mutex mutex_;
    std::condition_variable condition_;
};

enum class IoStatus {
    Ok,
    Closed,
    Timeout,
    Stopped,
    Error
};

std::string nowTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&seconds, &local);
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

std::string makeRunId() {
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return std::to_string(::getpid()) + "-" + std::to_string(milliseconds);
}

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "Authentication (choose one):\n"
        << "  --account-id ID --password PASSWORD\n"
        << "  --accounts-file PATH       JSON array of {accountId,password}\n\n"
        << "Group (choose one):\n"
        << "  --group-id ID              Existing group; all accounts must be members\n"
        << "  --create-group NAME        Create a group with the single supplied account\n\n"
        << "Load options:\n"
        << "  --host HOST                Default 127.0.0.1\n"
        << "  --port PORT                Default 8080\n"
        << "  --clients N                Default 20\n"
        << "  --warmup SEC               Default 5\n"
        << "  --duration SEC             Default 60\n"
        << "  --rate RPS                 Per-client request rate, default 5\n"
        << "  --pacing MODE              staggered (default) or synchronized\n"
        << "  --payload-bytes N          Approximate message size, default 64\n"
        << "  --max-inflight N           Per-client pending limit, default 128\n"
        << "  --connect-timeout-ms N     Default 3000\n"
        << "  --request-timeout-ms N     Default 10000\n"
        << "  --drain-ms N               Default 15000\n"
        << "  --fanout-settle-ms N       Keep receiving pushes after responses, default 2000\n"
        << "  --progress-sec N           Progress interval, 0 disables\n"
        << "  --account-rate-limit N     Warning threshold, default 20 msg/s/account\n"
        << "  --server-pid PID           Sample server CPU and RSS from /proc\n"
        << "  --no-group-check           Skip pre-test membership check\n"
        << "  --allow-partial-ready      Run even if some clients fail setup\n"
        << "  --allow-account-reuse      Permit multiple clients to share one account\n"
        << "  --json-out PATH            Default load_test_result.json\n"
        << "  --csv-out PATH             Append one summary row\n"
        << "  --help\n\n"
        << "The password can also be supplied through PROJECT_CHAT_BENCH_PASSWORD.\n";
}

bool parseInteger(const std::string& value, int& output) {
    try {
        size_t consumed = 0;
        long parsed = std::stol(value, &consumed);
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
        unsigned long long parsed = std::stoull(value, &consumed);
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
        auto nextValue = [&]() -> std::optional<std::string> {
            if (index + 1 >= argc) {
                return std::nullopt;
            }
            return std::string(argv[++index]);
        };

        if (option == "--help") {
            printUsage(argv[0]);
            return false;
        }
        if (option == "--no-group-check") {
            args.verifyGroup = false;
            continue;
        }
        if (option == "--allow-partial-ready") {
            args.allowPartialReady = true;
            continue;
        }
        if (option == "--allow-account-reuse") {
            args.allowAccountReuse = true;
            continue;
        }

        const auto value = nextValue();
        if (!value) {
            std::cerr << "missing value for " << option << '\n';
            return false;
        }

        if (option == "--host") {
            args.host = *value;
        } else if (option == "--port") {
            if (!parseInteger(*value, args.port)) return false;
        } else if (option == "--clients") {
            if (!parseInteger(*value, args.clients)) return false;
        } else if (option == "--warmup") {
            if (!parseInteger(*value, args.warmupSec)) return false;
        } else if (option == "--duration") {
            if (!parseInteger(*value, args.durationSec)) return false;
        } else if (option == "--rate") {
            if (!parseDouble(*value, args.ratePerClient)) return false;
        } else if (option == "--pacing") {
            const auto mode = parsePacingMode(*value);
            if (!mode) {
                std::cerr << "invalid pacing mode: " << *value
                          << "; expected staggered or synchronized\n";
                return false;
            }
            args.pacingMode = *mode;
        } else if (option == "--connect-timeout-ms") {
            if (!parseInteger(*value, args.connectTimeoutMs)) return false;
        } else if (option == "--request-timeout-ms") {
            if (!parseInteger(*value, args.requestTimeoutMs)) return false;
        } else if (option == "--drain-ms") {
            if (!parseInteger(*value, args.drainMs)) return false;
        } else if (option == "--fanout-settle-ms") {
            if (!parseInteger(*value, args.fanoutSettleMs)) return false;
        } else if (option == "--progress-sec") {
            if (!parseInteger(*value, args.progressSec)) return false;
        } else if (option == "--max-inflight") {
            if (!parseSize(*value, args.maxInflight)) return false;
        } else if (option == "--payload-bytes") {
            if (!parseSize(*value, args.payloadBytes)) return false;
        } else if (option == "--account-rate-limit") {
            if (!parseInteger(*value, args.accountRateLimit)) return false;
        } else if (option == "--server-pid") {
            if (!parseInteger(*value, args.serverPid)) return false;
        } else if (option == "--account-id") {
            args.accountId = *value;
        } else if (option == "--password") {
            args.password = *value;
        } else if (option == "--accounts-file") {
            args.accountsFile = *value;
        } else if (option == "--group-id") {
            args.groupId = *value;
        } else if (option == "--create-group") {
            args.createGroupName = *value;
        } else if (option == "--json-out") {
            args.jsonOutput = *value;
        } else if (option == "--csv-out") {
            args.csvOutput = *value;
        } else {
            std::cerr << "unknown option: " << option << '\n';
            return false;
        }
    }

    if (args.password.empty()) {
        if (const char* password = std::getenv("PROJECT_CHAT_BENCH_PASSWORD")) {
            args.password = password;
        }
    }

    if (args.port <= 0 || args.port > 65535 || args.clients <= 0 ||
        args.warmupSec < 0 || args.durationSec <= 0 || args.ratePerClient <= 0.0 ||
        args.connectTimeoutMs <= 0 || args.requestTimeoutMs <= 0 || args.drainMs < 0 ||
        args.fanoutSettleMs < 0 ||
        args.progressSec < 0 || args.maxInflight == 0 || args.payloadBytes > 4096) {
        std::cerr << "invalid numeric argument\n";
        return false;
    }
    if (args.accountsFile.empty() && (args.accountId.empty() || args.password.empty())) {
        std::cerr << "provide --accounts-file or --account-id with password\n";
        return false;
    }
    if (!args.accountsFile.empty() && !args.accountId.empty()) {
        std::cerr << "--accounts-file and --account-id are mutually exclusive\n";
        return false;
    }
    if (args.groupId.empty() == args.createGroupName.empty()) {
        std::cerr << "provide exactly one of --group-id or --create-group\n";
        return false;
    }
    return true;
}

std::optional<std::vector<Credential>> loadCredentials(const Args& args) {
    if (args.accountsFile.empty()) {
        return std::vector<Credential>{{args.accountId, args.password}};
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
        std::cerr << "accounts file must contain a non-empty JSON array\n";
        return std::nullopt;
    }

    std::vector<Credential> credentials;
    for (const auto& entry : *accounts) {
        if (!entry.is_object() || !entry.contains("accountId") || !entry.contains("password") ||
            !entry.at("accountId").is_string() || !entry.at("password").is_string()) {
            std::cerr << "each account requires string accountId and password\n";
            return std::nullopt;
        }
        Credential credential{entry.at("accountId").get<std::string>(),
                              entry.at("password").get<std::string>()};
        if (credential.accountId.empty() || credential.password.empty()) {
            std::cerr << "accountId and password cannot be empty\n";
            return std::nullopt;
        }
        credentials.push_back(std::move(credential));
    }
    return credentials;
}

bool waitForFd(int fd, short events, int timeoutMs) {
    pollfd descriptor{fd, events, 0};
    while (true) {
        const int result = ::poll(&descriptor, 1, timeoutMs);
        if (result > 0) {
            return (descriptor.revents & events) != 0 ||
                   (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0;
        }
        if (result == 0) {
            return false;
        }
        if (errno != EINTR) {
            return false;
        }
    }
}

int connectSocket(const Args& args) {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return -1;
    }

    const int originalFlags = ::fcntl(fd, F_GETFL, 0);
    if (originalFlags < 0 || ::fcntl(fd, F_SETFL, originalFlags | O_NONBLOCK) < 0) {
        ::close(fd);
        return -1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(args.port));
    if (::inet_pton(AF_INET, args.host.c_str(), &address.sin_addr) != 1) {
        ::close(fd);
        return -1;
    }

    int result = ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (result < 0 && errno != EINPROGRESS) {
        ::close(fd);
        return -1;
    }
    if (result < 0) {
        if (!waitForFd(fd, POLLOUT, args.connectTimeoutMs)) {
            ::close(fd);
            return -1;
        }
        int socketError = 0;
        socklen_t errorLength = sizeof(socketError);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &errorLength) < 0 || socketError != 0) {
            ::close(fd);
            return -1;
        }
    }

    if (::fcntl(fd, F_SETFL, originalFlags) < 0) {
        ::close(fd);
        return -1;
    }
    int enabled = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
    ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled));
    return fd;
}

IoStatus sendAll(int fd, const void* data, size_t length, int timeoutMs,
                 const std::atomic<bool>* running = nullptr) {
    const char* cursor = static_cast<const char*>(data);
    const TimePoint deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (length > 0) {
        if (running && !running->load(std::memory_order_acquire)) {
            return IoStatus::Stopped;
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        if (remaining.count() <= 0 || !waitForFd(fd, POLLOUT, static_cast<int>(remaining.count()))) {
            return IoStatus::Timeout;
        }
        const ssize_t written = ::send(fd, cursor, length, MSG_NOSIGNAL);
        if (written > 0) {
            cursor += written;
            length -= static_cast<size_t>(written);
            continue;
        }
        if (written == 0) {
            return IoStatus::Closed;
        }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
        }
        return IoStatus::Error;
    }
    return IoStatus::Ok;
}

IoStatus sendFrame(int fd, const std::string& payload, int timeoutMs,
                   const std::atomic<bool>* running = nullptr) {
    if (payload.empty() || payload.size() > std::numeric_limits<uint32_t>::max()) {
        return IoStatus::Error;
    }
    const uint32_t networkLength = htonl(static_cast<uint32_t>(payload.size()));
    const IoStatus header = sendAll(fd, &networkLength, sizeof(networkLength), timeoutMs, running);
    if (header != IoStatus::Ok) {
        return header;
    }
    return sendAll(fd, payload.data(), payload.size(), timeoutMs, running);
}

IoStatus receiveAll(int fd, void* data, size_t length, const std::atomic<bool>* running,
                    std::optional<TimePoint> deadline) {
    char* cursor = static_cast<char*>(data);
    while (length > 0) {
        if (running && !running->load(std::memory_order_acquire)) {
            return IoStatus::Stopped;
        }
        int pollTimeoutMs = 500;
        if (deadline) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - Clock::now());
            if (remaining.count() <= 0) {
                return IoStatus::Timeout;
            }
            pollTimeoutMs = static_cast<int>(std::min<int64_t>(remaining.count(), pollTimeoutMs));
        }
        pollfd descriptor{fd, POLLIN, 0};
        const int pollResult = ::poll(&descriptor, 1, pollTimeoutMs);
        if (pollResult == 0) {
            continue;
        }
        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }
            return IoStatus::Error;
        }
        const ssize_t received = ::recv(fd, cursor, length, 0);
        if (received > 0) {
            cursor += received;
            length -= static_cast<size_t>(received);
            continue;
        }
        if (received == 0) {
            return IoStatus::Closed;
        }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
        }
        return IoStatus::Error;
    }
    return IoStatus::Ok;
}

IoStatus receiveFrame(int fd, std::string& payload, const std::atomic<bool>* running,
                      std::optional<TimePoint> deadline = std::nullopt) {
    uint32_t networkLength = 0;
    const IoStatus header = receiveAll(fd, &networkLength, sizeof(networkLength), running, deadline);
    if (header != IoStatus::Ok) {
        return header;
    }
    const uint32_t length = ntohl(networkLength);
    if (length == 0 || length > kMaxFrameLength) {
        return IoStatus::Error;
    }
    payload.assign(length, '\0');
    return receiveAll(fd, payload.data(), payload.size(), running, deadline);
}

json makeRequest(im::MsgType type, uint64_t requestId, const std::string& accountId) {
    return json{{"ver", 1},
                {"type", im::msgTypeToInt(type)},
                {"req_id", requestId},
                {"seq", requestId},
                {"from", accountId},
                {"to", ""}};
}

uint64_t jsonUnsigned(const json& object, std::string_view key) {
    const auto iterator = object.find(std::string(key));
    if (iterator == object.end()) {
        return 0;
    }
    if (iterator->is_number_unsigned()) {
        return iterator->get<uint64_t>();
    }
    if (iterator->is_number_integer()) {
        const int64_t value = iterator->get<int64_t>();
        return value > 0 ? static_cast<uint64_t>(value) : 0;
    }
    return 0;
}

uint64_t jsonUnsignedAny(const json& object,
                         std::initializer_list<std::string_view> keys) {
    for (const std::string_view key : keys) {
        if (object.contains(std::string(key))) {
            return jsonUnsigned(object, key);
        }
    }
    return 0;
}

bool isResponseType(const json& message, im::MsgType expected, uint64_t requestId) {
    return message.value("req_id", 0ULL) == requestId &&
           (message.value("type", 0U) == im::msgTypeToInt(expected) ||
            message.value("type", 0U) == im::msgTypeToInt(im::MsgType::ERR));
}

bool requestResponse(int fd, const json& request, im::MsgType expected, int timeoutMs, json& response) {
    const std::string payload = request.dump();
    if (sendFrame(fd, payload, timeoutMs) != IoStatus::Ok) {
        return false;
    }
    const uint64_t requestId = request.at("req_id").get<uint64_t>();
    const TimePoint deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (Clock::now() < deadline) {
        std::string frame;
        if (receiveFrame(fd, frame, nullptr, deadline) != IoStatus::Ok) {
            return false;
        }
        if (frame == "PING") {
            if (sendFrame(fd, "PONG", timeoutMs) != IoStatus::Ok) {
                return false;
            }
            continue;
        }
        json message = json::parse(frame, nullptr, false);
        if (!message.is_discarded() && isResponseType(message, expected, requestId)) {
            response = std::move(message);
            return true;
        }
    }
    return false;
}

bool loginSocket(int fd, const Credential& credential, int timeoutMs, uint64_t& requestId,
                 json* loginResponse = nullptr) {
    json request = makeRequest(im::MsgType::LOGIN_REQ, requestId++, credential.accountId);
    request["accountId"] = credential.accountId;
    request["password"] = credential.password;
    json response;
    if (!requestResponse(fd, request, im::MsgType::LOGIN_RESP, timeoutMs, response) ||
        !response.value("ok", false)) {
        if (loginResponse) {
            *loginResponse = std::move(response);
        }
        return false;
    }
    if (loginResponse) {
        *loginResponse = std::move(response);
    }
    return true;
}

bool createGroup(Args& args, const Credential& owner) {
    int fd = connectSocket(args);
    if (fd < 0) {
        std::cerr << "setup connection failed\n";
        return false;
    }
    uint64_t requestId = 1;
    json loginResponse;
    if (!loginSocket(fd, owner, args.requestTimeoutMs, requestId, &loginResponse)) {
        std::cerr << "setup login failed: " << loginResponse.dump() << '\n';
        ::close(fd);
        return false;
    }
    json request = makeRequest(im::MsgType::CREATE_GROUP_REQ, requestId++, owner.accountId);
    request["groupName"] = args.createGroupName;
    json response;
    const bool success = requestResponse(fd, request, im::MsgType::CREATE_GROUP_RESP,
                                         args.requestTimeoutMs, response) &&
                         response.value("ok", false) && response.contains("data") &&
                         response.at("data").contains("groupId") &&
                         response.at("data").at("groupId").is_string();
    if (success) {
        args.groupId = response.at("data").at("groupId").get<std::string>();
        std::cerr << "created benchmark group: " << args.groupId << '\n';
    } else {
        std::cerr << "create group failed: " << response.dump() << '\n';
    }
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
    return success;
}

void updateMaximum(std::atomic<uint64_t>& target, uint64_t value) {
    uint64_t current = target.load(std::memory_order_relaxed);
    while (current < value &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

class Worker {
public:
    Worker(int id, const Args& args, Credential credential, Metrics& metrics,
           StartGate& gate)
        : id_(id), args_(args), credential_(std::move(credential)), metrics_(metrics), gate_(gate) {}

    void run() noexcept {
        bool reported = false;
        try {
            if (!handshake()) {
                gate_.report(false);
                reported = true;
                cleanup();
                return;
            }

            running_.store(true, std::memory_order_release);
            receiver_ = std::thread(&Worker::receiveLoop, this);
            gate_.report(true);
            reported = true;

            const auto start = gate_.waitForStart();
            if (!start) {
                stop();
                return;
            }
            std::this_thread::sleep_until(*start);
            sendLoop(*start);
            drainAndStop();
        } catch (const std::exception& exception) {
            std::cerr << "worker " << id_ << " exception: " << exception.what() << '\n';
            metrics_.workerExceptions.fetch_add(1, std::memory_order_relaxed);
            if (!reported) {
                gate_.report(false);
            }
            stop();
        } catch (...) {
            std::cerr << "worker " << id_ << " unknown exception\n";
            metrics_.workerExceptions.fetch_add(1, std::memory_order_relaxed);
            if (!reported) {
                gate_.report(false);
            }
            stop();
        }
    }

private:
    bool handshake() {
        fd_ = connectSocket(args_);
        if (fd_ < 0) {
            metrics_.connectFail.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        metrics_.connectOk.fetch_add(1, std::memory_order_relaxed);

        uint64_t requestId = nextRequestId_.fetch_add(1, std::memory_order_relaxed);
        json login = makeRequest(im::MsgType::LOGIN_REQ, requestId, credential_.accountId);
        login["accountId"] = credential_.accountId;
        login["password"] = credential_.password;
        json loginResponse;
        if (!requestResponse(fd_, login, im::MsgType::LOGIN_RESP, args_.requestTimeoutMs,
                             loginResponse) || !loginResponse.value("ok", false)) {
            metrics_.loginFail.fetch_add(1, std::memory_order_relaxed);
            std::cerr << "worker " << id_ << " login failed: " << loginResponse.dump() << '\n';
            return false;
        }
        metrics_.loginOk.fetch_add(1, std::memory_order_relaxed);

        if (args_.verifyGroup) {
            requestId = nextRequestId_.fetch_add(1, std::memory_order_relaxed);
            json request = makeRequest(im::MsgType::GROUP_MEMBERS_REQ, requestId,
                                       credential_.accountId);
            request["groupId"] = args_.groupId;
            json response;
            if (!requestResponse(fd_, request, im::MsgType::GROUP_MEMBERS_RESP,
                                 args_.requestTimeoutMs, response) ||
                !response.value("ok", false)) {
                metrics_.groupCheckFail.fetch_add(1, std::memory_order_relaxed);
                std::cerr << "worker " << id_ << " group check failed: " << response.dump() << '\n';
                return false;
            }
            metrics_.groupCheckOk.fetch_add(1, std::memory_order_relaxed);
        }

        metrics_.activeConnections.fetch_add(1, std::memory_order_relaxed);
        countedActive_ = true;
        return true;
    }

    std::string makeContent(uint64_t requestId, bool measured) const {
        std::string content = "bench:" + args_.runId + ':' + (measured ? "M:" : "W:") +
                              std::to_string(id_) + ':' + std::to_string(requestId) + ':';
        if (content.size() < args_.payloadBytes) {
            content.append(args_.payloadBytes - content.size(), 'x');
        }
        return content;
    }

    bool safeSend(const std::string& payload) {
        std::lock_guard<std::mutex> lock(writeMutex_);
        return sendFrame(fd_, payload, args_.requestTimeoutMs, &running_) == IoStatus::Ok;
    }

    Clock::duration initialPacingOffset() const {
        if (args_.pacingMode == PacingMode::Synchronized || args_.clients <= 1) {
            return Clock::duration::zero();
        }
        const double offsetSeconds =
            static_cast<double>(id_) /
            (static_cast<double>(args_.clients) * args_.ratePerClient);
        return std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(offsetSeconds));
    }

    void sendLoop(TimePoint start) {
        const TimePoint measurementStart = start + std::chrono::seconds(args_.warmupSec);
        const TimePoint end = measurementStart + std::chrono::seconds(args_.durationSec);
        const auto interval = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(1.0 / args_.ratePerClient));
        TimePoint nextSend = start + initialPacingOffset();
        TimePoint nextSweep = start;

        while (running_.load(std::memory_order_acquire) && Clock::now() < end) {
            TimePoint now = Clock::now();
            if (now >= nextSweep) {
                expirePending(now);
                nextSweep = now + std::chrono::milliseconds(50);
            }

            if (pendingSize() >= args_.maxInflight) {
                metrics_.inflightThrottled.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (now < nextSend) {
                std::this_thread::sleep_until(std::min(nextSend, nextSweep));
                continue;
            }

            if (now - nextSend > interval) {
                metrics_.scheduleLagEvents.fetch_add(1, std::memory_order_relaxed);
                nextSend = now;
            }

            const bool measured = now >= measurementStart;
            const uint64_t requestId = nextRequestId_.fetch_add(1, std::memory_order_relaxed);
            json request = makeRequest(im::MsgType::GROUP_MSG_REQ, requestId,
                                       credential_.accountId);
            request["groupId"] = args_.groupId;
            request["content"] = makeContent(requestId, measured);
            const std::string payload = request.dump();

            if (measured) {
                metrics_.attempted.fetch_add(1, std::memory_order_relaxed);
            }
            {
                std::lock_guard<std::mutex> lock(pendingMutex_);
                pending_.emplace(requestId, PendingRequest{Clock::now(), measured});
                updateMaximum(metrics_.peakInflight, pending_.size());
            }

            if (!safeSend(payload)) {
                {
                    std::lock_guard<std::mutex> lock(pendingMutex_);
                    pending_.erase(requestId);
                }
                if (measured) {
                    metrics_.sendFail.fetch_add(1, std::memory_order_relaxed);
                } else {
                    metrics_.warmupError.fetch_add(1, std::memory_order_relaxed);
                }
                running_.store(false, std::memory_order_release);
                break;
            }

            const uint64_t frameBytes = sizeof(uint32_t) + payload.size();
            metrics_.appBytesSent.fetch_add(frameBytes, std::memory_order_relaxed);
            if (measured) {
                metrics_.sent.fetch_add(1, std::memory_order_relaxed);
                metrics_.measuredBytesSent.fetch_add(frameBytes, std::memory_order_relaxed);
            } else {
                metrics_.warmupSent.fetch_add(1, std::memory_order_relaxed);
            }
            nextSend += interval;
        }
    }

    size_t pendingSize() {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        return pending_.size();
    }

    void expirePending(TimePoint now) {
        const auto timeout = std::chrono::milliseconds(args_.requestTimeoutMs);
        std::lock_guard<std::mutex> lock(pendingMutex_);
        for (auto iterator = pending_.begin(); iterator != pending_.end();) {
            if (now - iterator->second.begin < timeout) {
                ++iterator;
                continue;
            }
            if (iterator->second.measured) {
                metrics_.timeout.fetch_add(1, std::memory_order_relaxed);
            } else {
                metrics_.warmupTimeout.fetch_add(1, std::memory_order_relaxed);
            }
            timedOut_.insert(iterator->first);
            iterator = pending_.erase(iterator);
        }
        if (timedOut_.size() > args_.maxInflight * 8) {
            timedOut_.clear();
        }
    }

    void receiveLoop() noexcept {
        try {
            receiveLoopImpl();
        } catch (const std::exception& exception) {
            std::cerr << "worker " << id_ << " receive exception: " << exception.what() << '\n';
            metrics_.workerExceptions.fetch_add(1, std::memory_order_relaxed);
            metrics_.recvFail.fetch_add(1, std::memory_order_relaxed);
            running_.store(false, std::memory_order_release);
        } catch (...) {
            std::cerr << "worker " << id_ << " unknown receive exception\n";
            metrics_.workerExceptions.fetch_add(1, std::memory_order_relaxed);
            metrics_.recvFail.fetch_add(1, std::memory_order_relaxed);
            running_.store(false, std::memory_order_release);
        }
    }

    void receiveLoopImpl() {
        while (running_.load(std::memory_order_acquire)) {
            std::string frame;
            const IoStatus status = receiveFrame(fd_, frame, &running_);
            if (status == IoStatus::Stopped) {
                break;
            }
            if (status != IoStatus::Ok) {
                if (!closing_.load(std::memory_order_acquire)) {
                    metrics_.recvFail.fetch_add(1, std::memory_order_relaxed);
                }
                running_.store(false, std::memory_order_release);
                break;
            }

            const uint64_t frameBytes = sizeof(uint32_t) + frame.size();
            metrics_.appBytesRecv.fetch_add(frameBytes, std::memory_order_relaxed);
            if (frame == "PING") {
                if (!safeSend("PONG") && !closing_.load(std::memory_order_acquire)) {
                    metrics_.sendFail.fetch_add(1, std::memory_order_relaxed);
                    running_.store(false, std::memory_order_release);
                }
                continue;
            }

            json message = json::parse(frame, nullptr, false);
            if (message.is_discarded()) {
                metrics_.parseFail.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            const uint32_t type = message.value("type", 0U);
            if (type == im::msgTypeToInt(im::MsgType::GROUP_MSG_PUSH)) {
                handlePush(message, frameBytes);
                continue;
            }
            if (type == im::msgTypeToInt(im::MsgType::GROUP_EVENT_PUSH) ||
                type == im::msgTypeToInt(im::MsgType::FRIEND_EVENT_PUSH) ||
                type == im::msgTypeToInt(im::MsgType::DM_PUSH)) {
                metrics_.otherPushRecv.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            if (type != im::msgTypeToInt(im::MsgType::GROUP_MSG_RESP) &&
                type != im::msgTypeToInt(im::MsgType::ERR)) {
                continue;
            }
            handleResponse(message, frameBytes);
        }
    }

    void handlePush(const json& message, uint64_t frameBytes) {
        std::string content;
        if (message.contains("data") && message.at("data").is_object()) {
            const json& data = message.at("data");
            if (data.contains("content") && data.at("content").is_string()) {
                content = data.at("content").get<std::string>();
            }
        }
        const std::string measuredPrefix = "bench:" + args_.runId + ":M:";
        const std::string warmupPrefix = "bench:" + args_.runId + ":W:";
        if (content.starts_with(measuredPrefix)) {
            metrics_.pushRecv.fetch_add(1, std::memory_order_relaxed);
            metrics_.measuredBytesRecv.fetch_add(frameBytes, std::memory_order_relaxed);
        } else if (content.starts_with(warmupPrefix)) {
            metrics_.warmupPushRecv.fetch_add(1, std::memory_order_relaxed);
        } else {
            metrics_.otherPushRecv.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void handleResponse(const json& message, uint64_t frameBytes) {
        const uint64_t requestId = jsonUnsigned(message, "req_id");
        PendingRequest pending;
        bool matched = false;
        bool late = false;
        {
            std::lock_guard<std::mutex> lock(pendingMutex_);
            const auto iterator = pending_.find(requestId);
            if (iterator != pending_.end()) {
                pending = iterator->second;
                pending_.erase(iterator);
                matched = true;
            } else {
                const auto timeoutIterator = timedOut_.find(requestId);
                if (timeoutIterator != timedOut_.end()) {
                    timedOut_.erase(timeoutIterator);
                    late = true;
                }
            }
        }
        if (!matched) {
            if (late) {
                metrics_.lateResponses.fetch_add(1, std::memory_order_relaxed);
            } else {
                metrics_.unmatchedResponses.fetch_add(1, std::memory_order_relaxed);
            }
            return;
        }

        const bool ok = message.value("type", 0U) ==
                            im::msgTypeToInt(im::MsgType::GROUP_MSG_RESP) &&
                        message.value("ok", false);
        if (!pending.measured) {
            if (ok) {
                metrics_.warmupOk.fetch_add(1, std::memory_order_relaxed);
            } else {
                metrics_.warmupError.fetch_add(1, std::memory_order_relaxed);
            }
            return;
        }

        metrics_.measuredBytesRecv.fetch_add(frameBytes, std::memory_order_relaxed);
        if (!ok) {
            metrics_.serverErrorResponses.fetch_add(1, std::memory_order_relaxed);
            const int code = message.value("code", -1);
            std::lock_guard<std::mutex> lock(metrics_.errorMutex);
            ++metrics_.errorCodes[code];
            return;
        }

        if (message.contains("data") && message.at("data").is_object()) {
            const json& data = message.at("data");
            const uint64_t fanoutSent = jsonUnsignedAny(data, {"fanoutSent", "sent"});
            const uint64_t fanoutDropped = jsonUnsignedAny(data, {"fanoutDropped", "dropped"});
            const uint64_t fanoutClosed = jsonUnsignedAny(data, {"fanoutClosed", "closed"});
            const uint64_t fanoutOverloaded = jsonUnsignedAny(data, {"fanoutOverloaded", "overloaded"});
            const uint64_t fanoutFailed = jsonUnsignedAny(data, {"fanoutFailed", "failed"});
            uint64_t fanoutNoSuchConnection =
                jsonUnsignedAny(data, {"fanoutNoSuchConnection", "noSuchConnection"});
            if (fanoutNoSuchConnection == 0 &&
                fanoutDropped > fanoutClosed + fanoutOverloaded + fanoutFailed) {
                fanoutNoSuchConnection =
                    fanoutDropped - fanoutClosed - fanoutOverloaded - fanoutFailed;
            }

            metrics_.serverFanoutSent.fetch_add(fanoutSent, std::memory_order_relaxed);
            metrics_.serverDropped.fetch_add(fanoutDropped, std::memory_order_relaxed);
            metrics_.serverOverloaded.fetch_add(fanoutOverloaded, std::memory_order_relaxed);
            metrics_.serverClosed.fetch_add(fanoutClosed, std::memory_order_relaxed);
            metrics_.serverFanoutFailed.fetch_add(fanoutFailed, std::memory_order_relaxed);
            metrics_.serverNoSuchConnection.fetch_add(fanoutNoSuchConnection,
                                                       std::memory_order_relaxed);

            if (data.contains("queueWaitUs") || data.contains("persistUs")) {
                std::lock_guard<std::mutex> lock(metrics_.persistenceMutex);
                if (data.contains("queueWaitUs")) {
                    metrics_.queueWaitMs.push_back(
                        static_cast<double>(jsonUnsigned(data, "queueWaitUs")) / 1000.0);
                }
                if (data.contains("persistUs")) {
                    metrics_.persistMs.push_back(
                        static_cast<double>(jsonUnsigned(data, "persistUs")) / 1000.0);
                }
            }
        }

        const double latencyMs =
            std::chrono::duration<double, std::milli>(Clock::now() - pending.begin).count();
        {
            std::lock_guard<std::mutex> lock(metrics_.latencyMutex);
            metrics_.successLatenciesMs.push_back(latencyMs);
        }
        metrics_.responseOk.fetch_add(1, std::memory_order_relaxed);
    }

    void drainAndStop() {
        const TimePoint drainDeadline = Clock::now() + std::chrono::milliseconds(args_.drainMs);
        while (running_.load(std::memory_order_acquire) && Clock::now() < drainDeadline) {
            expirePending(Clock::now());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        expireAllPending();
        const TimePoint settleDeadline =
            Clock::now() + std::chrono::milliseconds(args_.fanoutSettleMs);
        while (running_.load(std::memory_order_acquire) && Clock::now() < settleDeadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        stop();
    }

    void expireAllPending() {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        for (const auto& [requestId, request] : pending_) {
            if (request.measured) {
                metrics_.timeout.fetch_add(1, std::memory_order_relaxed);
            } else {
                metrics_.warmupTimeout.fetch_add(1, std::memory_order_relaxed);
            }
            timedOut_.insert(requestId);
        }
        pending_.clear();
    }

    void stop() {
        closing_.store(true, std::memory_order_release);
        running_.store(false, std::memory_order_release);
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
        }
        if (receiver_.joinable() && receiver_.get_id() != std::this_thread::get_id()) {
            receiver_.join();
        }
        cleanup();
    }

    void cleanup() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        if (countedActive_) {
            metrics_.activeConnections.fetch_sub(1, std::memory_order_relaxed);
            countedActive_ = false;
        }
    }

    int id_{0};
    const Args& args_;
    Credential credential_;
    Metrics& metrics_;
    StartGate& gate_;
    int fd_{-1};
    bool countedActive_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> closing_{false};
    std::atomic<uint64_t> nextRequestId_{1};
    std::mutex writeMutex_;
    std::mutex pendingMutex_;
    std::unordered_map<uint64_t, PendingRequest> pending_;
    std::unordered_set<uint64_t> timedOut_;
    std::thread receiver_;
};

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

LatencyStats calculateStats(std::vector<double> values) {
    if (values.empty()) {
        return {};
    }
    std::sort(values.begin(), values.end());
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return LatencyStats{values.size(),
                        values.front(),
                        values.back(),
                        sum / static_cast<double>(values.size()),
                        percentile(values, 0.50),
                        percentile(values, 0.90),
                        percentile(values, 0.95),
                        percentile(values, 0.99),
                        percentile(values, 0.999)};
}

LatencyStats calculateLatencyStats(Metrics& metrics) {
    std::lock_guard<std::mutex> lock(metrics.latencyMutex);
    return calculateStats(metrics.successLatenciesMs);
}

std::pair<LatencyStats, LatencyStats> calculatePersistenceStats(Metrics& metrics) {
    std::lock_guard<std::mutex> lock(metrics.persistenceMutex);
    return {calculateStats(metrics.queueWaitMs), calculateStats(metrics.persistMs)};
}

json statsToJson(const LatencyStats& stats) {
    return json{{"samples", stats.samples},
                {"min", stats.minimumMs},
                {"avg", stats.averageMs},
                {"p50", stats.p50Ms},
                {"p90", stats.p90Ms},
                {"p95", stats.p95Ms},
                {"p99", stats.p99Ms},
                {"p999", stats.p999Ms},
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
            break;
        }
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

void ensureParentDirectory(const std::string& path) {
    if (path.empty()) {
        return;
    }
    const std::filesystem::path file(path);
    if (file.has_parent_path()) {
        std::filesystem::create_directories(file.parent_path());
    }
}

void appendCsv(const std::string& path, const json& result) {
    if (path.empty()) {
        return;
    }
    ensureParentDirectory(path);
    const bool needsHeader = !std::filesystem::exists(path) || std::filesystem::file_size(path) == 0;
    const std::string header =
        "timestamp,run_id,clients_requested,clients_ready,duration_s,rate_per_client,pacing_mode,"
        "sent,response_ok,success_rate,ok_qps,p50_ms,p95_ms,p99_ms,timeout,"
        "server_error,push_recv,fanout_sent,fanout_dropped,push_delivery_ratio,"
        "queue_wait_p95_ms,persist_p95_ms,server_cpu_pct,server_rss_peak_kb";
    if (!needsHeader) {
        std::ifstream existing(path);
        std::string existingHeader;
        std::getline(existing, existingHeader);
        if (existingHeader != header) {
            throw std::runtime_error(
                "CSV schema mismatch; use a new --csv-out file for this load-test version");
        }
    }
    std::ofstream output(path, std::ios::app);
    if (!output) {
        throw std::runtime_error("cannot open CSV output: " + path);
    }
    if (needsHeader) {
        output << header << '\n';
    }
    output << result.at("timestamp").get<std::string>() << ','
           << result.at("run_id").get<std::string>() << ','
           << result.at("config").at("clients_requested") << ','
           << result.at("connections").at("ready") << ','
           << result.at("config").at("duration_s") << ','
           << result.at("config").at("rate_per_client") << ','
           << result.at("config").at("pacing_mode").get<std::string>() << ','
           << result.at("requests").at("sent") << ','
           << result.at("requests").at("response_ok") << ','
           << result.at("requests").at("success_rate") << ','
           << result.at("throughput").at("ok_qps") << ','
           << result.at("latency_ms").at("p50") << ','
           << result.at("latency_ms").at("p95") << ','
           << result.at("latency_ms").at("p99") << ','
           << result.at("requests").at("timeout") << ','
           << result.at("requests").at("server_error_response") << ','
           << result.at("fanout").at("push_recv") << ','
           << result.at("fanout").at("server_reported_sent") << ','
           << result.at("fanout").at("server_reported_dropped") << ','
           << result.at("fanout").at("push_delivery_ratio") << ','
           << result.at("persistence_ms").at("queue_wait").at("p95") << ','
           << result.at("persistence_ms").at("sql_transaction").at("p95") << ','
           << result.at("process").at("server").at("cpu_percent") << ','
           << result.at("process").at("server").at("rss_peak_kb") << '\n';
}

json buildResult(const Args& args, Metrics& metrics, int readyClients,
                 double elapsedSeconds, const LatencyStats& latency,
                 const LatencyStats& queueWait, const LatencyStats& persist,
                 const ProcessSnapshot& loadBegin, const ProcessSnapshot& loadEnd,
                 uint64_t loadPeakRss, const ProcessSnapshot& serverBegin,
                 const ProcessSnapshot& serverEnd, uint64_t serverPeakRss,
                 const std::vector<std::string>& warnings) {
    const uint64_t sent = metrics.sent.load();
    const uint64_t ok = metrics.responseOk.load();
    const uint64_t serverErrors = metrics.serverErrorResponses.load();
    const uint64_t timeouts = metrics.timeout.load();
    const uint64_t transportErrors = metrics.sendFail.load() + metrics.recvFail.load() +
                                     metrics.workerExceptions.load();
    const uint64_t push = metrics.pushRecv.load();
    const uint64_t fanoutSent = metrics.serverFanoutSent.load();
    const uint64_t fanoutDropped = metrics.serverDropped.load();
    const uint64_t fanoutAttempted = fanoutSent + fanoutDropped;
    const uint64_t theoreticalFanout = readyClients > 1
        ? ok * static_cast<uint64_t>(readyClients - 1)
        : 0;
    const double duration = static_cast<double>(args.durationSec);
    const double successRate = sent > 0 ? static_cast<double>(ok) / static_cast<double>(sent) : 0.0;
    const double pushDelivery = fanoutSent > 0
        ? static_cast<double>(push) / static_cast<double>(fanoutSent)
        : 0.0;
    const double pushVsTheoretical = theoreticalFanout > 0
        ? static_cast<double>(push) / static_cast<double>(theoreticalFanout)
        : 0.0;
    const double fanoutCoverage = theoreticalFanout > 0
        ? static_cast<double>(fanoutAttempted) / static_cast<double>(theoreticalFanout)
        : 0.0;
    const double phaseSpreadMs = args.pacingMode == PacingMode::Staggered && args.clients > 1
        ? 1000.0 / args.ratePerClient *
              static_cast<double>(args.clients - 1) / static_cast<double>(args.clients)
        : 0.0;

    std::map<std::string, uint64_t> errorCodes;
    {
        std::lock_guard<std::mutex> lock(metrics.errorMutex);
        for (const auto& [code, count] : metrics.errorCodes) {
            errorCodes[std::to_string(code)] = count;
        }
    }

    json result{
        {"timestamp", nowTimestamp()},
        {"schema_version", 3},
        {"run_id", args.runId},
        {"warnings", warnings},
        {"config", {
            {"host", args.host},
            {"port", args.port},
            {"clients_requested", args.clients},
            {"credential_count", nullptr},
            {"group_id", args.groupId},
            {"warmup_s", args.warmupSec},
            {"duration_s", args.durationSec},
            {"elapsed_s", elapsedSeconds},
            {"rate_per_client", args.ratePerClient},
            {"target_qps", readyClients * args.ratePerClient},
            {"pacing_mode", std::string(pacingModeToString(args.pacingMode))},
            {"phase_spread_ms", phaseSpreadMs},
            {"payload_bytes", args.payloadBytes},
            {"max_inflight_per_client", args.maxInflight},
            {"request_timeout_ms", args.requestTimeoutMs},
            {"drain_ms", args.drainMs},
            {"fanout_settle_ms", args.fanoutSettleMs},
            {"allow_partial_ready", args.allowPartialReady},
            {"allow_account_reuse", args.allowAccountReuse}
        }},
        {"connections", {
            {"ready", readyClients},
            {"connect_ok", metrics.connectOk.load()},
            {"connect_fail", metrics.connectFail.load()},
            {"login_ok", metrics.loginOk.load()},
            {"login_fail", metrics.loginFail.load()},
            {"group_check_ok", metrics.groupCheckOk.load()},
            {"group_check_fail", metrics.groupCheckFail.load()},
            {"recv_fail", metrics.recvFail.load()},
            {"worker_exceptions", metrics.workerExceptions.load()}
        }},
        {"requests", {
            {"attempted", metrics.attempted.load()},
            {"sent", sent},
            {"response_ok", ok},
            {"success_rate", successRate},
            {"server_error_response", serverErrors},
            {"timeout", timeouts},
            {"error_total", transportErrors + serverErrors + timeouts +
                                metrics.parseFail.load() + metrics.unmatchedResponses.load()},
            {"transport_error_total", transportErrors},
            {"late_response", metrics.lateResponses.load()},
            {"unmatched_response", metrics.unmatchedResponses.load()},
            {"send_fail", metrics.sendFail.load()},
            {"parse_fail", metrics.parseFail.load()},
            {"inflight_throttled", metrics.inflightThrottled.load()},
            {"schedule_lag_events", metrics.scheduleLagEvents.load()},
            {"peak_inflight", metrics.peakInflight.load()},
            {"error_codes", errorCodes}
        }},
        {"warmup", {
            {"sent", metrics.warmupSent.load()},
            {"response_ok", metrics.warmupOk.load()},
            {"error", metrics.warmupError.load()},
            {"timeout", metrics.warmupTimeout.load()},
            {"push_recv", metrics.warmupPushRecv.load()}
        }},
        {"latency_ms", statsToJson(latency)},
        {"persistence_ms", {
            {"queue_wait", statsToJson(queueWait)},
            {"sql_transaction", statsToJson(persist)}
        }},
        {"throughput", {
            {"attempted_qps", duration > 0 ? metrics.attempted.load() / duration : 0.0},
            {"sent_qps", duration > 0 ? sent / duration : 0.0},
            {"ok_qps", duration > 0 ? ok / duration : 0.0},
            {"push_qps", duration > 0 ? push / duration : 0.0},
            {"total_events_qps", duration > 0 ? (ok + push) / duration : 0.0}
        }},
        {"fanout", {
            {"push_recv", push},
            {"other_push_recv", metrics.otherPushRecv.load()},
            {"server_reported_sent", fanoutSent},
            {"server_reported_dropped", fanoutDropped},
            {"server_reported_attempted", fanoutAttempted},
            {"server_reported_overloaded", metrics.serverOverloaded.load()},
            {"server_reported_closed", metrics.serverClosed.load()},
            {"server_reported_no_such_connection", metrics.serverNoSuchConnection.load()},
            {"server_reported_failed", metrics.serverFanoutFailed.load()},
            {"theoretical_fanout", theoreticalFanout},
            {"push_delivery_ratio", pushDelivery},
            {"push_vs_theoretical_ratio", pushVsTheoretical},
            {"target_coverage_ratio", fanoutCoverage}
        }},
        {"traffic", {
            {"app_bytes_sent_total", metrics.appBytesSent.load()},
            {"app_bytes_recv_total", metrics.appBytesRecv.load()},
            {"app_bytes_sent_measured", metrics.measuredBytesSent.load()},
            {"app_bytes_recv_measured", metrics.measuredBytesRecv.load()},
            {"send_mib_per_s", duration > 0
                ? metrics.measuredBytesSent.load() / duration / 1024.0 / 1024.0 : 0.0},
            {"recv_mib_per_s", duration > 0
                ? metrics.measuredBytesRecv.load() / duration / 1024.0 / 1024.0 : 0.0}
        }},
        {"process", {
            {"load_generator", {
                {"pid", ::getpid()},
                {"cpu_percent", calculateCpuPercent(loadBegin, loadEnd, duration)},
                {"rss_start_kb", loadBegin.rssKb},
                {"rss_end_kb", loadEnd.rssKb},
                {"rss_peak_kb", std::max({loadPeakRss, loadEnd.highWaterKb, loadEnd.rssKb})}
            }},
            {"server", {
                {"pid", args.serverPid},
                {"sampled", serverBegin.valid && serverEnd.valid},
                {"cpu_percent", calculateCpuPercent(serverBegin, serverEnd, duration)},
                {"rss_start_kb", serverBegin.rssKb},
                {"rss_end_kb", serverEnd.rssKb},
                {"rss_peak_kb", std::max({serverPeakRss, serverEnd.highWaterKb, serverEnd.rssKb})}
            }}
        }},
        {"quality_gate", {
            {"all_clients_ready", readyClients == args.clients},
            {"request_success_ge_99_9pct", successRate >= 0.999},
            {"no_server_errors", serverErrors == 0},
            {"no_timeouts", timeouts == 0},
            {"no_transport_errors", metrics.sendFail.load() == 0 &&
                                    metrics.recvFail.load() == 0 &&
                                    metrics.workerExceptions.load() == 0},
            {"no_fanout_drop", fanoutDropped == 0},
            {"fanout_target_coverage_ge_99pct", theoreticalFanout == 0 ||
                                                fanoutCoverage >= 0.99},
            {"push_delivery_ge_99pct", theoreticalFanout == 0 ||
                                       (fanoutSent > 0 && pushDelivery >= 0.99)}
        }}
    };
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    ::signal(SIGPIPE, SIG_IGN);

    Args args;
    args.runId = makeRunId();
    if (!parseArgs(argc, argv, args)) {
        return argc > 1 && std::string(argv[1]) == "--help" ? 0 : 1;
    }

    auto credentialsResult = loadCredentials(args);
    if (!credentialsResult) {
        return 2;
    }
    std::vector<Credential> credentials = std::move(*credentialsResult);
    std::unordered_set<std::string> uniqueAccounts;
    for (const Credential& credential : credentials) {
        uniqueAccounts.insert(credential.accountId);
    }
    if (!args.allowAccountReuse && uniqueAccounts.size() < static_cast<size_t>(args.clients)) {
        std::cerr << "benchmark requires at least one unique account per client: clients="
                  << args.clients << " unique_accounts=" << uniqueAccounts.size()
                  << "; use --allow-account-reuse only for explicit multi-device tests\n";
        return 2;
    }
    if (!args.createGroupName.empty() && uniqueAccounts.size() != 1) {
        std::cerr << "--create-group supports one logical account only; use --group-id for multiple accounts\n";
        return 2;
    }
    if (!args.createGroupName.empty() && !createGroup(args, credentials.front())) {
        return 3;
    }

    std::vector<std::string> warnings;
    if (args.serverPid <= 0) {
        warnings.emplace_back(
            "server process was not sampled; pass --server-pid to record server CPU and RSS");
    }
    if (args.drainMs < args.requestTimeoutMs) {
        warnings.emplace_back(
            "drain-ms is shorter than request-timeout-ms and may create artificial timeouts");
    }
    if (args.pacingMode == PacingMode::Synchronized && args.clients > 1) {
        warnings.emplace_back(
            "synchronized pacing creates periodic client bursts; compare with staggered results");
    }
    std::unordered_map<std::string, int> clientsPerAccount;
    for (int index = 0; index < args.clients; ++index) {
        ++clientsPerAccount[credentials[static_cast<size_t>(index) % credentials.size()].accountId];
    }
    if (args.accountRateLimit > 0) {
        for (const auto& [account, count] : clientsPerAccount) {
            const double accountRate = count * args.ratePerClient;
            if (accountRate > args.accountRateLimit) {
                std::ostringstream warning;
                warning << "account " << account << " targets " << accountRate
                        << " msg/s, above configured warning threshold "
                        << args.accountRateLimit << " msg/s; expect rate-limit errors";
                warnings.push_back(warning.str());
                std::cerr << "warning: " << warning.str() << '\n';
            }
        }
    }

    Metrics metrics;
    StartGate gate(args.clients);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(args.clients));
    const TimePoint fullBegin = Clock::now();

    for (int index = 0; index < args.clients; ++index) {
        Credential credential = credentials[static_cast<size_t>(index) % credentials.size()];
        threads.emplace_back([&, index, credential = std::move(credential)]() mutable {
            Worker worker(index, args, std::move(credential), metrics, gate);
            worker.run();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    const int readyClients = gate.waitUntilReported();
    if (readyClients == 0 || (!args.allowPartialReady && readyClients != args.clients)) {
        gate.open(Clock::now(), true);
        for (auto& thread : threads) {
            thread.join();
        }
        std::cerr << "client setup incomplete: ready=" << readyClients
                  << '/' << args.clients << '\n';
        return 4;
    }

    const TimePoint start = Clock::now() + std::chrono::seconds(1);
    const TimePoint measurementStart = start + std::chrono::seconds(args.warmupSec);
    const TimePoint measurementEnd = measurementStart + std::chrono::seconds(args.durationSec);
    gate.open(start, false);
    std::cerr << "ready=" << readyClients << '/' << args.clients
              << " group=" << args.groupId
              << " target_qps=" << readyClients * args.ratePerClient
              << " pacing=" << pacingModeToString(args.pacingMode) << '\n';

    std::this_thread::sleep_until(measurementStart);
    const ProcessSnapshot loadBegin = readProcessSnapshot(0);
    const ProcessSnapshot serverBegin = readProcessSnapshot(args.serverPid);
    uint64_t loadPeakRss = loadBegin.rssKb;
    uint64_t serverPeakRss = serverBegin.rssKb;
    TimePoint nextProgress = measurementStart + std::chrono::seconds(std::max(args.progressSec, 1));

    while (Clock::now() < measurementEnd) {
        const TimePoint wake = args.progressSec > 0 ? std::min(nextProgress, measurementEnd)
                                                    : measurementEnd;
        std::this_thread::sleep_until(wake);
        const ProcessSnapshot loadNow = readProcessSnapshot(0);
        const ProcessSnapshot serverNow = readProcessSnapshot(args.serverPid);
        loadPeakRss = std::max(loadPeakRss, loadNow.rssKb);
        serverPeakRss = std::max(serverPeakRss, serverNow.rssKb);
        if (args.progressSec > 0 && Clock::now() >= nextProgress) {
            const double elapsed = std::chrono::duration<double>(Clock::now() - measurementStart).count();
            std::cerr << "progress=" << std::fixed << std::setprecision(1) << elapsed << "s"
                      << " sent=" << metrics.sent.load()
                      << " ok=" << metrics.responseOk.load()
                      << " timeout=" << metrics.timeout.load()
                      << " server_err=" << metrics.serverErrorResponses.load()
                      << " push=" << metrics.pushRecv.load()
                      << " active=" << metrics.activeConnections.load() << '\n';
            nextProgress += std::chrono::seconds(args.progressSec);
        }
    }

    const ProcessSnapshot loadEnd = readProcessSnapshot(0);
    const ProcessSnapshot serverEnd = readProcessSnapshot(args.serverPid);
    for (auto& thread : threads) {
        thread.join();
    }
    const double fullElapsed = std::chrono::duration<double>(Clock::now() - fullBegin).count();
    const LatencyStats latency = calculateLatencyStats(metrics);
    const auto [queueWait, persist] = calculatePersistenceStats(metrics);
    json result = buildResult(args, metrics, readyClients, fullElapsed, latency,
                              queueWait, persist,
                              loadBegin, loadEnd, loadPeakRss,
                              serverBegin, serverEnd, serverPeakRss, warnings);
    result["config"]["credential_count"] = credentials.size();
    result["config"]["unique_account_count"] = uniqueAccounts.size();

    std::cout << result.dump(2) << std::endl;
    try {
        if (!args.jsonOutput.empty()) {
            ensureParentDirectory(args.jsonOutput);
            std::ofstream output(args.jsonOutput);
            if (!output) {
                throw std::runtime_error("cannot open JSON output: " + args.jsonOutput);
            }
            output << result.dump(2) << '\n';
            std::cerr << "json_result=" << args.jsonOutput << '\n';
        }
        appendCsv(args.csvOutput, result);
        if (!args.csvOutput.empty()) {
            std::cerr << "csv_result=" << args.csvOutput << '\n';
        }
    } catch (const std::exception& exception) {
        std::cerr << "failed to persist result: " << exception.what() << '\n';
        return 5;
    }

    return 0;
}
