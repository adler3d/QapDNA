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
#include <queue>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <mutex>
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

template<class t_unix_socket>
void stream_write(t_unix_socket& client, const string& z, const string& data) {
  auto&strData=data;
  string lenStr = to_string(strData.length());

  client.write(lenStr.c_str(), lenStr.length());
  client.write("\0", 1);
  client.write(z.c_str(), z.length());
  client.write("\0", 1);
  client.write(strData.c_str(), strData.length());
}
string qap_zchan_write(const string&z,const string&data){
  string n=to_string(data.size());auto sep=string("\0",1);
  return n+sep+z+sep+data;
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
      if (len_str.size()>=7||!all_of(len_str.begin(), len_str.end(), ::isdigit)) break;// TODO: ban him
      int len = stoi(len_str);

      auto e2 = buffer.find('\0', e1 + 1);
      if (e2 == string::npos) break;
      if(e2>512)break;// TODO: ban him
      int total = e2 + 1 + len;
      if (buffer.size() < total) break;

      string z = buffer.substr(e1 + 1, e2 - e1 - 1);
      string payload = buffer.substr(e2 + 1, len);
      buffer.erase(0, total);

      cb(z, payload);
    }
  }
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

struct QueuedGame {
  t_game_decl decl;
  string submitted;
  string status = "queued"; // queued, scheduled, failed
};

struct GameQueue {
  queue<QueuedGame> games;
  mutex mtx;
  condition_variable cv;
  atomic<bool> stopping{false};

  // Просто добавляем игру
  void push(const t_game_decl& gd) {
    unique_lock<mutex> lock(mtx);
    games.push({gd, qap_time(), "queued"});
    cv.notify_one();
  }

  // Блокирующее извлечение первой игры (если есть)
  optional<QueuedGame> wait_and_pop() {
    unique_lock<mutex> lock(mtx);
    cv.wait(lock, [this] { return stopping || !games.empty(); });
    if (stopping || games.empty()) return nullopt;

    auto game = move(games.front());
    games.pop();
    return game;
  }

  // Неблокирующая проверка
  bool is_empty(){
    lock_guard<mutex> lock(mtx);
    return games.empty();
  }

  size_t size(){
    lock_guard<mutex> lock(mtx);
    return games.size();
  }
};

struct t_node_info {
  string name;
  int total_cores = 0;
  int used_cores = 0;
  bool online = false;
  string last_heartbeat = 0;
  int available() const { return online ? (total_cores - used_cores) : 0; }
  bool can_run(int players) { return available() >= players; }
};
struct i_sch_api{
  virtual bool assign_game(const t_game_decl&gd,const string&node){LOG("assign_game::no_way");return false;}
};
struct Scheduler {
  i_sch_api*api=nullptr;
  //t_net_api& capi;
  mutex mtx;
  //map<string, t_node_info> nodes;
  vector<t_node_info> narr;
  map<int, queue<t_game_decl>> c2rarr;
  void add_cores(const string& node, int n) {
    lock_guard<mutex> lock(mtx);
    for (auto& ex : narr) {
      if (ex.name == node) {
        ex.used_cores = max(0, ex.used_cores - n);
        return;
      }
    }
    LOG("add_cores call from unk node: "+node);
  }
  //not thread safe!
  t_node_info*node2i(const string&node,int cores_needed=-1){
    //lock_guard<mutex> lock(mtx);
    for(auto&ex:narr){
      if(cores_needed<0){if(node==ex.name)return &ex;continue;}
      if(!ex.online||(ex.total_cores-ex.used_cores)<cores_needed)continue;
      return &ex;
    }
    return nullptr;
  };
  void add_game_decl(const t_game_decl&gd){
    lock_guard<mutex> lock(mtx);
    c2rarr[gd.arr.size()].push(gd);
  }
  vector<t_game_decl> fails;
  void main() {
    cleanup_old_nodes();
    while (true) {
      this_thread::sleep_for(16ms);
      lock_guard<mutex> lock(mtx);
      for(auto&[cores_needed,q]:c2rarr){
        if(q.empty())continue;
        t_node_info*target=node2i({},cores_needed);
        if(!target)continue;
        auto game=q.front();q.pop();
        target->used_cores+=cores_needed;
        if(!api->assign_game(game,target->name))fails.push_back(game);
        LOG("Scheduled game on node: "+target->name+" ; game_id:"+to_string(game.game_id));
      }
    }
  }
  void on_game_finished(const string&node,int players){
    add_cores(node,players);
  }
  bool on_node_up(const string&payload,const string&node) {
    /*auto parts=split(payload,",");
    if(parts.size()!=3)return false;
    if(parts[0]!="node_up")return false;
    int cores=stoi(parts[1]);
    string token=parts[2];
    if (token!=UPLOAD_TOKEN) {
      LOG("Auth failed for node");
      return false;
    }*/
    int cores=stoi(payload);
    if (cores <= 0 || cores > 128){LOG("more than 128 cores??? cores=="+to_string(cores));cores = 8;}
    lock_guard<mutex> lock(mtx);
    if(auto*p=node2i(node)){
      LOG("node already under control: "+node);
      *p={node,cores,0,true,qap_time()};
      return true;
    }
    qap_add_back(narr)={node,cores,0,true,qap_time()};
    return true;
  }
  //void on_node_down(const string&node){
  //  lock_guard<mutex> lock(mtx);
  //  nodes[node].online=false;
  //}
  void on_ping(const string&node){// TODO: call this from t_main
    lock_guard<mutex> lock(mtx);
    auto*p=node2i(node);
    if(!p){LOG("ping from unk node: "+node);return;}
    p->last_heartbeat=qap_time();
  }
  void cleanup_old_nodes() {
    thread([this] {
      while (true) {
        this_thread::sleep_for(30s);
        auto now = qap_time();
        lock_guard<mutex> lock(mtx);
        for (auto it = narr.begin(); it != narr.end(); ) {
          if (qap_time_diff(it->last_heartbeat,now) > 60*1000) {
            LOG("Node timeout: "+it->name);
            it = narr.erase(it);
          } else {
            ++it;
          }
        }
      }
    }).detach();
  }
};

struct CompileJob {
  //string coder;
  //string source_code;
  string json;
  string src;
  string cdn_src_url;
  function<void(int)> on_uploaderror;
  function<void(t_post_resp&)> on_complete;
};

struct CompileQueue {
  queue<CompileJob> jobs;
  mutex mtx;
  condition_variable cv;
  atomic<bool> stopping{false};
  thread worker_thread;
  CompileQueue() {
    worker_thread = thread([this](){ this->worker_loop(); });
  }
  ~CompileQueue() {
    stopping = true;
    cv.notify_all();
    worker_thread.join();
  }

  void push_job(const CompileJob& job) {
    if(job.json.empty()||stopping)return;
    {
      unique_lock<mutex> lock(mtx);
      jobs.push(job);
    }
    cv.notify_one();
  }

private:
  void worker_loop() {
    while (!stopping) {
      CompileJob job;
      {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock,[this](){return stopping||!jobs.empty();});
        if(stopping)break;
        job=std::move(jobs.front());
        jobs.pop();
      }
      auto s=http_put_with_auth(job.cdn_src_url,job.src,UPLOAD_TOKEN);
      if(s!=200){job.on_uploaderror(s);continue;}
      auto resp=http_post_json_with_auth("compile",job.json,UPLOAD_TOKEN,COMPILER_URL);
      job.on_complete(resp);
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
    string status;
    bool ok(){return status.substr(0,3)=="ok:";}
  };
  string id;
  string name;
  string token;
  string email;
  string time;
  unique_ptr<mutex> sarr_mtx;
  vector<t_source> sarr;
  int total_games=0;
  double elo=1500;
  bool allowed_next_src_upload()const{
    lock_guard<mutex> lock(*sarr_mtx);
    if(sarr.empty())return true;
    if(sarr.back().prod_time.empty())return false;
    return qap_time_diff(sarr.back().time,qap_time())>60*1000;
  }
};
struct t_cmd{bool valid=true;};
struct t_world{
  void use(int player_id,const t_cmd&cmd){}
  void step(){};
  bool finished(){return false;}
  vector<double> slot2score;
};
struct t_game_uploaded_ack{
  int game_id;
  string err;
};
struct t_finished_game{
  int game_id;
  vector<double> slot2ms;
  vector<double> slot2score;
  vector<string> slot2status; // OK, TL, PF, stderr
  int tick=0;
  int size=0;
  //string reason; // "maxtick", "crash", "TL", "PF"
  //double duration_sec;
  //time_t finished_at;
  string cdn_file()const{return to_string(game_id);};
  string serialize()const{return {};}
};
struct t_cdn_game:t_finished_game{
  struct t_player{
    string coder;
    string v;
    string err;
    vector<double> tick2ms;
  };
  vector<t_player> slot2player;
  vector<vector<t_cmd>> tick2cmds;
  string serialize()const{return {};}
  int size(){return 0;}// TODO: need return serialize().size() but optimized!
};
struct t_game{
  t_game_decl gd;
  t_finished_game fg;
  string ordered_at;
  string finished_at;
  string status="new";
  string author;
};
struct t_main : t_process,t_http_base {
  vector<t_coder_rec> carr;mutex carr_mtx;
  vector<t_game> garr;mutex garr_mtx;
  //map<string, string> node2ipport;
  t_net_api capi;
  map<int, emitter_on_data_decoder> client_decoders;mutex cds_mtx;//mutex n2i_mtx;
  CompileQueue compq;
  void on_client_data(int client_id, const string& data, function<void(const string&)> send) {
    lock_guard<mutex> lock(cds_mtx);
    auto& decoder = client_decoders[client_id];
    if (!decoder.cb) {
      decoder.cb = [this, client_id, send](const string& z, const string& payload) {
        if(z=="game_finished:"+UPLOAD_TOKEN){
          auto result=parse<t_finished_game>(payload);
          lock_guard<mutex> lock(garr_mtx);
          auto gid=result.game_id;
          if(gid<0||gid>=garr.size()){LOG("wrong game_id form ???");return;}
          auto&g=garr[gid];
          g.fg=result;
          g.status="finished";
          g.finished_at=qap_time();
          sch.on_game_finished(node(client_id),g.gd.arr.size());
          //update_elo(result);
        }
        if(z=="game_uploaded:"+UPLOAD_TOKEN){
          auto result=parse<t_game_uploaded_ack>(payload);
          lock_guard<mutex> lock(garr_mtx);
          auto gid=result.game_id;
          if(gid<0||gid>=garr.size()){LOG("wrong game_id form ???");return;}
          auto&g=garr[gid];
          g.status="uploaded";
        }
        if(z=="node_up:"+UPLOAD_TOKEN){
          lock_guard<mutex> lock(cid2i_mtx);
          cid2i[client_id].our=true;
          sch.on_node_up(payload,node(client_id));
        }
        if(z=="ping:"+UPLOAD_TOKEN){
          sch.on_ping(node(client_id));
        }
      };
    }
    decoder.feed(data.data(), data.size());
  }
  void client_killer(){
    thread([&]{
      for(;;){
        this_thread::sleep_for(1s);
        auto now=qap_time();
        lock_guard<mutex> lock(cid2i_mtx);
        vector<int> darr;
        for(auto&ex:cid2i){
          if(ex.second.our)continue;
          auto dt=qap_time_diff(ex.second.time,now);
          if(dt<1000*3)continue;
          server.disconnect_client(ex.second.cid);
          darr.push_back(ex.first);
        }
        for(auto&ex:darr)cid2i.erase(ex);
      }
    }).detach();
  }
  static string node(int client_id){return /*"node"+*/to_string(client_id);}
  static int node2cid(const string&node){return stoi(node);}
  struct t_client_info{
    string ip;
    int cid;
    socket_t sock;
    string time;
    bool our=false;
  };
  map<int,t_client_info> cid2i;mutex cid2i_mtx;
  void on_client_connected(int client_id, socket_t sock, const string& ip) {
    cout << "[t_main] client connected: " << ip << " (id=" << client_id << ")\n";
    //lock_guard<mutex> lock(n2i_mtx);node2ipport[node(client_id)] = ip + ":31456";
    lock_guard<mutex> lock(cid2i_mtx);
    cid2i[client_id]={ip,client_id,sock,qap_time()};
  }
  void on_client_disconnected(int client_id) {
    cout << "[t_main] Node disconnected: " << client_id << "\n";
    {lock_guard<mutex> lock(cds_mtx);client_decoders.erase(client_id);}
    //{lock_guard<mutex> lock(n2i_mtx);node2ipport.erase(node(client_id));}
  }
  t_coder_rec*coder2rec(const string&coder){for(auto&ex:carr){if(ex.name!=coder)continue;return &ex;}return nullptr;}
  t_coder_rec*email2rec(const string&email){for(auto&ex:carr){if(ex.email!=email)continue;return &ex;}return nullptr;}
  // t_site->t_main
  CompileJob new_source_job(const string&coder,const string&src){
    CompileJob out;
    t_coder_rec*p=nullptr;
    {
      lock_guard<mutex> lock(carr_mtx);
      auto*p=coder2rec(coder);
      if(!p)return out;
    }
    int src_id=-1;
    {
      lock_guard<mutex> lock(*p->sarr_mtx);
      src_id=p->sarr.size();
    }
    string v=to_string(src_id);
    t_coder_rec::t_source b;
    b.time=qap_time();
    b.cdn_src_url="source/"+p->id+"_"+v+".cpp";
    b.cdn_bin_url="binary/"+p->id+"_"+v+".cpp";
    {lock_guard<mutex> lock(*p->sarr_mtx);p->sarr.push_back(b);}
    json body_json;
    body_json["coder_id"] = p->id;
    body_json["elf_version"] = v;
    body_json["source_code"] = src;
    body_json["timeout_ms"] = 20000;
    body_json["memory_limit_mb"] = 512;
    out.src=src;
    out.json=body_json.dump();
    out.on_uploaderror=[&](int s){
      if(s==200)return;
      lock_guard<mutex> lock(*p->sarr_mtx);
      auto&src=p->sarr[src_id];
      src.status="http_upload_err:"+to_string(s);
      src.prod_time=qap_time();
    };
    out.on_complete=[&](t_post_resp&resp){
      lock_guard<mutex> lock(*p->sarr_mtx);
      if(resp.status!=200){
        auto&src=p->sarr[src_id];
        src.status="compile_err:"+resp.body;
        src.prod_time=qap_time();
        return;
      }
      auto&src=p->sarr[src_id];
      src.status="ok:"+resp.body;
      src.prod_time=qap_time();
    };
    return std::move(out);
  }
  struct t_new_src_resp{
    string v,time;
    string err;
  };
  struct t_new_coder_resp{
    string err;
    string id;
    string time;
  };
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
  struct t_sch_api:i_sch_api{
    t_main*pmain=nullptr;
    bool assign_game(const t_game_decl&game,const string&node)override{
      auto payload=qap_zchan_write("new_game:"+UPLOAD_TOKEN,serialize(game));
      lock_guard<mutex> lock(pmain->cid2i_mtx);
      auto&cid=pmain->cid2i[node2cid(node)].cid;
      return pmain->server.send_to_client(cid,payload);
    }
  };
  t_server_api server{31456};
  Scheduler sch;t_sch_api sch_api;
  int main(int port=31456){
    sch.api=&sch_api;sch_api.pmain=this;
    thread([this]{sch.main();}).detach();
    client_killer();
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
  map<string,string> coder2lgt; mutex coder2lgt_mtx;
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
    }).detach();
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
        {
          lock_guard<mutex> lock(carr_mtx);
          auto*p=coder2rec(coder);
          if(!p){res.status=404;res.set_content("not found","text/plain");return;}
          if(p->token!=token){res.status=403;res.set_content("unauthorized","text/plain");return;}
          if(!p->allowed_next_src_upload()){res.status=429;res.set_content("rate limit exceeded","text/plain");return;}
        }
        compq.push_job(std::move(new_source_job(coder,src)));
        res.status = 200;
        res.set_content("ok","text/plain");
      }
      catch (const std::exception& e) {
        res.status = 500;
        res.set_content(std::string("Exception: ") + e.what(), "text/plain");
      }
    });
    srv.Post("/game/mk", [this](const httplib::Request& req, httplib::Response& res) {
      try {
        if(req.body.size()>1024*128){res.status=500;res.set_content("req.body too long","text/plain");return;}
        auto j = json::parse(req.body);
        string author = j.value("author", "");
        string token = j.value("token", "");
        string config = j.value("config", "");
        auto players_json = j.value("players", json::array());
        if (author.empty() || token.empty() || config.empty() || players_json.empty()) {
          res.status = 400;
          res.set_content("Missing required fields", "text/plain");
          return;
        }
        t_coder_rec* auth_rec = nullptr;
        {
          lock_guard<mutex> lock(carr_mtx);
          auth_rec = coder2rec(author);
          if(!auth_rec){res.status=404;res.set_content("author not found","text/plain");return;}
          if(auth_rec->token!=token){res.status=403;res.set_content("invalid token","text/plain");return;}
        }
        map<string,string>&last_game_time=coder2lgt;
        {
          auto t=qap_time();
          lock_guard<mutex> lock(coder2lgt_mtx);
          if (last_game_time.count(author) && (qap_time_diff(last_game_time[author],t)) < 5 * 60*1000) {
            res.status = 429;
            res.set_content("Rate limit: one game per 5 minutes", "text/plain");
            return;
          }
          last_game_time[author]=t;
        }
        vector<t_game_slot> slots;
        {
          string err;
          for (auto& p : players_json) {
            string coder = p.value("coder", "");
            string version = p.value("version", "latest");
            lock_guard<mutex> lock(carr_mtx);
            if(carr.empty()){err="under_construction";break;}
            t_coder_rec*rec=coder.empty()?&carr[rand()%carr.size()]:coder2rec(coder);
            if(!rec){err="not found coder with name: "+coder;break;}
            lock_guard<mutex> lock2(*rec->sarr_mtx);
            auto&sarr=rec->sarr;
            int ver=(version=="latest"||version.empty())?(int)sarr.size()-1:stoi(version);
            auto fail=[&](int v){return v<0||v>=(int)sarr.size();};
            while(!fail(ver)&&!sarr[ver].ok())ver--;
            if(fail(ver)){err="wrong version for coder: "+coder;break;}
            t_game_slot slot;
            slot.coder=coder;
            slot.cdn_bin_file=rec->sarr[ver].cdn_bin_url;
            slots.push_back(slot);
          }
          if(!err.empty()){
            res.status=404;res.set_content(err,"text/plain");return;
          }
        }
        if (slots.size() < 2) {
          res.status = 400;
          res.set_content("At least 2 players required", "text/plain");
          return;
        }
        t_game_decl gd;
        {
          lock_guard<mutex> lock(garr_mtx);
          gd.arr=slots;
          gd.config=config;
          gd.game_id=garr.size();
          t_game&g=qap_add_back(garr);
          g.fg.game_id=-1;
          g.gd=gd;
          g.status="new";
          g.author="coder:"+author;
          g.ordered_at=qap_time();
        }
        sch.add_game_decl(gd);
        json resp = {{"game_id", gd.game_id}, {"status", "scheduled"},{"time",qap_time()}};
        res.status = 200;
        res.set_content(resp.dump(), "application/json");
      } catch (const exception& e) {
        res.status = 500;
        res.set_content(string("Exception: ") + e.what(), "text/plain");
      }
    });
    srv.Get("/top", [this](const httplib::Request& req, httplib::Response& res) {
      try {
        struct t_id_with_elo{
          int i=0;
          double elo=0;
        };
        vector<t_id_with_elo> top;
        {
          lock_guard<mutex> lock(carr_mtx);
          int i=-1;
          for(auto&c:carr){
            i++;
            if(c.sarr.empty())continue;
            top.push_back({i,c.elo});
          }
        }
        sort(top.begin(),top.end(),[](const t_id_with_elo&a,const t_id_with_elo&b) {
          return a.elo>b.elo;
        });
        vector<json> top_list;
        for(auto&ex:top){
          auto&c=carr[ex.i];
          top_list.push_back({
            {"id",c.id},
            {"name",c.name},
            {"rating",c.elo},
            {"games",c.total_games}
          });
        }
        res.status=200;
        res.set_content(json(top_list).dump(2), "application/json");
      } catch (const exception& e) {
        res.status = 500;
        res.set_content(e.what(), "text/plain");
      }
    });
    auto game2json=[](const t_game&g,json&j){
      j = {
        {"game_id", g.gd.game_id},
        {"tick", g.fg.tick},
        {"size", g.fg.size},
        {"ot", g.ordered_at},
        {"ft", g.finished_at},
        {"author", g.author},
        {"players", json::array()}
      };
      int z=-1;auto&jp=j["players"];
      for (const auto& p : g.gd.arr) {
        z++;
        auto s=g.fg.slot2score.size()?g.fg.slot2score[z]:0.0;
        auto ms=g.fg.slot2ms.size()?g.fg.slot2ms[z]:0.0;
        jp.push_back({{"coder",p.coder},{"score",s},{"ms",ms}});
      }
    };
    srv.Get("/games/latest", [this,game2json](const httplib::Request& req, httplib::Response& res) {
      try {
        int n = req.has_param("n") ? stoi(req.get_param_value("n")) : 10;
        n = max(1, min(n, 100));

        vector<json> recent;
        {
          lock_guard<mutex> lock(garr_mtx);
          int start = max(0, (int)garr.size() - n);
          for (int i = start; i < (int)garr.size(); ++i) {
            const auto& g = garr[i];
            json j;
            game2json(g,j);
            recent.push_back(std::move(j));
          }
        }
        res.status=200;
        res.set_content(json(recent).dump(2), "application/json");
      } catch (const exception& e) {
        res.status = 500;
        res.set_content(e.what(), "text/plain");
      }
    });
    srv.Get(R"(/coder/(\w+)/games)", [this,game2json](const httplib::Request& req, httplib::Response& res) {
      string coder = req.matches[1];
      vector<json> games;
      {
        lock_guard<mutex> lock(garr_mtx);
        for (const auto& g : garr) {
          bool found=false;
          for (const auto& p : g.gd.arr) {
            if (p.coder!=coder)continue;
            found=true;
          }
          if(!found)continue;
          json j;game2json(g,j);
          games.push_back(std::move(j));
        }
      }
      res.status=200;
      res.set_content(json(games).dump(2), "application/json");
    });
    srv.Get("/me", [this](const httplib::Request& req, httplib::Response& res) {
      try {
        string token = req.get_param_value("token");
        if (token.empty()) {
          res.status = 400;
          res.set_content("Missing token", "text/plain");
          return;
        }

        t_coder_rec* rec = nullptr;
        {
          lock_guard<mutex> lock(carr_mtx);
          for (auto& c : carr) {
            if (c.token == token) {
              rec = &c;
              break;
            }
          }
        }

        if (!rec) {
          res.status = 404;
          res.set_content("Invalid token", "text/plain");
          return;
        }

        json resp = {
          {"id", rec->id},
          {"name", rec->name},
          {"email", rec->email},
          {"rating", rec->elo},
          {"total_games", rec->total_games},
          {"last_upload", rec->sarr.empty() ? "" : rec->sarr.back().prod_time}
        };

        res.status = 200;
        res.set_content(resp.dump(2), "application/json");
      }
      catch (const exception& e) {
        res.status = 500;
        res.set_content(string("Exception: ") + e.what(), "text/plain");
      }
    });
    srv.Get(R"(/coder/(\w+))", [this](const httplib::Request& req, httplib::Response& res) {
      string name = req.matches[1];
      t_coder_rec* rec = nullptr;

      {
        lock_guard<mutex> lock(carr_mtx);
        rec = coder2rec(name);
      }

      if (!rec) {
        res.status = 404;
        res.set_content("Coder not found", "text/plain");
        return;
      }

      json resp = {
        {"id", rec->id},
        {"name", rec->name},
        {"rating", rec->elo},
        {"total_games", rec->total_games},
        {"src_count", (int)rec->sarr.size()}
      };

      res.status = 200;
      res.set_content(resp.dump(2), "application/json");
    });
    srv.Get(R"(/game/(\d+))", [this,game2json](const httplib::Request& req, httplib::Response& res) {
      try {
        int game_id = stoi(req.matches[1]);
        lock_guard<mutex> lock(garr_mtx);
        t_game* game = nullptr;
        if (game_id < 0 || game_id >= (int)garr.size()) {
          res.status = 404;
          res.set_content("Game not found", "text/plain");
          return;
        }
        game = &garr[game_id];
        json j;
        game2json(*game, j);
        res.status = 200;
        res.set_content(j.dump(2), "application/json");
      }
      catch (const exception& e) {
        res.status = 500;
        res.set_content(string("Exception: ") + e.what(), "text/plain");
      }
    });
    srv.Get("/status", [this](const httplib::Request& req, httplib::Response& res) {
      json status;
      {
        lock_guard<mutex> lock(carr_mtx);
        status["coders"] = carr.size();
      }
      {
        lock_guard<mutex> lock(garr_mtx);
        status["games_total"] = garr.size();
        int running = 0, finished = 0;
        for (const auto& g : garr) {
          if (g.status == "finished") finished++;
          else if (g.status != "new") running++;
        }
        status["games_running"] = running;
        status["games_finished"] = finished;
      }
      {
        lock_guard<mutex> lock(sch.mtx); // Scheduler's mtx
        status["nodes_online"] = sch.narr.size();
      }
      status["time"] = qap_time();

      res.status = 200;
      res.set_content(status.dump(2), "application/json");
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
