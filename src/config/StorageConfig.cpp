#include "config/StorageConfig.h"

StorageConfig StorageConfig::fromJson(const nlohmann::json& j){
    StorageConfig config;
    config.type_=ConfigParseHelper::getOrDefault(j,"type",config.type());
    config.fallbackToMemory_=ConfigParseHelper::getOrDefault(j,"fallback_to_memory",config.fallbackToMemory());
    return config;
}

void StorageConfig::loadFromEnv(){
    auto typeEnv=ConfigParseHelper::getEnv("CHAT_STORAGE_TYPE");
    if(typeEnv){
        type_=typeEnv.value();
    }
    auto fallbackEnv=ConfigParseHelper::getEnv("CHAT_STORAGE_FALLBACK_TO_MEMORY");
    if(fallbackEnv){
        fallbackToMemory_=ConfigParseHelper::parseEnvBool(fallbackEnv.value(),"CHAT_STORAGE_FALLBACK_TO_MEMORY");
    }
}
void StorageConfig::validateOrThrow()const{
    if(type_!="memory"&&type_!="sql"){
        throw std::runtime_error("Invaild type: "+type_);
    }

}

const std::string& StorageConfig::type()const{
    return type_;
}
bool StorageConfig::fallbackToMemory()const{
    return fallbackToMemory_;
}