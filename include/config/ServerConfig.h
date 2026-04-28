#pragma once
#include <cstdint>
#include <string>
#include "third_party/json.hpp"
#include "config/ConfigParseHelper.h"
/*
服务端配置，管监听与线程参数
*/
class ServerConfig{
public:
    static ServerConfig fromJson(const nlohmann::json&);//解析server节点
    void applyEnvOverrides();//覆盖环境变量
    void validateOrThrow()const;//校验端口范围，线程数非负

    //属性
    std::string host{"0.0.0.0"};//默认监听所有地址
    uint16_t port{8080};//默认端口8080
    int ioThreads{4};//默认IO线程数4，通常设置为CPU核心数的2倍，但不超过1000
    int backlog{1024};//默认监听队列长度1024，过小可能导致连接被拒绝，过大可能占用过多资源
};