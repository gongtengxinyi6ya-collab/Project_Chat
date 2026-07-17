#include <arpa/inet.h>
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
#include <random>
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

using Clock=std::chrono::steady_clock;
using TimePoint=Clock::time_point;
using json=nlohmann::json;

constexpr uint32_t kMaxFrameLength=1024U*1024U;
constexpr int kMaxEpollEvents=2048;
volatile sig_atomic_t gStopRequested=0;

void requestStop(int){
    gStopRequested=1;
}

enum class ConnectionState {
    Connecting,
    LoginWriting,
    LoginWaiting,
    Ready
};

enum class RequestKind {
    Group,
    Direct
};

struct Args {
    std::string host{"127.0.0.1"};
    int port{8080};
    size_t clients{100};
    std::string manifestPath{"tools/mixed_load_manifest_1000.json"};
    double connectRate{100.0};
    double totalQps{100.0};
    double groupRatio{0.5};
    int warmupSec{10};
    int durationSec{60};
    int drainMs{15000};
    int connectTimeoutMs{3000};
    int loginTimeoutMs{5000};
    int requestTimeoutMs{10000};
    size_t maxInflightPerClient{64};
    size_t payloadBytes{64};
    int progressSec{5};
    int serverPid{-1};
    std::string jsonOutput{"load-results/mixed_load_result.json"};
    std::string csvOutput{"load-results/mixed_load_summary.csv"};
};

struct AccountPlan {
    std::string accountId;
    std::string password;
    std::vector<std::string> groupIds;
    std::vector<std::string> friendAccountIds;
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
    double p90Ms{0.0};
    double p95Ms{0.0};
    double p99Ms{0.0};
    double p999Ms{0.0};
    double maximumMs{0.0};
};

struct RequestMetrics {
    uint64_t attempted{0};
    uint64_t queued{0};
    uint64_t ok{0};
    uint64_t serverError{0};
    uint64_t timeout{0};
    uint64_t aborted{0};
    uint64_t lateResponse{0};
    uint64_t warmupQueued{0};
    uint64_t warmupOk{0};
    uint64_t warmupError{0};
    std::vector<double> latenciesMs;
};

struct Metrics {
    uint64_t connectionAttempted{0};
    uint64_t connectOk{0};
    uint64_t connectFail{0};
    uint64_t connectTimeout{0};
    uint64_t loginOk{0};
    uint64_t loginFail{0};
    uint64_t loginTimeout{0};
    uint64_t readyCurrent{0};
    uint64_t readyPeak{0};
    uint64_t readyAtLoadStart{0};
    uint64_t readyAtLoadEnd{0};
    uint64_t unexpectedClose{0};
    uint64_t socketErrors{0};
    uint64_t parseFail{0};
    uint64_t unmatchedResponse{0};
    uint64_t schedulerThrottled{0};
    uint64_t schedulerLagEvents{0};
    uint64_t peakInflight{0};
    uint64_t heartbeatPingRecv{0};
    uint64_t heartbeatPongQueued{0};
    uint64_t heartbeatPongSent{0};
    uint64_t heartbeatPongFail{0};
    uint64_t groupPushRecv{0};
    uint64_t directPushRecv{0};
    uint64_t otherPushRecv{0};
    uint64_t groupFanoutSent{0};
    uint64_t groupFanoutDropped{0};
    uint64_t groupFanoutClosed{0};
    uint64_t groupFanoutOverloaded{0};
    uint64_t directDelivered{0};
    uint64_t directQueuedOffline{0};
    uint64_t directPushSent{0};
    uint64_t directPushFailed{0};
    uint64_t bytesSent{0};
    uint64_t bytesRecv{0};
    RequestMetrics group;
    RequestMetrics direct;
    std::map<int,uint64_t> errorCodes;
    std::map<int,uint64_t> loginErrorCodes;
};

struct PendingRequest {
    RequestKind kind{RequestKind::Group};
    TimePoint queuedAt{};
    bool measured{false};
};

struct ConnectionContext {
    int fd{-1};
    size_t index{0};
    ConnectionState state{ConnectionState::Connecting};
    const AccountPlan* plan{nullptr};
    TimePoint connectBegin{};
    TimePoint loginBegin{};
    TimePoint deadline{};
    uint64_t loginRequestId{1};
    uint64_t nextRequestId{2};
    uint64_t nextSequence{2};
    size_t nextGroupIndex{0};
    size_t nextFriendIndex{0};
    std::string input;
    size_t inputOffset{0};
    std::string output;
    size_t outputOffset{0};
    uint64_t pendingPongs{0};
    std::unordered_map<uint64_t,PendingRequest> pending;
    std::unordered_map<uint64_t,RequestKind> expired;
};

void printUsage(const char* program){
    std::cout
        <<"Usage: "<<program<<" [options]\n\n"
        <<"  --host HOST                  Default 127.0.0.1\n"
        <<"  --port PORT                  Default 8080\n"
        <<"  --clients N                  Default 100\n"
        <<"  --manifest PATH              Mixed data manifest from client preparation\n"
        <<"  --connect-rate N             Connections opened per second, default 100\n"
        <<"  --total-qps N                Group plus DM request QPS, default 100\n"
        <<"  --group-ratio N              Group request fraction (0,1), default 0.5\n"
        <<"  --warmup SEC                 Default 10\n"
        <<"  --duration SEC               Measured duration, default 60\n"
        <<"  --drain-ms N                 Response/push drain window, default 15000\n"
        <<"  --connect-timeout-ms N       Default 3000\n"
        <<"  --login-timeout-ms N         Default 5000\n"
        <<"  --request-timeout-ms N       Default 10000\n"
        <<"  --max-inflight N             Per-client limit, default 64\n"
        <<"  --payload-bytes N            Message body target bytes, default 64\n"
        <<"  --progress-sec N             Default 5; 0 disables progress\n"
        <<"  --server-pid PID             Sample server CPU/RSS/fd/thread metrics\n"
        <<"  --json-out PATH              Default load-results/mixed_load_result.json\n"
        <<"  --csv-out PATH               Default load-results/mixed_load_summary.csv\n"
        <<"  --help\n";
}

bool parseInteger(const std::string& value,int& output){
    try{
        size_t consumed=0;
        const long parsed=std::stol(value,&consumed);
        if(consumed!=value.size()||parsed<std::numeric_limits<int>::min()||
           parsed>std::numeric_limits<int>::max()) return false;
        output=static_cast<int>(parsed);
        return true;
    }
    catch(...){
        return false;
    }
}

bool parseSize(const std::string& value,size_t& output){
    try{
        size_t consumed=0;
        const unsigned long long parsed=std::stoull(value,&consumed);
        if(consumed!=value.size()) return false;
        output=static_cast<size_t>(parsed);
        return true;
    }
    catch(...){
        return false;
    }
}

bool parseDouble(const std::string& value,double& output){
    try{
        size_t consumed=0;
        output=std::stod(value,&consumed);
        return consumed==value.size()&&std::isfinite(output);
    }
    catch(...){
        return false;
    }
}

bool parseArgs(int argc,char** argv,Args& args){
    for(int index=1;index<argc;++index){
        const std::string option=argv[index];
        if(option=="--help"){
            printUsage(argv[0]);
            return false;
        }
        if(index+1>=argc){
            std::cerr<<"missing value for "<<option<<'\n';
            return false;
        }
        const std::string value=argv[++index];
        if(option=="--host") args.host=value;
        else if(option=="--port"){ if(!parseInteger(value,args.port)) return false; }
        else if(option=="--clients"){ if(!parseSize(value,args.clients)) return false; }
        else if(option=="--manifest") args.manifestPath=value;
        else if(option=="--connect-rate"){ if(!parseDouble(value,args.connectRate)) return false; }
        else if(option=="--total-qps"){ if(!parseDouble(value,args.totalQps)) return false; }
        else if(option=="--group-ratio"){ if(!parseDouble(value,args.groupRatio)) return false; }
        else if(option=="--warmup"){ if(!parseInteger(value,args.warmupSec)) return false; }
        else if(option=="--duration"){ if(!parseInteger(value,args.durationSec)) return false; }
        else if(option=="--drain-ms"){ if(!parseInteger(value,args.drainMs)) return false; }
        else if(option=="--connect-timeout-ms"){ if(!parseInteger(value,args.connectTimeoutMs)) return false; }
        else if(option=="--login-timeout-ms"){ if(!parseInteger(value,args.loginTimeoutMs)) return false; }
        else if(option=="--request-timeout-ms"){ if(!parseInteger(value,args.requestTimeoutMs)) return false; }
        else if(option=="--max-inflight"){ if(!parseSize(value,args.maxInflightPerClient)) return false; }
        else if(option=="--payload-bytes"){ if(!parseSize(value,args.payloadBytes)) return false; }
        else if(option=="--progress-sec"){ if(!parseInteger(value,args.progressSec)) return false; }
        else if(option=="--server-pid"){ if(!parseInteger(value,args.serverPid)) return false; }
        else if(option=="--json-out") args.jsonOutput=value;
        else if(option=="--csv-out") args.csvOutput=value;
        else{
            std::cerr<<"unknown option: "<<option<<'\n';
            return false;
        }
    }
    if(args.port<=0||args.port>65535||args.clients==0||args.connectRate<=0.0||
       args.totalQps<=0.0||args.groupRatio<=0.0||args.groupRatio>=1.0||args.warmupSec<0||
       args.durationSec<=0||args.drainMs<0||args.connectTimeoutMs<=0||args.loginTimeoutMs<=0||
       args.requestTimeoutMs<=0||args.maxInflightPerClient==0||args.payloadBytes==0||
       args.progressSec<0||args.manifestPath.empty()||args.jsonOutput.empty()){
        std::cerr<<"invalid argument value\n";
        return false;
    }
    return true;
}

std::optional<std::vector<AccountPlan>> loadManifest(const Args& args){
    std::ifstream input(args.manifestPath);
    if(!input){
        std::cerr<<"cannot open mixed manifest: "<<args.manifestPath<<'\n';
        return std::nullopt;
    }
    json document;
    try{
        input>>document;
    }
    catch(const std::exception& exception){
        std::cerr<<"invalid mixed manifest: "<<exception.what()<<'\n';
        return std::nullopt;
    }
    if(!document.is_object()||!document.contains("accounts")||
       !document["accounts"].is_array()){
        std::cerr<<"mixed manifest requires an accounts array\n";
        return std::nullopt;
    }
    std::vector<AccountPlan> plans;
    std::unordered_set<std::string> accountIds;
    for(const auto& entry:document["accounts"]){
        if(!entry.is_object()||!entry.contains("accountId")||!entry["accountId"].is_string()||
           !entry.contains("password")||!entry["password"].is_string()){
            std::cerr<<"manifest account requires string accountId and password\n";
            return std::nullopt;
        }
        AccountPlan plan;
        plan.accountId=entry["accountId"].get<std::string>();
        plan.password=entry["password"].get<std::string>();
        if(entry.contains("groupIds")&&entry["groupIds"].is_array()){
            plan.groupIds=entry["groupIds"].get<std::vector<std::string>>();
        }
        if(entry.contains("friendAccountIds")&&entry["friendAccountIds"].is_array()){
            plan.friendAccountIds=entry["friendAccountIds"].get<std::vector<std::string>>();
        }
        if(plan.accountId.empty()||plan.password.empty()||
           !accountIds.insert(plan.accountId).second){
            std::cerr<<"manifest accountId/password must be non-empty and unique\n";
            return std::nullopt;
        }
        plans.push_back(std::move(plan));
    }
    if(plans.size()<args.clients){
        std::cerr<<"manifest has "<<plans.size()<<" accounts, need "<<args.clients<<'\n';
        return std::nullopt;
    }
    std::unordered_set<std::string> activeAccountIds;
    for(size_t index=0;index<args.clients;++index){
        activeAccountIds.insert(plans[index].accountId);
    }
    for(size_t index=0;index<args.clients;++index){
        auto& friends=plans[index].friendAccountIds;
        friends.erase(std::remove_if(friends.begin(),friends.end(),
            [&activeAccountIds](const std::string& accountId){
                return activeAccountIds.count(accountId)==0;
            }),friends.end());
        if(plans[index].groupIds.empty()||plans[index].friendAccountIds.empty()){
            std::cerr<<"account "<<plans[index].accountId
                     <<" lacks a group or an online friend within the selected client subset\n";
            return std::nullopt;
        }
    }
    return plans;
}

std::string encodeFrame(std::string_view payload){
    const uint32_t networkLength=htonl(static_cast<uint32_t>(payload.size()));
    std::string frame(sizeof(networkLength),'\0');
    std::memcpy(frame.data(),&networkLength,sizeof(networkLength));
    frame.append(payload.data(),payload.size());
    return frame;
}

std::string makeLoginRequest(const AccountPlan& plan,uint64_t requestId){
    return json{{"ver",1},{"type",im::msgTypeToInt(im::MsgType::LOGIN_REQ)},
                {"req_id",requestId},{"from",plan.accountId},{"to",""},
                {"seq",requestId},{"accountId",plan.accountId},{"password",plan.password}}.dump();
}

std::string makePayload(size_t targetBytes,const std::string& prefix){
    std::string payload=prefix;
    if(payload.size()<targetBytes){
        payload.append(targetBytes-payload.size(),'x');
    }
    return payload;
}

std::string makeGroupRequest(ConnectionContext& context,const std::string& runId,
                             uint64_t requestId,size_t payloadBytes){
    const std::string& groupId=context.plan->groupIds[
        context.nextGroupIndex++%context.plan->groupIds.size()];
    const std::string content=makePayload(
        payloadBytes,runId+":group:"+std::to_string(requestId));
    return json{{"ver",1},{"type",im::msgTypeToInt(im::MsgType::GROUP_MSG_REQ)},
                {"req_id",requestId},{"from",context.plan->accountId},{"to",""},
                {"seq",context.nextSequence++},{"groupId",groupId},
                {"content",content}}.dump();
}

std::string makeDirectRequest(ConnectionContext& context,const std::string& runId,
                              uint64_t requestId,size_t payloadBytes){
    const std::string& target=context.plan->friendAccountIds[
        context.nextFriendIndex++%context.plan->friendAccountIds.size()];
    const std::string content=makePayload(
        payloadBytes,runId+":dm:"+std::to_string(requestId));
    return json{{"ver",1},{"type",im::msgTypeToInt(im::MsgType::DM_REQ)},
                {"req_id",requestId},{"from",context.plan->accountId},{"to",target},
                {"seq",context.nextSequence++},{"content",content}}.dump();
}

double elapsedMs(TimePoint begin,TimePoint end){
    return std::chrono::duration<double,std::milli>(end-begin).count();
}

double percentile(const std::vector<double>& sorted,double fraction){
    if(sorted.empty()) return 0.0;
    const double position=fraction*static_cast<double>(sorted.size()-1);
    const size_t lower=static_cast<size_t>(position);
    const size_t upper=std::min(lower+1,sorted.size()-1);
    const double weight=position-static_cast<double>(lower);
    return sorted[lower]*(1.0-weight)+sorted[upper]*weight;
}

LatencyStats calculateLatencyStats(std::vector<double> values){
    LatencyStats stats;
    if(values.empty()) return stats;
    std::sort(values.begin(),values.end());
    stats.samples=values.size();
    stats.minimumMs=values.front();
    stats.maximumMs=values.back();
    stats.averageMs=std::accumulate(values.begin(),values.end(),0.0)/
                    static_cast<double>(values.size());
    stats.p50Ms=percentile(values,0.50);
    stats.p90Ms=percentile(values,0.90);
    stats.p95Ms=percentile(values,0.95);
    stats.p99Ms=percentile(values,0.99);
    stats.p999Ms=percentile(values,0.999);
    return stats;
}

json latencyToJson(const LatencyStats& stats){
    return json{{"samples",stats.samples},{"min",stats.minimumMs},{"avg",stats.averageMs},
                {"p50",stats.p50Ms},{"p90",stats.p90Ms},{"p95",stats.p95Ms},
                {"p99",stats.p99Ms},{"p999",stats.p999Ms},{"max",stats.maximumMs}};
}

ProcessSnapshot readProcessSnapshot(int pid){
    ProcessSnapshot snapshot;
    if(pid<0) return snapshot;
    const std::string base=pid>0?"/proc/"+std::to_string(pid):"/proc/self";
    std::ifstream statFile(base+"/stat");
    std::string statLine;
    if(!statFile||!std::getline(statFile,statLine)) return snapshot;
    const size_t closingParenthesis=statLine.rfind(')');
    if(closingParenthesis==std::string::npos||closingParenthesis+2>=statLine.size()) return snapshot;
    std::istringstream fields(statLine.substr(closingParenthesis+2));
    std::vector<std::string> tokens;
    std::string token;
    while(fields>>token) tokens.push_back(token);
    if(tokens.size()<=21) return snapshot;
    try{
        snapshot.cpuTicks=std::stoull(tokens[11])+std::stoull(tokens[12]);
        snapshot.rssKb=std::stoull(tokens[21])*static_cast<uint64_t>(::sysconf(_SC_PAGESIZE))/1024ULL;
    }
    catch(...){
        return {};
    }
    std::ifstream statusFile(base+"/status");
    std::string line;
    while(std::getline(statusFile,line)){
        if(line.starts_with("VmHWM:")){
            std::istringstream value(line.substr(6));
            value>>snapshot.highWaterKb;
        }
        else if(line.starts_with("Threads:")){
            std::istringstream value(line.substr(8));
            value>>snapshot.threadCount;
        }
    }
    std::error_code error;
    for(const auto& entry:std::filesystem::directory_iterator(base+"/fd",error)){
        static_cast<void>(entry);
        ++snapshot.fdCount;
    }
    snapshot.valid=true;
    return snapshot;
}

double cpuPercent(const ProcessSnapshot& begin,const ProcessSnapshot& end,double seconds){
    if(!begin.valid||!end.valid||seconds<=0.0||end.cpuTicks<begin.cpuTicks) return 0.0;
    const double cpuSeconds=static_cast<double>(end.cpuTicks-begin.cpuTicks)/
                            static_cast<double>(::sysconf(_SC_CLK_TCK));
    return cpuSeconds/seconds*100.0;
}

std::string currentTimestamp(){
    const auto now=std::chrono::system_clock::now();
    const std::time_t value=std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&value,&local);
    std::ostringstream output;
    output<<std::put_time(&local,"%Y-%m-%d %H:%M:%S");
    return output.str();
}

void ensureParentDirectory(const std::string& path){
    const std::filesystem::path output(path);
    if(!output.has_parent_path()) return;
    std::error_code error;
    std::filesystem::create_directories(output.parent_path(),error);
    if(error) throw std::runtime_error("failed to create output directory: "+error.message());
}

class MixedLoadRunner {
public:
    MixedLoadRunner(Args args,std::vector<AccountPlan> plans)
        :args_(std::move(args)),plans_(std::move(plans)),random_(std::random_device{}()){
        epollFd_=::epoll_create1(EPOLL_CLOEXEC);
        if(epollFd_<0) throw std::runtime_error("epoll_create1 failed: "+std::string(std::strerror(errno)));
        address_.sin_family=AF_INET;
        address_.sin_port=htons(static_cast<uint16_t>(args_.port));
        if(::inet_pton(AF_INET,args_.host.c_str(),&address_.sin_addr)!=1){
            throw std::runtime_error("invalid IPv4 address: "+args_.host);
        }
        const auto now=std::chrono::system_clock::now().time_since_epoch();
        runId_=std::to_string(::getpid())+"-"+
               std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    }

    ~MixedLoadRunner(){
        closeAll(false);
        if(epollFd_>=0) ::close(epollFd_);
    }

    int run(){
        warnLimits();
        begin_=Clock::now();
        nextConnectAt_=begin_;
        nextProgressAt_=begin_+std::chrono::seconds(std::max(1,args_.progressSec));
        nextSampleAt_=begin_;
        connectInterval_=std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(1.0/args_.connectRate));
        requestInterval_=std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(1.0/args_.totalQps));
        loadProcessBegin_=readProcessSnapshot(0);
        serverProcessBegin_=readProcessSnapshot(args_.serverPid);

        std::vector<epoll_event> events(kMaxEpollEvents);
        while(!finished_){
            const TimePoint now=Clock::now();
            if(gStopRequested!=0){
                interrupted_=true;
                break;
            }
            launchConnections(now);
            expireSetup(now);
            beginLoadIfReady(now);
            scheduleRequests(now);
            expireRequests(now);
            sampleProcesses(now);
            printProgress(now);
            if(drainDeadline_&&now>=*drainDeadline_){
                metrics_.readyAtLoadEnd=metrics_.readyCurrent;
                finished_=true;
                break;
            }
            if(metrics_.connectionAttempted==args_.clients&&contexts_.empty()&&!loadBegin_){
                finished_=true;
                break;
            }

            int eventCount;
            do{
                eventCount=::epoll_wait(epollFd_,events.data(),static_cast<int>(events.size()),
                                        epollTimeoutMs(now));
            }while(eventCount<0&&errno==EINTR);
            if(eventCount<0){
                throw std::runtime_error("epoll_wait failed: "+std::string(std::strerror(errno)));
            }
            for(int index=0;index<eventCount;++index){
                handleEvent(events[static_cast<size_t>(index)]);
            }
        }

        const TimePoint end=Clock::now();
        if(metrics_.readyAtLoadEnd==0&&loadBegin_) metrics_.readyAtLoadEnd=metrics_.readyCurrent;
        closeAll(false);
        loadProcessEnd_=readProcessSnapshot(0);
        serverProcessEnd_=readProcessSnapshot(args_.serverPid);
        const double elapsedSeconds=std::chrono::duration<double>(end-begin_).count();
        const json result=buildResult(elapsedSeconds);
        writeResult(result);
        std::cout<<result.dump(2)<<'\n';
        return result.at("passed").get<bool>()?0:4;
    }

private:
    void warnLimits()const{
        rlimit limit{};
        if(::getrlimit(RLIMIT_NOFILE,&limit)==0&&
           limit.rlim_cur<static_cast<rlim_t>(args_.clients+128)){
            std::cerr<<"warning: RLIMIT_NOFILE="<<limit.rlim_cur
                     <<"; recommended at least "<<(args_.clients+128)<<'\n';
        }
        const double perAccountQps=args_.totalQps/static_cast<double>(args_.clients);
        if(perAccountQps>20.0){
            std::cerr<<"warning: per-account QPS="<<perAccountQps
                     <<" exceeds the current 20 msg/s account limiter\n";
        }
    }

    void launchConnections(TimePoint now){
        while(metrics_.connectionAttempted<args_.clients&&now>=nextConnectAt_){
            openConnection(static_cast<size_t>(metrics_.connectionAttempted),now);
            ++metrics_.connectionAttempted;
            nextConnectAt_+=connectInterval_;
            if(connectInterval_<=Clock::duration::zero()) nextConnectAt_=now;
        }
    }

    void openConnection(size_t index,TimePoint now){
        const int fd=::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
        if(fd<0){
            ++metrics_.connectFail;
            ++metrics_.socketErrors;
            return;
        }
        int enabled=1;
        ::setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&enabled,sizeof(enabled));
        ::setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&enabled,sizeof(enabled));
        auto context=std::make_unique<ConnectionContext>();
        context->fd=fd;
        context->index=index;
        context->plan=&plans_[index];
        context->connectBegin=now;
        context->deadline=now+std::chrono::milliseconds(args_.connectTimeoutMs);
        const int result=::connect(fd,reinterpret_cast<sockaddr*>(&address_),sizeof(address_));
        if(result<0&&errno!=EINPROGRESS){
            ++metrics_.connectFail;
            ++metrics_.socketErrors;
            ::close(fd);
            return;
        }
        epoll_event event{};
        event.events=EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLERR|EPOLLHUP;
        event.data.fd=fd;
        if(::epoll_ctl(epollFd_,EPOLL_CTL_ADD,fd,&event)<0){
            ++metrics_.connectFail;
            ++metrics_.socketErrors;
            ::close(fd);
            return;
        }
        contexts_.emplace(fd,std::move(context));
        ++connecting_;
        if(result==0) completeConnect(fd,now);
    }

    void completeConnect(int fd,TimePoint now){
        auto* context=find(fd);
        if(!context||context->state!=ConnectionState::Connecting) return;
        int socketError=0;
        socklen_t errorLength=sizeof(socketError);
        if(::getsockopt(fd,SOL_SOCKET,SO_ERROR,&socketError,&errorLength)<0||socketError!=0){
            ++metrics_.connectFail;
            ++metrics_.socketErrors;
            closeConnection(fd,false);
            return;
        }
        if(connecting_>0) --connecting_;
        ++metrics_.connectOk;
        context->state=ConnectionState::LoginWriting;
        context->loginBegin=now;
        context->deadline=now+std::chrono::milliseconds(args_.loginTimeoutMs);
        context->output=encodeFrame(makeLoginRequest(*context->plan,context->loginRequestId));
        ++loggingIn_;
        updateInterest(*context);
    }

    void handleEvent(const epoll_event& event){
        const int fd=event.data.fd;
        auto* context=find(fd);
        if(!context) return;
        if(context->state==ConnectionState::Connecting&&
           (event.events&(EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLHUP|EPOLLRDHUP))!=0){
            completeConnect(fd,Clock::now());
            context=find(fd);
            if(!context) return;
        }
        if((event.events&EPOLLIN)!=0){
            handleRead(fd);
            context=find(fd);
            if(!context) return;
        }
        if((event.events&EPOLLOUT)!=0){
            handleWrite(fd);
            context=find(fd);
            if(!context) return;
        }
        if((event.events&(EPOLLERR|EPOLLHUP|EPOLLRDHUP))!=0) handlePeerFailure(fd);
    }

    void handleRead(int fd){
        auto* context=find(fd);
        if(!context) return;
        char buffer[32768];
        while(true){
            const ssize_t received=::recv(fd,buffer,sizeof(buffer),0);
            if(received>0){
                metrics_.bytesRecv+=static_cast<uint64_t>(received);
                context->input.append(buffer,static_cast<size_t>(received));
                if(!processFrames(fd)) return;
                context=find(fd);
                if(!context) return;
                continue;
            }
            if(received==0){
                handlePeerFailure(fd);
                return;
            }
            if(errno==EINTR) continue;
            if(errno==EAGAIN||errno==EWOULDBLOCK) return;
            ++metrics_.socketErrors;
            handlePeerFailure(fd);
            return;
        }
    }

    bool processFrames(int fd){
        while(true){
            auto* context=find(fd);
            if(!context) return false;
            const size_t readable=context->input.size()-context->inputOffset;
            if(readable<sizeof(uint32_t)){
                compactInput(*context);
                return true;
            }
            uint32_t networkLength=0;
            std::memcpy(&networkLength,context->input.data()+context->inputOffset,sizeof(networkLength));
            const uint32_t length=ntohl(networkLength);
            if(length==0||length>kMaxFrameLength){
                ++metrics_.parseFail;
                failSetup(*context);
                closeConnection(fd,context->state==ConnectionState::Ready);
                return false;
            }
            if(readable<sizeof(uint32_t)+length){
                compactInput(*context);
                return true;
            }
            context->inputOffset+=sizeof(uint32_t);
            std::string payload=context->input.substr(context->inputOffset,length);
            context->inputOffset+=length;
            if(!handleFrame(fd,payload)) return false;
        }
    }

    bool handleFrame(int fd,const std::string& payload){
        auto* context=find(fd);
        if(!context) return false;
        if(payload=="PING"){
            ++metrics_.heartbeatPingRecv;
            context->output.append(encodeFrame("PONG"));
            ++context->pendingPongs;
            ++metrics_.heartbeatPongQueued;
            updateInterest(*context);
            return true;
        }
        json response;
        try{
            response=json::parse(payload);
        }
        catch(...){
            ++metrics_.parseFail;
            return true;
        }
        if(context->state==ConnectionState::LoginWriting||
           context->state==ConnectionState::LoginWaiting){
            if(response.value("req_id",uint64_t{0})!=context->loginRequestId) return true;
            if(!response.value("ok",false)){
                ++metrics_.loginFail;
                ++metrics_.loginErrorCodes[response.value("code",-1)];
                closeConnection(fd,false);
                return false;
            }
            if(loggingIn_>0) --loggingIn_;
            ++metrics_.loginOk;
            context->state=ConnectionState::Ready;
            ++metrics_.readyCurrent;
            metrics_.readyPeak=std::max(metrics_.readyPeak,metrics_.readyCurrent);
            readyFds_.push_back(fd);
            updateInterest(*context);
            return true;
        }

        const int type=response.value("type",-1);
        const TimePoint now=Clock::now();
        const bool countPush=warmupEnd_&&now>=*warmupEnd_&&(!drainDeadline_||now<*drainDeadline_);
        if(response.value("req_id",uint64_t{0})==0){
            if(countPush&&type==static_cast<int>(im::MsgType::GROUP_MSG_PUSH)) ++metrics_.groupPushRecv;
            else if(countPush&&type==static_cast<int>(im::MsgType::DM_PUSH)) ++metrics_.directPushRecv;
            else if(countPush) ++metrics_.otherPushRecv;
            return true;
        }
        const uint64_t requestId=response.value("req_id",uint64_t{0});
        const auto pending=context->pending.find(requestId);
        if(pending==context->pending.end()){
            const auto expired=context->expired.find(requestId);
            if(expired!=context->expired.end()){
                ++metricsFor(expired->second).lateResponse;
                context->expired.erase(expired);
            }
            else{
                ++metrics_.unmatchedResponse;
            }
            return true;
        }
        const PendingRequest request=pending->second;
        context->pending.erase(pending);
        if(currentInflight_>0) --currentInflight_;
        RequestMetrics& requestMetrics=metricsFor(request.kind);
        if(!request.measured){
            if(response.value("ok",false)) ++requestMetrics.warmupOk;
            else ++requestMetrics.warmupError;
            return true;
        }
        if(response.value("ok",false)){
            ++requestMetrics.ok;
            requestMetrics.latenciesMs.push_back(elapsedMs(request.queuedAt,now));
            collectBusinessResult(request.kind,response);
        }
        else{
            ++requestMetrics.serverError;
            ++metrics_.errorCodes[response.value("code",-1)];
        }
        return true;
    }

    void collectBusinessResult(RequestKind kind,const json& response){
        if(!response.contains("data")||!response["data"].is_object()) return;
        const auto& data=response["data"];
        if(kind==RequestKind::Group){
            metrics_.groupFanoutSent+=data.value("fanoutSent",uint64_t{0});
            metrics_.groupFanoutDropped+=data.value("fanoutDropped",uint64_t{0});
            metrics_.groupFanoutClosed+=data.value("fanoutClosed",uint64_t{0});
            metrics_.groupFanoutOverloaded+=data.value("fanoutOverloaded",uint64_t{0});
        }
        else{
            metrics_.directDelivered+=data.value("delivered",false)?1ULL:0ULL;
            metrics_.directQueuedOffline+=data.value("queuedOffline",false)?1ULL:0ULL;
            metrics_.directPushSent+=data.value("sent",uint64_t{0});
            metrics_.directPushFailed+=data.value("failed",uint64_t{0});
        }
    }

    void handleWrite(int fd){
        auto* context=find(fd);
        if(!context) return;
        while(context->outputOffset<context->output.size()){
            const ssize_t written=::send(fd,context->output.data()+context->outputOffset,
                                         context->output.size()-context->outputOffset,MSG_NOSIGNAL);
            if(written>0){
                context->outputOffset+=static_cast<size_t>(written);
                metrics_.bytesSent+=static_cast<uint64_t>(written);
                continue;
            }
            if(written<0&&errno==EINTR) continue;
            if(written<0&&(errno==EAGAIN||errno==EWOULDBLOCK)){
                updateInterest(*context);
                return;
            }
            ++metrics_.socketErrors;
            metrics_.heartbeatPongFail+=context->pendingPongs;
            failSetup(*context);
            closeConnection(fd,context->state==ConnectionState::Ready);
            return;
        }
        context->output.clear();
        context->outputOffset=0;
        metrics_.heartbeatPongSent+=context->pendingPongs;
        context->pendingPongs=0;
        if(context->state==ConnectionState::LoginWriting) context->state=ConnectionState::LoginWaiting;
        updateInterest(*context);
    }

    void handlePeerFailure(int fd){
        auto* context=find(fd);
        if(!context) return;
        const bool unexpected=context->state==ConnectionState::Ready;
        failSetup(*context);
        metrics_.heartbeatPongFail+=context->pendingPongs;
        closeConnection(fd,unexpected);
    }

    void failSetup(const ConnectionContext& context){
        if(context.state==ConnectionState::Connecting) ++metrics_.connectFail;
        else if(context.state==ConnectionState::LoginWriting||
                context.state==ConnectionState::LoginWaiting) ++metrics_.loginFail;
    }

    void expireSetup(TimePoint now){
        std::vector<int> expired;
        for(const auto& [fd,context]:contexts_){
            if(context->state!=ConnectionState::Ready&&now>=context->deadline) expired.push_back(fd);
        }
        for(int fd:expired){
            auto* context=find(fd);
            if(!context) continue;
            if(context->state==ConnectionState::Connecting){
                ++metrics_.connectTimeout;
                ++metrics_.connectFail;
            }
            else{
                ++metrics_.loginTimeout;
                ++metrics_.loginFail;
            }
            closeConnection(fd,false);
        }
    }

    void beginLoadIfReady(TimePoint now){
        if(loadBegin_||metrics_.connectionAttempted!=args_.clients||connecting_!=0||loggingIn_!=0) return;
        loadBegin_=now;
        metrics_.readyAtLoadStart=metrics_.readyCurrent;
        warmupEnd_=now+std::chrono::seconds(args_.warmupSec);
        measureEnd_=*warmupEnd_+std::chrono::seconds(args_.durationSec);
        drainDeadline_=*measureEnd_+std::chrono::milliseconds(args_.drainMs);
        nextRequestAt_=now;
        std::cerr<<"setup settled: ready="<<metrics_.readyAtLoadStart<<'/'<<args_.clients
                 <<", warmup="<<args_.warmupSec<<"s measure="<<args_.durationSec
                 <<"s drain="<<args_.drainMs<<"ms\n";
    }

    void scheduleRequests(TimePoint now){
        if(!loadBegin_||!measureEnd_||now>=*measureEnd_) return;
        size_t scheduledThisPass=0;
        while(now>=nextRequestAt_&&nextRequestAt_<*measureEnd_){
            const bool measured=warmupEnd_&&nextRequestAt_>=*warmupEnd_;
            if(!queueOneRequest(nextRequestAt_,measured)) ++metrics_.schedulerThrottled;
            nextRequestAt_+=requestInterval_;
            ++scheduledThisPass;
            if(scheduledThisPass>=10000){
                ++metrics_.schedulerLagEvents;
                nextRequestAt_=now;
                break;
            }
        }
        if(scheduledThisPass>1) ++metrics_.schedulerLagEvents;
    }

    bool queueOneRequest(TimePoint scheduledAt,bool measured){
        if(readyFds_.empty()) return false;
        const bool preferGroup=std::uniform_real_distribution<double>(0.0,1.0)(random_)<args_.groupRatio;
        for(size_t attempt=0;attempt<readyFds_.size();++attempt){
            const int fd=readyFds_[readyCursor_++%readyFds_.size()];
            auto* context=find(fd);
            if(!context||context->state!=ConnectionState::Ready||
               context->pending.size()>=args_.maxInflightPerClient) continue;
            RequestKind kind=preferGroup?RequestKind::Group:RequestKind::Direct;
            if(kind==RequestKind::Group&&context->plan->groupIds.empty()) kind=RequestKind::Direct;
            if(kind==RequestKind::Direct&&context->plan->friendAccountIds.empty()) kind=RequestKind::Group;
            if((kind==RequestKind::Group&&context->plan->groupIds.empty())||
               (kind==RequestKind::Direct&&context->plan->friendAccountIds.empty())) continue;
            const uint64_t requestId=context->nextRequestId++;
            std::string request=kind==RequestKind::Group
                ?makeGroupRequest(*context,runId_,requestId,args_.payloadBytes)
                :makeDirectRequest(*context,runId_,requestId,args_.payloadBytes);
            context->output.append(encodeFrame(request));
            context->pending.emplace(requestId,PendingRequest{kind,scheduledAt,measured});
            ++currentInflight_;
            metrics_.peakInflight=std::max(metrics_.peakInflight,currentInflight_);
            RequestMetrics& requestMetrics=metricsFor(kind);
            if(measured){
                ++requestMetrics.attempted;
                ++requestMetrics.queued;
            }
            else ++requestMetrics.warmupQueued;
            updateInterest(*context);
            return true;
        }
        return false;
    }

    void expireRequests(TimePoint now){
        if(now<nextTimeoutScan_) return;
        nextTimeoutScan_=now+std::chrono::milliseconds(100);
        const auto timeout=std::chrono::milliseconds(args_.requestTimeoutMs);
        for(auto& [fd,context]:contexts_){
            static_cast<void>(fd);
            for(auto iterator=context->pending.begin();iterator!=context->pending.end();){
                if(now-iterator->second.queuedAt<timeout){
                    ++iterator;
                    continue;
                }
                if(iterator->second.measured) ++metricsFor(iterator->second.kind).timeout;
                else ++metricsFor(iterator->second.kind).warmupError;
                context->expired.emplace(iterator->first,iterator->second.kind);
                iterator=context->pending.erase(iterator);
                if(currentInflight_>0) --currentInflight_;
            }
        }
    }

    RequestMetrics& metricsFor(RequestKind kind){
        return kind==RequestKind::Group?metrics_.group:metrics_.direct;
    }

    void compactInput(ConnectionContext& context){
        if(context.inputOffset==0) return;
        if(context.inputOffset==context.input.size()){
            context.input.clear();
            context.inputOffset=0;
        }
        else if(context.inputOffset>=4096){
            context.input.erase(0,context.inputOffset);
            context.inputOffset=0;
        }
    }

    void updateInterest(const ConnectionContext& context){
        epoll_event event{};
        event.events=EPOLLIN|EPOLLRDHUP|EPOLLERR|EPOLLHUP;
        if(context.state==ConnectionState::Connecting||context.outputOffset<context.output.size()){
            event.events|=EPOLLOUT;
        }
        event.data.fd=context.fd;
        if(::epoll_ctl(epollFd_,EPOLL_CTL_MOD,context.fd,&event)<0&&errno!=ENOENT){
            throw std::runtime_error("epoll_ctl mod failed: "+std::string(std::strerror(errno)));
        }
    }

    ConnectionContext* find(int fd){
        const auto iterator=contexts_.find(fd);
        return iterator==contexts_.end()?nullptr:iterator->second.get();
    }

    void closeConnection(int fd,bool unexpected){
        const auto iterator=contexts_.find(fd);
        if(iterator==contexts_.end()) return;
        auto& context=*iterator->second;
        if(context.state==ConnectionState::Connecting&&connecting_>0) --connecting_;
        else if((context.state==ConnectionState::LoginWriting||
                 context.state==ConnectionState::LoginWaiting)&&loggingIn_>0) --loggingIn_;
        else if(context.state==ConnectionState::Ready&&metrics_.readyCurrent>0){
            --metrics_.readyCurrent;
            if(unexpected) ++metrics_.unexpectedClose;
        }
        for(const auto& [requestId,pending]:context.pending){
            static_cast<void>(requestId);
            if(pending.measured) ++metricsFor(pending.kind).aborted;
        }
        currentInflight_-=std::min<uint64_t>(currentInflight_,context.pending.size());
        ::epoll_ctl(epollFd_,EPOLL_CTL_DEL,fd,nullptr);
        ::close(fd);
        contexts_.erase(iterator);
    }

    void closeAll(bool unexpected){
        std::vector<int> fds;
        fds.reserve(contexts_.size());
        for(const auto& [fd,context]:contexts_){
            static_cast<void>(context);
            fds.push_back(fd);
        }
        for(int fd:fds) closeConnection(fd,unexpected);
    }

    int epollTimeoutMs(TimePoint now)const{
        auto deadline=now+std::chrono::milliseconds(50);
        if(metrics_.connectionAttempted<args_.clients) deadline=std::min(deadline,nextConnectAt_);
        if(loadBegin_&&measureEnd_&&now<*measureEnd_) deadline=std::min(deadline,nextRequestAt_);
        if(drainDeadline_) deadline=std::min(deadline,*drainDeadline_);
        const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-now);
        return static_cast<int>(std::clamp<int64_t>(remaining.count(),0,50));
    }

    void sampleProcesses(TimePoint now){
        if(now<nextSampleAt_) return;
        const auto load=readProcessSnapshot(0);
        const auto server=readProcessSnapshot(args_.serverPid);
        loadPeakRss_=std::max({loadPeakRss_,load.rssKb,load.highWaterKb});
        loadPeakFd_=std::max(loadPeakFd_,load.fdCount);
        loadPeakThreads_=std::max(loadPeakThreads_,load.threadCount);
        serverPeakRss_=std::max({serverPeakRss_,server.rssKb,server.highWaterKb});
        serverPeakFd_=std::max(serverPeakFd_,server.fdCount);
        serverPeakThreads_=std::max(serverPeakThreads_,server.threadCount);
        nextSampleAt_=now+std::chrono::seconds(1);
    }

    uint64_t inflightCount()const{
        return currentInflight_;
    }

    void printProgress(TimePoint now){
        if(args_.progressSec==0||now<nextProgressAt_) return;
        std::cerr<<"elapsed="<<std::fixed<<std::setprecision(1)
                 <<std::chrono::duration<double>(now-begin_).count()
                 <<"s ready="<<metrics_.readyCurrent<<'/'<<args_.clients
                 <<" group_ok="<<metrics_.group.ok<<'/'<<metrics_.group.queued
                 <<" dm_ok="<<metrics_.direct.ok<<'/'<<metrics_.direct.queued
                 <<" inflight="<<inflightCount()
                 <<" group_push="<<metrics_.groupPushRecv
                 <<" dm_push="<<metrics_.directPushRecv<<'\n';
        nextProgressAt_=now+std::chrono::seconds(args_.progressSec);
    }

    json requestMetricsJson(const RequestMetrics& values,double durationSeconds)const{
        const uint64_t errors=values.serverError+values.timeout+values.aborted;
        const double successRate=values.queued==0?0.0:
            static_cast<double>(values.ok)/static_cast<double>(values.queued);
        return json{{"attempted",values.attempted},{"queued",values.queued},{"ok",values.ok},
                    {"server_error",values.serverError},{"timeout",values.timeout},
                    {"aborted",values.aborted},{"late_response",values.lateResponse},
                    {"error_total",errors},{"success_rate",successRate},
                    {"queued_qps",durationSeconds>0.0?values.queued/durationSeconds:0.0},
                    {"ok_qps",durationSeconds>0.0?values.ok/durationSeconds:0.0},
                    {"latency_ms",latencyToJson(calculateLatencyStats(values.latenciesMs))},
                    {"warmup",{{"queued",values.warmupQueued},{"ok",values.warmupOk},
                               {"error",values.warmupError}}}};
    }

    json processJson(const ProcessSnapshot& begin,const ProcessSnapshot& end,uint64_t peakRss,
                     uint64_t peakFd,uint64_t peakThreads,double seconds)const{
        return json{{"sampled",begin.valid&&end.valid},{"cpu_percent",cpuPercent(begin,end,seconds)},
                    {"rss_start_kb",begin.rssKb},{"rss_end_kb",end.rssKb},{"rss_peak_kb",peakRss},
                    {"fd_start",begin.fdCount},{"fd_end",end.fdCount},{"fd_peak",peakFd},
                    {"threads_start",begin.threadCount},{"threads_end",end.threadCount},
                    {"threads_peak",peakThreads}};
    }

    json buildResult(double elapsedSeconds)const{
        json errors=json::object();
        for(const auto& [code,count]:metrics_.errorCodes) errors[std::to_string(code)]=count;
        json loginErrors=json::object();
        for(const auto& [code,count]:metrics_.loginErrorCodes) loginErrors[std::to_string(code)]=count;
        const double setupRate=args_.clients==0?0.0:
            static_cast<double>(metrics_.readyAtLoadStart)/static_cast<double>(args_.clients);
        const double survivalRate=metrics_.readyAtLoadStart==0?0.0:
            static_cast<double>(metrics_.readyAtLoadEnd)/static_cast<double>(metrics_.readyAtLoadStart);
        const double groupDelivery=metrics_.groupFanoutSent==0?0.0:
            static_cast<double>(metrics_.groupPushRecv)/static_cast<double>(metrics_.groupFanoutSent);
        const double directDelivery=metrics_.directPushSent==0?0.0:
            static_cast<double>(metrics_.directPushRecv)/static_cast<double>(metrics_.directPushSent);
        const double groupSuccess=metrics_.group.queued==0?0.0:
            static_cast<double>(metrics_.group.ok)/static_cast<double>(metrics_.group.queued);
        const double directSuccess=metrics_.direct.queued==0?0.0:
            static_cast<double>(metrics_.direct.ok)/static_cast<double>(metrics_.direct.queued);
        const bool passed=metrics_.readyAtLoadStart==args_.clients&&
                          metrics_.readyAtLoadEnd==metrics_.readyAtLoadStart&&
                          metrics_.unexpectedClose==0&&metrics_.parseFail==0&&
                          metrics_.heartbeatPongFail==0&&groupSuccess>=0.99&&directSuccess>=0.99&&
                          groupDelivery>=0.99&&directDelivery>=0.99&&!interrupted_;
        const double measuredSeconds=static_cast<double>(args_.durationSec);
        return json{
            {"timestamp",currentTimestamp()},{"run_id",runId_},
            {"config",{{"host",args_.host},{"port",args_.port},{"clients",args_.clients},
                       {"manifest",args_.manifestPath},{"connect_rate",args_.connectRate},
                       {"total_qps",args_.totalQps},{"group_ratio",args_.groupRatio},
                       {"direct_ratio",1.0-args_.groupRatio},{"warmup_s",args_.warmupSec},
                       {"duration_s",args_.durationSec},{"drain_ms",args_.drainMs},
                       {"request_timeout_ms",args_.requestTimeoutMs},
                       {"max_inflight_per_client",args_.maxInflightPerClient},
                       {"payload_bytes",args_.payloadBytes},{"elapsed_s",elapsedSeconds}}},
            {"connections",{{"attempted",metrics_.connectionAttempted},{"connect_ok",metrics_.connectOk},
                            {"connect_fail",metrics_.connectFail},{"connect_timeout",metrics_.connectTimeout},
                            {"login_ok",metrics_.loginOk},{"login_fail",metrics_.loginFail},
                            {"login_timeout",metrics_.loginTimeout},{"ready_peak",metrics_.readyPeak},
                            {"ready_at_load_start",metrics_.readyAtLoadStart},
                            {"ready_at_load_end",metrics_.readyAtLoadEnd},{"setup_success_rate",setupRate},
                            {"survival_rate",survivalRate},{"unexpected_close",metrics_.unexpectedClose},
                            {"socket_errors",metrics_.socketErrors},{"parse_fail",metrics_.parseFail},
                            {"login_error_codes",loginErrors}}},
            {"group",{{"requests",requestMetricsJson(metrics_.group,measuredSeconds)},
                      {"fanout_sent",metrics_.groupFanoutSent},
                      {"fanout_dropped",metrics_.groupFanoutDropped},
                      {"fanout_closed",metrics_.groupFanoutClosed},
                      {"fanout_overloaded",metrics_.groupFanoutOverloaded},
                      {"push_recv",metrics_.groupPushRecv},{"push_delivery_ratio",groupDelivery}}},
            {"direct",{{"requests",requestMetricsJson(metrics_.direct,measuredSeconds)},
                       {"delivered",metrics_.directDelivered},
                       {"queued_offline",metrics_.directQueuedOffline},
                       {"server_push_sent",metrics_.directPushSent},
                       {"server_push_failed",metrics_.directPushFailed},
                       {"push_recv",metrics_.directPushRecv},{"push_delivery_ratio",directDelivery}}},
            {"scheduler",{{"throttled",metrics_.schedulerThrottled},
                          {"lag_events",metrics_.schedulerLagEvents},
                          {"peak_inflight",metrics_.peakInflight},
                          {"unmatched_response",metrics_.unmatchedResponse}}},
            {"heartbeat",{{"ping_recv",metrics_.heartbeatPingRecv},
                          {"pong_queued",metrics_.heartbeatPongQueued},
                          {"pong_sent",metrics_.heartbeatPongSent},
                          {"pong_fail",metrics_.heartbeatPongFail}}},
            {"traffic",{{"bytes_sent",metrics_.bytesSent},{"bytes_recv",metrics_.bytesRecv},
                        {"send_mib_per_s",elapsedSeconds>0.0?metrics_.bytesSent/1048576.0/elapsedSeconds:0.0},
                        {"recv_mib_per_s",elapsedSeconds>0.0?metrics_.bytesRecv/1048576.0/elapsedSeconds:0.0}}},
            {"process",{{"load_generator",processJson(loadProcessBegin_,loadProcessEnd_,loadPeakRss_,
                                                       loadPeakFd_,loadPeakThreads_,elapsedSeconds)},
                        {"server",processJson(serverProcessBegin_,serverProcessEnd_,serverPeakRss_,
                                              serverPeakFd_,serverPeakThreads_,elapsedSeconds)}}},
            {"error_codes",errors},{"interrupted",interrupted_},{"passed",passed}
        };
    }

    void writeResult(const json& result)const{
        ensureParentDirectory(args_.jsonOutput);
        std::ofstream output(args_.jsonOutput,std::ios::trunc);
        if(!output) throw std::runtime_error("cannot open JSON output: "+args_.jsonOutput);
        output<<result.dump(2)<<'\n';
        if(args_.csvOutput.empty()) return;
        ensureParentDirectory(args_.csvOutput);
        const bool header=!std::filesystem::exists(args_.csvOutput)||
                          std::filesystem::file_size(args_.csvOutput)==0;
        std::ofstream csv(args_.csvOutput,std::ios::app);
        if(!csv) throw std::runtime_error("cannot open CSV output: "+args_.csvOutput);
        if(header){
            csv<<"timestamp,clients,total_qps,group_ratio,group_ok,group_success,group_p95_ms,"
                   "group_push_ratio,dm_ok,dm_success,dm_p95_ms,dm_push_ratio,unexpected_close,"
                   "server_cpu_pct,server_rss_peak_kb,server_fd_peak,passed\n";
        }
        csv<<result.at("timestamp").get<std::string>()<<','
           <<result.at("config").at("clients")<<','<<result.at("config").at("total_qps")<<','
           <<result.at("config").at("group_ratio")<<','
           <<result.at("group").at("requests").at("ok")<<','
           <<result.at("group").at("requests").at("success_rate")<<','
           <<result.at("group").at("requests").at("latency_ms").at("p95")<<','
           <<result.at("group").at("push_delivery_ratio")<<','
           <<result.at("direct").at("requests").at("ok")<<','
           <<result.at("direct").at("requests").at("success_rate")<<','
           <<result.at("direct").at("requests").at("latency_ms").at("p95")<<','
           <<result.at("direct").at("push_delivery_ratio")<<','
           <<result.at("connections").at("unexpected_close")<<','
           <<result.at("process").at("server").at("cpu_percent")<<','
           <<result.at("process").at("server").at("rss_peak_kb")<<','
           <<result.at("process").at("server").at("fd_peak")<<','
           <<(result.at("passed").get<bool>()?1:0)<<'\n';
    }

    Args args_;
    std::vector<AccountPlan> plans_;
    int epollFd_{-1};
    sockaddr_in address_{};
    std::unordered_map<int,std::unique_ptr<ConnectionContext>> contexts_;
    std::vector<int> readyFds_;
    size_t readyCursor_{0};
    Metrics metrics_;
    uint64_t connecting_{0};
    uint64_t loggingIn_{0};
    uint64_t currentInflight_{0};
    TimePoint begin_{};
    TimePoint nextConnectAt_{};
    TimePoint nextProgressAt_{};
    TimePoint nextSampleAt_{};
    TimePoint nextTimeoutScan_{};
    TimePoint nextRequestAt_{};
    Clock::duration connectInterval_{};
    Clock::duration requestInterval_{};
    std::optional<TimePoint> loadBegin_;
    std::optional<TimePoint> warmupEnd_;
    std::optional<TimePoint> measureEnd_;
    std::optional<TimePoint> drainDeadline_;
    bool finished_{false};
    bool interrupted_{false};
    std::string runId_;
    std::mt19937_64 random_;
    ProcessSnapshot loadProcessBegin_;
    ProcessSnapshot loadProcessEnd_;
    ProcessSnapshot serverProcessBegin_;
    ProcessSnapshot serverProcessEnd_;
    uint64_t loadPeakRss_{0};
    uint64_t loadPeakFd_{0};
    uint64_t loadPeakThreads_{0};
    uint64_t serverPeakRss_{0};
    uint64_t serverPeakFd_{0};
    uint64_t serverPeakThreads_{0};
};

}

int main(int argc,char** argv){
    ::signal(SIGPIPE,SIG_IGN);
    ::signal(SIGINT,requestStop);
    ::signal(SIGTERM,requestStop);
    Args args;
    if(!parseArgs(argc,argv,args)){
        return argc>=2&&std::string(argv[1])=="--help"?0:2;
    }
    const auto plans=loadManifest(args);
    if(!plans) return 3;
    try{
        MixedLoadRunner runner(std::move(args),*plans);
        return runner.run();
    }
    catch(const std::exception& exception){
        std::cerr<<"mixed load test failed: "<<exception.what()<<'\n';
        return 5;
    }
}
