#pragma once
#include <cstdint>
#include <string>
#include "third_party/json.hpp"

/*
服务端配置，管监听与线程参数
*/
class ServerConfig{
public:
    static ServerConfig fromJson(const nlohmann::json&);//解析server节点
    void applyEnvOverrides();//
    void validateOrThrow()const;//校验端口范围，线程数非负
private:
    std::string host{"0.0.0.0"};
    uint16_t port{8080};
    int ioThreads{2};
    int backlog{1024};
};