#include "test_framework.hpp"
#include "crux/network/connection.hpp"
#include "crux/protocol/frame.hpp"

#include <thread>
#include <chrono>

using namespace crux;

TEST(listener_start_stop) {
    Listener listener;
    CHECK(listener.start(0)); // Use port 0 for OS-assigned port (not always available)
    // If port 0 doesn't work on all platforms, use a high port
    listener.stop();
    CHECK(!listener.is_running());
}

TEST(listener_start_specific_port) {
    Listener listener;
    // Use a high port unlikely to conflict
    CHECK(listener.start(19876));
    CHECK(listener.is_running());
    CHECK_EQ(listener.port(), 19876);
    listener.stop();
}

TEST(listener_double_start) {
    Listener listener;
    CHECK(listener.start(19877));
    // Starting again should stop the previous and restart
    CHECK(listener.start(19878));
    CHECK_EQ(listener.port(), 19878);
    listener.stop();
}

TEST(connection_connect_refused) {
    Connection conn;
    // Connect to a port nobody is listening on
    bool ok = conn.connect("127.0.0.1", 19999, 1000);
    CHECK(!ok);
    CHECK(!conn.last_error().empty());
}

TEST(loopback_connect_accept) {
    Listener listener;
    CHECK(listener.start(19880));

    // Connect from a client in another thread
    std::thread client_thread([&]() {
        // Small delay to ensure listener is ready
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Connection client;
        bool ok = client.connect("127.0.0.1", 19880, 3000);
        CHECK(ok);
        CHECK(client.is_connected());
        client.close();
    });

    auto server_conn = listener.accept(3000);
    CHECK(server_conn.has_value());
    if (server_conn) {
        CHECK(server_conn->is_connected());
        server_conn->close();
    }

    client_thread.join();
    listener.stop();
}

TEST(loopback_send_recv_frame) {
    Listener listener;
    CHECK(listener.start(19881));

    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Connection client;
        CHECK(client.connect("127.0.0.1", 19881, 3000));

        // Send a HELLO frame
        CHECK(client.send_frame(MessageType::HELLO, "ClientName"));

        // Wait for response
        auto frame = client.recv_frame(3000);
        CHECK(frame.has_value());
        if (frame) {
            CHECK_EQ(frame->type, MessageType::HELLO);
            CHECK_EQ(frame->payload, "ServerName");
        }

        client.close();
    });

    auto server = listener.accept(3000);
    CHECK(server.has_value());
    if (server) {
        // Receive HELLO
        auto frame = server->recv_frame(3000);
        CHECK(frame.has_value());
        if (frame) {
            CHECK_EQ(frame->type, MessageType::HELLO);
            CHECK_EQ(frame->payload, "ClientName");
        }

        // Send HELLO back
        CHECK(server->send_frame(MessageType::HELLO, "ServerName"));
        server->close();
    }

    client_thread.join();
    listener.stop();
}

TEST(loopback_full_handshake) {
    Listener listener;
    CHECK(listener.start(19882));

    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Connection client;
        CHECK(client.connect("127.0.0.1", 19882, 3000));

        // HELLO exchange
        CHECK(client.send_frame(MessageType::HELLO, "PeerA"));
        auto frame = client.recv_frame(3000);
        CHECK(frame.has_value());
        CHECK_EQ(frame->type, MessageType::HELLO);

        // Send REQUEST
        CHECK(client.send_frame(MessageType::REQUEST, "PeerA"));

        // Wait for ACCEPT
        frame = client.recv_frame(3000);
        CHECK(frame.has_value());
        CHECK_EQ(frame->type, MessageType::ACCEPT);

        // Send MESSAGE
        CHECK(client.send_frame(MessageType::MESSAGE, "Hello from A!"));

        // Receive MESSAGE
        frame = client.recv_frame(3000);
        CHECK(frame.has_value());
        CHECK_EQ(frame->type, MessageType::MESSAGE);
        CHECK_EQ(frame->payload, "Hello from B!");

        // CLOSE
        CHECK(client.send_frame(MessageType::CLOSE));
        client.close();
    });

    auto server = listener.accept(3000);
    CHECK(server.has_value());
    if (server) {
        // HELLO exchange
        auto frame = server->recv_frame(3000);
        CHECK(frame.has_value());
        CHECK_EQ(frame->type, MessageType::HELLO);

        CHECK(server->send_frame(MessageType::HELLO, "PeerB"));

        // Receive REQUEST
        frame = server->recv_frame(3000);
        CHECK(frame.has_value());
        CHECK_EQ(frame->type, MessageType::REQUEST);

        // Send ACCEPT
        CHECK(server->send_frame(MessageType::ACCEPT, "PeerB"));

        // Receive MESSAGE
        frame = server->recv_frame(3000);
        CHECK(frame.has_value());
        CHECK_EQ(frame->type, MessageType::MESSAGE);
        CHECK_EQ(frame->payload, "Hello from A!");

        // Send MESSAGE
        CHECK(server->send_frame(MessageType::MESSAGE, "Hello from B!"));

        // Receive CLOSE
        frame = server->recv_frame(3000);
        CHECK(frame.has_value());
        CHECK_EQ(frame->type, MessageType::CLOSE);

        server->close();
    }

    client_thread.join();
    listener.stop();
}

TEST(connection_disconnect_detection) {
    Listener listener;
    CHECK(listener.start(19883));

    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Connection client;
        CHECK(client.connect("127.0.0.1", 19883, 3000));
        // Immediately disconnect
        client.close();
    });

    auto server = listener.accept(3000);
    CHECK(server.has_value());
    if (server) {
        // Give client time to close
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Should detect disconnection
        auto frame = server->recv_frame(1000);
        CHECK(!frame.has_value());
        server->close();
    }

    client_thread.join();
    listener.stop();
}
