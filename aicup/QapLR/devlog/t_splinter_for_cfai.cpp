/*---
2025.08.29 13:08:30.001
реализуй dna так чтобы get_dna_score вернула как можно большее число. вот код симулятора:
*/
#include <vector>
#include <algorithm>
using namespace std;
typedef double real;
template<typename TYPE>inline TYPE Lerp(TYPE A,TYPE B,real v){return A+(B-A)*v;}
template<class TYPE>inline TYPE Clamp(TYPE v,TYPE a,TYPE b){return max(a,min(v, b));}
const real Pi=3.14159265;
const real Pi2=Pi*2;
const real PiD2=Pi/2;
const real PiD4=Pi/4;
//template<typename TYPE>TYPE Sign(const TYPE&value){if(value>0){return TYPE(+1);}else{return TYPE(value<0?-1:0);};};
//template<typename TYPE>bool InDip(const TYPE&&min,const TYPE&&val,const TYPE&&max){return (val>=min)&&(val<=max);};
//template<typename TYPE,typename TYPEB,typename TYPEC>bool InDip(const TYPE&&min,const TYPEB&&val,const TYPEC&&maxC){return (val>=min)&&(val<=max);};
template<typename TYPE,typename TYPEB,typename TYPEC>bool InDip(const TYPE min,const TYPEB val,const TYPEC max){return (val>=min)&&(val<=max);};
template<class TYPE>static TYPE&vec_add_back(vector<TYPE>&arr){arr.resize(arr.size()+1);return arr.back();}
template<class TYPE>static TYPE&qap_add_back(vector<TYPE>&arr){arr.resize(arr.size()+1);return arr.back();}
template<typename TYPE>TYPE Sign(TYPE value){if(value>0){return TYPE(+1);}else{return TYPE(value<0?-1:0);}};
struct vec2d{
public:
  real x,y;
  vec2d():x(0),y(0){}
  vec2d(real x,real y):x(x),y(y){}
  vec2d(const vec2d&v):x(v.x),y(v.y){}
  vec2d&operator=(const vec2d&v){x=v.x;y=v.y;return *this;}
  vec2d operator+()const{return *this;}
  vec2d operator-()const{return vec2d(-x,-y);}
  vec2d&operator+=(const vec2d&v){x+=v.x;y +=v.y;return *this;}
  vec2d&operator-=(const vec2d&v){x-=v.x; y-=v.y;return *this;}
  vec2d&operator*=(const real&f){x*=f;y*=f;return *this;}
  vec2d&operator/=(const real&f){x/=f;y/=f;return *this;}
public:
  vec2d Rot(const vec2d&OX)const{real M=OX.Mag();return vec2d(((x*+OX.x)+(y*OX.y))/M,((x*-OX.y)+(y*OX.x))/M);}
  vec2d UnRot(const vec2d&OX)const{real M=OX.Mag();if(M==0.0f){return vec2d(0,0);};return vec2d(((x*OX.x)+(y*-OX.y))/M,((x*OX.y)+(y*+OX.x))/M);}
  vec2d Ort()const{return vec2d(-y,x);}
  vec2d Norm()const{if((x==0)&&(y==0)){return vec2d(0,0);}return vec2d(x/this->Mag(),y/this->Mag());}
  vec2d SetMag(const real&val)const{return this->Norm()*val;}
  vec2d Mul(const vec2d&v)const{return vec2d(x*v.x,y*v.y);}
  vec2d Div(const vec2d&v)const{return vec2d(v.x!=0?x/v.x:x,v.y!=0?y/v.y:y);}
  real GetAng()const{return atan2(y,x);}
  real Mag()const{return sqrt(x*x+y*y);}
  real SqrMag()const{return x*x+y*y;}
public:
  friend vec2d operator+(const vec2d&u,const vec2d&v){return vec2d(u.x+v.x,u.y+v.y);}
  friend vec2d operator-(const vec2d&u,const vec2d&v){return vec2d(u.x-v.x,u.y-v.y);}
  friend vec2d operator*(const vec2d&u,const real&v){return vec2d(u.x*v,u.y*v);}
  friend vec2d operator*(const real&u,const vec2d&v){return vec2d(u*v.x,u*v.y);}
  friend bool operator==(const vec2d&u,const vec2d&v){return (u.x==v.x)||(u.y==v.y);}
  friend bool operator!=(const vec2d&u,const vec2d&v){return (u.x!=v.x)||(u.y!=v.y);}
public:
  //friend inline static vec2d Vec2dEx(const real&ang,const real&mag){return vec2d(cos(ang)*mag,sin(ang)*mag);}
public:
  #ifdef BOX2D_H
    operator b2Vec2()const{return b2Vec2(x,y);}
    vec2d(const b2Vec2& v):x(v.x),y(v.y){}
  #endif
public:
  real dist_to(const vec2d&p)const{return (p-*this).Mag();}
  real sqr_dist_to(const vec2d&p)const{return (p-*this).SqrMag();}
  bool dist_to_point_less_that_r(const vec2d&p,real r)const{return (p-*this).SqrMag()<r*r;}
public:
  bool dist_to_point_more_that_r(const vec2d&p,real r)const{return (p-*this).SqrMag()>r*r;}
  bool sqrdist_to_point_more_that_rr(const vec2d&p,real rr)const{return (p-*this).SqrMag()>rr;}
  bool sqrdist_to_point_less_that_rr(const vec2d&p,real rr)const{return (p-*this).SqrMag()<rr;}
public:
  static vec2d min(const vec2d&a,const vec2d&b){
    return vec2d(std::min(a.x,b.x),std::min(a.y,b.y));
  }
  static vec2d max(const vec2d&a,const vec2d&b){
    return vec2d(std::max(a.x,b.x),std::max(a.y,b.y));
  }
  static void comin(vec2d&a,const vec2d&b){a=min(a,b);}
  static void comax(vec2d&a,const vec2d&b){a=max(a,b);}
  static vec2d sign(const vec2d&p){return vec2d(Sign(p.x),Sign(p.y));}
};
inline vec2d Vec2dEx(const real&ang,const real&mag){return vec2d(cos(ang)*mag,sin(ang)*mag);}
inline int round(const real&val){return int(val>=0?val+0.5:val-0.5);}//{return int(val);}
inline static real dot(const vec2d&a,const vec2d&b){return a.x*b.x+a.y*b.y;}
inline static real cross(const vec2d&a,const vec2d&b){return a.x*b.y-a.y*b.x;}
template<class VECTOR_TYPE>
void qap_clean_if_deaded(VECTOR_TYPE&arr)
{
  size_t last=0;auto arr_size=arr.size();
  for(size_t i=0;i<arr_size;i++)
  {
    auto&ex=arr[i];
    if(ex.deaded)continue;
    if(last!=i)
    {
      auto&ax=arr[last];
      ax=std::move(ex);
    }
    last++;
  }
  if(last==arr.size())return;
  arr.resize(last);
}
double Dist2Line(const vec2d&point,const vec2d&a,const vec2d&b){
  auto p=(point-a).Rot(b-a);
  if(p.x<0||p.x>(b-a).Mag())return 1e9;
  return fabs(p.y);
}

struct t_ball {
    vec2d pos, vel;
    double mass = 1.0;
};

struct t_point{
    vec2d pos,v;bool deaded=false;int color=0;
};

struct t_spring {
    int a, b;                    // индексы шариков
    double rest_length;          // текущая целевая длина
    double k = 0.0351;//*3*3;//*2*2*0.02;              // жёсткость 0.1
};

const double ARENA_RADIUS = 512.0;
const double CENTER_RADIUS = 5.0;

struct t_splinter_world{
    static const int NUM_PLAYERS = 4;
    static const int BALLS_PER = 3;
    static const int MAX_TICK = 500;
    vector<size_t> deaded;
    vector<t_point> parr;
    vector<t_ball> balls;        // 12 шариков (4 игрока ? 3)
    vector<t_spring> springs;    // 12 пружин (4 ? 3)
    vector<double> slot2score;
    int tick = 0;
    
    struct t_cmd {
        double spring0_new_rest, spring1_new_rest, spring2_new_rest;
        double friction[3]={0.50,0.50,0.50}; // 0.1 — очень скользко, 0.99 — тормозит сильно
        bool valid() const {
            return spring0_new_rest >= 5 && spring0_new_rest <= 100 &&
                   spring1_new_rest >= 5 && spring1_new_rest <= 100 &&
                   spring2_new_rest >= 5 && spring2_new_rest <= 100 &&
                   friction[0] >= 0.01 && friction[0] <= 0.99 &&
                   friction[1] >= 0.01 && friction[1] <= 0.99 &&
                   friction[2] >= 0.01 && friction[2] <= 0.99;
        }
    };
    vector<t_cmd> cmd_for_player;
    void use(int player_id, const t_cmd& c) {
        auto cmd=c;
        auto*s=&cmd.spring0_new_rest;auto*f=cmd.friction;
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
    void step() {
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
            auto rl=deaded[s.a/3]?100:s.rest_length;
            double force = s.k * (dist - rl);
            forces[s.a] += dir * force;
            forces[s.b] -= dir * force; // противоположная сила
        }
        for(int i=0;i<4;i++)if(deaded[i])for(int s=0;s<3;s++)cmd_for_player[i].friction[s]=0.5;
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
            double friction=df*cmd_for_player[player_id].friction[ball_in_player];
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
          }
          for(int j=0;j<parr.size();j++){
            auto&b=parr[j];
            if(!a.pos.sqrdist_to_point_less_that_rr(b.pos,dd))continue;
            if(b.color==i/3){b.deaded=true;slot2score[b.color]++;continue;}
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
          auto p=ex.a/3;if(deaded[p])continue;
          auto a=balls[ex.a].pos;auto b=balls[ex.b].pos;
          for(int i=0;i<balls.size();i++){
            if(i/3==p||deaded[i/3])continue;
            auto&ex=balls[i];
            if(Dist2Line(ex.pos,a,b)>8)continue;
            deaded[p]=true;
            slot2score[i/3]+=64;
          }
        }
        int n=0;
        for(auto&ex:deaded){
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
      b.pos=(vec2d(rand()/double(RAND_MAX),rand()/double(RAND_MAX))-vec2d(0.5,0.5))*2*ARENA_RADIUS;b.color=i%4;
      if(b.pos.Mag()>ARENA_RADIUS){i--;w.parr.pop_back();return false;}
      return true;
    }
};
typedef t_splinter_world t_world;
void init_world(t_world&world) {
    // Очищаем
    world.deaded.assign(4,0);
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
        world.balls.push_back({starts[p] + vec2d(-10, 0),  {}});
        world.balls.push_back({starts[p] + vec2d(+10, 0),  {}});
        world.balls.push_back({starts[p] + vec2d(  0, 15), {}});

        // Пружины: 0-1, 1-2, 2-0
        world.springs.push_back({base_idx + 0, base_idx + 1, REST_LENGTH});
        world.springs.push_back({base_idx + 1, base_idx + 2, REST_LENGTH});
        world.springs.push_back({base_idx + 2, base_idx + 0, REST_LENGTH});
    }
    int n=32*4*2*2;//2;
    for(int i=0;i<n;i++){
      world.add_ball(world,i);
    }
}
t_world::t_cmd dna(const t_world&w,int player_id){
  return {};
}
t_world::t_cmd edna(const t_world&w,int player_id){return {};};
int get_dna_score(){
  t_world w;
  init_world(w);
  for(int iter=0;!w.finished_flag;iter++){
    w.use(0,dna(w,0));
    for(int i=1;i<4;i++)w.use(i,edna(w,i));
    w.step();
  }
  return w.slot2score[0];
}
int main(){
  return 0*get_dna_score(); // просто сделал чтобы проверить что компилируется.
}
/*/
---
2026.02.07 16:51:00.884
*/