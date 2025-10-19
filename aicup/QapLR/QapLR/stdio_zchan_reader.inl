class StdioZchanReader {
public:
    using message_cb_t = std::function<void(const std::string&, const std::string&)>;
    
    StdioZchanReader(message_cb_t cb) : decoder(), running(true) {
        decoder.cb = std::move(cb);
        reader_thread = std::thread([this]() {
            std::ios::sync_with_stdio(false);
            std::cin.tie(nullptr);
            
            char buffer[4096];
            while (running) {
                std::cin.read(buffer, sizeof(buffer));
                std::streamsize n = std::cin.gcount();
                if (n <= 0) break; // EOF или ошибка
                decoder.feed(buffer, static_cast<size_t>(n));
            }
            running = false;
        });
    }

    ~StdioZchanReader() {
        stop();
    }

    void stop() {
        if (running) {
            running = false;
            if (reader_thread.joinable()) {
                // Примечание: нельзя "разбудить" cin, кроме как через EOF
                // Поэтому при завершении t_node должен закрыть stdin QapLR
                reader_thread.join();
            }
        }
    }

private:
    emitter_on_data_decoder decoder;
    std::thread reader_thread;
    std::atomic<bool> running{false};
};
bool write_zchan(const std::string& z, const std::string& payload) {
    std::string frame = qap_zchan_write(z, payload);
    std::cout.write(frame.data(), frame.size());
    std::cout.flush();
    return !std::cout.fail();
}
void zchan_write(const std::string& z, const std::string& payload) {
    std::string frame = qap_zchan_write(z, payload);
    std::cout.write(frame.data(), static_cast<std::streamsize>(frame.size()));
    std::cout.flush();
    if (std::cout.fail()) {
        std::cerr << "QapLR: ERROR: Failed to write to stdout (zchan=" << z << ")"<<std::endl;
        exit(0);//TerminateProcess(GetCurrentProcess(),0);
    }
};
class IInputReader {
public:
    virtual ~IInputReader() = default;
    // Возвращает, сколько байт доступно для чтения без блокировки
    virtual std::streamsize available() = 0;
    // Читает не более max_bytes в буфер buffer, возвращает сколько реально прочитано
    virtual std::streamsize read(char* buffer, std::streamsize max_bytes) = 0;
};

#ifndef _WIN32
#include <sys/select.h>
#include <unistd.h>

class UnixInputReader : public IInputReader {
public:
    std::streamsize available() override {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);
        struct timeval timeout = {0, 0}; // неблокирующий опрос
        int ret = select(STDIN_FILENO + 1, &set, nullptr, nullptr, &timeout);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &set)) {
            // Здесь можно использовать ioctl/FIONREAD для точного числа байт (зависит от платформы)
            int bytes = 0;
            ioctl(STDIN_FILENO, FIONREAD, &bytes);
            return bytes;
        }
        return 0;
    }

    std::streamsize read(char* buffer, std::streamsize max_bytes) override {
        return ::read(STDIN_FILENO, buffer, max_bytes);
    }
};
#else
#include <windows.h>
class WindowsInputReader : public IInputReader {
public:
    std::streamsize available() override {
        DWORD bytesAvailable = 0;
        HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        if (!PeekNamedPipe(hStdIn, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
            return 0;
        }
        return static_cast<std::streamsize>(bytesAvailable);
    }

    std::streamsize read(char* buffer, std::streamsize max_bytes) override {
        DWORD bytesRead = 0;
        HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        if (!ReadFile(hStdIn, buffer, static_cast<DWORD>(max_bytes), &bytesRead, nullptr)) {
            return 0;
        }
        return static_cast<std::streamsize>(bytesRead);
    }
};
#endif
#ifdef _WIN32
using InputReader = WindowsInputReader;
#else
using InputReader = UnixInputReader;
#endif