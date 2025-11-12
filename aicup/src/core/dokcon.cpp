#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <fstream>
#include <memory>
using namespace std;

std::time_t timegm_portable(std::tm* tm) {
#if defined(_WIN32) || defined(_WIN64)
  return _mkgmtime(tm);
#else
  return timegm(tm);
#endif
}

void add_hours_to_tm(std::tm& tm, int hours) {
  std::time_t t = timegm_portable(&tm);
  t += static_cast<std::time_t>(hours) * 3600; // сдвигаем UTC-время
#if defined(_WIN32) || defined(_WIN64)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
}

string qap_time() {
  using namespace std::chrono;
  system_clock::time_point now = system_clock::now();
  auto duration = now.time_since_epoch();
  auto sec = duration_cast<seconds>(duration);
  auto millis = duration_cast<milliseconds>(duration - sec);
  std::time_t t_c = sec.count();

  std::tm utc_tm;
#if defined(_WIN32) || defined(_WIN64)
  gmtime_s(&utc_tm, &t_c);
#else
  gmtime_r(&t_c, &utc_tm);
#endif

  // Москва = UTC + 3
  add_hours_to_tm(utc_tm, 3);

  std::ostringstream oss;
  oss << std::put_time(&utc_tm, "%Y.%m.%d %H:%M:%S")
      << '.' << std::setfill('0') << std::setw(3) << millis.count();
  return oss.str();
}

string qap_zchan_write(const string&z,const string&data){
  string n=std::to_string(data.size());auto sep=string("\0",1);
  return n+sep+z+sep+data;
}

struct emitter_on_data_decoder{
  typedef std::function<void(const string&, const string&)> t_cb;
  t_cb cb;
  string buffer;
  struct t_parse_result{
    unsigned char s='o';
    string to_str()const{
      if(s=='o')return "WTF?";
      if(s=='0')return "wait_size";
      if(s=='1')return "wait_zchan";
      if(s=='2')return "wait_data";
      if(s=='s')return "size_atk";
      if(s=='d')return "digit_atk";
      if(s=='z')return "zchan_atk";
      if(s=='3')return "ok?";
      return "no_impl";
    }
    bool ok(){switch(s){case '0':case '1':case '2':return true;}return false;}
  };
  static bool is_digits(const string&s){for(char c:s)if(c<'0'||c>'9')return false;return true;}
  t_parse_result feed(const char* data, size_t len) {
    buffer.append(data, len);
    while (true) {
      auto e1 = buffer.find('\0');
      if (e1 == string::npos)return {'0'};
      string len_str = buffer.substr(0, e1);
      if(len_str.size()>=7)return {'s'};
      if(!is_digits(len_str))return {'d'};
      int len = stoi(len_str);
      auto e2 = buffer.find('\0', e1 + 1);
      if (e2 == string::npos)return {'1'};
      if(e2>512)return {'z'};
      int total = e2 + 1 + len;
      if (buffer.size() < total)return {'2'};
      string z = buffer.substr(e1 + 1, e2 - e1 - 1);
      string payload = buffer.substr(e2 + 1, len);
      buffer.erase(0, total);
      cb(z, payload);
    }
    return {'3'};
  }
};

// Unix Socket Server
class UnixSocketServer {
private:
  std::string socket_path_;
  int server_fd_ = -1;
  std::atomic<bool> running_{false};
  std::thread server_thread_;
  
  std::function<void(int client_fd)> client_handler_;

public:
  UnixSocketServer(const std::string& socket_path, 
          std::function<void(int client_fd)> client_handler)
    : socket_path_(socket_path), client_handler_(client_handler) {}

  ~UnixSocketServer() {
    stop();
  }

  bool start() {
    // Удаляем старый socket файл если существует
    ::unlink(socket_path_.c_str());

    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ == -1) {
      std::cerr << qap_time() << " Failed to create socket" << std::endl;
      return false;
    }

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      std::cerr << qap_time() << " Bind failed" << std::endl;
      close(server_fd_);
      return false;
    }

    if (listen(server_fd_, 10) == -1) {
      std::cerr << qap_time() << " Listen failed" << std::endl;
      close(server_fd_);
      return false;
    }

    // Устанавливаем права на socket
    chmod(socket_path_.c_str(), 0777);

    running_ = true;
    server_thread_ = std::thread([this]() { run(); });

    std::cout << qap_time() << " Server listening on Unix socket " << socket_path_ << std::endl;
    return true;
  }

  void stop() {
    running_ = false;
    if (server_fd_ != -1) {
      close(server_fd_);
      server_fd_ = -1;
    }
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
    ::unlink(socket_path_.c_str());
  }

private:
  void run() {
    while (running_) {
      int client_fd = accept(server_fd_, nullptr, nullptr);
      if (client_fd == -1) {
        if (running_) {
          std::cerr << qap_time() << " Accept failed" << std::endl;
        }
        continue;
      }

      // Обрабатываем клиента в отдельном потоке
      std::thread client_thread([this, client_fd]() {
        client_handler_(client_fd);
        close(client_fd);
      });
      client_thread.detach();
    }
  }
};

// AI Process Manager
class AIProcessManager {
private:
  pid_t ai_pid_ = -1;
  int stdin_pipe_[2] = {-1, -1};
  int stdout_pipe_[2] = {-1, -1};
  int stderr_pipe_[2] = {-1, -1};
  
  std::function<void(const std::string&, const std::string&)> send_message_;
  std::string binary_path_;
  
  std::thread stdout_thread_;
  std::thread stderr_thread_;
  std::atomic<bool> running_{false};

public:
  AIProcessManager(const std::string& binary_path, 
          std::function<void(const std::string&, const std::string&)> send_message)
    : binary_path_(binary_path), send_message_(send_message) {}

  ~AIProcessManager() {
    stop();
  }

  bool start() {
    if (ai_pid_ != -1) {
      return false; // Уже запущен
    }

    // Создаем пайпы
    if (pipe(stdin_pipe_) == -1 || pipe(stdout_pipe_) == -1 || pipe(stderr_pipe_) == -1) {
      return false;
    }

    ai_pid_ = fork();
    if (ai_pid_ == -1) {
      close_pipes();
      return false;
    }

    if (ai_pid_ == 0) { // Child process
      close(stdin_pipe_[1]); // Close write end of stdin
      close(stdout_pipe_[0]); // Close read end of stdout
      close(stderr_pipe_[0]); // Close read end of stderr

      // Перенаправляем стандартные потоки
      dup2(stdin_pipe_[0], STDIN_FILENO);
      dup2(stdout_pipe_[1], STDOUT_FILENO);
      dup2(stderr_pipe_[1], STDERR_FILENO);

      // Запускаем AI бинарник
      execl(binary_path_.c_str(), binary_path_.c_str(), nullptr);
      
      // Если дошли сюда - ошибка
      exit(1);
    } else { // Parent process
      close(stdin_pipe_[0]); // Close read end of stdin
      close(stdout_pipe_[1]); // Close write end of stdout
      close(stderr_pipe_[1]); // Close write end of stderr

      running_ = true;
      
      // Запускаем потоки для чтения stdout/stderr
      stdout_thread_ = std::thread([this]() { read_loop(stdout_pipe_[0], "ai_stdout"); });
      stderr_thread_ = std::thread([this]() { read_loop(stderr_pipe_[0], "ai_stderr"); });

      send_message_("log", "AI process started");
      return true;
    }
  }

  void stop() {
    running_ = false;
    
    if (ai_pid_ != -1) {
      kill(ai_pid_, SIGTERM);
      waitpid(ai_pid_, nullptr, 0);
      ai_pid_ = -1;
    }

    close_pipes();

    if (stdout_thread_.joinable()) stdout_thread_.join();
    if (stderr_thread_.joinable()) stderr_thread_.join();
  }

  bool write_to_stdin(const std::string& data) {
    if (stdin_pipe_[1] == -1) return false;
    
    ssize_t written = write(stdin_pipe_[1], data.data(), data.size());
    return written == static_cast<ssize_t>(data.size());
  }

  bool is_running() const { return ai_pid_ != -1; }

private:
  void close_pipes() {
    if (stdin_pipe_[0] != -1) { close(stdin_pipe_[0]); stdin_pipe_[0] = -1; }
    if (stdin_pipe_[1] != -1) { close(stdin_pipe_[1]); stdin_pipe_[1] = -1; }
    if (stdout_pipe_[0] != -1) { close(stdout_pipe_[0]); stdout_pipe_[0] = -1; }
    if (stdout_pipe_[1] != -1) { close(stdout_pipe_[1]); stdout_pipe_[1] = -1; }
    if (stderr_pipe_[0] != -1) { close(stderr_pipe_[0]); stderr_pipe_[0] = -1; }
    if (stderr_pipe_[1] != -1) { close(stderr_pipe_[1]); stderr_pipe_[1] = -1; }
  }

  void read_loop(int pipe_fd, const std::string& channel) {
    char buffer[4096];
    while (running_) {
      ssize_t n = read(pipe_fd, buffer, sizeof(buffer));
      if (n <= 0) break;
      
      send_message_(channel, std::string(buffer, n));
    }
  }
};

// Client Session Handler
class ClientSession {
private:
  int client_fd_;
  std::atomic<bool> connected_{true};
  emitter_on_data_decoder decoder_;
  AIProcessManager ai_manager_;
  
  std::string tmp_dir_ = ".";
  std::string ai_bin_name_ = "./ai.bin";
  
  std::mutex write_mutex_;

public:
  ClientSession(int client_fd, const std::string& tmp_dir = ".")
    : client_fd_(client_fd), tmp_dir_(tmp_dir),
     ai_manager_(tmp_dir + "/" + ai_bin_name_, 
           [this](const std::string& z, const std::string& data) {
             send_message(z, data);
           }) {
    
    decoder_.cb = [this](const std::string& z, const std::string& payload) {
      handle_message(z, payload);
    };
    
    // Отправляем приветствие
    send_message("hi from dokcon.cpp", "2025.10.18 12:01:08.493");
    
    // Запускаем цикл чтения
    std::thread([this]() { read_loop(); }).detach();
  }

  ~ClientSession() {
    connected_ = false;
    close(client_fd_);
  }

private:
  void read_loop() {
    char buffer[4096];
    while (connected_) {
      ssize_t n = read(client_fd_, buffer, sizeof(buffer));
      if (n <= 0) {
        connected_ = false;
        break;
      }
      decoder_.feed(buffer, n);
    }
  }

  void send_message(const std::string& z, const std::string& data) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    std::string frame = qap_zchan_write(z, data);
    ssize_t sent = write(client_fd_, frame.data(), frame.size());
    if (sent != static_cast<ssize_t>(frame.size())) {
      connected_ = false;
    }
  }

  void handle_message(const std::string& z, const std::string& msg) {
    if (z == "ai_stdin") {
      if (ai_manager_.is_running()) {
        ai_manager_.write_to_stdin(msg);
      } else {
        send_message("log", "i got ai_stdin but aiProcess is not started!");
      }
    } else if (z == "ai_binary") {
      std::string file_path = tmp_dir_ + "/" + ai_bin_name_;
      std::ofstream file(file_path, std::ios::binary);
      if (file) {
        file.write(msg.data(), msg.size());
        file.close();
        chmod(file_path.c_str(), 0755);
        send_message("ai_binary_ack", std::to_string(msg.size()));
      } else {
        send_message("log", "Failed to write binary file");
      }
    } else if (z == "ai_start") {
      if (ai_manager_.is_running()) {
        send_message("log", "AI already running");
      } else {
        if (ai_manager_.start()) {
          send_message("log", "AI process started successfully");
        } else {
          send_message("log", "Failed to start AI process");
        }
      }
    } else if (z == "ai_stop") {
      if (ai_manager_.is_running()) {
        ai_manager_.stop();
        send_message("log", "AI killed");
      } else {
        send_message("log", "No AI process running");
      }
    } else {
      send_message("log", "Unknown channel: " + z);
    }
  }
};

// Main application
class DokconServer {
public:
  std::unique_ptr<UnixSocketServer> server_;
  std::string socket_path_ = "/tmp/dokcon.sock";
  std::string tmp_dir_ = ".";

public:
  void run() {
    std::cout << qap_time() << " Starting dokcon.cpp server..." << std::endl;
    
    server_ = std::make_unique<UnixSocketServer>(
      socket_path_, 
      [this](int client_fd) { 
        handle_client(client_fd); 
      }
    );
    
    if (!server_->start()) {
      std::cerr << qap_time() << " Failed to start server" << std::endl;
      return;
    }
    
    // Ждем сигнала завершения
    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();
    
    server_->stop();
  }

private:
  void handle_client(int client_fd) {
    std::cout << qap_time() << " Client connected" << std::endl;
    
    // Создаем сессию для клиента (управляется внутри класса)
    ClientSession session(client_fd, tmp_dir_);
    
    // Ждем пока клиент отключится
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      // Проверка соединения происходит внутри ClientSession
    }
    
    std::cout << qap_time() << " Client disconnected" << std::endl;
  }
};

int main(int argc, char* argv[]) {
  if (argc > 1 && std::string(argv[1]) == "server_unix") {
    DokconServer server;
    server.socket_path_=std::getenv("SOCKET_PATH");
    server.run();
  } else {
    std::cout << "Usage: " << argv[0] << " [server_unix]" << std::endl;
  }
  
  return 0;
}