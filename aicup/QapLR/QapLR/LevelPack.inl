#define LEVEL_LIST(F)F(Level_MarketGame);
#define FRAMESCOPE(F)\
  F(Dot,"dot",2)\
  F(MenuItem,"MenuItem",0)\
  F(Market,"market_128",0)\
  F(Enemy,"enemy_v2_128",0)\
  F(Obstacle,"obstacle_v4_256",0)\
  F(RepairDock,"repair_dock_256",0)\
  F(FuelStation,"fuel_station_256",0)\
  F(BlackMarket,"black_market_256",0)\
  F(Workshop,"workshop_256",0)\
  //---
//it is only a draft
static bool CD_CircleAndWall(const vec2d&pos,vec2d&v,const real&r,const vec2d&a,const vec2d&b){
  if(fabs((pos-a).Rot(b-a).y)>r)return false;
  if((pos-a).Rot(b-a).x<0)return false;
  if((pos-a).Rot(b-a).x>(b-a).Mag())return false;
  if(Sign(v.Rot(b-a).y)==Sign((pos-a).Rot(b-a).y))return false;
  v=vec2d(v.Rot(b-a).x,-v.Rot(b-a).y).UnRot(b-a);return true;
}

//it is only a draft
static bool CD_CircleAndWallCircle(const vec2d&pos,vec2d&v,const real&d,const vec2d&a){
  if((pos-a).SqrMag()>d*d)return false;
  if(v.Rot(pos-a).x>0)return false;
  v=vec2d(-v.Rot(pos-a).x,v.Rot(pos-a).y).UnRot(pos-a);
  return true;
}
static bool CD_LineVsCircle(const vec2d&pos,const vec2d&a,const vec2d&b,const real&d){
  vec2d v;
  if(CD_CircleAndWallCircle(pos,v,d,a))return true;
  if(CD_CircleAndWallCircle(pos,v,d,b))return true;
  if(CD_CircleAndWall(pos,v,d,a,b))return true;
  return false;
}
class Level_MarketGame:public TGame::ILevel{
public:
  struct t_item{
    int id=0;
    int amount=0;
    int price=0;
    bool is_black=false;
    //std::string name; // уникальные имена для black market
  };
  struct t_market{
    vec2d pos;
    vector<t_item> items;
  };
  struct t_black_market{
    vec2d pos;
    real r=50;
    real ang=0;
    vector<t_item> items;
  };
  struct t_workshop{
    vec2d pos;
    real r=40;
    real ang=0;
    int upgrade_level=0;
    int upgrade_cost = 300;   // первый апгрейд дешевле
    int fuel_upgrade=0;
    int fuel_upgrade_cost = 200;
    int armor_upgrade=0;
    int armor_upgrade_cost = 180;
    int eco_upgrade=0;
    int eco_upgrade_cost = 250;
    int nitro_upgrade=0;
    int nitro_upgrade_cost = 300;
    int suspension_upgrade=0;
    int suspension_upgrade_cost = 180;
  };
  struct t_city{
    vector<t_market> arr;
    vector<t_black_market> black_markets;
    vector<t_workshop> workshops;
  };
  struct t_bookmark{
    vector<t_item> items;
  };
  struct t_cargo_item{
    int id=0;
    int amount=0;
    bool is_black=false;
  };
  struct t_cargo{
    vector<t_cargo_item> items;
    vector<t_cargo_item> black_items;
  };
  struct t_car{
    t_cargo cargo;
    real money=1000;
    vec2d pos;
    vec2d v;
    bool deaded=false;
    real hp=100;
    real max_hp=100;
    real fuel=600*4;
    real max_fuel=600*4;
    int speed_upgrade=0;
    int fuel_upgrade=0;
    int eco_upgrade=0;
    int armor_upgrade=0;
    int nitro_upgrade=0;
    int suspension_upgrade=0;
    int nitro_ticks=0;
  };

  struct t_dynamic_obstacle{
    vec2d pos;
    real ang=0;
    real len=100;
    real speed=1;
    real r=8;
    bool circle=false;
    real dt=0;
    real gang=0;
    real gspd=0;
    real hp=5 + rand()%10;
  };

  struct t_repair_dock{
    vec2d pos;
    real ang=0;
    real r=32;
  };
  struct t_fuel_station{
    vec2d pos;
    real ang=0;
    real r=70;
    real price_per_unit=0.7;
  };
  enum e_surface_type { SURFACE_NORMAL, SURFACE_MUD, SURFACE_ICE, SURFACE_SAND, SURFACE_ROAD, SURFACE_GRAVEL };

  enum e_disaster_type { DISASTER_NONE=0, DISASTER_FIRE=1, DISASTER_BLOCK=2 };

  struct t_road_tile {
    vec2d pos;
    real r;
    e_surface_type type = SURFACE_NORMAL;
  };

  struct t_disaster {
    e_disaster_type type=DISASTER_NONE;
    int ticks=0;
    int market_id=-1;
    vec2d pos;
  };

  struct t_world{
    int t=0;
    real obstacle_r=48;
    real tank_r=48;
    real market_r=32;
    t_car car;
    t_city city;
    vector<t_dynamic_obstacle> dyn_obs;
    vector<vec2d> obstacles;
    vector<t_repair_dock> rdarr;
    vector<t_fuel_station> fsarr;
    vector<t_road_tile> road_tiles;
    t_disaster disaster;
  };

  // ====== BLACK MARKET LOGIC ======
  int get_black_market_id(vec2d pos,bool ignore_r){
    int best_id = -1; real best = 1e30;
    for(int i=0;i<w.city.black_markets.size();i++){
      auto& bm = w.city.black_markets[i];
      auto d = (pos-bm.pos).Mag();
      if(best_id<0 || d<best){best_id=i;best=d;}
    }
    if(best_id<0)return -1;
    if(!ignore_r && (pos-w.city.black_markets[best_id].pos).Mag()>w.city.black_markets[best_id].r) return -1;
    return best_id;
  }

  bool buy_black(t_world&w,int black_market_id,int item_idx,int amount){
    if(black_market_id<0) return false;
    if(item_idx<0 || item_idx>=w.city.black_markets[black_market_id].items.size()) return false;
    auto& it = w.city.black_markets[black_market_id].items[item_idx];
    if(it.amount<amount) return false;
    int cost = it.price*amount;
    if(w.car.money<cost) return false;
    it.amount-=amount;
    w.car.money-=cost;
    // Найти/добавить в черный инвентарь
    auto& arr = w.car.cargo.black_items;
    auto itb = std::find_if(arr.begin(),arr.end(),[&](const t_cargo_item& e){return e.id==it.id;});
    if(itb==arr.end()){
      arr.push_back({it.id,amount,true});
    }else{
      itb->amount += amount;
    }
    return true;
  }
  bool sell_black(t_world&w,int black_market_id,int item_idx,int amount){
    if(black_market_id<0) return false;
    if(item_idx<0 || item_idx>=w.city.black_markets[black_market_id].items.size()) return false;
    auto& it = w.city.black_markets[black_market_id].items[item_idx];
    auto& arr = w.car.cargo.black_items;
    auto itb = std::find_if(arr.begin(),arr.end(),[&](const t_cargo_item& e){return e.id==it.id;});
    if(itb==arr.end() || itb->amount<amount) return false;
    itb->amount-=amount;
    if(itb->amount==0) arr.erase(itb);
    it.amount+=amount;
    w.car.money+=it.price*amount;
    return true;
  }
  
  // ==== WORKSHOP LOGIC ====
  int get_workshop_id(vec2d pos,bool ignore_r){
    int best_id = -1; real best = 1e30;
    for(int i=0;i<w.city.workshops.size();i++){
      auto& ws = w.city.workshops[i];
      auto d = (pos-ws.pos).Mag();
      if(best_id<0 || d<best){best_id=i;best=d;}
    }
    if(best_id<0)return -1;
    if(!ignore_r && (pos-w.city.workshops[best_id].pos).Mag()>w.city.workshops[best_id].r) return -1;
    return best_id;
  }
  bool can_upgrade_speed(){
    int wsid = get_workshop_id(w.car.pos,false);
    if(wsid<0) return false;
    auto& ws = w.city.workshops[wsid];
    return w.car.money>=ws.upgrade_cost;
  }
  bool do_upgrade_speed(){
    int wsid = get_workshop_id(w.car.pos,false);
    if(wsid<0) return false;
    auto& ws = w.city.workshops[wsid];
    if(w.car.money<ws.upgrade_cost) return false;
    w.car.money-=ws.upgrade_cost;
    w.car.speed_upgrade++;
    ws.upgrade_level++;
    ws.upgrade_cost = int(ws.upgrade_cost*1.7)+100;
    return true;
  }

  // ==== BASE GAME ====
  bool sell(t_world&w, int market_id, const t_cargo_item&item) {
    if(w.disaster.type==DISASTER_BLOCK && market_id==w.disaster.market_id) return false;
    auto&cit=w.car.cargo.items[item.id];
    if(cit.amount < item.amount) return false;
    cit.amount -= item.amount;
    auto&it=w.city.arr[market_id].items[item.id];
    it.amount += item.amount;
    w.car.money += it.price *item.amount;
    return true;
  }
  bool buy(t_world&w,int market_id,const t_cargo_item&item){
    if(w.disaster.type==DISASTER_BLOCK && market_id==w.disaster.market_id) return false;
    auto&it=w.city.arr[market_id].items[item.id];
    if(it.amount<item.amount)return false;
    auto dm=it.price*item.amount;
    if(w.car.money<dm)return false;
    it.amount-=item.amount;
    w.car.cargo.items[item.id].amount+=item.amount;
    w.car.money-=dm;
    return true;
  }
public:
  t_world w;
public:
  TGame*Game=nullptr;
public:

  // --- Black market names ---
  vector<string> black_item_names = {
    "Plutonium", "StealthChip", "GhostFuel", "NanoCortex", "Wormhole Pass",
    "Synthetic Gold", "Vortex Key", "Dark Crystal", "Quantum Oil"
  };
  std::string bid2str(int id) {
    if(id>=0 && id<int(black_item_names.size())) return black_item_names[id];
    return "Special"+IToS(id+1);
  }
  // --- Utility for collision check: point in circle (with explicit r1 + r2) ---
  bool point_in_circle(const vec2d& pos, const vec2d& center, double r) {
    return (pos - center).SqrMag() < r*r;
  }

  // --- Black market init (with obstacle/market/workshop/rd/fs overlap avoidance) ---
  void init_black_market() {
    w.city.black_markets.clear();
    int n = 2 + rand()%3;
    for(int i=0; i<n; i++){
      t_black_market bm;
      bool ignore = false;
      int try_count = 0;
      for(; try_count < 2000; ++try_count){
        ignore = false;
        bm.pos = vec2d(rand()%1600-800,rand()%1200-600);
        for(auto& ex : w.obstacles){
          if(point_in_circle(bm.pos, ex, bm.r + w.obstacle_r)) { ignore = true; break; }
        }
        for(auto& ex : w.city.arr){
          if(point_in_circle(bm.pos, ex.pos, bm.r + w.market_r)) { ignore = true; break; }
        }
        for(auto& ex : w.rdarr){
          if(point_in_circle(bm.pos, ex.pos, bm.r + ex.r)) { ignore = true; break; }
        }
        for(auto& ex : w.fsarr){
          if(point_in_circle(bm.pos, ex.pos, bm.r + ex.r)) { ignore = true; break; }
        }
        for(auto& ex : w.city.workshops){
          if(point_in_circle(bm.pos, ex.pos, bm.r + ex.r)) { ignore = true; break; }
        }
        for(auto& ex : w.city.black_markets){
          if(point_in_circle(bm.pos, ex.pos, (bm.r + ex.r)*4)) { ignore = true; break; }
        }
        if(!ignore) break;
      }
      if(ignore) continue;
      int n_items = 2 + rand()%3;
      for(int j=0;j<n_items;j++){
        t_item it;
        it.id = j;
        it.price = 250 + rand()%800;
        it.amount = 50 + rand()%100;
        it.is_black = true;
        bm.items.push_back(it);
      }
      bm.ang=(rand()%(314*2))/100.0;
      w.city.black_markets.push_back(bm);
    }
  }

  // --- Workshops (master) init with collision avoidance ---
  void init_workshops(){
    int n = 1 + rand()%2;
    for(int i=0; i<n; i++){
      t_workshop ws;
      bool ignore = false;
      int try_count = 0;
      for(; try_count < 2000; ++try_count){
        ignore = false;
        ws.pos = vec2d(rand()%1600-800,rand()%1200-600);
        for(auto& ex : w.obstacles){
          if(point_in_circle(ws.pos, ex, ws.r + w.obstacle_r)) { ignore = true; break; }
        }
        for(auto& ex : w.city.arr){
          if(point_in_circle(ws.pos, ex.pos, ws.r + w.market_r)) { ignore = true; break; }
        }
        for(auto& ex : w.rdarr){
          if(point_in_circle(ws.pos, ex.pos, ws.r + ex.r)) { ignore = true; break; }
        }
        for(auto& ex : w.fsarr){
          if(point_in_circle(ws.pos, ex.pos, ws.r + ex.r)) { ignore = true; break; }
        }
        for(auto& ex : w.city.black_markets){
          if(point_in_circle(ws.pos, ex.pos, ws.r + ex.r)) { ignore = true; break; }
        }
        for(auto& ex : w.city.workshops){
          if(point_in_circle(ws.pos, ex.pos, (ws.r + ws.r)*4)) { ignore = true; break; }
        }
        if(!ignore) break;
      }
      if(ignore) continue;
      ws.ang=(rand()%(314*2))/100.0;
      ws.upgrade_level = 0;
      ws.upgrade_cost = 600;
      w.city.workshops.push_back(ws);
    }
  }
  void init_road_tiles() {
    w.road_tiles.clear();
    for(int i=0; i<15; i++) {
      t_road_tile tile;
      tile.pos = vec2d(rand()%1800-900,rand()%1200-600);
      tile.r = 180 + rand()%80;
      tile.type = (e_surface_type)(rand()%6);
      w.road_tiles.push_back(tile);
    }
  }
  // --- Fuel stations init with collision avoidance ---
  void init_fuel_stations() {
    w.fsarr.clear();
    for(int i=0; i<3; i++){
      t_fuel_station fs;
      bool ignore = false;
      int try_count = 0;
      for(; try_count < 2000; ++try_count){
        ignore = false;
        fs.pos = vec2d(rand()%1600-800,rand()%1200-600);
        for(auto& ex : w.obstacles){
          if(point_in_circle(fs.pos, ex, fs.r + w.obstacle_r)) { ignore = true; break; }
        }
        for(auto& ex : w.city.arr){
          if(point_in_circle(fs.pos, ex.pos, fs.r + w.market_r)) { ignore = true; break; }
        }
        for(auto& ex : w.rdarr){
          if(point_in_circle(fs.pos, ex.pos, fs.r + ex.r)) { ignore = true; break; }
        }
        for(auto& ex : w.city.black_markets){
          if(point_in_circle(fs.pos, ex.pos, fs.r + ex.r)) { ignore = true; break; }
        }
        for(auto& ex : w.city.workshops){
          if(point_in_circle(fs.pos, ex.pos, fs.r + ex.r)) { ignore = true; break; }
        }
        for(auto& ex : w.fsarr){
          if(point_in_circle(fs.pos, ex.pos, fs.r + ex.r)) { ignore = true; break; }
        }
        if(!ignore) break;
      }
      if(ignore) continue;
      fs.ang=(rand()%(314*2))/100.0;
      w.fsarr.push_back(fs);
    }
  }

  // --- Вызов в init_city в правильном порядке ---
  bool init_city(){
    vector<int> base_price;
    base_price.resize(5);
    for(int i=0;i<5;i++){
      base_price[i]=25+rand()%100+pow(3,i+1);
    }
    int try_count=0;
    // --- Markets ---
    for(int i=0;i<10;i++){
      t_market m;
      m.pos=vec2d(rand()%1000-500,rand()%1000-500);//if(i==0)m.pos={};
      bool ignore=false;
      for(auto& ex:w.city.arr){
        if((ex.pos-m.pos).Mag()<w.market_r*10)ignore=true;
      }
      if(ignore){i--;try_count++;if(try_count>2000)return false;continue;}
      m.items.resize(5);
      for(int i=0;i<5;i++){m.items[i].id=i;m.items[i].price=base_price[i]+rand()%(50+int(pow(3,i+1)));m.items[i].amount=50+rand()%200;}
      w.city.arr.push_back(m);
    }
    init_obstacles();
    // --- Repair docks --
    for(int i=0;i<3;i++){
      t_repair_dock d;
      d.pos=vec2d(rand()%1600-800,rand()%1200-600);
      bool ignore=false;
      for(auto& ex:w.city.arr){
        if((ex.pos-d.pos).Mag()<w.market_r*5)ignore=true;
      }
      for(auto& ex:w.rdarr){
        if((ex.pos-d.pos).Mag()<w.market_r*5)ignore=true;
      }
      for(auto& ex:w.obstacles){
        if(point_in_circle(d.pos, ex, w.market_r + w.obstacle_r)) ignore=true;
      }
      if(ignore){i--;try_count++;if(try_count>2000)return false;continue;}
      w.rdarr.push_back(d);
    }
    // --- Black markets (after obstacles/markets/rdarr) ---
    init_black_market();
    // --- Workshops (after obstacles/markets/rdarr/black markets) ---
    init_workshops();
    // --- Fuel stations (after everything else) ---
    init_fuel_stations();
    return true;
  }
  void init_cargo_items(){
    w.car.cargo.items.resize(5);
    for(int i=0;i<5;i++)w.car.cargo.items[i].id=i;
    w.car.cargo.black_items.clear();
  }
  void init_obstacles(){
    for(int i=0;i<20;i++){
      vec2d p=vec2d(rand()%1200-600,rand()%1000-500);
      if((p-w.car.pos).Mag()<w.obstacle_r+w.tank_r)continue;
      bool ignore=false;
      for(auto&ex:w.city.arr){
        if((ex.pos-p).Mag()<w.obstacle_r+w.market_r)ignore=true;
      }
      if(ignore)continue;
      w.obstacles.push_back(p);
    }
  }
  bool init_dyn_obs_v2(){
    vector<vec2d> PA;
    for(auto&ex:w.city.arr){PA.push_back(ex.pos);}
    auto EA=get_voronoi_edges(PA);edges=EA;
    int try_count=0;
    for(int i=0;i<EA.size();i++){
      for(int j=0;j<3;j++){
        auto&it=EA[i];
        real k=(rand()%1000)/1000.0-0.5;
        vec2d p=((it.a+it.b)*0.5+(it.b-it.a)*k);
        real r=18;
        t_dynamic_obstacle ex;
        ex.circle=false;
        ex.pos=p;
        ex.ang=(rand()%(314*2))/100.0;
        ex.len=ex.circle?rand()%100+w.tank_r*2+r:rand()%500+200;
        ex.speed=(rand()%1000)/1000.0+1;
        ex.r=r;
        ex.gang=(rand()%1000)*Pi*2/1000.0;
        ex.gspd=(rand()%2000)/1000.0-1;
        ex.dt=(rand()%32000)*3.14*2*40/32000;
        ex.hp=5+rand()%10;
        bool ignore=is_dangerous(ex,w.car.pos,w.tank_r*1.5);
        for(auto&market:w.city.arr){
          if(ignore)break;
          if(is_dangerous(ex,market.pos,w.tank_r*1.5))ignore=true;
        }
        for(auto&d:w.rdarr){
          if(ignore)break;
          if(is_dangerous(ex,d.pos,w.tank_r*1.5))ignore=true;
        }
        for(auto&fs:w.fsarr){
          if(ignore)break;
          if(is_dangerous(ex,fs.pos,fs.r*1.5))ignore=true;
        }
        if(ignore){j--;try_count++;if(try_count>2000){j++;bad_edges.push_back(EA[i]);return false;}continue;}
        w.dyn_obs.push_back(ex);
      }
    }
    return bad_edges.empty();
  }
  void init_dyn_obs(){
    int try_count=0;
    for(int i=0;i<7+3+4;i++){
      vec2d p=vec2d(rand()%500-250,rand()%500-250);
      real r=rand()%8+10;
      t_dynamic_obstacle ex;
      ex.circle=i%2;
      ex.pos=p;
      ex.ang=(rand()%(314*2))/100.0;
      ex.len=ex.circle?rand()%500+w.tank_r*2+r:rand()%500+200;
      ex.speed=(rand()%1000)/1000.0+1;
      ex.r=r;
      ex.dt=rand();
      ex.hp=5+rand()%10;
      bool ignore=is_dangerous(ex,w.car.pos,w.tank_r*1.5);
      for(auto&market:w.city.arr){
        if(is_dangerous(ex,market.pos,w.tank_r*1.5))ignore=true;
      }
      for(auto&fs:w.fsarr){
        if(is_dangerous(ex,fs.pos,fs.r*1.5))ignore=true;
      }
      if(ignore){i--;try_count++;if(try_count>1000)i++;continue;}
      w.dyn_obs.push_back(ex);
    }
  }
  t_world world_at_begin;
  void reinit_the_same_level(){
    w=world_at_begin;
    Game->ReloadWinFail();
  }
  bool init_attempt(){
    srand(seed=int(g_clock.MS()));
    bad_edges.clear();
    w={};
    if(!init_city())return false;
    init_cargo_items();
    if(!init_dyn_obs_v2())return false;
    init_road_tiles();
    world_at_begin=w;
    return true;
  }
  int init_attempts=1;
  bool inited=false;
  e_surface_type get_surface_type(const vec2d&pos) {
    e_surface_type res = SURFACE_NORMAL;
    real best_r = 1e30;
    for(auto&tile:w.road_tiles){
      real d = (pos-tile.pos).Mag();
      if(d<tile.r && tile.r<best_r) { res = tile.type; best_r = tile.r; }
    }
    return res;
  }
  struct t_rec{
    int place;
    string user;
    real sec;
    string date;
    int seed;
    string game;
    vector<string> to_str()const{
      return {IToS(place),user,FToS2(sec),date,IToS(seed)};
    }
  };
  vector<t_rec> tops;
  string ref=file_get_contents("ref.txt")==""?"unk":"home";
  string game_version="market_v4";
  void reinit_top20(){
    tops={};
    TGame::wget(g_host,"/c/game_players_table.js?unique&csv&game="+game_version+"&n=15&ref="+ref+"&user="+Game->user_name,[&](const string&url,int p,int size){
      string s;
      s.resize(size);
      for(int i=0;i<size;i++)s[i]=((char*)p)[i];
      auto arr=split(s,"\n");
      if(arr.size())QapPopFront(arr);
      for(auto&ex:arr){
        auto t=split(ex,",");
        QapAssert(t.size()>=6);
        t_rec r;
        r.place=stoi(t[0]);
        r.user=t[1];
        r.sec=stof(t[2]);
        r.date=t[3];
        r.seed=stoi(t[4]);
        r.game=t[5];
        tops.push_back(r);
      }
    });
  }
  void Init(TGame*Game){
    this->Game=Game;
    inited=init_attempt();
    if(!inited){Sys.UPS_enabled=false;}
    reinit_top20();
  }
  bool is_dangerous(const t_dynamic_obstacle&ex,const vec2d&pos,real r){
    vec2d hv=Vec2dEx(ex.ang,ex.len);
    return CD_LineVsCircle(pos,ex.pos-hv,ex.pos+hv,ex.r+r);
    //for(int i=0;i<Sys.UPS*20;i++){
    //  if((dyn_pos(ex,i)-pos).Mag()<ex.r+r)return true;
    //}
    //return false;
  }
  int required_money=90000;
  bool Win(){return w.car.money>required_money;}
  bool Fail(){return w.car.deaded;}
public:
  struct t_edge{
    vec2d a,b;
  };
  vector<t_edge> edges,bad_edges;
  static vector<t_edge> get_voronoi_edges(vector<vec2d>&points){
    double eps=1e-12;
    using value_type=double;
    struct t_hacked_vec2d:vec2d{
      t_hacked_vec2d():vec2d(){}
      t_hacked_vec2d(real x,real y):vec2d(x,y){}
      bool operator<(const t_hacked_vec2d&p)const{return std::tie(x,y)<std::tie(p.x,p.y);}
    };
    using point=t_hacked_vec2d;
    using t_points=std::vector<point>;
    using site=typename t_points::const_iterator;
    using sweepline_type=sweepline<site,point,value_type>;
    auto arr=(vector<t_hacked_vec2d>&)points;
    sweepline_type SL{eps};
    std::sort(std::begin(arr), std::end(arr));
    SL(std::cbegin(arr),std::cend(arr));
    vector<t_edge> edges;
    edges.clear();
    vector<vec2d> VA;
    for(auto&ex:SL.vertices_)VA.push_back(ex.c);
    for(auto&ex:SL.edges_){
      if(ex.b==SL.inf&&ex.e==SL.inf)continue;
      edges.push_back({*ex.l,*ex.r});
    }
    return edges;
  }
public:
  string id2str(int id){
    static vector<string> arr={"wood","coal","gas","stell","copper"};
    return arr[id];
  }
  int get_market_id(vec2d pos,bool ignore_r){
    int market_id=-1;real best=1e30;
    for(int i=0;i<w.city.arr.size();i++){
      auto&ex=w.city.arr[i];
      auto a=(pos-ex.pos).SqrMag();
      if(market_id>=0&&a>best)continue;
      market_id=i;best=a;
    }
    if(market_id<0)return -1;
    auto dist=(pos-w.city.arr[market_id].pos).Mag();
    if(!ignore_r)if(dist>w.market_r)return -1;
    return market_id;
  }
  int get_fuel_station_id(vec2d pos,bool ignore_r){
    if(w.fsarr.empty()) return -1;
    int fs_id=-1;real best=1e30;
    for(int i=0;i<w.fsarr.size();i++){
      auto&ex=w.fsarr[i];
      auto a=(pos-ex.pos).SqrMag();
      if(fs_id>=0&&a>best)continue;
      fs_id=i;best=a;
    }
    if(fs_id<0)return -1;
    auto dist=(pos-w.fsarr[fs_id].pos).Mag();
    if(!ignore_r)if(dist>w.fsarr[fs_id].r)return -1;
    return fs_id;
  }
  int get_repair_dock_id(vec2d pos){
    auto&arr=w.rdarr;
    if(arr.empty()) return -1;
    int id=-1;real best=1e30;
    for(int i=0;i<arr.size();i++){
      auto&ex=arr[i];
      auto a=(pos-ex.pos).SqrMag();
      if(id>=0&&a>best)continue;
      id=i;best=a;
    }
    if(id<0)return -1;
    auto dist=(pos-arr[id].pos).Mag();
    if(dist>arr[id].r)return -1;
    return id;
  }
  bool under_demage=false;int under_demage_id=-1;
  void AddText(TextRender&TE){
    string BEG="^7";
    string SEP=" ^2: ^8";
    #define GOO(TEXT,VALUE)TE.AddText(string(BEG)+string(TEXT)+string(SEP)+string(VALUE));
    GOO("curr_t",FToS(w.t*1.0/Sys.UPS));
    GOO("prev_best_t",FToS(prev_best_t*1.0/Sys.UPS));
    GOO("curr_best_t",FToS(best_t*1.0/Sys.UPS));
    GOO("seed",IToS(seed));
    GOO("init_attempts",IToS(init_attempts));
    TE.AddText("^7---");
    GOO("Money",FToS2(w.car.money));
    TE.AddText("^8Need ^7"+IToS(required_money)+" ^8money to ^2win^8 in this game");
    TE.AddText("^7---");
    GOO("HP:",IToS(int(w.car.hp)));
    GOO("Fuel:",IToS(int(w.car.fuel))+"/"+IToS(int(w.car.max_fuel)));
    string surf="Normal";
    switch(get_surface_type(w.car.pos)){
      case SURFACE_NORMAL: surf="Normal"; break;
      case SURFACE_ICE: surf="Ice"; break;
      case SURFACE_MUD: surf="Mud"; break;
      case SURFACE_SAND: surf="Sand"; break;
      case SURFACE_ROAD: surf="Road"; break;
      case SURFACE_GRAVEL: surf="Gravel"; break;
    }
    TE.AddText("^7Surface: ^2"+surf);
    if(w.disaster.type==DISASTER_FIRE){
      TE.AddText("^1[Fire on the market] "+IToS(w.disaster.market_id+1)+"! Losses...");
    }else TE.BR();
    if(w.disaster.type==DISASTER_BLOCK){
      TE.AddText("^5[Market blocking] "+IToS(w.disaster.market_id+1)+"! Trading prohibited.");
    }else TE.BR();
    TE.AddText("^8Your cargo:");
    for(auto&it:w.car.cargo.items){
      TE.AddText(BEG+id2str(it.id)+SEP+IToS(it.amount));
    }
    TE.AddText("^8Black market cargo:");
    for(auto&it:w.car.cargo.black_items){
      TE.AddText("^1"+bid2str(it.id)+SEP+IToS(it.amount));
    }
    int bx=TE.bx;
    int cy=TE.y;
    vector<vector<string>> arr;
    vector<int> lens;lens.resize(5);
    for(auto&it:tops){arr.push_back(it.to_str());}
    for(auto&it:arr){
      for(int i=0;i<5;i++)lens[i]=max(lens[i],TE.text_len(it[i]));
    }
    lens.resize(3);
    auto seplen=TE.text_len("  ");
    vector<string> c={"^8","^7","^8","^7","^8"};
    real tot_len=0;for(auto&it:lens)tot_len+=it+seplen;
    TE.bx=Sys.SM.W*0.5-tot_len-TE.ident;TE.x=TE.bx;TE.y=Sys.SM.H*0.5-TE.ident;
    TE.AddText("^7---");
    TE.AddText("TOP15:");
    for(auto&it:arr){
      for(int i=0;i<lens.size();i++){auto x=TE.x;TE.AddTextNext(c[i]+it[i]);TE.x=x+lens[i]+seplen;}
      TE.BR();
    }
    TE.bx=bx;
    TE.y=cy;
    #undef GOO
  }
  void RenderText(QapDev&qDev){
    TextRender TE(&qDev);
    vec2d hs=vec2d(Sys.SM.W,Sys.SM.H)*0.5;
    hs.x=100;
    real ident=TE.ident;
    real Y=0;
    qDev.SetColor(0xff000000);
    TE.BeginScope(-hs.x+ident,+hs.y-ident,&Game->NormFont,&Game->BlurFont);
    RenderUI(qDev,TE);

    TE.AddText("^7--- Upgrades ---");
    auto& car = w.car;
    #define SHOW_UPG(name, val) TE.AddText(string("^7")+name+": ^2"+IToS(val));
    SHOW_UPG("Speed     ", car.speed_upgrade);
    SHOW_UPG("Fuel      ", car.fuel_upgrade);
    SHOW_UPG("Eco       ", car.eco_upgrade);
    SHOW_UPG("Armor     ", car.armor_upgrade);
    SHOW_UPG("Nitro     ", car.nitro_upgrade);
    SHOW_UPG("Suspension", car.suspension_upgrade);
    #undef SHOW_UPG
    RenderHint(qDev,TE);
    TE.EndScope();
  }

  struct t_ui_btn_state {
    int idx = -1, market_id = -1;
    enum { NONE = 0, BUY = 1, SELL = 2, QSELL = 3 } type = NONE;
    bool hovered = false;
  };

  struct t_ui_market_ctx {
    vec2d pos;
    vector<t_item>*items{};
    vector<t_cargo_item>*player_items{};
    int market_id{-1};
    bool is_black = false;
    bool is_workshop = false;
    std::function<void(int,int,int)> buy_fn;
    std::function<void(int,int,int)> sell_fn;
    std::function<std::string(int)> get_name_fn;
    std::function<int(int)> get_price_fn;
    std::function<int(int)> get_amount_fn;
  };

  t_ui_btn_state DrawUniversalMarketMenu(
    QapDev&qDev,TextRender&TE,real text_posx,const t_ui_market_ctx&ctx,bool buttons,bool quick_sell)
  {
    t_ui_btn_state result;result.type=result.NONE;result.hovered=false;
    if(!ctx.items||ctx.items->empty()||!ctx.player_items)return result;
    TE.bx=text_posx;TE.x=TE.bx;TE.y=+0.5*ctx.items->size()*TE.ident;
    int nitems=ctx.items->size();

    int namelen=0,pricelen=0,amountlen=0,mylen=0;
    vector<string> namearr,pricearr,amountarr,myarr;
    auto padIToS=[](int v,int pad){
      string is=IToS(v);
      int n=max(pad-int(is.size()),0);
      string sp;for(int i=0;i<n;i++)sp.push_back(' ');
      return sp+is;
    };

    for(int i=0;i<nitems;i++){
      string name="^8"+(ctx.get_name_fn?ctx.get_name_fn(i):(IToS(i+1)));
      string price="^3$^2"+padIToS(ctx.get_price_fn?ctx.get_price_fn(i):(*ctx.items)[i].price,4);
      string amt="^2"+padIToS(ctx.get_amount_fn?ctx.get_amount_fn(i):(*ctx.items)[i].amount,4);
      namearr.push_back(name);pricearr.push_back(price);amountarr.push_back(amt);
      namelen=max(namelen,TE.text_len(name));
      pricelen=max(pricelen,TE.text_len(price));
      amountlen=max(amountlen,TE.text_len(amt));
      if(buttons){
        int have=0;
        for(auto&ci:*ctx.player_items)
          if(ci.id==(*ctx.items)[i].id)have=ci.amount;
        string my="My:^2"+padIToS(have,3);
        myarr.push_back(my);
        mylen=max(mylen,TE.text_len(my));
      }
    }
    int seplen=TE.text_len(" ^4: ");
    int col_name=TE.bx;
    int col_price=col_name+namelen+seplen;
    int col_amount=col_price+pricelen+seplen;
    int col_my=col_amount+amountlen+seplen;
    int col_btn_buy=buttons?(col_my+mylen+seplen):0;
    int col_btn_sell=buttons?(col_btn_buy+TE.text_len(" [BuyAll]")+seplen*0):0;
    int row0_y=TE.y;
    vec2d mouse=kb.MousePos;

    bool is_blocked=(w.disaster.type==DISASTER_BLOCK)&&(ctx.market_id==w.disaster.market_id);
    static int max_x=0;
    for(int i=0;i<nitems;i++){
      TE.x=col_name;TE.y=row0_y-i*TE.ident;
      TE.AddTextNext("^7"+namearr[i]);
      TE.x=col_price;
      TE.AddTextNext("^7 : "+pricearr[i]);
      TE.x=col_amount;
      TE.AddTextNext("^7 : "+amountarr[i]);
      if(buttons){
        TE.x=col_my;
        TE.AddTextNext("^7 : "+myarr[i]);
        // BuyAll
        bool can_buy=(*ctx.items)[i].amount>0&&w.car.money>=((*ctx.items)[i].price);
        int max_buy=0;
        if(can_buy)max_buy=min(real((*ctx.items)[i].amount),w.car.money/(*ctx.items)[i].price);
        TE.x=col_btn_buy;
        bool buyall_hovered=false;
        string buy_btn_color="^8";
        if(is_blocked)buy_btn_color="^0";
        if(can_buy){
          auto dpos=mouse-vec2d(TE.x,TE.y-TE.ident);
          auto es=vec2d(TE.text_len(" [BuyAll]"),TE.ident);
          buyall_hovered=check_rect(dpos,es);
          if(!is_blocked&&buyall_hovered)buy_btn_color="^2";
          TE.AddTextNext(" "+buy_btn_color+"[BuyAll]");
          if(buyall_hovered&&!is_blocked){
            result.type=result.BUY;result.idx=i;result.market_id=ctx.market_id;result.hovered=true;
          }
        }else{
          if(is_blocked)
            TE.AddTextNext(" ^0[BuyAll]");
          else
            TE.AddTextNext(" ^7[BuyAll]");
        }
        // SellAll
        int have=0;for(auto&ci:*ctx.player_items)if(ci.id==(*ctx.items)[i].id)have=ci.amount;
        TE.x=col_btn_sell;
        bool sellall_hovered=false;
        string sell_btn_color="^8";
        if(is_blocked)sell_btn_color="^0";
        if(have>0){
          auto dpos=mouse-vec2d(TE.x,TE.y-TE.ident);
          auto es=vec2d(TE.text_len(" [SellAll]"),TE.ident);
          sellall_hovered=check_rect(dpos,es);
          if(!is_blocked&&sellall_hovered)sell_btn_color="^2";
          TE.AddTextNext(" "+sell_btn_color+"[SellAll]");
          if(sellall_hovered&&!is_blocked){
            result.type=result.SELL;result.idx=i;result.market_id=ctx.market_id;result.hovered=true;
          }
        }else{
          if(is_blocked)
            TE.AddTextNext(" ^0[SellAll]");
          else
            TE.AddTextNext(" ^7[SellAll]");
        }
      }
      max_x=max(max_x,TE.x);
      TE.BR();
    }
    // Quick Sell All (на отдельной строке)
    if(buttons&&quick_sell&&ctx.sell_fn&&nitems){
      TE.x=col_btn_sell;
      real quicksell_y=row0_y-nitems*32-TE.ident*1.5;
      auto dpos=mouse-vec2d(TE.x,quicksell_y);
      auto es=vec2d(TE.text_len(" [Quick Sell All]"),TE.ident);
      bool qsell_hovered=check_rect(dpos,es);
      TE.y=quicksell_y+TE.ident;
      string quicksell_btn_color="^8";
      if(is_blocked)quicksell_btn_color="^0";
      else if(qsell_hovered)quicksell_btn_color="^8";
      TE.AddTextNext(" "+quicksell_btn_color+"[Quick Sell All]");
      if(qsell_hovered&&!result.hovered&&!is_blocked){
        result.type=result.QSELL;result.idx=-2;result.market_id=ctx.market_id;result.hovered=true;
      }
      TE.BR();
    }
    return result;
  }
  t_ui_btn_state last_market_hover, last_black_hover;
  vec2d nuimpos=vec2d{-200,0};vec2d buimpos=vec2d{-600,0};
  t_ui_market_ctx ui_market_ctx{nuimpos}, ui_black_ctx{buimpos};
 
  // --- hovered-кнопка мастерской ---
  t_ui_btn_state workshop_btn_state;

  void RenderUI(QapDev&qDev,TextRender&TE)
  {
    ui_market_ctx.items = nullptr;
    ui_market_ctx.player_items = nullptr;
    ui_black_ctx.items = nullptr;
    ui_black_ctx.player_items = nullptr;
    int wsid=get_workshop_id(w.car.pos,false);
    if(wsid>=0){
      DrawUniversalUpgradeMenu(qDev,TE,-500,w.city.workshops[wsid],workshop_btn_state,true);
    }
    auto mf=[&](int mid,int x,bool btns){
      ui_market_ctx.items = nullptr;
      ui_market_ctx.player_items = nullptr;
      if (mid < 0)return;
      ui_market_ctx.items = &w.city.arr[mid].items;
      ui_market_ctx.player_items = &w.car.cargo.items;
      ui_market_ctx.market_id = mid;
      ui_market_ctx.is_black = false;
      ui_market_ctx.buy_fn = [&](int m,int i,int amt){t_cargo_item ci; ci.id=i; ci.amount=amt; buy(w,m,ci);};
      ui_market_ctx.sell_fn = [&](int m,int i,int amt){t_cargo_item ci; ci.id=i; ci.amount=amt; sell(w,m,ci);};
      ui_market_ctx.get_name_fn = [&](int i){return id2str(i);};
      ui_market_ctx.get_price_fn = [mid,this](int i){return int(w.city.arr[mid].items[i].price);};
      ui_market_ctx.get_amount_fn = [mid,this](int i){return w.city.arr[mid].items[i].amount;};
      DrawUniversalMarketMenu(qDev, TE, x, ui_market_ctx, btns, btns);
    };
    
    auto bf=[&](int bmid,int x,bool btns){
      ui_black_ctx.items = nullptr;
      ui_black_ctx.player_items = nullptr;
      if (bmid < 0)return;
      ui_black_ctx.items = &w.city.black_markets[bmid].items;
      ui_black_ctx.player_items = &w.car.cargo.black_items;
      ui_black_ctx.market_id = bmid;
      ui_black_ctx.is_black = true;
      ui_black_ctx.buy_fn = [&](int m,int i,int amt){buy_black(w,m,i,amt);};
      ui_black_ctx.sell_fn = [&](int m,int i,int amt){sell_black(w,m,i,amt);};
      ui_black_ctx.get_name_fn = [bmid,this](int i){return bid2str(w.city.black_markets[bmid].items[i].id);};
      ui_black_ctx.get_price_fn = [bmid,this](int i){return w.city.black_markets[bmid].items[i].price;};
      ui_black_ctx.get_amount_fn = [bmid,this](int i){return w.city.black_markets[bmid].items[i].amount;};
      
      DrawUniversalMarketMenu(qDev, TE, x, ui_black_ctx, btns, btns);
    };
    
    int mid = get_market_id(kb.MousePos, true);
    int bid = get_black_market_id(kb.MousePos, true);
    auto m0x=Sys.SM.W*0.5-290-32;
    auto m1x=Sys.SM.W*0.5-290-32-240*2.5-32;nuimpos.x=m1x;buimpos.x=m1x;
    if(bid>=0&&mid>=0){
      real md=(w.city.arr[mid].pos-kb.MousePos).Mag();
      real bd=(w.city.black_markets[bid].pos-kb.MousePos).Mag();
      if(bd<md){bf(bid,m0x,false);}else{mf(mid,m0x,false);}
    }else{
      if(bid>=0){bf(bid,m0x,false);}else if(mid>=0)mf(mid,m0x,false);
    }
    {
      int mid = get_market_id(w.car.pos, false);
      int bid = get_black_market_id(w.car.pos, false);
      mf(mid,m1x,true);
      bf(bid,m1x,true);
    }
  }
  void Render(QapDev&qDev){
    if(bool need_draw_rock0_as_thrt=true){
      if(bool need_draw_road_tiles=true){
        qDev.BindTex(0,0);
        QapDev::BatchScope Scope(qDev);
        for(auto&tile:w.road_tiles){
          QapColor col=0xffcccccc;
          switch(tile.type){
            case SURFACE_NORMAL: col=0x80bbbbbb; break;
            case SURFACE_ICE:    col=0x80b0e0ff; break;
            case SURFACE_MUD:    col=0x80a0522d; break;
            case SURFACE_SAND:   col=0x80ffff99; break;
            case SURFACE_ROAD:   col=0x80444444; break;
            case SURFACE_GRAVEL: col=0x80888888; break;
          }
          qDev.SetColor(col);
          qDev.DrawCircleEx(tile.pos,0,tile.r,32,0);
        }
      }
      if(bool need_draw_black_market=true)
      {
        qDev.BindTex(0,Game->Atlas.pTex);
        QapDev::BatchScope Scope(qDev);
        auto&F=*Game->FrameBlackMarket;
        F.Bind(qDev);
        for(auto&ex:w.city.black_markets){
          qDev.SetColor(0xff111111);
          qDev.DrawQuad(ex.pos.x,ex.pos.y,F.w,F.h,ex.ang);
        }
      }
      if(bool need_draw_workshops=true)
      {
        qDev.BindTex(0,Game->Atlas.pTex);
        QapDev::BatchScope Scope(qDev);
        auto&F=*Game->FrameWorkshop;
        F.Bind(qDev);
        for(auto&ex:w.city.workshops){
          qDev.SetColor(0xff00ffcc);
          qDev.DrawQuad(ex.pos.x,ex.pos.y,F.w,F.h,ex.ang);
        }
      }
      if(need_draw_dyn_obs_lines){
        QapDev::BatchScope Scope(qDev);
        qDev.BindTex(0,0);
        qDev.SetColor(0xffbbbbbb);
        for(auto&ex:w.dyn_obs){
          auto d=Vec2dEx(ex.ang,ex.len);
          DrawLine(qDev,ex.pos-d,ex.pos+d,ex.r*2);
        }
        qDev.SetColor(0xffbbbbff);
        for(auto&ex:w.dyn_obs){
          qDev.DrawQuad(ex.pos.x,ex.pos.y,8,8);
        }
      }
      auto draw_shadow_quad_v2=[&](QapDev&qDev,const TGame::t_frame&f,vec2d pos,real r,QapColor c){
        real zoom=r*2/real(f.pF->w);
        draw_shadow_quad(qDev,f.pS,true,pos,vec2d(f.pS->w*zoom,f.pS->h*zoom),c);
        draw_shadow_quad(qDev,f.pF,false,pos,vec2d(1,1)*r*2,c);
      };
      if(bool need_draw_obstacles=Game->FrameObstacle){
        qDev.SetColor(0xff888888);
        qDev.BindTex(0,Game->Atlas.pTex);
        QapDev::BatchScope Scope(qDev);
        auto&F=*Game->FrameObstacle;
        qDev.SetColor(0xffffffff);
        F.Bind(qDev);
        for(auto&ex:w.obstacles){
          qDev.DrawQuad(ex.x,ex.y,F.w,F.h);
        }
      }
      if(bool need_draw_rdarr=Game->FrameRepairDock){
        qDev.SetColor(0xffeeffee);
        qDev.BindTex(0,Game->Atlas.pTex);
        Game->FrameRepairDock->Bind(qDev);
        QapDev::BatchScope Scope(qDev);
        for(auto&d:w.rdarr){
          auto p=d.pos;
          qDev.DrawQuad(p.x,p.y,256,256,d.ang);
        }
      }
      int fsid=get_fuel_station_id(w.car.pos,false);
      if(bool need_draw_fsarr=true){
        if(Game->FrameFuelStation){
          qDev.BindTex(0,Game->Atlas.pTex);
          Game->FrameFuelStation->Bind(qDev);
          QapDev::BatchScope Scope(qDev);
          int i=0;
          for(auto&fs:w.fsarr){
            auto&p=fs.pos;
            qDev.SetColor(fsid==i?0xff88ddff:0xffffeebb);
            qDev.DrawQuad(p.x,p.y,256,256,fs.ang);
            i++;
          }
        }else{
          for(auto&fs:w.fsarr){
            qDev.DrawCircleEx(fs.pos,0,fs.r,32,0);
          }
        }
      }
      if(bool need_draw_dyn_obs=Game->FrameEnemy){
        qDev.BindTex(0,Game->Atlas.pTex);
        Game->FrameEnemy->Bind(qDev);
        QapDev::BatchScope Scope(qDev);
        int i=-1;
        for(auto&m:w.dyn_obs){
          i++;
          auto p=dyn_pos(m);
          qDev.SetColor(under_demage_id==i?0xffffaa88:0xffffffff);
          qDev.DrawQuad(p.x,p.y,128,128,m.gang);
        }
      }
      if(bool need_draw_tank=Game->th_rt_tex&&Game->th_rt_tex_full){
        bool cargo_empty=true;
        for(auto&ex:w.car.cargo.items)if(ex.amount>0)cargo_empty=false;
        auto*pF=cargo_empty?Game->th_rt_tex:Game->th_rt_tex_full;
        qDev.BindTex(0,pF);
        qDev.SetColor(under_demage?0xffffaaaa:0xffffffff);
        auto scale=0.5;
        qDev.DrawQuad(w.car.pos.x,w.car.pos.y,pF->W*scale,pF->H*scale,(-w.car.v.Ort()).GetAng());
      }
      if(bool need_draw_voronoi=kb.Down['V']){
        qDev.BindTex(0,0);
        qDev.SetColor(0xff0000ff);
        for(auto&ex:edges){
          DrawLine(qDev,ex.a,ex.b,4);
        }
        qDev.SetColor(0xffff0000);
        for(auto&ex:bad_edges){
          DrawLine(qDev,ex.a,ex.b,4);
        }
      }
      if(bool need_draw_markets=Game->FrameMarket){
        qDev.SetColor(0xffffffff);
        qDev.BindTex(0,Game->Atlas.pTex);
        QapDev::BatchScope Scope(qDev);
        Game->FrameMarket->Bind(qDev);
        int blocked_market = (w.disaster.type==2) ? w.disaster.market_id : -1;
        int fired_market = (w.disaster.type==1) ? w.disaster.market_id : -1;
        int i=0;
        for(auto&m:w.city.arr){
          qDev.SetColor(blocked_market>=0&&blocked_market==i?0xff888888:fired_market>=0&&fired_market==i?0xffff8888:0xffffffff);
          qDev.DrawQuad(m.pos.x,m.pos.y,128,128);
          i++;
        }
      }
      
      auto mf=[&](int id,int,bool){
        auto mpos=w.city.arr[id].pos;
        qDev.BindTex(0,Game->Atlas.pTex);
        qDev.SetColor(0xffff0000);
        draw_shadow_quad_v2(qDev,Game->frame_Dot,mpos,16,0xffff0000);
      };
      auto bf=[&](int id,int,bool){
        auto mpos=w.city.black_markets[id].pos;
        qDev.BindTex(0,Game->Atlas.pTex);
        qDev.SetColor(0xffff0000);
        draw_shadow_quad_v2(qDev,Game->frame_Dot,mpos,16,0xffff0000);
      };
      
      int mid = get_market_id(kb.MousePos, true);
      int bid = get_black_market_id(kb.MousePos, true);

      if(bid>=0&&mid>=0){
        real md=(w.city.arr[mid].pos-kb.MousePos).Mag();
        real bd=(w.city.black_markets[bid].pos-kb.MousePos).Mag();
        if(bd<md){bf(bid,450,false);}else{mf(mid,450,false);}
      }else{
        if(bid>=0){bf(bid,450,false);}else if(mid>=0)mf(mid,450,false);
      }
      
      int pid=get_market_id(w.car.pos,false);
      if(pid>=0&&Game->frame_Dot){
        auto mpos=w.city.arr[pid].pos;
        qDev.BindTex(0,Game->Atlas.pTex);
        qDev.SetColor(0xff00ff00);
        draw_shadow_quad_v2(qDev,Game->frame_Dot,mpos,16*0.45,0xff00ff00);
      }
      RenderText(qDev);
    }
  }
  struct t_button{t_cargo_item ci;int mid=0;bool hovered=false;};
  t_button bt_buy_all;
  t_button bt_sell_all;
  bool check_rect(vec2d dpos,vec2d es){
    return (dpos.x>0&&dpos.x<es.x)&&(dpos.y>0&&dpos.y<es.y);
  }
  
  void RenderHint(QapDev&qDev,TextRender&TE)
  {
    int fsid=get_fuel_station_id(w.car.pos,false);
    if(fsid>=0){
      auto&fs=w.fsarr[fsid];
      TE.bx=0;TE.x=TE.bx;
      TE.AddText("^8---");
      TE.AddText("^3[Fuel Station] ^7- Refueling a car");
      TE.AddText("^7Price per ^21 ^7unit of fuel: ^2"+FToS2(fs.price_per_unit));
    }
    int wsid=get_workshop_id(w.car.pos,false);
    if(wsid>=0){
      auto&ws=w.city.workshops[wsid];
      TE.bx=0;TE.x=TE.bx;
      TE.AddText("^8---");
      TE.AddText("^3[Workshop] ^7- ^8Tuning & Upgrades");
    }
    int rdid=get_repair_dock_id(w.car.pos);
    if(rdid>=0){
      auto&ws=w.rdarr[rdid];
      TE.bx=0;TE.x=TE.bx;
      TE.AddText("^8---");
      TE.AddText("^3[Repair Dock] ^7- ^8Auto repair");
    }
  }
  vec2d dyn_pos(const t_dynamic_obstacle&ex,int dt=0)
  {
    vec2d offset=Vec2dEx(ex.ang,ex.len*sin(Pi*2*ex.speed*(w.t+dt)/30/Sys.UPS));
    vec2d offset2=Vec2dEx(Pi*2*ex.speed*(w.t+dt+ex.dt)/30/Sys.UPS,ex.len);
    return ex.pos+(ex.circle?offset2:offset);
  }
  bool need_draw_dyn_obs_lines=false;
  
  static void draw_shadow_quad(QapDev&qDev,QapAtlas::TFrame*pF,bool shadow,vec2d pos,vec2d wh,QapColor color,real ang=0){
    qDev.color=shadow?QapColor(0xff000000):color;
    pF->Bind(qDev);
    auto p=pos;
    if(shadow)p+=vec2d(1.0,-1.0);
    qDev.DrawQuad(p.x,p.y,wh.x,wh.y,ang);
  }
  void DrawLine(QapDev&qDev,const vec2d&a,const vec2d&b,real line_size)
  {
    auto p=(b+a)*0.5;
    qDev.DrawQuad(p.x,p.y,(b-a).Mag(),line_size,(b-a).GetAng());
  }
  void start_disaster() {
    if (w.disaster.type>DISASTER_NONE) return;
    if (rand()%900!=0) return;
    w.disaster.type = (e_disaster_type)(1 + rand()%2);
    w.disaster.ticks = 1000+rand()%2000;
    w.disaster.market_id = rand()%w.city.arr.size();
    w.disaster.pos = w.city.arr[w.disaster.market_id].pos;
  }
  void update_disaster() {
    if (w.disaster.type==DISASTER_NONE) return;
    w.disaster.ticks--;
    if (w.disaster.ticks<=0) {
      w.disaster.type = DISASTER_NONE; w.disaster.market_id = -1;
      return;
    }
    if (w.disaster.type==DISASTER_FIRE) {
      auto& items = w.city.arr[w.disaster.market_id].items;
      for (auto& it: items) if (rand()%80==0 && it.amount>0) it.amount--;
    }
  }
  bool colide(){
    int i=-1;under_demage_id=-1;
    for(auto&ex:w.dyn_obs){
      i++;
      if((w.car.pos-dyn_pos(ex)).Mag()<ex.r+w.tank_r){under_demage_id=i;return true;}
    }
    return false;
  }
  bool is_car_blocked_now(){
    int blocked_market = (w.disaster.type==DISASTER_BLOCK) ? w.disaster.market_id : -1;
    if(blocked_market>=0){
      if((w.car.pos-w.city.arr[blocked_market].pos).Mag()<w.market_r*1.8){
        return true;
      }
    }
    return false;
  }
  void update_buttons(){
    if(last_market_hover.hovered&&kb.Down[mbLeft]==false){
      int gg=1;
      if(0){
        bool flag=kb_v2->Down[mbLeft];
        file_put_contents("g_log.txt",join(g_log,"\n"));
        int gg=1;
      }
      int bg=2;
    }
    if (last_market_hover.hovered && kb.Down[mbLeft]) {
      switch(last_market_hover.type){
        case t_ui_btn_state::BUY: {
          int i = last_market_hover.idx;
          int max_buy = std::min(real((*ui_market_ctx.items)[i].amount), w.car.money/(*ui_market_ctx.items)[i].price);
          if (max_buy > 0) ui_market_ctx.buy_fn(last_market_hover.market_id, i, max_buy);
        } break;
        case t_ui_btn_state::SELL: {
          int i = last_market_hover.idx;
          int have = 0; for(auto&ci:*ui_market_ctx.player_items) if(ci.id==(*ui_market_ctx.items)[i].id) have=ci.amount;
          if (have > 0) ui_market_ctx.sell_fn(last_market_hover.market_id, i, have);
        } break;
        case t_ui_btn_state::QSELL:
          for(int i=0;i<ui_market_ctx.items->size();i++) {
            int have=0;
            for(auto&ci:*ui_market_ctx.player_items) if(ci.id==(*ui_market_ctx.items)[i].id) have=ci.amount;
            if(have>0) ui_market_ctx.sell_fn(ui_market_ctx.market_id,i,have);
          }
          break;
        case t_ui_btn_state::NONE: break;
      }
    }
    if (last_black_hover.hovered && kb.Down[mbLeft]) {
      switch(last_black_hover.type){
        case t_ui_btn_state::BUY: {
          int i = last_black_hover.idx;
          int max_buy = std::min(real((*ui_black_ctx.items)[i].amount), w.car.money/(*ui_black_ctx.items)[i].price);
          if (max_buy > 0) ui_black_ctx.buy_fn(last_black_hover.market_id, i, max_buy);
        } break;
        case t_ui_btn_state::SELL: {
          int i = last_black_hover.idx;
          int have = 0; for(auto&ci:*ui_black_ctx.player_items) if(ci.id==(*ui_black_ctx.items)[i].id) have=ci.amount;
          if (have > 0) ui_black_ctx.sell_fn(last_black_hover.market_id, i, have);
        } break;
        case t_ui_btn_state::QSELL:
          for(int i=0;i<ui_black_ctx.items->size();i++) {
            int have=0;
            for(auto&ci:*ui_black_ctx.player_items) if(ci.id==(*ui_black_ctx.items)[i].id) have=ci.amount;
            if(have>0) ui_black_ctx.sell_fn(ui_black_ctx.market_id,i,have);
          }
          break;
        case t_ui_btn_state::NONE: break;
      }
    }
  }
  // --- Добавляем enum для апгрейдов ---
  enum e_workshop_upgrade {
    UP_SPEED = 0,
    UP_FUEL,
    UP_ECO,
    UP_ARMOR,
    UP_NITRO,
    UP_SUSPENSION,
    UP_TOTAL
  };

  // --- Универсальный UI для мастерской ---
  t_ui_btn_state DrawUniversalUpgradeMenu(
    QapDev&qDev, TextRender&TE, real text_posx, t_workshop&ws, t_ui_btn_state&btn_state, bool buttons)
  {
    t_ui_btn_state result; result.type = result.NONE; result.idx = -1; result.hovered = false;
    TE.bx = text_posx;
    TE.x = TE.bx;
    TE.y = 0;
    std::vector<std::string> upg_names = {"Speed", "Fuel", "Eco", "Armor", "Nitro", "Suspension"};
    std::vector<int> upg_lvls = {ws.upgrade_level, ws.fuel_upgrade, ws.eco_upgrade, ws.armor_upgrade, ws.nitro_upgrade, ws.suspension_upgrade};
    std::vector<int> upg_costs = {ws.upgrade_cost, ws.fuel_upgrade_cost, ws.eco_upgrade_cost, ws.armor_upgrade_cost, ws.nitro_upgrade_cost, ws.suspension_upgrade_cost};

    int maxlen=0, costlen=0, lvllen=0;
    std::vector<std::string> lvlarr, costarr;
    for(int i=0;i<UP_TOTAL;i++){
      std::string lvl = IToS(upg_lvls[i]);
      std::string cost = IToS(upg_costs[i]);
      lvlarr.push_back(lvl);
      costarr.push_back(cost);
      maxlen = std::max(maxlen, TE.text_len(upg_names[i]));
      lvllen = std::max(lvllen, TE.text_len(lvl));
      costlen = std::max(costlen, TE.text_len(cost));
    }
    int seplen = TE.text_len("  ");
    int col_name = TE.bx;
    int col_lvl = col_name + maxlen + seplen;
    int col_cost = col_lvl + lvllen + seplen;
    int col_btn = col_cost + costlen + seplen;

    for(int i=0;i<UP_TOTAL;i++){
      TE.x=col_name; TE.y = -i*TE.ident;
      TE.AddTextNext("^7"+upg_names[i]);
      TE.x=col_lvl;  TE.AddTextNext("  "+lvlarr[i]);
      TE.x=col_cost; TE.AddTextNext("  "+costarr[i]);
      TE.x=col_btn;
      bool can_upg = w.car.money >= upg_costs[i];
      bool hovered = false;
      if(buttons && can_upg){
        auto dpos = kb.MousePos - vec2d(TE.x, TE.y-TE.ident);
        auto es = vec2d(TE.text_len(" [Upgrade]"), TE.ident);
        hovered = check_rect(dpos, es);
        TE.AddTextNext(" "+std::string(hovered?"^2":"^8")+"[Upgrade]");
        if(hovered){
          result.type = t_ui_btn_state::BUY;
          result.idx = i;
          result.market_id = -1;
          result.hovered = true;
        }
      }else{
        TE.AddTextNext(" ^7[Upgrade]");
      }
      TE.BR();
    }
    btn_state = result;
    return result;
  }
  void Update(TGame*Game){
    if(!inited){
      bool ok=init_attempt();
      init_attempts++;
      if(ok){inited=true;Sys.UPS_enabled=true;Sys.ResetClock();ui_market_ctx={};ui_black_ctx={};}else{return;}
    }
    {
      QapDev*pQapDev=nullptr;
      TextRender TE(pQapDev);TE.dummy=true;
      vec2d hs=vec2d(Sys.SM.W,Sys.SM.H)*0.5;
      hs.x=100;
      real ident=TE.ident;
      real Y=0;
      TE.BeginScope(-hs.x+ident,+hs.y-ident,&Game->NormFont,&Game->BlurFont);
      last_market_hover = DrawUniversalMarketMenu(*pQapDev, TE, nuimpos.x, ui_market_ctx, true, true);
      last_black_hover  = DrawUniversalMarketMenu(*pQapDev, TE, buimpos.x, ui_black_ctx, true, true);
      TE.EndScope();
      update_buttons();
      last_market_hover={};
      last_black_hover={};
    }
    {
      QapDev*pQapDev=nullptr;
      TextRender TE(pQapDev);TE.dummy=true;
      vec2d hs=vec2d(Sys.SM.W,Sys.SM.H)*0.5;
      hs.x=100;
      real ident=24.0;
      real Y=0;
      TE.BeginScope(-hs.x+ident,+hs.y-ident,&Game->NormFont,&Game->BlurFont);
      int wsid=get_workshop_id(w.car.pos,false);
      if(wsid>=0){
        DrawUniversalUpgradeMenu(*pQapDev,TE,-500,w.city.workshops[wsid],workshop_btn_state,true);
      }
      TE.EndScope();
      update_buttons();
      for(auto&ex:w.fsarr){ex.price_per_unit=std::min(3.0,0.03+real(w.t)/Sys.UPS/60.0/3.0);}
      // --- обработка апгрейда с помощью енумов ---
      if(workshop_btn_state.hovered && kb.OnDown(mbLeft) && wsid>=0){
        auto& ws = w.city.workshops[wsid];
        switch(static_cast<e_workshop_upgrade>(workshop_btn_state.idx)){
          case UP_SPEED:
            if(w.car.money>=ws.upgrade_cost){
              w.car.money -= ws.upgrade_cost;
              w.car.speed_upgrade++;
              ws.upgrade_level++;
              ws.upgrade_cost = int(ws.upgrade_cost*1.7)+100;
            }
            break;
          case UP_FUEL:
            if(w.car.money>=ws.fuel_upgrade_cost){
              w.car.money -= ws.fuel_upgrade_cost;
              w.car.fuel_upgrade++;
              ws.fuel_upgrade++;
              ws.fuel_upgrade_cost = int(ws.fuel_upgrade_cost*1.6)+90;
            }
            break;
          case UP_ECO:
            if(w.car.money>=ws.eco_upgrade_cost){
              w.car.money -= ws.eco_upgrade_cost;
              w.car.eco_upgrade++;
              ws.eco_upgrade++;
              ws.eco_upgrade_cost = int(ws.eco_upgrade_cost*1.6)+80;
            }
            break;
          case UP_ARMOR:
            if(w.car.money>=ws.armor_upgrade_cost){
              w.car.money -= ws.armor_upgrade_cost;
              w.car.armor_upgrade++;
              ws.armor_upgrade++;
              ws.armor_upgrade_cost = int(ws.armor_upgrade_cost*1.7)+100;
            }
            break;
          case UP_NITRO:
            if(w.car.money>=ws.nitro_upgrade_cost){
              w.car.money -= ws.nitro_upgrade_cost;
              w.car.nitro_upgrade++;
              ws.nitro_upgrade++;
              ws.nitro_upgrade_cost = int(ws.nitro_upgrade_cost*1.7)+100;
            }
            break;
          case UP_SUSPENSION:
            if(w.car.money>=ws.suspension_upgrade_cost){
              w.car.money -= ws.suspension_upgrade_cost;
              w.car.suspension_upgrade++;
              ws.suspension_upgrade++;
              ws.suspension_upgrade_cost = int(ws.suspension_upgrade_cost*1.6)+90;
            }
            break;
          default: break;
        }
      }
    }
    // --- Починка ---
    for(auto&ex:w.rdarr){
      auto dist=w.tank_r;
      if((w.car.pos-ex.pos).Mag()<dist){
        if(w.car.money>=10&&w.car.hp<w.car.max_hp){
          w.car.money-=10;
          w.car.hp++;
        }
      }
    }
    // --- Заправка ---
    int fsid=get_fuel_station_id(w.car.pos,false);
    if (fsid >= 0) {
      auto& fs = w.fsarr[fsid];
      int need = w.car.max_fuel - w.car.fuel;
      if (need > 0 && w.car.money >= fs.price_per_unit) {
        w.car.fuel += 1;
        w.car.money -= fs.price_per_unit;
        if (w.car.fuel > w.car.max_fuel) w.car.fuel = w.car.max_fuel;
        if (w.car.money < 0) w.car.money = 0;
      }
    }
    // --- Экономический кризис и катастрофы ---
    start_disaster();
    update_disaster();

    // --- dyn_obs: исчезновение при окончании HP и получение урона при столкновении ---
    for(int i=int(w.dyn_obs.size())-1; i>=0; --i){
      auto& ex = w.dyn_obs[i];
      if((w.car.pos-dyn_pos(ex)).Mag()<ex.r+w.tank_r && !w.car.deaded){
        ex.hp -= 1;
      }
      if(ex.hp<=0) { w.dyn_obs.erase(w.dyn_obs.begin()+i); continue; }
    }
    // --- Покрытие дорог + интеграция апгрейдов ---
    e_surface_type surface = get_surface_type(w.car.pos);
    real base_fuel_per_tick = 0.07;
    real fuel_per_tick = base_fuel_per_tick;
    real speed_mod = 1.0;
    real eco_bonus = 1.0 - 0.25 * w.car.eco_upgrade; // до -75% расхода
    real suspension_bonus = 1.0 + 1.0 * w.car.suspension_upgrade;
    real base_max_hp = 100.0;
    real base_max_fuel = 600.0*4;
    // учёт апгрейдов HP и топлива
    w.car.max_hp = base_max_hp + w.car.armor_upgrade*100;
    w.car.max_fuel = base_max_fuel + w.car.fuel_upgrade*600;
    if(w.car.hp>w.car.max_hp) w.car.hp=w.car.max_hp;
    if(w.car.fuel>w.car.max_fuel) w.car.fuel=w.car.max_fuel;

    switch(surface){
      case SURFACE_NORMAL: speed_mod=1.0; fuel_per_tick=0.09; break;
      case SURFACE_ICE:    speed_mod=1.4; fuel_per_tick=0.08; break;
      case SURFACE_MUD:    speed_mod=0.5*suspension_bonus; fuel_per_tick=0.17/eco_bonus; break;
      case SURFACE_SAND:   speed_mod=0.7*suspension_bonus; fuel_per_tick=0.13/eco_bonus; break;
      case SURFACE_ROAD:   speed_mod=1.2; fuel_per_tick=0.07/eco_bonus; break;
      case SURFACE_GRAVEL: speed_mod=0.8*suspension_bonus; fuel_per_tick=0.11/eco_bonus; break;
    }
    // апгрейд скорости
    speed_mod += 0.75 * w.car.speed_upgrade;
    // --- Нитро-ускоритель ---
    if(w.car.nitro_upgrade>0 && kb.OnDown('N') && w.car.nitro_ticks<=0 && w.car.fuel>10){
      w.car.nitro_ticks = 600 + 200*w.car.nitro_upgrade;
      w.car.fuel -= 10;
    }
    if(w.car.nitro_ticks>0){
      speed_mod *= 1.6 + 0.7*w.car.nitro_upgrade; // заметный буст
      w.car.nitro_ticks--;
    }
    // --- Движение ---
    for(auto&ex:w.dyn_obs){ex.gang+=Pi*0.9*ex.gspd/180;}
    if(kb.OnDown('L'))need_draw_dyn_obs_lines=!need_draw_dyn_obs_lines;
    auto dk=0.0;auto dAng=6.28*0.33/(2*Sys_UPD);
    if(kb.OnDown(VK_F9)){reinit_the_same_level();}
    if(kb.Down[VK_LEFT]||kb.Down['A']){dk=+1;}
    if(kb.Down[VK_RIGHT]||kb.Down['D']){dk=-1;}
    if(w.car.deaded)dk=0;
    w.car.v=Vec2dEx(w.car.v.GetAng()+Clamp(dk,-5.0,+5.0)*dAng,1);
    auto v2=vec2d();
    bool was_move=false;
    if(kb.Down[VK_UP]||kb.Down['W']){v2=+w.car.v*speed_mod;was_move=true;}
    if(kb.Down[VK_DOWN]||kb.Down['S']){v2=-w.car.v*speed_mod;was_move=true;}
    if(is_car_blocked_now())v2*=0.15;
    auto new_pos=w.car.pos+v2;
    auto new_v2=vec2d(0,0);int n=0;
    for(auto&ex:w.obstacles){
      auto dist=w.obstacle_r+w.tank_r;
      if((new_pos-ex).Mag()<dist){
        new_v2+=(new_pos-ex).SetMag(dist)+ex-w.car.pos;
        n++;
      }
    }
    v2=n?new_v2*(1.0/n):v2;
    bool runned=!Win()&&!Fail();
    if(runned){
      if(w.car.fuel>0){
        w.car.pos+=v2;
        if(was_move&&v2.Mag()>0.1){
          w.car.fuel-=fuel_per_tick;
          if(w.car.fuel<0)w.car.fuel=0;
        }
      }else w.car.pos+=v2*0.25;
    }
    if(runned)w.t++;
    under_demage=colide();
    if(under_demage){
      w.car.hp--;
      if(w.car.hp<=0){w.car.hp=0;w.car.deaded=true;}
    }
    on_win=false;
    if(Win()){if(!wined)on_win=true;wined=true;}
    if(bool need_best_t=true){
      auto fn="score.txt";
      if(w.t==1){
        auto s=file_get_contents(fn);
        prev_best_t=s.empty()?1e9:stoi(s);
        best_t=prev_best_t;
      }
      if(on_win){
        if(prev_best_t>w.t){best_t=w.t;file_put_contents(fn,IToS(w.t));}
      }
    }
    if(on_win){
      TGame::wget(g_host,"/c/game_players_table.js?game="+game_version+"&user="+Game->user_name+"&sec="+FToS(w.t*1.0/Sys.UPS)+"&seed="+IToS(seed)+"&ref="+ref,[](const string&url,int ptr,int size){});
      reinit_top20();
    }
  }
  int seed=0;
  int prev_best_t=0;
  int best_t=0;
  bool wined=false;
  bool on_win=false;
  
  bool quick_sell_all(const t_ui_market_ctx&ctx) {
    bool sold=false;
    for(int i=0;i<ctx.items->size();i++) {
      int have=0;
      for(auto&ci:*ctx.player_items) if(ci.id==(*ctx.items)[i].id) have=ci.amount;
      if(have>0) {ctx.sell_fn(ctx.market_id,i,have);sold=true;}
    }
    return sold;
  }
};