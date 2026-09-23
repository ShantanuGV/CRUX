#pragma once

#ifdef _WIN32

#include <winsock2.h>

namespace crux::platform {

using Socket = SOCKET;

constexpr Socket INVALID_SOCKET_VALUE = INVALID_SOCKET;

}

#else

namespace crux::platform {

using Socket = int;

constexpr Socket INVALID_SOCKET_VALUE = -1;

}

#endif