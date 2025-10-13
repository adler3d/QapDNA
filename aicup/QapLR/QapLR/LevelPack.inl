#define LEVEL_LIST(F)F(Level_LocalRunner);
#define FRAMESCOPE(F)\
  F(MenuItem,"MenuItem",0)\
  //---
#define FRAMESCOPE2(F)\
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

struct t_offcentric_scope{
  QapDev&qDev;
  const vec2d&unit_pos;
  const vec2d&unit_dir;
  const real scale;
  t_offcentric_scope(QapDev&qDev,const vec2d&unit_pos,const vec2d&unit_dir,real scale,bool unit_offcentric):
    qDev(qDev),unit_pos(unit_pos),unit_dir(unit_dir),scale(scale)
  {
    qDev.xf=make_xf(qDev.viewport,unit_pos,unit_dir,scale,unit_offcentric);
    QapAssert(!qDev.use_xf);
    qDev.use_xf=true;
  }
  static real get_koef(){return 0.25;}
  static transform2f make_xf(const t_quad&viewport,const vec2d&unit_pos,const vec2d&unit_dir,real scale,bool unit_offcentric)
  {
    static const real offcentric_koef=get_koef();
    vec2d offcentric=vec2d(0,viewport.size.y*offcentric_koef);
    auto base_offset=unit_pos;//+qDev.viewport.pos;
    transform2f xf;
    xf.r.set(unit_dir.GetAng());
    xf.r.mul(scale);
    xf.p.set_zero();
    xf.p=(xf*-base_offset)+vec2f(-(unit_offcentric?offcentric:vec2d{}));
    return xf;
  }
  static vec2d screen_to_world(const t_quad&viewport,const vec2d&s2wpos,const vec2d&cam_pos,const vec2d&cam_dir,real scale,bool offcentric)
  {
    auto off_offset=offcentric?vec2d(0,viewport.size.y*t_offcentric_scope::get_koef()):vec2d(0,0);
    return cam_pos+(s2wpos+off_offset).Rot(cam_dir)*(1.0/scale);
  };
 ~t_offcentric_scope()
  {
    qDev.use_xf=false;
    qDev.xf.set_ident();
  }
};

class WithOffcentricScope{
public:
  void UpdateOffcentricScope(const QapKeyboard&kb){
    vec2d mpos=kb.MousePos;
    if(kb.OnDown(mbRight)){drag_wp=s2w(mpos);}
    if(kb.Down(mbRight)){cam_pos+=-s2w(mpos)+drag_wp;}

    if(kb.Down(VK_ADD))scale*=1.01;
    if(kb.Down(VK_SUBTRACT))scale/=1.01;
    if(kb.OnDown(VK_DIVIDE))scale=1.0;
    if(kb.OnDown(VK_MULTIPLY))scale*=0.5;
    cam_pos+=kb.get_dir_from_wasd_and_arrows(true,false).Rot(cam_dir)*(6.0/scale);

    if(kb.zDelta>0){auto wp=s2w(mpos);scale*=1.5;cam_pos+=(-s2w(mpos)+wp);}
    if(kb.zDelta<0){auto wp=s2w(mpos);scale/=1.5;cam_pos+=(-s2w(mpos)+wp);}
  }
public:
  vec2d s2w(const vec2d&pos){
    return t_offcentric_scope::screen_to_world(*pviewport,pos,cam_pos,cam_dir,scale,cam_offcentric);
  }
  vec2d w2s(const vec2d&pos){
    return t_offcentric_scope::make_xf(*pviewport,cam_pos,cam_dir,scale,cam_offcentric)*pos;
  }
public:
  vec2d drag_wp;
  vec2d cam_pos;
  real scale=1.0;
  vec2d cam_dir=vec2d(1,0);
  bool cam_rot=false;
  bool cam_offcentric=false;
  t_quad*pviewport=nullptr;
};

struct t_splinter{
  static double Dist2Line(const vec2d&point,const vec2d&a,const vec2d&b){
    auto p=(point-a).Rot(b-a);
    if(p.x<0||p.x>(b-a).Mag())return 1e9;
    return fabs(p.y);
  }
  struct t_world{
    struct t_ball {
      #define DEF_PRO_COPYABLE()
      #define DEF_PRO_CLASSNAME()t_ball
      #define DEF_PRO_VARIABLE(ADD)\
      ADD(vec2d,pos,{})\
      ADD(vec2d,vel,{})\
      ADD(double,mass,1.0)\
      //===
      #include "defprovar.inl"
      //===
    };
    struct t_point {
      #define DEF_PRO_COPYABLE()
      #define DEF_PRO_CLASSNAME()t_point
      #define DEF_PRO_VARIABLE(ADD)\
      ADD(vec2d,pos,{})\
      ADD(vec2d,v,{})\
      ADD(bool,deaded,false)\
      ADD(int,color,0)\
      //===
      #include "defprovar.inl"
      //===
    };
    struct t_spring {
      #define DEF_PRO_COPYABLE()
      #define DEF_PRO_CLASSNAME()t_spring
      #define DEF_PRO_VARIABLE(ADD)\
      ADD(int,a,{})\
      ADD(int,b,{})\
      ADD(double,rest_length,100)\
      ADD(double,k,0.0351)\
      //===
      #include "defprovar.inl"
      //===
    };
  public:
    struct t_cmd {
      #define DEF_PRO_COPYABLE()
      #define DEF_PRO_CLASSNAME()t_cmd
      #define DEF_PRO_VARIABLE(ADD)\
      ADD(double,spring0_new_rest,100)\
      ADD(double,spring1_new_rest,100)\
      ADD(double,spring2_new_rest,100)\
      ADD(double,friction0,0.5)\
      ADD(double,friction1,0.5)\
      ADD(double,friction2,0.5)\
      //===
      #include "defprovar.inl"
      //===
    public:
      bool valid() const {
          return  spring0_new_rest >= 5 && spring0_new_rest <= 100 &&
                  spring1_new_rest >= 5 && spring1_new_rest <= 100 &&
                  spring2_new_rest >= 5 && spring2_new_rest <= 100 &&
                  friction0 >= 0.01 && friction0 <= 0.99 &&
                  friction1 >= 0.01 && friction1 <= 0.99 &&
                  friction2 >= 0.01 && friction2 <= 0.99;
      }
      double&f(int id){return (&friction0)[id];}
      double&s(int id){return (&spring0_new_rest)[id];}
    };
  public:
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_world
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(int,tick,0)\
    ADD(double,ARENA_RADIUS,512.0)\
    ADD(vector<t_ball>,balls,{})\
    ADD(vector<t_spring>,springs,{})\
    ADD(vector<t_point>,parr,{})\
    ADD(vector<int>,slot2deaded,{})\
    ADD(vector<double>,slot2score,{})\
    ADD(vector<t_cmd>,cmd_for_player,{})\
    //===
    #include "defprovar.inl"
    //===
  public:
      void use(int player_id, const t_cmd& c) {
          auto cmd=c;
          auto*s=&cmd.spring0_new_rest;auto*f=&cmd.friction0;
          for(int i=0;i<3;i++)s[i]=Clamp(s[i],5.0,100.0);
          for(int i=0;i<3;i++)f[i]=Clamp(f[i],0.01,0.99);
          int base = player_id * 3;
          if (!cmd.valid()){
            int fail=1;
            return;
          }
          cmd_for_player[player_id]=cmd;
          springs[base + 0].rest_length = cmd.spring0_new_rest;
          springs[base + 1].rest_length = cmd.spring1_new_rest;
          springs[base + 2].rest_length = cmd.spring2_new_rest;
      }
      vec2d get_center_of_mass(int player_id)const{
          vec2d sum(0, 0);
          int base = player_id * 3;
          for (int i = 0; i < 3; i++) {
              sum = sum + balls[base + i].pos;
          }
          return sum *(1.0/3);
      }
      bool finished_flag=false;
      void step(std::mt19937&gen,bool with_score_from_points=true) {
          tick++;

          // Сброс сил
          vector<vec2d> forces(balls.size());

          // Расчёт сил пружин
          for (auto& s : springs) {
              auto& a = balls[s.a];
              auto& b = balls[s.b];
              vec2d delta = b.pos - a.pos;
              double dist = delta.Mag();
              if (dist == 0) continue;
              vec2d dir = delta.Norm();
              auto rl=slot2deaded[s.a/3]?100:s.rest_length;
              double force = s.k * (dist - rl);
              forces[s.a] += dir * force;
              forces[s.b] -= dir * force; // противоположная сила
          }
          for(int i=0;i<4;i++)if(slot2deaded[i])cmd_for_player[i]={};
          // Обновление скорости и позиции (простой интегратор)
          for (int i = 0; i < balls.size(); i++) {
              auto& b = balls[i];
              b.vel += forces[i] *(1.0/(b.mass * 1.0));
              // Вместо глобального трения:
              // b.vel = b.vel * 0.98;

              // Делаем по шарику:
              int player_id = i / 3;
              int ball_in_player = i % 3;
              auto t=(tick+3000)*Pi2/4000.0;
              auto s=sin(t)*0.5+0.5;//0->0->0.5->0.95;1->
              auto p=pow(s,2);
              double df=1.0-Lerp(0.05,1.0,p);//tick%1000>500?0.05:1;//0.05*20.1/20;
              double friction=df*cmd_for_player[player_id].f(ball_in_player);
              b.vel*=1.0-friction;
              b.pos+=b.vel*1.0;

              // Отскок от стенки (арена — круг)
              vec2d&to_center = b.pos;auto r=ARENA_RADIUS-8;
              double d = to_center.Mag();
              if (d > r) {
                  vec2d n = to_center.Norm();
                  b.pos = n * r;
                  // отражение скорости
                  //double vn = b.vel.x * n.x + b.vel.y * n.y;
                  //b.vel = {b.vel.x - 2*vn*n.x, b.vel.y - 2*vn*n.y};
                  //b.vel *= 0.5; // гасим при ударе
                  auto v=b.vel.Rot(to_center);
                  if(v.x>0)v.x*=-0.5;
                  b.vel=v.UnRot(to_center);
              }
          }

          auto br=8.0;auto dd=br*br*4;
          for(int i=0;i<balls.size();i++){
            auto&a=balls[i];
            for(int j=i+1;j<balls.size();j++){
              auto&b=balls[j];
              if(!a.pos.sqrdist_to_point_less_that_rr(b.pos,dd))continue;
              auto ba=a.pos-b.pos;auto k=ba.Mag();
              //a.vel-=-ba*0.35;
              //b.vel-=+ba*0.35;
              auto avox=a.vel.Rot(ba);auto bvox=b.vel.Rot(ba);
              auto dv=avox.x-bvox.x;
              if(dv<0)swap(avox.x,bvox.x);
              a.vel=avox.UnRot(ba);
              b.vel=bvox.UnRot(ba);
              auto c=0.5*(a.pos+b.pos);
              auto dp=ba.SetMag(br);
              b.pos=c-dp;
              a.pos=c+dp;
            }
            for(int j=0;j<parr.size();j++){
              auto&b=parr[j];
              if(!a.pos.sqrdist_to_point_less_that_rr(b.pos,dd))continue;
              if(b.color==i/3){b.deaded=true;if(with_score_from_points)slot2score[b.color]++;continue;}
              auto ba=a.pos-b.pos;auto k=ba.Mag();
              auto avox=a.vel.Rot(ba);auto bvox=b.v.Rot(ba);
              auto dv=avox.x-bvox.x;
              if(dv<0)swap(avox.x,bvox.x);
              a.vel=avox.UnRot(ba);
              b.v=bvox.UnRot(ba);
            }
          }
          qap_clean_if_deaded(parr);
          for(int i=0;i<parr.size();i++){
            auto&a=parr[i];
            for(int j=i+1;j<parr.size();j++){
              auto&b=parr[j];
              if(!a.pos.sqrdist_to_point_less_that_rr(b.pos,dd))continue;
              auto ba=a.pos-b.pos;auto k=ba.Mag();
              auto avox=a.v.Rot(ba);auto bvox=b.v.Rot(ba);
              auto dv=avox.x-bvox.x;
              if(dv<0)swap(avox.x,bvox.x);
              a.v=avox.UnRot(ba);
              b.v=bvox.UnRot(ba);
              //a.v-=-ba*0.35;
              //b.v-=+ba*0.35;
            }
          }
          for(int j=0;j<parr.size();j++){
            auto&b=parr[j];
            b.v*=0.95;
            b.pos+=b.v;
            vec2d&to_center = b.pos;auto r=ARENA_RADIUS-8;
            double d = to_center.Mag();
            if (d > r) {
                vec2d n = to_center.Norm();
                b.pos = n * r;
                auto v=b.v.Rot(to_center);
                if(v.x>0)v.x*=-0.5;
                b.v=v.UnRot(to_center);
            }
          }
        
          for(auto&ex:springs){
            auto p=ex.a/3;if(slot2deaded[p])continue;
            auto a=balls[ex.a].pos;auto b=balls[ex.b].pos;
            for(int i=0;i<balls.size();i++){
              if(i/3==p||slot2deaded[i/3])continue;
              auto&ex=balls[i];
              if(Dist2Line(ex.pos,a,b)>8)continue;
              slot2deaded[p]=true;
              slot2score[i/3]+=64;
            }
          }
          int n=0;
          for(auto&ex:slot2deaded){
            if(!ex)n++;
          }
          finished_flag=n<=1;
          vector<int> p2n(4);
          for(auto&ex:parr){
            p2n[ex.color]++;
          }
          for(int i=0;i<4;i++){
            if(!p2n[i]){
              finished_flag=true;
            }
          }
          for(int i=0;i<4;i++){
            if(p2n[i]<32){
              for(int i=0;i<4;i++)add_ball(*this,i,gen);
            }
          }
      }
      static bool add_ball(t_world&w,int i,std::mt19937&gen){
        auto r=[&gen](){static std::uniform_real_distribution<double>dist(-1.0,1.0);return vec2d{dist(gen),dist(gen)};};
        auto&b=qap_add_back(w.parr);
        b.pos=r()*w.ARENA_RADIUS;b.color=i%4;
        if(b.pos.Mag()>w.ARENA_RADIUS){i--;w.parr.pop_back();return false;}
        return true;
      };
  };
  static void init_world(t_world&world,std::mt19937&gen) {
    // Очищаем
    world.slot2deaded.assign(4,0);
    world.balls.clear();
    world.springs.clear();
    //world.slot2reached.assign(4, false);
    world.slot2score.assign(4, 0);
    world.tick = 0;

    // Углы для 4 игроков
    vector<vec2d> starts = {
      vec2d(-70, -70),  // лево-низ
      vec2d(+70, -70),  // право-низ
      vec2d(+70, +70),  // право-верх
      vec2d(-70, +70)   // лево-верх
    };

    const double REST_LENGTH = 30.0;
    
    auto r=[&gen](){static std::uniform_real_distribution<double>dist(-1.0,1.0);return vec2d{dist(gen),dist(gen)};};
    world.cmd_for_player.resize(4);
    for (int p = 0; p < 4; p++) {
      int base_idx = world.balls.size();

      // Три шарика в форме треугольника
      starts[p]=r()*500;
      auto add=[&](vec2d pos){qap_add_back(world.balls).pos=pos;};
      add(starts[p] + r()*15 + 0*vec2d(-10,  0));
      add(starts[p] + r()*15 + 0*vec2d(+10,  0));
      add(starts[p] + r()*15 + 0*vec2d(  0, 15));
        
      auto adds=[&](int x,int y){auto&b=qap_add_back(world.springs);b.a=x;b.b=y;b.rest_length=REST_LENGTH;};
      // Пружины: 0-1, 1-2, 2-0
      adds(base_idx + 0, base_idx + 1);
      adds(base_idx + 1, base_idx + 2);
      adds(base_idx + 2, base_idx + 0);
    }
    int n=32*4*2*2;//2;
    for(int i=0;i<n;i++){
      t_world::add_ball(world,i,gen);
    }
  }
  struct t_world_impl:i_world{
    t_world w;
    std::mt19937 gen;
    ~t_world_impl(){};
    void use(int player,const string&cmd,string&outmsg)override{
      t_world::t_cmd out;
      bool ok=QapLoadFromStr(out,cmd);
      if(!ok){outmsg="cmd load failed";return;}else{outmsg.clear();}
      w.use(player,out);
    }
    void step()override{w.step(gen);}
    bool finished()override{return w.finished_flag;}
    void get_score(vector<double>&out)override{out=w.slot2score;}
    void is_alive(vector<int>&out)override{out.resize(w.slot2deaded.size());for(int i=0;i<out.size();i++)out[i]=!w.slot2deaded[i];}
    void get_vpow(int player,string&out)override{
      out=QapSaveToStr(w);
    }
    void init(unsigned seed)override{
      gen=mt19937(seed);
      init_world(w,gen);
    }
    bool init_from_config(const string&cfg,string&outmsg)override{
      outmsg="no impl";
      return false;
    }
    unique_ptr<i_world> clone()override{return make_unique<t_world_impl>(*this);}
    void renderV0(QapDev&qDev)override{
      static vector<QapColor> player_colors={
        0xFFFF8080, // красный
        0xFF80FF80, // зелёный
        0xFF8080FF, // синий
        0xFFFFFF80  // жёлтый
      };
      auto&world=w;
      // --- Рисуем арену ---
      qDev.SetColor(0x40000000);
      qDev.DrawCircle(vec2d{}, world.ARENA_RADIUS,0,2, 64);

      // --- Рисуем цель ---
      qDev.SetColor(0x8000FF00); // полупрозрачный зелёный
      qDev.DrawCircle(vec2d{}, 5,0,2, 32);

      // --- Рисуем пружины ---
      for (const auto& spring : world.springs) {
          const auto& a = world.balls[spring.a].pos;
          const auto& b = world.balls[spring.b].pos;
          int player_id = spring.a / 3; // определяем игрока по индексу шарика
          qDev.SetColor(player_colors[player_id] & 0x80FFFFFF); // полупрозрачные
          qDev.DrawLine(a, b, 3.0);
          qDev.SetColor(0xff000000);
          qDev.DrawLine(a, b, 1.0);
      }
      if(0)
      for(int i=0;i<1;i++){
        vec2d c,v;
        for(int j=0;j<3;j++){
          int id=i*3+j;
          auto&ex=world.balls[id];v+=ex.vel;c+=ex.pos;
        }
        c*=(1/3.0);v*=(1/3.0);
        auto t=c+(v.Norm()*0.5+v.Ort().Norm())*200;
        qDev.SetColor(0xFFFFFFFF);
        qDev.DrawCircleEx(t, 0, 8.0, 16, 0);
        qDev.SetColor(0xFFFF0000);
        qDev.DrawCircleEx(c+v.SetMag(200), 0, 8.0, 16, 0);
      }
      
      for(int i=0;i<world.parr.size();i++){
        auto&ex=world.parr[i];
        qDev.SetColor(player_colors[ex.color]);
        qDev.DrawCircleEx(ex.pos, 0, 8.0, 10, 0);
      }

      // --- Рисуем шарики ---
      for (int p = 0; p < 4; p++) {
          for (int i = 0; i < 3; i++) {
              int idx = p * 3 + i;
              const auto& ball = world.balls[idx];
              qDev.SetColor(world.slot2deaded[p]?0x40000000:player_colors[p]&0xAAFFFFFF);
              qDev.DrawCircleEx(ball.pos, 0, 8.0+(world.cmd_for_player[p].f(i)*4), 32, 0);
              qDev.SetColor(0xff000000);
              qDev.DrawCircleEx(ball.pos, 8.0, 9.0, 32, 0);
          }
      }
      qDev.SetColor(0xFFFFFFFF);
    }
    int get_tick()override{return w.tick;}
  };
  static unique_ptr<i_world> mk_world(const int version){
    return make_unique<t_world_impl>();
  }
};

static unique_ptr<i_world> mk_world(const string&world){
  #define F(LVL)
  LEVEL_LIST(F)
  #undef F
  if(world=="t_splinter")return t_splinter::mk_world(0);
  return nullptr;
}

class Level_LocalRunner:public TGame::ILevel,public WithOffcentricScope{
public:
  TGame*Game=nullptr;
public:
  void reinit_the_same_level(){
    Game->ReloadWinFail();
  }
  bool inited=false;
  void Init(TGame*Game){
    this->pviewport=&Game->qDev.viewport;
    this->Game=Game;
  }
  bool Win(){return false;;}
  bool Fail(){return false;}
public:
  void AddText(TextRender&TE){
    string BEG="^7";
    string SEP=" ^2: ^8";
    #define GOO(TEXT,VALUE)TE.AddText(string(BEG)+string(TEXT)+string(SEP)+string(VALUE));
    GOO("curr_t",FToS(session.world->get_tick()*1.0/Sys.UPS));
    GOO("seed",IToS(seed));
    TE.AddText("^7---");
    vector<int> is_alive;session.world->is_alive(is_alive);
    for(int i=0;i<g_args.num_players;i++){
      string color="^7waiting";
      if(session.carr[i])color=session.connected[i]?(is_alive[i]?"^3online":"^2deaded"):"^4offline";
      if(session.carr[i]&&session.carr[i]->network_state==session.carr[i]->nsErr)color="^1error";
      GOO("player_status["+IToS(i)+"]",color);
    }
    TE.AddText("^7---");
    int bx=TE.bx;
    int cy=TE.y;
    TE.bx=bx;
    TE.y=cy;
    #undef GOO
  }
  int frame=-1;vector<unique_ptr<i_world>> ws;
  void RenderImpl(QapDev&qDev){
    QapDev::BatchScope Scope(qDev);
    t_offcentric_scope scope(qDev,cam_pos,cam_dir,scale,cam_offcentric);
    vec2d mpos=kb.MousePos;
    qDev.BindTex(0, nullptr);
    static bool set_frame=false;if(kb.OnDown('N'))set_frame=!set_frame;
    frame=-1;
    if(set_frame)frame=mpos.x+pviewport->size.x/2;
    if(frame<0||frame>=ws.size())frame=-1;
    auto&world=frame<0?*session.world:*ws[frame];
    auto api=world.get_render_api_version();
    if(api==0){
      world.renderV0(qDev);
    }
  }
  vector<int> AdlerCraftMap;
  MapGenerator AdlerCraft_mapgen;
  void genmap(){
    MapGenerator::Options opts;
    AdlerCraftMap=AdlerCraft_mapgen.generateMap(opts);
  }
  void RenderMap(QapDev&qDev){
    //genmap();
    if(AdlerCraftMap.empty())return;
    QapDev::BatchScope Scope(qDev);
    t_offcentric_scope scope(qDev,cam_pos,cam_dir,scale,cam_offcentric);
    vec2d mpos=kb.MousePos;
    qDev.BindTex(0,nullptr);
    auto cs=10.0;
    for(int y=0;y<80;y++)for(int x=0;x<80;x++){
      qDev.color=AdlerCraftMap[x+y*80]?0xFF008000:0xffaaaaaa;
      qDev.DrawQuad(y*cs-cs*40,x*cs-cs*40,cs,cs);
    }
  }
  void Render(QapDev&qDev){
    RenderImpl(qDev);
    RenderMap(qDev);
    RenderText(qDev);
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
    RenderHint(qDev,TE);
    TE.EndScope();
  }
  void RenderUI(QapDev&qDev,TextRender&TE)
  {
  }
  void RenderHint(QapDev&qDev,TextRender&TE)
  {
  }
  static void draw_shadow_quad(QapDev&qDev,QapAtlas::TFrame*pF,bool shadow,vec2d pos,vec2d wh,QapColor color,real ang=0){
    qDev.color=shadow?QapColor(0xff000000):color;
    pF->Bind(qDev);
    auto p=pos;
    if(shadow)p+=vec2d(1.0,-1.0);
    qDev.DrawQuad(p.x,p.y,wh.x,wh.y,ang);
  }
  void Update(TGame*Game){
    if(bool neeed_empty_te=true)
    {
      QapDev*pQapDev=nullptr;
      TextRender TE(pQapDev);TE.dummy=true;
      vec2d hs=vec2d(Sys.SM.W,Sys.SM.H)*0.5;
      hs.x=100;
      real ident=TE.ident;
      real Y=0;
      TE.BeginScope(-hs.x+ident,+hs.y-ident,&Game->NormFont,&Game->BlurFont);
      TE.EndScope();
    }
    if(kb.OnDown('Q')){genmap();Sys.UPS_enabled=false;}
    if(kb.OnDown(VK_ESCAPE)){Sys.NeedClose=true;}
    if(kb.OnDown(VK_F9)){reinit_the_same_level();}
    bool runned=!Win()&&!Fail();
    if(runned){
      bool done=false;
      while(session.try_step()){
        ws.push_back(session.world->clone());
        done=session.is_finished();
        if(done)break;
        session.send_vpow_to_all();
      }
      if(done){
        string report=session.generate_report();
        cerr<<report;
        //Sys.NeedClose = true;
      }
    }
    UpdateOffcentricScope(kb);
  }
public:
  unsigned seed=0;
};