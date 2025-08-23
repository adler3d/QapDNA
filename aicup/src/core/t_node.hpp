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
struct t_unix_socket {
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
      qap_close();
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
    if (n <= 0) { qap_close(); return ""; }
    return string(buf, n);
  }

  int read(char*buf,int size) {
    int n = recv(sock, buf, size, 0);
    if (n <= 0) { qap_close();}
    return n;
  }

  void qap_close() {
    if (sock != -1) { ::qap_close(sock); sock = -1; }
  }
};

struct t_node:t_process,t_node_cache{
  struct t_runned_game;
  struct t_docker_api_v2{
    int player_id;
    t_runned_game* pgame = nullptr;
    t_node* pnode = nullptr;

    t_unix_socket socket;
    string err;
    string conid;
    vector<double> time_log;
    string socket_path_in_container="/tmp/dokcon.sock";
    string socket_path_on_host;
    emitter_on_data_decoder decoder;
    ~t_docker_api_v2() {
        socket.qap_close();
        if (!socket_path_on_host.empty()) {
            unlink(socket_path_on_host.c_str());
        }
    }
    t_docker_api_v2() {
        decoder.cb = [this](const string& z, const string& msg) {
            if (z == "ai_stdout") {
                on_stdout(string_view(msg));
            } else if (z == "ai_stderr") {
                on_stderr(string_view(msg));
            } else if (z == "log") {
                on_stderr("[CTRL] " + msg + "\n");
            } else if(z=="ai_binary_ack"){
              pnode->send_vpow(*pgame,player_id);
            }
        };
    }
    void start_reading() {
        pnode->loop_v2.add(socket.sock, [this](int fd) {
            char buf[4096];
            int n = socket.read(buf, sizeof(buf));
            if (n > 0) {
                decoder.feed(buf, n);
            } else {
                // —окет закрыт
                on_closed_stdout();
                on_closed_stderr();
                pnode->loop_v2.remove(fd);
                socket.qap_close();
            }
        });
    }
    void on_stdout(const string_view& data) {
      pnode->on_player_stdout(*pgame, player_id, data);
    }

    void on_stderr(const string_view& data) {
      if (data.size() + err.size() < pgame->gd.stderr_max)
        err += data;
    }

    void on_closed_stdout() {}
    void on_closed_stderr() {}
    void on_stdin_open() {}

    void write_stdin(const string& data) {
      if (socket.connected) {
        stream_write(socket, "ai_stdin",data);
      }
    }

    void close_stdin() {
      if (socket.connected) {
        stream_write(socket, "ai_stdin","EOF\n");
      }
    }
  };
  struct t_status{
    bool TL=false;
    bool PF=false;
    bool ok()const{return !TL&&!PF;}
    string to_str(){if(ok())return "ok";string out=TL?"TL":"";if(PF)out+="PF";return out;}
  };
  struct t_runned_game{
    t_game_decl gd;
    int tick=0;
    bool started=true;
    vector<unique_ptr<t_docker_api_v2>> slot2api;
    vector<t_cmd> slot2cmd;
    vector<t_status> slot2status;
    vector<int> slot2ready;
    vector<string> slot2bin;
    vector<vector<t_cmd>> tick2cmds;
    string cdn_upload_token=UPLOAD_TOKEN;
    t_world w;
    void init(){
      slot2cmd.resize(gd.arr.size());
      slot2status.resize(gd.arr.size());
      slot2ready.resize(gd.arr.size());
      slot2bin.resize(gd.arr.size());
    }
    void free(){
      for (auto& api : slot2api) {
        if (api) {
          api->pnode->loop_v2.remove(api->socket.sock);
          api->socket.qap_close();
        }
      }
    }
    ~t_runned_game(){free();}
    void new_tick(){for(auto&ex:slot2ready)ex=false;for(auto&ex:slot2cmd)ex={};}
    bool all_ready(){
      for(int i=0;i<slot2cmd.size();i++){
        if(slot2ready[i]||!slot2status[i].ok())continue;
        return false;
      }
      return true;
    }
    t_cdn_game mk_cg(){
      t_cdn_game out;
      (t_finished_game&)out=mk_fg();
      for(int i=0;i<gd.arr.size();i++){
        auto&p=out.slot2player[i];auto&a=*slot2api[i];
        p.tick2ms=a.time_log;
        p.err=a.err;
      }
      out.tick2cmds=tick2cmds;
      return out;
    }
    t_finished_game mk_fg(){
      t_finished_game out;
      out.game_id=gd.game_id;
      out.slot2score=w.slot2score;
      for(auto&ex:slot2status){
        out.slot2status.push_back(ex.to_str());
      }
      out.tick=tick;
      //out.reason="ok";
      return out;
    }
  };
  static constexpr int buff_size=1024*64;
  bool download_binary(const string& cdn_url, string& out_binary) {
    string tmp_file = "/tmp/ai_bin_" + sha256(cdn_url);
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
    thread t([&](){
      string cmd="docker kill "+conid;
      system(cmd.c_str());
    });
    t.detach();
  }
  struct t_container_monitor{
    static void kill(t_runned_game&g,int pid){
      auto&conid=g.slot2api[pid]->conid;
      t_node::kill(conid);
    }
    struct t_task{
      t_runned_game*pgame=nullptr;
      int player_id;
      double started_at;
      double ms;
      bool done=false;
      bool deaded=false;
      void on_done(QapClock&clock){
        done=true;
        ms=clock.MS()-started_at;
        get().time_log.push_back(ms);
      }
      t_docker_api_v2&get(){return *pgame->slot2api[player_id];}
      bool kill_if_TL(double ms){
        auto&gd=pgame->gd;
        auto TL=pgame->tick?gd.TL:gd.TL0;
        if(ms<=TL)return false;
        kill(*pgame,player_id);
        pgame->slot2status[player_id].TL=true;
        deaded=true;
        return true;
      }
    };
    vector<t_task> tasks;
    t_node*pnode=nullptr;
    QapClock clock;
    void add(t_runned_game*pgame,int player_id) {
      tasks.push_back({pgame,player_id,clock.MS()});
    }
    void clear(int game_id){
      QapCleanIf(tasks,[&](const t_task&t){return t.pgame->gd.game_id==game_id;});
    }
    void update(){
      bool frag=false;
      for(auto&t:tasks)if(t.done){
        frag=frag||t.kill_if_TL(t.ms);
      }
      auto now=clock.MS();
      for(auto&t:tasks)if(!t.done){
        frag=frag||t.kill_if_TL(now-t.started_at);
      }
      //if(!frag)return;
      //QapCleanIf(tasks,[](const t_task&t){return t.deaded;});
    }
  };
  t_container_monitor container_monitor;
  bool spawn_docker(const string& cdn_url, t_docker_api_v2& api, int game_id, int player_id, t_runned_game& g) {
    api.conid = "game_" + to_string(game_id) + "_p" + to_string(player_id) + "_" + to_string((rand()<<16)+rand());
    //api.socket_path_on_host = "/tmp/dokcon_" + api.conid + ".sock";
    string socketDir = "/tmp/dokcon_sockets";
    api.socket_path_on_host=socketDir+"/dokcon_"+api.conid+".sock";
    api.socket_path_in_container="/tmp/dokcon_"+api.conid+".sock";
    unlink(api.socket_path_on_host.c_str());
    string binary;
    if (!download_binary(cdn_url, binary)) {
      api.on_stderr("download failed\n");
      return false;
    }
    g.slot2bin[player_id] = binary;
    system(("mkdir -p " + socketDir).c_str());
    string cmd = "docker run -d --rm --memory=512m --memory-swap=512m --network=none --cpus=\"1.0\" "
                 "-e SOCKET_PATH="+api.socket_path_in_container+" "
                 "--name " + api.conid + " "
                 "--mount type=bind,src="+socketDir+",dst=/tmp "
               //"--mount type=bind,src=" + api.socket_path_on_host + ",dst=/tmp/dokcon.sock "
                 "--mount type=tmpfs,tmpfs-size=64m,destination=/tmpfs "
                 "universal-runner:latest";

    int result = system(cmd.c_str());
    if (result != 0) {
      api.on_stderr("docker run failed: " + to_string(result) + "\n");
      return false;
    }
    auto*api_ptr=&api;
    bool ok=loop_v2.connect_to_container_socket(api,[this,api_ptr,binary](){
      stream_write(api_ptr->socket, "ai_binary", binary);
      stream_write(api_ptr->socket, "ai_start", "");
      api_ptr->start_reading();
    });

    return ok;
  }
  static string config2seed(const string&config){return {};}
  vector<unique_ptr<t_runned_game>> rgarr;
  // јсинхронна€ отправка
  void t_cdn_api_upload_async(
      const string& path,
      const t_cdn_game& data,
      const string& token,
      function<void(const string& error)> on_done
  ) {
    thread([path, data, token, on_done = move(on_done)]() {
      string error;
      try {
        auto s=http_put_with_auth(path, data.serialize(),token);
        if(s!=200)error="HTTP error "+to_string(s);
      } catch (const exception& e) {
        error = string("Exception in http_put_with_auth:") + e.what();
      }
      on_done(error);
    }).detach();
  }
  struct t_game_uploaded_ack{
    int game_id;
    string err;
  };
  t_net_api capi;
  void t_main_api_send(const string& z, const t_finished_game& ref) {
    string payload = "game_result," + to_string(ref.game_id) + "," + serialize(ref);
    capi.write_to_socket(local_main_ip_port, payload + "\n");
  }
  void t_main_api_send(const string& z, const t_game_uploaded_ack& ref) {
    string payload = "game_uploaded," + to_string(ref.game_id);
    if (!ref.err.empty()) {
        payload += ",error=" + ref.err;
    }
    capi.write_to_socket(local_main_ip_port, payload + "\n");
  }
  int game_n=0;mutex rgarr_mutex;
  bool new_game(const t_game_decl&gd){
    t_runned_game*pgame=nullptr;
    {lock_guard<mutex> lock(rgarr_mutex);auto&gu=qap_add_back(rgarr);gu=make_unique<t_runned_game>();pgame=gu.get();}
    auto&g=*pgame;
    g.gd=gd;
    g.init();
    int i=-1;
    for(auto&ex:gd.arr){
      i++;
      auto&b2=qap_add_back(g.slot2api);
      b2=make_unique<t_docker_api_v2>();
      t_docker_api_v2&v2=*b2.get();
      v2.pnode=this;
      v2.player_id=i;
      v2.pgame=&g;
      bool ok=spawn_docker(ex.cdn_bin_file,v2,gd.game_id,i,g);
      if(!ok){
        for(auto&api:g.slot2api){
          kill(api->conid);
        }
        LOG("game aborted due to spawn_docker error:"+v2.conid);
        lock_guard<mutex> lock(rgarr_mutex);
        QapCleanIf(rgarr,[&](auto&ref){return ref->gd.game_id==gd.game_id;});
        return false;
      }
    }
    game_n++;
  }
  void send_game_result(t_runned_game& game) {
    auto game_id = game.gd.game_id;
    auto finished_game = game.mk_fg();
    auto cdn_game = game.mk_cg();
    auto token = game.cdn_upload_token;
    t_main_api_send("game_result",finished_game);
    t_cdn_api_upload_async("replay/" + to_string(game_id), cdn_game, token,
      [this,game_id](const string&error){
        t_game_uploaded_ack a;
        a.game_id=game_id;
        a.err=error;
        t_main_api_send("game_uploaded",a);
        lock_guard<mutex> lock(rgarr_mutex);
        QapCleanIf(rgarr,[&](const auto&g){return g&&g->gd.game_id==game_id;});
      }
    );
  }
  void game_tick(t_runned_game&game){
    if(game.started){
      for(int i=0;i<game.slot2cmd.size();i++)game.w.use(i,game.slot2cmd[i]);
      game.tick2cmds.push_back(game.slot2cmd);
      game.w.step();
      if(game.w.finished()||game.tick>=game.gd.maxtick){
        send_game_result(game);
        for(int i=0;i<game.gd.arr.size();i++)container_monitor.kill(game,i);
        container_monitor.clear(game.gd.game_id);
        return;
      }
    }
    for(int i=0;i<game.slot2api.size();i++){
      if(!game.slot2status[i].ok())continue;
      send_vpow(game,i);
    }
  }
  void send_vpow(t_runned_game&game,int i){
    string vpow=serialize(game.w,i);
    auto&api=game.slot2api[i];
    api->write_stdin("vpow,"+vpow+"\n");
    container_monitor.add(&game,i);
  }
  void on_player_stdout(t_runned_game&g,int player_id,const string_view&data){
    string s(data);
    if(!g.started){
      if(s.find("READY")==string::npos){
        auto&api=g.slot2api[player_id];
        api->on_stderr("\nERROR DETECTED: YOU MUST SAY READY BEFORE FIRST ACTION!!!\n");
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
      if(r){g.slot2api[player_id]->on_stderr("\nERROR DETECTED: ANSWER AFTER ANSWER!!!\n");}
      r=true;
      for(auto&ex:container_monitor.tasks)if(ex.player_id==player_id)ex.on_done(container_monitor.clock);
    }else{
      auto&api=g.slot2api[player_id];
      api->on_stderr("INVALID_MOVE: parsing failed\n");
      container_monitor.kill(g,player_id);
      g.slot2status[player_id].PF=true;
    }
    if(g.all_ready()){
      tick(g);
    }
  }
  void tick(t_runned_game&g){
    container_monitor.update();
    container_monitor.clear(g.gd.game_id);
    game_tick(g);
    if(g.started)g.tick++;
    g.new_tick();
  }

  struct t_event_loop_v2{
    struct t_monitored_fd {
      int fd;
      string path;  // дл€ Unix-socket
      function<void(int fd)> on_ready;
      //function<void()> on_error;
      bool connected = false;
    };
    vector<t_monitored_fd> fds;
    mutex mtx;
    t_node*pnode=nullptr;
    void remove(int fd) {
      lock_guard<mutex> lock(mtx);
      QapCleanIf(fds, [fd](const t_monitored_fd& f) { return f.fd == fd; });
    }
    void add(int fd, function<void(int)>&&on_ready/*, const function<void()>& on_error = []{}*/) {
      lock_guard<mutex> lock(mtx);
      fds.push_back({fd, "", std::move(on_ready)/*, on_error*/});
    }
    void wait_for_socket(const std::string& socket_path, int max_wait_ms=1024, int poll_interval_ms=16) {
        namespace fs = std::filesystem;
        int waited = 0;
        while (!fs::exists(socket_path) && waited < max_wait_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
            waited += poll_interval_ms;
        }
        if (!fs::exists(socket_path)) {
          LOG("Socket file not created after waiting: "+socket_path);
        }
    }
    bool connect_to_container_socket(t_docker_api_v2&api,function<void()>&&on_connect) {
      wait_for_socket(api.socket_path_on_host);
      auto&client=api.socket;
      if (!client.connect_unix(api.socket_path_on_host)) {
        LOG("client.connect_unix(path) failed with "+api.socket_path_on_host); 
        return false;
      }

      auto wrapper = [this,&client,on_connect](int){
        if (client.connected)return;
        int err = 0;
        socklen_t len = sizeof(err);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
          client.qap_close();LOG("connect_to_container_socket::wrapper failed"); 
          return;
        }
        client.connected = true;
        on_connect();
      };

      add(client.sock,move(wrapper));
      return true;
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
        int n = poll(pfds.data(), pfds.size(), 1);  // 1ms таймаут Ч нормально
        if (n <= 0) continue;

        {
          lock_guard<mutex> lock(mtx);
          for (auto& pfd : pfds) {
            if (pfd.revents & (POLLERR | POLLHUP)) {
              for (auto& f : fds) {
                if (f.fd == pfd.fd) {
                  //if (f.on_error) f.on_error();
                  qap_close(pfd.fd);
                  remove(pfd.fd);  // удал€ем из списка
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
  t_event_loop_v2 loop_v2;
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

    // new_game(...) Ч где-то вызываетс€

    //loop.run(this);
    loop_v2.pnode=this;
    loop_v2.run();
    return 0;
  }
};
