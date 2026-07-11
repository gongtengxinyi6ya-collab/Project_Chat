cmake_minimum_required(VERSION 3.15)

project(Project_Chat VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
option(ENABLE_SQL "Enable MySQL storage backend" ON)
option(ENABLE_REDIS "Enable Redis cache backend" ON)
# =========================
# 库（核心代码）
# =========================
file(GLOB SRC_FILES src/*.cpp src/im/*.cpp src/config/*.cpp src/logger/*.cpp src/storage/*.cpp src/storage/memory/*.cpp src/security/*.cpp src/auth/*.cpp src/id/*.cpp src/infra/health/*.cpp src/infra/signal/*.cpp src/timer/*.cpp src/infra/maintenance/*.cpp src/infra/thread/*.cpp)
file(GLOB RATE_LIMIT_SRC_FILES
    src/security/rate_limit/RateLimiter.cpp
)
list(APPEND SRC_FILES ${RATE_LIMIT_SRC_FILES})
if(ENABLE_SQL)
    file(GLOB SQL_SRC_FILES src/storage/sql/*.cpp)
    list(APPEND SRC_FILES ${SQL_SRC_FILES})
endif()
if(ENABLE_REDIS)
    file(GLOB REDIS_SRC_FILES src/infra/redis/*.cpp src/security/rate_limit/RedisRateLimitStore.cpp)
    list(APPEND SRC_FILES ${REDIS_SRC_FILES})
endif()

add_library(chat_lib ${SRC_FILES})
if(ENABLE_REDIS)
    find_path(HIREDIS_HEADER hiredis/hiredis.h
        PATHS /usr/local/include /usr/include
    )

    find_library(HIREDIS_LIB hiredis
        PATHS /usr/local/lib64 /usr/local/lib /usr/lib64 /usr/lib
    )

    find_path(REDIS_PLUS_PLUS_HEADER sw/redis++/redis++.h
        PATHS /usr/local/include /usr/include
    )

    find_library(REDIS_PLUS_PLUS_LIB redis++
        PATHS /usr/local/lib64 /usr/local/lib /usr/lib64 /usr/lib
    )

    if(NOT HIREDIS_HEADER OR NOT HIREDIS_LIB)
        message(FATAL_ERROR "hiredis not found")
    endif()

    if(NOT REDIS_PLUS_PLUS_HEADER OR NOT REDIS_PLUS_PLUS_LIB)
        message(FATAL_ERROR "redis-plus-plus not found")
    endif()

    target_include_directories(chat_lib PUBLIC
        ${HIREDIS_HEADER}
        ${REDIS_PLUS_PLUS_HEADER}
    )

    target_link_libraries(chat_lib PUBLIC
        ${REDIS_PLUS_PLUS_LIB}
        ${HIREDIS_LIB}
        pthread
    )

    target_compile_definitions(chat_lib PUBLIC PROJECT_CHAT_ENABLE_REDIS)
endif()
target_include_directories(chat_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
if(ENABLE_SQL)
    find_package(mysql-concpp CONFIG REQUIRED)
    target_link_libraries(chat_lib PUBLIC mysql::concpp-jdbc)
    target_include_directories(chat_lib
    PUBLIC
        /usr/include/mysql-cppconn/jdbc
)
    target_compile_definitions(chat_lib PUBLIC PROJECT_CHAT_ENABLE_SQL)
    add_executable(sql_repo_test tools/sql_repo_test.cpp)
    target_link_libraries(sql_repo_test PRIVATE chat_lib)
endif()
find_package(OpenSSL REQUIRED)
target_link_libraries(chat_lib PUBLIC OpenSSL::SSL OpenSSL::Crypto)
if(MSVC)
    target_compile_options(chat_lib PRIVATE /W4)
else()
    target_compile_options(chat_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =========================
# server
# =========================
add_executable(server server/server.cpp)

target_link_libraries(server PRIVATE chat_lib)

# =========================
# client
# =========================
add_executable(client client/client.cpp)

target_link_libraries(client PRIVATE chat_lib)
# =========================
# load_test
# =========================
add_executable(load_test tools/load_test.cpp)

target_link_libraries(load_test PRIVATE chat_lib)
# =========================
# ASAN
# =========================
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)

if(ENABLE_ASAN)
    target_compile_options(chat_lib PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(chat_lib PRIVATE -fsanitize=address)
endif()