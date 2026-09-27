#include "test_framework.hpp"
#include "crux/platform/socket.hpp"

int main() {
    // Initialize platform networking for network tests
    crux::platform::net_init();

    int result = crux::test::run_all_tests();

    crux::platform::net_cleanup();
    return result;
}
