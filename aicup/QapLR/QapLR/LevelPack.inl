#define LEVEL_LIST(F)F(Level_SplinterWorld);
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
class Level_SplinterWorld:public TGame::ILevel{
public:
  static double Dist2Line(const vec2d&point,const vec2d&a,const vec2d&b){
    auto p=(point-a).Rot(b-a);
    if(p.x<0||p.x>(b-a).Mag())return 1e9;
    return fabs(p.y);
  }
  struct t_splinter_world{
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
    double ARENA_RADIUS = 512.0;
  public:
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_splinter_world
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(int,tick,0)\
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
      void step(bool with_score_from_points=true) {
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
              for(int i=0;i<4;i++)add_ball(*this,i);
            }
          }
      }
      static bool add_ball(t_splinter_world&w,int&i){
        auto&b=qap_add_back(w.parr);
        b.pos=(vec2d(rand()/double(RAND_MAX),rand()/double(RAND_MAX))-vec2d(0.5,0.5))*2*w.ARENA_RADIUS;b.color=i%4;
        if(b.pos.Mag()>w.ARENA_RADIUS){i--;w.parr.pop_back();return false;}
        return true;
      }
  };
  typedef t_splinter_world t_world;
  static void init_world(t_world&world) {
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

    world.cmd_for_player.resize(4);
    for (int p = 0; p < 4; p++) {
      int base_idx = world.balls.size();

      // Три шарика в форме треугольника
      auto rnd=[](){return rand()*1.0/RAND_MAX;};auto r=[&](){return vec2d{rnd(),rnd()}*2-vec2d{1,1};};
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
      world.add_ball(world,i);
    }
  }
public:
  t_world w;t_world&world=w;
public:
  TGame*Game=nullptr;
public:
  t_world world_at_begin;
  void reinit_the_same_level(){
    w=world_at_begin;
    Game->ReloadWinFail();
  }
  bool init_attempt(){
    srand(seed=int(g_clock.MS()));
    w={};
    init_world(w);
    world_at_begin=w;
    return true;
  }
  int init_attempts=1;
  bool inited=false;
  void Init(TGame*Game){
    this->Game=Game;
    inited=init_attempt();
    if(!inited){Sys.UPS_enabled=false;}
    //reinit_top20();
  }
  bool Win(){return false;;}
  bool Fail(){return false;}
public:
  void AddText(TextRender&TE){
    string BEG="^7";
    string SEP=" ^2: ^8";
    #define GOO(TEXT,VALUE)TE.AddText(string(BEG)+string(TEXT)+string(SEP)+string(VALUE));
    GOO("curr_t",FToS(w.tick*1.0/Sys.UPS));
    GOO("seed",IToS(seed));
    GOO("init_attempts",IToS(init_attempts));
    TE.AddText("^7---");
    int bx=TE.bx;
    int cy=TE.y;
    TE.bx=bx;
    TE.y=cy;
    #undef GOO
  }
  int frame=-1;vector<t_world> ws;
  void RenderImpl(QapDev&qDev){
    vec2d mpos=kb.MousePos;
    qDev.BindTex(0, nullptr);
    
    QapDev::BatchScope Scope(qDev);
    static bool set_frame=false;if(kb.OnDown('N'))set_frame=!set_frame;
    frame=-1;
    static TScreenMode SM=GetScreenMode();
    if(set_frame)frame=mpos.x+SM.W/2;
    if(frame<0||frame>=ws.size())frame=-1;
    auto&world=frame<0?this->world:ws[frame];

    // --- Рисуем арену ---
    qDev.SetColor(0x40000000);
    qDev.DrawCircle(vec2d(0, 0), world.ARENA_RADIUS,0,2, 64);

    // --- Рисуем цель ---
    qDev.SetColor(0x8000FF00); // полупрозрачный зелёный
    qDev.DrawCircle(vec2d(0, 0), 5,0,2, 32);

    auto center=world.get_center_of_mass(0);auto&w=world;auto p=0;
    struct t_beste{double v;bool ok=false;int e;void use(t_beste c){if(!ok||c.v>v)*this=c;}};
    t_beste be;
    for(int e=0;e<4;e++)if(e!=p&&!w.slot2deaded[e])for(int i=0;i<3;i++){
      auto&ex=w.balls[i+e*3];
      be.use({-ex.pos.dist_to(center),true,e});
    }
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
            // Стрелка скорости
            if(0)if (ball.vel.Mag() > 0.1) {
                vec2d head = ball.pos + ball.vel.Norm() * 20;
                qDev.SetColor(0xFFFFFFFF);
                //DrawLine(qDev, ball.pos, head, 2.0);
                // Маленький треугольник на конце
                auto perp = ball.vel.Ort().Norm() * 5;
                qDev.SetColor(0xFFFFFFFF);
                qDev.DrawTrigon(head, head+ball.vel.Norm() * 20 - perp, head+ball.vel.Norm() * 20 + perp);
            }
        }
    }

    // --- Отладка: тики и статус ---
    qDev.SetColor(0xFFFFFFFF);
    /*
    string debug = "Tick: " + IToS(world.tick)+" ms:"+FToS(ms)+" "+to_string(seed);
    qap_text::draw(qDev, vec2d(-300, -200), debug, 24);
    for(int i=0;i<4;i++){
      string msg="score["+IToS(i)+"] = "+FToS(world.slot2score[i]);
      qDev.SetColor(player_colors[i]);
      qap_text::draw(qDev, vec2d(-300, -224-24*i), msg, 24);
    }*/
  }
  vector<int> AdlerCraftMap;
  MapGenerator AdlerCraft_mapgen;
  void genmap(){
    AdlerCraftMap=AdlerCraft_mapgen.generateMap();
  }
  void RenderMap(QapDev&qDev){
    //genmap();
    if(AdlerCraftMap.empty())return;
    QapDev::BatchScope Scope(qDev);
    vec2d mpos=kb.MousePos;
    qDev.BindTex(0,nullptr);
    auto cs=10.0;
    for(int y=0;y<80;y++)for(int x=0;x<80;x++){
      qDev.color=AdlerCraftMap[x+y*80]?0xFF008000:0xff444444;
      qDev.DrawQuad(y*cs-cs*40,x*cs-cs*40,cs,cs);
    }
  }
  void Render(QapDev&qDev){
    //RenderImpl(qDev);
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
    if(!inited){
      bool ok=init_attempt();
      init_attempts++;
      if(ok){inited=true;Sys.UPS_enabled=true;Sys.ResetClock();}else{return;}
    }
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

    }
    if(runned)w.tick++;
  }
  int seed=0;
  vector<QapColor> player_colors={
    0xFFFF8080, // красный
    0xFF80FF80, // зелёный
    0xFF8080FF, // синий
    0xFFFFFF80  // жёлтый
  };
};