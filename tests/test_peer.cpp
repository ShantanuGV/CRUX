#include "test_framework.hpp"
#include "crux/core/peer.hpp"

using namespace crux;

TEST(peer_add) {
    PeerBook book;
    CHECK(book.empty());
    CHECK_EQ(book.size(), 0u);

    book.add({"Alice", "192.168.1.1", 5623});
    CHECK_EQ(book.size(), 1u);
    CHECK(!book.empty());

    const auto* p = book.at(0);
    CHECK(p != nullptr);
    CHECK_EQ(p->nickname, "Alice");
    CHECK_EQ(p->address, "192.168.1.1");
    CHECK_EQ(p->port, 5623);
}

TEST(peer_add_multiple) {
    PeerBook book;
    book.add({"Alice", "10.0.0.1", 5623});
    book.add({"Bob", "10.0.0.2", 5624});
    book.add({"Charlie", "10.0.0.3", 5625});
    CHECK_EQ(book.size(), 3u);
}

TEST(peer_remove) {
    PeerBook book;
    book.add({"Alice", "10.0.0.1", 5623});
    book.add({"Bob", "10.0.0.2", 5624});

    CHECK(book.remove(0));
    CHECK_EQ(book.size(), 1u);
    CHECK_EQ(book.at(0)->nickname, "Bob");
}

TEST(peer_remove_invalid_index) {
    PeerBook book;
    book.add({"Alice", "10.0.0.1", 5623});
    CHECK(!book.remove(5));
    CHECK_EQ(book.size(), 1u);
}

TEST(peer_update) {
    PeerBook book;
    book.add({"Alice", "10.0.0.1", 5623});

    Peer updated{"Alice Updated", "192.168.1.100", 9999};
    CHECK(book.update(0, updated));
    CHECK_EQ(book.at(0)->nickname, "Alice Updated");
    CHECK_EQ(book.at(0)->address, "192.168.1.100");
    CHECK_EQ(book.at(0)->port, 9999);
}

TEST(peer_update_invalid_index) {
    PeerBook book;
    Peer p{"Test", "1.2.3.4", 1234};
    CHECK(!book.update(0, p));
}

TEST(peer_at_invalid) {
    PeerBook book;
    CHECK(book.at(0) == nullptr);
    CHECK(book.at(100) == nullptr);

    book.add({"Alice", "10.0.0.1", 5623});
    CHECK(book.at(0) != nullptr);
    CHECK(book.at(1) == nullptr);
}
