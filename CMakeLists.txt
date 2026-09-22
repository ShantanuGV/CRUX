cmake_minimum_required(VERSION 3.16)

project(crux
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(crux
    src/main.cpp
    src/tcp.cpp
)

target_include_directories(crux
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
