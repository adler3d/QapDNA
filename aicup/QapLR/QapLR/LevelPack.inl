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

#define I_WORLD
#include "t_splinter.inl"

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
    this->pviewport=&Game->qDev.viewport;pviewport->size=vec2d{real(Sys.SM.W),real(Sys.SM.H)};
    this->Game=Game;
  }
  bool Win(){return false;}
  bool Fail(){return false;}
public:
  void AddText(TextRender&TE){
    string BEG="^7";
    string SEP=" ^2: ^8";
    #define GOO(TEXT,VALUE)TE.AddText(string(BEG)+string(TEXT)+string(SEP)+string(VALUE));
    GOO("curr_t",FToS(session.world->get_tick()*1.0/Sys.UPS));
    GOO("seed",IToS(g_args.seed_initial));
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
  int frame=-1;bool set_frame=false;
  void RenderImpl(QapDev&qDev){
    QapDev::BatchScope Scope(qDev);
    t_offcentric_scope scope(qDev,cam_pos,cam_dir,scale,cam_offcentric);
    vec2d mpos=kb.MousePos;
    qDev.BindTex(0, nullptr);
    frame=-1;
    if(set_frame)frame=mpos.x+pviewport->size.x/2;
    auto&ws=session.ws;
    if(frame<0||frame>=ws.size())frame=-1;
    lock_guard<mutex> lock(session.mtx);
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
    if(kb.OnDown('N'))set_frame=!set_frame;
    if(kb.OnDown('S'))step_by_step=!step_by_step;
    if(kb.OnDown('U'))Sys.UPS_enabled=true;
    if(kb.OnDown('D'))Sys.UPS_enabled=false;
    if(kb.OnDown('Q')){genmap();Sys.UPS_enabled=false;}
    if(kb.OnDown(VK_ESCAPE)){Sys.NeedClose=true;}
    if(kb.OnDown(VK_F9)){reinit_the_same_level();}
    bool runned=!Win()&&!Fail();
    //if(runned){
    //  bool done=false;
    //  while(session.try_step()){
    //    ws.push_back(session.world->clone());
    //    done=session.is_finished();
    //    if(done){
    //      break;
    //    }
    //    session.send_vpow_to_all();
    //    if(step_by_step)break;
    //  }
    //  if(done){
    //    string report=session.generate_report();
    //    cerr<<report;
    //    for(auto&ex:session.carr)if(ex)ex->off();
    //    //Sys.NeedClose = true;
    //  }
    //}
    UpdateOffcentricScope(kb);
  }
public:
  bool step_by_step=true;
};