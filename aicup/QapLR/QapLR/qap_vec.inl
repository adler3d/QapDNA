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
typedef unsigned char uchar;
class QapColor{
public:
  typedef QapColor SelfClass;
  uchar b,g,r,a;
  QapColor():b(255),g(255),r(255),a(255){}
  QapColor(uchar A,uchar R,uchar G,uchar B):a(A),r(R),g(G),b(B){}
  QapColor(uchar R,uchar G,uchar B):a(255),r(R),g(G),b(B){}
  QapColor(const QapColor& v):a(v.a),r(v.r),g(v.g),b(v.b){}
  QapColor(const unsigned int&v){((unsigned int&)*this)=v;}
  bool operator==(const QapColor&v)const{return (a==v.a)&&(r==v.r)&&(g==v.g)&&(b==v.b);}
  QapColor&operator=(const QapColor&v){a=v.a; r=v.r; g=v.g; b=v.b; return *this;}
  QapColor operator+()const{return *this;}
  QapColor&operator*=(const QapColor&v){
    a=Clamp(int(a)*int(v.a)/255,0,255);
    r=Clamp(int(r)*int(v.r)/255,0,255);
    g=Clamp(int(g)*int(v.g)/255,0,255);
    b=Clamp(int(b)*int(v.b)/255,0,255);
    return *this;
  }
  QapColor&operator+=(const QapColor&v){
    a=Clamp(int(a)+int(v.a),0,255);
    r=Clamp(int(r)+int(v.r),0,255);
    g=Clamp(int(g)+int(v.g),0,255);
    b=Clamp(int(b)+int(v.b),0,255);
    return *this;
  }
  QapColor&operator-=(const QapColor&v){
    a=Clamp(int(a)-int(v.a),0,255);
    r=Clamp(int(r)-int(v.r),0,255);
    g=Clamp(int(g)-int(v.g),0,255);
    b=Clamp(int(b)-int(v.b),0,255);
    return *this;
  }
  QapColor operator*(const QapColor&v)const{
    return QapColor(int(a)*int(v.a)/255,int(r)*int(v.r)/255,int(g)*int(v.g)/255,int(b)*int(v.b)/255);
  }
  QapColor operator+(const QapColor&v)const{
    return QapColor(Clamp(int(a)+int(v.a),0,255),Clamp(int(r)+int(v.r),0,255),Clamp(int(g)+int(v.g),0,255),Clamp(int(b)+int(v.b),0,255));
  }
  QapColor operator-(const QapColor&v)const{
    return QapColor(Clamp(int(a)-int(v.a),0,255),Clamp(int(r)-int(v.r),0,255),Clamp(int(g)-int(v.g),0,255),Clamp(int(b)-int(v.b),0,255));
  }
  QapColor&operator*=(real f){
    b=uchar(Clamp(real(b)*f,0.0,255.0));
    g=uchar(Clamp(real(g)*f,0.0,255.0));
    r=uchar(Clamp(real(r)*f,0.0,255.0));
    return *this;
  }
  QapColor&operator/=(real r){
    real f=1.0/r;
    b=uchar(Clamp(real(b)*f,0.0,255.0));
    g=uchar(Clamp(real(g)*f,0.0,255.0));
    r=uchar(Clamp(real(r)*f,0.0,255.0));
    return *this;
  }
  operator unsigned int&()const{return *(unsigned int*)this;}
  uchar GetLuminance()const{return (int(r)+int(g)+int(b))/3;}
  QapColor toGray()const{auto l=GetLuminance(); return QapColor(a,l,l,l);}
  inline static QapColor Mix(const QapColor&A,const QapColor&B,const real&t){
    real ct=Clamp(t,0.0,1.0), tA=1.0-ct, tB=ct;
    QapColor O;
    O.b=uchar(Clamp(real(A.b)*tA+real(B.b)*tB,0.0,255.0));
    O.g=uchar(Clamp(real(A.g)*tA+real(B.g)*tB,0.0,255.0));
    O.r=uchar(Clamp(real(A.r)*tA+real(B.r)*tB,0.0,255.0));
    O.a=uchar(Clamp(real(A.a)*tA+real(B.a)*tB,0.0,255.0));
    return O;
  }
  inline static QapColor HalfMix(const QapColor&A,const QapColor&B){
    QapColor O;
    O.b=(int(A.b)+int(B.b))>>1;
    O.g=(int(A.g)+int(B.g))>>1;
    O.r=(int(A.r)+int(B.r))>>1;
    O.a=(int(A.a)+int(B.a))>>1;
    return O;
  }
  inline QapColor inv_rgb()const{return QapColor(a,0xff-r,0xff-g,0xff-b);}
  inline QapColor swap_rg()const{return QapColor(a,b,g,r);}
};
class vec2f{
public:
  typedef vec2f SelfClass;
  float x,y;
  vec2f():x(0),y(0){}
  vec2f(const vec2d&v):x(v.x),y(v.y){}
  vec2f(float x,float y):x(x),y(y){}
  void set_zero(){x=0.0f;y=0.0f;}
  friend vec2f operator*(const vec2f&u,const float&v){return vec2f(u.x*v,u.y*v);}
  friend vec2f operator*(const float&u,const vec2f&v){return vec2f(u*v.x,u*v.y);}
  friend vec2f operator+(const vec2f&u,const vec2f&v){return vec2f(u.x+v.x,u.y+v.y);}
  friend vec2f operator-(const vec2f&u,const vec2f&v){return vec2f(u.x-v.x,u.y-v.y);}
  friend void operator*=(vec2f&ref,float r){ref.x*=r;ref.y*=r;}
  void operator+=(const vec2d&v){x+=v.x;y+=v.y;}
  void operator-=(const vec2d&v){x-=v.x;y-=v.y;}
  friend vec2f operator*(float u,const vec2f&v){return vec2f(u*v.x,u*v.y);}
  operator vec2d()const{return vec2d(x,y);}
  friend bool operator==(const vec2f&u,const vec2f&v){return (u.x==v.x)&&(u.y==v.y);}
  friend bool operator!=(const vec2f&u,const vec2f&v){return (u.x!=v.x)||(u.y!=v.y);}
  vec2f operator-()const{return vec2f(-x,-y);}
};
inline static real dot(const vec2f&a,const vec2f&b){return a.x*b.x+a.y*b.y;}
inline static real cross(const vec2f&a,const vec2f&b){return a.x*b.y-a.y*b.x;}
class QapMat22{
public:
  vec2f col1,col2;
  QapMat22():col1(1,0),col2(0,1){}
  QapMat22(const vec2f&c1,const vec2f&c2):col1(c1),col2(c2){}
  QapMat22(float a11,float a12,float a21,float a22){col1.x=a11;col1.y=a21;col2.x=a12;col2.y=a22;}
  explicit QapMat22(float ang){
    float c=cosf(ang),s=sinf(ang);
    col1.x=c; col2.x=-s; col1.y=s; col2.y=+c;
  }
  void set(const vec2f&c1,const vec2f&c2){col1=c1;col2=c2;}
  void set(float ang){
    float c=cosf(ang),s=sinf(ang);
    col1.x=c; col2.x=-s; col1.y=s; col2.y=+c;
  }
  void set_ident(){col1.x=1.0f;col2.x=0.0f;col1.y=0.0f;col2.y=1.0f;}
  void set_zero(){col1.x=0.0f;col2.x=0.0f;col1.y=0.0f;col2.y=0.0f;}
  float GetAngle()const{return atan2(col1.y,col1.x);}
  void mul(real r){col1*=r;col2*=r;}
};

class transform2f{
public:
  vec2f p; QapMat22 r;
  transform2f(){}
  transform2f(const vec2f&p,const QapMat22&r):p(p),r(r){}
  explicit transform2f(const vec2f&p):p(p){}
  void set_ident(){p.set_zero();r.set_ident();}
  void set(const vec2d&p,float ang){this->p=p;this->r.set(ang);}
  float getAng()const{return atan2(r.col1.y,r.col1.x);}
  friend vec2f operator*(const transform2f&T,const vec2f&v){
    float x=(+T.r.col1.x*v.x+T.r.col2.x*v.y)+T.p.x;
    float y=(+T.r.col1.y*v.x+T.r.col2.y*v.y)+T.p.y;
    return vec2f(x,y);
  }
};
typedef transform2f b2Transform;
  
inline transform2f MakeZoomTransform(const vec2d&zoom)
{
  transform2f tmp(vec2f(0,0),QapMat22(vec2f(zoom.x,0.f),vec2f(0.f,zoom.y)));
  return tmp;
}
class vec3f{
public:
  float x,y,z;
  //vec3f(const D3DVECTOR&v):D3DVECTOR(v){}
  vec3f(){x=0;y=0;z=0;}
  vec3f(float x,float y,float z)
  {
    #define F(a)this->a=a;
    F(x);F(y);F(z);
    #undef F
  }
public:
  bool dist_to_point_less_that_r(const vec3f&p,real r)const{return (p-*this).SqrMag()<r*r;}
  bool dist_to_point_less_that_r(const vec3f&p,float r)const{return (p-*this).SqrMag()<r*r;}
  friend vec3f operator*(const float&u,const vec3f&v)
  {
    return vec3f(v.x*u,v.y*u,v.z*u);
  }
  friend vec3f operator*(const vec3f&v,const float&u)
  {
    return vec3f(v.x*u,v.y*u,v.z*u);
  }
  friend vec3f operator+(const vec3f&v,const vec3f&u)
  {
    return vec3f(v.x+u.x,v.y+u.y,v.z+u.z);
  }
  friend vec3f operator-(const vec3f&v,const vec3f&u)
  {
    return vec3f(v.x-u.x,v.y-u.y,v.z-u.z);
  }
  void operator*=(const float&k)
  {
    x*=k;
    y*=k;
    z*=k;
  }
  bool operator==(const vec3f&v)const
  {
    auto&a=*this;
    bool xok=a.x==v.x;
    bool yok=a.y==v.y;
    bool zok=a.z==v.z;
    return xok&&yok&&zok;
  }
  bool operator!=(const vec3f&v)const
  {
    return !operator==(v);
  }
  void operator+=(const vec3f&v)
  {
    x+=v.x;
    y+=v.y;
    z+=v.z;
  }
  void operator-=(const vec3f&v)
  {
    x-=v.x;
    y-=v.y;
    z-=v.z;
  }
  vec3f operator+()const{return *this;}
  vec3f operator-()const{return *this*-1;}
  vec3f RawMul(const vec3f&b)const
  {
    auto&a=*this;
    return vec3f(a.x*b.x,a.y*b.y,a.z*b.z);
  }
  vec3f RawMul(float x,float y,float z)const
  {
    auto&a=*this;
    return vec3f(a.x*x,a.y*y,a.z*z);
  }
  vec3f Mul(const vec3f&b)const
  {
    auto&a=*this;
    return vec3f(a.x*b.x,a.y*b.y,a.z*b.z);
  }
  vec3f Mul(float x,float y,float z)const
  {
    auto&a=*this;
    return vec3f(a.x*x,a.y*y,a.z*z);
  }
  float Mag()const
  {
    return sqrt(x*x+y*y+z*z);
  }
  float SqrMag()const
  {
    return x*x+y*y+z*z;
  }
  vec3f Norm()const
  {
    if((x==0)&&(y==0)&&(z==0))
    {
      return vec3f(0,0,0);
    }
    auto k=1.0f/Mag();
    return vec3f(x*k,y*k,z*k);
  }
  vec3f cross(const vec3f&b)const
  {
    auto&a=*this;
    return vec3f(
      +(a.y*b.z-a.z*b.y),
      -(a.x*b.z-a.z*b.x),
      +(a.x*b.y-a.y*b.x)
    );
  }
  float dot(const vec3f&b)const
  {
    auto&a=*this;
    return (
      a.x*b.x+
      a.y*b.y+
      a.z*b.z
    );
  }
};
inline float dot(const vec3f&a,const vec3f&b){return a.dot(b);}
//inline float dot(const vec3d&a,const vec3d&b){return a.dot(b);}
inline vec3f cross(const vec3f&a,const vec3f&b){return a.cross(b);}
//inline vec3d cross(const vec3d&a,const vec3d&b){return a.cross(b);}
class vec2i{
public:
  typedef vec2i SelfClass;
  int x, y;
  vec2i():x(0),y(0){}
  vec2i(int x,int y):x(x),y(y){}
  friend vec2i operator*(int u,const vec2i&v){return vec2i(u*v.x,u*v.y);}
  friend vec2i operator*(const vec2i&v,int u){return vec2i(u*v.x,u*v.y);}
  friend vec2i operator/(const vec2i&v,int d){return vec2i(v.x/d,v.y/d);}
  friend vec2i operator+(const vec2i&u,const vec2i&v){return vec2i(u.x+v.x,u.y+v.y);}
  friend vec2i operator-(const vec2i&u,const vec2i&v){return vec2i(u.x-v.x,u.y-v.y);}
  void operator+=(const vec2i&v){x+=v.x;y+=v.y;}
  void operator-=(const vec2i&v){x-=v.x;y-=v.y;}
  int SqrMag()const{return x*x+y*y;}
  float Mag()const{return sqrt(float(x*x+y*y));}
  operator vec2d()const{return vec2d(x,y);}
  operator vec2f()const{return vec2f(x,y);}
  vec2i operator+()const{return vec2i(+x,+y);}
  vec2i operator-()const{return vec2i(-x,-y);}
  friend bool operator==(const vec2i&u,const vec2i&v){return (u.x==v.x)&&(u.y==v.y);}
  friend bool operator!=(const vec2i&u,const vec2i&v){return (u.x!=v.x)||(u.y!=v.y);}
  static vec2i fromVec2d(const vec2d&v){return vec2i(v.x,v.y);}
};
struct Dip2i{
  int a,b;
  Dip2i(int a,int b):a(a),b(b){}
  void Take(int x){a=min(a,x);b=max(b,x);}
  Dip2i Norm()const{return Dip2i(min(a,b),max(a,b));}
  int Mag()const{return b-a;}
  struct Transform{
    float x,s;
    Transform(float x,float s):x(x),s(s){}
    Transform(const Dip2i&from,const Dip2i&to){
      s=float(to.Norm().Mag())/float(from.Norm().Mag());
      x=to.a-from.a;
    }
    float operator*(int v){return x+v*s;}
  };
};
struct vec4f{
public:
  float b,g,r,a;
  //struct{float x,y,z,w;};
  vec4f(){}
  vec4f(float b,float g,float r,float a):b(b),g(g),r(r),a(a){}
  vec4f(const QapColor&ref):b(ref.b/255.f),g(ref.g/255.f),r(ref.r/255.f),a(ref.a/255.f){}
  vec4f&operator+=(const vec4f&v){b+=v.b;g+=v.g;r+=v.r;a+=v.a;return *this;}
  vec4f&operator*=(const float&k){b*=k;g*=k;r*=k;a*=k;return *this;}
  friend vec4f operator*(const float&u,const vec4f&v){return vec4f(u*v.b,u*v.g,u*v.r,u*v.a);}
  friend vec4f operator+(const vec4f&u,const vec4f&v){return vec4f(u.b+v.b,u.g+v.g,u.r+v.r,u.a+v.a);}
  #define F(r)Clamp(int(r*255),int(0),int(255))
  QapColor GetColor(){return QapColor(F(a),F(r),F(g),F(b));}
  #undef F
};
union vec4i{
public:
  struct{int x,y,z,w;};
  struct{int b,g,r,a;};
  vec4i(int b,int g,int r,int a):b(b),g(g),r(r),a(a){}
  vec4i(const QapColor&ref):b(ref.b),g(ref.g),r(ref.r),a(ref.a){}
  vec4i&operator+=(const vec4i&v){b+=v.b;g+=v.g;r+=v.r;a+=v.a;return *this;}
  vec4i operator*(const int&v){return vec4i(x*v,y*v,z*v,w*v);}
  vec4i operator/(const int&v){return vec4i(x/v,y/v,z/v,w/v);}
  vec4i operator+(const vec4i&v){return vec4i(x+v.x,y+v.y,z+v.z,w+v.w);}
  #define F(r)Clamp(int(r),int(0),int(255))
  QapColor GetColor(){return QapColor(F(a),F(r),F(g),F(b));}
  #undef F
};
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
template<class TYPE>static bool qap_check_id(const vector<TYPE>&arr,int id){return id>=0&&id<arr.size();}