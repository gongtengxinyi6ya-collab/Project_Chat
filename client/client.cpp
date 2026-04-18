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

#include "LogMacros.h"
#include "im/ImMessage.h"
#include "third_party/json.hpp"
struct ClientState{
    std::string username;
    std::unordered_set<std::string> rooms;
    std::string activeRoom;
    uint64_t nextReqId{1};
    uint64_t nextSeq{1};

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
    std::string buildJoinReq(ClientState& state,std::string room){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::JOIN_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["room"]=room;
        return body.dump();
    }
    std::string buildLeaveReq(ClientState& state,std::optional<std::string> room){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::LEAVE_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        if(room.has_value()){
            body["room"]=room.value();
        }
        return body.dump();
    }
    std::string buildRoomMsgReq(ClientState& state,std::string content,std::optional<std::string> room){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::ROOM_MSG_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["content"]=content;
        if(room.has_value()){
            body["room"]=room.value();
        }
        return body.dump();
    }
    std::string buildRoomMembers(ClientState& state,std::optional<std::string> room){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::ROOM_MEMBERS_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.username;
        body["to"]="";
        body["seq"]=state.allocSeq();
        if(room.has_value()){
            body["room"]=room.value();
        }
        return body.dump();
    }
};
//把/auth jason,/dm tom hello,/list,/join room,/leave ,/say 转为payload字符串，返回nullopt表示解析失败
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
    if(line.rfind("/join ",0)==0){
        if(state.username.empty()){
            std::cerr<<"Please authenticate first using /auth <username>"<<std::endl;
            return std::nullopt;
        }
        std::string room=line.substr(6);
        return builder.buildJoinReq(state,room);
    }
    if(line=="/leave"){
        if(state.username.empty()){
            std::cerr<<"please authenticate first"<<std::endl;
            return std::nullopt;
        }
        return builder.buildLeaveReq(state,std::nullopt);
    }
    if(line.rfind("/leave ",0)==0){
        if(state.username.empty()){
            std::cerr<<"please authenticate first"<<std::endl;
            return std::nullopt;
        }
        std::string room=line.substr(7);
        return builder.buildLeaveReq(state,room);
    }
    if(line.rfind("/say ",0)==0){
        if(state.username.empty()){
            std::cerr<<"Please authenticate first using /auth <username>"<<std::endl;
            return std::nullopt;
        }
        std::string content=line.substr(5);
        return builder.buildRoomMsgReq(state,content,std::nullopt);
    }
    if(line.rfind("/say ",0)==0){
        if(state.username.empty()){
            std::cerr<<"Please authenticate first using /auth <username>"<<std::endl;
            return std::nullopt;
        }
        size_t firstSpace=line.find(' ',5);
        if(firstSpace==std::string::npos) return std::nullopt;
        std::string room=line.substr(5,firstSpace-5);
        std::string content=line.substr(firstSpace+1);
        return builder.buildRoomMsgReq(state,content,room);
    }
    if(line=="/members"){
        return builder.buildRoomMembers(state,std::nullopt);
    }
    if(line.rfind("/members ",0)==0){
        std::string room=line.substr(9);
        return builder.buildRoomMembers(state,room);
    }
    if(line=="/rooms"){
        std::cout<<"Room Lists: "<<std::endl;
        for(const auto& room:state.rooms){
            std::cout<<room<<std::endl;
        }
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
                for(const auto& user:json["data"]["users"]){
                    std::cout<<user.get<std::string>()<<" ";
                }
                std::cout<<std::endl;
                break;
            }
            case im::MsgType::JOIN_RESP:{
                std::cout<<"JOIN_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                if(json["ok"].get<bool>()){
                    state.rooms.insert(json["data"]["room"]);
                    state.activeRoom=json["data"]["active_room"];
                }
                break;
            }
            case im::MsgType::LEAVE_RESP:{
                std::cout<<"LEAVE_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                if(json["ok"].get<bool>()){
                    state.rooms.erase(json["data"]["room"]);
                    state.activeRoom=json["data"]["active_room"];
                }
                break;
            }
            case im::MsgType::ROOM_MSG_RESP:
                std::cout<<"ROOM_MSG_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            case im::MsgType::ROOM_MSG_PUSH:
                std::cout<<"[Room: "<<json["data"]["room"].get<std::string>()<<"] "<<json["data"]["from"].get<std::string>()<<": "<<json["data"]["content"].get<std::string>()<<std::endl;
                break;
            case im::MsgType::ROOM_MEMBERS_RESP:{
                std::cout<<"Room members ("<<json["data"]["count"]<<" members in total) :"<<std::endl;
                for(const auto& member:json["data"]["members"]){
                    std::cout<<member.get<std::string>()<<" ";
                }
                break;
            }
            case im::MsgType::ROOM_EVENT_PUSH:
                std::cout<<"[ROOM EVENT] "<<json["data"]["user"].get<std::string>()<<" "<<json["data"]["event"].get<std::string>()<<" "<<json["data"]["room"].get<std::string>()<<std::endl;
                break;
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
