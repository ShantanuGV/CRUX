cmake_minimum_required(VERSION 3.16)

project(crux
    VERSION 1.1.0
    LANGUAGES CXX
    DESCRIPTION "CRUX - Encrypted Direct Peer-to-Peer Messenger"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── OpenSSL ─────────────────────────────────────────────────
find_package(OpenSSL REQUIRED)

# ─── Main executable ─────────────────────────────────────────

set(CRUX_SOURCES
    src/main.cpp
    src/core/application.cpp
    src/core/peer.cpp
    src/core/state.cpp
    src/network/connection.cpp
    src/network/tls_connection.cpp
    src/protocol/frame.cpp
    src/storage/storage.cpp
    src/ui/terminal.cpp
    src/nat/nat.cpp
    src/identity/identity.cpp
    src/sync/address_sync.cpp
    src/updater/updater.cpp
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
    target_link_libraries(crux PRIVATE ws2_32 OpenSSL::SSL OpenSSL::Crypto)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(crux PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
endif()

# ─── Tests ───────────────────────────────────────────────────

enable_testing()

set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_frame.cpp
    tests/test_peer.cpp
    tests/test_state.cpp
    tests/test_storage.cpp
    tests/test_network.cpp
    tests/test_identity.cpp
    tests/test_addr_sync.cpp
    # Shared sources needed by tests
    src/core/peer.cpp
    src/core/state.cpp
    src/protocol/frame.cpp
    src/storage/storage.cpp
    src/network/connection.cpp
    src/network/tls_connection.cpp
    src/nat/nat.cpp
    src/identity/identity.cpp
    src/sync/address_sync.cpp
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
    target_link_libraries(crux_tests PRIVATE ws2_32 OpenSSL::SSL OpenSSL::Crypto)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(crux_tests PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
endif()

add_test(NAME crux_tests COMMAND crux_tests)

# ─── Install ─────────────────────────────────────────────────

install(TARGETS crux
    RUNTIME DESTINATION bin
)

# Static linking option for portable binaries
option(CRUX_STATIC "Build a statically linked binary" OFF)
if(CRUX_STATIC)
    if(WIN32 AND MINGW)
        target_link_options(crux PRIVATE -static -static-libgcc -static-libstdc++)
    elseif(UNIX)
        target_link_options(crux PRIVATE -static)
    endif()
endif()
