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