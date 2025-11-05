#include <string>
#include <algorithm>
void LOG(const std::string&str);
#include "netapi.h"

#include <thread>
#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <array>
using namespace std;

#include "../../QapLR/QapLR/qap_assert.inl"
#include "../../QapLR/QapLR/qap_vec.inl"
#include "../../QapLR/QapLR/qap_io.inl"
#include "../../QapLR/QapLR/qap_time.inl"

#include "FromQapEng.h"
#include "thirdparty/picosha2.h"
#include "thirdparty/httplib.h"//13
#undef ADD
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
#include "html_stuff.h"
#include "t_cdn.hpp"

uint64_t sim_time_ms = 0; // симулированное время в миллисекундах
double sim_speed = 10000.0; // 1 реальная секунда = 1000 симулированных секунд
std::mutex sim_mtx;

const string DOCKERFILE_PATH = "./docker/universal-runner/Dockerfile"; //TODO: check this file must exists!
const string IMAGE_NAME = "universal-runner:latest";
const string ARCHIVE_NAME = "/tmp/universal-runner.tar";
// DON'T USE LOCALHOST WITH cpp-httplib !!! THIS DON'T WORK!
const string CDN_HOSTPORT = "127.0.0.1:"+to_string(t_cdn::CDN_PORT);
const string CDN_URL="http://"+CDN_HOSTPORT;
const string CDN_URL_IMAGES=CDN_URL+"/images/"; //TODO: replace to t_main "ip:port/images/"
const string COMPILER_URL="http://127.0.0.1:3000";
const int MAIN_PORT=31457;
void LOG(const string&str){cerr<<"["<<qap_time()<<"] "<<(str)<<endl;}
bool isValidName(const std::string& name) {
  for (char c : name) {
    if (!isalnum(c)&&c!='_'&&c!='.') {
      return false;
    }
  }
  return true;
}

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
string local_main_ip_port="127.0.0.1:"+to_string(MAIN_PORT);
//vector<string> split(...){return {};}
//#define QapAssert(...)
string generate_token(string coder_name,string time) {
  return "token:"+to_string((rand()<<16)+rand())+coder_name+time;
  //return sha256( random_bytes(32) + time + coder_name );
}
template<class TYPE>TYPE parse(const string&s){
  TYPE out;
  QapLoadFromStr(out,s);
  return out;
}
template<class TYPE>
string serialize(const TYPE&ref){auto out=ref;return QapSaveToStr(out);}

/*template<class t_unix_socket>
void stream_write(t_unix_socket& client, const string& z, const string& data) {
  auto&strData=data;
  string lenStr = to_string(strData.length());

  client.write(lenStr.c_str(), lenStr.length());
  client.write("\0", 1);
  client.write(z.c_str(), z.length());
  client.write("\0", 1);
  client.write(strData.c_str(), strData.length());
}*/

#include "aicup_structs.inl"

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

    auto game = std::move(games.front());
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
    LOG("Scheduler:: add_cores call from unk node: "+node);
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
        LOG("Scheduler:: Scheduled game on node: "+target->name+" ; game_id:"+to_string(game.game_id));
      }
    }
  }
  void on_game_finished(const string&node,int players){
    add_cores(node,players);
  }
  void on_game_aborted(const string&node,int players){
    add_cores(node,players);
  }
  bool on_node_up(const string&payload,const string&node) {
    int cores=stoi(payload);
    if (cores <= 0 || cores > 128){LOG("Scheduler:: more than 128 cores??? cores=="+to_string(cores));cores = 8;}
    lock_guard<mutex> lock(mtx);
    if(auto*p=node2i(node)){
      LOG("Scheduler:: node already under control: "+node);
      *p={node,cores,0,true,qap_time()};//TODO: we think all cores is unused but this is not always true
      return true;
    }
    narr.push_back({node,cores,0,true,qap_time()});
    //qap_add_back(narr)={node,cores,0,true,qap_time()};
    return true;
  }
  //void on_node_down(const string&node){
  //  lock_guard<mutex> lock(mtx);
  //  nodes[node].online=false;
  //}
  void on_ping(const string&node){
    lock_guard<mutex> lock(mtx);
    auto*p=node2i(node);
    if(!p){LOG("Scheduler:: ping from unk node: "+node);return;}
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
            LOG("Scheduler:: Node timeout: "+it->name);
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
  atomic<size_t> jobs_all=0;
  atomic<size_t> jobs_upe=0;
  atomic<size_t> jobs_done=0;
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
    jobs_all++;
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
      if(s!=200){jobs_upe++;job.on_uploaderror(s);continue;}
      auto resp=http_post_json_with_auth("compile",job.json,UPLOAD_TOKEN,COMPILER_URL);
      jobs_done++;
      job.on_complete(resp);
    }
  }
};

//struct t_cmd{bool valid=true;};
struct t_world{
  //void use(int player_id,const t_cmd&cmd){}
  //void step(){};
  //bool finished(){return false;}
  vector<double> slot2score;
};
struct t_vote{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_vote
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(uint64_t,from_uid,{})\
  ADD(int64_t,delta,0)\
  ADD(string,time,{})\
  ADD(string,reason,{})\
  //===
  #include "defprovar.inl"
  //===
  static t_vote mk(uint64_t f,int64_t d,const string&t,const string&r){
    t_vote out;
    out.from_uid=f;
    out.delta=d;
    out.time=t;
    out.reason=r;
    return out;
  }
};
struct t_votes{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_votes
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(vector<t_vote>,arr,{})\
  ADD(int64_t,up,0)\
  ADD(int64_t,down,0)\
  //===
  #include "defprovar.inl"
  //===
  void add(const t_vote&v){up+=v.delta>0?v.delta:0;down+=v.delta<0?-v.delta:0;arr.push_back(v);}
  int64_t tot()const{return up-down;}
};

struct t_ban{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_ban
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(uint64_t,from_uid,{})\
  ADD(string,time,{})\
  ADD(string,until,{})\
  ADD(string,reason,{})\
  //===
  #include "defprovar.inl"
  //===
};
struct t_coder_rec{
  struct t_source{
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_source
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(string,cdn_src_url,{})\
    ADD(string,cdn_bin_url,{})\
    ADD(string,time,{})\
    ADD(string,prod_time,{})\
    ADD(string,status,{})\
    ADD(uint64_t,size,0)\
    //===
    #include "defprovar.inl"
    //===
    bool ok()const{return status.find("ok:{\"success\":true,\"")==0;}
  };
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_coder_rec
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(uint64_t,id,0)\
  ADD(uint64,lvl,999999)\
  ADD(string,last_ip,{})\
  ADD(string,sysname,{})\
  ADD(string,visname,{})\
  ADD(string,token,{})\
  ADD(string,email,{})\
  ADD(string,time,{})\
  ADD(t_votes,karma,{})\
  ADD(vector<t_ban>,bans,{})\
  //===
  #include "defprovar.inl"
  //===
  bool is_banned(const chrono::system_clock::time_point&now)const{
    if(bans.empty())return false;
    auto&b=bans.back();
    auto tp=parse_qap_time(b.until);
    auto diff=chrono::duration_cast<chrono::milliseconds>(now-tp).count();
    return diff<0;
  }
};
typedef map<uint64_t,uint64_t> t_phase2rank;
typedef map<uint64_t,uint64_t> t_phase2rank;
struct t_season_coder{
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_season_coder
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(uint64_t,uid,{})\
  ADD(string,sysname,{})\
  ADD(vector<t_coder_rec::t_source>,sarr,{})\
  ADD(string,last_submit_time,{})\
  ADD(bool,hide,false)\
  ADD(bool,is_meta,false)\
  ADD(string,meta_name,{})\
  ADD(t_phase2rank,phase2rank,{})\
  //===
  #include "defprovar.inl"
  //===
  int try_get_last_valid_ver(int v)const{
    auto fail=[&](int v){return v<0||v>=(int)sarr.size();};
    if(fail(v))return -1;
    while(!fail(v)&&!sarr[v].ok())v--;
    return v;
  }
  bool allowed_next_src_upload()const{
    //lock_guard<mutex> lock(*sarr_mtx);
    if(sarr.empty())return true;
    if(sarr.back().prod_time.empty())return false;
    return qap_time_diff(sarr.back().time,qap_time())>60*1000;
  }
  //t_season_coder&set(uint64_t id){
  //  user_id=id;sysname=LowerStr(u);reg_time=qap_time();
  //  sarr_mtx=make_unique<mutex>();
  //  return *this;
  //}
};
typedef map<uint64_t,double> t_uid2score;
typedef map<uint64_t,uint64_t> t_uid2games;
typedef map<uint64_t,string> t_uid2source_phase;
struct t_phase {
  struct t_qr_decl {
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_qr_decl
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(string,from_phase,{})\
    ADD(uint64_t,top_n,50)\
    //===
    #include "defprovar.inl"
    //===
  };
  struct t_qualification_rule {
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_qualification_rule
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(uint64_t,from_phase,{})\
    ADD(uint64_t,top_n,50)\
    //===
    #include "defprovar.inl"
    //===
  };
  struct t_uid_rec{
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_uid_rec
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(double,score,1500)\
    ADD(uint64_t,games,0)\
    ADD(uint64_t,source_phase,0)\
    //===
    #include "defprovar.inl"
    //===
  };
  typedef map<uint64_t,t_uid_rec> t_uid2rec;
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_phase
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(uint64_t,phase,0)\
  ADD(string,type,{})\
  ADD(string,phase_name,{})\
  ADD(string,world,{})\
  ADD(string,game_config,{})\
  ADD(uint64_t,num_players,2)\
  ADD(uint64_t,ticksPerGame,7500)\
  ADD(uint64_t,msPerTick,35)\
  ADD(uint64_t,stderrKB,16)\
  ADD(vector<t_qualification_rule>,qualifying_from,{})\
  ADD(t_uid2rec,uid2rec,{})\
  ADD(vector<uint64_t>,games,{})\
  ADD(uint64_t,finished_games,0)\
  ADD(bool,on_prev_wave_end,true)\
  ADD(string,scheduled_start_time,"2025.11.02,16:36:37.447")\
  ADD(string,scheduled_end_time,"2025.11.03,09:46:51.632")\
  ADD(bool,is_active,false)\
  ADD(bool,is_closed,false)\
  ADD(bool,is_completed,false)\
  ADD(vector<vector<uint64_t>>,wave2gid,{})\
  ADD(string,last_wave_time,"2025.10.31,19:08:07.120")\
  ADD(uint64_t,sandbox_wave_interval_ms,5*60*1000)\
  ADD(double,games_per_coder_per_hour,1.0)\
  //===
  #include "defprovar.inl"
  //===
  bool prev_wave_done()const{return games.size()==finished_games;}
};
typedef map<uint64_t,t_season_coder> t_uid2scoder;
struct t_season {
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_season
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(uint64_t,season,0)\
  ADD(string,season_name,{})\
  ADD(string,title,{})\
  ADD(t_uid2scoder,uid2scoder,{})\
  ADD(vector<t_phase>,phases,{})\
  ADD(uint64_t,cur_phase,0)\
  ADD(bool,is_finalized,false)\
  //===
  #include "defprovar.inl"
  //===
  void sync(){
    for(int i=1;i<phases.size();i++){
      auto&prev=phases[i-1];
      auto&cur=phases[i-0];
      prev.scheduled_end_time=cur.scheduled_start_time;
    }
  }
};
struct t_comment {
#define DEF_PRO_COPYABLE()
#define DEF_PRO_CLASSNAME()t_comment
#define DEF_PRO_VARIABLE(ADD)\
ADD(uint64_t,id,0)\
ADD(uint64_t,parent_id,0)\
ADD(uint64_t,game_id,0)\
ADD(uint64_t,author_uid,0)\
ADD(string,url,{})\
ADD(string,title,{})\
ADD(string,content,{})\
ADD(string,created_at,{})\
ADD(t_votes,votes,{})\
ADD(vector<uint64_t>,comments,{})\
ADD(t_votes,hidden,{})\
ADD(t_votes,is_pinned,{})\
//===
#include "defprovar.inl"
//===
};
#include "t_config_structs.h"
struct t_main : t_http_base {
  #include "waveman.h"
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()t_main
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(vector<t_coder_rec>,carr,{})\
  ADD(vector<t_game>,garr,{})\
  ADD(vector<t_comment>,comments,{})\
  ADD(vector<t_season>,seasons,{})\
  //===
  #include "defprovar.inl"
  //===
  WaveManager waveman;
  mutex mtx;
  //map<string, string> node2ipport;
  //t_net_api capi;
  map<int, emitter_on_data_decoder> client_decoders;mutex cds_mtx;//mutex n2i_mtx;
  CompileQueue compq;
  void on_client_data(int client_id, const string& data, function<void(const string&)> send) {
    lock_guard<mutex> lock(cds_mtx);
    auto& decoder = client_decoders[client_id];
    if (!decoder.cb) {
      decoder.cb = [this, client_id, send](const string& z, const string& payload) {
        if(z=="game_finished:"+UPLOAD_TOKEN){
          auto result=parse<t_finished_game>(payload);
          lock_guard<mutex> lock(mtx);
          auto gid=result.game_id;
          if(gid<0||gid>=garr.size()){LOG("t_main::wrong game_id form "+to_string(client_id));return;}
          auto&g=garr[gid];
          g.fg=result;
          g.status="finished";
          g.finished_at=qap_time();
          if(qap_check_id(g.gd.season,seasons)){
            auto&s=seasons[g.gd.season];
            if(qap_check_id(s.phases,g.gd.phase)){
              auto&p=s.phases[g.gd.phase];
              p.finished_games++;
              p.on_prev_wave_end=p.prev_wave_done();
            }
          }
          sch.on_game_finished(node(client_id),g.gd.arr.size());
          LOG("game finished "+to_string(gid));
          //update_elo(result);
        }
        if(z=="game_uploaded:"+UPLOAD_TOKEN){
          auto result=parse<t_game_uploaded_ack>(payload);
          lock_guard<mutex> lock(mtx);
          auto gid=result.game_id;
          if(gid<0||gid>=garr.size()){LOG("t_main::wrong game_id form "+to_string(client_id));return;}
          auto&g=garr[gid];
          g.status="uploaded";
          LOG("game uploaded "+to_string(gid));
        }
        if(z=="game_aborted:"+UPLOAD_TOKEN){
          auto a=split(payload,",");
          if(a.size()!=2)return;
          auto game_id=stoull(a[0]);
          lock_guard<mutex> lock(mtx);
          auto gid=game_id;
          if(gid<0||gid>=garr.size()){LOG("t_main::wrong game_id form "+to_string(client_id));return;}
          auto&g=garr[gid];
          g.status="aborted by "+a[1];
          g.finished_at=qap_time();
          sch.on_game_aborted(node(client_id),g.gd.arr.size());
          LOG("game aborted "+to_string(gid));
        }
        if(z=="node_up:"+UPLOAD_TOKEN){
          auto a=split(payload,",");
          if(a.size()!=2)return;
          {
            lock_guard<mutex> lock(cid2i_mtx);
            for(auto&ex:cid2i){
              if(ex.second.ut!=a[1])continue;
              auto&n=cid2i[client_id];
              n=ex.second;n.cid=client_id;n.our=true;
              sch.on_node_up(a[0],n.ut);
              ex.second.deaded=true;
              return;
            }
            auto&r=cid2i[client_id];
            QapAssert(a[1].size());
            QapAssert(!r.ut.size());
            r.ut=a[1];
            r.our=true;
          }
          sch.on_node_up(a[0],node(client_id));
        }
        if(z=="ping:"+UPLOAD_TOKEN){
          auto n=node(client_id,true);
          if(n.size())sch.on_ping(n);
          server.send_to_client(client_id,qap_zchan_write("pong",":)"));
        }
      };
    }
    auto r=decoder.feed(data.data(),data.size());
    if(!r.ok())server.disconnect_client(client_id);
  }
  void client_killer(){
    thread([&]{
      for(;;){
        this_thread::sleep_for(1s);
        auto now=qap_time();
        lock_guard<mutex> lock(cid2i_mtx);
        vector<int> darr;
        for(auto&ex:cid2i){
          if(ex.second.deaded){darr.push_back(ex.second.cid);continue;}
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
  string node(int client_id,bool can_be_empty=false){
    lock_guard<mutex> lock(cid2i_mtx);
    auto ut=cid2i[client_id].ut;if(!can_be_empty)QapAssert(ut.size());return ut;
  }
  int node2cid(const string&node){
    lock_guard<mutex> lock(cid2i_mtx);
    for(auto&ex:cid2i){
      if(ex.second.ut!=node)continue;
      return ex.second.cid;
    }
    QapNoWay();
    return -1;
  }
  struct t_client_info{
    string ip;
    int cid;
    //socket_t sock;
    string time;
    string ut;
    bool our=false;
    bool deaded=false;
  };
  map<int,t_client_info> cid2i;mutex cid2i_mtx;
  void on_client_connected(int client_id, socket_t sock, const string& ip) {
    cout << "[t_main] client connected: " << ip << " (id=" << client_id << ")\n";
    //lock_guard<mutex> lock(n2i_mtx);node2ipport[node(client_id)] = ip + ":31456";
    lock_guard<mutex> lock(cid2i_mtx);
    cid2i[client_id]={ip,client_id,/*sock,*/qap_time()};
    string ut=sha256(qap_time()+"2025.08.25 18:44:20.685"+to_string((rand()<<16)+rand()));
    server.send_to_client(client_id,qap_zchan_write("hi",ut));
  }
  void on_client_disconnected(int client_id) {
    cout << "[t_main] Node disconnected: " << client_id << "\n";
    {lock_guard<mutex> lock(cds_mtx);client_decoders.erase(client_id);}
    //{lock_guard<mutex> lock(n2i_mtx);node2ipport.erase(node(client_id));}
  }
  t_coder_rec*coder2rec(const string&coder){for(auto&ex:carr){if(ex.sysname!=coder)continue;return &ex;}return nullptr;}
  t_coder_rec*email2rec(const string&email){for(auto&ex:carr){if(ex.email!=email)continue;return &ex;}return nullptr;}
  // t_site->t_main
  CompileJob new_source_job(uint64_t uid,const string&src){
    CompileJob out;
    if(!qap_check_id(carr,uid)||seasons.empty())return out;
    t_coder_rec&c=carr[uid];
    auto&s=seasons.back();auto sid=(uint64_t)seasons.size()-1;
    auto it=s.uid2scoder.find(uid);
    if(it==s.uid2scoder.end())return out;
    auto&sc=it->second;
    auto src_id=sc.sarr.size();
    string v=to_string(src_id);
    t_coder_rec::t_source b;
    b.time=qap_time();
    b.cdn_src_url="source/"+to_string(s.season)+"_"+to_string(uid)+"_"+v+".cpp";
    b.cdn_bin_url="binary/"+to_string(s.season)+"_"+to_string(uid)+"_"+v+".elf";
    b.size=src.size();
    sc.sarr.push_back(b);
    json body_json;
    body_json["coder_id"] = uid;
    body_json["elf_version"] = v;
    body_json["source_code"] = src;
    body_json["timeout_ms"] = 20000;
    body_json["memory_limit_mb"] = 512;
    body_json["season"] = to_string(s.season);
    out.cdn_src_url=b.cdn_src_url;
    out.src=src;
    out.json=body_json.dump();
    out.on_uploaderror=[this,uid,sid,src_id](int s){
      if(s==200)return;
      lock_guard<mutex> lock(mtx);
      auto&src=seasons[sid].uid2scoder[uid].sarr[src_id];
      src.status="http_upload_err:"+to_string(s);
      src.prod_time=qap_time();
    };
    out.on_complete=[this,uid,sid,src_id](t_post_resp&resp){
      lock_guard<mutex> lock(mtx);
      auto&src=seasons[sid].uid2scoder[uid].sarr[src_id];
      if(resp.status!=200){
        src.status="compile_err:"+resp.body;
        src.prod_time=qap_time();
        return;
      }
      src.status="ok:"+resp.body;
      src.prod_time=qap_time();
    };
    return std::move(out);
  }
  struct t_sch_api:i_sch_api{
    t_main*pmain=nullptr;
    bool assign_game(const t_game_decl&game,const string&node)override{
      auto payload=qap_zchan_write("new_game:"+UPLOAD_TOKEN,serialize(game));
      auto cid=pmain->node2cid(node);
      //lock_guard<mutex> lock(pmain->cid2i_mtx);
      //auto&cid=pmain->cid2i[cid].cid;
      return pmain->server.send_to_client(cid,payload);
    }
  };
  t_server_api server{MAIN_PORT};
  Scheduler sch;t_sch_api sch_api;
  int main(int port=MAIN_PORT){
    sch.api=&sch_api;sch_api.pmain=this;
    thread([this]{sch.main();}).detach();
    thread update_seasons_thread([this]() {
      seasons_main_loop();
    });
    update_seasons_thread.detach();
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
    //cout << "Press Enter to stop...\n"; string dummy;getline(cin, dummy);
    for(;;){std::this_thread::sleep_for(1000ms);}
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
      srv.set_payload_max_length(1024*1024);
      setup_routes();
      if(!srv.listen("0.0.0.0",80)){
        cerr << "[t_site] Failed to start server\n";
        return -1;
      }
      return 0;
    }).detach();
  }
  RateLimiter rate_limiter;
  static string get_token_from_request(const httplib::Request& req, httplib::Response& res){
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
      res.status = 401;
      res.set_content("Authorization header is missing", "text/plain");
      return {};
    }
    const string prefix = "Bearer ";
    if (auth_header.find(prefix) != 0) {
      res.status = 400;
      res.set_content("Authorization header must be of the form 'Bearer <token>'", "text/plain");
      return {};
    }
    string token = auth_header.substr(prefix.size());
    if (token.empty()) {
      res.status = 400;
      res.set_content("Token is missing in Authorization header", "text/plain");
      return {};
    }
    return token;
  };
  void setup_routes(){
    #define RATE_LIMITER(LIMIT)\
      if (!rate_limiter.is_allowed(req.remote_addr,req.matched_route,LIMIT)) {\
        res.set_content("Rate limit exceeded", "text/plain");\
        res.status = 429;\
        return;\
      }\
    //RATE_LIMITER
    srv.Post("/coder/new",[this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(25);
      try {
        if(req.body.size()>=1024*16){res.status=409;res.set_content("max request size is 16KB","text/plain");return;}
        auto j = json::parse(req.body);

        string name = j.value("name", "");
        string email = j.value("email", "");
        string sysname = LowerStr(name);
        string sysemail = LowerStr(email);

        if (name.empty() || email.empty()) {
          res.status = 400;
          res.set_content("Missing required fields: name or email", "text/plain");
          return;
        }
        if(name.size()<3){res.status=409;res.set_content("min name size is 3","text/plain");return;}
        if(name.size()>42){res.status=409;res.set_content("max name size is 42","text/plain");return;}
        if(email.size()>42){res.status=409;res.set_content("max email size is 42","text/plain");return;}
        if(email.find('@')==string::npos){res.status=409;res.set_content("email must contain @","text/plain");return;}
        if(!isValidName(name)){res.status=409;res.set_content("some char in name is not allowed! allowed=gen_dips(\"azAZ09\")+\"_.\"","text/plain");return;}
        {
          lock_guard<mutex> lock(mtx);
          int users_on_ip=0;
          for (const auto& c : carr) {
            if (c.sysname == sysname) {
              res.status = 409;
              res.set_content("Duplicate coder name", "text/plain");
              return;
            }
            if (c.email == sysemail) {
              res.status = 409;
              res.set_content("Duplicate coder email", "text/plain");
              return;
            }
            if (c.last_ip == req.remote_addr)users_on_ip++;
          }
          if(users_on_ip>2){
            res.status = 409;
            res.set_content("No way - ban by IP unordered", "text/plain");
            return;
          }
          t_coder_rec b;
          b.last_ip=req.remote_addr;
          b.id = carr.size();
          b.time = qap_time();
          b.visname = name;
          b.sysname = sysname;
          b.email = sysemail;
          b.token = sha256(b.time + name + email + to_string((rand() << 16) + rand()) + "2025.08.23 15:10:42.466");
          b.lvl=!b.id?0:(b.id<=10?999999-1000+b.id*16:999999);
          if(b.id<=10)b.karma.add(t_vote::mk(0,+2,qap_time(),"init_v1"));
          if(b.id<=64)b.karma.add(t_vote::mk(0,+1,qap_time(),"init_v2"));
          carr.push_back(std::move(b));

          uint64_t uid = carr.back().id;
          if (!seasons.empty() && !seasons.back().is_finalized) {
            t_season& s = seasons.back();
            if (s.uid2scoder.count(uid) == 0) {
              auto&sc=s.uid2scoder[uid];
              sc.uid=uid;
              sc.sysname=sysname;
              if (!s.phases.empty()) {
                t_phase& active = s.phases[s.cur_phase];
                if (active.type == "sandbox") {
                  active.uid2rec[uid]={};
                }
              }
            }
          }

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
    srv.Post("/api/seasons/current/submit", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(10);
      string token=get_token_from_request(req,res);
      if(token.empty())return;
      try {
        if (req.body.size() >= 1024 * 512) {
          res.status = 409;
          res.set_content("max request size is 512KB", "text/plain");
          return;
        }
        auto j = json::parse(req.body);
        string src = j.value("src", "");
        if (src.empty()) {
          res.status = 400;
          res.set_content("Missing 'src'", "text/plain");
          return;
        }
        if (src.size() > 1024 * 256) {
          res.status = 409;
          res.set_content("max source size is 256KB", "text/plain");
          return;
        }

        uint64_t uid = UINT64_MAX;
        {
          lock_guard<mutex> lock(mtx);
          for (size_t i = 0; i < carr.size(); ++i) {
            if (carr[i].token == token) {
              uid = i;
              break;
            }
          }
        }
        if (uid == UINT64_MAX) {
          res.status = 403;
          res.set_content("Invalid token", "text/plain");
          return;
        }

        lock_guard<mutex> lock(mtx);
        if (seasons.empty()) {
          res.status = 400;
          res.set_content("No active season", "text/plain");
          return;
        }
        t_season& season = seasons.back();auto season_id=to_string((int64_t)seasons.size()-1);
        if (season.is_finalized) {
          res.status = 400;
          res.set_content("Season is finalized", "text/plain");
          return;
        }
        auto it = season.uid2scoder.find(uid);
        auto curp=season.cur_phase;
        if (it == season.uid2scoder.end()) {
          bool can_auto_join = false;
          if (!season.phases.empty()) {
            for(;curp<season.phases.size();curp++){
              t_phase& cur = season.phases[curp];
              if (cur.type == "sandbox" && !cur.is_closed) {
                can_auto_join = true;break;
              }
            }
          }
          if (!can_auto_join) {
            res.status = 403;
            res.set_content("You are not a participant of this season", "text/plain");
            return;
          }
          auto&sc=season.uid2scoder[uid];
          sc.uid=uid;
          sc.sysname=carr[uid].sysname;
          if (!season.phases.empty()) {
            t_phase& cur = season.phases[curp];
            cur.uid2rec[uid] = {};
          }
          it = season.uid2scoder.find(uid);
        }

        t_season_coder& sc = it->second;
        if (!sc.allowed_next_src_upload()) {
          res.status = 429;
          res.set_content("Rate limit: wait 60 seconds between submissions", "text/plain");
          return;
        }
        compq.push_job(std::move(new_source_job(uid,src)));
        res.status = 200;
        res.set_content("["+qap_time()+"] - ok // size = "+to_string(src.size()),"text/plain");
      } catch (const exception& e) {
        res.status = 500;
        res.set_content(string("Exception: ") + e.what(), "text/plain");
      }
    });
    /*
    srv.Post("/source/new",[this](const httplib::Request& req, httplib::Response& res) {
      //RATE_LIMITER(25);
      try {
        if(req.body.size()>=1024*512){res.status=409;res.set_content("max request size is 512KB","text/plain");return;}
        auto j = json::parse(req.body);
        string coder = LowerStr(j.value("coder", ""));
        string src = j.value("src", "");
        string token = j.value("token", "");
        if (coder.empty() || src.empty()|| token.empty()) {
          res.status = 400;
          res.set_content("Missing required fields: coder or src or token", "text/plain");
          return;
        }
        if (src.size()>=1024*256) {
          res.status = 409;
          res.set_content("max source size is 256KB","text/plain");
          return;
        }
        {
          lock_guard<mutex> lock(carr_mtx);
          auto*p=coder2rec(coder);
          if(!p){res.status=404;res.set_content("not found","text/plain");return;}
          if(p->token!=token){res.status=403;res.set_content("unauthorized","text/plain");return;}
          p->last_ip=req.remote_addr;
          //if(!p->allowed_next_src_upload()){res.status=429;res.set_content("rate limit exceeded","text/plain");return;}
        }
        //compq.push_job(std::move(new_source_job(coder,src)));
        res.status = 200;
        res.set_content("["+qap_time()+"] - ok // size = "+to_string(src.size()),"text/plain");
      }
      catch (const std::exception& e) {
        res.status = 500;
        res.set_content(std::string("Exception: ") + e.what(), "text/plain");
      }
    });
    srv.Post("/game/mk", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(25);
      try {
        if(req.body.size()>=1024*128){res.status=409;res.set_content("max request size is 128KB","text/plain");return;}
        auto j = json::parse(req.body);
        string author = LowerStr(j.value("author", ""));
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
          auth_rec->last_ip=req.remote_addr;
        }
        map<string,string>&last_game_time=coder2lgt;
        {
          auto t=qap_time();
          lock_guard<mutex> lock(coder2lgt_mtx);
          if(0)if (last_game_time.count(author) && (qap_time_diff(last_game_time[author],t)) < 5*60*1000) {
            res.status = 429;
            //res.set_content("Rate limit: one game per 10 sec", "text/plain");
            res.set_content("Rate limit: one game per 5 minutes", "text/plain");
            return;
          }
          last_game_time[author]=t;
        }
        vector<t_game_slot> slots;
        {
          string err;
          for (auto& p : players_json) {
            string coder = LowerStr(p.value("coder", ""));
            string version = p.value("version", "latest");
            lock_guard<mutex> lock(carr_mtx);
            if(ai2cid.empty()){err="under_construction";break;}
            
            t_coder_rec*rec=coder.empty()?&carr[ai2cid[rand()%ai2cid.size()]]:coder2rec(coder);
            if(!rec){err="not found coder with name: "+coder;break;}
            lock_guard<mutex> lock2(*rec->sarr_mtx);
            auto&sarr=rec->sarr;
            int ver=(version=="latest"||version.empty())?(int)sarr.size()-1:stoi(version);
            ver=rec->try_get_last_valid_ver(ver);
            if(ver<0){err="wrong version for coder: "+coder;break;}
            t_game_slot slot;
            slot.coder=rec->sysname;
            slot.v=ver;
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
        json resp = {{"game_id", gd.game_id}, {"status", "scheduled"},{"time",qap_time()},{"slot:",slots.size()}};
        res.status = 200;
        res.set_content(resp.dump(), "application/json");
      } catch (const exception& e) {
        res.status = 500;
        res.set_content(string("Exception: ") + e.what(), "text/plain");
      }
    });
    srv.Get("/top", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(5);
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
            {"name",c.visname},
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
    });*//*
    auto game2json=[](const t_game&g,json&j){
      j = {
        {"game_id", g.gd.game_id},
        {"tick", g.fg.tick},
        {"size", g.fg.size},
        {"ot", g.ordered_at},
        {"ft", g.finished_at},
        {"status", g.status},
        {"author", g.author},
        {"players", json::array()}
      };
      int z=-1;auto&jp=j["players"];
      for (const auto& p : g.gd.arr) {
        z++;
        auto s=g.fg.slot2score.size()?g.fg.slot2score[z]:0.0;
        auto ms=g.fg.slot2ms.size()?g.fg.slot2ms[z]:0.0;
        auto st=g.fg.slot2status.size()?g.fg.slot2status[z]:"ok";
        jp.push_back({{"coder",p.coder},{"score",s},{"ms",ms},{"v",g.gd.arr[z].v},{"status",st}});
      }
    };
    srv.Get("/games/latest", [this,game2json](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(25);
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
      RATE_LIMITER(25);
      string coder = LowerStr(req.matches[1]);
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
      RATE_LIMITER(25);
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
              c.last_ip=req.remote_addr;
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
          {"name", rec->visname},
          {"email", rec->email},
          {"rating", rec->elo},
          {"total_games", rec->total_games},
          {"last_upload", rec->sarr.empty() ? "" : rec->sarr.back().time}
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
      RATE_LIMITER(25);
      string name = LowerStr(req.matches[1]);
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
      vector<json> sarr;
      {
        lock_guard<mutex> lock(*rec->sarr_mtx);
        for(auto&ex:rec->sarr){
          json j;
          j["time"]=ex.time;
          j["prod_time"]=ex.prod_time;
          j["status"]=ex.status;
          j["size"]=ex.size;
          sarr.push_back(std::move(j));
        }
      }
      json resp = {
        {"id", rec->id},
        {"name", rec->visname},
        {"rating", rec->elo},
        {"total_games", rec->total_games},
        {"last_upload", rec->sarr.empty() ? "" : rec->sarr.back().time},
        {"sarr", sarr}
      };
      res.status = 200;
      res.set_content(resp.dump(2), "application/json");
    });
    srv.Get(R"(/game/(\d+))", [this,game2json](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(25);
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
      RATE_LIMITER(25);
      json status;
      {
        lock_guard<mutex> lock(carr_mtx);
        status["coders"] = carr.size();
      }
      {
        lock_guard<mutex> lock(garr_mtx);
        status["games_total"] = garr.size();
        int running = 0, finished = 0, uploaded=0, news=0;
        for (const auto& g : garr) {
          if (g.status == "new") news++;
          if (g.status == "finished") finished++;
          if (g.status == "uploaded") uploaded++;
        }
        status["games_news"] = news;
        //status["games_running"] = running;
        status["games_finished"] = finished;
        status["games_uploaded"] = uploaded;
      }
      status["compq.jobs.size"]=compq.jobs.size();
      status["compq.jobs_all"]=compq.jobs_all.load();
      status["compq.jobs_upe"]=compq.jobs_upe.load();
      status["compq.jobs_done"]=compq.jobs_done.load();
      {
        lock_guard<mutex> lock(sch.mtx);
        vector<json> narr;
        for(auto&ex:sch.narr){
          json j;
          j["name"]=ex.name;
          j["total_cores"]=ex.total_cores;
          j["used_cores"]=ex.used_cores;
          j["online"]=ex.online;
          j["last_heartbeat"]=ex.last_heartbeat;
          narr.push_back(std::move(j));
        }
        status["nodes_online"]=narr;
      }
      status["time"] = qap_time();
      status["start_time"] = main_start_time;
      status["debug_time"] = "2025.08.25 10:11:11.454";

      res.status = 200;
      res.set_content(status.dump(2), "application/json");
    });*/
    srv.Get(R"(/api/seasons/([^/]+)/phases/([^/]+)/leaderboard)", [this](const httplib::Request& req, httplib::Response& res) {
      string season_name = req.matches[1];
      string phase_name = req.matches[2];
      RATE_LIMITER(15);
      lock_guard<mutex> lock(mtx);
      const t_season* season = find_season(season_name);
      if (!season) {
        res.status = 404;
        res.set_content("Season not found", "text/plain");
        return;
      }
      const t_phase* phase = find_phase(*season, phase_name);
      if (!phase) {
        res.status = 404;
        res.set_content("Phase not found", "text/plain");
        return;
      }
      vector<pair<double, uint64_t>> ranked;
      for (const auto& [uid, rec] : phase->uid2rec) {
        ranked.emplace_back(-rec.score, uid);
      }
      sort(ranked.begin(), ranked.end());
      json lb = json::array();
      int rank = 1;
      for (auto& [neg_score, uid] : ranked) {
        if (uid >= carr.size()) continue;
        lb.push_back({
          {"rank", rank++},
          {"uid", uid},
          {"coder", carr[uid].sysname},
          {"score", -neg_score},
          {"games", phase->uid2rec.at(uid).games}
        });
      }
      json resp = {
        {"season", season_name},
        {"phase", phase_name},
        {"is_completed", phase->is_completed},
        {"leaderboard", lb}
      };
      res.status = 200;
      res.set_content(resp.dump(2), "application/json");
    });

    srv.Post(R"(/api/seasons/([^/]+)/phases/([^/]+)/manual_game)", [this](const httplib::Request& req, httplib::Response& res) {
      string season_name = req.matches[1];
      string phase_name = req.matches[2];
      RATE_LIMITER(10);
      string token=get_token_from_request(req,res);
      if(token.empty())return;
      try {
        auto j = json::parse(req.body);
        // Найти uid по токену
        uint64_t uid = UINT64_MAX;
        {
          lock_guard<mutex> lock(mtx);
          for (size_t i = 0; i < carr.size(); ++i) {
            if (carr[i].token == token) {
              uid = i;
              break;
            }
          }
        }
        if (uid == UINT64_MAX) {
          res.status = 403;
          res.set_content("Invalid token", "text/plain");
          return;
        }
        lock_guard<mutex> lock(mtx);
        t_season* season = find_season(season_name);
        if (!season || season->is_finalized) {
          res.status = 404;
          res.set_content("Season not found or finalized", "text/plain");
          return;
        }
        t_phase* phase = find_phase(*season, phase_name);
        if (!phase || phase->type != "sandbox" || !phase->is_active) {
          res.status = 400;
          res.set_content("Manual games allowed only in active sandbox phases", "text/plain");
          return;
        }
        if (season->uid2scoder.count(uid) == 0) {
          res.status = 403;
          res.set_content("You are not a participant of this season", "text/plain");
          return;
        }
        auto players_json = j.value("players", json::array());
        if (players_json.empty()) {
          res.status = 400;
          res.set_content("Invalid players list", "text/plain");
          return;
        }
        vector<t_game_slot> slots;
        for (auto& pj : players_json) {
          t_game_slot slot;
          if (pj.contains("coder")) {
            string coder_name = LowerStr(pj["coder"]);
            string version = pj.value("version", "latest");
            t_coder_rec* coder = nullptr;
            for (auto& c : carr) {
              if (c.sysname == coder_name) {
                coder = &c;
                break;
              }
            }
            if (!coder) {
              res.status = 404;
              res.set_content("Coder not found: " + coder_name, "text/plain");
              return;
            }
            auto it = season->uid2scoder.find(coder->id);
            if (it == season->uid2scoder.end()) {
              res.status = 403;
              res.set_content("Coder not in this season: " + coder_name, "text/plain");
              return;
            }
            auto& sc = it->second;
            int ver = (version == "latest") ? (int)sc.sarr.size() - 1 : stoi(version);
            ver = sc.try_get_last_valid_ver(ver);
            if (ver < 0) {
              res.status = 400;
              res.set_content("No valid version for coder: " + coder_name, "text/plain");
              return;
            }
            slot.uid = coder->id;
            slot.coder = coder->sysname;
            slot.v = ver;
            slot.cdn_bin_file = sc.sarr[ver].cdn_bin_url;
          } else {
            res.status = 400;
            res.set_content("Each player must have 'coder'", "text/plain");
            return;
          }
          slots.push_back(slot);
        }
        uint64_t maxtick = min<uint64_t>(j.value("maxtick", phase->ticksPerGame), 20000);
        double msPerTick = min<double>(j.value("msPerTick", (double)phase->msPerTick), 100.0);
        uint64_t stderrKB = min<uint64_t>(j.value("stderrKB", phase->stderrKB), 64);

        t_game_decl gd;
        gd.arr = slots;
        gd.world = phase->world;
        gd.config = phase->game_config;
        gd.maxtick = maxtick;
        gd.TL = msPerTick;
        gd.TL0 = max(gd.TL0, msPerTick);
        gd.stderr_max = stderrKB * 1024;

        {
          gd.game_id = garr.size();
          gd.season = season->season;
          gd.phase = phase->phase;
          gd.wave = kInvalidIndex;
          t_game game;
          game.gd = gd;
          game.status = "manual";
          game.ordered_at = qap_time();
          game.author = "manual:" + carr[uid].sysname;
          garr.push_back(std::move(game));
        }

        sch.add_game_decl(gd);

        json resp = {
          {"game_id", gd.game_id},
          {"status", "scheduled"},
          {"time", qap_time()}
        };
        res.status = 200;
        res.set_content(resp.dump(), "application/json");
      } catch (const exception& e) {
        res.status = 500;
        res.set_content("Exception: " + string(e.what()), "text/plain");
      }
    });
    srv.Post("/api/seasons/current/config", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(10);
      try {
        if (req.body.empty()) {
          res.status = 400;
          res.set_content("Empty request body", "text/plain");
          return;
        }
        json j = json::parse(req.body);
        auto cfg = j.get<TSeasonConfig>();
        lock_guard<mutex> lock(mtx);
        res.status=apply_config(cfg)?200:400;
        res.set_content((res.status==200?"ok:":"fail:")+qap_time(),"text/plain");
      } catch (const std::exception& e) {
        res.status = 500;
        res.set_content("Exception: " + string(e.what()), "text/plain");
      }
    });
    srv.Get("/api/seasons/current", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(10);
      lock_guard<mutex> lock(mtx);
      if (seasons.empty()) {
        res.status = 404;
        res.set_content("No active season", "text/plain");
        return;
      }
      const auto& s = seasons.back();
      json j;
      j["season_name"] = s.season_name;
      j["title"] = s.title;
      j["cur_phase"] = s.cur_phase;
      j["is_finalized"] = s.is_finalized;
      json phases = json::array();
      for (const auto& p : s.phases) {
        json pj;
        pj["phase"] = p.phase;
        pj["name"] = p.phase_name;
        pj["type"] = p.type;
        pj["playersPerGame"] = p.num_players;
        pj["world"] = p.world;
        pj["startTime"] = p.scheduled_start_time;
        pj["endTime"] = p.scheduled_end_time;
        pj["is_active"] = p.is_active;
        pj["is_closed"] = p.is_closed;
        pj["is_completed"] = p.is_completed;
        pj["games_count"] = p.games.size();
        pj["finished_games"] = p.finished_games;

        pj["ticksPerGame"] = p.ticksPerGame;
        pj["msPerTick"] = p.msPerTick;
        pj["stderrKB"] = p.stderrKB;

        if (p.type == "round") {
          json rules = json::array();
          for (const auto& r : p.qualifying_from) {
            rules.push_back({{"fromPhaseName", s.phases[r.from_phase].phase_name}, {"topN", r.top_n}});
          }
          pj["qualifyingFrom"] = rules;
        }
        phases.push_back(pj);
      }
      j["phases"] = phases;
      res.status = 200;
      res.set_content(j.dump(2), "application/json");
    });
    srv.Get("/api/seasons/current/me", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(25);
      lock_guard<mutex> lock(mtx);
      string token=get_token_from_request(req,res);
      if(token.empty())return;
      uint64_t uid = UINT64_MAX;
      string sysname;
      for (size_t i = 0; i < carr.size(); ++i) {
        if (carr[i].token == token) {
          uid = i;
          sysname = carr[i].sysname;
          break;
        }
      }
      if (uid == UINT64_MAX) {
        res.status = 403;
        res.set_content("Invalid token", "text/plain");
        return;
      }
      if (seasons.empty()) {
        res.status = 404;
        res.set_content("No active season", "text/plain");
        return;
      }

      const t_season& season = seasons.back();
      json resp;
      resp["uid"] = uid;
      resp["sysname"] = sysname;

      auto it = season.uid2scoder.find(uid);
      bool in_season = (it != season.uid2scoder.end());
      resp["in_season"] = in_season;

      if (!in_season) {
        resp["can_submit_now"] = false;
        res.status = 200;
        res.set_content(resp.dump(), "application/json");
        return;
      }

      const t_season_coder& sc = it->second;
      resp["last_submit_time"] = sc.sarr.empty() ? "" : sc.sarr.back().time;
      resp["can_submit_now"] = sc.allowed_next_src_upload();

      // Найти текущую активную фазу
      const t_phase* active_phase = nullptr;
      for (const auto& p : season.phases) {
        if (p.is_active) {
          active_phase = &p;
          break;
        }
      }

      if (active_phase && active_phase->uid2rec.count(uid)) {
        const auto& rec = active_phase->uid2rec.at(uid);
        vector<pair<double, uint64_t>> ranked;
        for (const auto& [u, r] : active_phase->uid2rec) {
          ranked.emplace_back(-r.score, u);
        }
        sort(ranked.begin(), ranked.end());
        int rank = 1;
        for (auto& [_, u] : ranked) {
          if (u == uid) break;
          rank++;
        }
        resp["current_phase"] = {
          {"name", active_phase->phase_name},
          {"type", active_phase->type},
          {"rank", rank},
          {"rank_of", ranked.size()},
          {"score", rec.score},
          {"games_played", rec.games}
        };
      }

      res.status = 200;
      res.set_content(resp.dump(), "application/json");
    });
    srv.Get(R"(/api/seasons/([^/]+)/coders/(\w+)/versions)", [this](const httplib::Request& req, httplib::Response& res) {
      string season_name = req.matches[1];
      string coder_name = LowerStr(req.matches[2]);
      RATE_LIMITER(15);
      lock_guard<mutex> lock(mtx);

      const t_season* season = find_season(season_name);
      if (!season) {
        res.status = 404;
        res.set_content("Season not found", "text/plain");
        return;
      }

      const t_coder_rec* coder = nullptr;
      for (const auto& c : carr) {
        if (c.sysname == coder_name) {
          coder = &c;
          break;
        }
      }
      if (!coder) {
        res.status = 404;
        res.set_content("Coder not found", "text/plain");
        return;
      }

      auto it = season->uid2scoder.find(coder->id);
      if (it == season->uid2scoder.end()) {
        res.status = 404;
        res.set_content("Coder not in this season", "text/plain");
        return;
      }

      const t_season_coder& sc = it->second;
      json versions = json::array();
      for (size_t i = 0; i < sc.sarr.size(); ++i) {
        const auto& v = sc.sarr[i];
        versions.push_back({
          {"version", i},
          {"time", v.time},
          {"prod_time", v.prod_time},
          {"status", v.status},
          {"size", v.size},
          {"cdn_src_url", v.cdn_src_url},
          {"cdn_bin_url", v.cdn_bin_url}
        });
      }

      json resp = {
        {"season", season_name},
        {"coder", coder_name},
        {"uid", coder->id},
        {"versions", versions}
      };
      res.status = 200;
      res.set_content(resp.dump(2), "application/json");
    });
    srv.Get(R"(/api/seasons/([^/]+)/strategies/latest)", [this](const httplib::Request& req, httplib::Response& res) {
      string season_name = req.matches[1];
      int n = req.has_param("n") ? stoi(req.get_param_value("n")) : 20;
      n = max(1, min(n, 1000));
      RATE_LIMITER(15);
      lock_guard<mutex> lock(mtx);
      const t_season* season = find_season(season_name);
      if (!season) {
        res.status = 404;
        res.set_content("Season not found", "text/plain");
        return;
      }
      vector<tuple<string, uint64_t, uint64_t, string>> versions; // {time, uid, ver, status}
      for (const auto& [uid, sc] : season->uid2scoder) {
        for (size_t v = 0; v < sc.sarr.size(); ++v) {
          const auto& ver = sc.sarr[v];
          if (ver.time.empty()) continue;
          versions.emplace_back(ver.time, uid, v, ver.status);
        }
      }
      sort(versions.begin(), versions.end(), [](const auto& a, const auto& b) {
        return get<0>(a) > get<0>(b);
      });
      if ((int)versions.size() > n) versions.resize(n);
      json out = json::array();
      for (auto& [time, uid, ver, status] : versions) {
        if (uid >= carr.size()) continue;
        auto&src=season->uid2scoder.at(uid).sarr[ver];
        out.push_back({
          {"time", time},
          {"uid", uid},
          {"coder", carr[uid].sysname},
          {"version", ver},
          {"status", status},
          {"size", src.size},
          {"cdn_src_url", src.cdn_src_url}
        });
      }
      res.status = 200;
      res.set_content(out.dump(2), "application/json");
    });
    srv.Get("/", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(25);
      res.set_file_content("index.html","text/html");
    });
    #define F(FN)\
      srv.Get("/" FN, [this](const httplib::Request&req,httplib::Response&res){\
        RATE_LIMITER(25);res.set_file_content(FN,"text/html");\
      });
    //---
    F("index.html");
    F("comments.html");
    F("ainews.html");
    F("admin.html");
    F("strategies.html");
    srv.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout <<"["<<qap_time()<<"]t_site: "<< "Request: " << req.method << " " << req.path;
        if (!req.remote_addr.empty()) {
            std::cout << " from " << req.remote_addr;
        }
        std::cout << " | Response Status: " << res.status << std::endl;
    });
    setup_routes_v2();
  }
  pair<uint64_t, bool> auth_by_bearer(const httplib::Request& req) {
    string auth = req.get_header_value("Authorization");
    if (auth.substr(0, 7) != "Bearer ") return {0, false};
    string token = auth.substr(7);
    for (size_t i = 0; i < carr.size(); ++i) {
      if (carr[i].token == token) return {i, true};
    }
    return {0, false};
  }
  int64_t get_karma(uint64_t uid,const chrono::system_clock::time_point&now) {
    if (uid >= carr.size()) return -1;
    if (carr[uid].is_banned(now)) return -999999;
    return carr[uid].karma.tot();
  }
  json comment_to_json(const t_comment& c, const vector<t_coder_rec>& carr,bool user) {
    json j = {
      {"id", c.id},
      {"parent_id", c.parent_id},
      {"author", c.author_uid < carr.size() ? carr[c.author_uid].sysname : "deleted"},
      {"author_uid", c.author_uid},
      {"title", c.hidden.tot()>0&&user?"":c.title},
      {"url", c.hidden.tot()>0&&user?"":c.url},
      {"content", c.hidden.tot()>0&&user?"":c.content},
      {"created_at", c.created_at},
      {"upvotes", c.votes.up},
      {"downvotes", c.votes.down},
      {"comment_count", c.comments.size()},
      {"hidden", c.hidden.tot()>0},
      {"is_pinned", c.is_pinned.tot()>0}
    };
    return j;
  }

  json build_comment_tree(uint64_t id, const vector<t_comment>& comments, const vector<t_coder_rec>& carr, bool user) {
    if (id >= comments.size()) return nullptr;
    json node = comment_to_json(comments[id], carr,user);
    json replies = json::array();
    for (uint64_t child_id : comments[id].comments) {
      replies.push_back(build_comment_tree(child_id, comments, carr,user));
    };
    node["replies"] = replies;
    return node;
  }
  void setup_routes_v2(){
    srv.Get("/api/comments", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(20);
      string sort = req.get_param_value("sort");
      if (sort != "new") sort = "hot";
      int n = min(100, max(1, req.has_param("n") ? stoi(req.get_param_value("n")) : 30));

      lock_guard<mutex> lock(mtx);
      vector<pair<double, const t_comment*>> scored;
      auto now=qap_time();
      for (const auto& c : comments) {
        if (c.parent_id != 0 || c.hidden.tot()>0) continue;
        double score = (sort == "new")
          ? qap_time_parse(c.created_at)
          : (c.votes.tot() - 1) / pow(qap_time_diff(c.created_at, now) / (3600.0 * 1000.0) + 2, 1.5);
        scored.emplace_back(score, &c);
      }
      std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.first > b.first; });
      if ((int)scored.size() > n) scored.resize(n);

      json out = json::array();
      for (auto& [_, c] : scored) {
        if (c->author_uid >= carr.size()) continue;
        out.push_back({
          {"id", c->id},
          {"author", carr[c->author_uid].sysname},
          {"author_uid", c->author_uid},
          {"title", c->title},
          {"url", c->url},
          {"content", c->content},
          {"created_at", c->created_at},
          {"upvotes", c->votes.up},
          {"downvotes", c->votes.down},
          {"comment_count", c->comments.size()}
        });
      }
      res.set_content(out.dump(), "application/json");
    });
    srv.Delete(R"(/api/comments/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(5);
      uint64_t id = stoull(req.matches[1]);
      lock_guard<mutex> lock(mtx);
      if (id >= comments.size() || comments[id].hidden.tot()>0) {
        res.status = 404;
        return;
      }
      auto&c=comments[id];
      auto [uid, ok] = auth_by_bearer(req);
      if (!ok) { res.status = 401; return; }
      if (uid>=carr.size()){
        res.status = 403;
        return;
      }
      bool moder=carr[uid].lvl<999999;
      auto author=c.author_uid==uid;
      if(!(moder||author)){
        res.status = 403;
        res.set_content("allowed only for moders or author", "text/plain");
        return;
      }
      auto j = json::parse(req.body);
      string reason=j.value("reason", "");
      auto delta=j.value("delta",999999-int64_t(carr[uid].lvl));
      if (reason.empty()||reason.size()>1024*4) {
          res.status = 400;
          res.set_content(R"({"error":"reason empty or over 4KB"})", "application/json");
          return;
      }
      c.hidden.add(t_vote::mk(uid,author?1:delta,qap_time(),reason));
      res.set_content(R"({"ok":true})", "application/json");
    });
    srv.Get(R"(/api/user/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(5);
      uint64_t uid = stoull(req.matches[1]);
      lock_guard<mutex> lock(mtx);
      if (uid>=carr.size()){
        res.status = 403;
        return;
      }
      auto&c=carr[uid];
      json j = {
        {"name", c.visname},
        {"lvl", c.lvl},
        {"time", c.time},
        {"karma", c.karma.tot()}
      };
      res.set_content(j.dump(2), "application/json");
    });
    srv.Post("/api/comments", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(5);
      lock_guard<mutex> lock(mtx);
      auto [uid, ok] = auth_by_bearer(req);
      if (!ok) { res.status = 401; return; }
      if (get_karma(uid,chrono::system_clock::now()) < 0) {
        res.status = 403;
        res.set_content("Karma too low", "text/plain");
        return;
      }

      auto j = json::parse(req.body);
      uint64_t parent_id = j.value("parent_id", uint64_t(0));
      string title = j.value("title", "");
      string url = j.value("url", "");
      string content = j.value("content", "");
      title=sanitizeHtml(title,true);
      url=sanitizeHtml(url,true);
      content=sanitizeHtml(content,false);
      // Для корневого поста: требуем title и url
      if (parent_id == 0 && (title.empty() || url.empty() || url.find("http") != 0)) {
        res.status = 400;
        res.set_content("Root posts require non-empty title and valid URL", "text/plain");
        return;
      }
      t_comment c;
      c.id = comments.size();
      c.parent_id = parent_id;
      c.author_uid = uid;
      c.title = title;
      c.url = url;
      c.content = content;
      c.created_at = qap_time();
      comments.push_back(c);

      // Связываем с родителем
      if (parent_id != 0 && parent_id < comments.size()) {
        comments[parent_id].comments.push_back(c.id);
      }

      res.status = 201;
      res.set_content("\""+qap_time()+"\"", "application/json");
    });
    srv.Get(R"(/api/comments/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(15);
      lock_guard<mutex> lock(mtx);
      auto [uid, ok] = auth_by_bearer(req);
      uint64_t id;
      try {
        id = stoull(req.matches[1]);
      } catch (...) {
        res.status = 400;
        res.set_content(R"({"error":"invalid comment id"})", "application/json");
        return;
      }
      if (id >= comments.size() || comments[id].hidden.tot()) {
        res.status = 404;
        return;
      }
      json tree = build_comment_tree(id, comments, carr,uid);
      res.set_content(tree.dump(2), "application/json");
    });
    srv.Get("/save", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(15);
      auto [uid, ok] = auth_by_bearer(req);
      if (!ok||uid) { res.status = 404; return; }
      auto s=QapSaveToStr(*this);
      file_put_contents("save.qap",s);
      res.set_content("["+qap_time()+"]: size="+to_string(s.size()/1024.0/1024.0)+"MB", "text/plain");
    });
    srv.Post(R"(/api/vote/comment/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        RATE_LIMITER(15);

        uint64_t comment_id;
        try {
            comment_id = stoull(req.matches[1]);
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid comment id"})", "application/json");
            return;
        }

        lock_guard<mutex> lock(mtx);

        auto [uid, ok] = auth_by_bearer(req);
        if (!ok || get_karma(uid, chrono::system_clock::now()) <= 0) {
            res.status = 401;
            return;
        }

        if (comment_id >= comments.size() || comments[comment_id].hidden.tot() > 0) {
            res.status = 404;
            return;
        }

        if (uid == comments[comment_id].author_uid) {
            res.status = 400;
            return; // нельзя голосовать за себя
        }

        int dir = 0;
        try {
            auto j = json::parse(req.body);
            dir = j.value("dir", 0);
            if (dir != 1 && dir != -1) {
                res.status = 400;
                res.set_content(R"({"error":"invalid vote direction"})", "application/json");
                return;
            }
        } catch (const json::parse_error& e) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
            //std::cerr << "JSON parse error: " << e.what() << std::endl;
            return;
        }

        t_vote v;
        v.from_uid = uid;
        v.delta = dir;
        v.time = qap_time();
        v.reason = "comment_vote";
        comments[comment_id].votes.add(v);

        auto& target_uid = comments[comment_id].author_uid;
        if (target_uid < carr.size()) {
            t_vote kv;
            kv.from_uid = uid;
            kv.delta = dir;
            kv.time = v.time;
            kv.reason = "karma_from_comment:" + std::to_string(comment_id);
            carr[target_uid].karma.add(kv);
        }

        res.set_content(R"({"ok":true})", "application/json");
    });

    srv.Post(R"(/api/vote/user/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        RATE_LIMITER(15);

        lock_guard<mutex> lock(mtx);

        uint64_t target_uid;
        try {
            target_uid = stoull(req.matches[1]);
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid user id"})", "application/json");
            return;
        }

        auto [voter_uid, ok] = auth_by_bearer(req);
        if (!ok || get_karma(voter_uid, chrono::system_clock::now()) <= 0) {
            res.status = 401;
            return;
        }
        if (voter_uid == target_uid) {
            res.status = 400;
            return;
        }

        int dir = 0;string reason;
        try {
            auto j = json::parse(req.body);
            dir = j.value("dir", 0);
            reason = j.value("reason", reason);
            if (dir != 1 && dir != -1) {
                res.status = 400;
                res.set_content(R"({"error":"invalid vote direction"})", "application/json");
                return;
            }
            if (reason.empty()||reason.size()>1024*4) {
                res.status = 400;
                res.set_content(R"({"error":"reason empty or over 4KB"})", "application/json");
                return;
            }
        } catch (const json::parse_error& e) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
            //std::cerr << "JSON parse error: " << e.what() << std::endl;
            return;
        }

        if (target_uid >= carr.size()) { res.status = 404; return; }

        t_vote v;
        v.from_uid = voter_uid;
        v.delta = dir;
        v.time = qap_time();
        v.reason = reason;
        carr[target_uid].karma.add(v);

        res.set_content(R"({"ok":true})", "application/json");
    });

    srv.Post(R"(/api/mod/ban/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
      RATE_LIMITER(15);
      uint64_t target_uid = stoull(req.matches[1]);
      lock_guard<mutex> lock(mtx);
      if (target_uid >= carr.size()) { res.status = 404; return; }
      auto [mod_uid, ok] = auth_by_bearer(req);
      if (!ok || carr[mod_uid].lvl>=carr[target_uid].lvl) {
        res.status = 403;
        return;
      }
      auto j = json::parse(req.body);
      auto reason = j.value("reason",string{});
      auto dt = j.value("dt",1000i64*3600*24*7*2);
      auto hide_all = j.value("hide_all",false);
      if (reason.empty()) {
        res.status = 403;
        res.set_content(R"({"ok":false,"err":"need not empty reason"})", "application/json");
        return;
      }
      auto now=qap_time();
      auto&c=carr[target_uid];
      if(!c.bans.empty()){
        auto&prev=c.bans.back();
        if(qap_time_diff(prev.until,now)<0){
          if(prev.from_uid<carr.size()&&carr[prev.from_uid].lvl<carr[mod_uid].lvl){
            res.status = 403;
            res.set_content(R"({"ok":false,"err":"you do not have sufficient rights"})", "application/json");
            return;
          }
        }
      }
      auto&b=qap_add_back(c.bans);
      b.from_uid=mod_uid;
      b.time=now;
      b.until=qap_time_addms(now,dt);
      b.reason=reason;
      if(hide_all)for(auto&c:comments)if(c.author_uid==target_uid)c.hidden=true;
      res.set_content(R"({"ok":true})", "application/json");
    });
  }
  string main_start_time=qap_time();
public:
//---T_MAIN_TEST---
public:
  const t_season*find_season(const string&name)const{for(auto&p:seasons)if(p.season_name==name)return &p;return nullptr;}
  t_season*find_season(const string&name){for(auto&p:seasons)if(p.season_name==name)return &p;return nullptr;}
  const t_phase* find_phase(const t_season& season, const string& phase_name)const{
    for (auto& p : season.phases) {
      if (p.phase_name == phase_name) return &p;
    }
    return nullptr;
  }
  t_phase* find_phase(t_season& season,const string&phase_name){
    for (auto& p : season.phases) {
      if (p.phase_name == phase_name) return &p;
    }
    return nullptr;
  }
  bool is_previous_phase_completed(const t_season& season, const t_phase& current) {
    if (current.phase == 0) return true; // первая фаза — всегда готова

    const t_phase& prev = season.phases[current.phase - 1];
    return prev.is_completed;
  }
  void finalize_previous_phase(t_season& season, const t_phase& current) {
    if (current.phase == 0) return;
    t_phase& prev = season.phases[current.phase - 1];
    if (prev.is_completed) return;
    if (!prev.is_closed) return; // ждём только если закрыта

    bool all_games_done = (prev.finished_games == prev.games.size());
    if (!all_games_done) return;

    compute_ratings_for_phase(prev, season);
    prev.is_completed = true;
    LOG("Phase " + prev.phase_name + " finalized.");
  }
  void compute_ratings_for_phase(const t_phase& current,t_season& season){}
public:
  //void ensure_user_in_season(t_season& season, uint64_t user_id) {
  //  t_phase* active =&season.phases[season.cur_phase];
  //  if (season.uid2scoder.count(user_id) == 0) {
  //    season.uid2scoder[user_id] = t_season_coder{user_id, "", {}};
  //    active->uid2rec[user_id]={};
  //  }
  //}
  //void init_splinter_2025_season(t_season& s) {
  //  s.season_name = "splinter_2025";
  //  //s.world = "t_splinter";
  //  s.title = "Splinter 2025";
  //
  //  // Определяем фазы в порядке их появления
  //  vector<tuple<string, string, uint64_t, vector<t_phase::t_qr_decl>>> phase_specs = {
  //    // {phase_id, type, num_players, qualifying_from}
  //    {"S1", "sandbox", 16, {}},
  //    {"R1", "round",   16, {{"S1", 900}}},
  //    {"S2", "sandbox", 16, {}},
  //    {"R2", "round",   16, {{"R1", 300}, {"S2", 60}}},
  //    {"SF", "sandbox", 16, {}},
  //    {"F",  "round",   16, {{"R2", 50},  {"SF", 10}}},
  //    {"S",  "sandbox", 16, {}}
  //  };
  //  for (size_t i = 0; i < phase_specs.size(); ++i) {
  //    auto& [phase_name, type, num_players, rules] = phase_specs[i];
  //    t_phase p;
  //    p.phase = i;              // ← критично для ссылок из игр
  //    p.phase_name = phase_name;
  //    p.type = type;
  //    p.num_players = num_players;
  //    // ...
  //    s.phases.push_back(std::move(p));
  //  }
  //  //for (auto& [phase_id, type, num_players, rules] : phase_specs) {
  //  //  auto phase = make_unique<t_phase>();
  //  //  phase->phase_id = phase_id;
  //  //  phase->type = type;
  //  //  phase->num_players = num_players;
  //  //  //phase->qualifying_from = rules;
  //  //  // uids и uid2score заполнятся позже при активации
  //  //  s.phases.push_back(std::move(phase));
  //  //}
  //}
  void next_phase(t_season& season,uint64_t phase) {
    t_phase*target=&season.phases[phase];

    if (season.cur_phase < season.phases.size()) {
      season.phases[season.cur_phase].is_active = false;
    }
    target->is_active = true;
    season.cur_phase = phase;

    // === Проведём отбор участников ===
    target->uid2rec.clear();

    if (target->type == "sandbox") {
      // В песочницу — все, кто есть в сезоне
      for (const auto& [uid, _] : season.uid2scoder) {
        target->uid2rec[uid]={};
      }
    } else if (target->type == "round") {
      set<uint64_t> already_qualified;

      for (const auto& rule : target->qualifying_from) {
        t_phase*src_phase=&season.phases[rule.from_phase];
        if (!src_phase) continue;

        // Собираем кандидатов из исходной фазы
        vector<pair<double, uint64_t>> ranked;
        for (auto&[uid,rec]:src_phase->uid2rec) {
          // Пропускаем тех, кто уже отобран
          if (already_qualified.count(uid)) continue;

          double score = rec.score;
          ranked.emplace_back(-score, uid);
        }
        std::sort(ranked.begin(), ranked.end());

        // Берём до rule.top_n новых участников
        uint64_t added = 0;
        for (auto& [neg_score, uid] : ranked) {
          if (added >= rule.top_n) break;
          auto&rec=target->uid2rec[uid];
          rec.source_phase=rule.from_phase;
          already_qualified.insert(uid);
          added++;
        }
      }
    }
    LOG("Activated phase: "+to_string(phase)+" with "+to_string(target->uid2rec.size())+" participants");
    waveman.startRound(qap_time_diff(target->scheduled_start_time,target->scheduled_end_time));
  }
  void update_seasons() {
    lock_guard<mutex> lock(mtx);
    if (seasons.empty()) return;
    t_season& season = seasons.back();
    auto now = qap_time();

    // 1. Найдём активную фазу (для удобства)
    t_phase* active_phase = nullptr;
    for (auto& p : season.phases) {
      if (p.is_active) {
        active_phase = &p;
        break;
      }
    }

    // 2. Обработка каждой фазы
    for (size_t i = 0; i < season.phases.size(); ++i) {
      t_phase& phase = season.phases[i];

      // Пропускаем завершённые
      if (phase.is_completed) continue;

      // --- Закрытие предыдущей фазы ---
      if (i > 0) {
        t_phase& prev = season.phases[i - 1];
        // Если время фазы i наступило — закрываем фазу i-1
        if (!prev.is_closed && qap_time_diff(phase.scheduled_start_time, now) >= 0) {
          prev.is_active = false;
          prev.is_closed = true;
          LOG("Phase " + prev.phase_name + " closed. Waiting for " +
            to_string(prev.games.size() - prev.finished_games) + " games to finish.");
        }
      }
      if(i+1==season.phases.size()){
        if(phase.is_active&&qap_time_diff(phase.scheduled_end_time,now)>=0){
          phase.is_active = false;
          phase.is_closed = true;
          LOG("Phase " + phase.phase_name + " closed. Waiting for " +
            to_string(phase.games.size() - phase.finished_games) + " games to finish.");
        }
      }

      // --- Активация фазы ---
      if (!phase.is_active && !phase.is_closed) {
        bool can_activate = false;
        if (i == 0) {
          // Первая фаза — активируем по времени
          can_activate = (qap_time_diff(phase.scheduled_start_time, now) >= 0);
        } else {
          // Последующие — только если предыдущая завершена
          can_activate = season.phases[i - 1].is_completed &&
                  (qap_time_diff(phase.scheduled_start_time, now) >= 0);
        }
        if (can_activate) {
          next_phase(season, i);
        }
      }

      // --- Завершение фазы (если закрыта и все игры сыграны) ---
      if (phase.is_closed && !phase.is_completed) {
        if (phase.finished_games == phase.games.size() && !phase.games.empty()) {
          compute_ratings_for_phase(phase, season);
          phase.is_completed = true;
          LOG("Phase " + phase.phase_name + " COMPLETED.");
        }
      }
    }

    // 3. Запуск волн в активной фазе (если она не закрыта)
    if (active_phase && active_phase->is_active && !active_phase->is_closed) {
      launch_missing_games_for_phase(*active_phase, season);
    }
    // После обработки всех фаз:
    if (!season.is_finalized) {
      bool all_completed = season.phases.size();
      for (auto& p : season.phases) {
        if (!p.is_completed) {
          all_completed = false;
          break;
        }
      }
      if (all_completed) {
        season.is_finalized = true;
        LOG("Season " + season.season_name + " FINALIZED");
      }
    }
  }
  void seasons_main_loop(){
    for(;;){
      update_seasons();
      std::this_thread::sleep_for(128ms);
    }
  }
  void launch_missing_games_for_phase(t_phase& phase, t_season& season) {
    if (!phase.is_active || phase.is_closed) return;

    auto now = qap_time();

    bool should_launch_wave = false;
    string wave_start_time=qap_time();

    if (phase.type == "round") {
      //if (phase.current_wave >= phase.total_waves) return; // всё сделано
      should_launch_wave = phase.prev_wave_done()&&phase.on_prev_wave_end;
    } else if (phase.type == "sandbox") {
      if (qap_time_diff(phase.last_wave_time,now)>=phase.sandbox_wave_interval_ms) {
        should_launch_wave = phase.prev_wave_done()&&phase.on_prev_wave_end;
      }
    }

    if(!should_launch_wave)return;
    waveman.roundDurationMS=qap_time_diff(phase.scheduled_start_time,phase.scheduled_end_time);
    if(!waveman.tryStartNextWave())return;

    vector<t_game_decl> wave = generate_wave(phase, season);
    if (wave.empty()) return;
    
    phase.on_prev_wave_end=false;
    if(phase.games.size())waveman.markWaveEnd();
    waveman.markWaveStart();
    vector<uint64_t> new_gids;
    {
      //lock_guard<mutex> lock(mtx);
      for (auto& gd : wave) {
        gd.game_id = garr.size();
        gd.season=season.season;
        gd.phase=phase.phase;
        gd.wave=phase.wave2gid.size();
        t_game game;
        game.gd = gd;
        game.status = "scheduled";
        game.ordered_at = wave_start_time;
        game.author = "system";//:season:" + season.season_name + ":" + phase.phase_name + ":wave" + to_string(phase.current_wave);
        garr.push_back(std::move(game));
        new_gids.push_back(gd.game_id);
      }
    }

    phase.games.insert(phase.games.end(),new_gids.begin(),new_gids.end());
    phase.wave2gid.push_back(std::move(new_gids));
    phase.last_wave_time = wave_start_time;
    //phase.current_wave++;
    LOG("Launched wave for " + phase.phase_name +
      " (" + phase.type + ") with " + to_string(wave.size()) + " games");

    for (auto& gd : wave) {
      sch.add_game_decl(gd);
    }
  }
  default_random_engine dre{time(0)};
  vector<t_game_decl> generate_wave(t_phase& phase, t_season& season) {
    vector<t_game_decl> wave;
    //lock_guard<mutex> lock(mtx);

    if (phase.uid2rec.empty()) return{};
    if (phase.num_players > carr.size()) return{};

    // Проверка: все ли uid корректны?
    for (auto&[uid,rec]:phase.uid2rec) {
      if (uid >= carr.size()) {
        LOG("generate_wave: invalid uid " + to_string(uid) + " >= carr.size()");
        return{};
      }
    }
    vector<uint64_t> uids;
    for(auto&[uid,rec]:phase.uid2rec){
      uids.push_back(uid);
    }
    shuffle(uids.begin(), uids.end(), dre);

    uint64_t games_in_wave = 0;
    if (phase.type == "round") {
      games_in_wave = double(phase.uid2rec.size())/(phase.num_players?phase.num_players:2);
    } else {
      //double hours_since_last = qap_time_diff(phase.last_wave_time, qap_time()) / (3600.0 * 1000.0);
      games_in_wave = double(phase.uid2rec.size())/(phase.num_players?phase.num_players:2);
    }
    // ниже код создаст games_in_wave игр.
    size_t next_idx = 0;
    for (uint64_t g = 0; g < games_in_wave; ++g) {
      vector<t_game_slot> slots;
      for (uint64_t p = 0; p < phase.num_players; ++p) {
        if (next_idx >= uids.size()) {
          next_idx = 0;
          shuffle(uids.begin(), uids.end(), dre);
        }
        uint64_t uid = uids[next_idx++];
        auto& coder = season.uid2scoder[uid];
        if (coder.sarr.empty()) continue;

        int ver = -1;
        {
          //lock_guard<mutex> lock2(*coder.sarr_mtx);
          ver = coder.try_get_last_valid_ver((int)coder.sarr.size() - 1);
          if (ver < 0) continue;
          QapAssert(coder.sysname.size());
          t_game_slot slot;
          slot.uid=uid;
          slot.coder = coder.sysname;
          slot.v = ver;
          slot.cdn_bin_file = coder.sarr[ver].cdn_bin_url;
          slots.push_back(slot);
        }
      }

      if (slots.size() == phase.num_players) {
        t_game_decl gd;
        gd.arr = slots;
        gd.config = phase.game_config;
        gd.world = phase.world;
        gd.stderr_max=phase.stderrKB;
        gd.TL=phase.msPerTick;
        gd.TL0=std::max(gd.TL0,gd.TL);
        gd.maxtick=phase.ticksPerGame;
        wave.push_back(gd);
      }
    }

    return wave;
  }
public:
  bool handle_set_phase_num_players(const string& phase_name, uint64_t num_players) {
    lock_guard<mutex> lock(mtx);

    if (seasons.empty()) return false;
    t_season&season=seasons.back();

    t_phase* target_phase=find_phase(season,phase_name);

    if (!target_phase) {
      LOG("handle_set_phase_num_players: phase '" + phase_name + "' not found");
      return false;
    }

    if (num_players < 2 || num_players > 64) {
      LOG("handle_set_phase_num_players: invalid num_players=" + to_string(num_players));
      return false;
    }

    target_phase->num_players = num_players;
    LOG("Updated phase '" + phase_name + "' num_players to " + to_string(num_players));
    return true;
  }
  //bool handle_schedule_transition(const string& from_phase_name, const string& to_phase_name, const string& time) {
  //  if (seasons.empty()) return false;
  //  t_season&season = seasons.back();
  //
  //  auto*target_phase=find_phase(season,to_phase_name);
  //  if (!target_phase) {
  //    LOG("handle_schedule_transition: target phase '" + to_phase_name + "' not found");
  //    return false;
  //  }
  //  season.scheduled_transitions.push_back({false,to_phase_name,time});
  //
  //  LOG("Scheduled transition to '" + to_phase_name + "' at " + time);
  //  return true;
  //}
  bool handle_add_sandbox_wave(const t_season& season) {
    return false;
  }
public:
  struct t_dsl_cmd {
    enum type {
      unknown,
      single_sandbox,
      standard_phases,
      set_num_players,
      set_qualification,
      add_phase,
      remove_phase,
      schedule_transition
    } cmd = unknown;

    // set_num_players
    string phase_name;
    uint64_t num_players = 0;

    // set_qualification: qual_rules = {{"R1",300}, {"S2",60}}
    vector<pair<string, uint64_t>> qual_rules;

    // add_phase
    string new_phase_name;
    string new_phase_type; // "sandbox" or "round"
    uint64_t new_num_players = 2;

    // remove_phase
    string phase_to_remove;

    // schedule_transition
    string from_phase;
    string to_phase;
    string at_time;
  };

  // Безопасный stoull с проверкой
  bool safe_stoull(const string& s, uint64_t& out) {
    if (s.empty()) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    try {
      out = stoull(s);
      return true;
    } catch (...) { return false; }
  }
  t_dsl_cmd parse_dsl_command(const string& input) {
    t_dsl_cmd cmd;

    // 1. single_sandbox
    if (input == "single_sandbox") {
      cmd.cmd = t_dsl_cmd::single_sandbox;
      return cmd;
    }

    // 2. standard_phases
    if (input == "standard_phases") {
      cmd.cmd = t_dsl_cmd::standard_phases;
      return cmd;
    }

    // 3. schedule_transition: "S1->R1 at ..."
    size_t arrow = input.find("->");
    size_t at_kw = input.find(" at ");
    if (arrow != string::npos && at_kw != string::npos && at_kw > arrow + 2) {
      cmd.cmd = t_dsl_cmd::schedule_transition;
      cmd.from_phase = input.substr(0, arrow);
      cmd.to_phase  = input.substr(arrow + 2, at_kw - (arrow + 2));
      cmd.at_time  = input.substr(at_kw + 4);
      return cmd;
    }

    // 4. Остальные команды: PHASE_NAME(...)
    size_t paren = input.find('(');
    if (paren == string::npos || input.back() != ')') return cmd;

    string phase = input.substr(0, paren);
    string inner = input.substr(paren + 1, input.size() - paren - 2);

    // 4a. set_num_players: "R2(players_per_game=4)"
    if (inner.size() >= 18 && inner.substr(0, 18) == "players_per_game=") {
      cmd.cmd = t_dsl_cmd::set_num_players;
      cmd.phase_name = phase;
      safe_stoull(inner.substr(18), cmd.num_players);
      return cmd;
    }

    // 4b. set_qualification: "R2(qual_from=R1:300,S2:60)"
    if (inner.size() >= 11 && inner.substr(0, 11) == "qual_from=") {
      cmd.cmd = t_dsl_cmd::set_qualification;
      cmd.phase_name = phase;
      string rules_str = inner.substr(11);
      vector<string> rules = split(rules_str, ",");
      for (const string& rule : rules) {
        size_t colon = rule.find(':');
        if (colon == string::npos) continue;
        string src_phase = rule.substr(0, colon);
        string count_str = rule.substr(colon + 1);
        uint64_t count;
        if (safe_stoull(count_str, count)) {
          cmd.qual_rules.push_back({src_phase, count});
        }
      }
      return cmd;
    }

    // 4c. add_phase: "add_phase(S3,sandbox,8)"
    if (phase == "add_phase") {
      cmd.cmd = t_dsl_cmd::add_phase;
      vector<string> args = split(inner, ",");
      if (args.size() == 3) {
        cmd.new_phase_name = args[0];
        cmd.new_phase_type = args[1];
        safe_stoull(args[2], cmd.new_num_players);
      }
      return cmd;
    }

    // 4d. remove_phase: "remove_phase(S3)"
    if (phase == "remove_phase") {
      cmd.cmd = t_dsl_cmd::remove_phase;
      cmd.phase_to_remove = inner;
      return cmd;
    }

    return cmd;
  }
  void apply_single_sandbox(t_season& s) {
    s.phases.clear();
    t_phase p;
    p.phase = 0;
    p.phase_name = "S1";
    p.type = "sandbox";
    p.num_players = 16;
    p.game_config = "";
    p.is_active = true;
    s.phases.push_back(p);
    s.cur_phase = 0;
    LOG("Season reset to single sandbox (S1)");
  }
  void apply_standard_phases(t_season& s) {
    //s.phases.clear();
    //
    //struct spec { string name; string type; uint64_t players; };
    //vector<spec> specs = {
    //  {"S1", "sandbox", 16},
    //  {"R1", "round",  16},
    //  {"S2", "sandbox", 16},
    //  {"R2", "round",  16},
    //  {"SF", "sandbox", 16},
    //  {"F", "round",  16},
    //  {"S", "sandbox", 16}
    //};
    //
    //for (size_t i = 0; i < specs.size(); ++i) {
    //  t_phase p;
    //  p.phase = i;
    //  p.phase_name = specs[i].name;
    //  p.type = specs[i].type;
    //  p.num_players = specs[i].players;
    //  p.game_config = "";
    //  p.is_active = (i == 0); // only S1 active initially
    //  s.phases.push_back(p);
    //}
    //
    //// Квалификации (как в init_splinter_2025_season)
    //if (s.phases.size() > 1) {
    //  s.phases[1].qualifying_from = {{0, 900}}; // R1 ← S1 top 900
    //}
    //if (s.phases.size() > 3) {
    //  s.phases[3].qualifying_from = {{1, 300}, {2, 60}}; // R2 ← R1 top 300 + S2 top 60
    //}
    //if (s.phases.size() > 5) {
    //  s.phases[5].qualifying_from = {{3, 50}, {4, 10}}; // F ← R2 top 50 + SF top 10
    //}
    //
    //s.cur_phase = 0;
    //LOG("Season set to standard phases: S1-R1-S2-R2-SF-F-S");
  }
  bool apply_set_qualification(t_season& s, const string& target_phase_name, const vector<pair<string, uint64_t>>& rules) {
    //t_phase* target = find_phase(s, target_phase_name);
    //if (!target) return false;
    //
    //target->qualifying_from.clear();
    //for (auto& [src_name, top_n] : rules) {
    //  const t_phase* src = find_phase(s, src_name);
    //  if (src) {
    //    target->qualifying_from.push_back({src->phase, top_n});
    //  }
    //}
    return true;
  }
  bool apply_add_phase(t_season& s, const string& name, const string& type, uint64_t players) {
    if (type != "sandbox" && type != "round") return false;
    if (players < 2 || players > 64) return false;

    // Проверка уникальности имени
    for (auto& p : s.phases) {
        if (p.phase_name == name) return false;
    }

    t_phase p;
    p.phase = s.phases.size();
    p.phase_name = name;
    p.type = type;
    p.num_players = players;
    p.game_config = "";
    //p.world
    p.is_active = false;
    s.phases.push_back(p);
    return true;
  }
  void apply_dsl_command(const string& config_str) {
    //lock_guard<mutex> lock(mtx);
    if (seasons.empty()) return;
    t_season& s = seasons.back();

    auto cmd = parse_dsl_command(config_str);

    switch (cmd.cmd) {
      case t_dsl_cmd::single_sandbox:
        apply_single_sandbox(s);
        break;
      case t_dsl_cmd::standard_phases:
        apply_standard_phases(s);
        break;
      case t_dsl_cmd::set_num_players:
        handle_set_phase_num_players(cmd.phase_name, cmd.num_players);
        break;
      case t_dsl_cmd::set_qualification:
        apply_set_qualification(s, cmd.phase_name, cmd.qual_rules);
        break;
      case t_dsl_cmd::add_phase:
        apply_add_phase(s, cmd.new_phase_name, cmd.new_phase_type, cmd.new_num_players);
        break;
      case t_dsl_cmd::remove_phase:
        //apply_remove_phase(s, cmd.phase_to_remove);
        break;
      case t_dsl_cmd::schedule_transition:
        //handle_schedule_transition(cmd.from_phase, cmd.to_phase, cmd.at_time);
        break;
      default:
        LOG("Unknown DSL command: " + config_str);
    }
  }
public:
  bool apply_config(const TSeasonConfig& cfg) {
    //lock_guard<mutex> lock(mtx);

    if (seasons.empty()) {
      LOG("apply_config: no active season");
      return false;
    }

    t_season& season = seasons.back();

    // Карта: имя фазы → индекс в season.phases
    unordered_map<string, size_t> name_to_index;
    for (size_t i = 0; i < season.phases.size(); ++i) {
      name_to_index[season.phases[i].phase_name] = i;
    }

    for (const auto& pc : cfg.phases) {
      t_phase* target = nullptr;
      bool is_new = false;

      auto it = name_to_index.find(pc.phaseName);
      if (it != name_to_index.end()) {
        target = &season.phases[it->second];
      } else {
        t_phase new_phase;
        new_phase.phase = season.phases.size(); // стабильный ID
        new_phase.phase_name = pc.phaseName;
        new_phase.type = pc.type;
        season.phases.push_back(new_phase);
        target = &season.phases.back();
        name_to_index[pc.phaseName] = season.phases.size() - 1;
        is_new = true;
      }

      target->world = pc.world;
      target->num_players = pc.playersPerGame;
      target->game_config = ""; // или передавай отдельно, если нужно
      target->scheduled_start_time=pc.startTime;
      target->scheduled_end_time=pc.endTime;

      target->ticksPerGame=pc.ticksPerGame;
      target->msPerTick=pc.msPerTick;
      target->stderrKB=pc.stderrKB;

      //target->is_active = false;
      if (pc.type == "sandbox") {
         target->games_per_coder_per_hour = pc.gamesPerCoderPerHour;
         target->sandbox_wave_interval_ms = pc.gamesPerCoderPerHour?3600*1000/pc.gamesPerCoderPerHour:5*60*1000;
      }

      if (pc.type == "round") {
        target->qualifying_from.clear();
        for (const auto& rule : pc.qualifyingFrom) {
          uint64_t src_phase_id = kInvalidIndex;
          for (const auto& p : season.phases) {
            if (p.phase_name == rule.fromPhaseName) {
              src_phase_id = p.phase;
              break;
            }
          }
          if (src_phase_id != kInvalidIndex) {
            auto&b=qap_add_back(target->qualifying_from);
            b.from_phase=src_phase_id;
            b.top_n=rule.topN;
          }
          // Если фаза ещё не создана — правило проигнорируется (или можно отложить)
        }
      }

      if (is_new) {
        LOG("Added new phase: " + pc.phaseName);
      } else {
        LOG("Updated phase: " + pc.phaseName);
      }
    }
    season.sync();
    return true;
  }
public:
  static void LOG2(const string&str){cerr<<"["<<::qap_time()<<"]["<<qap_time()<<"] "<<(str)<<endl;}
  static string get_json_cfg(){
    const char* v2=R"({
  "machineTypes": [
    {
      "cores": 4,
      "pricePerDay": 50
    }
  ],
  "diskPricePerGBPerDay": 0.4,
  "seasonName": "splinter_2025",
  "startTime": "2025.11.02 00:00:00.000",
  "phases1": [],
  "phases2": [],
  "phases": [
    {
      "name": "S1",
      "type": "sandbox",
      "durationHours": 72,
      "coders": 1000,
      "gamesPerCoderPerHour": 1,
      "ticksPerGame": 7500,
      "msPerTick": 35,
      "playersPerGame": 4,
      "stderrKB": 16,
      "replayBytesPerTick": 100,
      "world": "t_splinter",
      "startTime": "2025.11.02 00:00:00.000",
      "endTime": "2025.11.05 00:00:00.000"
    },
    {
      "name": "R1",
      "type": "round",
      "durationHours": 24,
      "coders": 900,
      "gamesPerCoder": 48,
      "ticksPerGame": 7500,
      "msPerTick": 35,
      "playersPerGame": 4,
      "stderrKB": 16,
      "replayBytesPerTick": 100,
      "world": "t_splinter",
      "qualifyingFrom": [
        {
          "fromPhaseName": "S1",
          "topN": 900
        }
      ],
      "startTime": "2025.11.05 00:00:00.000",
      "endTime": "2025.11.06 00:00:00.000"
    },
    {
      "name": "S2",
      "type": "sandbox",
      "durationHours": 72,
      "coders": 500,
      "gamesPerCoderPerHour": 1,
      "ticksPerGame": 7500,
      "msPerTick": 35,
      "playersPerGame": 4,
      "stderrKB": 16,
      "replayBytesPerTick": 100,
      "world": "t_splinter",
      "startTime": "2025.11.06 00:00:00.000",
      "endTime": "2025.11.09 00:00:00.000"
    },
    {
      "name": "R2",
      "type": "round",
      "durationHours": 24,
      "coders": 360,
      "gamesPerCoder": 96,
      "ticksPerGame": 7500,
      "msPerTick": 35,
      "playersPerGame": 4,
      "stderrKB": 16,
      "replayBytesPerTick": 100,
      "world": "t_splinter",
      "qualifyingFrom": [
        {
          "fromPhaseName": "R1",
          "topN": 300
        },
        {
          "fromPhaseName": "S2",
          "topN": 60
        }
      ],
      "startTime": "2025.11.09 00:00:00.000",
      "endTime": "2025.11.10 00:00:00.000"
    },
    {
      "name": "SF",
      "type": "sandbox",
      "durationHours": 72,
      "coders": 250,
      "gamesPerCoderPerHour": 1,
      "ticksPerGame": 7500,
      "msPerTick": 35,
      "playersPerGame": 4,
      "stderrKB": 16,
      "replayBytesPerTick": 100,
      "world": "t_splinter",
      "startTime": "2025.11.10 00:00:00.000",
      "endTime": "2025.11.13 00:00:00.000"
    },
    {
      "name": "F",
      "type": "round",
      "durationHours": 24,
      "coders": 60,
      "gamesPerCoder": 250,
      "ticksPerGame": 7500,
      "msPerTick": 35,
      "playersPerGame": 4,
      "stderrKB": 16,
      "replayBytesPerTick": 100,
      "world": "t_splinter",
      "qualifyingFrom": [
        {
          "fromPhaseName": "R2",
          "topN": 50
        },
        {
          "fromPhaseName": "SF",
          "topN": 10
        }
      ],
      "startTime": "2025.11.13 00:00:00.000",
      "endTime": "2025.11.14 00:00:00.000"
    }
  ]
})";
    return v2;
  }
  // Внутри struct t_main:


// Подмена qap_time()
static string qap_time_off() {
    lock_guard<mutex> lock(sim_mtx);
    // Преобразуем sim_time_ms → строку вида "2025.11.02 12:34:56.789"
    auto total_sec = sim_time_ms / 1000;
    auto ms = sim_time_ms % 1000;

    // Начинаем с 2025-11-01 00:00:00 UTC
    time_t base = 1761955200; // 2025-11-01 00:00:00 UTC
    time_t t = base + total_sec;
    struct tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y.%m.%d %H:%M:%S", &tm);
    return string(buf) + "." + (ms < 10 ? "00" : ms < 100 ? "0" : "") + to_string(ms);
}

static void sim_sleep(uint64_t ms) {
  //for(int i=0;i<10;i++)
  {
    // Ждём в реальном времени, но продвигаем sim_time_ms
    this_thread::sleep_for(milliseconds(static_cast<int>(ms / sim_speed)));
    {
        lock_guard<mutex> lock(sim_mtx);
        sim_time_ms += ms;
    }
  }
}
  void create_initial_season(const string& season_name) {
    lock_guard<mutex> lock(mtx);
    if (!seasons.empty()) return;

    t_season s;
    s.season = 0;
    s.season_name = season_name;
    s.title = season_name;

    // Начальная песочница
    t_phase sandbox;
    sandbox.phase = 0;
    sandbox.phase_name = "S1";
    sandbox.type = "sandbox";
    sandbox.world = "t_splinter";
    sandbox.num_players = 4;
    sandbox.scheduled_start_time = qap_time(); // сейчас
    sandbox.sandbox_wave_interval_ms = 10 * 60 * 1000; // 10 минут
    sandbox.is_active = true;
    s.phases.push_back(sandbox);
    s.cur_phase = 0;

    seasons.push_back(s);
    LOG("Initial season '" + season_name + "' created with sandbox S1");
}
  void simulate_new_coders(int count, const string& phase_name) {
    lock_guard<mutex> lock(mtx);
    if (seasons.empty()) return;
    t_season& s = seasons.back();

    t_phase* target_phase = find_phase(s, phase_name);
    if (!target_phase) return;

    for (int i = 0; i < count; ++i) {
      uint64_t uid = carr.size();
      t_coder_rec coder;
      coder.id = uid;
      coder.sysname = "sim_coder_" + to_string(uid);
      coder.visname = coder.sysname;
      coder.email = coder.sysname + "@sim.test";
      coder.token = generate_token(coder.sysname, qap_time());
      coder.time = qap_time();
      carr.push_back(coder);

      // Добавить в сезон
      auto&sc=s.uid2scoder[uid];
      sc.uid=uid;
      sc.sysname=coder.sysname;
      auto&b=qap_add_back(sc.sarr);
      b.cdn_src_url=to_string(i);
      b.status="ok:{\"success\":true,\"";
      // Добавить в текущую фазу
      if (target_phase->uid2rec.count(uid) == 0) {
        target_phase->uid2rec[uid]={};
      }
    }
    LOG("Simulated " + to_string(count) + " new coders in phase " + phase_name);
  }
  static void run_season_simulation(t_main& m) {
    {
      lock_guard<mutex> lock(m.mtx);
      t_season s;
      s.season = 0;
      s.season_name = "splinter_2025";
      s.title = "Splinter 2025";
      m.seasons.push_back(s);
    }
    try {
      json j = json::parse(get_json_cfg());
      TSeasonConfig cfg = j.get<TSeasonConfig>();
      m.apply_config(cfg);
    } catch (const exception& e) {
      LOG("Failed to parse config: " + string(e.what()));
      return;
    }
    thread update_thread([&m]() {
      while (true) {
        m.update_seasons();
        {
          lock_guard<mutex> lock(m.mtx);
          for(auto&ex:m.garr){
            if(rand()%3==0){
              auto&s=ex.status;
              if(s=="finished")continue;
              s="finished";
              auto&p=m.seasons[ex.gd.season].phases[ex.gd.phase];
              p.finished_games++;
              p.on_prev_wave_end=p.prev_wave_done();
            }
          }
        }
        this_thread::sleep_for(100ms);
      }
    });
    update_thread.detach();
    LOG("Simulation started at " + qap_time());

    m.sim_sleep(0);
    m.simulate_new_coders(4, "S1");
    for(int d=1;d<24;d++)
    for(int i=0;i<24;i++){
      for(int i=0;i<60;i++)m.sim_sleep(60 * 1000);
      auto&s=m.seasons.back();
      auto&p=s.phases[s.cur_phase];
      if(p.phase_name[0]=='S')m.simulate_new_coders(1,p.phase_name);
    }
    LOG("Simulation completed");
  }
  static int sim_main() {
    t_main m;
    sim_speed = 10000.0;
    m.run_season_simulation(m);
    for (;;) this_thread::sleep_for(1s);
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
typedef int pid_t;
void kill_by_pid(...){}
pid_t fork(...){return {};}
void execl(...){return;}
int pipe(...){return {};}
static constexpr int STDIN_FILENO=0;
static constexpr int STDOUT_FILENO=0;
static constexpr int SIGPIPE=0;
#define dup2(...)0
#define fdopen(...)0
#define qap_read(...)0
#define send(...)0
#else
static inline pollfd make_pollin(int fd){return pollfd{fd,POLLIN,0};}
static inline pollfd make_pollinout(int fd){return pollfd{fd,POLLIN|POLLOUT,0};}
#define qap_close close
void kill_by_pid(pid_t runner_pid){kill(runner_pid,SIGTERM);}
#define qap_read read
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
               + CDN_URL_IMAGES + "universal-runner.tar";
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
void setup_main(t_main&m){
  auto mem=file_get_contents("save.qap");
  bool ok=QapLoadFromStr(m,mem);
  if(!ok){
    m={};
    t_season s;
    s.season = 0;
    s.season_name = "splinter_2025";
    s.title = "Splinter 2025";
    m.seasons.push_back(s);
  }
  //auto&b=qap_add_back(m.carr);
  //b.
  /*
  {auto&b=qap_add_back(qap_add_back(m.carr).set(0,"Adler","adler3d@gmail.com","321").sarr);
  b.status="ok:";b.cdn_bin_url="0";b.cdn_src_url="0";b.prod_time=qap_time();
  m.ai2cid.push_back(0);}
  {auto&b=qap_add_back(qap_add_back(m.carr).set(1,"Dobord","dobord@example.com","123").sarr);
  b.status="ok:";b.cdn_bin_url="1";b.cdn_src_url="1";b.prod_time=qap_time();}
  {auto&b=qap_add_back(qap_add_back(m.carr).set(2,"Adler3D","adler3d@ya.ru","321").sarr);
  b.status="ok:";b.cdn_bin_url="2";b.cdn_src_url="2";b.prod_time=qap_time();}
  m.ai2cid.push_back(0);
  m.ai2cid.push_back(1);
  m.ai2cid.push_back(2);
  qap_add_back(m.carr).set(3,"Admin","admin@example.com","admin");
  */
}
//#include "t_main_test.h"
#include <signal.h>

int main(int argc,char*argv[]){
  //main_test();//t_coder_rec::t_source
  //t_main::sim_main();
  //return 0;
  cout<<qap_time_addms("2025.03.01 01:02:47.600",0)<<endl;
  signal(SIGPIPE, SIG_IGN);
  srand(time(0));
  if(bool prod=true){
    string mode="all";
    if(argc>1)mode=argv[1];
    if("all"==mode){
      thread([]{
        t_cdn cdn;
        return cdn.main();
      }).detach();
      publish_runner_image();
      thread([]{
        if(!ensure_runner_image())return -3145601;
        t_node node;
        return node.main();
      }).detach();
      t_main m;
      setup_main(m);
      return m.main();
    }
    if("t_main"==mode){
      publish_runner_image();
      t_main m;
      setup_main(m);
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
  return 0;
}
