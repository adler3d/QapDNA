#include "netapi.h"
#include "FromQapEng.h"
#include "thirdparty/picosha2.h"
#include <vector>
#include <cmath>
#include <string>
#include <chrono>
#include <future>

using namespace std;
using namespace std::chrono;

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
string generate_token(string coder_name,string timestamp) {
  return "token:"+to_string((rand()<<16)+rand())+coder_name+timestamp;
  //return sha256( random_bytes(32) + timestamp + coder_name );
}
template<class TYPE>TYPE parse(const string_view&s){return {};}
string serialize(...){return "nope";}
struct t_client20250817{
  struct t_input{
    struct t_visible_part_of_world{};
    struct t_allowed_cmds{};
    t_visible_part_of_world vpow;
    t_allowed_cmds cmds;
    string status="ok";
  };
  struct t_ai{
    struct t_cmd{};
    t_cmd decide_next_action(t_input::t_visible_part_of_world w,t_input::t_allowed_cmds cmds){
      return {};
    };
  };
  string main_ip_port=1?local_main_ip_port:"82.208.124.191:31456";
  t_ai ai;
  t_net_api api;
  void worker(string rw){
    string line;
    while(api.readline_from_socket(rw,line)){
      auto inp=parse<t_input>(line);
      if(inp.status!="ok")break;
      auto cmd=ai.decide_next_action(inp.vpow,inp.cmds);
      string serialized=serialize(cmd);
      api.write_to_socket(rw,serialized+"\n");
    }
    api.close_socket(rw);
  }
  int max_games=10;
  string token="default";
  string coder="unk";
  string email="email@example.com";
  int main(){
    load_config();
    if(token=="default"){
      api.write_to_socket(main_ip_port,"hi_i_default_and_idk_me_token,"+coder+","+to_string(max_games)+","+email+"\n");
      string resp;
      if(!api.readline_from_socket(main_ip_port,resp))return -1;
      if(resp=="u_banned")return -2;
      if(resp=="u_already_say_this")return -3;
      token=resp;
      save_config();
    }else{
      api.write_to_socket(main_ip_port,"hi_i_have_token,"+token+","+coder+","+to_string(max_games));
    }
    for(;;){
      string line;
      if(!api.readline_from_socket(main_ip_port,line))return 0;
      if(line=="ok")continue;
      if(line=="fail")return -4;
      if(line=="req_limit_reached")return -5;
      auto v=split(line,",");
      if(v[0]=="game_aborted")continue;
      auto sock=line;
      std::thread t([&,sock](){worker(sock);});
      t.detach();
    }
    return 0;
  }
  string config_fn="t_client.cfg";
  void save_config(){
    vector<string> arr;
    arr.push_back(to_string(max_games));
    arr.push_back(token);
    arr.push_back(coder);
    arr.push_back(email);
    file_put_contents(config_fn,join(arr,","));
  }
  void load_config(){
    auto s=file_get_contents(config_fn);
    auto arr=split(s,",");
    if(arr.size()!=4)return;
    max_games=std::stoi(arr[0]);
    token=arr[1];
    coder=arr[2];
    email=arr[3];
  }
};
struct t_main20250817{
  struct t_coder{
    string coder;
    string token="def";
    string email;
    int max_games=10;
    string ip;
    int total_games=0;
    vector<string> sources;
  };
  struct t_game_rec{
    struct t_slot{string coder;double score=0;string err;string status;};
    vector<t_slot> arr;
    string cdn_file;
    int tick=0;
  };
  vector<t_game_rec> garr;
  t_server_api server{31465};
  map<string,bool> ip2ban;
  map<string,t_coder> coder2acc;
  map<int,string> cid2ip;
  map<string,int> ip2n;
  int main(){
    server.onClientConnected=[&](int client_id,socket_t socket,const std::string&ip){
      std::cout << "[" << client_id << "] connected from IP " << ip << "\n";
      if(ip2ban[ip]){
        std::string greeting="u_banned\n";
        send(socket,greeting.c_str(),static_cast<int>(greeting.size()),0);
        server.disconnect_clients_by_ip(ip);
        return;
      }
      cid2ip[client_id]=ip;
    };

    server.onClientDisconnected = [&](int client_id) {
      std::cout << "[" << client_id << "] disconnected\n";
    };
    const int max_games=16;
    server.onClientData=[&](int client_id,const std::string& data,std::function<void(const std::string&)> send){
      std::cout << "[" << client_id << "] received: " << data;
      auto v=split(data,",");
      if(v[0]=="hi_i_default_and_idk_me_token"){
        #define NO_WAY(ANSWER){send(string(ANSWER)+"\n");server.disconnect_clients_by_ip(cid2ip[client_id]);return;}
        if(v.size()!=4){NO_WAY("wrong_number_of_params");}
        if(v[1].empty()){NO_WAY("your_name_is_empty");}
        if(v[1].size()>42){NO_WAY("your_name_is_too_long");}
        if(v[3].empty()){NO_WAY("your_email_is_empty");}
        if(v[3].size()>42){NO_WAY("your_email_is_too_long");}
        //(имя_кодера,сколько_одновременно_игр_он_тянет,email)
        auto&acc=coder2acc[v[1]];
        if(!acc.coder.empty()){NO_WAY("coder_name_already_used");}
        auto&ip=cid2ip[client_id];
        if(ip2n[ip]){NO_WAY("u_already_say_this");}
        ip2n[ip]++;
        acc.max_games=std::max(1,std::min(max_games,std::stoi(v[2])));
        acc.email=v[3];
        acc.ip=ip;
        acc.token=generate_token(acc.coder,"2025.08.16 22:05:03.625"+acc.ip);
        return;
      }else if(v[0]=="hi_i_have_token"){
        //(токен,имя_кодера,сколько_одновременно_игр_он_тянет)
        if(v.size()!=4){NO_WAY("wrong_number_of_params");}
        auto it=find_if(coder2acc.begin(),coder2acc.end(),[&](auto&p){
          return p.second.token==v[1];
        });
        if(it==coder2acc.end()){NO_WAY("invalid_token");}
        auto&acc=it->second;
        if(acc.coder!=v[2]){NO_WAY("wrong_coder");}
        acc.max_games=std::max(1,std::min(max_games,std::stoi(v[3])));
        send("ok\n");
        return;
        #undef NO_WAY
      }

      send("Message received\n");
    };

    server.start();

    std::cout << "Press Enter to stop server...\n";
    std::string dummy;
    std::getline(std::cin, dummy);

    std::cout << "Disconnecting clients from IP 127.0.0.1\n";
    server.disconnect_clients_by_ip("127.0.0.1");

    server.stop();
  }
};

struct t_process{
  string machine;
  string name;
  //string main;
};

struct t_coder_rec{
  string name;
  string token;
  string email;
  vector<string> sources;
  int total_games=0;
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
  int maxtick=20000;
  int stderr_max=1024*64;
  double TL=50.0;
  double TL0=1000.0;
};
struct t_main:t_process{
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
//struct t_player{
//  string coder;
//  string app;
//};
//struct t_game{
//  vector<t_player> players;
//  string config;
//  int tick=0;
//  int maxtick=20000;
//};
#ifdef _WIN32
static FILE*popen(...){return nullptr;}
static int mkfifo(...){return 0;}
static int open(...){return 0;}
static int write(...){return 0;}
static int close(...){return 0;}
static int read(...){return 0;}
static int poll(...){return 0;}
static constexpr int O_WRONLY=0;
static constexpr int O_NONBLOCK=0;
static constexpr int O_RDONLY=0;
static inline pollfd make_pollin(...){return {};}
static inline pollfd make_pollinout(...){return {};}
#define unlink(...)
#define getsockopt(...)0
#else
static inline pollfd make_pollin(int fd){return pollfd{fd,POLLIN,0};}
static inline pollfd make_pollinout(int fd){return pollfd{fd,POLLIN|POLLOUT,0};}
#endif

// t_main.cpp

#include <cstdlib>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

const string DOCKERFILE_PATH = "./docker/universal-runner/Dockerfile"; //TODO: check this file must exists!
const string IMAGE_NAME = "universal-runner:latest";
const string ARCHIVE_NAME = "/tmp/universal-runner.tar";
const string CDN_URL = "https://cdn.your-system.com/images/"; //TODO: replace to t_main "ip:port/images/"

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

/*bool upload_to_cdn() {
  cout << "[t_main] Uploading to CDN...\n";
  string cmd = "curl -X PUT -T " + ARCHIVE_NAME + " " + CDN_URL + "universal-runner.tar";
  int result = system(cmd.c_str());
  if (result != 0) {
    cerr << "[t_main] Failed to upload to CDN\n";
    return false;
  }
  cout << "[t_main] Image uploaded to CDN\n";
  return true;
}*/

void publish_runner_image() {
  if (!fs::exists(DOCKERFILE_PATH)) {
    cerr << "[t_main] Dockerfile not found: " + DOCKERFILE_PATH + "\n";
    return;
  }
  if (!build_image()) return;
  if (!save_image()) return;
  //if (!upload_to_cdn()) return;
  cout << "[t_main] Runner image published and ready for t_node\n";
}

// t_node.cpp

bool download_image_from_cdn() {
  cout << "[t_node] Downloading runner image from CDN...\n";
  string cmd = "curl -o /tmp/universal-runner.tar " + string(CDN_URL) + "universal-runner.tar";
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

const string t_node_cache_dir="/t_node_cache/bins/";
struct t_node_cache{
  struct t_lru_cache{
    string log_file="/t_node_cache/lru.log";
    const size_t mb=1024*1024;
    size_t max_size=5*1024*mb;
    size_t current_size=0;
    map<string,time_t> access_time;//hash2at
    mutex mtx;
    static string get_hash(const string&url){return sha256(url);}
    static string get_path(const string&hash){return t_node_cache_dir+hash+".bin";}
    void touch(const string&hash){
      lock_guard<mutex> lock(mtx);
      access_time[hash]=time(0);
      ofstream log(log_file,ios::app);
      if(!log)return;
      log<<access_time[hash]<<" "<<hash<<"\n";
    }
    void cleanup(){
      lock_guard<mutex> lock(mtx);
      vector<tuple<time_t,string,string>> files;
      for(auto&[hash,t]:access_time){
        string path=get_path(hash);
        struct stat buf;
        if(stat(path.c_str(),&buf)!=0)continue;
        files.push_back({t,path,hash});
      }
      sort(files.begin(),files.end());
      size_t freed=0;
      for(auto&[t,path,hash]:files){
        struct stat buf;
        if(stat(path.c_str(),&buf)!=0)continue;
        unlink(path.c_str());
        access_time.erase(hash);
        freed+=buf.st_size;
        if(freed>=1024*mb)break;
      }
    }
    bool ensure_space(size_t needed){
      if(current_size+needed<=max_size){
        return true;
      }
      cleanup();
      return (current_size+needed)<=max_size;
    }
  };
  t_lru_cache lru;
  static void LOG(...){}
  static string get_cache_path(const string&fn){
    return t_node_cache_dir+sha256(fn)+".bin";
  }
  bool load_from_cache(const string&fn,string&out){
    string path=get_cache_path(fn);
    out=file_get_contents(path);
    return true;
  }
  bool save_to_cache(const string&fn,const string&mem) {
    string path=get_cache_path(fn);
    system(("mkdir -p "+t_node_cache_dir).c_str());
    file_put_contents(path,mem);
    return true;
  }
  bool cached_download(const string&fn,string&mem){
    auto path=get_cache_path(fn);
    if(load_from_cache(fn,mem)){
      lru.touch(lru.get_hash(fn));
      return true;
    }
    struct stat buf;
    if(stat(path.c_str(),&buf)==0){
      unlink(path.c_str());
    }
    string tmp_file="/tmp/dl_"+to_string(rand())+to_string(rand())+to_string(rand());
    string cmd="curl -s -f -o "+tmp_file+" "+fn;
    int result=system(cmd.c_str());
    if(result!=0){
      unlink(tmp_file.c_str());
      LOG("CURL CANT LOAD BINARY FROM "+fn);
      return false;
    }
    mem=file_get_contents(tmp_file);
    unlink(tmp_file.c_str());
    save_to_cache(fn,mem);
    {
      lock_guard<mutex> lock(lru.mtx);
      if(!lru.ensure_space(mem.size())){
        LOG("NOT UNOUGHT SPACE IN FS FOR "+fn);
        return false;
      }
      lru.current_size+=mem.size();
    }
    lru.touch(lru.get_hash(fn));
    return true;
  }
};
struct tcp_client {
  int sock = -1;
  bool connected=false;
  bool connect_unix(const string& path) {
    #ifdef _WIN32
    cerr << "Unix sockets not supported on Windows\n";
    return false;
    #else
    sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) return false;
    make_nonblocking(sock);
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      close();
      return false;
    }
    return true;
    #endif
  }
  #ifndef _WIN32
  static int make_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
  }
  #endif
  void write(const void* data, size_t len) {
    if (sock != -1) ::send(sock, (const char*)data, len, 0);
  }
  void write(const string&s) {
    write(s.data(),s.size());
  }

  string read_chunk() {
    char buf[4096];
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n <= 0) { close(); return ""; }
    return string(buf, n);
  }

  int read(char*buf,int size) {
    int n = recv(sock, buf, size, 0);
    if (n <= 0) { close();}
    return n;
  }

  void close() {
    if (sock != -1) { ::close(sock); sock = -1; }
  }
};
struct stream_write_encoder {
  tcp_client& socket;
  stream_write_encoder(tcp_client& s) : socket(s) {}

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
};
void stream_write(tcp_client& client, const string& z, const string& data) {
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

/*struct emitter_on_data_decoder {
  using callback_t = function<void(const string& z, const string& msg)>;

  tcp_client* client;
  string buffer;
  callback_t cb;

  emitter_on_data_decoder(tcp_client* c, callback_t f) : client(c), cb(f) {}

  void update() {
    string chunk = client->read_chunk();
    if (chunk.empty()) {
      if (client->is_closed()) {
        // Сокет закрыт
        on_error("socket closed");
      }
      return; // нет данных — норма
    }

    buffer += chunk;

    while (true) {
      auto e1 = buffer.find('\0');
      if (e1 == string::npos) break;

      string len_str = buffer.substr(0, e1);
      if (!is_numeric(len_str)) { on_error("invalid length"); return; }
      int len = stoi(len_str);

      auto e2 = buffer.find('\0', e1 + 1);
      if (e2 == string::npos) break;

      int total_header = e2 + 1;
      if (buffer.size() < total_header + len) break;

      string z = buffer.substr(e1 + 1, e2 - e1 - 1);
      string payload = buffer.substr(total_header, len);
      buffer.erase(0, total_header + len);

      cb(z, payload);
    }

    if (buffer.size() > 1024 * 1024*64) {
      on_error("buffer overflow");
    }
  }
  void on_error(...){}
};
*/
struct t_node:t_process,t_node_cache{
  struct t_pipe_names{
    string in;   // /tmp/ai_in_{game_id}_{player_id}
    string out;  // /tmp/ai_out_{game_id}_{player_id}
    string err;  // /tmp/ai_err_{game_id}_{player_id}
  };
  t_pipe_names make_pipe_names(int game_id,int player_id){
    return {
      "/tmp/ai_in_" +to_string(game_id)+"_"+to_string(player_id)+"_"+to_string(rand()),
      "/tmp/ai_out_"+to_string(game_id)+"_"+to_string(player_id)+"_"+to_string(rand()),
      "/tmp/ai_err_"+to_string(game_id)+"_"+to_string(player_id)+"_"+to_string(rand())
    };
  }
  static void cleanup_pipes(const t_pipe_names&names){
    unlink(names.in.c_str());
    unlink(names.out.c_str());
    unlink(names.err.c_str());
  }
  static bool create_pipes(const t_pipe_names&names){
    if(mkfifo(names.in.c_str(), 0666)==-1&&errno!=EEXIST)return false;
    if(mkfifo(names.out.c_str(),0666)==-1&&errno!=EEXIST)return false;
    if(mkfifo(names.err.c_str(),0666)==-1&&errno!=EEXIST)return false;
    return true;
  }
  struct i_output{
    virtual void on_stdout(const string_view&data)=0;
    virtual void on_stderr(const string_view&data)=0;
    virtual void on_closed_stdout()=0;
    virtual void on_closed_stderr()=0;
    virtual void on_stdin_open()=0;
  };
  struct i_input{
    virtual void write_stdin(const string&data)=0;
    virtual void close_stdin()=0;
  };
  struct t_runned_game;
  struct t_docker_api_v2 : i_input, i_output {
    int player_id;
    t_runned_game* pgame = nullptr;
    t_node* pnode = nullptr;

    tcp_client socket;
    string err;
    string container_id;
    string socket_path_in_container = "/tmp/dokcon.sock";
    string socket_path_on_host;  // например: /tmp/dokcon_game1_p0.sock
    shared_ptr<emitter_on_data_decoder> decoder;
    void on_stdout(const string_view& data) override {
      pnode->on_player_stdout(*pgame, player_id, data);
    }

    void on_stderr(const string_view& data) override {
      if (data.size() + err.size() < pgame->gd.stderr_max)
        err += data;
    }

    void on_closed_stdout() override {}
    void on_closed_stderr() override {}
    void on_stdin_open() override {}

    void write_stdin(const string& data) override {
      if (socket.connected) {
        stream_write(socket, "ai_stdin",data);
      }
    }

    void close_stdin() override {
      if (socket.connected) {
        stream_write(socket, "ai_stdin","EOF\n");
      }
    }
  };
  struct t_player_ctx:i_output{
    int player_id;
    string coder;
    t_runned_game*pgame=nullptr;
    t_node*pnode=nullptr;
    string err;
    string conid;
    vector<double> time_log;
    void on_stdout(const string_view&data)override{
      pnode->on_player_stdout(*pgame,player_id,data);
    }
    void on_stderr(const string_view&data)override{
      if(data.size()+err.size()<pgame->gd.stderr_max)err+=data;
    }
    void on_closed_stderr()override{}
    void on_closed_stdout()override{}
    void on_stdin_open()override{}
  };
  struct t_docker_api{
    unique_ptr<i_input>  input;
    unique_ptr<t_player_ctx> output;
    void send(const string&data){if(input)input->write_stdin(data);}
  };
  struct t_status{bool TL=false;bool PF=false;bool ok()const{return !TL&&!PF;}};
  struct t_cmd{bool valid=true;};
  struct t_world{
    void use(int player_id,const t_cmd&cmd){}
    void step(){};
    bool finished(){return false;}
  };
  struct t_runned_game{
    t_game_decl gd;
    int tick=0;
    bool started=false;
    vector<unique_ptr<t_docker_api>> slot2api;
    vector<unique_ptr<t_docker_api_v2>> slot2v2;
    vector<t_cmd> slot2cmd;
    vector<t_status> slot2status;
    vector<int> slot2ready;
    vector<int> ready_sent;
    vector<string> slot2bin;
    t_world w;
    vector<t_pipe_names> pipes;
    void init(){
      slot2cmd.resize(gd.arr.size());
      slot2status.resize(gd.arr.size());
      slot2ready.resize(gd.arr.size());
      ready_sent.resize(gd.arr.size());
      slot2bin.resize(gd.arr.size());
    }
    void free(){for(auto&ex:pipes)cleanup_pipes(ex);pipes.clear();}
    ~t_runned_game(){free();}
    void new_tick(){for(auto&ex:slot2ready)ex=false;}
    bool all_ready(){
      for(int i=0;i<slot2cmd.size();i++){
        if(slot2ready[i]||!slot2status[i].ok())continue;
        return false;
      }
      return true;
    }
  };
  static constexpr int buff_size=1024*64;
  bool download_binary(const string& cdn_url, string& out_binary) {
    string tmp_file = "/tmp/ai_bin_" + to_string(rand());
    string cmd = "curl -s -f -o " + tmp_file + " " + cdn_url;
    int result = system(cmd.c_str());
    if (result != 0) {
      unlink(tmp_file.c_str());
      return false;
    }
    out_binary=file_get_contents(tmp_file);
    unlink(tmp_file.c_str());
    return true;
  }
  static void kill(const string&conid){
    string cmd="docker kill "+conid;
    system(cmd.c_str());
  }
  struct t_container_monitor{
    static void kill(t_runned_game&g,int pid){
      auto&conid=g.slot2api[pid]->output.get()->conid;
      t_node::kill(conid);
    }
    struct t_task{
      t_runned_game*pgame=nullptr;
      int player_id;
      double started_at;
      double ms;
      bool done=false;
      void on_done(QapClock&clock){
        done=true;
        ms=clock.MS()-started_at;
        get().time_log.push_back(ms);
      }
      t_player_ctx&get(){return *pgame->slot2api[player_id]->output.get();}
      void kill_if_TL(double ms){
        auto&gd=pgame->gd;
        auto TL=pgame->tick?gd.TL:gd.TL0;
        if(ms<=TL)return;
        kill(*pgame,player_id);
        pgame->slot2status[player_id].TL=true;
      }
    };
    vector<t_task> tasks;
    t_node*pnode=nullptr;
    QapClock clock;
    void add(t_runned_game*pgame,int player_id) {
      tasks.push_back({pgame,player_id,clock.MS()});
    }
    //static constexpr int MAX_TICK_TIME=50;
    void donekiller(){
      for(auto&t:tasks)if(t.done){
        t.kill_if_TL(t.ms);
      }
    }
    void update(){
      donekiller();
      auto now=clock.MS();
      for(auto&t:tasks)if(!t.done){
        t.kill_if_TL(now-t.started_at);
      }
    }
  };
  t_container_monitor container_monitor;
  bool spawn_docker_old(const string&fn,t_docker_api&api,int game_id,int player_id,t_runned_game&g){
    string container_id="game_"+to_string(game_id)+"_p"+to_string(player_id)+"_"+to_string(rand());
    api.output->conid=container_id;
    auto names=make_pipe_names(game_id,player_id);
    if(!create_pipes(names)){
      api.output->on_stderr("create_pipes failed\n");
      return false;
    }
    string binary;
    if(!download_binary(fn,binary)){
      api.output->on_stderr("download failed\n");
      return false;
    }
    g.slot2bin[player_id]=binary;
    g.pipes.push_back(names);
    add_to_event_loop(names.out,&g,player_id,true);
    add_to_event_loop(names.err,&g,player_id,false);
    string itag="universal-runner:latest";//get_image_tag(fn);
    string cmd = "docker run -i --rm "
                 "--name " + container_id + " "
                 "--memory=512m --cpus=1 --network=none --read-only "
                 "--mount type=pipe,src=" + names.in  + ",dst=/dev/stdin "
                 "--mount type=pipe,src=" + names.out + ",dst=/dev/stdout "
                 "--mount type=pipe,src=" + names.err + ",dst=/dev/stderr "
                 +itag;
    int result=system(cmd.c_str());
    if(result!=0){
      api.output->on_stderr("docker run failed\n");
      return false;
    }
    struct t_stdin_writer:i_input{
      int fd=-1;
      void send_to_player(const string&data){
        if(fd==-1)return;
        write(fd,data.c_str(),data.size());
      }
      void write_stdin(const string&data){send_to_player(data);};
      void close_stdin(){if(fd==-1)return;close(fd);fd=-1;};
    };
    auto inp=make_unique<t_stdin_writer>();
    inp->fd=open(names.in.c_str(),O_WRONLY|O_NONBLOCK);
    api.input=std::move(inp);
    return true;
  }
  bool spawn_docker(const string& cdn_url, t_docker_api_v2& api, int game_id, int player_id, t_runned_game& g) {
    // 1. Генерируем уникальный ID и пути
    api.container_id = "game_" + to_string(game_id) + "_p" + to_string(player_id) + "_" + to_string(rand());
    api.socket_path_on_host = "/tmp/dokcon_" + api.container_id + ".sock";

    // Удаляем старый сокет, если есть
    unlink(api.socket_path_on_host.c_str());

    // 2. Скачиваем бинарник
    string binary;
    if (!download_binary(cdn_url, binary)) {
      api.on_stderr("download failed\n");
      return false;
    }
    g.slot2bin[player_id] = binary;

    string cmd = "docker run -d --rm "
                 "--name " + api.container_id + " "
                 "--mount type=bind,src=" + api.socket_path_on_host + ",dst=/tmp/dokcon.sock "
                 "--mount type=tmpfs,tmpfs-size=64m,destination=/tmpfs "
                 "dokcon-runner:latest";

    int result = system(cmd.c_str());
    if (result != 0) {
      // Отправить ошибку в i_output
      api.on_stderr("docker run failed: " + to_string(result) + "\n");
      return false;
    }
    /*
    // 3. Запускаем в фоне
    thread([cmd, api, this, &g, player_id, game_id]() mutable {
      int result = system(cmd.c_str());
      if (result != 0) {
        // Отправить ошибку в i_output
        api.on_stderr("docker run failed: " + to_string(result) + "\n");
        return;
      }
    }).detach();*/
    auto*api_ptr=&api;
    loop_v2.add_unix_socket(api.socket_path_on_host, [this,api_ptr,binary](tcp_client& client, int fd) {
      api_ptr->socket = client;

      // 2. Отправляем команды
      stream_write(api_ptr->socket, "ai_binary", binary);
      stream_write(api_ptr->socket, "ai_start", "");

      // 3. Начинаем слушать
      start_reading(*api_ptr);
    });

    /*
    // 4. Планируем подключение к сокету (не ждём!)
    loop.add_connect_task(api.socket_path_on_host, [api,binary](bool ok){
      if (!ok) {
        api.on_stderr("connect to socket failed\n");
        return;
      }
      // Успешно подключились
      stream_write_encoder(api.socket, "ai_binary")(binary);
      stream_write_encoder(api.socket, "ai_start")("");
      start_message_stream(api); // начинаем слушать
    });*/
    return true;
  }
  void start_reading(t_docker_api_v2& api) {
    auto decoder = make_shared<emitter_on_data_decoder>();
    decoder->cb=[&api](const string& z, const string& msg) {
      if (z == "ai_stdout") {
        api.on_stdout(string_view(msg));
      } else if (z == "ai_stderr") {
        api.on_stderr(string_view(msg));
      } else if (z == "log") {
        // Лог контроллера
        api.on_stderr("[CTRL] " + msg + "\n");
      }
    };
    api.decoder = decoder; 

    loop_v2.add(api.socket.sock, [this,decoder,&api](int fd) {
      char buf[4096];
      int n = api.socket.read(buf, sizeof(buf));
      if (n > 0) {
        decoder->feed(buf, n);
      } else {
        // Сокет закрыт
        api.on_closed_stdout();
        api.on_closed_stderr();
        loop_v2.remove(fd);
        close(fd);
      }
    });
  }
  static string config2seed(const string&config){return {};}
  vector<unique_ptr<t_runned_game>> rgarr;
  int game_n=0;
  void new_game(const t_game_decl&gd){
    auto&gu=qap_add_back(rgarr);
    gu=make_unique<t_runned_game>();
    auto&g=*gu.get();
    g.gd=gd;
    g.init();
    int i=-1;
    for(auto&ex:gd.arr){
      i++;
      auto&b=qap_add_back(g.slot2api);
      b=make_unique<t_docker_api>();
      auto pc=make_unique<t_player_ctx>();
      auto&o=*pc.get();
      o.coder=ex.coder;
      o.player_id=i;
      o.pnode=this;
      o.pgame=&g;
      b->output=std::move(pc);
      auto&c=*b.get();
      //spawn_docker(ex.cdn_bin_file,c,gd.game_id,i,g);
      auto&b2=qap_add_back(g.slot2v2);
      b2=make_unique<t_docker_api_v2>();
      t_docker_api_v2&v2=*b2.get();
      v2.pnode=this;
      v2.player_id=i;
      v2.pgame=&g;
      spawn_docker(ex.cdn_bin_file,v2,gd.game_id,i,g);
    }
    game_n++;
  }
  void send_game_result(t_runned_game&game){

  }
  void game_tick(t_runned_game&game){
    if(game.started){
      for(int i=0;i<game.slot2cmd.size();i++)game.w.use(i,game.slot2cmd[i]);
      game.w.step();
      if(game.w.finished()||game.tick>=game.gd.maxtick){
        send_game_result(game);
        for(int i=0;i<game.gd.arr.size();i++)container_monitor.kill(game,i);
        return;
      }
    }
    for(int i=0;i<game.slot2api.size();i++){
      if(!game.slot2status[i].ok())continue;
      string vpow=serialize(game.w,i);
      auto&api=game.slot2api[i];
      api->send("vpow,"+vpow+"\n");
      container_monitor.add(&game,i);
    }
  }
  void on_player_stdout(t_runned_game&g,int player_id,const string_view&data){
    string s(data);
    if(!g.started&&!g.ready_sent[player_id]){
      if(s.find("HI")==string::npos){
        auto&api=g.slot2api[player_id];
        api->output->on_stderr("\nERROR DETECTED: YOU MUST SAY HI IN FIRST MSG TO GET BINARY!!!\n");
        return;
      }
      auto&bin=g.slot2bin[player_id];
      g.slot2api[player_id]->send(bin);
      g.ready_sent[player_id]=true;
      bin.clear();
      return;
    }
    if(!g.ready_sent[player_id])return;// TODO: remove this because this is use less check?
    if(!g.started){
      if(s.find("READY")==string::npos){
        auto&api=g.slot2api[player_id];
        api->output->on_stderr("\nERROR DETECTED: YOU MUST SAY READY BEFORE FIRST ACTION!!!\n");
        return;
      }
      g.slot2ready[player_id]=true;
      if(g.all_ready()){
        tick(g);
        g.started=true;
      }
      return;
    }
    auto move=parse<t_cmd>(data);
    if(move.valid){
      g.slot2cmd[player_id]=move;
      auto&r=g.slot2ready[player_id];
      if(r){g.slot2api[player_id]->output->on_stderr("\nERROR DETECTED: ANSWER AFTER ANSWER!!!\n");}
      r=true;
      for(auto&ex:container_monitor.tasks)if(ex.player_id==player_id)ex.on_done(container_monitor.clock);
    }else{
      auto&api=g.slot2api[player_id];
      api->output->on_stderr("INVALID_MOVE: parsing failed\n");
      container_monitor.kill(g,player_id);
      g.slot2status[player_id].PF=true;
    }
    if(g.all_ready()){
      tick(g);
    }
  }
  void tick(t_runned_game&g){
    container_monitor.donekiller();
    container_monitor.tasks.clear();
    game_tick(g);
    if(g.started)g.tick++;
    g.new_tick();
  }

  struct t_event_loop_v2{
    struct t_monitored_fd {
      int fd;
      string path;  // для Unix-socket
      function<void(int fd)> on_ready;
      function<void()> on_error;
      bool connected = false;
    };
    vector<t_monitored_fd> fds;
    mutex mtx;
    t_node*pnode=nullptr;
    void remove(int fd) {
      lock_guard<mutex> lock(mtx);
      QapCleanIf(fds, [fd](const t_monitored_fd& f) { return f.fd == fd; });
    }
    void add(int fd, const function<void(int)>& on_ready, const function<void()>& on_error = []{}) {
      lock_guard<mutex> lock(mtx);
      fds.push_back({fd, "", on_ready, on_error});
    }

    void add_unix_socket(const string& path,const function<void(tcp_client&, int)>& on_connect) {
      tcp_client client;
      if (!client.connect_unix(path)) {
        LOG("client.connect_unix(path) failed with "+path); 
        return;
      }

      int fd = client.sock;

      auto wrapper = [this, client, on_connect,fd](int) mutable {
        if (!client.connected) {
          int err = 0;
          socklen_t len = sizeof(err);
          if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
            close(fd);LOG("add_unix_socket::wrapper failed"); 
            return;
          }
          client.connected = true;
          on_connect(client, fd);
        }
      };

      add(fd, wrapper);
    }

    void run() {
      vector<pollfd> pfds;
      while (true) {
        pfds.clear();
        {
          lock_guard<mutex> lock(mtx);
          for (auto& f : fds) {
            //pfds.push_back({f.fd, POLLIN | POLLOUT, 0});
            pfds.push_back(make_pollinout(f.fd));
          }
        }
        pnode->container_monitor.update();
        int n = poll(pfds.data(), pfds.size(), 1);  // 1ms таймаут — нормально
        if (n <= 0) continue;

        {
          lock_guard<mutex> lock(mtx);
          for (auto& pfd : pfds) {
            if (pfd.revents & (POLLERR | POLLHUP)) {
              for (auto& f : fds) {
                if (f.fd == pfd.fd) {
                  if (f.on_error) f.on_error();
                  close(pfd.fd);
                  remove(pfd.fd);  // удаляем из списка
                  LOG("Socket error on fd="+to_string(pfd.fd));
                  break;
                }
              }
              continue;
            }
            if (pfd.revents & POLLIN) {
              // data ready
              for (auto& f : fds) {
                if (f.fd == pfd.fd) {
                  f.on_ready(pfd.fd);
                }
              }
            }
          }
        }
      }
    }
  };
  struct t_event_loop{/*
    struct t_connect_task {
      t_node*pnode=nullptr;
      string path;
      function<void(bool)> cb;
      int attempts = 0;
      void try_connect() {
        tcp_client tmp;
        if (tmp.connect_unix(path)) {
          cb(true);
          return;
        }
        tmp.close();
        if (++attempts > 50) { // 50 попыток × 10мс = 500мс
          cb(false);
          return;
        }
        // Перепланировать
        pnode->loop.add_immediate([this] { try_connect(); });
      }
    };
    void add_connect_task(const string& path, function<void(bool)> cb) {
      t_connect_task task{pnode,path, cb};
      connect_tasks.push_back(move(task));
      connect_tasks.back().try_connect(); // начать попытки
    }*/
    struct t_monitored_pipe{
      int fd;
      string path;
      t_runned_game*game;
      int player_id;
      bool active;
    };
    struct t_fd_info{
      t_runned_game*g;
      int pid;
      bool out;
    };
    //vector<t_connect_task> connect_tasks;
    vector<t_monitored_pipe> pipes;
    map<int,t_fd_info> fd2info;
    mutex mtx;
    t_node*pnode=nullptr;
    void add(const string&path,t_runned_game*g,int pid,bool out){
      int fd=open(path.c_str(),O_RDONLY|O_NONBLOCK);
      if(fd==-1)return;
      lock_guard<mutex> lock(mtx);
      pipes.push_back({fd,path,g,pid,true});
      auto&i=fd2info[fd];
      i.g=g;i.pid=pid;i.out=out;
    }
    void run(t_node*pn){
      pnode=pn;
      pnode->container_monitor.pnode=pn;
      vector<pollfd> fds;
      for(;;){
        pnode->container_monitor.update();
        fds.clear();
        {
          lock_guard<mutex> lock(mtx);
          for(auto&p:pipes){
            if(p.active&&p.fd!=-1){
              fds.push_back(make_pollin(p.fd));
            }
          }
        }
        int ready=poll(fds.data(),fds.size(),10);
        if(ready<=0)continue;
        bool need_clean=false;
        for(auto&ex:fds){
          if(!(ex.revents&POLLIN))continue;
          char buf[buff_size];
          int n=read(ex.fd,buf,sizeof(buf)-1);
          t_fd_info i;
          {
            lock_guard<mutex> lock(mtx);
            i=fd2info[ex.fd];
          }
          auto&o=*i.g->slot2api[i.pid]->output.get();
          if(n>0){
            if(i.out){
              o.on_stdout(string_view(buf,n));
            }else{
              o.on_stderr(string_view(buf,n));
            }
          }else{
            {
              lock_guard<mutex> lock(mtx);
              fd2info.erase(ex.fd);
            }
            close(ex.fd);
            if(i.out){
              o.on_closed_stdout();
            }else{
              o.on_closed_stderr();
            }
            for(auto&p:pipes){
              if(p.fd!=ex.fd)continue;
              p.active=false;
              need_clean=true;
            }
          }
        }
        if(!need_clean)continue;
        lock_guard<mutex> lock(mtx);
        QapCleanIf(pipes,[](t_monitored_pipe&p){return !p.active;});
      }
      int gg=1;
    }
  };
  t_event_loop loop;t_event_loop_v2 loop_v2;
  void add_to_event_loop(const string&path,t_runned_game*g,int pid,bool out){
    loop.add(path,g,pid,out);
  }
  void periodic_cleanup(){
    thread t([this](){
      while (true) {
        this_thread::sleep_for(1h);
        lru.cleanup();
      }
    });
    t.detach();
  }
  int main(){
    periodic_cleanup();
    loop.pnode=this;

    // new_game(...) — где-то вызывается

    //loop.run(this);
    loop_v2.pnode=this;
    loop_v2.run();
    return 0;
  }
};

int main() {
  srand(time(0));
  if(bool prod=false){
    string mode="none";
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
