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
    (void)system(("mkdir -p "+t_node_cache_dir).c_str());
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

  //string read_chunk() {
  //  char buf[4096];
  //  int n = recv(sock, buf, sizeof(buf)-1, 0);
  //  if (n <= 0) { qap_close(); return ""; }
  //  return string(buf, n);
  //}
  int read(char*buf,int size) {
    int n = recv(sock, buf, size, 0);
    if (n <= 0) {
      #ifdef _WIN32
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS) {
        return 0; // временно нет данных
      }
      #else
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0; // временно нет данных
      }
      #endif
      qap_close();
      return -1;
    }
    return n;
  }

  void qap_close() {
    if (sock != -1) { ::qap_close(sock); sock = -1; }
  }
};
struct t_player_packet_decoder {
  string buffer;
  vector<string> completed_packets;
  using packet_cb_t = function<void(const string& cmd)>;
  packet_cb_t on_packet;
  t_player_packet_decoder(packet_cb_t cb = nullptr) : on_packet(cb) {}
  void feed(const char* data, size_t len) {
    buffer.append(data, len);
    while (true) {
      if (buffer.size() < sizeof(uint32_t)) break;
      uint32_t cmd_len = *reinterpret_cast<const uint32_t*>(buffer.data());
      if (cmd_len > 1024 * 1024) {
        buffer.clear();
        if(on_packet)on_packet("");
        return;
      }
      size_t total_size = sizeof(uint32_t) + cmd_len;
      if (buffer.size() < total_size) break;
      string cmd(buffer.data() + sizeof(uint32_t), cmd_len);
      if (on_packet) on_packet(cmd);
      buffer.erase(0, total_size);
    }
  }
  bool has_incomplete_data() const {
    if (buffer.size() < sizeof(uint32_t)) return !buffer.empty();
    uint32_t len = *reinterpret_cast<const uint32_t*>(buffer.data());
    return buffer.size() < sizeof(uint32_t) + len;
  }
};
struct t_node:t_process,t_node_cache{
  struct t_runned_game;
  struct t_qaplr_process {
      pid_t pid = -1;
      int stdin_fd = -1;   // write to QapLR
      int stdout_fd = -1;  // read from QapLR
      FILE* stdin_file = nullptr;
      FILE* stdout_file = nullptr;

      t_runned_game* pgame = nullptr;
      t_node* pnode = nullptr;

      emitter_on_data_decoder decoder;

      ~t_qaplr_process() { stop(); }

      void stop() {
          if (stdin_file) { fclose(stdin_file); stdin_file = nullptr; }
          if (stdout_file) { fclose(stdout_file); stdout_file = nullptr; }
          if (pid > 0) {
              kill_by_pid(pid);
              pid = -1;
          }
      }

      bool start(const t_game_decl& gd) {
          int stdin_pipe[2], stdout_pipe[2];
          if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
              LOG("t_node::start::pipe error");
              return false;
          }

          pid = fork();
          if (pid == 0) {
              // Дочерний: перенаправляем stdin/stdout
              dup2(stdin_pipe[0], STDIN_FILENO);
              dup2(stdout_pipe[1], STDOUT_FILENO);
              qap_close(stdin_pipe[0]); qap_close(stdin_pipe[1]);
              qap_close(stdout_pipe[0]); qap_close(stdout_pipe[1]);

              // Формируем аргументы
              execl("./QapLR",
                  "QapLR",
                  gd.world.c_str(),
                  to_string(gd.arr.size()).c_str(),
                  to_string(gd.seed_initial).c_str(),
                  to_string(gd.seed_strategies).c_str(),
                  //("--world=" + gd.world).c_str(),
                  //("--num-players=" + to_string(gd.arr.size())).c_str(),
                  //("--seed-initial=" + to_string(gd.seed_initial)).c_str(),
                  //("--seed-strategies=" + to_string(gd.seed_strategies)).c_str(),
                  //"--no-gui",
                  "--remote",
                  nullptr
              );
              perror("execl QapLR failed");
              _exit(1);
          }

          if (pid < 0) {
              perror("fork");
              qap_close(stdin_pipe[0]); qap_close(stdin_pipe[1]);
              qap_close(stdout_pipe[0]); qap_close(stdout_pipe[1]);
              return false;
          }

          // Родитель: закрываем ненужные концы
          qap_close(stdin_pipe[0]);   // читать из stdin QapLR не будем
          qap_close(stdout_pipe[1]);  // писать в stdout QapLR не будем

          stdin_fd = stdin_pipe[1];
          stdout_fd = stdout_pipe[0];

          stdin_file = fdopen(stdin_fd, "w");
          stdout_file = fdopen(stdout_fd, "r");
          if (!stdin_file || !stdout_file) {
              stop();
              return false;
          }

          // Настройка буферизации: unbuffered для надёжности
          setvbuf(stdin_file, nullptr, _IONBF, 0);
          setvbuf(stdout_file, nullptr, _IONBF, 0);

          // Запуск чтения из stdout QapLR
          start_reading();

          return true;
      }

      void start_reading() {
          pnode->loop_v2.add(stdout_fd, [this](int fd) {
              t_unix_socket socket{fd};
              char buf[4096];
              int n = socket.read(buf, sizeof(buf));
              if (n > 0) {
                decoder.feed(buf, n);
              } else if (n < 0) {
                // Ошибка → закрыть
                pnode->loop_v2.remove(fd);
                socket.qap_close();
              }
          });
      }

      void write_zchan(const string& z, const string& payload) {
          string frame = qap_zchan_write(z, payload);
          if (stdin_file) {
              fwrite(frame.data(), 1, frame.size(), stdin_file);
              fflush(stdin_file);
          }
      }

      t_qaplr_process() {
          decoder.cb = [this](const string& z, const string& payload) {
              if (z == "finished") {
                  pnode->on_qaplr_finished(*pgame);
              } else if (z == "result") {
                  // payload: "score0,score1,..."
                  pgame->world.slot2score.clear();
                  stringstream ss(payload);
                  string item;
                  while (getline(ss, item, ',')) {
                      pgame->world.slot2score.push_back(stod(item));
                  }
              } else if (z.substr(0, 8) == "violate/") {
                  if (isdigit(z[8])) {
                      int player_id = stoi(z.substr(8));
                      if (qap_check_id(player_id, pgame->slot2api)) {
                          pnode->container_monitor.kill(*pgame, player_id);
                          pgame->slot2status[player_id].PF = true;
                      }
                  }
              } else if (z.substr(0, 3) == "err") {
                  // Логируем ошибки, но не убиваем
                  int player_id = stoi(z.substr(3));
                  if (qap_check_id(player_id, pgame->slot2api)) {
                      pgame->slot2api[player_id]->on_stderr("[QapLR] " + payload + "\n");
                  }
              }else if (z.substr(0, 5) == "seed/"&&isdigit(z[5])) {
                  int player_id = stoi(z.substr(5));
                  if (qap_check_id(player_id, pgame->slot2api)) {
                      // Отправляем seed в контейнер — БЕЗ запуска TL!
                      pgame->slot2api[player_id]->write_stdin(
                          payload//qap_zchan_write("seed", payload)
                      );
                      LOG("seed delivered");
                  }
              } else if (z[0] == 'p' && isdigit(z[1])) {
                  // Это данные для игрока — пересылаем в докер
                  int player_id = stoi(z.substr(1));
                  if (qap_check_id(player_id, pgame->slot2api)) {
                      pgame->slot2api[player_id]->write_stdin(
                          payload//qap_zchan_write("vpow", payload)  // или "cmd", но лучше "vpow"
                      );
                      // Запускаем таймер TL
                      pnode->container_monitor.add(pgame, player_id);
                  }
              }
          };
      }
  };
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
    string binary;
    ~t_docker_api_v2() {
        socket.qap_close();
        if (!socket_path_on_host.empty()) {
            unlink(socket_path_on_host.c_str());
        }
    }
    t_docker_api_v2() {
      decoder.cb = [this](const string& z, const string& msg) {
        LOG("t_node::t_docker_api_v2::cb::z='"+z+"' msg='"+msg+"' sock="+to_string(socket.sock));
        stream_write(socket, "stress", "test");
        if(z=="hi from dokcon.js"){
          stream_write(socket, "ai_binary", binary);
          stream_write(socket, "ai_start", "2025.10.18 12:55:05.686");
        }
        if (z == "ai_stdout") {
          on_stdout(string_view(msg));
        } else if (z == "ai_stderr") {
          on_stderr(string_view(msg));
        } else if (z == "log") {
          on_stderr("[CTRL] " + msg + "\n");
        } else if(z=="ai_binary_ack"){
          LOG("t_node::ai_binary_ack");
          pgame->on_container_ready(player_id);
          //pnode->send_vpow(*pgame,player_id);
        }
      };
    }
    void start_reading() {
        pnode->loop_v2.add(socket.sock, [this](int fd) {
            char buf[4096];
            int n = socket.read(buf, sizeof(buf));
            if (n > 0) {
                decoder.feed(buf, n);
            } else if (n < 0){
                // Сокет закрыт
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
    bool EL=false;
    bool ok()const{return !TL&&!PF&!EL;}
    string to_str(){if(ok())return "ok";string out=TL?"TL":"";if(PF)out+="PF";if(EL)out+="EL";return out;}
  };
  struct t_runned_game{
    t_game_decl gd;
    int tick=0;
    //pid_t runner_pid = 0;
    //string runner_socket_path;
    //unique_ptr<t_docker_api_v2> runner;
    unique_ptr<t_qaplr_process> qaplr;
    vector<unique_ptr<t_docker_api_v2>> slot2api;
    vector<string> slot2cmd;
    vector<t_status> slot2status;
    vector<int> slot2ready;
    vector<string> slot2bin;
    vector<vector<string>> tick2cmds;
    //vector<emitter_on_data_decoder> slot2eodd;
    vector<t_player_packet_decoder> slot2decoder;
    string cdn_upload_token=UPLOAD_TOKEN;
    //t_world w;
    t_world world;
    t_node*pnode=nullptr;
    int num_containers_ready = 0;
    bool qaplr_launched = false;
    void on_container_ready(int player_id) {
      if (slot2status[player_id].ok()) {
        ++num_containers_ready;
        LOG("t_runned_game::container " + to_string(player_id) + " ready (" + to_string(num_containers_ready) + "/" + to_string(gd.arr.size()) + ")");
        if (num_containers_ready == (int)gd.arr.size() && !qaplr_launched) {
          launch_qaplr();
        }
      }
    }
    void launch_qaplr() {
      if (qaplr_launched) return;
      qaplr_launched = true;
      qaplr=make_unique<t_qaplr_process>();
      qaplr->pnode=pnode;
      qaplr->pgame=this;
      if(!qaplr->start(gd)){
        LOG("t_node::qaplr spawn failed, aborting game " + to_string(gd.game_id));
        lock_guard<mutex> lock(pnode->rgarr_mutex);
        QapCleanIf(pnode->rgarr, [&](const auto& ref) {
          return ref->gd.game_id == gd.game_id;
        });
        return;
      }
      LOG("t_runned_game::QapLR launched after all containers ready");
    }
    void init(t_node*pnode){
      this->pnode=pnode;
      slot2cmd.resize(gd.arr.size());
      slot2status.resize(gd.arr.size());
      slot2ready.resize(gd.arr.size());
      slot2bin.resize(gd.arr.size());
      slot2decoder.resize(gd.arr.size());
      init_decoders();
    }
    static string mk_len_packed(const string&msg){
      string out(sizeof(uint32_t),'0');
      uint32_t&len=(uint32_t&)out[0];len=msg.size();
      return out+msg;
    }
    void init_decoders(){
      for (int i = 0; i < gd.arr.size(); ++i) {
        int player_id = i;
        slot2decoder[i].on_packet = [this, player_id](const string& cmd) {
          if (cmd.empty()) {
            // Ошибка: oversized или битый пакет
            if (qap_check_id(player_id, slot2api)) {
              slot2api[player_id]->on_stderr("[t_node] oversized or invalid packet\n");
              pnode->container_monitor.kill(*this, player_id);
              slot2status[player_id].PF = true;
            }
            return;
          }

          // Команда получена полностью!
          // 1. Сохраняем для replay
          if (tick2cmds.size() <= tick) tick2cmds.resize(tick + 1,vector<string>(gd.arr.size()));
          tick2cmds[tick][player_id] = cmd;

          // 2. Останавливаем таймер TL
          for (auto& task : pnode->container_monitor.tasks) {
            if (task.pgame == this && task.player_id == player_id && !task.done) {
              task.on_done(pnode->container_monitor.clock);
              break;
            }
          }
          
          // 3. Пересылаем в QapLR
          if (qaplr) {
            qaplr->write_zchan("p"+to_string(player_id),mk_len_packed(cmd));
          }
        };
      }
    }
    void free(){
      qaplr=nullptr;
      for (auto& api : slot2api) {
        if (api) {
          //t_node::kill(api->conid);
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
        p.coder=gd.arr[i].coder;
        p.v=gd.arr[i].cdn_bin_file;
      }
      out.tick2cmds=tick2cmds;
      return out;
    }
    t_finished_game mk_fg(){
      t_finished_game out;
      out.game_id=gd.game_id;
      out.slot2score=world.slot2score;
      int i=-1;
      for(auto&ex:slot2status){
        i++;
        out.slot2status.push_back(ex.to_str());
        double ms=0;auto&a=*slot2api[i];for(auto&t:a.time_log)ms+=t;
        out.slot2ms.push_back(ms);
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
      (void)system(cmd.c_str());
    });
    t.detach();
  }
  struct t_container_monitor{
    static void kill(t_runned_game&g,int pid){
      auto&conid=g.slot2api[pid]->conid;
      t_node::kill(conid);
      if(g.qaplr){
        g.qaplr->write_zchan("drop/"+to_string(pid),"");
      }
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
        if(deaded)return false;
        auto&gd=pgame->gd;
        auto TL=pgame->tick?gd.TL:gd.TL0;
        if(ms<=TL)return false;
        kill(*pgame,player_id);
        pgame->slot2status[player_id].TL=true;
        deaded=true;
        return true;
      }
    };
    mutex tasks_mtx;
    vector<t_task> tasks;
    t_node*pnode=nullptr;
    QapClock clock;
    void add(t_runned_game*pgame,int player_id) {
      lock_guard<mutex> lock(tasks_mtx);
      QapCleanIf(tasks, [&](const t_task& t) {
        return t.pgame == pgame && t.player_id == player_id;
      });
      tasks.push_back({pgame,player_id,clock.MS()});
    }
    void clear(int game_id){
      lock_guard<mutex> lock(tasks_mtx);
      QapCleanIf(tasks,[&](const t_task&t){return t.pgame->gd.game_id==game_id;});
    }
    void update(){
      lock_guard<mutex> lock(tasks_mtx);
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
    api.conid = "game_" + to_string(game_id) + "_p" + to_string(player_id) + "_" + to_string(unsigned(rand()<<16)+rand());
    //api.socket_path_on_host = "/tmp/dokcon_" + api.conid + ".sock";
    string socketDir = "/tmp/dokcon_sockets";
    api.socket_path_on_host=socketDir+"/dokcon_"+api.conid+".sock";
    api.socket_path_in_container="/tmp/dokcon_"+api.conid+".sock";
    unlink(api.socket_path_on_host.c_str());
    string binary;
    if (!download_binary(cdn_url, binary)) {
      api.on_stderr("download failed\n");
      LOG("spawn_docker::download_binary failed at "+cdn_url);
      return false;
    }
    g.slot2bin[player_id] = binary;
    (void)system(("mkdir -p " + socketDir).c_str());
    string cmd = "docker run -d --rm --memory=512m --memory-swap=512m --network=none --cpus=\"1.0\" "
                 "-e SOCKET_PATH="+api.socket_path_in_container+" "
                 "--name " + api.conid + " "
                 "--mount type=bind,src="+socketDir+",dst=/tmp "
               //"--mount type=bind,src=" + api.socket_path_on_host + ",dst=/tmp/dokcon.sock "
                 "--mount type=tmpfs,tmpfs-size=64m,destination=/tmpfs "
                 "universal-runner:latest";
    LOG("spawn_docker::say\n"+cmd);
    int result = system(cmd.c_str());
    if (result != 0) {
      LOG("spawn_docker::docker run failed: " + to_string(result)+" for "+cdn_url);
      api.on_stderr("docker run failed: " + to_string(result) + "\n");
      return false;
    }
    auto*api_ptr=&api;api.binary=binary;
    bool ok=loop_v2.connect_to_container_socket(api,[this,api_ptr](){
      stream_write(api_ptr->socket, "true", "please delivered this to dokcon.js");
      LOG("loop_v2.connect_to_container_socket::done::bef::start_reading");
      api_ptr->start_reading();
      stream_write(api_ptr->socket, "false", "please don't delivered this to dokcon.js");
    });

    return ok;
  }
  static string config2seed(const string&config){return {};}
  vector<unique_ptr<t_runned_game>> rgarr;
  // Асинхронная отправка
  void t_cdn_api_upload_async(
      const string& path,
      const t_cdn_game& data,
      const string& token,
      function<void(const string& error)> on_done
  ) {
    thread([path, data, token, on_done = move(on_done)]() {
      string error;auto DATA=data;
      try {
        auto s=http_put_with_auth(path, DATA.serialize(),token);
        if(s!=200)error="HTTP error "+to_string(s);
      } catch (const exception& e) {
        error = string("Exception in http_put_with_auth:") + e.what();
      }
      on_done(error);
    }).detach();
  }
  //t_net_api capi;
  void t_main_api_send(const string& z, const t_finished_game& ref) {
    //string payload=qap_zchan_write("game_finished:"+UPLOAD_TOKEN,serialize(ref));
    //capi.write_to_socket(local_main_ip_port,payload);
    swd->try_write("game_finished:"+UPLOAD_TOKEN,serialize(ref));
  }
  void t_main_api_send(const string& z, const t_game_uploaded_ack& ref) {
    //string payload=qap_zchan_write("game_uploaded:"+UPLOAD_TOKEN,serialize(ref));
    //capi.write_to_socket(local_main_ip_port,payload);
    swd->try_write("game_uploaded:"+UPLOAD_TOKEN,serialize(ref));
  }
  int game_n=0;mutex rgarr_mutex;
  bool new_game(const t_game_decl&gd){
    t_runned_game*pgame=nullptr;
    {lock_guard<mutex> lock(rgarr_mutex);auto&gu=qap_add_back(rgarr);gu=make_unique<t_runned_game>();pgame=gu.get();}
    auto&g=*pgame;
    g.gd=gd;
    g.init(this);
    /*
    g.qaplr=make_unique<t_qaplr_process>();
    g.qaplr->pnode=this;
    g.qaplr->pgame=&g;
    if(!g.qaplr->start(gd)){
      LOG("t_node::qaplr spawn failed, aborting game " + to_string(gd.game_id));
      lock_guard<mutex> lock(rgarr_mutex);
      QapCleanIf(rgarr, [&](const auto& ref) {
        return ref->gd.game_id == gd.game_id;
      });
      return false;
    }*/
    int i=-1;
    for(auto&ex:gd.arr){
      i++;
      auto&b2=qap_add_back(g.slot2api);
      b2=make_unique<t_docker_api_v2>();
      t_docker_api_v2&v2=*b2.get();
      v2.pnode=this;
      v2.player_id=i;
      v2.pgame=&g;
      bool ok=spawn_docker(CDN_URL+"/"+ex.cdn_bin_file,v2,gd.game_id,i,g);
      if(!ok){
        for(auto&api:g.slot2api){
          kill(api->conid);
        }
        LOG("t_node::game("+to_string(gd.game_id)+") aborted due to spawn_docker error:"+v2.conid);
        swd->try_write("game_aborted:"+UPLOAD_TOKEN,to_string(gd.game_id)+","+to_string(i));
        lock_guard<mutex> lock(rgarr_mutex);
        QapCleanIf(rgarr,[&](auto&ref){return ref->gd.game_id==gd.game_id;});
        return false;
      }
    }
    game_n++;
    return true;
  }
  void send_game_result(t_runned_game& game) {
    auto game_id = game.gd.game_id;
    auto finished_game = game.mk_fg();
    auto cdn_game = game.mk_cg();
    auto token = game.cdn_upload_token;
    finished_game.size=cdn_game.size();
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
  void on_qaplr_finished(t_runned_game& g) {
    LOG("t_node::QapLR finished for game " + to_string(g.gd.game_id));
    for (int i = 0; i < (int)g.slot2api.size(); ++i) {
      if (g.slot2status[i].ok()) {
          container_monitor.kill(g, i);
      }
    }
    container_monitor.clear(g.gd.game_id);
    send_game_result(g);
  }
  void on_player_stdout(t_runned_game&g,int player_id,const string_view&data){
    if (!qap_check_id(player_id, g.slot2decoder)) return;
    g.slot2decoder[player_id].feed(data.data(), data.size());
  }
  struct t_event_loop_v2{
    struct t_monitored_fd {
      int fd;
      //string path;  // для Unix-socket
      function<void(int&fd)> on_ready;
      //function<void()> on_error;
      //bool connected = false;
    };
    vector<t_monitored_fd> fds,fds2;
    set<int> fdsr;
    mutex mtx,mtx2,mtxr;
    t_node*pnode=nullptr;
    void remove_without_lock(int fd) {
      QapCleanIf(fds, [fd](const t_monitored_fd& f) { return f.fd == fd; });
      LOG("t_event_loop_v2::remove_without_lock fd="+to_string(fd));
    }
    void remove(int fd) {
      lock_guard<mutex> lock(mtxr);
      fdsr.insert(fd);
      LOG("t_event_loop_v2::remove fd="+to_string(fd));
      //remove_without_lock(fd);
    }
    void add(int fd, function<void(int&)>&&on_ready/*, const function<void()>& on_error = []{}*/) {
      lock_guard<mutex> lock(mtx2);
      fds2.push_back({fd, std::move(on_ready)/*, on_error*/});
      LOG("t_event_loop_v2::add fd="+to_string(fd));
    }
    void wait_for_socket(const std::string& socket_path, int max_wait_ms=1024, int poll_interval_ms=16) {
        namespace fs = std::filesystem;
        int waited = 0;
        while (!fs::exists(socket_path) && waited < max_wait_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
            waited += poll_interval_ms;
        }
        if (!fs::exists(socket_path)) {
          LOG("t_node::Socket file not created after waiting: "+socket_path);
        }
    }
    bool connect_to_container_socket(t_docker_api_v2&api,function<void()>&&on_connect) {
      wait_for_socket(api.socket_path_on_host);
      auto&client=api.socket;
      if (!client.connect_unix(api.socket_path_on_host)) {
        LOG("t_node::client.connect_unix(path) failed with "+api.socket_path_on_host); 
        return false;
      }

      auto wrapper = [this,&client,on_connect](int&fd){
        if (client.connected)return;
        int err = 0;
        socklen_t len = sizeof(err);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
          client.qap_close();LOG("t_node::connect_to_container_socket::wrapper failed"); 
          return;
        }
        client.connected = true;
        on_connect();
        fd=-1;
      };

      add(client.sock,move(wrapper));
      return true;
    }

    void run() {
      vector<pollfd> pfds;auto time=qap_time();//QapClock clock;
      while (true) {
        pfds.clear();
        {
          lock_guard<mutex> lock(mtx);
          {
            lock_guard<mutex> lock(mtx2);
            for(auto&f:fds2)fds.push_back(f);
            fds2.clear();
          }
          {
            lock_guard<mutex> lock(mtxr);
            QapCleanIf(fds,[this](const t_monitored_fd&f){return -1==f.fd||fdsr.count(f.fd);});
            fdsr.clear();
          }
          for (auto& f : fds) {
            pfds.push_back(make_pollinout(f.fd));
          }
          auto cur=qap_time();
          if(qap_time_diff(time,cur)>2500/*clock.MS()>2500*/){
            LOG("pfds.size()=="+to_string(pfds.size()));
            //clock.Start();
            time=cur;
          }
        }
        pnode->container_monitor.update();
        if(pfds.empty()){this_thread::sleep_for(16ms);}
        int n = poll(pfds.data(), pfds.size(), 1);  // 1ms таймаут — нормально
        if (n <= 0) continue;

        {
          lock_guard<mutex> lock(mtx);
          for (auto& pfd : pfds) {
            if (pfd.revents & (POLLERR | POLLHUP)) {
              for (auto& f : fds) {
                if (f.fd == pfd.fd) {
                  //if (f.on_error) f.on_error();
                  qap_close(pfd.fd);
                  remove_without_lock(pfd.fd);  // удаляем из списка
                  LOG("t_node::Socket error on fd="+to_string(pfd.fd));
                  break;
                }
              }
              continue;
            }
            if (pfd.revents & POLLIN) {
              // data ready
              for (auto& f : fds) {
                if (f.fd == pfd.fd) {
                  f.on_ready(f.fd);
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
    thread t([this]{
      while (true) {
        this_thread::sleep_for(1h);
        lru.cleanup();
      }
    });
    t.detach();
  }/*
  void periodic_ping(){
    thread t([this]{
      while (true) {
        this_thread::sleep_for(10s);
        capi.write_to_socket(local_main_ip_port,qap_zchan_write("ping:"+UPLOAD_TOKEN,qap_time()));
      }
    });
    t.detach();
  }*/
  unique_ptr<SocketWithDecoder> swd;
  string unique_token;
  int main(){
    static auto cores=to_string(thread::hardware_concurrency()*32);
    atomic<bool> pong_received{false};
    auto cb=[&](const string&z,const string&payload){
      if(z=="hi"){
        if(unique_token.empty())unique_token=payload;
        bool ok=swd->try_write("node_up:"+UPLOAD_TOKEN,cores+","+unique_token);
        LOG("t_node::node_up result:"+string(ok?"true":"false"));
        //if(!ok)return;
        //swd->reconnect();
      }
      if(z=="new_game:"+UPLOAD_TOKEN){
        LOG("t_node::new_game bef");
        new_game(parse<t_game_decl>(payload));
        LOG("t_node::new_game aft");
      }
      if (z == "pong") {
        pong_received = true;
      }
    };
    swd=make_unique<SocketWithDecoder>(local_main_ip_port,std::move(cb),true);
    swd->begin();
    thread([this,&pong_received]{
      while(true){
        pong_received = false;
        bool sent = swd->try_write("ping:" + UPLOAD_TOKEN, qap_time());
        if (!sent) {
          LOG("t_node::ping send failed");
          this_thread::sleep_for(250ms);
          swd->reconnect();
          continue;
        }
        this_thread::sleep_for(2s); // ждём ответ
        if (!pong_received) {
          LOG("t_node::pong not received — reconnecting");
          swd->reconnect();
        }
        this_thread::sleep_for(1s);
      }
    }).detach();
    periodic_cleanup();
    //periodic_ping();

    // new_game(...) — где-то вызывается

    //loop.run(this);
    //auto cores=to_string(thread::hardware_concurrency());
    //capi.write_to_socket(local_main_ip_port,qap_zchan_write("node_up:"+UPLOAD_TOKEN,cores));
    loop_v2.pnode=this;
    loop_v2.run();
    return 0;
  }
};
