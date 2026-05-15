cmake_minimum_required(VERSION 3.15)

project(Project_Chat VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
option(ENABLE_SQL "Enable MySQL storage backend" ON)
# =========================
# 库（核心代码）
# =========================
file(GLOB SRC_FILES src/*.cpp src/im/*.cpp src/config/*.cpp src/logger/*.cpp src/storage/*.cpp src/storage/memory/*.cpp)

if(ENABLE_SQL)
    file(GLOB SQL_SRC_FILES src/storage/sql/*.cpp)
    list(APPEND SRC_FILES ${SQL_SRC_FILES})
endif()

add_library(chat_lib ${SRC_FILES})

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
endif()
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