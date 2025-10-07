#define LEVEL_LIST(F)F(Level_SplinterWorld);
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
class Level_SplinterWorld:public TGame::ILevel{
public:
  struct t_world{int t=0;};
public:
  t_world w;
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
    GOO("curr_t",FToS(w.t*1.0/Sys.UPS));
    GOO("seed",IToS(seed));
    GOO("init_attempts",IToS(init_attempts));
    TE.AddText("^7---");
    int bx=TE.bx;
    int cy=TE.y;
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
    RenderHint(qDev,TE);
    TE.EndScope();
  }
  void RenderUI(QapDev&qDev,TextRender&TE)
  {
  }
  void Render(QapDev&qDev){

    RenderText(qDev);
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
    {
      QapDev*pQapDev=nullptr;
      TextRender TE(pQapDev);TE.dummy=true;
      vec2d hs=vec2d(Sys.SM.W,Sys.SM.H)*0.5;
      hs.x=100;
      real ident=24.0;
      real Y=0;
      TE.BeginScope(-hs.x+ident,+hs.y-ident,&Game->NormFont,&Game->BlurFont);
      TE.EndScope();
    }
    if(kb.OnDown(VK_F9)){reinit_the_same_level();}
    bool runned=!Win()&&!Fail();
    if(runned){

    }
    if(runned)w.t++;
  }
  int seed=0;
};