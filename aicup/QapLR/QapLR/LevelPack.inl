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
    gui_init();
  }
  bool Win(){return false;}
  bool Fail(){return false;}
public:
  void AddText(TextRender&TE){
    string BEG="^7";
    string SEP=" ^2: ^8";
    #define GOO(TEXT,VALUE)TE.AddText(string(BEG)+string(TEXT)+string(SEP)+string(VALUE));
    GOO("frame",IToS(frame));
    GOO("playback_speed",FToS(playback_speed));
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
  int frame=0;bool set_frame=false;
  void RenderBarV0(QapDev&qDev){
    qDev.BindTex(0,nullptr);
    vec2d mpos=kb.MousePos;
    auto n=session.ws.size();auto W=pviewport->size.x;
    int frame=Lerp<real>(0,n+1,(mpos.x+W*0.5)/W);
    vec2d beg=pviewport->get_vertex_by_dir(vec2d(-1,-1));
    vec2d end=beg+vec2d((frame<0?n:frame)*(W/n),0);
    real hc=4;real hb=hc+8;
    vec2d wh=vec2d(end.x-beg.x,hc);vec2d center=(beg+end)*0.5+vec2d(0,hb*0.5);
    qDev.color=0xFF000000;
    qDev.DrawQuad(0,center.y,W,hb);
    qDev.color=0xFF0000FF;
    qDev.DrawQuad(center.x,center.y,wh.x,wh.y);
  }
  void set_playback_speed(real speed) {
    playback_speed = speed;
    speed_scrollbar.pos = speed_to_slider(speed, get_world_type());
  }
  enum class EWorldType {
    Discrete,   // шаги фиксированы: 0–8 кадров/сек
    Continuous  // шаги масштабируются от 0 до 4*UPS
  };
  EWorldType get_world_type() const {
    // Пример: если мир использует дискретные тики с фиксированным шагом — Discrete
    // Иначе — Continuous.
    // Вы можете определить это по API или флагу в world.
    return EWorldType::Continuous;//session.world->is_discrete() ? EWorldType::Discrete : EWorldType::Continuous;
  }
  real slider_to_speed(real slider_pos, EWorldType type) {
    slider_pos = Clamp(slider_pos, 0.0, 1.0);
    if (type == EWorldType::Discrete) {
      return slider_pos * 8.0; // 0..8 FPS
    } else {
      return slider_pos * 4.0 * Sys.UPS; // 0..4*UPS FPS
    }
  }
  real speed_to_slider(real speed, EWorldType type) {
    if (type == EWorldType::Discrete) {
      return Clamp(speed / 8.0, 0.0, 1.0);
    } else {
      return Clamp(speed / (4.0 * Sys.UPS), 0.0, 1.0);
    }
  }
  double last_frame_time = 0.0;
  struct t_scrollbar {
    vec2d center;      // центр элемента
    vec2d size;        // ширина и высота (size.x — длина, size.y — толщина)
    real pos = 0.5;    // нормализованная позиция [0..1]

    // Возвращает абсолютную позицию в пикселях от левого края
    real get_pixel_pos() const { return center.x - size.x * 0.5f + pos * size.x; }

    // Установка позиции по абсолютной координате мыши
    void set_by_mouse_x(real mouse_x) {
      pos = Clamp<real>((mouse_x - (center.x - size.x * 0.5)) / size.x, 0.0, 1.0);
    }

    bool hit_test(const vec2d& mpos) const {
      return mpos.x >= center.x - size.x * 0.5f &&
             mpos.x <= center.x + size.x * 0.5f &&
             mpos.y >= center.y - size.y * 0.5f &&
             mpos.y <= center.y + size.y * 0.5f;
    }
  };

  void draw_scrollbar(QapDev& qDev, const t_scrollbar& sb,QapColor c=0xFF0000FF) {
    // Фон
    qDev.color = 0xFF000000;
    qDev.DrawQuad(sb.center.x, sb.center.y, sb.size.x, sb.size.y);

    real handle_x = sb.get_pixel_pos()-sb.pos*sb.size.x*0.5;
    qDev.color = c;
    qDev.DrawQuad(handle_x, sb.center.y, sb.pos*sb.size.x, sb.size.y-4);
  }
  struct t_button {
    vec2d center;
    vec2d size;
    bool down = false;
    bool pressed = false; // сработало при отпускании

    bool hit_test(const vec2d& mpos) const {
      return mpos.x >= center.x - size.x * 0.5f &&
             mpos.x <= center.x + size.x * 0.5f &&
             mpos.y >= center.y - size.y * 0.5f &&
             mpos.y <= center.y + size.y * 0.5f;
    }
  };

  void draw_play_pause_button(QapDev& qDev, const t_button& btn, bool is_paused) {
    qDev.color = 0xFFFFFFFF;

    if (!is_paused) {
      // Пауза: две палочки
      real w = 3.0f, gap = 4.0f, h = btn.size.y * 0.8f;
      qDev.DrawQuad(btn.center.x - gap, btn.center.y, w, h);
      qDev.DrawQuad(btn.center.x + gap, btn.center.y, w, h);
    } else {
      // Воспроизведение: треугольник
      vec2d A = btn.center + vec2d(-btn.size.x * 0.4f, -btn.size.y * 0.4f);
      vec2d B = btn.center + vec2d(-btn.size.x * 0.4f, +btn.size.y * 0.4f);
      vec2d C = btn.center + vec2d(+btn.size.x * 0.3f, 0);
      qDev.DrawTrigon(A, B, C);
    }
  }

  // Обновление кнопки — общая логика
  bool update_button(t_button& btn) {
    vec2d mpos = kb.MousePos;
    bool hovered = btn.hit_test(mpos);
    bool newstate=false;
    btn.pressed = false;
    if (kb.OnDown(mbLeft) && hovered) {
      btn.down = true;
      newstate=true;
    }
    if (!kb.Down[mbLeft]) {
      if (btn.down && hovered) {
        btn.pressed = true;newstate=true;
      }
      btn.down = false;
    }
    return newstate;
  }
  t_scrollbar frame_scrollbar;
  t_scrollbar speed_scrollbar;
  t_button play_pause_btn;
  void gui_init(){
    real bar_y = pviewport->get_vertex_by_dir(vec2d(-1, -1)).y + 8;
    real W = pviewport->size.x;

    frame_scrollbar.center = vec2d(0, bar_y);
    frame_scrollbar.size = vec2d(W-100*2, 16);

    play_pause_btn.center = vec2d(-W * 0.5 + 50, bar_y);
    play_pause_btn.size = vec2d(20, 16);

    speed_scrollbar.center = vec2d(W * 0.5 - 50, bar_y);
    speed_scrollbar.size = vec2d(80, 16);
    speed_scrollbar.pos = 0.5;
    set_playback_speed(get_world_type()==EWorldType::Continuous?Sys.UPS:8.0);
  }
  void UpdateBar(){
    if(update_button(play_pause_btn))last_frame_time=g_clock.MS()*0.001;;
    if (play_pause_btn.pressed) {
      step_by_step = !step_by_step;
    }
    EWorldType wtype = get_world_type();
    vec2d mpos = kb.MousePos;

    if (kb.Down[mbLeft]) {
      if (dragging_frame||frame_scrollbar.hit_test(mpos)) {
        dragging_frame=true;
        frame_scrollbar.set_by_mouse_x(mpos.x);
        auto n = session.ws.size();
        frame = n > 0 ? Clamp<int>(frame_scrollbar.pos * n, 0, (int)n - 1) : -1;
      }
      if (speed_scrollbar.hit_test(mpos)&&!dragging_frame) {
        speed_scrollbar.set_by_mouse_x(mpos.x);
        playback_speed = slider_to_speed(speed_scrollbar.pos, wtype);
      }
    }else{
      if(dragging_frame)last_frame_time=g_clock.MS()*0.001;
      dragging_frame = false;
    }

    // --- Автоматическое воспроизведение с поддержкой мульти-шага ---
    if (!step_by_step&&!dragging_frame && frame >= 0 && !session.ws.empty()) {
      double current_time = g_clock.MS()*0.001; // в секундах
      double target_interval = (playback_speed > 0) ? (1.0 / playback_speed) : 0.0;

      if (target_interval > 0) {
        double elapsed = current_time - last_frame_time;
        if (elapsed >= target_interval) {
          // Сколько кадров мы "должны" пройти?
          int steps = (int)(elapsed / target_interval);

          // Но не выходим за границы
          int max_possible = (int)session.ws.size() - 1 - frame;
          if (max_possible <= 0) {
            // Достигли конца
            //step_by_step = true;
          } else {
            int actual_steps = min(steps, max_possible);
            frame += actual_steps;

            // Обновляем last_frame_time с учётом реально пройденных шагов
            last_frame_time += actual_steps * target_interval;

            // Синхронизируем скроллбар
            frame_scrollbar.pos = real(frame) / session.ws.size();
          }
        }
      }
    }
    frame_scrollbar.pos = real(frame) / session.ws.size();
  }
  void RenderBar(QapDev& qDev) {
    qDev.BindTex(0, nullptr);
    draw_scrollbar(qDev, frame_scrollbar);
    draw_scrollbar(qDev, speed_scrollbar,0xFF008100);
    draw_play_pause_button(qDev, play_pause_btn, step_by_step);
  }
  real playback_speed = 1.0; // от 0 до 8
  bool dragging_frame = false; // для отслеживания drag'а (опционально, но удобно)
  void RenderImpl(QapDev&qDev){
    QapDev::BatchScope Scope(qDev);
    t_offcentric_scope scope(qDev,cam_pos,cam_dir,scale,cam_offcentric);
    vec2d mpos=kb.MousePos;
    qDev.BindTex(0,nullptr);
    //frame=-1;
    auto n=session.ws.size();auto W=pviewport->size.x;
    if(set_frame)if(kb.Down[mbLeft])frame=Lerp<real>(0,n+1,(mpos.x+W*0.5)/W);
    auto&ws=session.ws;
    if(frame<0||frame>=ws.size())frame=-1;
    lock_guard<mutex> lock(session.mtx);
    auto&world=frame<0?*session.world:*ws[frame];
    auto api=world.get_render_api_version();
    if(api==0){
      world.renderV0(qDev);
    }
    //auto&w2=frame<0||frame>=session.ws2.size()?*session.world:*session.ws2[frame];
    //auto api2=w2.get_render_api_version();
    //if(api2==0){
    //  w2.renderV0(qDev);
    //}
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
    RenderBar(qDev);
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
    auto reset=[&](){last_frame_time=g_clock.MS()*0.001;};
    if(kb.OnDown(VK_LEFT))if(frame>0){frame--;step_by_step=true;}
    if(kb.OnDown(VK_RIGHT))if(frame+1<session.ws.size()){frame++;step_by_step=true;}
    if(kb.OnDown(VK_HOME)){frame=0;reset();}
    if(kb.OnDown(VK_END)){frame=int(session.ws.size())-1;reset();}
    if(kb.OnDown('N'))set_frame=!set_frame;
    if(kb.OnDown('S')||kb.OnDown(VK_SPACE)){step_by_step=!step_by_step;reset();}
    if(kb.OnDown('U'))Sys.UPS_enabled=true;
    if(kb.OnDown('D'))Sys.UPS_enabled=false;
    if(kb.OnDown('Q')){genmap();Sys.UPS_enabled=false;}
    if(kb.OnDown(VK_ESCAPE)){Sys.NeedClose=true;}
    if(kb.OnDown(VK_F9)){reinit_the_same_level();}
    bool runned=!Win()&&!Fail();
    UpdateBar();
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
    //MapSerializeTest();
  }
  /*struct t_ball_map{
    typedef map<string,double> t_map;
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_ball_map
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(vec2d,pos,{})\
    ADD(vec2d,vel,{})\
    ADD(t_map,map,{})\
    //===
    #include "defprovar.inl"
    //===
  };
  void MapSerializeTest(){
    t_ball_map bm,am;
    bm.map["a"]=15.25;bm.map["b"]=10.5;
    auto s=QapSaveToStr(bm);
    QapLoadFromStr(am,s);
    int gg=1;
  }*/
public:
  bool step_by_step=false;
};