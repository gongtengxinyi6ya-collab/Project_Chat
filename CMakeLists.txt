cmake_minimum_required(VERSION 3.15)

project(Project_Chat VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# =========================
# 库（核心代码）
# =========================
file(GLOB SRC_FILES src/*.cpp src/im/*.cpp)

add_library(chat_lib ${SRC_FILES})

target_include_directories(chat_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

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
# ASAN
# =========================
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)

if(ENABLE_ASAN)
    target_compile_options(chat_lib PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(chat_lib PRIVATE -fsanitize=address)
endif()