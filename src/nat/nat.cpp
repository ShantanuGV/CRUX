#include "crux/nat/nat.hpp"
#include "crux/platform/socket.hpp"

#include <sstream>
#include <algorithm>
#include <cstring>

namespace crux::nat {

// ═══════════════════════════════════════════════════════════════
//  Constants
// ═══════════════════════════════════════════════════════════════

static constexpr const char* SSDP_MULTICAST = "239.255.255.250";
static constexpr std::uint16_t SSDP_PORT     = 1900;
static constexpr int SSDP_TIMEOUT_MS         = 3000;
static constexpr int HTTP_TIMEOUT_MS         = 5000;

// UPnP service types to search for (in priority order)
static const char* UPNP_SERVICE_TYPES[] = {
    "urn:schemas-upnp-org:service:WANIPConnection:1",
    "urn:schemas-upnp-org:service:WANPPPConnection:1",
    "urn:schemas-upnp-org:service:WANIPConnection:2",
};
static constexpr int NUM_SERVICE_TYPES = 3;

// ═══════════════════════════════════════════════════════════════
//  Lifecycle
// ═══════════════════════════════════════════════════════════════

NATTraversal::~NATTraversal() {
    cleanup();
}

NATStatus NATTraversal::setup(std::uint16_t internal_port) {
    status_ = NATStatus{};

    // 1. Discover local IP
    status_.local_ip = platform::get_local_ip();

    // 2. Discover public IP via HTTP
    status_.public_ip = discover_public_ip();

    // 3. Try UPnP gateway discovery
    if (discover_gateway()) {
        status_.upnp_available = true;
        status_.gateway_ip = gateway_host_;

        // 4. Try to get external IP from the gateway
        std::string upnp_ip = get_external_ip_upnp();
        if (!upnp_ip.empty()) {
            status_.public_ip = upnp_ip;
        }

        // 5. Add port mapping
        if (add_port_mapping(status_.local_ip, internal_port, internal_port)) {
            status_.port_forwarded = true;
            status_.external_port = internal_port;
            mapping_active_ = true;
            mapped_port_ = internal_port;
        }
    }

    return status_;
}

void NATTraversal::cleanup() {
    if (mapping_active_ && !gateway_host_.empty()) {
        delete_port_mapping(mapped_port_);
        mapping_active_ = false;
    }
}

// ═══════════════════════════════════════════════════════════════
//  UPnP SSDP Discovery
// ═══════════════════════════════════════════════════════════════

bool NATTraversal::discover_gateway() {
    // Create UDP socket for SSDP
    auto sock = platform::socket_create_udp();
    if (!platform::socket_valid(sock)) return false;

    platform::socket_set_broadcast(sock);

    // Bind to any port
    // (needed on some systems to receive responses)
#ifdef _WIN32
    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = 0;
    ::bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));
#else
    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = 0;
    ::bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr));
#endif

    // Try each UPnP service type
    for (int st = 0; st < NUM_SERVICE_TYPES; ++st) {
        std::string search =
            "M-SEARCH * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
            "MAN: \"ssdp:discover\"\r\n"
            "MX: 3\r\n"
            "ST: " + std::string(UPNP_SERVICE_TYPES[st]) + "\r\n"
            "\r\n";

        platform::socket_sendto(sock, search.data(), search.size(),
                                SSDP_MULTICAST, SSDP_PORT);
    }

    // Also search for the generic IGD device
    std::string igd_search =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
        "\r\n";
    platform::socket_sendto(sock, igd_search.data(), igd_search.size(),
                            SSDP_MULTICAST, SSDP_PORT);

    // Collect responses
    char buf[4096];
    std::string location_url;

    for (int attempt = 0; attempt < 5; ++attempt) {
        std::string from_addr;
        std::uint16_t from_port = 0;
        int n = platform::socket_recvfrom(sock, buf, sizeof(buf) - 1,
                                          from_addr, from_port, SSDP_TIMEOUT_MS);
        if (n <= 0) break;
        buf[n] = '\0';
        std::string response(buf);

        // Find LOCATION header
        auto loc_pos = response.find("LOCATION:");
        if (loc_pos == std::string::npos) {
            loc_pos = response.find("Location:");
        }
        if (loc_pos == std::string::npos) {
            loc_pos = response.find("location:");
        }
        if (loc_pos == std::string::npos) continue;

        auto line_end = response.find("\r\n", loc_pos);
        if (line_end == std::string::npos) line_end = response.find("\n", loc_pos);
        if (line_end == std::string::npos) continue;

        std::string loc_line = response.substr(loc_pos, line_end - loc_pos);
        auto http_pos = loc_line.find("http://");
        if (http_pos == std::string::npos) continue;

        location_url = loc_line.substr(http_pos);
        // Trim whitespace
        while (!location_url.empty() && (location_url.back() == ' ' ||
               location_url.back() == '\r' || location_url.back() == '\n')) {
            location_url.pop_back();
        }
        break;
    }

    platform::socket_close(sock);

    if (location_url.empty()) {
        status_.error = "No UPnP gateway found on the network";
        return false;
    }

    // Parse the location URL: http://host:port/path
    // Skip "http://"
    std::string url = location_url.substr(7);
    auto slash_pos = url.find('/');
    std::string host_port = (slash_pos != std::string::npos) ? url.substr(0, slash_pos) : url;
    std::string path = (slash_pos != std::string::npos) ? url.substr(slash_pos) : "/";

    auto colon_pos = host_port.find(':');
    if (colon_pos != std::string::npos) {
        gateway_host_ = host_port.substr(0, colon_pos);
        try {
            gateway_port_ = static_cast<std::uint16_t>(
                std::stoi(host_port.substr(colon_pos + 1)));
        } catch (...) {
            gateway_port_ = 80;
        }
    } else {
        gateway_host_ = host_port;
        gateway_port_ = 80;
    }

    // Fetch the device description XML
    std::string xml = http_get(gateway_host_, gateway_port_, path);
    if (xml.empty()) {
        status_.error = "Failed to fetch UPnP device description";
        return false;
    }

    // Find the WANIPConnection or WANPPPConnection service control URL
    for (int st = 0; st < NUM_SERVICE_TYPES; ++st) {
        std::string svc_type = UPNP_SERVICE_TYPES[st];

        // Look for the service type in the XML
        auto svc_pos = xml.find(svc_type);
        if (svc_pos == std::string::npos) continue;

        // Find the controlURL after the service type
        auto ctrl_pos = xml.find("<controlURL>", svc_pos);
        if (ctrl_pos == std::string::npos) continue;
        ctrl_pos += 12; // skip "<controlURL>"

        auto ctrl_end = xml.find("</controlURL>", ctrl_pos);
        if (ctrl_end == std::string::npos) continue;

        control_url_path_ = xml.substr(ctrl_pos, ctrl_end - ctrl_pos);
        service_type_ = svc_type;

        // Ensure path starts with /
        if (!control_url_path_.empty() && control_url_path_[0] != '/') {
            control_url_path_ = "/" + control_url_path_;
        }

        return true;
    }

    status_.error = "UPnP gateway found but no WAN connection service";
    return false;
}

// ═══════════════════════════════════════════════════════════════
//  UPnP SOAP Actions
// ═══════════════════════════════════════════════════════════════

bool NATTraversal::add_port_mapping(const std::string& local_ip,
                                     std::uint16_t internal_port,
                                     std::uint16_t external_port) {
    std::string body =
        "<?xml version=\"1.0\"?>\r\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
        "<s:Body>\r\n"
        "<u:AddPortMapping xmlns:u=\"" + service_type_ + "\">\r\n"
        "<NewRemoteHost></NewRemoteHost>\r\n"
        "<NewExternalPort>" + std::to_string(external_port) + "</NewExternalPort>\r\n"
        "<NewProtocol>TCP</NewProtocol>\r\n"
        "<NewInternalPort>" + std::to_string(internal_port) + "</NewInternalPort>\r\n"
        "<NewInternalClient>" + local_ip + "</NewInternalClient>\r\n"
        "<NewEnabled>1</NewEnabled>\r\n"
        "<NewPortMappingDescription>CRUX P2P Messenger</NewPortMappingDescription>\r\n"
        "<NewLeaseDuration>0</NewLeaseDuration>\r\n"
        "</u:AddPortMapping>\r\n"
        "</s:Body>\r\n"
        "</s:Envelope>\r\n";

    std::string action = service_type_ + "#AddPortMapping";
    std::string response = http_post(gateway_host_, gateway_port_,
                                     control_url_path_, body, action);

    if (response.empty()) {
        status_.error = "Failed to send UPnP port mapping request";
        return false;
    }

    // Check for success (HTTP 200) or error
    if (response.find("200 OK") != std::string::npos ||
        response.find("200 ok") != std::string::npos) {
        return true;
    }

    // Check for "ConflictInMappingEntry" — port already mapped (possibly by us)
    if (response.find("ConflictInMappingEntry") != std::string::npos ||
        response.find("718") != std::string::npos) {
        // Try to delete and re-add
        delete_port_mapping(external_port);
        std::string retry = http_post(gateway_host_, gateway_port_,
                                      control_url_path_, body, action);
        if (retry.find("200") != std::string::npos) return true;
    }

    status_.error = "UPnP port mapping failed (router may not allow it)";
    return false;
}

bool NATTraversal::delete_port_mapping(std::uint16_t external_port) {
    std::string body =
        "<?xml version=\"1.0\"?>\r\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
        "<s:Body>\r\n"
        "<u:DeletePortMapping xmlns:u=\"" + service_type_ + "\">\r\n"
        "<NewRemoteHost></NewRemoteHost>\r\n"
        "<NewExternalPort>" + std::to_string(external_port) + "</NewExternalPort>\r\n"
        "<NewProtocol>TCP</NewProtocol>\r\n"
        "</u:DeletePortMapping>\r\n"
        "</s:Body>\r\n"
        "</s:Envelope>\r\n";

    std::string action = service_type_ + "#DeletePortMapping";
    std::string response = http_post(gateway_host_, gateway_port_,
                                     control_url_path_, body, action);
    return response.find("200") != std::string::npos;
}

std::string NATTraversal::get_external_ip_upnp() {
    std::string body =
        "<?xml version=\"1.0\"?>\r\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
        "<s:Body>\r\n"
        "<u:GetExternalIPAddress xmlns:u=\"" + service_type_ + "\">\r\n"
        "</u:GetExternalIPAddress>\r\n"
        "</s:Body>\r\n"
        "</s:Envelope>\r\n";

    std::string action = service_type_ + "#GetExternalIPAddress";
    std::string response = http_post(gateway_host_, gateway_port_,
                                     control_url_path_, body, action);

    if (response.empty()) return "";

    return xml_find_tag(response, "NewExternalIPAddress");
}

// ═══════════════════════════════════════════════════════════════
//  Public IP Discovery (HTTP fallback)
// ═══════════════════════════════════════════════════════════════

std::string NATTraversal::discover_public_ip() {
    // Try multiple services in case one is down
    struct IPService {
        const char* host;
        std::uint16_t port;
        const char* path;
    };
    static const IPService services[] = {
        {"api.ipify.org",     80, "/"},
        {"ifconfig.me",       80, "/ip"},
        {"icanhazip.com",     80, "/"},
        {"checkip.amazonaws.com", 80, "/"},
    };

    for (const auto& svc : services) {
        std::string response = http_get(svc.host, svc.port, svc.path);
        if (response.empty()) continue;

        // Extract body (after \r\n\r\n)
        auto body_start = response.find("\r\n\r\n");
        std::string body;
        if (body_start != std::string::npos) {
            body = response.substr(body_start + 4);
        } else {
            body = response;
        }

        // Trim whitespace
        while (!body.empty() && (body.front() == ' ' || body.front() == '\r' ||
               body.front() == '\n' || body.front() == '\t')) {
            body.erase(body.begin());
        }
        while (!body.empty() && (body.back() == ' ' || body.back() == '\r' ||
               body.back() == '\n' || body.back() == '\t')) {
            body.pop_back();
        }

        // Basic validation: should look like an IP address
        if (!body.empty() && body.size() <= 45 &&
            body.find_first_of("0123456789.") != std::string::npos) {
            return body;
        }
    }

    return "";
}

// ═══════════════════════════════════════════════════════════════
//  Minimal HTTP Client (no external dependencies)
// ═══════════════════════════════════════════════════════════════

std::string NATTraversal::http_get(const std::string& host, std::uint16_t port,
                                    const std::string& path) {
    auto sock = platform::socket_create();
    if (!platform::socket_valid(sock)) return "";

    if (!platform::socket_connect(sock, host, port, HTTP_TIMEOUT_MS)) {
        platform::socket_close(sock);
        return "";
    }

    std::string request =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "Connection: close\r\n"
        "User-Agent: CRUX/1.0\r\n"
        "\r\n";

    platform::socket_send(sock, request.data(), request.size());

    // Read response
    std::string response;
    char buf[4096];
    while (true) {
        int ready = platform::socket_poll_read(sock, HTTP_TIMEOUT_MS);
        if (ready <= 0) break;
        int n = platform::socket_recv(sock, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        response += buf;
        // Stop if we've received the full response
        if (response.size() > 32768) break;
    }

    platform::socket_close(sock);
    return response;
}

std::string NATTraversal::http_post(const std::string& host, std::uint16_t port,
                                     const std::string& path,
                                     const std::string& body,
                                     const std::string& soap_action) {
    auto sock = platform::socket_create();
    if (!platform::socket_valid(sock)) return "";

    if (!platform::socket_connect(sock, host, port, HTTP_TIMEOUT_MS)) {
        platform::socket_close(sock);
        return "";
    }

    std::string request =
        "POST " + path + " HTTP/1.1\r\n"
        "Host: " + host + ":" + std::to_string(port) + "\r\n"
        "Content-Type: text/xml; charset=\"utf-8\"\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "SOAPAction: \"" + soap_action + "\"\r\n"
        "Connection: close\r\n"
        "\r\n" + body;

    platform::socket_send(sock, request.data(), request.size());

    // Read response
    std::string response;
    char buf[4096];
    while (true) {
        int ready = platform::socket_poll_read(sock, HTTP_TIMEOUT_MS);
        if (ready <= 0) break;
        int n = platform::socket_recv(sock, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        response += buf;
        if (response.size() > 32768) break;
    }

    platform::socket_close(sock);
    return response;
}

// ═══════════════════════════════════════════════════════════════
//  XML Helper
// ═══════════════════════════════════════════════════════════════

std::string NATTraversal::xml_find_tag(const std::string& xml,
                                        const std::string& tag) {
    // Search for <tag> or <ns:tag>
    std::string open1 = "<" + tag + ">";
    std::string close1 = "</" + tag + ">";

    auto pos = xml.find(open1);
    if (pos != std::string::npos) {
        pos += open1.size();
        auto end = xml.find(close1, pos);
        if (end != std::string::npos) {
            return xml.substr(pos, end - pos);
        }
    }

    // Try with any namespace prefix: <*:tag>
    std::string pattern = ":" + tag + ">";
    pos = xml.find(pattern);
    if (pos != std::string::npos) {
        pos += pattern.size();
        // Find closing tag with same or any prefix
        auto end = xml.find("</" , pos);
        if (end != std::string::npos) {
            return xml.substr(pos, end - pos);
        }
    }

    return "";
}

} // namespace crux::nat
