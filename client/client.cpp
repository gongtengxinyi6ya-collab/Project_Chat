// src/client.cpp
#include "Buffer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
#include <optional>
#include <thread>
#include <unordered_set>

#include "logger/LogMacros.h"
#include "im/ImMessage.h"
#include "third_party/json.hpp"
struct ClientState{
    std::string username;
    std::string pendingLoginUsername;
    bool loggedIn{false};
    std::unordered_set<std::string> groupIds;
    uint64_t nextReqId{1};
    uint64_t nextSeq{1};
    std::string token;
    int64_t tokenExpireAtMs{0};

    uint64_t allocReqId(){
        return nextReqId++;
    }
    uint64_t allocSeq(){
        return nextSeq++;
    }
};
class CommandBuilder{
public:
    std::string buildAuthReq(ClientState& state,std::string user){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::AUTH_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=user;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["user"]=user;
        return body.dump();
        
    }
    std::string buildDmReq(ClientState& state,std::string to,std::string content){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::DM_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]=to;
        body["seq"]=state.allocSeq();
        body["content"]=content;
        return body.dump();
    }
    std::string buildListUsersReq(ClientState& state){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::LIST_USERS_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        return body.dump();
    }
    std::string buildCreateGroupReq(ClientState& state,std::string groupName){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::CREATE_GROUP_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["groupName"]=groupName;
        return body.dump();
    }
    std::string buildJoinReq(ClientState& state,std::string groupId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::JOIN_GROUP_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["groupId"]=groupId;
        return body.dump();
    }
    std::string buildLeaveReq(ClientState& state,std::optional<std::string> groupId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::LEAVE_GROUP_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        if(groupId.has_value()){
            body["groupId"]=groupId.value();
        }
        return body.dump();
    }
    std::string buildGroupMsgReq(ClientState& state,std::string content,std::optional<std::string> groupId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::GROUP_MSG_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["content"]=content;
        if(groupId.has_value()){
            body["groupId"]=groupId.value();
        }
        return body.dump();
    }
    std::string buildGroupMembers(ClientState& state,std::optional<std::string> groupId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::GROUP_MEMBERS_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        if(groupId.has_value()){
            body["groupId"]=groupId.value();
        }
        return body.dump();
    }
    std::string buildListGroupsReq(ClientState& state){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::LIST_GROUPS_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        return body.dump();
    }
    std::string buildHistoryReq(ClientState& state,std::string groupId,uint64_t beforeMsgId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::GROUP_HISTORY_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["groupId"]=groupId;
        body["beforeMsgId"]=beforeMsgId;
        return body.dump();
    }
    std::string buildOfflineListReq(ClientState& state,size_t limit){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::OFFLINE_LIST_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["limit"]=limit;
        return body.dump();
    }
    std::string buildOfflineAckReq(ClientState& state,const std::vector<uint64_t>& msgIds){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::OFFLINE_ACK_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["msg_ids"]=msgIds;
        return body.dump();
    }
    std::string buildRegisterReq(ClientState& state,std::string username,std::string password){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::REGISTER_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["username"]=username;
        body["password"]=password;
        return body.dump();
    }
    std::string buildLoginReq(ClientState& state,std::string username,std::string password){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::LOGIN_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=username;
        body["to"]="";
        body["seq"]=state.allocSeq();  
        body["username"]=username;
        body["password"]=password;
        return body.dump();
    }
    std::string buildLogoutReq(ClientState& state){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::LOGOUT_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();  
        body["token"]=state.token;
        return body.dump();
    }
    std::string buildTokenLoginReq(ClientState& state,std::string token){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::TOKEN_LOGIN_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]="";
        body["to"]="";
        body["seq"]=state.allocSeq();  
        body["token"]=token;
        return body.dump();
    }
};
//把/auth jason,/dm tom hello,/list,/gjoin room,/gleave ,/gmembers,/gls,/gsay 转为payload字符串，返回nullopt表示解析失败
std::optional<std::string> tryParseCommandLine(const std::string line,ClientState& state){
    if(line.empty()) return std::nullopt;
    if(line[0]!='/'){
        //不是/开头，默认当raw payload
        return line;
    }
    CommandBuilder builder;
    if(line.rfind("/auth ",0)==0){
        std::string user=line.substr(6);
        state.username=user;
        return builder.buildAuthReq(state,user);
    }
    if(line.rfind("/dm ",0)==0){
        if(state.username.empty()){
            std::cerr<<"Please authenticate first using /auth <username>"<<std::endl;
            return std::nullopt;
        }
        size_t firstSpace=line.find(' ',4);
        if(firstSpace==std::string::npos) return std::nullopt;
        std::string to=line.substr(4,firstSpace-4);
        std::string content=line.substr(firstSpace+1);
        return builder.buildDmReq(state,to,content);
    }
    if(line=="/list"){
        return builder.buildListUsersReq(state);
    }
    if(line.rfind("/gjoin ",0)==0){
        if(state.username.empty()){
            std::cerr<<"Please authenticate first using /login <username> <password>"<<std::endl;
            return std::nullopt;
        }
        std::string groupId=line.substr(7);
        return builder.buildJoinReq(state,groupId);
    }
    if(line=="/gleave"){
        if(state.username.empty()){
            std::cerr<<"please authenticate first"<<std::endl;
            return std::nullopt;
        }
        return builder.buildLeaveReq(state,std::nullopt);
    }
    if(line.rfind("/gleave ",0)==0){
        if(state.username.empty()){
            std::cerr<<"please authenticate first"<<std::endl;
            return std::nullopt;
        }
        std::string groupId=line.substr(8);
        return builder.buildLeaveReq(state,groupId);
    }
    
    if(line.rfind("/gsayto ",0)==0){
        if(state.username.empty()){
            std::cerr<<"Please authenticate first using /auth <username>"<<std::endl;
            return std::nullopt;
        }
        size_t firstSpace=line.find(' ',8);
        if(firstSpace==std::string::npos) return std::nullopt;
        std::string groupId=line.substr(8,firstSpace-8);
        std::string content=line.substr(firstSpace+1);
        return builder.buildGroupMsgReq(state,content,groupId);
    }
    if(line.rfind("/gmembers ",0)==0){
        std::string groupId=line.substr(10);
        return builder.buildGroupMembers(state,groupId);
    }
    if(line=="/gls"){
        return builder.buildListGroupsReq(state);
    }
    if(line.rfind("/gcreate ",0)==0){
        std::string groupName=line.substr(9);
        return builder.buildCreateGroupReq(state,groupName);
    }
    if(line.rfind("/ghistory ",0)==0){
        size_t firstSpace=line.find(' ',11);
        if(firstSpace==std::string::npos) return std::nullopt;
        std::string groupId=line.substr(11,firstSpace-11);
        uint64_t beforeMsgId=std::stoull(line.substr(firstSpace+1));
        return builder.buildHistoryReq(state,groupId,beforeMsgId);
    }
    if(line.rfind("/offlinelist ",0)==0){
        size_t firstSpace=line.find(' ',13);
        if(firstSpace==std::string::npos) return std::nullopt;
        size_t limit=std::stoull(line.substr(13,firstSpace-13));
        return builder.buildOfflineListReq(state,limit);
    }
    if(line.rfind("/offlineack ",0)==0){
        size_t firstSpace=line.find(' ',14);
        if(firstSpace==std::string::npos) return std::nullopt;
        std::vector<uint64_t> msgIds;
        std::string msgIdsStr=line.substr(14);
        size_t start=0;
        while(true){
            size_t commaPos=msgIdsStr.find(',',start);
            if(commaPos==std::string::npos){
                msgIds.push_back(std::stoull(msgIdsStr.substr(start)));
                break;
            }
            msgIds.push_back(std::stoull(msgIdsStr.substr(start,commaPos-start)));
            start=commaPos+1;
        }
        return builder.buildOfflineAckReq(state,msgIds);
    }
    if(line.rfind("/register ",0)==0){
        size_t firstSpace=line.find(' ',10);
        if(firstSpace==std::string::npos) return std::nullopt;
        std::string username=line.substr(10,firstSpace-10);
        std::string password=line.substr(firstSpace+1);
        return builder.buildRegisterReq(state,username,password);
    }
    if(line.rfind("/login ",0)==0){
        size_t firstSpace=line.find(' ',7);
        if(firstSpace==std::string::npos) return std::nullopt;
        std::string username=line.substr(7,firstSpace-7);
        std::string password=line.substr(firstSpace+1);
        state.pendingLoginUsername=username;
        return builder.buildLoginReq(state,username,password);
    }
    if(line=="/logout"){
        return builder.buildLogoutReq(state);
    }
    if(line.rfind("/tokenlogin ",0)==0){
        std::string token=line.substr(12);
        return builder.buildTokenLoginReq(state,token);
    }
    return std::nullopt;
}
//尝试parse JSON,按type分类打印摘要，失败则原样输出
void printPretty(const std::string& payload,ClientState& state){
    try{
        auto json=nlohmann::json::parse(payload);
        if(!json.contains("type")||!json["type"].is_number()){
            std::cout<<payload<<std::endl;
            return;
        }
        int typeInt=json["type"].get<int>();
        auto typeOpt=im::msgTypeFromInt(typeInt);
        if(!typeOpt.has_value()){
            std::cout<<payload<<std::endl;
            return;
        }
        im::MsgType type=typeOpt.value();
        switch(type){
            case im::MsgType::AUTH_RESP:
                std::cout<<"AUTH_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            case im::MsgType::DM_PUSH:
                std::cout<<"[DM] "<<json["data"]["from"].get<std::string>()<<": "<<json["data"]["content"].get<std::string>()<<std::endl;
                break;
            case im::MsgType::DM_RESP:
                std::cout<<"DM_RESP: "<<(json["ok"].get<bool>()?"delivered":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            case im::MsgType::LIST_USERS_RESP:
            {
                std::cout<<"Online users: ";
                if(json.contains("data")&&json["data"].contains("users")&&json["data"]["users"].is_array()){
                for(const auto& user:json["data"]["users"]){
                    std::cout<<user.get<std::string>()<<" ";
                }
            }
                std::cout<<std::endl;
                break;
            }
            case im::MsgType::CREATE_GROUP_RESP:{

                std::string groupId=json["data"]["groupId"].get<std::string>();
                std::cout<<state.username<<" create the group: "<<groupId<<std::endl;
                state.groupIds.insert(groupId);
                break;
            }
            case im::MsgType::JOIN_GROUP_RESP:{
                std::cout<<"JOIN_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                if(json["ok"].get<bool>()){
                    if(json["data"].contains("groupId")&&json["data"]["groupId"].is_string()){
                        state.groupIds.insert(json["data"]["groupId"]);
                    }
                }
                break;
            }
            case im::MsgType::LEAVE_GROUP_RESP:{
                std::cout<<"LEAVE_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                if(json["ok"].get<bool>()){
                    if(json["data"].contains("groupId")&&json["data"]["groupId"].is_string()){
                        state.groupIds.erase(json["data"]["groupId"]);
                    }
                }
                break;
            }
            case im::MsgType::GROUP_MSG_RESP:
                std::cout<<"ROOM_MSG_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            case im::MsgType::GROUP_MSG_PUSH:
                std::cout<<"[Group: "<<json["data"]["groupId"].get<std::string>()<<"] "<<json["data"]["from"].get<std::string>()<<": "<<json["data"]["content"].get<std::string>()<<std::endl;
                break;
            case im::MsgType::GROUP_MEMBERS_RESP:{
                std::cout<<"Group members ("<<json["data"]["count"]<<" members in total) :"<<std::endl;
                for(const auto& member:json["data"]["members"]){
                    std::cout<<member.get<std::string>()<<std::endl;
                }
                break;
            }
            case im::MsgType::GROUP_EVENT_PUSH:
                std::cout<<"[GROUP EVENT] "<<json["data"]["user"].get<std::string>()<<" "<<json["data"]["event"].get<std::string>()<<" "<<json["data"]["groupId"].get<std::string>()<<std::endl;
                break;
            case im::MsgType::LIST_GROUPS_RESP:{
                std::cout<<"Groups you joined (total "<<json["data"]["count"].get<int>()<<" groups): "<<std::endl;
                state.groupIds.clear();
                for(const auto& groupId:json["data"]["groupIds"]){
                    state.groupIds.insert(groupId.get<std::string>());
                    std::cout<<groupId.get<std::string>()<<" ";
                }
                std::cout<<std::endl;
                break;
            }
            case im::MsgType::GROUP_HISTORY_RESP:{
                std::cout<<"Group history messages: "<<std::endl;
                for(const auto& msg:json["data"]["messages"]){
                    std::cout<<"[Group: "<<msg["groupId"].get<std::string>()<<"] "<<msg["from"].get<std::string>()<<": "<<msg["content"].get<std::string>()<<" (msgId: "<<msg["msgId"].get<uint64_t>()<<")"<<std::endl;
                }
                break;
            }
            case im::MsgType::LOGIN_RESP:{
                if(json["ok"].get<bool>()){
                    if(json["data"].contains("username")&&json["data"]["username"].is_string()){
                        state.username=json["data"]["username"].get<std::string>();
                    }
                    state.loggedIn=true;
                    state.pendingLoginUsername.clear();
                    if(json["data"].contains("token")&&json["data"]["token"].is_string()){
                        state.token=json["data"]["token"].get<std::string>();
                    }
                    if(json["data"].contains("expireAtMs")&&json["data"]["expireAtMs"].is_number()){
                        state.tokenExpireAtMs=json["data"]["expireAtMs"].get<int64_t>();
                    }
                    std::cout<<"Login successful, token: "<<state.token<<" expireAt: "<<state.tokenExpireAtMs<<std::endl;
                }
                else{
                state.loggedIn=false;
                state.pendingLoginUsername.clear();
                }
                std::cout<<"LOGIN_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            }
            case im::MsgType::REGISTER_RESP:{
                std::cout<<"REGISTER_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            }
            case im::MsgType::TOKEN_LOGIN_RESP:{
                if(json["ok"].get<bool>()){
                    if(json["data"].contains("username")&&json["data"]["username"].is_string()){
                        state.username=json["data"]["username"].get<std::string>();
                    }
                    state.loggedIn=true;
                    state.pendingLoginUsername.clear();
                    if(json["data"].contains("expireAtMs")&&json["data"]["expireAtMs"].is_number()){
                        state.tokenExpireAtMs=json["data"]["expireAtMs"].get<int64_t>();
                    }
                }
                else{
                state.loggedIn=false;
                state.pendingLoginUsername.clear();
                }
                std::cout<<"TOKEN_LOGIN_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            }
            case im::MsgType::LOGOUT_RESP:{
                if(json["ok"].get<bool>()){
                    state.username.clear();
                    state.loggedIn=false;
                    state.groupIds.clear();
                    state.token.clear();
                    state.tokenExpireAtMs=0;
                }
                std::cout<<"LOGOUT_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            }
            default:
                std::cout<<payload<<std::endl;
                break;
        }
    }catch(const std::exception& e){
        std::cerr<<"Failed to parse JSON: "<<e.what()<<std::endl;
        std::cout<<payload<<std::endl;
    }
}

static bool sendAllFramed(int fd, const std::string& payload) {
    if (payload.size() > kMaxFrameLen) {
        std::cerr << "payload too large: " << payload.size() << " > " << kMaxFrameLen << "\n";
        return false;
    }

    Buffer out;
    out.appendUint32(static_cast<uint32_t>(payload.size()));
    out.append(payload.data(), payload.size());

    while (out.readableBytes() > 0) {
        ssize_t n = ::send(fd, out.peek(), out.readableBytes(), MSG_NOSIGNAL);
        if (n > 0) {
            out.retrieve(static_cast<size_t>(n));
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        std::cerr << "send failed: errno=" << errno << " " << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

static void recvLoop(int fd, std::atomic<bool>& running,ClientState& state) {
    Buffer in;
    char tmp[4096];

    while (running.load()) {
        ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n > 0) {
            in.append(tmp, static_cast<size_t>(n));

            while (true) {
                if (in.readableBytes() < 4) break;

                uint32_t len = in.peekUInt32();
                if (len == 0 || len > kMaxFrameLen) {
                    std::cerr << "protocol error: len=" << len << "\n";
                    running.store(false);
                    return;
                }
                if (in.readableBytes() < 4 + len) break;

                in.retrieveUInt32();
                std::string payload = in.retrieveAsString(len);
                if(payload=="PING"){
                    if(!sendAllFramed(fd, "PONG")){
                        running.store(false);
                        return;
                    }
                    continue;
                }
                printPretty(payload,state);
            }
            continue;
        }

        if (n == 0) {
            std::cerr << "server closed\n";
            running.store(false);
            return;
        }

        if (errno == EINTR) continue;
        std::cerr << "recv failed: errno=" << errno << " " << std::strerror(errno) << "\n";
        running.store(false);
        return;
    }
}

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

    const char* ip = "127.0.0.1";
    int port = 8080;
    if (argc >= 2) ip = argv[1];
    if (argc >= 3) port = std::stoi(argv[2]);

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        std::cerr << "inet_pton failed for ip=" << ip << "\n";
        ::close(fd);
        return 1;
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "connect failed: errno=" << errno << " " << std::strerror(errno) << "\n";
        ::close(fd);
        return 1;
    }
    LOG_INFO("Connected to " + std::string(ip) + ":" + std::to_string(port));

    std::string line;
    ClientState state;
    std::atomic<bool> running{true};
    std::thread reader([&] { recvLoop(fd, running,state); });

    while (running.load() && std::getline(std::cin, line)) {
        if (line == "/quit") break;
        auto payloadOpt = tryParseCommandLine(line, state);
        if (!payloadOpt) {
            std::cerr << "invalid command\n";
            continue;   
        }
        if (!sendAllFramed(fd, *payloadOpt)) {
            running.store(false);
            break;
        }
    }

    running.store(false);
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);

    if (reader.joinable()) reader.join();
    return 0;
}
