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
      void step(std::mt19937&gen,bool with_score_from_points=true,bool with_pointsgen=true) {
          tick++;

          // ����� ���
          vector<vec2d> forces(balls.size());

          // ������ ��� ������
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
              forces[s.b] -= dir * force; // ��������������� ����
          }
          for(int i=0;i<slot2score.size();i++)if(slot2deaded[i])cmd_for_player[i]={};
          // ���������� �������� � ������� (������� ����������)
          for (int i = 0; i < balls.size(); i++) {
              auto& b = balls[i];
              b.vel += forces[i] *(1.0/(b.mass * 1.0));
              // ������ ����������� ������:
              // b.vel = b.vel * 0.98;

              // ������ �� ������:
              int player_id = i / 3;
              int ball_in_player = i % 3;
              auto t=(tick+3000)*Pi2/4000.0;
              auto s=sin(t)*0.5+0.5;//0->0->0.5->0.95;1->
              auto p=pow(s,2);
              double df=1.0-Lerp(0.05,1.0,p);//tick%1000>500?0.05:1;//0.05*20.1/20;
              double friction=df*cmd_for_player[player_id].f(ball_in_player);
              b.vel*=1.0-friction;
              b.pos+=b.vel*1.0;

              // ������ �� ������ (����� � ����)
              vec2d&to_center = b.pos;auto r=ARENA_RADIUS-8;
              double d = to_center.Mag();
              if (d > r) {
                  vec2d n = to_center.Norm();
                  b.pos = n * r;
                  // ��������� ��������
                  //double vn = b.vel.x * n.x + b.vel.y * n.y;
                  //b.vel = {b.vel.x - 2*vn*n.x, b.vel.y - 2*vn*n.y};
                  //b.vel *= 0.5; // ����� ��� �����
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
        auto r=[&gen](){static std::uniform_real_distribution<double>dist(-1.0,1.0);return vec2d{dist(gen),dist(gen)};};
        auto&b=qap_add_back(w.parr);
        b.pos=r()*w.ARENA_RADIUS;b.color=i%w.slot2score.size();
        if(b.pos.Mag()>w.ARENA_RADIUS){i--;w.parr.pop_back();return false;}
        return true;
      };
  };
  static void init_world(t_world&world,std::mt19937&gen,uint32_t num_players) {
    // �������
    world.slot2deaded.assign(num_players,0);
    world.balls.clear();
    world.springs.clear();
    world.slot2score.assign(num_players, 0);
    world.tick = 0;

    // ���� ��� num_players �������
    vector<vec2d> starts = {
      vec2d(-70, -70),  // ����-���
      vec2d(+70, -70),  // �����-���
      vec2d(+70, +70),  // �����-����
      vec2d(-70, +70)   // ����-����
    };
    starts.resize(num_players);

    const double REST_LENGTH = 30.0;
    
    auto r=[&gen](){static std::uniform_real_distribution<double>dist(-1.0,1.0);return vec2d{dist(gen),dist(gen)};};
    world.cmd_for_player.resize(num_players);
    for (int p = 0; p < num_players; p++) {
      int base_idx = world.balls.size();

      // ��� ������ � ����� ������������
      starts[p]=r()*500;
      auto add=[&](vec2d pos){qap_add_back(world.balls).pos=pos;};
      add(starts[p] + r()*15 + 0*vec2d(-10,  0));
      add(starts[p] + r()*15 + 0*vec2d(+10,  0));
      add(starts[p] + r()*15 + 0*vec2d(  0, 15));
        
      auto adds=[&](int x,int y){auto&b=qap_add_back(world.springs);b.a=x;b.b=y;b.rest_length=REST_LENGTH;};
      // �������: 0-1, 1-2, 2-0
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
      bool ok=QapLoadFromStr(out,cmd);
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
    void init(uint32_t seed,uint32_t num_players)override{
      gen=mt19937(seed);
      init_world(w,gen,num_players);
    }
    int init_from_config(const string&cfg,string&outmsg)override{
      outmsg="no impl";
      return 0;
    }
    unique_ptr<i_world> clone()override{return make_unique<t_world_impl>(*this);}
    void renderV0(QapDev&qDev)override{
      static vector<QapColor> player_colors = {
        // ���� ��������
        0xFFFF8080, // �������
        0xFF80FF80, // ������
        0xFF8080FF, // �����
        0xFFFFFF80, // �����

        // �������������� � 28 ����
        0xFFFF80FF, // ������� (magenta)
        0xFF80FFFF, // ������� (cyan)
        0xFFFFFF00, // ������ ����� (������� ����)
        0xFFFF0000, // ������ �������
        0xFF00FF00, // ������ ������
        0xFF0000FF, // ������ �����
        0xFF800080, // ����� ��������
        0xFF008080, // ����� ������� (���������)
        0xFF808000, // ���������
        0xFFFFA500, // ���������
        0xFFDA70D6, // �������
        0xFF9370DB, // ������� ����������
        0xFF4B0082, // ������
        0xFF483D8B, // ����� ��������� ����-�����
        0xFF00CED1, // ����� ���������
        0xFF2E8B57, // ������� �����
        0xFF8B4513, // �����-����������
        0xFFA0522D, // �����
        0xFFCD853F, // ����
        0xFFD2691E, // ������� �������
        0xFFB22222, // ��������� �������
        0xFF8B0000, // ����-�������
        0xFF228B22, // ����-������
        0xFF006400, // ����� ������ ������
        0xFF191970, // ����-����� (midnight blue)
        0xFF000080, // ����-����� (navy)
        0xFF4682B4, // �������� �����
        0xFF6495ED, // ������
        0xFF7B68EE, // ������� ��������� �����
        0xFFBA55D3, // ��������� ����������
        0xFFFF6347, // ��������
        0xFF32CD32  // �������� ������
      };
      check_it(3013710);
      while(player_colors.size()<w.slot2score.size()){
        player_colors.push_back(QapColor(255,rand()%256,rand()%256,rand()%256));
      }
      check_it(3013711);
      auto&world=w;
      // --- ������ ����� ---
      qDev.SetColor(0x40000000);
      qDev.DrawCircle(vec2d{}, world.ARENA_RADIUS,0,2, 64);
      
      check_it(3013712);
      // --- ������ ���� ---
      qDev.SetColor(0x8000FF00); // �������������� ������
      qDev.DrawCircle(vec2d{}, 5,0,2, 32);
      
      check_it(3013713);
      // --- ������ ������� ---
      for (const auto& spring : world.springs) {
          const auto& a = world.balls[spring.a].pos;
          const auto& b = world.balls[spring.b].pos;
          int player_id = spring.a / 3; // ���������� ������ �� ������� ������
          qDev.SetColor(player_colors[player_id] & 0x80FFFFFF); // ��������������
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
      // --- ������ ������ ---
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
              //auto&v=world.cmd_for_player[p].f(i);
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