// t_cdn.cpp
/*
#define CPPHTTPLIB_SERVER_ONLY
#include "thirdparty/httplib.h"
#include "netapi.h" // для file_put_contents, file_get_contents
#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;
*/
#ifdef _WIN32
static const string CDN_DATA_DIR = "./";
#else
static const string CDN_DATA_DIR = "/tmp/cdn/"; // Позже можно на /var/lib/cdn
#endif
static const string UPLOAD_TOKEN = "s3cr3t_t0k3n_f0r_cdn_uploads_2025"; // < вынеси в аргументы
struct t_cdn{
  static const int CDN_PORT = 12346;

  mutex fs_mutex;

  bool is_authorized(const httplib::Request& req) {
      auto auth = req.get_header_value("Authorization");
      return auth == "Bearer " + UPLOAD_TOKEN;
  }

  struct t_metrics {
      std::atomic<uint64_t> bytes_uploaded{0};
      std::atomic<uint64_t> bytes_downloaded{0};
      std::atomic<uint64_t> requests_total{0};
      std::atomic<uint64_t> requests_replay{0};
      std::atomic<uint64_t> requests_source{0};
      std::atomic<uint64_t> requests_binary{0};
      std::atomic<uint64_t> requests_image{0};

      void inc_upload(size_t bytes, const std::string& type) {
          bytes_uploaded += bytes;
          requests_total++;
          if (type == "replay") requests_replay++;
          else if (type == "source") requests_source++;
          else if (type == "binary") requests_binary++;
          else if (type == "image") requests_image++;
      }

      void inc_download(size_t bytes, const std::string& type) {
          bytes_downloaded += bytes;
          requests_total++;
          if (type == "replay") requests_replay++;
          else if (type == "source") requests_source++;
          else if (type == "binary") requests_binary++;
          else if (type == "image") requests_image++;
      }

      std::string to_prometheus() const {
          return
              //"# HELP cdn_bytes_uploaded Total bytes uploaded to CDN\n"
              //"# TYPE cdn_bytes_uploaded counter\n"
              "cdn_bytes_uploaded " + std::to_string(bytes_uploaded) + "\n"

              //"# HELP cdn_bytes_downloaded Total bytes downloaded from CDN\n"
              //"# TYPE cdn_bytes_downloaded counter\n"
              "cdn_bytes_downloaded " + std::to_string(bytes_downloaded) + "\n"

              //"# HELP cdn_requests_total Total HTTP requests\n"
              //"# TYPE cdn_requests_total counter\n"
              "cdn_requests_total " + std::to_string(requests_total) + "\n"

              //"# HELP cdn_requests_replay Number of replay requests\n"
              //"# TYPE cdn_requests_replay counter\n"
              "cdn_requests_replay " + std::to_string(requests_replay) + "\n"

              //"# HELP cdn_requests_source Number of source requests\n"
              //"# TYPE cdn_requests_source counter\n"
              "cdn_requests_source " + std::to_string(requests_source) + "\n"

              //"# HELP cdn_requests_binary Number of binary requests\n"
              //"# TYPE cdn_requests_binary counter\n"
              "cdn_requests_binary " + std::to_string(requests_binary) + "\n"

              //"# HELP cdn_requests_image Number of image requests\n"
              //"# TYPE cdn_requests_image counter\n"
              "cdn_requests_image " + std::to_string(requests_image) + "\n";
      }
  };
  struct RateLimiter {
      struct ClientInfo {
          int count = 0;
          time_t first_request = 0;
          time_t blocked_until = 0;
      };

      mutex mtx;
      map<string, ClientInfo> clients;
      set<string> whitelist; // белый список

      bool is_allowed(const string& ip) {
          lock_guard<mutex> lock(mtx);

          // Пропускаем белые IP
          if (whitelist.count(ip)) return true;

          auto now = time(0);
          auto& info = clients[ip];

          if (info.blocked_until > now) {
              return false;
          }

          if (now - info.first_request > 10) {
              // Сброс за 10 секунд
              info.count = 1;
              info.first_request = now;
          } else {
              info.count++;
              if (info.count > 25) {
                  info.blocked_until = now + 30;
                  return false;
              }
          }

          return true;
      }

      void add_to_whitelist(const string& ip) {
          lock_guard<mutex> lock(mtx);
          whitelist.insert(ip);
      }

      void remove_from_whitelist(const string& ip) {
          lock_guard<mutex> lock(mtx);
          whitelist.erase(ip);
      }
  };

  void add_content_handlers(
      httplib::Server& svr,
      const std::string& base_path,
      const std::string& type,
      bool auth_get,
      std::function<void(const httplib::Request&, httplib::Response&, const std::string&)> on_put,
      std::function<void(const httplib::Request&, httplib::Response&, const std::string&)> on_get
  ) {
      auto put_path = "/" + base_path + "/([^/]+)";
      auto get_path = "/" + base_path + "/([^/]+)";

      svr.Put(put_path.c_str(), [this,on_put, type](const httplib::Request& req, httplib::Response& res) {
          if (!is_authorized(req)) {
              res.set_content("Unauthorized", "text/plain");
              res.status = 403;
              return;
          }
          auto name = req.matches[1].str();
          on_put(req, res, name);
          g_metrics.inc_upload(req.body.size(), type);
      });

      svr.Get(get_path.c_str(), [this,auth_get,on_get, type](const httplib::Request& req, httplib::Response& res) {
          if (!rate_limiter.is_allowed(req.remote_addr)) {
              res.set_content("Rate limit exceeded", "text/plain");
              res.status = 429;
              return;
          }
          if (auth_get&&!is_authorized(req)) {
              res.set_content("Unauthorized", "text/plain");
              res.status = 403;
              return;
          }
          auto name = req.matches[1].str();
          on_get(req, res, name);
          if (res.status == 200) {
              lock_guard<mutex> lock(fs_mutex);
              g_metrics.inc_download(res.body.size(), type);
          }
      });
  }
  void cleanup_old_clients() {
      lock_guard<mutex> lock(rate_limiter.mtx);
      auto&arr=rate_limiter.clients;
      auto now = time(0);
      for (auto it = arr.begin(); it != arr.end(); ) {
          // Если неактивен > 10 минут и не забанен — удаляем
          if (now - it->second.first_request > 600 && it->second.blocked_until == 0) {
              it = arr.erase(it);
          } else {
              ++it;
          }
      }
  }
  RateLimiter rate_limiter;

  t_metrics g_metrics;
  httplib::Server svr;
  int main() {
      thread([&](){
        this_thread::sleep_for(600s);
        cleanup_old_clients();
      }).detach();
      system(("mkdir -p "+CDN_DATA_DIR).c_str());
      system(("mkdir -p "+CDN_DATA_DIR+"images/").c_str());
      system(("mkdir -p "+CDN_DATA_DIR+"static/").c_str());
      #define LOGWAY()cout<<local_cur_date_str_v4()<<" "<<"["<<req.remote_addr<<"] "<<req.path/*<<" "<<req.path_params*/<<endl;
      svr.Get("/metrics", [&](const httplib::Request& req, httplib::Response& res) {
          res.set_content(g_metrics.to_prometheus(), "text/plain");
          res.status = 200;
          LOGWAY();
      });

      svr.Put(R"(/images/([^/]+\.tar))", [&](const httplib::Request& req, httplib::Response& res) {
          if (!is_authorized(req)) {res.status = 403;return;}
          auto filename = req.matches[1].str();
          auto path = CDN_DATA_DIR + "images/" + filename;
          {
              lock_guard<mutex> lock(fs_mutex);
              if (!file_put_contents(path, req.body)){res.status = 500; return;}
          }
          g_metrics.inc_upload(req.body.size(), "image");
          res.status = 200;
          LOGWAY();
      });
      svr.Get(R"(/images/([^/]+\.tar))", [&](const httplib::Request& req, httplib::Response& res) {
          if (!rate_limiter.is_allowed(req.remote_addr)) {
              res.set_content("Rate limit exceeded", "text/plain");
              res.status = 429;
              return;
          }
          //if (!is_authorized(req)) {res.status = 403;return;}
          auto filename = req.matches[1].str();
          auto path = CDN_DATA_DIR + "images/" + filename;
          string data;
          {
              lock_guard<mutex> lock(fs_mutex);
              data=file_get_contents(path);
              if (data.empty()){res.status = 404;return;}
          }
          g_metrics.inc_download(req.body.size(), "image");
          res.status = 200;
          LOGWAY();
      });
      add_content_handlers(svr, "static", "static",false,
          [this](const auto& req, auto& res, const string& name) {
              if (!is_authorized(req)) {
                  res.status = 403;
                  return;
              }
              auto path = CDN_DATA_DIR + "static/" + name;
              {
                  lock_guard<mutex> lock(fs_mutex);
                  if (!file_put_contents(path, req.body)) {
                      res.status = 500;
                      return;
                  }
              }
              res.status = 200;
              LOGWAY();
          },
          [this](const auto& req, auto& res, const string& name) {
              if (!rate_limiter.is_allowed(req.remote_addr)) {
                  res.status = 429;
                  return;
              }
              auto path = CDN_DATA_DIR + "static/" + name;
              string data;
              {
                  lock_guard<mutex> lock(fs_mutex);
                  data = file_get_contents(path);
              }
              if (data.empty()) {
                  res.status = 404;
                  return;
              }
              // Определим Content-Type по расширению
              string ct = "application/octet-stream";
              const auto ext="."+GetFileExt(name);
              if (ext==(".js")) ct = "text/javascript";
              else if (ext==(".txt")) ct = "text/plain";
              else if (ext==(".css")) ct = "text/css";
              else if (ext==(".wasm")) ct = "application/wasm";
              else if (ext==(".png")) ct = "image/png";
              else if (ext==(".jpg") || ext==(".jpeg")) ct = "image/jpeg";

              res.set_content(data, ct);
              g_metrics.inc_download(data.size(), "static");
              LOGWAY();
          }
      );

      svr.Post("/whitelist", [&](const httplib::Request& req, httplib::Response& res) {
          if (!is_authorized(req)) {
              res.status = 403;
              return;
          }

          auto ips_str = req.get_param_value("ips");
          auto action = req.get_param_value("action");

          if (ips_str.empty() || (action != "add" && action != "remove")) {
              res.status = 400;
              return;
          }

          // Разделяем по запятой
          vector<string> ips;
          stringstream ss(ips_str);
          string ip;
          while (getline(ss, ip, ',')) {
              if (!ip.empty()) {
                  ips.push_back(ip);
              }
          }

          lock_guard<mutex> lock(rate_limiter.mtx);
          for (const auto& ip : ips) {
              if (action == "add") {
                  rate_limiter.whitelist.insert(ip);
              } else if (action == "remove") {
                  rate_limiter.whitelist.erase(ip);
              }
          }

          res.status = 200;
          /*
          curl -X POST "http://your-cdn:8080/whitelist" \
             -H "Authorization: Bearer s3cr3t_t0k3n_f0r_cdn_uploads_2025" \
             -d "ips=192.168.1.100,192.168.1.101,10.0.0.5&action=add"
          */
          LOGWAY();
      });

      // --- Health check ---
      svr.Post("/health", [&](const httplib::Request& req, httplib::Response& res) {
          res.set_content("OK", "text/plain");
          res.status = 200;
          LOGWAY();
      });

      add_content_handlers(svr, "replay", "replay",false,
          [this](const auto& req, auto& res, const string& name) {
              auto path = CDN_DATA_DIR + "replay_" + name + ".bin";
              {
                  lock_guard<mutex> lock(fs_mutex);
                  if (!file_put_contents(path, req.body)) {
                      res.status = 500;
                      return;
                  }
              }
              res.status = 200;
              LOGWAY();
          },
          [this](const auto& req, auto& res, const string& name) {
              auto path = CDN_DATA_DIR + "replay_" + name + ".bin";
              string data;
              {
                  lock_guard<mutex> lock(fs_mutex);
                  data = file_get_contents(path);
              }
              if (data.empty()) {
                  res.status = 404;
                  return;
              }
              res.set_content(data, "application/octet-stream");
              LOGWAY();
          }
      );

      add_content_handlers(svr, "source", "source",true,
          [this](const auto& req, auto& res, const string& name) {
              auto path = CDN_DATA_DIR + "source_" + name;
              {
                  lock_guard<mutex> lock(fs_mutex);
                  if (!file_put_contents(path, req.body)) {
                      res.status = 500;
                      return;
                  }
              }
              res.status = 200;
              LOGWAY();
          },
          [this](const auto& req, auto& res, const string& name) {
              auto path = CDN_DATA_DIR + "source_" + name;
              string data;
              {
                  lock_guard<mutex> lock(fs_mutex);
                  data = file_get_contents(path);
              }
              if (data.empty()) {
                  res.status = 404;
                  return;
              }
              res.set_content(data, "application/octet-stream");
              LOGWAY();
          }
      );

      add_content_handlers(svr, "binary", "binary",false,
          [this](const auto& req, auto& res, const string& name) {
              auto path = CDN_DATA_DIR + "binary_" + name;
              {
                  lock_guard<mutex> lock(fs_mutex);
                  if (!file_put_contents(path, req.body)) {
                      res.status = 500;
                      return;
                  }
              }
              res.status = 200;
              LOGWAY();
          },
          [this](const auto& req, auto& res, const string& name) {
              auto path = CDN_DATA_DIR + "binary_" + name;
              string data;
              {
                  lock_guard<mutex> lock(fs_mutex);
                  data = file_get_contents(path);
              }
              if (data.empty()) {
                  res.status = 404;
                  return;
              }
              res.set_content(data, "application/octet-stream");
              LOGWAY();
          }
      );
      #undef LOGWAY
      // Запуск сервера
      cout << "[t_cdn] Starting server on port " << CDN_PORT << "...\n";
      cout << "[t_cdn] Data dir: " << CDN_DATA_DIR << "\n";
      cout << "[t_cdn] Token: " << UPLOAD_TOKEN << "\n";

      if (!svr.listen("0.0.0.0", CDN_PORT)) {
          cerr << "[t_cdn] Failed to start server\n";
          return -1;
      }

      return 0;
  }
};