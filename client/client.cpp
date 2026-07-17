// src/client.cpp
#include "Buffer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "logger/LogMacros.h"
#include "im/ImMessage.h"
#include "third_party/json.hpp"
struct ClientState{
    std::string accountId;
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
        body["from"]=state.accountId;
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
        body["from"]=state.accountId;
        body["to"]="";
        body["seq"]=state.allocSeq();
        return body.dump();
    }
    std::string buildCreateGroupReq(ClientState& state,std::string groupName){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::CREATE_GROUP_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["groupName"]=groupName;
        return body.dump();
    }
    std::string buildJoinReq(ClientState& state,std::string groupId,std::string message="request to join group"){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::JOIN_GROUP_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["groupId"]=groupId;
        body["message"]=message;
        return body.dump();
    }
    std::string buildApplyGroupJoinReq(ClientState& state,const std::string& groupId,const std::string& message){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::APPLY_GROUP_JOIN_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["groupId"]=groupId;
        body["message"]=message;
        return body.dump();
    }
    std::string buildReviewGroupJoinReq(ClientState& state,const std::string& groupId,
                                        const std::string& applicantAccountId,bool approve){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::REVIEW_GROUP_JOIN_REQUEST_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;
        body["to"]="";
        body["seq"]=state.allocSeq();
        body["groupId"]=groupId;
        body["applicantAccountId"]=applicantAccountId;
        body["approve"]=approve;
        return body.dump();
    }
    std::string buildLeaveReq(ClientState& state,std::optional<std::string> groupId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::LEAVE_GROUP_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;
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
        body["from"]=state.accountId;
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
        body["from"]=state.accountId;
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
        body["from"]=state.accountId;
        body["to"]="";
        body["seq"]=state.allocSeq();
        return body.dump();
    }
    std::string buildHistoryReq(ClientState& state,std::string groupId,uint64_t beforeMsgId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::GROUP_HISTORY_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;
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
        body["from"]=state.accountId;
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
        body["from"]=state.accountId;
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
    std::string buildLoginReq(ClientState& state,std::string accountId,std::string password){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::LOGIN_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=accountId;
        body["accountId"]=accountId;
        body["to"]="";
        body["seq"]=state.allocSeq();  
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
    std::string buildGetProfileReq(ClientState& state){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::GET_PROFILE_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;
        body["to"]="";
        body["seq"]=state.allocSeq();  
        return body.dump();
    }
    std::string buildUpdateProfileReq(ClientState& state,std::optional<std::string> nickname,std::optional<std::string> avatarUrl,std::optional<std::string> signature){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::UPDATE_PROFILE_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;    
        body["to"]="";
        body["seq"]=state.allocSeq();  
        if(nickname.has_value()){
            body["nickname"]=nickname.value();
        }
        if(avatarUrl.has_value()){
            body["avatarUrl"]=avatarUrl.value();
        }
        if(signature.has_value()){
            body["signature"]=signature.value();
        }
        return body.dump();
    }
    std::string buildSearchUserReq(ClientState& state,std::string accountId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::SEARCH_USER_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;    
        body["to"]="";
        body["seq"]=state.allocSeq();  
        body["accountId"]=accountId;
        return body.dump();
    }
    std::string buildListFriendsReq(ClientState& state){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::LIST_FRIENDS_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;    
        body["to"]="";
        body["seq"]=state.allocSeq();  
        return body.dump();
    }
    std::string buildSendFriendRequestReq(ClientState& state,std::string targetAccountId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::SEND_FRIEND_REQUEST_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;    
        body["to"]="";
        body["seq"]=state.allocSeq();  
        body["targetAccountId"]=targetAccountId;
        return body.dump();
    }
    std::string buildListFriendRequestReq(ClientState& state){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::LIST_FRIEND_REQUEST_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;    
        body["to"]="";
        body["seq"]=state.allocSeq();  
        return body.dump();
    }
    std::string buildAcceptFriendRequestReq(ClientState& state,std::string requestId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::ACCEPT_FRIEND_REQUEST_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;    
        body["to"]="";
        body["seq"]=state.allocSeq();  
        body["requestId"]=std::stoull(requestId);
        return body.dump();
    }
    std::string buildRejectFriendRequestReq(ClientState& state,std::string requestId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::REJECT_FRIEND_REQUEST_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;    
        body["to"]="";
        body["seq"]=state.allocSeq();  
        body["requestId"]=std::stoull(requestId);
        return body.dump();
    }
    std::string buildRemoveFriendReq(ClientState& state,std::string friendAccountId){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::REMOVE_FRIEND_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;    
        body["to"]="";
        body["seq"]=state.allocSeq();  
        body["targetAccountId"]=friendAccountId;
        return body.dump();
    }
    std::string buildDmHistoryReq(ClientState& state,std::string peerAccountId,uint64_t beforeMsgId,uint64_t lastMsgId,size_t limit=20){
        nlohmann::json body;
        body["ver"]=1;
        body["type"]=im::msgTypeToInt(im::MsgType::DM_HISTORY_REQ);
        body["req_id"]=state.allocReqId();
        body["from"]=state.accountId;    
        body["to"]="";
        body["seq"]=state.allocSeq();  
        body["peerAccountId"]=peerAccountId;
        body["beforeMsgId"]=beforeMsgId;
        body["lastMsgId"]=lastMsgId;
        body["limit"]=limit;
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
        state.accountId=user;
        return builder.buildAuthReq(state,user);
    }
    if(line.rfind("/dm ",0)==0){
        if(state.accountId.empty()){
            std::cerr<<"Please authenticate first using /login <accountId> <password>"<<std::endl;
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
        if(state.accountId.empty()){
            std::cerr<<"Please authenticate first using /login <accountId> <password>"<<std::endl;
            return std::nullopt;
        }
        std::string arguments=line.substr(7);
        size_t firstSpace=arguments.find(' ');
        if(firstSpace==std::string::npos){
            return builder.buildJoinReq(state,arguments);
        }
        std::string groupId=arguments.substr(0,firstSpace);
        std::string message=arguments.substr(firstSpace+1);
        return builder.buildJoinReq(state,groupId,message);
    }
    if(line=="/gleave"){
        if(state.accountId.empty()){
            std::cerr<<"Please authenticate first using /login <accountId> <password>"<<std::endl;
            return std::nullopt;
        }
        return builder.buildLeaveReq(state,std::nullopt);
    }
    if(line.rfind("/gleave ",0)==0){
        if(state.accountId.empty()){
            std::cerr<<"Please authenticate first using /login <accountId> <password>"<<std::endl;
            return std::nullopt;
        }
        std::string groupId=line.substr(8);
        return builder.buildLeaveReq(state,groupId);
    }
    
    if(line.rfind("/gsayto ",0)==0){
        if(state.accountId.empty()){
            std::cerr<<"Please authenticate first using /login <accountId> <password>"<<std::endl;
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
    if(line=="/getprofile"){
        return builder.buildGetProfileReq(state);
    }
    if(line.rfind("/updateprofile ",0)==0){
        size_t firstSpace=line.find(' ',15);
        if(firstSpace==std::string::npos) return std::nullopt;
        std::string nickname;
        std::string avatarUrl;
        std::string signature;
        size_t secondSpace=line.find(' ',firstSpace+1); 
        if(secondSpace==std::string::npos){
            nickname=line.substr(15,firstSpace-15);
            avatarUrl=line.substr(firstSpace+1);
        }
        else{
            nickname=line.substr(15,firstSpace-15);
            avatarUrl=line.substr(firstSpace+1,secondSpace-firstSpace-1);
            signature=line.substr(secondSpace+1);
        }
        return builder.buildUpdateProfileReq(state,nickname.empty()?std::nullopt:std::optional<std::string>(nickname),avatarUrl.empty()?std::nullopt:std::optional<std::string>(avatarUrl),signature.empty()?std::nullopt:std::optional<std::string>(signature));
    }
    if(line.rfind("/searchuser ",0)==0){
        std::string accountId=line.substr(12);
        return builder.buildSearchUserReq(state,accountId);
    }
    if(line=="/listfriends"){
        return builder.buildListFriendsReq(state);
    }
    if(line.rfind("/sendfriendrequest ",0)==0){
        std::string targetAccountId=line.substr(19);
        return builder.buildSendFriendRequestReq(state,targetAccountId);
    }
    if(line=="/listfriendrequests"){
        return builder.buildListFriendRequestReq(state);
    }
    if(line.rfind("/acceptfriendrequest ",0)==0){
        std::string requestId=line.substr(21);
        //捕获requestId,非数字时会抛出异常，外层捕获后提示解析失败
        if(requestId.empty()){
            return std::nullopt;
        }
        return builder.buildAcceptFriendRequestReq(state,requestId);
    }
    if(line.rfind("/rejectfriendrequest ",0)==0){
        std::string requestId=line.substr(21);  
        return builder.buildRejectFriendRequestReq(state,requestId);
    }
    if(line.rfind("/removefriend ",0)==0){
        std::string friendAccountId=line.substr(14);  
        return builder.buildRemoveFriendReq(state,friendAccountId);
    }
    if(line.rfind("/dmhistory ",0)==0){
        size_t firstSpace=line.find(' ',11);
        if(firstSpace==std::string::npos) return std::nullopt;
        std::string peerAccountId=line.substr(11,firstSpace-11);
        uint64_t beforeMsgId=std::stoull(line.substr(firstSpace+1));
        return builder.buildDmHistoryReq(state,peerAccountId,beforeMsgId,0);
    }
    if (line.rfind("/dmsync ", 0) == 0) {
    size_t firstSpace = line.find(' ', 8);
    if (firstSpace == std::string::npos) {
        return std::nullopt;
    }

    std::string peerAccountId = line.substr(8, firstSpace - 8);
    uint64_t lastMsgId = std::stoull(line.substr(firstSpace + 1));

    return builder.buildDmHistoryReq(
        state,
        peerAccountId,
        0,
        lastMsgId
    );
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
                std::cout<<"[DM] "<<json["data"]["fromAccountId"].get<std::string>()<<": "<<json["data"]["content"].get<std::string>()<<std::endl;
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
            case im::MsgType::APPLY_GROUP_JOIN_RESP:{
                if(!json.value("ok",false)){
                    std::cout<<"GROUP_JOIN_APPLY_RESP: failed msg: "<<json.value("msg","")<<std::endl;
                    break;
                }
                const auto& data=json["data"];
                std::cout<<"GROUP_JOIN_APPLY_RESP: success groupId="<<data.value("groupId","")
                         <<" submitted="<<data.value("submitted",false)
                         <<" alreadyPending="<<data.value("alreadyPending",false)
                         <<" alreadyIn="<<data.value("alreadyIn",false)<<std::endl;
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
                std::cout<<"[Group: "<<json["data"]["groupId"].get<std::string>()<<"] "<<json["data"]["fromUsername"].get<std::string>()<<": "<<json["data"]["content"].get<std::string>()<<std::endl;
                break;
            case im::MsgType::GROUP_MEMBERS_RESP:{
                std::cout<<"Group members ("<<json["data"]["count"]<<" members in total) :"<<std::endl;
                for (const auto& member : json["data"]["members"]) {
                    if (member.is_string()) {
                    std::cout << member.get<std::string>() << std::endl;
                }else {
                std::string accountId = member.value("accountId", "");
                std::string nickname = member.value("nickname", "");
                std::string username = member.value("username", "");

                std::string display = !nickname.empty() ? nickname : username;
                std::cout << accountId;
                if (!display.empty()) {
                     std::cout << " (" << display << ")";
                }
                std::cout << std::endl;
    }
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
                        std::cout<<"[Group: "<<msg["groupId"].get<std::string>()<<"] "<<msg["senderUsername"].get<std::string>()<<": "<<msg["content"].get<std::string>()<<" (msgId: "<<msg["msgId"].get<uint64_t>()<<")"<<std::endl;
                }
                break;
            }
            case im::MsgType::OFFLINE_LIST_RESP:{
                for(const auto& msg:json["data"]["messages"]){
                    if(msg["type"].get<std::string>()=="Direct"){
                         std::cout<<"[Offline DM] peerAccountId: "<<msg["peerAccountId"].get<std::string>()<<" (msgId: "<<msg["msgId"].get<uint64_t>()<<")"<<std::endl;
                    }
                    else if(msg["type"].get<std::string>()=="Group"){
                         std::cout<<"[Offline Group] "<<msg["groupId"].get<std::string>()<<" (msgId: "<<msg["msgId"].get<uint64_t>()<<")"<<std::endl;
                    }
                }
                break;
            }
            case im::MsgType::LOGIN_RESP:{
                if(json["ok"].get<bool>()){
                    if(json["data"].contains("username")&&json["data"]["username"].is_string()){
                        state.username=json["data"]["username"].get<std::string>();
                    }
                    if(json["data"].contains("accountId")&&json["data"]["accountId"].is_string()){
                        state.accountId=json["data"]["accountId"].get<std::string>();
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
                if(json["ok"].get<bool>()){
                    if(json["data"].contains("accountId")&&json["data"]["accountId"].is_string()){
                        state.accountId=json["data"]["accountId"].get<std::string>();
                        std::cout<<"Registered successfully, accountId: "<<state.accountId<<std::endl;
                    }
                }
                std::cout<<"REGISTER_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            }
            case im::MsgType::TOKEN_LOGIN_RESP:{
                if(json["ok"].get<bool>()){
                    if(json["data"].contains("username")&&json["data"]["username"].is_string()){
                        state.username=json["data"]["username"].get<std::string>();
                    }
                    if(json["data"].contains("accountId")&&json["data"]["accountId"].is_string()){
                        state.accountId=json["data"]["accountId"].get<std::string>();
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
                    state.accountId.clear();
                    state.username.clear();
                    state.loggedIn=false;
                    state.groupIds.clear();
                    state.token.clear();
                    state.tokenExpireAtMs=0;
                }
                std::cout<<"LOGOUT_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            }
            case im::MsgType::GET_PROFILE_RESP:{
                if(json["ok"].get<bool>()){
                    std::cout<<"Profile: "<<std::endl;
                    if(json["data"].contains("accountId")&&json["data"]["accountId"].is_string()){
                        std::cout<<"AccountId: "<<json["data"]["accountId"].get<std::string>()<<std::endl;
                    }
                    if(json["data"].contains("username")&&json["data"]["username"].is_string()){
                        std::cout<<"Username: "<<json["data"]["username"].get<std::string>()<<std::endl;
                    }
                    if(json["data"].contains("nickname")&&json["data"]["nickname"].is_string()){
                        std::cout<<"Nickname: "<<json["data"]["nickname"].get<std::string>()<<std::endl;
                    }
                    if(json["data"].contains("avatarUrl")&&json["data"]["avatarUrl"].is_string()){
                        std::cout<<"AvatarUrl: "<<json["data"]["avatarUrl"].get<std::string>()<<std::endl;
                    }
                    if(json["data"].contains("signature")&&json["data"]["signature"].is_string()){
                        std::cout<<"Signature: "<<json["data"]["signature"].get<std::string>()<<std::endl;
                    }
                }
                else{
                    std::cout<<"Failed to get profile: "<<json["msg"].get<std::string>()<<std::endl;
                }
                break;
            }
            case im::MsgType::UPDATE_PROFILE_RESP:{

                std::cout<<"UPDATE_PROFILE_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                if(json["ok"].get<bool>()){
                    if(json["data"].contains("nickname")&&json["data"]["nickname"].is_string()){
                        std::cout<<"New nickname: "<<json["data"]["nickname"].get<std::string>()<<std::endl;
                    }
                    if(json["data"].contains("avatarUrl")&&json["data"]["avatarUrl"].is_string()){
                        std::cout<<"New avatarUrl: "<<json["data"]["avatarUrl"].get<std::string>()<<std::endl;
                    }
                    if(json["data"].contains("signature")&&json["data"]["signature"].is_string()){
                        std::cout<<"New signature: "<<json["data"]["signature"].get<std::string>()<<std::endl;
                    }
                }
                break;
            }
            case im::MsgType::SEARCH_USER_RESP:{
                std::cout<<"SEARCH_USER_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                if(json["ok"].get<bool>()){
                    std::cout<<"User profile: "<<std::endl;
                    if(json["data"].contains("accountId")&&json["data"]["accountId"].is_string()){
                        std::cout<<"AccountId: "<<json["data"]["accountId"].get<std::string>()<<std::endl;
                    }
                    if(json["data"].contains("username")&&json["data"]["username"].is_string()){
                        std::cout<<"Username: "<<json["data"]["username"].get<std::string>()<<std::endl;
                    }
                    if(json["data"].contains("nickname")&&json["data"]["nickname"].is_string()){
                        std::cout<<"Nickname: "<<json["data"]["nickname"].get<std::string>()<<std::endl;
                    }
                    if(json["data"].contains("avatarUrl")&&json["data"]["avatarUrl"].is_string()){
                        std::cout<<"AvatarUrl: "<<json["data"]["avatarUrl"].get<std::string>()<<std::endl;
                    }
                    if(json["data"].contains("signature")&&json["data"]["signature"].is_string()){
                        std::cout<<"Signature: "<<json["data"]["signature"].get<std::string>()<<std::endl;
                    }
                }
                break;
            }
            case im::MsgType::LIST_FRIENDS_RESP:{
                std::cout<<"Friends list: "<<std::endl;
                for (const auto& friendInfo : json["data"]["friends"]) {
                    std::string accountId = friendInfo.value("accountId", "");
                    std::string nickname = friendInfo.value("nickname", "");
                    std::string username = friendInfo.value("username", "");

                    std::string display = !nickname.empty() ? nickname : username;
                    std::cout << accountId;
                    if (!display.empty()) {
                        std::cout << " (" << display << ")";
                    }
                    std::cout << std::endl;
                }
                break;
            }
            case im::MsgType::SEND_FRIEND_REQUEST_RESP:{
                std::cout<<"SEND_FRIEND_REQUEST_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            }
            case im::MsgType::LIST_FRIEND_REQUEST_RESP:{
                std::cout<<"Friend requests: "<<std::endl;
                for (const auto& request : json["data"]["requests"]) {
                    uint64_t requestId = request.value("requestId", 0);
                    std::string fromAccountId = request.value("accountId", "");
                    std::string fromNickname = request.value("nickname", "");
                    std::string fromUsername = request.value("username", "");
                    std::cout << "Request ID: " << requestId << std::endl;
                    std::cout << "From Account ID: " << fromAccountId << std::endl;
                    std::cout << "From Nickname: " << fromNickname << std::endl;
                    std::cout << "From Username: " << fromUsername << std::endl;
                }
                break;
            }
            case im::MsgType::ACCEPT_FRIEND_REQUEST_RESP:{
                std::cout<<"ACCEPT_FRIEND_REQUEST_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            }
            case im::MsgType::REJECT_FRIEND_REQUEST_RESP:{
                std::cout<<"REJECT_FRIEND_REQUEST_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            }
            case im::MsgType::REMOVE_FRIEND_RESP:{
                std::cout<<"REMOVE_FRIEND_RESP: "<<(json["ok"].get<bool>()?"success":"failed")<<" msg: "<<json["msg"].get<std::string>()<<std::endl;
                break;
            }
            case im::MsgType::FRIEND_EVENT_PUSH:{
                //根据data.event分类打印
                std::string event=json["data"]["event"].get<std::string>();
                if(event=="friendRequestReceived"){
                    std::cout<<"[FRIEND REQUEST] "<<json["data"]["requesterAccountId"].get<std::string>()<<" ("<<json["data"]["username"].get<std::string>()<<") sent you a friend request. Request ID: "<<json["data"]["requestId"].get<uint64_t>()<<std::endl;
                }
                else if(event=="friendRequestAccepted"){
                    std::cout<<"[FRIEND EVENT] "<<json["data"]["accountId"].get<std::string>()<<" ("<<json["data"]["username"].get<std::string>()<<") accepted your friend request."<<std::endl;
                }
                else if(event=="friendRequestRejected"){
                    std::cout<<"[FRIEND EVENT] "<<json["data"]["accountId"].get<std::string>()<<" ("<<json["data"]["username"].get<std::string>()<<") rejected your friend request."<<std::endl;
                }
                else if(event=="friendRemoved"){
                    std::cout<<"[FRIEND EVENT] "<<json["data"]["accountId"].get<std::string>()<<" ("<<json["data"]["username"].get<std::string>()<<") removed you from friends."<<std::endl;
                }
                break;
            }
            case im::MsgType::DM_HISTORY_RESP: {
                std::string mode = json["data"].value("mode", "latest");
                std::cout << "DM history (" << mode << "):" << std::endl;
                for (const auto& msg : json["data"]["messages"]) {
                    std::cout << "[] DM"
                  << msg["fromUsername"].get<std::string>()
                  << " (" << msg["fromAccountId"].get<std::string>() << "): "
                  << msg["content"].get<std::string>()
                  << " (msgId: "
                  << msg["msgId"].get<uint64_t>()
                  << ")"
                  << std::endl;
    }

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

struct BatchRegisterOptions {
    std::string host{"127.0.0.1"};
    int port{8080};
    int count{0};
    int startIndex{1};
    std::string usernamePrefix;
    std::string password;
    std::string groupId;
    std::string applicationMessage{"load test account application"};
    std::string outputPath{"tools/load_test_accounts.json"};
    int intervalMs{6500};
    int timeoutMs{5000};
    int maxRateLimitRetries{3};
    std::string reviewerAccount;
    std::string reviewerPassword;
    int approvalIntervalMs{20};
    bool resume{false};
};

static void printBatchRegisterUsage(const char* program) {
    std::cout
        << "Usage:\n"
        << "  " << program << " --batch-register [options]\n\n"
        << "Required options:\n"
        << "  --count N                    Number of accounts to register\n"
        << "  --username-prefix PREFIX     Generated username prefix\n"
        << "  --password PASSWORD          Shared password for generated accounts\n\n"
        << "Optional options:\n"
        << "  --host HOST                  Default 127.0.0.1\n"
        << "  --port PORT                  Default 8080\n"
        << "  --start-index N              First generated account index, default 1\n"
        << "  --group-id ID                Apply generated accounts to this group\n"
        << "  --output PATH                Default tools/load_test_accounts.json\n"
        << "  --resume                     Continue from an existing output file\n"
        << "  --interval-ms N              Delay between registrations, default 6500\n"
        << "  --timeout-ms N               Per-request timeout, default 5000\n"
        << "  --max-rate-limit-retries N   Default 3\n"
        << "  --application-message TEXT   Join application message\n"
        << "  --reviewer-account ID        Group owner/admin account; enables auto approval\n"
        << "  --reviewer-password PASSWORD Group owner/admin password\n"
        << "  --approval-interval-ms N      Delay between approvals, default 20\n\n"
        << "Environment fallbacks:\n"
        << "  PROJECT_CHAT_BENCH_PASSWORD\n"
        << "  PROJECT_CHAT_REVIEWER_ACCOUNT\n"
        << "  PROJECT_CHAT_REVIEWER_PASSWORD\n";
}

static bool parsePositiveInt(const std::string& value, int& output, bool allowZero=false) {
    try {
        size_t consumed=0;
        long parsed=std::stol(value,&consumed);
        if(consumed!=value.size()||parsed>(std::numeric_limits<int>::max())||
           (allowZero ? parsed<0 : parsed<=0)){
            return false;
        }
        output=static_cast<int>(parsed);
        return true;
    }
    catch(const std::exception&){
        return false;
    }
}

static std::optional<BatchRegisterOptions> parseBatchRegisterOptions(int argc,char** argv) {
    BatchRegisterOptions options;
    for(int index=2;index<argc;++index){
        std::string option=argv[index];
        if(option=="--help"){
            printBatchRegisterUsage(argv[0]);
            return std::nullopt;
        }
        if(option=="--resume"){
            options.resume=true;
            continue;
        }
        if(index+1>=argc){
            std::cerr<<"missing value for "<<option<<std::endl;
            return std::nullopt;
        }
        std::string value=argv[++index];
        if(option=="--host"){
            options.host=std::move(value);
        }
        else if(option=="--port"){
            if(!parsePositiveInt(value,options.port)||options.port>65535){
                std::cerr<<"invalid --port\n";
                return std::nullopt;
            }
        }
        else if(option=="--count"){
            if(!parsePositiveInt(value,options.count)){
                std::cerr<<"invalid --count\n";
                return std::nullopt;
            }
        }
        else if(option=="--start-index"){
            if(!parsePositiveInt(value,options.startIndex)){
                std::cerr<<"invalid --start-index\n";
                return std::nullopt;
            }
        }
        else if(option=="--username-prefix"){
            options.usernamePrefix=std::move(value);
        }
        else if(option=="--password"){
            options.password=std::move(value);
        }
        else if(option=="--group-id"){
            options.groupId=std::move(value);
        }
        else if(option=="--output"){
            options.outputPath=std::move(value);
        }
        else if(option=="--interval-ms"){
            if(!parsePositiveInt(value,options.intervalMs,true)){
                std::cerr<<"invalid --interval-ms\n";
                return std::nullopt;
            }
        }
        else if(option=="--timeout-ms"){
            if(!parsePositiveInt(value,options.timeoutMs)){
                std::cerr<<"invalid --timeout-ms\n";
                return std::nullopt;
            }
        }
        else if(option=="--max-rate-limit-retries"){
            if(!parsePositiveInt(value,options.maxRateLimitRetries,true)){
                std::cerr<<"invalid --max-rate-limit-retries\n";
                return std::nullopt;
            }
        }
        else if(option=="--application-message"){
            options.applicationMessage=std::move(value);
        }
        else if(option=="--reviewer-account"){
            options.reviewerAccount=std::move(value);
        }
        else if(option=="--reviewer-password"){
            options.reviewerPassword=std::move(value);
        }
        else if(option=="--approval-interval-ms"){
            if(!parsePositiveInt(value,options.approvalIntervalMs,true)){
                std::cerr<<"invalid --approval-interval-ms\n";
                return std::nullopt;
            }
        }
        else{
            std::cerr<<"unknown batch option: "<<option<<std::endl;
            return std::nullopt;
        }
    }

    if(options.password.empty()){
        if(const char* password=std::getenv("PROJECT_CHAT_BENCH_PASSWORD")){
            options.password=password;
        }
    }
    if(options.reviewerAccount.empty()){
        if(const char* account=std::getenv("PROJECT_CHAT_REVIEWER_ACCOUNT")){
            options.reviewerAccount=account;
        }
    }
    if(options.reviewerPassword.empty()){
        if(const char* password=std::getenv("PROJECT_CHAT_REVIEWER_PASSWORD")){
            options.reviewerPassword=password;
        }
    }
    if(options.count<=0||options.usernamePrefix.empty()||options.password.empty()||
       options.outputPath.empty()){
        std::cerr<<"--count, --username-prefix and --password are required\n";
        printBatchRegisterUsage(argv[0]);
        return std::nullopt;
    }
    if(options.reviewerAccount.empty()!=options.reviewerPassword.empty()){
        std::cerr<<"--reviewer-account and --reviewer-password must be provided together\n";
        return std::nullopt;
    }
    if(options.groupId.empty()&&!options.reviewerAccount.empty()){
        std::cerr<<"reviewer credentials require --group-id\n";
        return std::nullopt;
    }
    if(options.intervalMs<6000){
        std::cerr<<"warning: interval below 6000 ms may trigger the default 10 registrations/minute IP limit\n";
    }
    return options;
}

struct BatchPrepareGroupsOptions {
    std::string host{"127.0.0.1"};
    int port{8080};
    std::string accountsPath{"tools/load_test_accounts_100.json"};
    std::string reviewerAccount;
    std::string reviewerPassword;
    std::string groupNamePrefix{"load_test"};
    std::string outputDirectory{"tools/load_test_groups"};
    std::vector<int> groupSizes{20,50,100};
    std::string applicationMessage{"load test group application"};
    int timeoutMs{10000};
    int requestIntervalMs{20};
};

static void printBatchPrepareGroupsUsage(const char* program){
    std::cout
        <<"Usage:\n"
        <<"  "<<program<<" --batch-prepare-groups [options]\n\n"
        <<"Required options:\n"
        <<"  --reviewer-account ID        Group owner account\n"
        <<"  --reviewer-password PASSWORD Group owner password\n\n"
        <<"Optional options:\n"
        <<"  --host HOST                  Default 127.0.0.1\n"
        <<"  --port PORT                  Default 8080\n"
        <<"  --accounts-file PATH         Existing reusable accounts file\n"
        <<"  --group-sizes LIST           Total members including owner, default 20,50,100\n"
        <<"  --group-name-prefix PREFIX   Default load_test\n"
        <<"  --output-dir PATH            Default tools/load_test_groups\n"
        <<"  --application-message TEXT   Join application message\n"
        <<"  --timeout-ms N               Per-request timeout, default 10000\n"
        <<"  --request-interval-ms N      Delay between requests, default 20\n\n"
        <<"Reviewer credentials may also use PROJECT_CHAT_REVIEWER_ACCOUNT and "
        <<"PROJECT_CHAT_REVIEWER_PASSWORD.\n";
}

static bool parseGroupSizes(const std::string& value,std::vector<int>& sizes){
    std::vector<int> parsedSizes;
    std::stringstream stream(value);
    std::string item;
    while(std::getline(stream,item,',')){
        int size=0;
        if(!parsePositiveInt(item,size)||size<2){
            return false;
        }
        parsedSizes.push_back(size);
    }
    if(parsedSizes.empty()){
        return false;
    }
    std::sort(parsedSizes.begin(),parsedSizes.end());
    parsedSizes.erase(std::unique(parsedSizes.begin(),parsedSizes.end()),parsedSizes.end());
    sizes=std::move(parsedSizes);
    return true;
}

static std::optional<BatchPrepareGroupsOptions> parseBatchPrepareGroupsOptions(int argc,char** argv){
    BatchPrepareGroupsOptions options;
    for(int index=2;index<argc;++index){
        std::string option=argv[index];
        if(option=="--help"){
            printBatchPrepareGroupsUsage(argv[0]);
            return std::nullopt;
        }
        if(index+1>=argc){
            std::cerr<<"missing value for "<<option<<std::endl;
            return std::nullopt;
        }
        std::string value=argv[++index];
        if(option=="--host"){
            options.host=std::move(value);
        }
        else if(option=="--port"){
            if(!parsePositiveInt(value,options.port)||options.port>65535){
                std::cerr<<"invalid --port\n";
                return std::nullopt;
            }
        }
        else if(option=="--accounts-file"){
            options.accountsPath=std::move(value);
        }
        else if(option=="--reviewer-account"){
            options.reviewerAccount=std::move(value);
        }
        else if(option=="--reviewer-password"){
            options.reviewerPassword=std::move(value);
        }
        else if(option=="--group-sizes"){
            if(!parseGroupSizes(value,options.groupSizes)){
                std::cerr<<"invalid --group-sizes\n";
                return std::nullopt;
            }
        }
        else if(option=="--group-name-prefix"){
            options.groupNamePrefix=std::move(value);
        }
        else if(option=="--output-dir"){
            options.outputDirectory=std::move(value);
        }
        else if(option=="--application-message"){
            options.applicationMessage=std::move(value);
        }
        else if(option=="--timeout-ms"){
            if(!parsePositiveInt(value,options.timeoutMs)){
                std::cerr<<"invalid --timeout-ms\n";
                return std::nullopt;
            }
        }
        else if(option=="--request-interval-ms"){
            if(!parsePositiveInt(value,options.requestIntervalMs,true)){
                std::cerr<<"invalid --request-interval-ms\n";
                return std::nullopt;
            }
        }
        else{
            std::cerr<<"unknown batch prepare option: "<<option<<std::endl;
            return std::nullopt;
        }
    }

    if(options.reviewerAccount.empty()){
        if(const char* account=std::getenv("PROJECT_CHAT_REVIEWER_ACCOUNT")){
            options.reviewerAccount=account;
        }
    }
    if(options.reviewerPassword.empty()){
        if(const char* password=std::getenv("PROJECT_CHAT_REVIEWER_PASSWORD")){
            options.reviewerPassword=password;
        }
    }
    if(options.reviewerAccount.empty()||options.reviewerPassword.empty()||
       options.accountsPath.empty()||options.outputDirectory.empty()||
       options.groupNamePrefix.empty()){
        std::cerr<<"reviewer credentials, accounts file, group name prefix and output directory are required\n";
        printBatchPrepareGroupsUsage(argv[0]);
        return std::nullopt;
    }
    return options;
}

class BatchConnection {
public:
    BatchConnection()=default;
    ~BatchConnection(){
        close();
    }
    BatchConnection(const BatchConnection&)=delete;
    BatchConnection& operator=(const BatchConnection&)=delete;

    bool connectTo(const std::string& host,int port){
        close();
        fd_=::socket(AF_INET,SOCK_STREAM,0);
        if(fd_<0){
            lastError_="socket failed: "+std::string(std::strerror(errno));
            return false;
        }
        sockaddr_in address{};
        address.sin_family=AF_INET;
        address.sin_port=htons(static_cast<uint16_t>(port));
        if(::inet_pton(AF_INET,host.c_str(),&address.sin_addr)!=1){
            lastError_="invalid IPv4 address: "+host;
            close();
            return false;
        }
        if(::connect(fd_,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0){
            lastError_="connect failed: "+std::string(std::strerror(errno));
            close();
            return false;
        }
        return true;
    }

    std::optional<nlohmann::json> request(const std::string& payload,int timeoutMs){
        nlohmann::json requestDocument;
        try{
            requestDocument=nlohmann::json::parse(payload);
        }
        catch(const std::exception& e){
            lastError_="invalid request JSON: "+std::string(e.what());
            return std::nullopt;
        }
        const uint64_t requestId=requestDocument.value("req_id",uint64_t{0});
        if(fd_<0||!sendAllFramed(fd_,payload)){
            lastError_="failed to send framed request";
            return std::nullopt;
        }

        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeoutMs);
        while(std::chrono::steady_clock::now()<deadline){
            while(input_.readableBytes()>=4){
                const uint32_t length=input_.peekUInt32();
                if(length==0||length>kMaxFrameLen){
                    lastError_="invalid response frame length: "+std::to_string(length);
                    return std::nullopt;
                }
                if(input_.readableBytes()<4+length){
                    break;
                }
                input_.retrieveUInt32();
                std::string frame=input_.retrieveAsString(length);
                if(frame=="PING"){
                    if(!sendAllFramed(fd_,"PONG")){
                        lastError_="failed to send heartbeat PONG";
                        return std::nullopt;
                    }
                    continue;
                }
                try{
                    auto response=nlohmann::json::parse(frame);
                    if(response.value("req_id",uint64_t{0})==requestId){
                        return response;
                    }
                }
                catch(const std::exception&){
                    continue;
                }
            }

            const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline-std::chrono::steady_clock::now()).count();
            if(remaining<=0){
                break;
            }
            pollfd descriptor{fd_,POLLIN,0};
            int pollResult;
            do{
                pollResult=::poll(&descriptor,1,static_cast<int>(remaining));
            }while(pollResult<0&&errno==EINTR);
            if(pollResult==0){
                break;
            }
            if(pollResult<0||(descriptor.revents&(POLLERR|POLLHUP|POLLNVAL))!=0){
                lastError_="connection closed while waiting for response";
                return std::nullopt;
            }
            char buffer[4096];
            ssize_t received;
            do{
                received=::recv(fd_,buffer,sizeof(buffer),0);
            }while(received<0&&errno==EINTR);
            if(received<=0){
                lastError_=received==0?"server closed connection":"recv failed: "+std::string(std::strerror(errno));
                return std::nullopt;
            }
            input_.append(buffer,static_cast<size_t>(received));
        }
        lastError_="request timed out after "+std::to_string(timeoutMs)+" ms";
        return std::nullopt;
    }

    const std::string& lastError()const{
        return lastError_;
    }

private:
    void close(){
        if(fd_>=0){
            ::shutdown(fd_,SHUT_RDWR);
            ::close(fd_);
            fd_=-1;
        }
        if(input_.readableBytes()>0){
            input_.retrieveAll();
        }
    }

    int fd_{-1};
    Buffer input_;
    std::string lastError_;
};

static std::string responseMessage(const nlohmann::json& response){
    if(response.contains("msg")&&response["msg"].is_string()){
        return response["msg"].get<std::string>();
    }
    return "unknown response error";
}

static bool isRateLimited(const nlohmann::json& response){
    return !response.value("ok",false)&&
           response.value("code",-1)==static_cast<int>(im::ErrorCode::RATE_LIMITED);
}

static int retryAfterMs(const nlohmann::json& response,int fallbackMs){
    if(response.contains("data")&&response["data"].is_object()){
        return response["data"].value("retryAfterMs",fallbackMs);
    }
    return fallbackMs;
}

static bool writeBatchRegisterResult(const BatchRegisterOptions& options,
                                     const nlohmann::json& accounts,
                                     const nlohmann::json& registeredNotApplied,
                                     const nlohmann::json& failures,
                                     const nlohmann::json& approvalFailures){
    size_t groupReady=accounts.size();
    if(!options.groupId.empty()){
        groupReady=0;
        for(const auto& account:accounts){
            const std::string status=account.value("applicationStatus","");
            if(status=="approved"||status=="already_approved"||status=="already_in"){
                ++groupReady;
            }
        }
    }
    nlohmann::json document{
        {"mode",options.groupId.empty()?"accounts_only":"group_application"},
        {"groupId",options.groupId},
        {"reviewerAccountId",options.reviewerAccount},
        {"accounts",accounts},
        {"registeredNotApplied",registeredNotApplied},
        {"failures",failures},
        {"approvalFailures",approvalFailures},
        {"summary",{
            {"requested",options.count},
            {"startIndex",options.startIndex},
            {"registered",accounts.size()+registeredNotApplied.size()},
            {"applicationAccepted",accounts.size()},
            {"groupReady",groupReady},
            {"registeredNotApplied",registeredNotApplied.size()},
            {"registrationFailed",failures.size()},
            {"approvalFailed",approvalFailures.size()}
        }}
    };
    std::filesystem::path outputPath(options.outputPath);
    std::error_code error;
    if(outputPath.has_parent_path()){
        std::filesystem::create_directories(outputPath.parent_path(),error);
        if(error){
            std::cerr<<"failed to create output directory: "<<error.message()<<std::endl;
            return false;
        }
    }
    std::ofstream output(outputPath,std::ios::trunc);
    if(!output){
        std::cerr<<"failed to open output file: "<<options.outputPath<<std::endl;
        return false;
    }
    output<<document.dump(2)<<'\n';
    return output.good();
}

static bool loadBatchRegisterResume(const BatchRegisterOptions& options,
                                    nlohmann::json& accounts,
                                    nlohmann::json& registeredNotApplied){
    if(!options.resume||!std::filesystem::exists(options.outputPath)){
        return true;
    }
    std::ifstream input(options.outputPath);
    if(!input){
        std::cerr<<"failed to open resume file: "<<options.outputPath<<std::endl;
        return false;
    }
    try{
        nlohmann::json document;
        input>>document;
        if(!document.is_object()||!document.contains("accounts")||
           !document["accounts"].is_array()){
            std::cerr<<"resume file does not contain an accounts array\n";
            return false;
        }
        accounts=document["accounts"];
        if(document.contains("registeredNotApplied")&&
           document["registeredNotApplied"].is_array()){
            registeredNotApplied=document["registeredNotApplied"];
        }
    }
    catch(const std::exception& exception){
        std::cerr<<"failed to parse resume file: "<<exception.what()<<std::endl;
        return false;
    }
    std::cout<<"Resuming from "<<options.outputPath
             <<", existing accounts="<<accounts.size()
             <<", registered_not_applied="<<registeredNotApplied.size()<<std::endl;
    return true;
}

static bool approveBatchApplications(const BatchRegisterOptions& options,
                                     nlohmann::json& accounts,
                                     nlohmann::json& approvalFailures){
    if(options.groupId.empty()||options.reviewerAccount.empty()){
        return true;
    }

    BatchConnection connection;
    if(!connection.connectTo(options.host,options.port)){
        approvalFailures.push_back({{"step","reviewer_connect"},{"error",connection.lastError()}});
        return false;
    }

    ClientState reviewerState;
    reviewerState.accountId=options.reviewerAccount;
    CommandBuilder builder;
    auto loginResponse=connection.request(
        builder.buildLoginReq(reviewerState,options.reviewerAccount,options.reviewerPassword),
        options.timeoutMs);
    if(!loginResponse||!loginResponse->value("ok",false)){
        approvalFailures.push_back({{"step","reviewer_login"},
                                    {"accountId",options.reviewerAccount},
                                    {"error",loginResponse?responseMessage(*loginResponse):connection.lastError()}});
        return false;
    }

    size_t reviewed=0;
    for(auto& credential:accounts){
        const std::string status=credential.value("applicationStatus","");
        if(status=="already_in"||status=="approved"||status=="already_approved"){
            continue;
        }
        const std::string applicantAccountId=credential.value("accountId","");
        if(applicantAccountId.empty()){
            approvalFailures.push_back({{"step","approve_group"},{"error","missing applicant accountId"}});
            credential["applicationStatus"]="approval_failed";
            continue;
        }

        auto reviewResponse=connection.request(
            builder.buildReviewGroupJoinReq(reviewerState,options.groupId,applicantAccountId,true),
            options.timeoutMs);
        if(!reviewResponse||!reviewResponse->value("ok",false)){
            nlohmann::json failure{{"step","approve_group"},{"accountId",applicantAccountId},
                                   {"username",credential.value("username","")},
                                   {"error",reviewResponse?responseMessage(*reviewResponse):connection.lastError()}};
            if(reviewResponse){
                failure["code"]=reviewResponse->value("code",-1);
            }
            approvalFailures.push_back(std::move(failure));
            credential["applicationStatus"]="approval_failed";
        }
        else{
            if(!reviewResponse->contains("data")||!(*reviewResponse)["data"].is_object()){
                credential["applicationStatus"]="approval_failed";
                approvalFailures.push_back({{"step","approve_group"},
                                            {"accountId",applicantAccountId},
                                            {"username",credential.value("username","")},
                                            {"error","review response missing data object"}});
                continue;
            }
            const auto& data=(*reviewResponse)["data"];
            if(data.value("approved",false)){
                credential["applicationStatus"]=data.value("alreadyHandled",false)
                    ?"already_approved":"approved";
            }
            else if(data.value("alreadyHandled",false)){
                credential["applicationStatus"]="already_in";
            }
            else{
                credential["applicationStatus"]="approval_failed";
                approvalFailures.push_back({{"step","approve_group"},
                                            {"accountId",applicantAccountId},
                                            {"username",credential.value("username","")},
                                            {"error","review response did not confirm approval"}});
            }
        }
        ++reviewed;
        std::cout<<"[approval "<<reviewed<<'/'<<accounts.size()<<"] "
                 <<applicantAccountId<<" status="<<credential.value("applicationStatus","")<<std::endl;
        if(options.approvalIntervalMs>0){
            std::this_thread::sleep_for(std::chrono::milliseconds(options.approvalIntervalMs));
        }
    }
    return approvalFailures.empty();
}

static int runBatchRegister(int argc,char** argv){
    auto optionsResult=parseBatchRegisterOptions(argc,argv);
    if(!optionsResult){
        return argc>=3&&std::string(argv[2])=="--help"?0:2;
    }
    const BatchRegisterOptions options=std::move(*optionsResult);
    nlohmann::json accounts=nlohmann::json::array();
    nlohmann::json registeredNotApplied=nlohmann::json::array();
    nlohmann::json failures=nlohmann::json::array();
    nlohmann::json approvalFailures=nlohmann::json::array();
    if(!loadBatchRegisterResume(options,accounts,registeredNotApplied)){
        return 3;
    }
    std::unordered_set<std::string> completedUsernames;
    for(const auto* list:{&accounts,&registeredNotApplied}){
        for(const auto& credential:*list){
            if(credential.is_object()&&credential.contains("username")&&
               credential["username"].is_string()){
                completedUsernames.insert(credential["username"].get<std::string>());
            }
        }
    }
    const int lastIndex=options.startIndex+options.count-1;
    const int width=std::max(3,static_cast<int>(std::to_string(lastIndex).size()));

    std::cout<<"Batch registration started: count="<<options.count
             <<" startIndex="<<options.startIndex
             <<" mode="<<(options.groupId.empty()?"accounts_only":"group_application")
             <<" groupId="<<options.groupId
             <<" intervalMs="<<options.intervalMs<<std::endl;
    std::cout<<"Credentials contain plaintext test passwords; protect the output file.\n";

    for(int offset=0;offset<options.count;++offset){
        const int index=options.startIndex+offset;
        const int progress=offset+1;
        std::ostringstream usernameStream;
        usernameStream<<options.usernamePrefix<<std::setw(width)<<std::setfill('0')<<index;
        const std::string username=usernameStream.str();
        if(completedUsernames.contains(username)){
            std::cout<<"["<<progress<<'/'<<options.count<<"] "<<username
                     <<" already persisted, skipping\n";
            continue;
        }
        bool finished=false;

        for(int attempt=0;attempt<=options.maxRateLimitRetries&&!finished;++attempt){
            BatchConnection connection;
            if(!connection.connectTo(options.host,options.port)){
                failures.push_back({{"username",username},{"step","connect"},{"error",connection.lastError()}});
                std::cerr<<"["<<progress<<'/'<<options.count<<"] "<<username<<" connect failed: "
                         <<connection.lastError()<<std::endl;
                finished=true;
                break;
            }

            ClientState state;
            state.username=username;
            CommandBuilder builder;
            auto registerResponse=connection.request(
                builder.buildRegisterReq(state,username,options.password),options.timeoutMs);
            if(!registerResponse){
                failures.push_back({{"username",username},{"step","register"},{"error",connection.lastError()}});
                std::cerr<<"["<<progress<<'/'<<options.count<<"] "<<username<<" register failed: "
                         <<connection.lastError()<<std::endl;
                finished=true;
                break;
            }
            if(isRateLimited(*registerResponse)&&attempt<options.maxRateLimitRetries){
                const int waitMs=retryAfterMs(*registerResponse,options.intervalMs)+200;
                std::cerr<<"["<<progress<<'/'<<options.count<<"] registration rate limited; retrying in "
                         <<waitMs<<" ms"<<std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
                continue;
            }
            if(!registerResponse->value("ok",false)){
                failures.push_back({{"username",username},{"step","register"},
                                    {"code",registerResponse->value("code",-1)},
                                    {"error",responseMessage(*registerResponse)}});
                std::cerr<<"["<<progress<<'/'<<options.count<<"] "<<username<<" register rejected: "
                         <<responseMessage(*registerResponse)<<std::endl;
                finished=true;
                break;
            }
            if(!registerResponse->contains("data")||
               !(*registerResponse)["data"].contains("accountId")||
               !(*registerResponse)["data"]["accountId"].is_string()){
                failures.push_back({{"username",username},{"step","register"},{"error","missing accountId"}});
                std::cerr<<"["<<progress<<'/'<<options.count<<"] register response missing accountId\n";
                finished=true;
                break;
            }

            const std::string accountId=(*registerResponse)["data"]["accountId"].get<std::string>();
            state.accountId=accountId;
            nlohmann::json credential{{"accountId",accountId},{"password",options.password},
                                      {"username",username}};
            if(options.groupId.empty()){
                credential["applicationStatus"]="not_requested";
                accounts.push_back(std::move(credential));
                completedUsernames.insert(username);
                std::cout<<"["<<progress<<'/'<<options.count<<"] registered "<<username
                         <<" accountId="<<accountId<<std::endl;
                finished=true;
                break;
            }
            credential["groupId"]=options.groupId;
            auto loginResponse=connection.request(
                builder.buildLoginReq(state,accountId,options.password),options.timeoutMs);
            if(!loginResponse||!loginResponse->value("ok",false)){
                credential["step"]="login";
                credential["error"]=loginResponse?responseMessage(*loginResponse):connection.lastError();
                registeredNotApplied.push_back(std::move(credential));
                std::cerr<<"["<<progress<<'/'<<options.count<<"] "<<username
                         <<" registered but login failed\n";
                finished=true;
                break;
            }

            auto applyResponse=connection.request(
                builder.buildApplyGroupJoinReq(state,options.groupId,options.applicationMessage),
                options.timeoutMs);
            if(!applyResponse||!applyResponse->value("ok",false)){
                credential["step"]="apply_group";
                credential["error"]=applyResponse?responseMessage(*applyResponse):connection.lastError();
                if(applyResponse){
                    credential["code"]=applyResponse->value("code",-1);
                }
                registeredNotApplied.push_back(std::move(credential));
                std::cerr<<"["<<progress<<'/'<<options.count<<"] "<<username
                         <<" registered but group application failed\n";
                finished=true;
                break;
            }

            const auto& data=(*applyResponse)["data"];
            std::string applicationStatus="accepted";
            if(data.value("alreadyIn",false)){
                applicationStatus="already_in";
            }
            else if(data.value("alreadyPending",false)){
                applicationStatus="already_pending";
            }
            else if(data.value("submitted",false)){
                applicationStatus="submitted";
            }
            credential["applicationStatus"]=applicationStatus;
            accounts.push_back(std::move(credential));
            completedUsernames.insert(username);
            std::cout<<"["<<progress<<'/'<<options.count<<"] registered "<<username
                     <<" accountId="<<accountId<<" application="<<applicationStatus<<std::endl;
            finished=true;
        }

        if(!finished){
            failures.push_back({{"username",username},{"step","register"},
                                {"error","rate-limit retry count exhausted"}});
        }
        if(!writeBatchRegisterResult(options,accounts,registeredNotApplied,failures,approvalFailures)){
            return 3;
        }
        if(progress<options.count&&options.intervalMs>0){
            std::this_thread::sleep_for(std::chrono::milliseconds(options.intervalMs));
        }
    }

    const bool approvalsOk=approveBatchApplications(options,accounts,approvalFailures);
    if(!writeBatchRegisterResult(options,accounts,registeredNotApplied,failures,approvalFailures)){
        return 3;
    }

    std::cout<<"Batch registration finished: applications="<<accounts.size()
              <<" registered_not_applied="<<registeredNotApplied.size()
              <<" failures="<<failures.size()
              <<" approval_failures="<<approvalFailures.size()<<std::endl;
    std::cout<<"Result file: "<<options.outputPath<<std::endl;
    if(!options.groupId.empty()&&options.reviewerAccount.empty()){
        std::cout<<"Approve pending applications before using the accounts for load testing.\n";
    }
    return failures.empty()&&registeredNotApplied.empty()&&approvalsOk?0:4;
}

struct ReusableCredential {
    std::string accountId;
    std::string password;
    std::string username;
};

struct PreparedLoadGroup {
    int targetMemberCount{0};
    std::string groupName;
    std::string groupId;
    std::filesystem::path accountsPath;
    nlohmann::json accounts{nlohmann::json::array()};
    nlohmann::json failures{nlohmann::json::array()};
    std::unordered_set<std::string> readyAccountIds;
    size_t applicationsSubmitted{0};
    size_t approvalsSucceeded{0};
    size_t alreadyMembers{0};
};

struct PendingGroupApproval {
    size_t groupIndex{0};
    size_t credentialIndex{0};
};

static std::optional<std::vector<ReusableCredential>> loadReusableCredentials(
    const BatchPrepareGroupsOptions& options){
    std::ifstream input(options.accountsPath);
    if(!input){
        std::cerr<<"cannot open reusable accounts file: "<<options.accountsPath<<std::endl;
        return std::nullopt;
    }
    nlohmann::json document;
    try{
        input>>document;
    }
    catch(const std::exception& e){
        std::cerr<<"invalid reusable accounts JSON: "<<e.what()<<std::endl;
        return std::nullopt;
    }
    const nlohmann::json* accounts=&document;
    if(document.is_object()&&document.contains("accounts")){
        accounts=&document["accounts"];
    }
    if(!accounts->is_array()||accounts->empty()){
        std::cerr<<"reusable accounts file must contain a non-empty accounts array\n";
        return std::nullopt;
    }

    std::vector<ReusableCredential> credentials;
    std::unordered_set<std::string> uniqueAccountIds;
    for(const auto& entry:*accounts){
        if(!entry.is_object()||!entry.contains("accountId")||!entry["accountId"].is_string()||
           !entry.contains("password")||!entry["password"].is_string()){
            std::cerr<<"reusable account requires string accountId and password\n";
            return std::nullopt;
        }
        ReusableCredential credential{
            .accountId=entry["accountId"].get<std::string>(),
            .password=entry["password"].get<std::string>(),
            .username=entry.value("username","")
        };
        if(credential.accountId.empty()||credential.password.empty()){
            std::cerr<<"reusable accountId and password cannot be empty\n";
            return std::nullopt;
        }
        if(credential.accountId==options.reviewerAccount){
            continue;
        }
        if(uniqueAccountIds.insert(credential.accountId).second){
            credentials.push_back(std::move(credential));
        }
    }
    return credentials;
}

static bool connectAndLoginBatchAccount(BatchConnection& connection,
                                        const BatchPrepareGroupsOptions& options,
                                        const std::string& accountId,
                                        const std::string& password,
                                        ClientState& state,
                                        std::string& error){
    if(!connection.connectTo(options.host,options.port)){
        error=connection.lastError();
        return false;
    }
    state=ClientState{};
    state.accountId=accountId;
    CommandBuilder builder;
    auto response=connection.request(
        builder.buildLoginReq(state,accountId,password),options.timeoutMs);
    if(!response||!response->value("ok",false)){
        error=response?responseMessage(*response):connection.lastError();
        return false;
    }
    if(response->contains("data")&&(*response)["data"].is_object()){
        state.username=(*response)["data"].value("username","");
    }
    return true;
}

static std::string makePreparedGroupName(const std::string& prefix,int memberCount,
                                         int64_t runSuffix){
    const std::string suffix="_"+std::to_string(memberCount)+"_"+std::to_string(runSuffix);
    constexpr size_t maxGroupNameLength=64;
    if(prefix.size()+suffix.size()<=maxGroupNameLength){
        return prefix+suffix;
    }
    return prefix.substr(0,maxGroupNameLength-suffix.size())+suffix;
}

static nlohmann::json makePreparedCredential(const ReusableCredential& credential,
                                             const PreparedLoadGroup& group,
                                             const std::string& role){
    return nlohmann::json{
        {"accountId",credential.accountId},
        {"password",credential.password},
        {"username",credential.username},
        {"groupId",group.groupId},
        {"role",role}
    };
}

static void addPreparedCredential(PreparedLoadGroup& group,
                                  const ReusableCredential& credential,
                                  const std::string& role){
    if(group.readyAccountIds.insert(credential.accountId).second){
        group.accounts.push_back(makePreparedCredential(credential,group,role));
    }
}

static bool writeJsonDocument(const std::filesystem::path& path,
                              const nlohmann::json& document){
    std::error_code error;
    if(path.has_parent_path()){
        std::filesystem::create_directories(path.parent_path(),error);
        if(error){
            std::cerr<<"failed to create output directory: "<<error.message()<<std::endl;
            return false;
        }
    }
    std::ofstream output(path,std::ios::trunc);
    if(!output){
        std::cerr<<"failed to open output file: "<<path.string()<<std::endl;
        return false;
    }
    output<<document.dump(2)<<'\n';
    return output.good();
}

static int runBatchPrepareGroups(int argc,char** argv){
    auto optionsResult=parseBatchPrepareGroupsOptions(argc,argv);
    if(!optionsResult){
        return argc>=3&&std::string(argv[2])=="--help"?0:2;
    }
    const BatchPrepareGroupsOptions options=std::move(*optionsResult);
    auto credentialsResult=loadReusableCredentials(options);
    if(!credentialsResult){
        return 3;
    }
    const auto& credentials=*credentialsResult;
    const int largestGroup=options.groupSizes.back();
    if(credentials.size()<static_cast<size_t>(largestGroup-1)){
        std::cerr<<"not enough reusable accounts: need "<<(largestGroup-1)
                 <<", available "<<credentials.size()<<std::endl;
        return 3;
    }

    BatchConnection reviewerConnection;
    ClientState reviewerState;
    std::string reviewerError;
    if(!connectAndLoginBatchAccount(reviewerConnection,options,options.reviewerAccount,
                                    options.reviewerPassword,reviewerState,reviewerError)){
        std::cerr<<"reviewer login failed: "<<reviewerError<<std::endl;
        return 4;
    }

    const auto runSuffix=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    CommandBuilder reviewerBuilder;
    std::vector<PreparedLoadGroup> groups;
    groups.reserve(options.groupSizes.size());
    for(int memberCount:options.groupSizes){
        PreparedLoadGroup group;
        group.targetMemberCount=memberCount;
        group.groupName=makePreparedGroupName(options.groupNamePrefix,memberCount,runSuffix);
        auto response=reviewerConnection.request(
            reviewerBuilder.buildCreateGroupReq(reviewerState,group.groupName),options.timeoutMs);
        if(!response||!response->value("ok",false)||!response->contains("data")||
           !(*response)["data"].is_object()||
           !(*response)["data"].contains("groupId")||
           !(*response)["data"]["groupId"].is_string()){
            std::cerr<<"create group "<<group.groupName<<" failed: "
                     <<(response?responseMessage(*response):reviewerConnection.lastError())<<std::endl;
            return 4;
        }
        group.groupId=(*response)["data"]["groupId"].get<std::string>();
        group.accountsPath=std::filesystem::path(options.outputDirectory)/
            ("load_test_accounts_"+std::to_string(memberCount)+".json");
        ReusableCredential reviewerCredential{
            .accountId=options.reviewerAccount,
            .password=options.reviewerPassword,
            .username=reviewerState.username
        };
        addPreparedCredential(group,reviewerCredential,"owner");
        std::cout<<"created group target="<<memberCount
                 <<" groupId="<<group.groupId
                 <<" name="<<group.groupName<<std::endl;
        groups.push_back(std::move(group));
    }

    std::vector<PendingGroupApproval> pendingApprovals;
    for(size_t credentialIndex=0;
        credentialIndex<static_cast<size_t>(largestGroup-1);++credentialIndex){
        std::vector<size_t> targetGroups;
        for(size_t groupIndex=0;groupIndex<groups.size();++groupIndex){
            if(credentialIndex<static_cast<size_t>(groups[groupIndex].targetMemberCount-1)){
                targetGroups.push_back(groupIndex);
            }
        }

        BatchConnection accountConnection;
        ClientState accountState;
        std::string loginError;
        const auto& credential=credentials[credentialIndex];
        if(!connectAndLoginBatchAccount(accountConnection,options,credential.accountId,
                                        credential.password,accountState,loginError)){
            for(size_t groupIndex:targetGroups){
                groups[groupIndex].failures.push_back({
                    {"step","applicant_login"},
                    {"accountId",credential.accountId},
                    {"error",loginError}
                });
            }
            std::cerr<<"applicant login failed accountId="<<credential.accountId
                     <<" error="<<loginError<<std::endl;
            continue;
        }

        CommandBuilder applicantBuilder;
        for(size_t groupIndex:targetGroups){
            auto& group=groups[groupIndex];
            auto response=accountConnection.request(
                applicantBuilder.buildApplyGroupJoinReq(accountState,group.groupId,
                                                        options.applicationMessage),
                options.timeoutMs);
            if(!response||!response->value("ok",false)||!response->contains("data")||
               !(*response)["data"].is_object()){
                group.failures.push_back({
                    {"step","apply_group"},
                    {"accountId",credential.accountId},
                    {"code",response?response->value("code",-1):-1},
                    {"error",response?responseMessage(*response):accountConnection.lastError()}
                });
                continue;
            }
            const auto& data=(*response)["data"];
            if(data.value("alreadyIn",false)){
                addPreparedCredential(group,credential,"member");
                ++group.alreadyMembers;
            }
            else if(data.value("submitted",false)||data.value("alreadyPending",false)){
                ++group.applicationsSubmitted;
                pendingApprovals.push_back({groupIndex,credentialIndex});
            }
            else{
                group.failures.push_back({
                    {"step","apply_group"},
                    {"accountId",credential.accountId},
                    {"error","application response did not confirm submission"}
                });
            }
            if(options.requestIntervalMs>0){
                std::this_thread::sleep_for(std::chrono::milliseconds(options.requestIntervalMs));
            }
        }
        std::cout<<"applications processed "<<(credentialIndex+1)<<'/'<<(largestGroup-1)
                 <<" accountId="<<credential.accountId<<std::endl;
        if((credentialIndex+1)%20==0){
            auto keepAliveResponse=reviewerConnection.request(
                reviewerBuilder.buildListGroupsReq(reviewerState),options.timeoutMs);
            if(!keepAliveResponse||!keepAliveResponse->value("ok",false)){
                reviewerError.clear();
                if(!connectAndLoginBatchAccount(reviewerConnection,options,
                                                options.reviewerAccount,
                                                options.reviewerPassword,
                                                reviewerState,reviewerError)){
                    std::cerr<<"reviewer keepalive reconnect failed: "
                             <<reviewerError<<std::endl;
                    return 4;
                }
                reviewerBuilder=CommandBuilder{};
            }
        }
    }

    for(size_t index=0;index<pendingApprovals.size();++index){
        const auto pending=pendingApprovals[index];
        auto& group=groups[pending.groupIndex];
        const auto& credential=credentials[pending.credentialIndex];
        auto response=reviewerConnection.request(
            reviewerBuilder.buildReviewGroupJoinReq(reviewerState,group.groupId,
                                                    credential.accountId,true),
            options.timeoutMs);
        bool approved=false;
        if(response&&response->value("ok",false)&&response->contains("data")&&
           (*response)["data"].is_object()){
            const auto& data=(*response)["data"];
            approved=data.value("approved",false);
        }
        if(approved){
            addPreparedCredential(group,credential,"member");
            ++group.approvalsSucceeded;
        }
        else{
            group.failures.push_back({
                {"step","approve_group"},
                {"accountId",credential.accountId},
                {"code",response?response->value("code",-1):-1},
                {"error",response?responseMessage(*response):reviewerConnection.lastError()}
            });
        }
        if(options.requestIntervalMs>0){
            std::this_thread::sleep_for(std::chrono::milliseconds(options.requestIntervalMs));
        }
        if((index+1)%20==0||index+1==pendingApprovals.size()){
            std::cout<<"approvals processed "<<(index+1)<<'/'<<pendingApprovals.size()<<std::endl;
        }
    }

    nlohmann::json manifestGroups=nlohmann::json::array();
    bool allReady=true;
    for(auto& group:groups){
        const bool ready=group.accounts.size()==static_cast<size_t>(group.targetMemberCount)&&
                         group.failures.empty();
        allReady=allReady&&ready;
        nlohmann::json document{
            {"groupId",group.groupId},
            {"groupName",group.groupName},
            {"targetMemberCount",group.targetMemberCount},
            {"accounts",group.accounts},
            {"failures",group.failures},
            {"summary",{
                {"ready",ready},
                {"actualMemberCount",group.accounts.size()},
                {"applicationsSubmitted",group.applicationsSubmitted},
                {"approvalsSucceeded",group.approvalsSucceeded},
                {"alreadyMembers",group.alreadyMembers},
                {"failureCount",group.failures.size()}
            }}
        };
        if(!writeJsonDocument(group.accountsPath,document)){
            return 5;
        }
        manifestGroups.push_back({
            {"groupId",group.groupId},
            {"groupName",group.groupName},
            {"targetMemberCount",group.targetMemberCount},
            {"actualMemberCount",group.accounts.size()},
            {"ready",ready},
            {"accountsFile",group.accountsPath.string()},
            {"failureCount",group.failures.size()}
        });
    }

    const std::filesystem::path manifestPath=
        std::filesystem::path(options.outputDirectory)/"manifest.json";
    nlohmann::json manifest{
        {"sourceAccountsFile",options.accountsPath},
        {"reviewerAccountId",options.reviewerAccount},
        {"allReady",allReady},
        {"groups",manifestGroups}
    };
    if(!writeJsonDocument(manifestPath,manifest)){
        return 5;
    }
    std::cout<<"group preparation finished, manifest="<<manifestPath.string()<<std::endl;
    std::cout<<"Credential files contain plaintext test passwords; protect the output directory.\n";
    return allReady?0:6;
}

static bool parsePositiveSize(const std::string& value,size_t& output,bool allowZero=false){
    try{
        size_t consumed=0;
        const unsigned long long parsed=std::stoull(value,&consumed);
        if(consumed!=value.size()||(!allowZero&&parsed==0)||
           parsed>std::numeric_limits<size_t>::max()){
            return false;
        }
        output=static_cast<size_t>(parsed);
        return true;
    }
    catch(const std::exception&){
        return false;
    }
}

struct MixedPrepareOptions {
    std::string host{"127.0.0.1"};
    int port{8080};
    std::string accountsPath{"tools/long_connection_accounts_1000.json"};
    std::string outputPath{"tools/mixed_load_manifest_1000.json"};
    std::string groupNamePrefix{"mixed_load"};
    size_t groupCount{20};
    size_t membersPerGroup{50};
    size_t friendDegree{10};
    int timeoutMs{10000};
    int requestIntervalMs{20};
    size_t checkpointEvery{50};
    bool resume{false};
};

struct MixedAccountRecord {
    ReusableCredential credential;
    std::unordered_set<std::string> groupIds;
    std::unordered_set<std::string> friendAccountIds;
};

struct MixedGroupRecord {
    size_t groupIndex{0};
    std::string groupId;
    std::string groupName;
    std::string ownerAccountId;
    std::vector<std::string> members;
};

static void printMixedPrepareUsage(const char* program){
    std::cout
        <<"Usage:\n"
        <<"  "<<program<<" --batch-prepare-mixed [options]\n\n"
        <<"Options:\n"
        <<"  --host HOST                  Default 127.0.0.1\n"
        <<"  --port PORT                  Default 8080\n"
        <<"  --accounts-file PATH         Default tools/long_connection_accounts_1000.json\n"
        <<"  --output PATH                Default tools/mixed_load_manifest_1000.json\n"
        <<"  --groups N                   Disjoint groups to create, default 20\n"
        <<"  --members-per-group N        Members per group including owner, default 50\n"
        <<"  --friend-degree N            Even friend degree per account, default 10\n"
        <<"  --group-name-prefix PREFIX   Default mixed_load\n"
        <<"  --timeout-ms N               Per-request timeout, default 10000\n"
        <<"  --request-interval-ms N      Delay between write requests, default 20\n"
        <<"  --checkpoint-every N         Persist after N friend edges, default 50\n"
        <<"  --resume                     Continue from an existing output manifest\n"
        <<"  --help\n";
}

static std::optional<MixedPrepareOptions> parseMixedPrepareOptions(int argc,char** argv){
    MixedPrepareOptions options;
    for(int index=2;index<argc;++index){
        const std::string option=argv[index];
        if(option=="--help"){
            printMixedPrepareUsage(argv[0]);
            return std::nullopt;
        }
        if(option=="--resume"){
            options.resume=true;
            continue;
        }
        if(index+1>=argc){
            std::cerr<<"missing value for "<<option<<std::endl;
            return std::nullopt;
        }
        const std::string value=argv[++index];
        if(option=="--host"){
            options.host=value;
        }
        else if(option=="--port"){
            if(!parsePositiveInt(value,options.port)||options.port>65535){
                std::cerr<<"invalid --port\n";
                return std::nullopt;
            }
        }
        else if(option=="--accounts-file"){
            options.accountsPath=value;
        }
        else if(option=="--output"){
            options.outputPath=value;
        }
        else if(option=="--groups"){
            if(!parsePositiveSize(value,options.groupCount)){
                std::cerr<<"invalid --groups\n";
                return std::nullopt;
            }
        }
        else if(option=="--members-per-group"){
            if(!parsePositiveSize(value,options.membersPerGroup)||options.membersPerGroup<2){
                std::cerr<<"invalid --members-per-group\n";
                return std::nullopt;
            }
        }
        else if(option=="--friend-degree"){
            if(!parsePositiveSize(value,options.friendDegree,true)){
                std::cerr<<"invalid --friend-degree\n";
                return std::nullopt;
            }
        }
        else if(option=="--group-name-prefix"){
            options.groupNamePrefix=value;
        }
        else if(option=="--timeout-ms"){
            if(!parsePositiveInt(value,options.timeoutMs)){
                std::cerr<<"invalid --timeout-ms\n";
                return std::nullopt;
            }
        }
        else if(option=="--request-interval-ms"){
            if(!parsePositiveInt(value,options.requestIntervalMs,true)){
                std::cerr<<"invalid --request-interval-ms\n";
                return std::nullopt;
            }
        }
        else if(option=="--checkpoint-every"){
            if(!parsePositiveSize(value,options.checkpointEvery)){
                std::cerr<<"invalid --checkpoint-every\n";
                return std::nullopt;
            }
        }
        else{
            std::cerr<<"unknown mixed preparation option: "<<option<<std::endl;
            return std::nullopt;
        }
    }
    if(options.accountsPath.empty()||options.outputPath.empty()||options.groupNamePrefix.empty()||
       options.friendDegree%2!=0){
        std::cerr<<"paths and group prefix cannot be empty; --friend-degree must be even\n";
        return std::nullopt;
    }
    return options;
}

static std::optional<std::vector<ReusableCredential>> loadMixedCredentials(
    const std::string& accountsPath){
    std::ifstream input(accountsPath);
    if(!input){
        std::cerr<<"cannot open accounts file: "<<accountsPath<<std::endl;
        return std::nullopt;
    }
    nlohmann::json document;
    try{
        input>>document;
    }
    catch(const std::exception& e){
        std::cerr<<"invalid accounts JSON: "<<e.what()<<std::endl;
        return std::nullopt;
    }
    const nlohmann::json* entries=&document;
    if(document.is_object()&&document.contains("accounts")){
        entries=&document["accounts"];
    }
    if(!entries->is_array()||entries->empty()){
        std::cerr<<"accounts file must contain a non-empty accounts array\n";
        return std::nullopt;
    }
    std::vector<ReusableCredential> credentials;
    std::unordered_set<std::string> seen;
    credentials.reserve(entries->size());
    for(const auto& entry:*entries){
        if(!entry.is_object()||!entry.contains("accountId")||!entry["accountId"].is_string()||
           !entry.contains("password")||!entry["password"].is_string()){
            std::cerr<<"each account requires string accountId and password\n";
            return std::nullopt;
        }
        ReusableCredential credential{
            .accountId=entry["accountId"].get<std::string>(),
            .password=entry["password"].get<std::string>(),
            .username=entry.value("username","")
        };
        if(credential.accountId.empty()||credential.password.empty()||
           !seen.insert(credential.accountId).second){
            std::cerr<<"accountId/password must be non-empty and accountId must be unique\n";
            return std::nullopt;
        }
        credentials.push_back(std::move(credential));
    }
    return credentials;
}

static bool connectAndLoginMixed(BatchConnection& connection,const MixedPrepareOptions& options,
                                 const ReusableCredential& credential,ClientState& state,
                                 std::string& error){
    if(!connection.connectTo(options.host,options.port)){
        error=connection.lastError();
        return false;
    }
    state=ClientState{};
    state.accountId=credential.accountId;
    CommandBuilder builder;
    auto response=connection.request(
        builder.buildLoginReq(state,credential.accountId,credential.password),options.timeoutMs);
    if(!response||!response->value("ok",false)){
        error=response?responseMessage(*response):connection.lastError();
        return false;
    }
    state.loggedIn=true;
    if(response->contains("data")&&(*response)["data"].is_object()){
        state.username=(*response)["data"].value("username",credential.username);
    }
    return true;
}

static std::optional<nlohmann::json> requestMixedWithRateLimit(
    BatchConnection& connection,const std::string& payload,const MixedPrepareOptions& options){
    constexpr int maxAttempts=5;
    for(int attempt=0;attempt<maxAttempts;++attempt){
        auto response=connection.request(payload,options.timeoutMs);
        if(!response||!isRateLimited(*response)){
            return response;
        }
        const int delayMs=std::max(retryAfterMs(*response,1000),options.requestIntervalMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    return std::nullopt;
}

static std::string mixedEdgeKey(const std::string& first,const std::string& second){
    return first<second?first+"|"+second:second+"|"+first;
}

static nlohmann::json sortedJsonArray(const std::unordered_set<std::string>& values){
    std::vector<std::string> sorted(values.begin(),values.end());
    std::sort(sorted.begin(),sorted.end());
    return sorted;
}

static bool writeMixedManifest(const MixedPrepareOptions& options,
                               const std::vector<MixedAccountRecord>& accounts,
                               const std::vector<MixedGroupRecord>& groups,
                               const std::unordered_set<std::string>& completedEdges){
    nlohmann::json accountEntries=nlohmann::json::array();
    for(const auto& account:accounts){
        accountEntries.push_back({
            {"accountId",account.credential.accountId},
            {"password",account.credential.password},
            {"username",account.credential.username},
            {"groupIds",sortedJsonArray(account.groupIds)},
            {"friendAccountIds",sortedJsonArray(account.friendAccountIds)}
        });
    }
    nlohmann::json groupEntries=nlohmann::json::array();
    for(const auto& group:groups){
        groupEntries.push_back({
            {"groupIndex",group.groupIndex},
            {"groupId",group.groupId},
            {"groupName",group.groupName},
            {"ownerAccountId",group.ownerAccountId},
            {"members",group.members}
        });
    }
    size_t friendLinks=0;
    for(const auto& account:accounts){
        friendLinks+=account.friendAccountIds.size();
    }
    nlohmann::json document{
        {"sourceAccountsFile",options.accountsPath},
        {"accounts",std::move(accountEntries)},
        {"groups",std::move(groupEntries)},
        {"completedFriendEdges",sortedJsonArray(completedEdges)},
        {"summary",{
            {"accountCount",accounts.size()},
            {"groupCount",groups.size()},
            {"targetGroupCount",options.groupCount},
            {"membersPerGroup",options.membersPerGroup},
            {"friendEdgeCount",completedEdges.size()},
            {"friendLinkCount",friendLinks},
            {"targetFriendDegree",options.friendDegree},
            {"ready",groups.size()==options.groupCount&&
                     completedEdges.size()==accounts.size()*(options.friendDegree/2)}
        }}
    };
    return writeJsonDocument(options.outputPath,document);
}

static bool loadMixedCheckpoint(const MixedPrepareOptions& options,
                                std::vector<MixedAccountRecord>& accounts,
                                std::vector<MixedGroupRecord>& groups,
                                std::unordered_set<std::string>& completedEdges){
    if(!options.resume||!std::filesystem::exists(options.outputPath)){
        return true;
    }
    std::ifstream input(options.outputPath);
    nlohmann::json document;
    try{
        input>>document;
    }
    catch(const std::exception& e){
        std::cerr<<"invalid mixed checkpoint: "<<e.what()<<std::endl;
        return false;
    }
    if(document.contains("summary")&&document["summary"].is_object()){
        const auto& summary=document["summary"];
        if(summary.value("targetGroupCount",options.groupCount)!=options.groupCount||
           summary.value("membersPerGroup",options.membersPerGroup)!=options.membersPerGroup||
           summary.value("targetFriendDegree",options.friendDegree)!=options.friendDegree){
            std::cerr<<"checkpoint topology differs from current mixed preparation options\n";
            return false;
        }
    }
    std::unordered_map<std::string,size_t> accountIndex;
    for(size_t index=0;index<accounts.size();++index){
        accountIndex.emplace(accounts[index].credential.accountId,index);
    }
    if(document.contains("accounts")&&document["accounts"].is_array()){
        for(const auto& entry:document["accounts"]){
            const auto found=accountIndex.find(entry.value("accountId",""));
            if(found==accountIndex.end()){
                continue;
            }
            if(entry.contains("groupIds")&&entry["groupIds"].is_array()){
                for(const auto& groupId:entry["groupIds"]){
                    if(groupId.is_string()) accounts[found->second].groupIds.insert(groupId.get<std::string>());
                }
            }
            if(entry.contains("friendAccountIds")&&entry["friendAccountIds"].is_array()){
                for(const auto& friendId:entry["friendAccountIds"]){
                    if(friendId.is_string()) accounts[found->second].friendAccountIds.insert(friendId.get<std::string>());
                }
            }
        }
    }
    if(document.contains("groups")&&document["groups"].is_array()){
        for(const auto& entry:document["groups"]){
            if(!entry.is_object()||!entry.contains("members")||!entry["members"].is_array()){
                continue;
            }
            MixedGroupRecord group;
            group.groupIndex=entry.value("groupIndex",groups.size());
            group.groupId=entry.value("groupId","");
            group.groupName=entry.value("groupName","");
            group.ownerAccountId=entry.value("ownerAccountId","");
            group.members=entry["members"].get<std::vector<std::string>>();
            if(!group.groupId.empty()&&group.members.size()==options.membersPerGroup){
                groups.push_back(std::move(group));
            }
        }
    }
    if(document.contains("completedFriendEdges")&&document["completedFriendEdges"].is_array()){
        for(const auto& edge:document["completedFriendEdges"]){
            if(edge.is_string()) completedEdges.insert(edge.get<std::string>());
        }
    }
    std::cout<<"resumed groups="<<groups.size()<<" friend_edges="<<completedEdges.size()<<std::endl;
    return true;
}

static std::string makeMixedGroupName(const MixedPrepareOptions& options,size_t groupIndex,
                                      int64_t runSuffix){
    const std::string suffix="_"+std::to_string(groupIndex+1)+"_"+std::to_string(runSuffix);
    constexpr size_t maxLength=64;
    if(options.groupNamePrefix.size()+suffix.size()<=maxLength){
        return options.groupNamePrefix+suffix;
    }
    return options.groupNamePrefix.substr(0,maxLength-suffix.size())+suffix;
}

static int runBatchPrepareMixed(int argc,char** argv){
    auto optionsResult=parseMixedPrepareOptions(argc,argv);
    if(!optionsResult){
        return argc>=3&&std::string(argv[2])=="--help"?0:2;
    }
    const MixedPrepareOptions options=std::move(*optionsResult);
    auto credentialsResult=loadMixedCredentials(options.accountsPath);
    if(!credentialsResult){
        return 3;
    }
    const auto& credentials=*credentialsResult;
    const size_t groupAccountCount=options.groupCount*options.membersPerGroup;
    if(groupAccountCount>credentials.size()||options.friendDegree>=credentials.size()){
        std::cerr<<"need at least groups*members-per-group accounts and friend-degree < account count\n";
        return 3;
    }

    std::vector<MixedAccountRecord> accounts;
    accounts.reserve(credentials.size());
    for(const auto& credential:credentials){
        accounts.push_back(MixedAccountRecord{.credential=credential});
    }
    std::vector<MixedGroupRecord> groups;
    std::unordered_set<std::string> completedEdges;
    if(!loadMixedCheckpoint(options,accounts,groups,completedEdges)){
        return 4;
    }
    if(!options.resume&&!writeMixedManifest(options,accounts,groups,completedEdges)){
        return 4;
    }
    std::unordered_set<size_t> completedGroupIndexes;
    for(const auto& group:groups){
        completedGroupIndexes.insert(group.groupIndex);
    }

    const auto runSuffix=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    for(size_t groupIndex=0;groupIndex<options.groupCount;++groupIndex){
        if(completedGroupIndexes.count(groupIndex)>0){
            continue;
        }
        const size_t firstAccount=groupIndex*options.membersPerGroup;
        const auto& owner=credentials[firstAccount];
        BatchConnection ownerConnection;
        ClientState ownerState;
        std::string error;
        if(!connectAndLoginMixed(ownerConnection,options,owner,ownerState,error)){
            std::cerr<<"group owner login failed accountId="<<owner.accountId
                     <<" error="<<error<<std::endl;
            return 5;
        }
        CommandBuilder ownerBuilder;
        MixedGroupRecord group;
        group.groupIndex=groupIndex;
        group.groupName=makeMixedGroupName(options,groupIndex,runSuffix);
        group.ownerAccountId=owner.accountId;
        auto createResponse=requestMixedWithRateLimit(
            ownerConnection,ownerBuilder.buildCreateGroupReq(ownerState,group.groupName),options);
        if(!createResponse||!createResponse->value("ok",false)||
           !createResponse->contains("data")||!(*createResponse)["data"].is_object()||
           !(*createResponse)["data"].contains("groupId")){
            std::cerr<<"create mixed group failed index="<<groupIndex
                     <<" error="<<(createResponse?responseMessage(*createResponse):ownerConnection.lastError())
                     <<std::endl;
            return 5;
        }
        group.groupId=(*createResponse)["data"]["groupId"].get<std::string>();
        group.members.push_back(owner.accountId);
        accounts[firstAccount].groupIds.insert(group.groupId);

        for(size_t memberOffset=1;memberOffset<options.membersPerGroup;++memberOffset){
            const size_t accountIndex=firstAccount+memberOffset;
            const auto& member=credentials[accountIndex];
            BatchConnection memberConnection;
            ClientState memberState;
            error.clear();
            if(!connectAndLoginMixed(memberConnection,options,member,memberState,error)){
                std::cerr<<"group applicant login failed accountId="<<member.accountId
                         <<" error="<<error<<std::endl;
                return 5;
            }
            CommandBuilder memberBuilder;
            auto applyResponse=requestMixedWithRateLimit(
                memberConnection,
                memberBuilder.buildApplyGroupJoinReq(memberState,group.groupId,
                                                      "mixed load test membership"),options);
            if(!applyResponse||!applyResponse->value("ok",false)||
               !applyResponse->contains("data")||!(*applyResponse)["data"].is_object()){
                std::cerr<<"group application failed accountId="<<member.accountId
                         <<" error="<<(applyResponse?responseMessage(*applyResponse):memberConnection.lastError())
                         <<std::endl;
                return 5;
            }
            const auto& applyData=(*applyResponse)["data"];
            bool memberReady=applyData.value("alreadyIn",false);
            if(!memberReady&&(applyData.value("submitted",false)||
                              applyData.value("alreadyPending",false))){
                auto reviewResponse=requestMixedWithRateLimit(
                    ownerConnection,
                    ownerBuilder.buildReviewGroupJoinReq(ownerState,group.groupId,
                                                         member.accountId,true),options);
                if(reviewResponse&&reviewResponse->value("ok",false)&&
                   reviewResponse->contains("data")&&(*reviewResponse)["data"].is_object()){
                    const auto& reviewData=(*reviewResponse)["data"];
                    memberReady=reviewData.value("memberAdded",false)||
                                reviewData.value("approved",false);
                }
                if(!memberReady){
                    std::cerr<<"group approval failed accountId="<<member.accountId
                             <<" error="<<(reviewResponse?responseMessage(*reviewResponse):ownerConnection.lastError())
                             <<std::endl;
                    return 5;
                }
            }
            if(!memberReady){
                std::cerr<<"group membership not confirmed accountId="<<member.accountId<<std::endl;
                return 5;
            }
            group.members.push_back(member.accountId);
            accounts[accountIndex].groupIds.insert(group.groupId);
            if(options.requestIntervalMs>0){
                std::this_thread::sleep_for(std::chrono::milliseconds(options.requestIntervalMs));
            }
        }
        groups.push_back(std::move(group));
        completedGroupIndexes.insert(groupIndex);
        if(!writeMixedManifest(options,accounts,groups,completedEdges)){
            return 6;
        }
        std::cout<<"prepared group "<<(groupIndex+1)<<'/'<<options.groupCount
                 <<" groupId="<<groups.back().groupId<<std::endl;
    }

    const size_t halfDegree=options.friendDegree/2;
    const size_t expectedEdges=accounts.size()*halfDegree;
    size_t newEdgesSinceCheckpoint=0;
    for(size_t sourceIndex=0;sourceIndex<accounts.size();++sourceIndex){
        BatchConnection requesterConnection;
        ClientState requesterState;
        std::string error;
        bool requesterConnected=false;
        CommandBuilder requesterBuilder;
        for(size_t offset=1;offset<=halfDegree;++offset){
            const size_t targetIndex=(sourceIndex+offset)%accounts.size();
            const auto& requester=credentials[sourceIndex];
            const auto& receiver=credentials[targetIndex];
            const std::string edgeKey=mixedEdgeKey(requester.accountId,receiver.accountId);
            if(completedEdges.count(edgeKey)>0){
                continue;
            }
            if(!requesterConnected){
                if(!connectAndLoginMixed(requesterConnection,options,requester,requesterState,error)){
                    std::cerr<<"friend requester login failed accountId="<<requester.accountId
                             <<" error="<<error<<std::endl;
                    return 7;
                }
                requesterConnected=true;
            }
            auto sendResponse=requestMixedWithRateLimit(
                requesterConnection,
                requesterBuilder.buildSendFriendRequestReq(requesterState,receiver.accountId),options);
            uint64_t requestId=0;
            bool alreadyFriends=false;
            bool pendingRequest=false;
            if(sendResponse&&sendResponse->value("ok",false)&&sendResponse->contains("data")&&
               (*sendResponse)["data"].is_object()){
                requestId=(*sendResponse)["data"].value("requestId",uint64_t{0});
            }
            else if(sendResponse){
                const int code=sendResponse->value("code",-1);
                alreadyFriends=code==static_cast<int>(im::ErrorCode::ALREADY_FRIENDS);
                pendingRequest=code==static_cast<int>(im::ErrorCode::FRIEND_REQUEST_EXISTS)||
                               code==static_cast<int>(im::ErrorCode::USER_EXISTS)||
                               code==static_cast<int>(im::ErrorCode::Conflict);
                if(!alreadyFriends&&!pendingRequest){
                    std::cerr<<"friend request failed from="<<requester.accountId
                             <<" to="<<receiver.accountId<<" code="<<code
                             <<" error="<<responseMessage(*sendResponse)<<std::endl;
                    return 7;
                }
            }
            else{
                std::cerr<<"friend request failed from="<<requester.accountId
                         <<" to="<<receiver.accountId
                         <<" error="<<requesterConnection.lastError()<<std::endl;
                return 7;
            }

            if(!alreadyFriends){
                BatchConnection receiverConnection;
                ClientState receiverState;
                error.clear();
                if(!connectAndLoginMixed(receiverConnection,options,receiver,receiverState,error)){
                    std::cerr<<"friend receiver login failed accountId="<<receiver.accountId
                             <<" error="<<error<<std::endl;
                    return 7;
                }
                CommandBuilder receiverBuilder;
                if(pendingRequest&&requestId==0){
                    auto listResponse=requestMixedWithRateLimit(
                        receiverConnection,receiverBuilder.buildListFriendRequestReq(receiverState),options);
                    if(listResponse&&listResponse->value("ok",false)&&
                       listResponse->contains("data")&&(*listResponse)["data"].is_object()&&
                       (*listResponse)["data"].contains("requests")&&
                       (*listResponse)["data"]["requests"].is_array()){
                        for(const auto& request:(*listResponse)["data"]["requests"]){
                            if(request.value("accountId","")==requester.accountId){
                                requestId=request.value("requestId",uint64_t{0});
                                break;
                            }
                        }
                    }
                }
                if(requestId==0){
                    std::cerr<<"cannot resolve friend request id from="<<requester.accountId
                             <<" to="<<receiver.accountId<<std::endl;
                    return 7;
                }
                auto acceptResponse=requestMixedWithRateLimit(
                    receiverConnection,
                    receiverBuilder.buildAcceptFriendRequestReq(receiverState,
                                                                std::to_string(requestId)),options);
                if(!acceptResponse||(!acceptResponse->value("ok",false)&&
                   acceptResponse->value("code",-1)!=static_cast<int>(im::ErrorCode::ALREADY_FRIENDS))){
                    std::cerr<<"accept friend request failed requestId="<<requestId
                             <<" error="<<(acceptResponse?responseMessage(*acceptResponse):receiverConnection.lastError())
                             <<std::endl;
                    return 7;
                }
            }

            completedEdges.insert(edgeKey);
            accounts[sourceIndex].friendAccountIds.insert(receiver.accountId);
            accounts[targetIndex].friendAccountIds.insert(requester.accountId);
            ++newEdgesSinceCheckpoint;
            if(options.requestIntervalMs>0){
                std::this_thread::sleep_for(std::chrono::milliseconds(options.requestIntervalMs));
            }
            if(newEdgesSinceCheckpoint>=options.checkpointEvery){
                if(!writeMixedManifest(options,accounts,groups,completedEdges)){
                    return 8;
                }
                newEdgesSinceCheckpoint=0;
                std::cout<<"friend edges "<<completedEdges.size()<<'/'<<expectedEdges<<std::endl;
            }
        }
    }
    if(!writeMixedManifest(options,accounts,groups,completedEdges)){
        return 8;
    }
    std::cout<<"mixed load data ready: groups="<<groups.size()
             <<" friend_edges="<<completedEdges.size()
             <<" manifest="<<options.outputPath<<std::endl;
    std::cout<<"Manifest contains plaintext test passwords; protect and exclude it from production.\n";
    return groups.size()==options.groupCount&&completedEdges.size()==expectedEdges?0:9;
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

    if(argc>=2&&std::string(argv[1])=="--batch-register"){
        return runBatchRegister(argc,argv);
    }
    if(argc>=2&&std::string(argv[1])=="--batch-prepare-groups"){
        return runBatchPrepareGroups(argc,argv);
    }
    if(argc>=2&&std::string(argv[1])=="--batch-prepare-mixed"){
        return runBatchPrepareMixed(argc,argv);
    }

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
