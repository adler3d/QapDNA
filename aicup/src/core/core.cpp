#include "netapi.h"
#include "FromQapEng.h"
#include "thirdparty/picosha2.h"
#include "thirdparty/httplib.h"//13
#include "thirdparty/json.hpp"
#include <vector>
#include <cmath>
#include <string>
#include <chrono>
#include <future>
using namespace std;
using namespace std::chrono;
using json = nlohmann::json;
#include "t_cdn.hpp"
const string DOCKERFILE_PATH = "./docker/universal-runner/Dockerfile"; //TODO: check this file must exists!
const string IMAGE_NAME = "universal-runner:latest";
const string ARCHIVE_NAME = "/tmp/universal-runner.tar";
const string CDN_HOSTPORT = "localhost:"+to_string(t_cdn::CDN_PORT);
const string CDN_URL="http://"+CDN_HOSTPORT;
const string CDN_URL_IMAGES=CDN_URL+"/images/"; //TODO: replace to t_main "ip:port/images/"
const string COMPILER_URL="http://localhost:"+3000;

static void LOG(...){}

int http_put_with_auth(const string& path, const string& body, const string& token,const string&cdn=CDN_URL) {
  try {
    httplib::Client cli(cdn);
    cli.set_connection_timeout(60);
    cli.set_read_timeout(60);
    cli.set_write_timeout(60);

    auto res = cli.Put(
      ("/" + path).c_str(),
      {{"Authorization", "Bearer " + token}},
      body,
      "application/octet-stream"
    );

    if (res && res->status == 200) {
      return 200;
    } else {
      return res ? res->status : 500;
    }
  } catch (const exception& e) {
    return 500;
  }
}
struct t_post_resp{
  int status;
  string body;
};
t_post_resp http_post_json_with_auth(const string& path, const string& body, const string& token,const string&cdn=CDN_URL) {
  try {
    httplib::Client cli(cdn);
    cli.set_connection_timeout(60);
    cli.set_read_timeout(60);
    cli.set_write_timeout(60);

    auto res = cli.Post(
      ("/" + path).c_str(),
      {{"Authorization", "Bearer " + token}},
      body,
      "application/json"
    );

    return res?t_post_resp{res->status,res->body}:t_post_resp{500,"no http_post response"};
  } catch (const exception& e) {
    return {500,e.what()};
  }
}

string sha256(const string&s){
  std::vector<unsigned char> hash(picosha2::k_digest_size);
  picosha2::hash256(s.begin(),s.end(),hash.begin(),hash.end());
  return picosha2::bytes_to_hex_string(hash.begin(),hash.end());
}
string get_image_tag(const string&binary_path){
  auto s=file_get_contents(binary_path);
  string hash=sha256(s);
  return "ai_cache_"+hash.substr(0,16);
}

bool has_cached_image(const string&image_tag){
  string cmd="docker image inspect "+image_tag+" > /dev/null 2>&1";
  return system(cmd.c_str())==0;
}

struct t_elo_score {
  double a, b, c, d;
  double get() const { return 0.25 * (a + b + c + d); }
  void set(double val) { a = b = c = d = val; }
};

struct t_player_with_score{
  string coder;
  t_elo_score cur, next;
  double game_score;
};

void update_score(vector<t_player_with_score>& players) {
  constexpr double K = 32.0;
  size_t n = players.size();
  for (size_t i = 0; i < n; ++i) {
    double Ri = players[i].cur.get();
    double expected_score = 0.0;
    for (size_t j = 0; j < n; ++j) {
      if (i == j) continue;
      double Rj = players[j].cur.get();
      double expected = 1.0 / (1.0 + pow(10.0, (Rj - Ri) / 400.0));
      expected_score += expected;
    }
    expected_score /= (n - 1);
    double max_score = 0.0;
    for (const auto& p : players) {
      if (p.game_score > max_score) max_score = p.game_score;
    }
    double actual_score = max_score > 0 ? players[i].game_score / max_score : 0;
    double new_rating = Ri + K * (actual_score - expected_score);
    players[i].next.set(new_rating);
  }
}


using namespace std;
string local_main_ip_port="127.0.0.1:31456";
//vector<string> split(...){return {};}
//#define QapAssert(...)
string generate_token(string coder_name,string time) {
  return "token:"+to_string((rand()<<16)+rand())+coder_name+time;
  //return sha256( random_bytes(32) + time + coder_name );
}
template<class TYPE>TYPE parse(const string_view&s){return {};}
string serialize(...){return "nope";}

/*
struct stream_write_encoder {
  t_unix_socket& socket;
  stream_write_encoder(t_unix_socket& s) : socket(s) {}

  void operator()(const string& z, const string& data) {
    string strData = data.empty() ? "" : data;
    string lenStr = to_string(strData.length());

    socket.write(lenStr);
    socket.write("\0", 1);
    socket.write(z);
    socket.write("\0", 1);
    socket.write(strData);
  }

  void operator()(const string& z, const void* data, size_t len) {
    string lenStr = to_string(len);
    socket.write(lenStr);
    socket.write("\0", 1);
    socket.write(z);
    socket.write("\0", 1);
    socket.write((const char*)data, len);
  }
};*/
template<class t_unix_socket>
void stream_write(t_unix_socket& client, const string& z, const string& data) {
  string strData = data;
  string lenStr = to_string(strData.length());

  client.write(lenStr.c_str(), lenStr.length());
  client.write("\0", 1);
  client.write(z.c_str(), z.length());
  client.write("\0", 1);
  client.write(strData.c_str(), strData.length());
}

struct emitter_on_data_decoder{
  function<void(const string&, const string&)> cb;
  string buffer;

  void feed(const char* data, size_t len) {
    buffer.append(data, len);
    while (true) {
      auto e1 = buffer.find('\0');
      if (e1 == string::npos) break;

      string len_str = buffer.substr(0, e1);
      if (!all_of(len_str.begin(), len_str.end(), ::isdigit)) break;
      int len = stoi(len_str);

      auto e2 = buffer.find('\0', e1 + 1);
      if (e2 == string::npos) break;

      int total = e2 + 1 + len;
      if (buffer.size() < total) break;

      string z = buffer.substr(e1 + 1, e2 - e1 - 1);
      string payload = buffer.substr(e2 + 1, len);
      buffer.erase(0, total);

      cb(z, payload);
    }
  }
};

struct t_process{
  string machine;
  string name;
  //string main;
};

struct t_coder_rec{
  struct t_source{
    string cdn_src_url;
    string cdn_bin_url;
    string time;
    string prod_time;
    string err;
  };
  string id;
  string name;
  string token;
  string email;
  string time;
  unique_ptr<mutex> sarr_mtx;
  vector<t_source> sarr;
  int total_games=0;
  bool allowed_next_src_upload()const{
    lock_guard<mutex> lock(*sarr_mtx);
    if(sarr.empty())return true;
    if(sarr.back().prod_time.empty())return false;
    return qap_time_diff(sarr.back().time,qap_time())>60*1000;
  }
};
struct t_game_rec{
  struct t_slot{string coder;double score=0;string err;string status;};
  vector<t_slot> arr;
  string cdn_file;
  int tick=0;
};
struct t_game_slot{string coder;string cdn_bin_file;};
struct t_game_decl{
  vector<t_game_slot> arr;
  string config;
  int game_id=0;
  int maxtick=20000;// or 6000 * 0.050 == 5*60
  int stderr_max=1024*64;
  double TL=20.0;
  double TL0=1000.0;
};
struct t_main_v0:t_process{
  vector<t_coder_rec> carr;
  vector<t_game_rec> garr;
  map<string,string> node2ipport;
  t_net_api capi;
  void create_new_game_on_node(const string&node,const t_game_decl&gd){
    capi.write_to_socket(node2ipport[node],"new_game,"+serialize(gd)+"\n");
  }
  int main(){
    return 0;
  }
};
struct t_cmd{bool valid=true;};
struct t_world{
  void use(int player_id,const t_cmd&cmd){}
  void step(){};
  bool finished(){return false;}
  vector<double> slot2score;
};
struct t_finished_game{
  int game_id;
  vector<double> slot2score;
  vector<string> slot2status; // OK, TL, PF, stderr
  int tick=0;
  //string reason; // "maxtick", "crash", "TL", "PF"
  //double duration_sec;
  //time_t finished_at;
  string serialize()const{return {};}
};
struct t_cdn_game:t_finished_game{
  struct t_player{
    string err;
    vector<double> tick2ms;
  };
  vector<t_player> slot2player;
  vector<vector<t_cmd>> tick2cmds;
  string serialize()const{return {};}
};
struct t_main : t_process,t_http_base {
  vector<t_coder_rec> carr;
  vector<t_game_rec> garr;
  map<string, string> node2ipport;
  t_net_api capi;
  map<int, emitter_on_data_decoder> client_decoders;mutex cds_mtx;mutex n2i_mtx;
  void on_client_data(int client_id, const string& data, function<void(const string&)> send) {
    lock_guard<mutex> lock(cds_mtx);
    auto& decoder = client_decoders[client_id];
    if (!decoder.cb) {
      decoder.cb = [this, client_id, send](const string& z, const string& payload) {
        if (z == "new_game") {
          t_game_decl gd = parse<t_game_decl>(payload);
          //create_new_game_on_node("node1", gd); // или по IP
          //send("ok,game_received\n");
        }
        if (z == "game_finished") {
          auto result = parse<t_finished_game>(payload);
          //update_elo(result);
          //send("ok,result_received\n");
        } else {
          //send("error,unknown_command\n");
        }
      };
    }
    decoder.feed(data.data(), data.size());
  }
  static string node(int client_id){return "node"+to_string(client_id);}
  void on_client_connected(int client_id, socket_t sock, const string& ip) {
    cout << "[t_main] client connected: " << ip << " (id=" << client_id << ")\n";
    //lock_guard<mutex> lock(n2i_mtx);node2ipport[node(client_id)] = ip + ":31456";
  }
  void on_client_disconnected(int client_id) {
    cout << "[t_main] Node disconnected: " << client_id << "\n";
    {lock_guard<mutex> lock(cds_mtx);client_decoders.erase(client_id);}
    //{lock_guard<mutex> lock(n2i_mtx);node2ipport.erase(node(client_id));}
  }
  t_coder_rec*coder2rec(const string&coder){for(auto&ex:carr){if(ex.name!=coder)continue;return &ex;}return nullptr;}
  t_coder_rec*email2rec(const string&email){for(auto&ex:carr){if(ex.email!=email)continue;return &ex;}return nullptr;}
  // t_site->t_main
  struct t_new_src_resp{
    string v,time;
    string err;
  };
  t_new_src_resp new_source(const string&coder,const string&src){
    auto*p=coder2rec(coder);if(!p){LOG("coder_rec not found for "+coder);return {"", "", "coder_rec_not_found"};}
    string v=to_string(p->sarr.size());
    t_coder_rec::t_source b;
    b.time=qap_time();
    b.cdn_src_url="source/"+p->id+"_"+v+".cpp";
    b.cdn_bin_url="binary/"+p->id+"_"+v+".cpp";
    int src_id=-1;
    {lock_guard<mutex> lock(*p->sarr_mtx);src_id=p->sarr.size();p->sarr.push_back(b);}
    auto s=http_put_with_auth(b.cdn_src_url,src,UPLOAD_TOKEN);
    if(s!=200){
      lock_guard<mutex> lock(*p->sarr_mtx);
      auto&src=p->sarr[src_id];
      src.err="http_upload_err:"+to_string(s);
      src.prod_time=qap_time();
      return {v,b.time,to_string(s)};
    }
    json body_json;
    body_json["coder_id"] = p->id;
    body_json["elf_version"] = v;
    body_json["source_code"] = src;
    body_json["timeout_ms"] = 20000;
    body_json["memory_limit_mb"] = 512;
    auto resp=http_post_json_with_auth("compile",body_json.dump(),UPLOAD_TOKEN,COMPILER_URL);
    if(resp.status!=200){
      lock_guard<mutex> lock(*p->sarr_mtx);
      auto&src=p->sarr[src_id];
      src.err="compile:"+resp.body;
      src.prod_time=qap_time();
      return {v,b.time,"compile_api_error_"+to_string(s)};
    }
    {
      lock_guard<mutex> lock(*p->sarr_mtx);
      auto&src=p->sarr[src_id];
      src.err=resp.body;
      src.prod_time=qap_time();
      src.err="ok";
    }
    return {v,b.time};
  }
  struct t_new_coder_resp{
    string err;
    string id;
    string time;
  };
  mutex carr_mtx;
  t_new_coder_resp new_coder(const string&coder,const string&email){
    t_new_coder_resp out;
    lock_guard<mutex> lock(carr_mtx);
    if(coder2rec(coder)){out.err="name";return out;}
    if(email2rec(email)){out.err="email";return out;}
    out.id=to_string(carr.size());
    out.time=qap_time();
    auto&b=qap_add_back(carr);
    b.id=out.id;
    b.time=out.time;
    b.name=coder;
    b.email=email;
    b.sarr_mtx=make_unique<mutex>();
    b.token=sha256(b.time+b.name+b.email+to_string((rand()<<16)+rand())+"2025.08.23 15:10:42.466");// TODO: add more randomness
  }
  t_server_api server{31456};
  int main(int port=31456){
    carr.reserve(31456*3);
    server.port=port;
    server.onClientConnected = [this](int id, socket_t sock, const string& ip) {
      on_client_connected(id, sock, ip);
    };
    server.onClientDisconnected = [this](int id) {
      on_client_disconnected(id);
    };
    server.onClientData = [this](int id, const string& data, function<void(const string&)> send) {
      on_client_data(id, data, send);
    };
    http_server_main();
    server.start();//server.
    cout << "[t_main] Server started on port "+to_string(port)+"\n";
    cout << "Press Enter to stop...\n";
    string dummy;
    getline(cin, dummy);

    server.stop();
    return 0;
  }

  httplib::Server srv;
  void http_server_main(){
    thread([&](){
      this_thread::sleep_for(600s);
      cleanup_old_clients();
    }).detach();
    thread([this](){
      setup_routes();
      if(!srv.listen("0.0.0.0",80)){
        cerr << "[t_site] Failed to start server\n";
        return -1;
      }
      return 0;
    }).detach();;
  }
  void setup_routes(){
    srv.Post("/coder/new",[this](const httplib::Request& req, httplib::Response& res) {
    try {
      auto j = json::parse(req.body);

      string name = j.value("name", "");
      string email = j.value("email", "");

      if (name.empty() || email.empty()) {
        res.status = 400;
        res.set_content("Missing required fields: name or email", "text/plain");
        return;
      }

      {
        lock_guard<mutex> lock(carr_mtx);
        // Проверяем уникальность имени и email
        for (const auto& c : carr) {
          if (c.name == name) {
            res.status = 409;
            res.set_content("Duplicate coder name", "text/plain");
            return;
          }
          if (c.email == email) {
            res.status = 409;
            res.set_content("Duplicate coder email", "text/plain");
            return;
          }
        }
        // Создаем запись нового кодера
        t_coder_rec b;
        b.id = to_string(carr.size());
        b.time = qap_time();
        b.name = name;
        b.email = email;
        b.token = sha256(b.time + name + email + to_string((rand() << 16) + rand()) + "2025.08.23 15:10:42.466");
        b.sarr_mtx = make_unique<mutex>();

        carr.push_back(move(b));
      }

      // Возвращаем информацию о новом кодере
      json resp_json;
      resp_json["id"] = carr.back().id;
      resp_json["token"] = carr.back().token;
      resp_json["time"] = carr.back().time;

      res.status = 200;
      res.set_content(resp_json.dump(), "application/json");
    }
    catch (const exception& e) {
      res.status = 500;
      res.set_content(string("Exception: ") + e.what(), "text/plain");
    }
    });
    srv.Post("/source/new",[this](const httplib::Request& req, httplib::Response& res) {
      try {
        auto j = json::parse(req.body);
        string coder = j.value("coder", "");
        string src = j.value("src", "");
        string token = j.value("token", "");
        if (coder.empty() || src.empty()|| token.empty()) {
          res.status = 400;
          res.set_content("Missing required fields: coder or src or token", "text/plain");
          return;
        }
        lock_guard<mutex> lock(carr_mtx);
        auto*p=coder2rec(coder);
        if(!p){res.status=404;res.set_content("not found","text/plain");return;}
        if(p->token!=token){res.status=403;res.set_content("unauthorized","text/plain");return;}
        if(!p->allowed_next_src_upload()){res.status=429;res.set_content("rate limit exceeded","text/plain");return;}
        thread([this,coder,src](){
          new_source(coder,src);
        }).detach();
        /*
        auto resp = new_source(coder, src);
        if (!resp.err.empty()) {
          res.status = 500;
          res.set_content("Error: " + resp.err, "text/plain");
          return;
        }
        json resp_json;
        resp_json["version"] = resp.v;
        resp_json["time"] = resp.time;
        res.status = 200;
        res.set_content(resp_json.dump(), "application/json");
        */
        res.status = 200;
        res.set_content("ok","text/plain");
      }
      catch (const std::exception& e) {
        res.status = 500;
        res.set_content(std::string("Exception: ") + e.what(), "text/plain");
      }
    });
  }

};

#ifdef _WIN32
static FILE*popen(...){return nullptr;}
static int mkfifo(...){return 0;}
static int open(...){return 0;}
static int write(...){return 0;}
static int qap_close(...){return 0;}
static int read(...){return 0;}
static int poll(...){return 0;}
//static constexpr int O_WRONLY=0;
static constexpr int O_NONBLOCK=0;
//static constexpr int O_RDONLY=0;
static inline pollfd make_pollin(...){return {};}
static inline pollfd make_pollinout(...){return {};}
#define unlink(...)
#define getsockopt(...)0
#else
static inline pollfd make_pollin(int fd){return pollfd{fd,POLLIN,0};}
static inline pollfd make_pollinout(int fd){return pollfd{fd,POLLIN|POLLOUT,0};}
#define qap_close close
#endif

#include <cstdlib>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

bool build_image() {
  cout << "[t_main] Building Docker image...\n";
  string cmd = "docker build -t " + IMAGE_NAME + " -f " + DOCKERFILE_PATH + " .";
  int result = system(cmd.c_str());
  if (result != 0) {
    cerr << "[t_main] Failed to build image\n";
    return false;
  }
  cout << "[t_main] Image built: " + IMAGE_NAME + "\n";
  return true;
}

bool save_image() {
  cout << "[t_main] Saving image to tar...\n";
  string cmd = "docker save " + IMAGE_NAME + " -o " + ARCHIVE_NAME;
  int result = system(cmd.c_str());
  if (result != 0) {
    cerr << "[t_main] Failed to save image\n";
    return false;
  }
  cout << "[t_main] Image saved to " + ARCHIVE_NAME + "\n";
  return true;
}

bool upload_to_cdn() {
  cout << "[t_main] Uploading to CDN...\n";
  string cmd = "curl -X PUT -T " + ARCHIVE_NAME + " "
               "-H 'Authorization: Bearer " + UPLOAD_TOKEN + "' "
               + CDN_URL_IMAGES + "images/universal-runner.tar";
  int result = system(cmd.c_str());
  if (result != 0) {
    cerr << "[t_main] Failed to upload to CDN\n";
    return false;
  }
  cout << "[t_main] Image uploaded to CDN\n";
  return true;
}

void publish_runner_image() {
  if (!fs::exists(DOCKERFILE_PATH)) {
    cerr << "[t_main] Dockerfile not found: " + DOCKERFILE_PATH + "\n";
    return;
  }
  if (!build_image()) return;
  if (!save_image()) return;
  if (!upload_to_cdn()) return;
  cout << "[t_main] Runner image published and ready for t_node\n";
}

bool download_image_from_cdn() {
  cout << "[t_node] Downloading runner image from CDN...\n";
  string cmd = "curl -o /tmp/universal-runner.tar " + string(CDN_URL_IMAGES) + "universal-runner.tar";
  int result = system(cmd.c_str());
  if (result != 0) {
    cerr << "[t_node] Failed to download image\n";
    return false;
  }
  cout << "[t_node] Image downloaded\n";
  return true;
}

bool load_image() {
  cout << "[t_node] Loading image into Docker...\n";
  string cmd = "docker load -i /tmp/universal-runner.tar";
  int result = system(cmd.c_str());
  if (result != 0) {
    cerr << "[t_node] Failed to load image\n";
    return false;
  }
  cout << "[t_node] Image loaded into Docker\n";
  return true;
}

bool ensure_runner_image() {
  string cmd = "docker image inspect universal-runner:latest > /dev/null 2>&1";
  if (system(cmd.c_str()) == 0) {
      cout << "[t_node] Runner image already exists\n";
      return true;
  }
  if (!download_image_from_cdn()) return false;
  if (!load_image()) return false;
  fs::remove("/tmp/universal-runner.tar");
  return true;
}

#include "t_node.hpp"

int main() {
  srand(time(0));
  if(bool prod=true){
    string mode="t_cdn";
    if("t_main"==mode){
      publish_runner_image();
      t_main m;
      return m.main();
    }
    if("t_node"==mode){
      if(!ensure_runner_image())return -3145601;
      t_node n;
      return n.main();
    }
    if("t_cdn"==mode){
      t_cdn n;
      return n.main();
    }
  }
  t_net_api api;
  //string line;
  //if (api.readline_from_socket("127.0.0.1:80", line)) {
  //  std::cout << "Received: " << line << std::endl;
  //}

  api.write_to_socket("127.0.0.1:31456", "POST / HTTP/1.1\r\n\r\n\r\n");
  string line;
  for(;;){
    line.clear();
    api.readline_from_socket("127.0.0.1:31456",line);
  }
  //api.close_socket("82.208.124.191:80");

  return 0;
}
