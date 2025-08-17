#include <string>
#include <map>
#include <memory>
#include <cstring>
#include <iostream>
#include <mutex>
#include <vector>
#include <algorithm>

#ifdef _WIN32
  #define NOMINMAX
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socklen_t = int;
  #define SHUT_RDWR SD_BOTH
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  using SOCKET = int;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR (-1)
  #define closesocket(s) close(s)
  #define ioctlsocket ioctl
  #define SD_BOTH SHUT_RDWR
#endif

using std::string;

// »нициализаци¤ Winsock
struct WinsockInitializer {
  WinsockInitializer() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
  }
  ~WinsockInitializer() {
#ifdef _WIN32
    WSACleanup();
#endif
  }
};
static WinsockInitializer winsock_init;

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <ctime>
#include <string>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <set>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET socket_t;
  #define CLOSESOCKET closesocket
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <cstring>
  typedef int socket_t;
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
  #define CLOSESOCKET close
#endif

class t_server_api {
public:
    using on_client_connected_t = std::function<void(int client_id, socket_t clientSocket, const std::string& clientIp)>;
    using on_client_disconnected_t = std::function<void(int client_id)>;
    using on_client_data_t = std::function<void(int client_id, const std::string& data, std::function<void(const std::string&)>)>;

    // Публичные поля для удобства
    unsigned short port;
    std::atomic<int> client_count;
    socket_t serverSocket;
    std::thread workerThread;
    std::atomic<bool> isRunning;

    std::mutex clientsMutex;

    // Храним потоки клиентов, можно потом расширить управление клиентами
    std::vector<std::thread> clientThreads;

    // Карта: client_id → socket
    std::unordered_map<int, socket_t> clientSockets;

    // Карта: client_id → IP
    std::unordered_map<int, std::string> clientIps;

    // Для обратного поиска: IP → set client_id
    std::unordered_map<std::string, std::set<int>> ipToClientIds;

    // Коллбеки
    on_client_connected_t onClientConnected;
    on_client_disconnected_t onClientDisconnected;
    on_client_data_t onClientData;

    explicit t_server_api(unsigned short p)
        : port(p), client_count(0), serverSocket(INVALID_SOCKET), isRunning(false) {}

    ~t_server_api() {
        stop();
    }

    void start() {
#ifdef _WIN32
        WSADATA wsaData;
        int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (wsa_result != 0) {
            std::cerr << "WSAStartup failed: " << wsa_result << std::endl;
            return;
        }
#endif

        isRunning = true;

        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == INVALID_SOCKET) {
            std::cerr << "Error creating socket\n";
            cleanup();
            return;
        }

        int yes = 1;
#ifdef _WIN32
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
#else
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(serverSocket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed\n";
            cleanup();
            return;
        }

        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed\n";
            cleanup();
            return;
        }

        workerThread = std::thread([this]() { this->accept_loop(); });

        std::cout << "Server listening on port " << port << std::endl;
    }

    void stop() {
        if (!isRunning) return;

        isRunning = false;

        // Закрываем все клиентские сокеты
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto& cs : clientSockets) {
                CLOSESOCKET(cs.second);
            }
            clientSockets.clear();
            clientIps.clear();
            ipToClientIds.clear();
        }

        CLOSESOCKET(serverSocket);

        if (workerThread.joinable())
            workerThread.join();

#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Отключить все клиенты с данным IP
    void disconnect_clients_by_ip(const std::string& ip) {
        std::lock_guard<std::mutex> lock(clientsMutex);

        auto it = ipToClientIds.find(ip);
        if (it == ipToClientIds.end()) return;

        for (int client_id : it->second) {
            auto sock_it = clientSockets.find(client_id);
            if (sock_it != clientSockets.end()) {
                std::cout << "Disconnecting client #" << client_id << " with IP " << ip << std::endl;
                CLOSESOCKET(sock_it->second);
                // Сокет будет очищен при disconnect
            }
        }
        // Очистим записи о клиентах с этим IP, но реально очистка придет при отключении клиентов в client_handler
        // Можно очищать здесь подольше, но лучше доверять client_handler
    }

private:
    void accept_loop() {
        while (isRunning) {
            sockaddr_in client_addr{};
#ifdef _WIN32
            int client_len = sizeof(client_addr);
#else
            socklen_t client_len = sizeof(client_addr);
#endif
            socket_t client_socket = accept(serverSocket, (sockaddr*)&client_addr, &client_len);
            if (client_socket == INVALID_SOCKET) {
                if (isRunning) {
                    std::cerr << "Accept failed\n";
                }
                break;
            }
            int client_id = ++client_count;

            // Получаем IP клиента
            char ip_str[INET_ADDRSTRLEN] = {0};
#ifdef _WIN32
            InetNtopA(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
#else
            inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
#endif
            std::string client_ip(ip_str);

            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clientSockets[client_id] = client_socket;
                clientIps[client_id] = client_ip;
                ipToClientIds[client_ip].insert(client_id);
            }

            // Коллбек после подключения с IP и сокетом
            if (onClientConnected) {
                try {
                    onClientConnected(client_id, client_socket, client_ip);
                } catch (...) {}
            }

            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clientThreads.emplace_back(&t_server_api::client_handler, this, client_socket, client_id);
                clientThreads.back().detach();
            }
        }
    }

    void client_handler(socket_t client_socket, int client_id) {
        constexpr int buf_size = 1024;
        char buffer[buf_size];

        auto send_func = [client_socket](const std::string& msg) {
            ::send(client_socket, msg.c_str(), static_cast<int>(msg.size()), 0);
        };

        while (isRunning) {
            int len = recv(client_socket, buffer, buf_size, 0);
            if (len <= 0) break;

            std::string data(buffer, len);

            if (onClientData) {
                try {
                    onClientData(client_id, data, send_func);
                } catch (...) {}
            }
        }

        CLOSESOCKET(client_socket);

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            // Очищаем данные клиента
            auto it = clientIps.find(client_id);
            if (it != clientIps.end()) {
                const std::string& ip = it->second;
                clientIps.erase(it);

                auto ipIt = ipToClientIds.find(ip);
                if (ipIt != ipToClientIds.end()) {
                    ipIt->second.erase(client_id);
                    if (ipIt->second.empty()) {
                        ipToClientIds.erase(ipIt);
                    }
                }
            }

            clientSockets.erase(client_id);
        }

        if (onClientDisconnected) {
            try {
                onClientDisconnected(client_id);
            } catch (...) {}
        }
    }

    void cleanup() {
        isRunning = false;
        if (serverSocket != INVALID_SOCKET) {
            CLOSESOCKET(serverSocket);
            serverSocket = INVALID_SOCKET;
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }
};


// -------------------------------------------
// Пример использования с новым коллбеком onClientConnected
// -------------------------------------------

int main2() {
    t_server_api server(31456);

    server.onClientConnected = [](int client_id, socket_t socket, const std::string& ip) {
        std::cout << "[" << client_id << "] connected from IP " << ip << "\n";
        std::string greeting = "Hello client #" + std::to_string(client_id) + " from " + ip + "\n";
        send(socket, greeting.c_str(), static_cast<int>(greeting.size()), 0);
    };

    server.onClientDisconnected = [](int client_id) {
        std::cout << "[" << client_id << "] disconnected\n";
    };

    server.onClientData = [&](int client_id, const std::string& data, std::function<void(const std::string&)> send) {
        std::cout << "[" << client_id << "] received: " << data;
        if (data.find("bye") != std::string::npos) {
            std::cout << "Client " << client_id << " said bye, disconnecting...\n";

            // Чтобы отключить, нужно знать IP этого клиента
            // Получим IP через класс сервера (для демонстрации)
            // В реальности в коллбеке можно хранить ip по client_id уникально
            // Здесь показан только принцип — для реального применения вынести handler в класс
            // Например:
            std::lock_guard<std::mutex> lock(server.clientsMutex);
            auto client_ip=server.clientIps[client_id];
            server.disconnect_clients_by_ip(client_ip);

            // Для примера отключим этого клиента по IP
            // Для этого нужно дать доступ к серверу, либо вызвать disconnect_clients_by_ip где-то отдельно
            // Здесь демонстрация вызова (в main нет доступа к server внутри лямбды здесь)
        }

        // Отвечаем клиенту
        send("Message received\n");
    };

    server.start();

    std::cout << "Press Enter to stop server...\n";
    std::string dummy;
    std::getline(std::cin, dummy);

    // Например, демонстрация: отключить всех клиентов с IP "127.0.0.1"
    std::cout << "Disconnecting clients from IP 127.0.0.1\n";
    server.disconnect_clients_by_ip("127.0.0.1");

    server.stop();

    return 0;
}

const int SOCKET_TIMEOUT_MS = 60*60*1000;
const int BUFFER_SIZE = 64*1024;

std::pair<string, int> parse_endpoint(const string& endpoint) {
  auto pos = endpoint.find(':');
  if (pos == string::npos) return {endpoint, 80};
  return {endpoint.substr(0, pos), std::stoi(endpoint.substr(pos + 1))};
}

class Socket {
  SOCKET sock;
  bool valid;
  std::vector<char> buffer;
  size_t buffer_offset;
  size_t buffer_size;  // реальное количество данных в буфере
  mutable std::mutex io_mutex;

  bool wait_readable(int timeout_ms) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(static_cast<unsigned int>(sock), &read_set);

    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(static_cast<int>(sock + 1), &read_set, nullptr, nullptr, &tv);
    return result > 0;
  }

  bool wait_writable(int timeout_ms) {
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(static_cast<unsigned int>(sock), &write_set);

    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(static_cast<int>(sock + 1), nullptr, &write_set, nullptr, &tv);
    return result > 0;
  }

  bool fill_buffer(int timeout_ms) {
    if (!wait_readable(timeout_ms)) return false;

    int n = ::recv(sock, buffer.data(), static_cast<int>(buffer.size()), 0);
    if (n <= 0) return false;

    buffer_offset = 0;
    buffer_size = static_cast<size_t>(n);  // ? ”читываем реальный размер
    return true;
  }

public:
  Socket(SOCKET s) : sock(s), valid(s != INVALID_SOCKET), buffer(BUFFER_SIZE), buffer_offset(0), buffer_size(0) {}

  ~Socket() {
    if (valid) {
      shutdown(sock, SD_BOTH);
      closesocket(sock);
    }
  }

  bool readline(string& line, int timeout_ms = SOCKET_TIMEOUT_MS) {
    std::lock_guard<std::mutex> lock(io_mutex);
    line.clear();

    while (true) {
      if (buffer_offset >= buffer_size) {
        if (!fill_buffer(timeout_ms)) return false;
      }

      size_t available = buffer_size - buffer_offset;
      char* start = buffer.data() + buffer_offset;

      char* newline = static_cast<char*>(std::memchr(start, '\n', available));
      if (newline) {
        size_t len = newline - start;
        if (newline > start && *(newline - 1) == '\r') {
          line.append(start, len - 1);
        } else {
          line.append(start, len);
        }
        buffer_offset += len + 1;  // пропускаем \n
        return true;
      }

      // Ќет \n Ч копируем всЄ, кроме \r
      for (size_t i = 0; i < available; ++i) {
        if (start[i] != '\r') line += start[i];
      }
      buffer_offset = buffer_size;  // весь буфер обработан
    }
  }

  bool write(const string& data, int timeout_ms = SOCKET_TIMEOUT_MS) {
    std::lock_guard<std::mutex> lock(io_mutex);
    if (!valid || data.empty()) return false;

    const char* ptr = data.data();
    size_t len = data.size();
    char nl = '\n';

    while (len > 0) {
      if (!wait_writable(timeout_ms)) return false;
      int sent = ::send(sock, ptr, static_cast<int>(len), 0);
      if (sent <= 0) return false;
      ptr += sent;
      len -= sent;
    }

    if (!wait_writable(timeout_ms)) return false;
    return ::send(sock, &nl, 1, 0) == 1;
  }

  static std::shared_ptr<Socket> connect_to(const string& endpoint, int timeout_ms = SOCKET_TIMEOUT_MS) {
    auto [host, port] = parse_endpoint(endpoint);

    struct addrinfo hints{}, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    string port_str = std::to_string(port);
    if (::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
      return nullptr;
    }

    SOCKET sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
      freeaddrinfo(res);
      return nullptr;
    }

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    int conn_result = ::connect(sock, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen));
    bool connected = false;

    if (conn_result == 0) {
      connected = true;
    }
#ifdef _WIN32
    else if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
    else if (errno == EINPROGRESS)
#endif
    {
      if (wait_writable_for_socket(sock, timeout_ms)) {
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len);
        if (error == 0) connected = true;
      }
    }

    freeaddrinfo(res);

    if (connected) {
#ifdef _WIN32
      u_long mode = 0;
      ioctlsocket(sock, FIONBIO, &mode);
#else
      int flags = fcntl(sock, F_GETFL, 0);
      fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif
      return std::shared_ptr<Socket>(new Socket(sock));
    }

    closesocket(sock);
    return nullptr;
  }

private:
  static bool wait_writable_for_socket(SOCKET s, int timeout_ms) {
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(static_cast<unsigned int>(s), &write_set);
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return select(static_cast<int>(s + 1), nullptr, &write_set, nullptr, &tv) > 0;
  }
};

struct t_net_api {
  //string main_socket = "82.208.124.191:80";
  std::map<string, std::shared_ptr<Socket>> sockets;
  std::mutex mtx;

  bool readline_from_socket(const string& endpoint, string& line) {
    std::shared_ptr<Socket> sock;

    {
      std::lock_guard<std::mutex> lock(mtx);
      auto it = sockets.find(endpoint);
      if (it != sockets.end()) {
        sock = it->second;
      }
    }

    if (!sock) {
      auto new_sock = Socket::connect_to(endpoint);
      if (!new_sock) return false;

      std::lock_guard<std::mutex> lock(mtx);
      auto& existing = sockets[endpoint];
      if (existing) {
        sock = existing;  // кто-то уже добавил
      } else {
        sockets[endpoint] = new_sock;
        sock = std::move(new_sock);
      }
    }

    return sock->readline(line);
  }

  void write_to_socket(const string& endpoint, const string& data) {
    std::shared_ptr<Socket> sock;

    {
      std::lock_guard<std::mutex> lock(mtx);
      auto it = sockets.find(endpoint);
      if (it != sockets.end()) {
        sock = it->second;
      }
    }

    if (!sock) {
      auto new_sock = Socket::connect_to(endpoint);
      if (!new_sock) return;

      std::lock_guard<std::mutex> lock(mtx);
      auto& existing = sockets[endpoint];
      if (existing) {
        sock = existing;
      } else {
        sockets[endpoint] = new_sock;
        sock = std::move(new_sock);
      }
    }

    sock->write(data);
  }

  void close_socket(const string& endpoint) {
    std::lock_guard<std::mutex> lock(mtx);
    sockets.erase(endpoint);
  }

  void close_all_sockets() {
    std::lock_guard<std::mutex> lock(mtx);
    sockets.clear();
  }
};
/*
---
2025.08.07 22:55:24.868
*/