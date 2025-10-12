// socket_adapter.cpp
// Компиляция:
//   Linux/macOS: g++ -std=c++17 -O2 socket_adapter.cpp -o socket_adapter
//   Windows:     cl /EHsc /std:c++17 socket_adapter.cpp /link ws2_32.lib

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <set>
#include <stdexcept>
#include <atomic>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    using ssize_t = int;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/wait.h>
    #include <errno.h>
    using socket_t = int;
    const int INVALID_SOCKET = -1;
    const int SOCKET_ERROR = -1;
#endif

void throw_system_error(const std::string& msg) {
#ifdef _WIN32
    throw std::runtime_error(msg + " (WSAGetLastError: " + std::to_string(WSAGetLastError()) + ")");
#else
    throw std::runtime_error(msg + " (errno: " + std::to_string(errno) + ")");
#endif
}

std::pair<std::string, int> parse_address(const std::string& addr) {
    size_t pos = addr.find_last_of(':');
    if (pos == std::string::npos || pos == 0) {
        throw std::invalid_argument("Invalid address format. Expected 'host:port'");
    }
    std::string host = addr.substr(0, pos);
    int port = std::stoi(addr.substr(pos + 1));
    if (port <= 0 || port > 65535) {
        throw std::invalid_argument("Port must be between 1 and 65535");
    }
    return {host, port};
}

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS] <host:port> <program> [args...]\n"
              << "\n"
              << "Options:\n"
              << "  -e, --no-stderr    Do not forward child's stderr to socket\n"
              << "  -h, --help         Show this help\n"
              << "\n"
              << "Example:\n"
              << "  " << prog << " 127.0.0.1:12345 ./QapLR world 4 1 2\n";
}

// ================ PLATFORM-SPECIFIC COPY FUNCTIONS ================

#ifdef _WIN32

void copy_pipe_to_socket(HANDLE pipe_read, socket_t sock) {
    char buf[4096];
    DWORD bytes_read;
    while (ReadFile(pipe_read, buf, sizeof(buf), &bytes_read, nullptr) && bytes_read > 0) {
        int sent = 0;
        while (sent < static_cast<int>(bytes_read)) {
            int res = send(sock, buf + sent, bytes_read - sent, 0);
            if (res <= 0) return;
            sent += res;
        }
    }
}

void copy_socket_to_pipe(socket_t sock, HANDLE pipe_write) {
    char buf[4096];
    int received;
    while ((received = recv(sock, buf, sizeof(buf), 0)) > 0) {
        DWORD written = 0;
        while (written < static_cast<DWORD>(received)) {
            DWORD dw;
            if (!WriteFile(pipe_write, buf + written, received - written, &dw, nullptr)) break;
            written += dw;
        }
    }
}

#else // Unix-like

void copy_fd_to_socket(int fd, socket_t sock) {
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t sent = 0;
        while (sent < n) {
            ssize_t res = write(sock, buf + sent, n - sent);
            if (res <= 0) return;
            sent += res;
        }
    }
}

void copy_socket_to_fd(socket_t sock, int fd) {
    char buf[4096];
    ssize_t n;
    while ((n = read(sock, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t res = write(fd, buf + written, n - written);
            if (res <= 0) return;
            written += res;
        }
    }
}

#endif

// ================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    bool forward_stderr = true;
    int arg_start = 1;
    std::set<std::string> known_flags = {"-e", "--no-stderr", "-h", "--help"};

    while (arg_start < argc) {
        std::string arg = argv[arg_start];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-e" || arg == "--no-stderr") {
            forward_stderr = false;
            ++arg_start;
        } else if (arg[0] == '-' && known_flags.count(arg) == 0) {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        } else if (arg[0] != '-') {
            break;
        } else {
            break;
        }
    }

    if (arg_start + 1 >= argc) {
        std::cerr << "Missing <host:port> and/or <program>\n";
        print_help(argv[0]);
        return 1;
    }

    std::string addr_str = argv[arg_start];
    std::vector<std::string> prog_args(argv + arg_start + 1, argv + argc);

    auto [host, port] = parse_address(addr_str);

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw_system_error("WSAStartup failed");
    }
#endif

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) throw_system_error("socket() failed");

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
        reinterpret_cast<const char*>(&yes), sizeof(yes)
#else
        &yes, sizeof(yes)
#endif
    );

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        throw_system_error("Invalid IP address: " + host);
    }

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        throw_system_error("connect() failed");
    }

    // Отключаем алгоритм Нейгла
    int nodelay = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
#ifdef _WIN32
        reinterpret_cast<const char*>(&nodelay), sizeof(nodelay)
#else
        &nodelay, sizeof(nodelay)
#endif
    );

    std::cout << "Connected to " << host << ":" << port << "\n";

#ifdef _WIN32
    // === Windows: CreateProcess + Pipes ===
    HANDLE stdin_read = nullptr, stdin_write = nullptr;
    HANDLE stdout_read = nullptr, stdout_write = nullptr;
    HANDLE stderr_read = nullptr, stderr_write = nullptr;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0) ||
        !CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        throw std::runtime_error("CreatePipe failed for stdin/stdout");
    }

    if (forward_stderr) {
        if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
            throw std::runtime_error("CreatePipe failed for stderr");
        }
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = forward_stderr ? stderr_write : GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {};

    // Собираем командную строку
    std::string cmd_line;
    for (size_t i = 0; i < prog_args.size(); ++i) {
        if (i > 0) cmd_line += " ";
        std::string arg = prog_args[i];
        if (arg.find_first_of(" \t\n\"") != std::string::npos) {
            size_t pos = 0;
            while ((pos = arg.find('"', pos)) != std::string::npos) {
                arg.insert(pos, "\\");
                pos += 2;
            }
            cmd_line += "\"" + arg + "\"";
        } else {
            cmd_line += arg;
        }
    }

    if (!CreateProcessA(
        prog_args[0].c_str(),
        cmd_line.empty() ? nullptr : &cmd_line[0],
        nullptr, nullptr, TRUE, 0, nullptr, nullptr,
        &si, &pi)) {
        throw std::runtime_error("CreateProcess failed");
    }

    // Закрываем ненужные хэндлы в родителе
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    if (forward_stderr) CloseHandle(stderr_write);

    std::atomic<bool> child_exited{false};

    // Поток наблюдения за дочерним процессом
    std::thread monitor_thread([sock, pi, &child_exited]() {
        WaitForSingleObject(pi.hProcess, INFINITE);
        child_exited = true;
        // Принудительно закрываем сокет, чтобы разблокировать recv
        closesocket(sock);
    });

    // Запускаем потоки
    std::thread t_stdout([stdout_read, sock]() {
        copy_pipe_to_socket(stdout_read, sock);
    });

    std::thread t_stderr;
    if (forward_stderr) {
        t_stderr = std::thread([stderr_read, sock]() {
            copy_pipe_to_socket(stderr_read, sock);
        });
    }

    // Основной поток: сокет → stdin
    copy_socket_to_pipe(sock, stdin_write);

    // Ждём завершения всех потоков
    t_stdout.join();
    if (forward_stderr) t_stderr.join();
    monitor_thread.join(); // ← дожидаемся монитора
    
    // Очистка
    CloseHandle(stdin_write);
    CloseHandle(stdout_read);
    if (forward_stderr) CloseHandle(stderr_read);
    closesocket(sock);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    WSACleanup();
    return static_cast<int>(exit_code);

#else // Unix-like
    // === Unix: fork + dup2 ===
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
        throw std::runtime_error("pipe() failed");
    }
    if (forward_stderr && pipe(stderr_pipe) != 0) {
        throw std::runtime_error("pipe() failed for stderr");
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        if (forward_stderr) close(stderr_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        if (forward_stderr) {
            dup2(stderr_pipe[1], STDERR_FILENO);
        }

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        if (forward_stderr) close(stderr_pipe[1]);

        std::vector<const char*> c_args;
        for (const auto& a : prog_args) c_args.push_back(a.c_str());
        c_args.push_back(nullptr);

        execvp(prog_args[0].c_str(), (char* const*)c_args.data());
        std::cerr << "execvp failed for " << prog_args[0] << "\n";
        _exit(127);
    }

    // Родитель
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    if (forward_stderr) close(stderr_pipe[1]);

    std::atomic<bool> child_exited{false};

    std::thread monitor_thread([sock, pid, &child_exited]() {
        int status;
        waitpid(pid, &status, 0);
        child_exited = true;
        close(sock); // разблокирует read(sock)
    });

    std::thread t_stdout([stdout_pipe0 = stdout_pipe[0], sock]() {
        copy_fd_to_socket(stdout_pipe0, sock);
    });

    std::thread t_stderr;
    if (forward_stderr) {
        t_stderr = std::thread([stderr_pipe0 = stderr_pipe[0], sock]() {
            copy_fd_to_socket(stderr_pipe0, sock);
        });
    }

    copy_socket_to_fd(sock, stdin_pipe[1]);

    t_stdout.join();
    if (forward_stderr) t_stderr.join();

    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    if (forward_stderr) close(stderr_pipe[0]);
    close(sock);

    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
#endif
}
#ifdef _WIN32
#include <shellapi.h>  // CommandLineToArgvW
#include <locale>

// Альтернатива без codecvt (рекомендуется)
std::string utf16_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

int WINAPI QapLR_WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (wargv == nullptr) {
        MessageBoxA(nullptr, "Failed to parse command line", "Error", MB_ICONERROR);
        return 1;
    }

    std::vector<std::vector<char>> argv_storage;
    std::vector<char*> argv;

    for (int i = 0; i < argc; ++i) {
        std::string utf8 = utf16_to_utf8(wargv[i]);
        argv_storage.emplace_back(utf8.begin(), utf8.end());
        argv_storage.back().push_back('\0'); // гарантируем null-терминатор
        argv.push_back(argv_storage.back().data());
    }
    int result=main(argc,argv.data());

    LocalFree(wargv); // освобождаем память, выделенную CommandLineToArgvW
    return result;
}
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
  return QapLR_WinMain(hInstance,hPrevInstance,lpCmdLine,nCmdShow);
}
#endif