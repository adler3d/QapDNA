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
    ADD(int,your_id,0)\
    ADD(int64_t,tick,0)\
    ADD(int64_t,ticks,20000)\
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
      void step(std::mt19937&gen,bool with_score_from_points=true,bool with_pointsgen=true) {
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
          for(int i=0;i<slot2score.size();i++)if(slot2deaded[i])cmd_for_player[i]={};
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
          finished_flag=n<=1||tick>=ticks;
          vector<int> p2n(slot2score.size());
          for(auto&ex:parr){
            p2n[ex.color]++;
          }
          for(int i=0;i<slot2score.size();i++){
            if(!p2n[i]){
              finished_flag=true;
            }
          }
          if(with_pointsgen){
            for(int i=0;i<slot2score.size();i++){
              if(p2n[i]<32){
                for(int i=0;i<slot2score.size();i++)add_point(*this,i,gen);
              }
            }
          }
      }
      static bool add_point(t_world&w,int i,std::mt19937&gen){
        auto r=[&gen](){static std::uniform_real_distribution<double>dist(-1.0,1.0);auto x=dist(gen);return vec2d{x,dist(gen)};};
        auto&b=qap_add_back(w.parr);
        b.pos=r()*w.ARENA_RADIUS;b.color=i%w.slot2score.size();
        if(b.pos.Mag()>w.ARENA_RADIUS){i--;w.parr.pop_back();return false;}
        return true;
      };
//#include <string>
//#include <sstream>
//#include <iomanip>

static std::string compare_t_ball(const t_ball& a, const t_ball& b) {
    #define CMP(field) if (a.field != b.field) return #field;
    CMP(pos.x); CMP(pos.y);
    CMP(vel.x); CMP(vel.y);
    CMP(mass);
    return "";
}
#undef CMP
static std::string compare_t_point(const t_point& a, const t_point& b) {
    #define CMP(field) if (a.field != b.field) return #field;
    CMP(pos.x); CMP(pos.y);
    CMP(v.x);   CMP(v.y);
    CMP(deaded);
    CMP(color);
    return "";
}
#undef CMP
static std::string compare_t_spring(const t_spring& a, const t_spring& b) {
    #define CMP(field) if (a.field != b.field) return #field;
    CMP(a);
    CMP(b);
    CMP(rest_length);
    CMP(k);
    return "";
}
#undef CMP
static std::string compare_t_cmd(const t_cmd& a, const t_cmd& b) {
    #define CMP(field) if (a.field != b.field) return #field;
    CMP(spring0_new_rest);
    CMP(spring1_new_rest);
    CMP(spring2_new_rest);
    CMP(friction0);
    CMP(friction1);
    CMP(friction2);
    return "";
}
#undef CMP
static std::string compare_int(const int& a, const int& b) {
    if (a!= b) return "#";
    return "";
}
static std::string compare_double(const double& a, const double& b) {
    if (a!= b) return "#";
    return "";
}
static std::string compare_worlds(const t_world& a, const t_world& b) {
    std::ostringstream path;
// Сравнение примитивов
    #define CMP(field) \
        if (a.field != b.field) { \
            path << #field; \
            return path.str(); \
        }

    // Сравнение векторов с индексами
    #define CMP_VECTOR(vec, type) \
        if (a.vec.size() != b.vec.size()) { \
            path << #vec "[size]"; \
            return path.str(); \
        } \
        for (size_t i = 0; i < a.vec.size(); ++i) { \
            std::ostringstream subpath; \
            subpath << #vec "[" << i << "]"; \
            auto diff = compare_ ## type(a.vec[i], b.vec[i]); \
            if (!diff.empty()) { \
                path << subpath.str() << "." << diff; \
                return path.str(); \
            } \
        }

    // Сравнение вложенных структур
    #define CMP_STRUCT(field, type) \
        { \
            auto diff = compare_ ## type(a.field, b.field); \
            if (!diff.empty()) { \
                path << #field << "." << diff; \
                return path.str(); \
            } \
        }

    // === Примитивные поля t_world ===
    CMP(your_id);
    CMP(tick);
    CMP(ARENA_RADIUS);

    // === Вектора ===
    CMP_VECTOR(balls, t_ball);
    CMP_VECTOR(springs, t_spring);
    CMP_VECTOR(parr, t_point);
    CMP_VECTOR(slot2deaded, int);
    CMP_VECTOR(slot2score, double);
    CMP_VECTOR(cmd_for_player, t_cmd);

    // Если всё совпадает
    return "";
#undef CMP
}
  };
  static void init_world(t_world&world,std::mt19937&gen,uint32_t num_players) {
    // Очищаем
    world.slot2deaded.assign(num_players,0);
    world.balls.clear();
    world.springs.clear();
    world.slot2score.assign(num_players, 0);
    world.tick = 0;

    // Углы для num_players игроков
    vector<vec2d> starts = {
      vec2d(-70, -70),  // лево-низ
      vec2d(+70, -70),  // право-низ
      vec2d(+70, +70),  // право-верх
      vec2d(-70, +70)   // лево-верх
    };
    starts.resize(num_players);

    const double REST_LENGTH = 30.0;
    
    auto r=[&gen](){static std::uniform_real_distribution<double>dist(-1.0,1.0);auto x=dist(gen);return vec2d{x,dist(gen)};};
    world.cmd_for_player.resize(num_players);
    for (int p = 0; p < num_players; p++) {
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
    int n=num_players<=4?32*num_players*2*2:num_players*32;
    for(int i=0;i<n;i++){
      t_world::add_point(world,i,gen);
    }
  }
  #ifdef I_WORLD
  struct t_world_impl:i_world{
    t_world w;
    std::mt19937 gen;
    ~t_world_impl(){};
    void use(int player,const string&cmd,string&outmsg)override{
      t_world::t_cmd out;
      bool ok=cmd.size()&&QapLoadFromStr(out,cmd);
      if(!ok){outmsg="cmd load failed";return;}else{outmsg.clear();}
      w.use(player,out);
    }
    void step()override{w.step(gen);}
    bool finished()override{return w.finished_flag;}
    void get_score(vector<double>&out)override{out=w.slot2score;}
    void is_alive(vector<int>&out)override{out.resize(w.slot2deaded.size());for(int i=0;i<out.size();i++)out[i]=!w.slot2deaded[i];}
    void get_vpow(int player,string&out)override{
      w.your_id=player;
      out=QapSaveToStr(w);
    }
    void init(uint32_t seed,uint32_t num_players,int64_t ticks)override{
      gen=mt19937(seed);
      init_world(w,gen,num_players);
      if(ticks>=0)w.ticks=ticks;
    }
    int init_from_config(const string&cfg,string&outmsg)override{
      outmsg="no impl";
      return 0;
    }
    string diff(const string&vpow)override{
      t_world w2;
      QapLoadFromStr(w2,vpow);
      return t_world::compare_worlds(w,w2);
    }
    unique_ptr<i_world> clone()override{return make_unique<t_world_impl>(*this);}
    void renderV0(QapDev&qDev)override{
      static vector<QapColor> player_colors = {
        // Ваши исходные
        0xFFFF8080, // красный
        0xFF80FF80, // зелёный
        0xFF8080FF, // синий
        0xFFFFFF80, // жёлтый

        // Дополнительные — 28 штук
        0xFFFF80FF, // розовый (magenta)
        0xFF80FFFF, // голубой (cyan)
        0xFFFFFF00, // чистый жёлтый (немного ярче)
        0xFFFF0000, // чистый красный
        0xFF00FF00, // чистый зелёный
        0xFF0000FF, // чистый синий
        0xFF800080, // тёмная маджента
        0xFF008080, // тёмный голубой (бирюзовый)
        0xFF808000, // оливковый
        0xFFFFA500, // оранжевый
        0xFFDA70D6, // орхидея
        0xFF9370DB, // средний фиолетовый
        0xFF4B0082, // индиго
        0xFF483D8B, // тёмный сланцевый серо-синий
        0xFF00CED1, // тёмный бирюзовый
        0xFF2E8B57, // морская волна
        0xFF8B4513, // седло-коричневый
        0xFFA0522D, // сиена
        0xFFCD853F, // перу
        0xFFD2691E, // горький шоколад
        0xFFB22222, // кирпичный красный
        0xFF8B0000, // тёмно-красный
        0xFF228B22, // тёмно-зелёный
        0xFF006400, // тёмный зелёный лесной
        0xFF191970, // тёмно-синий (midnight blue)
        0xFF000080, // тёмно-синий (navy)
        0xFF4682B4, // стальной синий
        0xFF6495ED, // василёк
        0xFF7B68EE, // средний сланцевый синий
        0xFFBA55D3, // умеренный орхидейный
        0xFFFF6347, // томатный
        0xFF32CD32  // лаймовый зелёный
      };
      check_it(3013710);
      while(player_colors.size()<w.slot2score.size()){
        player_colors.push_back(QapColor(255,rand()%256,rand()%256,rand()%256));
      }
      check_it(3013711);
      auto&world=w;
      // --- Рисуем арену ---
      qDev.SetColor(0x40000000);
      qDev.DrawCircle(vec2d{}, world.ARENA_RADIUS,0,2, 64);
      
      check_it(3013712);
      // --- Рисуем цель ---
      qDev.SetColor(0x8000FF00); // полупрозрачный зелёный
      qDev.DrawCircle(vec2d{}, 5,0,2, 32);
      
      check_it(3013713);
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
      check_it(3013714);
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
      
      check_it(3013715);
      for(int i=0;i<world.parr.size();i++){
        auto&ex=world.parr[i];
        qDev.SetColor(player_colors[ex.color]);
        qDev.DrawCircleEx(ex.pos, 0, 8.0, 10, 0);
      }
      
      check_it(3013716);
      // --- Рисуем шарики ---
      for (int p = 0; p < world.slot2score.size(); p++) {
          for (int i = 0; i < 3; i++) {
              int idx = p * 3 + i;
              const auto& ball = world.balls[idx];
              check_it(30137171);
              auto deaded=world.slot2deaded[p];
              check_it(301371712);
              auto c=player_colors[p];
              check_it(301371713);
              qDev.SetColor(deaded?0x40000000:c&0xAAFFFFFF);
              check_it(30137172);
              auto&v=world.cmd_for_player[p].f(i);
              check_it(301371721);
              if(qap_check_id(world.cmd_for_player,p)&&i<3){
                qDev.DrawCircleEx(ball.pos, 0, 8.0+(v*4), 32, 0);
              }
              check_it(30137173);
              qDev.SetColor(0xff000000);
              check_it(30137174);
              qDev.DrawCircleEx(ball.pos, 8.0, 9.0, 32, 0);
              check_it(30137175);
          }
      }
      check_it(3013717);
      qDev.SetColor(0xFFFFFFFF);
    }
    int get_tick()override{return w.tick;}
  };
  static unique_ptr<i_world> mk_world(int version){
    return make_unique<t_world_impl>();
  }
  #endif
};