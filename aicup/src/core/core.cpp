#include "netapi.h"
#include "FromQapEng.h"

#include <vector>
#include <cmath>
#include <string>
using namespace std;

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
template<class TYPE>TYPE parse(string s){return {};}
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
  string main;
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
struct t_game_decl{vector<t_game_slot> arr;string config;int game_id=0;int maxtick=20000;int stderr_max=1024*64;};
struct t_main:t_process{
  vector<t_coder_rec> carr;
  vector<t_game_rec> garr;
  map<string,string> node2ipport;
  t_net_api capi;
  void create_new_game_on_node(const string&node,const t_game_decl&gd){
    capi.write_to_socket(node2ipport[node],"new_game,"+serialize(gd)+"\n");
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
static constexpr int O_WRONLY=0;
static constexpr int O_NONBLOCK=0;
static constexpr int O_RDONLY=0;
#else
#endif
struct t_node:t_process{
  struct t_pipe_names {
    string in;   // /tmp/ai_in_{game_id}_{player_id}
    string out;  // /tmp/ai_out_{game_id}_{player_id}
    string err;  // /tmp/ai_err_{game_id}_{player_id}
  };
  t_pipe_names make_pipe_names(int game_id, int player_id) {
    return {
      "/tmp/ai_in_" +to_string(game_id)+"_"+to_string(player_id)+"_"+to_string(rand()),
      "/tmp/ai_out_"+to_string(game_id)+"_"+to_string(player_id)+"_"+to_string(rand()),
      "/tmp/ai_err_"+to_string(game_id)+"_"+to_string(player_id)+"_"+to_string(rand())
    };
  }
  bool create_pipes(const t_pipe_names& names) {
    if (mkfifo(names.in.c_str(), 0666) == -1 && errno != EEXIST) return false;
    if (mkfifo(names.out.c_str(),0666) == -1 && errno != EEXIST)return false;
    if (mkfifo(names.err.c_str(),0666) == -1 && errno != EEXIST)return false;
    return true;
  }
  struct i_output{
    virtual void on_stdout(const string&data)=0;
    virtual void on_stderr(const string&data)=0;
    virtual void on_closed_stdout()=0;
    virtual void on_closed_stderr()=0;
    virtual void on_stdin_open()=0;
  };
  struct i_input{
    virtual void write_stdin(const string&data)=0;
    virtual void close_stdin()=0;
  };
  struct t_docker_api{
    unique_ptr<i_input>  input;
    unique_ptr<i_output> output;
    void send(const string&data){if(input)input->write_stdin(data);}
  };
  struct t_cmd{bool valid=true;};
  struct t_status{bool TL=false;bool ok()const{return !TL;}};
  struct t_runned_game{
    t_game_decl gd;
    int tick=0;
    vector<unique_ptr<t_docker_api>> slot2api;
    vector<t_cmd> slot2cmd;
    vector<t_status> slot2status;
    vector<int> slot2ready;
    vector<t_pipe_names> slot2pipe_names;
    void init(){
      slot2cmd.resize(gd.arr.size());
      slot2status.resize(gd.arr.size());
      slot2ready.resize(gd.arr.size());
    }
    void new_tick(){for(auto&ex:slot2ready)ex=false;}
    bool all_ready(){
      for(int i=0;i<slot2cmd.size();i++){
        if(slot2ready[i]||!slot2status[i].ok())continue;
        return false;
      }
      return true;
    }
  };
  struct t_player_ctx:i_output{
    int player_id;
    string coder;
    t_runned_game*pgame=nullptr;
    t_node*pnode=nullptr;
    string err;
    void on_stdout(const string&data)override{
      pnode->on_player_stdout(*pgame,player_id,data);
    }
    void on_stderr(const string&data)override{
      if(data.size()+err.size()<pgame->gd.stderr_max)err+=data;
    }
    void on_closed_stderr()override{}
    void on_closed_stdout()override{}
    void on_stdin_open()override{}
  };
  static constexpr int buff_size=1024*64;
  static void start_stdout_reader(const string&pipe_path,i_output*output){
    thread t([pipe_path,output](){
      int fd=open(pipe_path.c_str(),O_RDONLY);
      if(fd==-1){
        output->on_stderr("failed to open stdout pipe\n");
        return;
      }
      char buffer[buff_size];
      int n;
      while((n=read(fd,buffer,sizeof(buffer)-1))>0){
        buffer[n]='\0';
        output->on_stdout(string(buffer,n));
      }
      close(fd);
      output->on_closed_stdout();
    });
    t.detach();
  }
  static void start_stderr_reader(const string&pipe_path,i_output*output){
    thread t([pipe_path,output](){
      int fd=open(pipe_path.c_str(),O_RDONLY);
      if(fd==-1){
        output->on_stderr("failed to open stderr pipe\n");
        return;
      }
      char buffer[buff_size];
      int n;
      while((n=read(fd,buffer,sizeof(buffer)-1))>0){
        buffer[n]='\0';
        output->on_stderr(string(buffer, n));
      }
      close(fd);
      output->on_closed_stderr();
    });
    t.detach();
  }
  static bool build_ai_image(const string& binary_path, const string& container_id) {
    string dir = "/tmp/ai_build_" + container_id;
    system(("mkdir -p " + dir).c_str());
    system(("cp " + binary_path + " " + dir + "/ai.bin").c_str());
    ofstream df((dir + "/Dockerfile").c_str());
    df << "FROM ubuntu:20.04\n"
          "RUN useradd -m ai\n"
          "USER ai\n"
          "WORKDIR /home/ai\n"
          "COPY ai.bin /home/ai/\n"
          "RUN chmod +x ai.bin\n"
          "CMD [\"./ai.bin\"]\n";
    df.close();
    string cmd = "docker build -t ai_image_" + container_id + " " + dir;
    int result = system(cmd.c_str());
    return result == 0;
  }
  struct t_container_monitor {
    struct t_task {
      t_runned_game*pgame=nullptr;
      string container_id;
      int player_id;
      time_t started_at;
    };
    vector<t_task> tasks;
    t_node*pnode=nullptr;
    void add(t_runned_game*pgame,const string&container_id,int player_id) {
      tasks.push_back({pgame,container_id,player_id,time(0)});
    }
    static constexpr int MAX_TICK_TIME=50;
    void update(){
      for(auto&t:tasks){
        if(time(0)-t.started_at<=MAX_TICK_TIME)continue; // TODO: заменить на высокоточный таймер
        string cmd="docker kill "+t.container_id;
        system(cmd.c_str());
        t.pgame->slot2status[t.player_id].TL=true;
      }
    }
  };
  bool spawn_docker(const string&fn,t_docker_api&api,int game_id,int player_id){
    string container_id="game_"+to_string(game_id)+"_p"+to_string(player_id)+"_"+to_string(rand());
    auto names=make_pipe_names(game_id,player_id);
    if(!create_pipes(names)){
      api.output->on_stderr("create_pipes failed\n");
      return false;
    }
    string cmd = "docker run --rm "
                 "--name " + container_id + " "
                 "--memory=512m --cpus=1 --network=none --read-only "
                 "--mount type=pipe,src=" + names.in  + ",dst=/dev/stdin "
                 "--mount type=pipe,src=" + names.out + ",dst=/dev/stdout "
                 "--mount type=pipe,src=" + names.err + ",dst=/dev/stderr "
                 "ai_image_" + container_id;
    if(!build_ai_image(fn,container_id)) {
      api.output->on_stderr("build failed\n");
      return false;
    }
    FILE*pipe=popen(cmd.c_str(),"r+");
    if(!pipe){
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
    auto*output=api.output.get();
    if(output){
      start_stdout_reader(names.out,output);
      start_stderr_reader(names.err,output);
    }else return false;
    return true;
  }
  void spawn_docker_old(const string&fn,t_docker_api&api){
    struct t_stdin_writer:i_input{
      void write_stdin(const string&data){};
      void close_stdin(){};
    };
    api.input=make_unique<t_stdin_writer>();
    return;
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
      spawn_docker(ex.cdn_bin_file,c,gd.game_id,i);
    }
    game_n++;
  }
  void game_tick(t_runned_game&game) {
    string world_state=serialize(game.gd,game.tick);
    for(int i=0;i<game.slot2api.size();i++){
      auto&api=game.slot2api[i];
      api->send("vpow,"+world_state+"\n");
    }
    this_thread::sleep_for(50ms);
  }
  void on_player_stdout(t_runned_game&g,int player_id,const string&data){
    auto move=parse<t_cmd>(data);
    if(move.valid){
      g.slot2cmd[player_id]=move; // TODO: нужно гармотно сообщат о том что мув невалидный и мочить процесс с ошибкой.
      g.slot2ready[player_id]=true; // TODO: нужно где-то проверять g.all_ready
    }
  }
  int main(){
    t_container_monitor monitor;
    monitor.pnode=this;
    while(true){
      monitor.update();
      for(auto&gu:rgarr){
        auto&g=*gu.get();
        if(g.tick>=g.gd.maxtick)continue;
        game_tick(g);
        g.tick++;
        g.new_tick();
      }
      this_thread::sleep_for(50ms);
    }
  }
};

int main() {
  srand(time(0));
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
