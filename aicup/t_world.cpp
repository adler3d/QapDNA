//2025.08.12 16:25:07.104 -> 2025.08.16 20:15:09.105
struct t_world{
  struct t_machine{
    vec2d pos;
    string name;
    int hp=100;
    int maxhp=100;
  };
  struct t_player{
    string coder;
    string app;
  };
  struct t_game{
    vector<t_player> players;
    string admin;
    string config;
    int tick=0;
    int maxtick=20000;
  };
  struct t_process{
    string machine;
    string name;
    string main;
  };
  struct t_node:t_process{
    vector<t_game> games;
  };
  struct t_elf{
    string coder;
    string src;
    string dest;
    string stderr;
    string status;//TL,ML,OK
    int tick=0;
    int max_tick=rand()%100;
    int mem=0;
  };
  struct t_compiler:t_process{
    vector<t_elf> elfs;
  };
  struct t_proxy:t_process{
    string admin_or_proxy;
    vector<string> to_proxys;
    vector<string> to_fronts;
  };
  struct t_admin:t_process{
    string proxy;
    string to_cdn_file;
  };
  struct t_coder_rec{
    string name;
    string client;
    string token;
    vector<string> sources;
    int total_games=0;
  };
  struct t_game_rec{
    struct t_slot{string coder;double score=0;string stderr;string status;};
    vector<t_slot> arr;
    string cdn_file;
    int tick=0;
  };
  struct t_main:t_process{
    vector<t_coder_rec> carr;
    vector<t_game_rec> garr;
  };
  struct t_site:t_process{
    vector<string> fronts;
    string cdn_src_file;
  };
  struct t_file{
    string name;
    string content;
  };
  struct t_cdn:t_process{
    vector<t_file> files;
  };
  struct t_front{
    string main;
    string coder_or_anon;
    string proxy;
    string site;
    string cdn;
    string cdn_src_file;
  };
  struct t_provider{
    string name;
    vector<t_machine> arr;
    double price_per_node_hr=7.5;
  };
  struct t_client{
    t_game{
      string token;
      string node;
    };
    string coder;
    string ctoken;
    bool remote=true;
    vector<t_game> games;
    vector<string> sources;
  };
  struct t_coder{
    string name;
    bool sponsor=true;
    double money=1000;
    int servers=5;
  };
  struct t_supervisor:t_process{
    struct t_event{string source;string type;vector<string> params;};
    vector<t_event> events;
  };
  //sensors/actuators/users/user_side_programs
  t_habr habr;//единственное доступное СМИ?
  t_adler adler;// типа я
  t_github github;
  vector<t_coder> coders;
  vector<t_client> clients;
  vector<t_provider> providers;
  vector<t_front> fronts;
  //processes
  t_supervisor sv;
  vector<t_cdn> cdns;
  vector<t_site> sites;
  vector<t_main> mains;
  vector<t_admin> admins;
  vector<t_proxy> proxys;
  vector<t_compiler> carr;
  vector<t_node> nodes;
  //impl
  struct t_cmd{
  };
  typedef vector<t_cmd> t_cmds;
  void use(const t_cmds&cmd){
    // только запись в буфферы
  }
  void step(){
    // только логика основанная на чтении буфферов и обновлении состояния мира.
  }
};
