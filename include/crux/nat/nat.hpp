#pragma once

#include <cstdint>
#include <string>

namespace crux::nat {

// Result of NAT traversal setup
struct NATStatus {
    bool   upnp_available  = false;   // Router supports UPnP
    bool   port_forwarded  = false;   // Port mapping was created
    std::string public_ip;            // Public (internet-facing) IP
    std::string local_ip;             // Local (LAN) IP
    std::uint16_t external_port = 0;  // Port on the router
    std::string gateway_ip;           // Router's IP
    std::string error;                // Error message if something failed
};

// Manages UPnP port forwarding and public IP discovery.
// Allows CRUX to work across the internet, not just LAN.
class NATTraversal {
public:
    NATTraversal() = default;
    ~NATTraversal();

    NATTraversal(const NATTraversal&) = delete;
    NATTraversal& operator=(const NATTraversal&) = delete;

    // Perform full NAT setup:
    //   1. Discover local IP
    //   2. Discover public IP (via HTTP)
    //   3. Find UPnP gateway on the network
    //   4. Add port mapping (internal_port → same external port)
    NATStatus setup(std::uint16_t internal_port);

    // Remove the port mapping (call on exit)
    void cleanup();

    // Get the last status
    const NATStatus& status() const { return status_; }

private:
    // ─── UPnP SSDP Discovery ───────────────────────────────
    bool discover_gateway();

    // ─── UPnP SOAP Actions ─────────────────────────────────
    bool add_port_mapping(const std::string& local_ip,
                          std::uint16_t internal_port,
                          std::uint16_t external_port);
    bool delete_port_mapping(std::uint16_t external_port);
    std::string get_external_ip_upnp();

    // ─── Public IP Discovery ───────────────────────────────
    std::string discover_public_ip();

    // ─── HTTP helpers (minimal, no external deps) ──────────
    std::string http_get(const std::string& host, std::uint16_t port,
                         const std::string& path);
    std::string http_post(const std::string& host, std::uint16_t port,
                          const std::string& path,
                          const std::string& body,
                          const std::string& soap_action);

    // ─── XML helpers (minimal string-based parsing) ────────
    std::string xml_find_tag(const std::string& xml, const std::string& tag);

    // ─── State ─────────────────────────────────────────────
    NATStatus status_;

    // UPnP gateway info
    std::string control_url_path_;    // e.g., "/ctl/IPConn"
    std::string gateway_host_;        // e.g., "192.168.1.1"
    std::uint16_t gateway_port_ = 0;  // e.g., 49152
    std::string service_type_;        // WANIPConnection or WANPPPConnection

    // Track if we created a mapping (so we can remove it)
    bool mapping_active_ = false;
    std::uint16_t mapped_port_ = 0;
};

} // namespace crux::nat
