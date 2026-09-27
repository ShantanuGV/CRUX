#include "test_framework.hpp"
#include "crux/storage/storage.hpp"
#include <filesystem>

using namespace crux;

TEST(storage_save_load_peers) {
    PeerBook book;
    book.add({"Alice", "192.168.1.1", 5623});
    book.add({"Bob", "10.0.0.2", 8080});

    CHECK(Storage::save_peers(book));

    PeerBook loaded;
    CHECK(Storage::load_peers(loaded));
    CHECK_EQ(loaded.size(), 2u);
    CHECK_EQ(loaded.at(0)->nickname, "Alice");
    CHECK_EQ(loaded.at(0)->address, "192.168.1.1");
    CHECK_EQ(loaded.at(0)->port, 5623);
    CHECK_EQ(loaded.at(1)->nickname, "Bob");
    CHECK_EQ(loaded.at(1)->address, "10.0.0.2");
    CHECK_EQ(loaded.at(1)->port, 8080);
}

TEST(storage_save_load_settings) {
    Settings s;
    s.listen_port = 9999;
    s.nickname = "TestUser";

    CHECK(Storage::save_settings(s));

    Settings loaded;
    CHECK(Storage::load_settings(loaded));
    CHECK_EQ(loaded.listen_port, 9999);
    CHECK_EQ(loaded.nickname, "TestUser");
}

TEST(storage_empty_peers) {
    PeerBook book;
    CHECK(Storage::save_peers(book));

    PeerBook loaded;
    CHECK(Storage::load_peers(loaded));
    CHECK_EQ(loaded.size(), 0u);
}

TEST(storage_config_dir_exists) {
    std::string dir = Storage::config_dir();
    CHECK(!dir.empty());
    CHECK(std::filesystem::exists(dir));
}
