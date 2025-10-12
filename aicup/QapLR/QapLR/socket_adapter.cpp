// socket_adapter.cpp
// Компиляция:
//   Linux/macOS: g++ -std=c++17 -O2 socket_adapter.cpp -o socket_adapter
//   Windows:     cl /EHsc /std:c++17 socket_adapter.cpp /link ws2_32.lib
//   cl socket_adapter.cpp /std:c++17 /EHsc /nologo /O2
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <set>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <io.h>
    #include <fcntl.h>
    #include <Ws2tcpip.h>
    using ssize_t = int;
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h> // для TCP_NODELAY
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/wait.h>
    #include <errno.h>
#endif

void throw_error(const std::string& msg) {
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
#pragma warning(disable : 4996)
void copy_stream(int in_fd, int out_fd) {
    char buf[4096];
    ssize_t n;
    while ((n = read(in_fd, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(out_fd, buf + written, n - written);
            if (w <= 0) return;
            written += w;
        }
    }
}
#ifdef _WIN32
// Только для Windows
void copy_pipe_to_socket(HANDLE pipe_read, SOCKET sock) {
    char buf[4096];
    DWORD bytes_read;
    while (ReadFile(pipe_read, buf, sizeof(buf), &bytes_read, nullptr) && bytes_read > 0) {
        int sent = 0;
        while (sent < (int)bytes_read) {
            int res = send(sock, buf + sent, bytes_read - sent, 0);
            if (res <= 0) return;
            sent += res;
        }
    }
}

void copy_socket_to_pipe(SOCKET sock, HANDLE pipe_write) {
    char buf[4096];
    int received;
    while ((received = recv(sock, buf, sizeof(buf), 0)) > 0) {
        DWORD written = 0;
        while (written < (DWORD)received) {
            DWORD dw;
            if (!WriteFile(pipe_write, buf + written, received - written, &dw, nullptr)) break;
            written += dw;
        }
    }
}
#endif

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS] <host:port> <program> [args...]\n"
              << "\n"
              << "Options:\n"
              << "  -e, --no-stderr    Do not forward child's stderr to socket\n"
              << "  -h, --help         Show this help\n"
              << "\n"
              << "Example:\n"
              << "  " << prog << " 127.0.0.1:12345 ./ai other args\n"
              << "  " << prog << " -e 127.0.0.1:12345 ./ai other args\n";
}
//t_world 4 0 1 -g
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    bool forward_stderr = true;
    int arg_start = 1;

    // Парсим опции
    std::set<std::string> known_flags = {"-e", "--no-stderr", "-h", "--help"};
    while (arg_start < argc) {
        std::string arg = argv[arg_start];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "-e" || arg == "--no-stderr") {
            forward_stderr = false;
            ++arg_start;
        } else if (known_flags.count(arg)) {
            std::cerr << "Flag '" << arg << "' requires no argument\n";
            return 1;
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        } else {
            break; // первый не-флаг — адрес
        }
    }

    if (arg_start + 1 >= argc) {
        std::cerr << "Missing <host:port> and/or <program>\n";
        print_help(argv[0]);
        return 1;
    }

    std::string addr_str = argv[arg_start];
    std::vector<std::string> prog_args(argv + arg_start + 1, argv + argc);
    std::vector<const char*> c_args;
    for (const auto& a : prog_args) c_args.push_back(a.c_str());
    c_args.push_back(nullptr);

    auto [host, port] = parse_address(addr_str);

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw_error("WSAStartup failed");
    }
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) throw_error("socket() failed");

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
        (char*)
#endif
        &yes, sizeof(yes));

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        throw_error("Invalid IP address: " + host);
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        throw_error("connect() failed");
    }

    // ?? ВАЖНО: отключаем буферизацию TCP (алгоритм Нейгла)
    int nodelay = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
#ifdef _WIN32
        (char*)
#endif
        &nodelay, sizeof(nodelay)) < 0) {
        std::cerr << "Warning: failed to set TCP_NODELAY\n";
    }

    std::cout << "Connected to " << host << ":" << port << "\n";

#ifdef _WIN32
    HANDLE child_stdin_read, child_stdin_write;
    HANDLE child_stdout_read, child_stdout_write;
    HANDLE child_stderr_read = nullptr, child_stderr_write = nullptr;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    if (!CreatePipe(&child_stdin_read, &child_stdin_write, &sa, 0) ||
        !CreatePipe(&child_stdout_read, &child_stdout_write, &sa, 0)) {
        throw std::runtime_error("CreatePipe failed for stdin/stdout");
    }

    if (forward_stderr) {
        if (!CreatePipe(&child_stderr_read, &child_stderr_write, &sa, 0)) {
            throw std::runtime_error("CreatePipe failed for stderr");
        }
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = child_stdin_read;
    si.hStdOutput = child_stdout_write;
    si.hStdError = forward_stderr ? child_stderr_write : GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {};

    std::string cmd_line;
    for (size_t i = 0; i < prog_args.size(); ++i) {
        if (i > 0) cmd_line += " ";
        // Простая экранировка аргументов
        std::string arg = prog_args[i];
        if (arg.find_first_of(" \t\n\"") != std::string::npos) {
            // Экранируем кавычки
            size_t pos = 0;
            while ((pos = arg.find('"', pos)) != std::string::npos) {
                arg.insert(pos, 1, '\\');
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
        // Cleanup
        CloseHandle(child_stdin_read); CloseHandle(child_stdin_write);
        CloseHandle(child_stdout_read); CloseHandle(child_stdout_write);
        if (forward_stderr) {
            CloseHandle(child_stderr_read); CloseHandle(child_stderr_write);
        }
        throw std::runtime_error("CreateProcess failed");
    }

    CloseHandle(child_stdin_read);
    CloseHandle(child_stdout_write);
    if (forward_stderr) CloseHandle(child_stderr_write);

    int stdin_write_fd = _open_osfhandle((intptr_t)child_stdin_write, _O_WRONLY | _O_BINARY);
    int stdout_read_fd = _open_osfhandle((intptr_t)child_stdout_read, _O_RDONLY | _O_BINARY);
    int stderr_read_fd = -1;
    if (forward_stderr) {
        stderr_read_fd = _open_osfhandle((intptr_t)child_stderr_read, _O_RDONLY | _O_BINARY);
        if (stderr_read_fd == -1) {
            throw std::runtime_error("_open_osfhandle (stderr) failed");
        }
    }

    if (stdin_write_fd == -1 || stdout_read_fd == -1) {
        throw std::runtime_error("_open_osfhandle failed");
    }

#else // Unix-like
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2] = {-1, -1};
    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
        throw std::runtime_error("pipe() failed for stdin/stdout");
    }
    if (forward_stderr && pipe(stderr_pipe) != 0) {
        throw std::runtime_error("pipe() failed for stderr");
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        if (forward_stderr) close(stderr_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        if (forward_stderr) {
            dup2(stderr_pipe[1], STDERR_FILENO);
        } else {
            // Оставляем stderr как есть (обычно терминал или /dev/null)
        }

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        if (forward_stderr) close(stderr_pipe[1]);

        execvp(prog_args[0].c_str(), (char* const*)c_args.data());
        std::cerr << "execvp failed for " << prog_args[0] << "\n";
        _exit(127);
    } else if (pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    // Parent
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    if (forward_stderr) close(stderr_pipe[1]);

    int stdin_write_fd = stdin_pipe[1];
    int stdout_read_fd = stdout_pipe[0];
    int stderr_read_fd = forward_stderr ? stderr_pipe[0] : -1;
#endif

    // === Потоки копирования ===
    std::thread t_stdout([sock, stdout_read_fd]() {
        copy_stream(stdout_read_fd, sock);
    });

    std::thread t_stderr;
    if (forward_stderr) {
        t_stderr = std::thread([sock, stderr_read_fd]() {
            copy_stream(stderr_read_fd, sock);
        });
    }

    // Основной цикл: сокет > stdin
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(stdin_write_fd, buf + written, n - written);
            if (w <= 0) break;
            written += w;
        }
    }

    // Завершение
    t_stdout.join();
    if (forward_stderr) t_stderr.join();

    close(stdin_write_fd);
    close(stdout_read_fd);
    if (forward_stderr) close(stderr_read_fd);
    close(sock);

#ifdef _WIN32
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    WSACleanup();
    return static_cast<int>(exit_code);
#else
    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
#endif
}

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