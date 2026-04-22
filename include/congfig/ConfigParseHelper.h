#include <string>
#include<optional>
#include <stdexcept>
#include "third_party/json.hpp"
/*工具类，统一容错和类型检查，减少配置类重复代码；报错信息带完整键名*/

class ConfigParseHelper{
public:
    template<typename T>
    //如果键不存在或类型错误，抛出异常，读取子对象节点，必填字符串
    static T getOrThrow(const nlohmann::json& j,const std::string& key){
        if(!j.contains(key)){
            throw std::runtime_error("Missing config key: "+key);
        }
        try{
            return j.at(key).get<T>();
        }catch(const nlohmann::json::exception& e){
            throw std::runtime_error("Invalid type for config key: "+key+", expected "+typeid(T).name());
        }
    }
    template<typename T>
    //如果键不存在，返回默认值；如果存在但类型错误，抛出异常，可读取整数，无符号整数，布尔
    static T getOrDefault(const nlohmann::json& j,const std::string& key,const T& defaultValue){
        if(!j.contains(key)){
            return defaultValue;
        }
        try{
            return j.at(key).get<T>();
        }catch(const nlohmann::json::exception& e){
            throw std::runtime_error("Invalid type for config key: "+key+", expected "+typeid(T).name());
        }
    }
    //读取环境变量，返回optional<string>，不存在返回nullopt
    static std::optional<std::string> getEnv(const char* name){
        const char* value = std::getenv(name);
        if(value){
            return std::string(value);
        }
        return std::nullopt;
    }
    //环境变量文本转bool不区分大小写，接受"true","1","yes"为true，"false","0","no"为false，其他值抛异常
    static bool parseEnvBool(const std::string& value,const std::string& envName){
        std::string lowerValue;
        std::transform(value.begin(), value.end(), std::back_inserter(lowerValue), ::tolower);
        if(lowerValue=="true"||lowerValue=="1"||lowerValue=="yes"){
            return true;
        }else if(lowerValue=="false"||lowerValue=="0"||lowerValue=="no"){
            return false;
        }else{
            throw std::runtime_error("Invalid boolean value for environment variable "+envName+": "+value);
        }
    }
    //环境变量文本转整数
    static int parseEnvInt(const std::string& value,const std::string& envName){
        try{
            return std::stoi(value);
        }catch(const std::exception& e){
            throw std::runtime_error("Invalid integer value for environment variable "+envName+": "+value);
        }
    }
    //环境变量文本转无符号整数,检查非负与上限
    static uint32_t parseEnvUInt(const std::string& value,const std::string& envName,uint32_t maxValue=UINT32_MAX){
        try{
            int intValue = std::stoi(value);
            if(intValue<0||static_cast<uint32_t>(intValue)>maxValue){
                throw std::runtime_error("Integer value out of range for environment variable "+envName+": "+value);
            }
            return static_cast<uint32_t>(intValue);
        }catch(const std::exception& e){
            throw std::runtime_error("Invalid unsigned integer value for environment variable "+envName+": "+value);
        }
    }
    //通用范围校验，错误信息包含字段名，当前值，合法范围
    static void checkRange(const std::string& field,int64_t value,int64_t minValue,int64_t maxValue){
        if(value<minValue||value>maxValue){
            throw std::runtime_error(field+" value "+std::to_string(value)+" out of range ["+std::to_string(minValue)+","+std::to_string(maxValue)+"]");
        }
    }
    //统一类型错误抛异常
    static void throwTypeError(const std::string& key,const std::string& expectedType){
        throw std::runtime_error("Invalid type for config key: "+key+", expected "+expectedType);
    }
    //统一缺失字段报错
    static void throwMissingKeyError(const std::string& key){
        throw std::runtime_error("Missing config key: "+key);
    }
};