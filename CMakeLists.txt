cmake_minimum_required(VERSION 3.16)

project(crux
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "CRUX - Direct Peer-to-Peer TCP CLI Messenger"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Main executable ────────────────────────────────────────

set(CRUX_SOURCES
    src/main.cpp
    src/core/application.cpp
    src/core/peer.cpp
    src/core/state.cpp
    src/network/connection.cpp
    src/protocol/frame.cpp
    src/storage/storage.cpp
    src/ui/terminal.cpp
)

# Platform-specific sources
if(WIN32)
    list(APPEND CRUX_SOURCES src/platform/windows/socket.cpp)
else()
    list(APPEND CRUX_SOURCES src/platform/linux/socket.cpp)
endif()

add_executable(crux ${CRUX_SOURCES})

target_include_directories(crux
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Platform-specific link libraries
if(WIN32)
    target_link_libraries(crux PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(crux PRIVATE Threads::Threads)
endif()

# ─── Tests ──────────────────────────────────────────────────

enable_testing()

set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_frame.cpp
    tests/test_peer.cpp
    tests/test_state.cpp
    tests/test_storage.cpp
    tests/test_network.cpp
    # Shared sources needed by tests
    src/core/peer.cpp
    src/core/state.cpp
    src/protocol/frame.cpp
    src/storage/storage.cpp
    src/network/connection.cpp
)

if(WIN32)
    list(APPEND TEST_SOURCES src/platform/windows/socket.cpp)
else()
    list(APPEND TEST_SOURCES src/platform/linux/socket.cpp)
endif()

add_executable(crux_tests ${TEST_SOURCES})

target_include_directories(crux_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    target_link_libraries(crux_tests PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(crux_tests PRIVATE Threads::Threads)
endif()

add_test(NAME crux_tests COMMAND crux_tests)
