#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace {
std::mutex startupMutex;
int serverPort = 0;
std::string assetRoot;
std::atomic<int> clients {0};
bool Send(int fd, const char *bytes, size_t count)
{
    while (count) {
        ssize_t sent = send(fd, bytes, count, MSG_NOSIGNAL);
        if (sent <= 0) { if (errno == EINTR) continue; return false; }
        bytes += sent; count -= static_cast<size_t>(sent);
    }
    return true;
}
const char *Mime(const std::string &path)
{
    const auto dot = path.find_last_of('.');
    const auto extension = dot == std::string::npos ? "" : path.substr(dot);
    if (extension == ".js" || extension == ".mjs") return "text/javascript; charset=utf-8";
    if (extension == ".html") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".json" || extension == ".map") return "application/json";
    if (extension == ".wasm") return "application/wasm";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    if (extension == ".ttf") return "font/ttf";
    if (extension == ".woff") return "font/woff";
    if (extension == ".woff2") return "font/woff2";
    return "application/octet-stream";
}
void Serve(int fd)
{
    timeval timeout {10, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    std::string request;
    char buffer[16384];
    while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16384) {
        ssize_t size = recv(fd, buffer, sizeof(buffer), 0);
        if (size <= 0) break;
        request.append(buffer, static_cast<size_t>(size));
    }
    auto fail = [&](const char *status) {
        std::string response = std::string("HTTP/1.1 ") + status + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        Send(fd, response.data(), response.size());
    };
    bool head = request.rfind("HEAD ", 0) == 0;
    if (!head && request.rfind("GET ", 0) != 0) { fail("405 Method Not Allowed"); close(fd); --clients; return; }
    size_t start = head ? 5 : 4;
    size_t end = request.find(' ', start);
    std::string encoded = request.substr(start, end - start);
    encoded = encoded.substr(0, encoded.find_first_of("?#"));
    std::string path;
    bool valid = !encoded.empty() && encoded.front() == '/';
    for (size_t i = 0; i < encoded.size() && valid; ++i) {
        unsigned char value = encoded[i];
        if (value == '%') {
            if (i + 2 >= encoded.size()) { valid = false; break; }
            auto hex = [](char c) -> int { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; };
            int a = hex(encoded[i+1]), b = hex(encoded[i+2]);
            if (a < 0 || b < 0) { valid = false; break; }
            value = static_cast<unsigned char>((a << 4) | b); i += 2;
        }
        if (!value || value == '\\' || value < 32) { valid = false; break; }
        path.push_back(value);
    }
    if (path == "/") path = "/index.html";
    char canonical[PATH_MAX];
    std::string resolved;
    if (valid && realpath((assetRoot + path).c_str(), canonical)) resolved = canonical;
    struct stat info {};
    if (resolved.rfind(assetRoot + "/", 0) != 0 || stat(resolved.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
        fail("404 Not Found"); close(fd); --clients; return;
    }
    FILE *file = fopen(resolved.c_str(), "rb");
    if (!file) { fail("404 Not Found"); close(fd); --clients; return; }
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + std::string(Mime(resolved)) +
        "\r\nContent-Length: " + std::to_string(info.st_size) +
        "\r\nConnection: close\r\nCache-Control: no-cache\r\nX-Content-Type-Options: nosniff"
        "\r\nCross-Origin-Opener-Policy: same-origin\r\nCross-Origin-Embedder-Policy: require-corp"
        "\r\nCross-Origin-Resource-Policy: cross-origin\r\nAccess-Control-Allow-Origin: *\r\nService-Worker-Allowed: /\r\n\r\n";
    if (Send(fd, header.data(), header.size()) && !head) {
        size_t size;
        while ((size = fread(buffer, 1, sizeof(buffer), file)) > 0 && Send(fd, buffer, size)) {}
    }
    fclose(file); close(fd); --clients;
}
}

// Static, read-only assets only. Never expose workspace files or a command HTTP API.
extern "C" int ArkForgeStartWorkbenchServer(const char *root)
{
    std::lock_guard<std::mutex> lock(startupMutex);
    char canonical[PATH_MAX];
    if (!realpath(root, canonical)) return -1;
    if (serverPort) return assetRoot == canonical ? serverPort : -1;
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) { close(fd); return -1; }
    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(18712);
    if (bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) || listen(fd, 32)) { close(fd); return -1; }
    socklen_t length = sizeof(address);
    if (getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length)) { close(fd); return -1; }
    assetRoot = canonical; serverPort = ntohs(address.sin_port);
    std::thread([fd]() {
        for (;;) {
            int client = accept4(fd, nullptr, nullptr, SOCK_CLOEXEC);
            if (client < 0) { if (errno == EINTR) continue; break; }
            if (++clients > 48) { --clients; close(client); continue; }
            std::thread(Serve, client).detach();
        }
        close(fd);
    }).detach();
    return serverPort;
}
