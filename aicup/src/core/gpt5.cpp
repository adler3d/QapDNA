#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #undef UNICODE
  #include <windows.h>
  #include <intrin.h>
  #pragma comment(lib,"ws2_32.lib")
  #pragma comment(lib,"user32.lib")
  #pragma comment(lib,"Shell32.lib")
#endif
#include <thread>
#include <iostream>
#include <chrono>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <array>
using namespace std;

inline string IToS(const int&val){return to_string(val);}
inline string FToS(const double&val){return to_string(val);}
inline string FToS2(const float&val){std::stringstream ss;ss<<std::fixed<<std::setprecision(2)<<val;return ss.str();}
#ifdef __EMSCRIPTEN__
#define __debugbreak()EM_ASM({throw new Error("__debugbreak");});
#endif
#ifdef QAP_UNIX
#include <iostream>
#include <signal.h>
#define __debugbreak()raise(SIGTRAP);
#endif
inline bool SysQapAssert(const string&exp,bool&ignore,const string&filename,const int line,const string&funcname);
inline bool SysQapDebugMsg(const string&msg,bool&ignore,const string&filename,const int line,const string&funcname);
#if(defined(_DEBUG)||defined(QAP_DEBUG))
#define QapAssert(_Expression)if(!bool(_Expression)){static bool ignore=false;if(SysQapAssert((#_Expression),ignore,__FILE__,__LINE__,__FUNCTION__))__debugbreak();}
#else
#define QapAssert(_Expression)if(bool(_Expression)){};
#endif
#if(defined(_DEBUG)||defined(QAP_DEBUG))
#define QapDebugMsg(_Message){static bool ignore=false;if(SysQapDebugMsg((_Message),ignore,__FILE__,__LINE__,__FUNCTION__))__debugbreak();}
#else
#define QapDebugMsg(_Message)
#endif
#if(defined(_DEBUG)||defined(QAP_DEBUG))
#define QapNoWay(){QapDebugMsg("no way!");}
#else
#define QapNoWay()
#endif
enum QapMsgBoxRetval{qmbrSkip,qmbrBreak,qmbrIgnore};
inline int WinMessageBox(const string&caption,const string&text)
{
  #ifdef _WIN32
  string full_text=text+"\n\n    [Skip]            [Break]            [Ignore]";
  const int nCode=MessageBoxA(NULL,full_text.c_str(),caption.c_str(),MB_CANCELTRYCONTINUE|MB_ICONHAND|MB_SETFOREGROUND|MB_TASKMODAL);
  QapMsgBoxRetval retval=qmbrSkip;
  if(IDCONTINUE==nCode)retval=qmbrIgnore;
  if(IDTRYAGAIN==nCode)retval=qmbrBreak;
  return retval;
  #else
  #ifdef __EMSCRIPTEN__
  EM_ASM({alert(UTF8ToString($0)+"\n"+UTF8ToString($1));},int(caption.c_str()),int(text.c_str()));
  return qmbrBreak;
  #endif
  #endif
  #ifdef QAP_UNIX
  std::cerr<<"WinMessageBox:"+caption+"\n"+text<<std::endl;
  return qmbrBreak;
  #endif
}
typedef int(*TQapMessageBox)(const string&caption,const string&text);
struct TMessageBoxCaller
{
  static int Call(const string&caption,const string&text)
  {
    return Get()(caption,text);
  }
  static TQapMessageBox&Get()
  {
    static TQapMessageBox func=WinMessageBox;
    return func;
  }
  struct t_hack
  {
    TQapMessageBox old;
    t_hack(TQapMessageBox func)
    {
      old=Get();
      Get()=func;
    }
    ~t_hack()
    {
      Get()=old;
    }
  };
};
inline bool SysQapAssert(const string&exp,bool&ignore,const string&filename,const int line,const string&funcname)
{
  if(ignore)return false;
  std::string text="Source file :\n"+filename
      +"\n\nLine : "+std::to_string(line)
      +"\n\nFunction :\n"+funcname
      +"\n\nAssertion failed :\n"+exp;
  auto retval=(QapMsgBoxRetval)TMessageBoxCaller::Call("Assertion failed",text);
  if(qmbrIgnore==retval)ignore=true;
  return qmbrBreak==retval;
}
inline bool SysQapDebugMsg(const string&msg,bool&ignore,const string&filename,const int line,const string&funcname)
{
  if(ignore)return false;
  std::string text="Source file :\n"+filename
      +"\n\nLine : "+std::to_string(line)
      +"\n\nFunction :\n"+funcname
      +"\n\nDebug message :\n"+msg;
  auto retval=(QapMsgBoxRetval)TMessageBoxCaller::Call("Debug message",text);
  if(qmbrIgnore==retval)ignore=true;
  return qmbrBreak==retval;
}

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
  QapColor(const unsigned int&v){*this=(QapColor&)v;}
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

using std::string;
using std::vector;
using std::map;
using std::fstream;
using std::iostream;
using std::stringstream;
using std::array;
typedef double real;
typedef long long int int64;
typedef long long unsigned int uint64;
typedef unsigned int uint;
typedef unsigned char uchar;
class CrutchIO{
public:
  static bool FileExist(const string&FN)
  {
    std::fstream f;
    f.open(FN.c_str(),std::ios::in|std::ios::binary);
    return f.is_open();
  }
private:
  static int FileLength(iostream&f)
  {
    using namespace std;
    f.seekg(0,ios::end);
    auto L=f.tellg();
    f.seekg(0,ios::beg);
    return int(L);
  };
public:
  int pos;
  string mem;
  CrutchIO():mem(""),pos(0){};
  bool LoadFile(const string&FN)
  {
    using namespace std;
    fstream f;
    f.open(FN.c_str(),ios::in|ios::binary);
    if(!f)return false;
    int L=FileLength(f);
    mem.resize(L);
    if(L)f.read(&mem[0],L);
    //printf("f->size=%i\n",L);
    //printf("f->Chcount=%i\n",f._Chcount);
    f.close(); pos=0;
    return true;
  };
  bool SaveFile(const string&FN)
  {
    using namespace std;
    fstream f;
    f.open(FN.c_str(),ios::out|ios::binary);
    if(!f)return false;
    if(!mem.empty())f.write(&mem[0],mem.size());
    f.close(); pos=0; int L=mem.size();
    return true;
  };
  void read(char*c,int count)
  {
    for(int i=0;i<count;i++)c[i]=mem[pos++];//FIXME: тут можно юзать memcpy
  };
  void write(char*c,int count)
  {
    //mem.reserve(max(mem.capacity(),mem.size()+count));
    int n=mem.size();
    mem.resize(n+count);//Hint: resize гарантировано копирует содержимое.
    for(int i=0;i<count;i++)mem[n+i]=c[i];//FIXME: тут можно юзать memcpy
    pos+=count;
  };
};
//-------------------------------------------//
class QapIO{
public:
  QapIO(){}
public:
  virtual void SavePOD(void*p,int count)=0;
  virtual void LoadPOD(void*p,int count)=0;
  virtual bool TryLoad(int count)=0;
  virtual void Crash()=0;
  virtual bool IsCrashed()=0;
  virtual bool IsSizeIO()=0;
  virtual int GetSize()=0;
  virtual void WriteTo(QapIO&ref)=0;
public:
  #define LIST(F)F(int)F(unsigned int)F(char)F(unsigned char)F(bool)F(int64)F(uint64)F(float)F(real)F(short)F(unsigned short)F(vec2i)F(vec2d)F(vec2f)
  #define F(TYPE)void load(TYPE&ref){if(!TryLoad(sizeof(ref))){Crash();return;}LoadPOD(&ref,sizeof(ref));}
  LIST(F)
  #undef F
  #define F(TYPE)void save(TYPE&ref){SavePOD(&ref,sizeof(ref));}
  LIST(F)
  #undef F
  #undef LIST
  void load(std::string&ref)
  {
    int size=0;
    load(size);
    if(size<0){Crash();return;}
    if(!size){ref.clear();return;}
    if(!TryLoad(size)){Crash();return;}
    ref.resize(size);
    LoadPOD(&ref[0],size);
  }
  void save(std::string&ref)
  {
    int size=ref.size();
    save(size);
    if(!size)return;
    SavePOD(&ref[0],size);
  }
  template<class TYPE>
  void load(std::vector<TYPE>&ref)
  {
    int size=0;
    load(size);
    if(size<0){Crash();return;}
    ref.resize(size);
    for(int i=0;i<size;i++){
      load(ref[i]);
    }
  }
  template<class TYPE>
  void load_as_pod(std::vector<TYPE>&ref)
  {
    int size=0;
    load(size);
    if(size<0){Crash();return;}
    ref.resize(size);
    for(int i=0;i<size;i++){
      LoadPOD(&ref[i],sizeof(TYPE));
    }
  }
  template<class TYPE>
  void save(std::vector<TYPE>&ref)
  {
    int size=ref.size();
    save(size);
    if(!size)return;
    for(int i=0;i<size;i++){
      save(ref[i]);
    }
  }
  template<class TYPE>
  void save_as_pod(std::vector<TYPE>&ref)
  {
    int size=ref.size();
    save(size);
    if(!size)return;
    for(int i=0;i<size;i++){
      SavePOD(&ref[i],sizeof(TYPE));
    }
  }
  void write_raw_string(string&s){if(s.empty())return;SavePOD((void*)&s[0],s.size());}
};
//-------------------------------------------//
class TDataIO:public QapIO{
public:
  CrutchIO IO;
  bool crashed;
  TDataIO():crashed(false),QapIO(){}
  //TDataIO(const TDataIO&)=delete;
  //TDataIO(TDataIO&)=delete;
  //TDataIO(TDataIO&&)=delete;
  //void operator=(const TDataIO&)=delete;
  //void operator=(TDataIO&)=delete;
  //void operator=(TDataIO&&)=delete;
public:
  void SavePOD(void*p,int count)
  {
    this->IO.write((char*)p,count);
  }
  void LoadPOD(void*p,int count)
  {
    int max_count=int(this->IO.mem.size())-int(this->IO.pos);
    if(count>max_count)
    {
      QapAssert(count<=max_count);
      return;
    }
    this->IO.read((char*)p,count);
  }
  bool TryLoad(int count)
  {
    auto max_size=int(IO.mem.size())-int(IO.pos);
    return (count>=0)&&(max_size>=count);
  }
  void Crash()
  {
    IO.pos=IO.mem.size();
    crashed=true;
  }
  bool IsCrashed()
  {
    return crashed;
  }
  bool IsSizeIO()
  {
    return false;
  }
  int GetSize()
  {
    return IO.mem.size();
  }
  void SaveTo(QapIO&Stream)
  {
    int size=IO.mem.size();
    Stream.SavePOD(&size,sizeof(size));
    if(!size)return;
    Stream.SavePOD(&IO.mem.front(),size);
  }
  void LoadFrom(QapIO&Stream)
  {
    int size;
    Stream.LoadPOD(&size,sizeof(size));
    if(!size)return;
    IO.mem.resize(size);
    Stream.LoadPOD(&IO.mem.front(),size);
  }
  void WriteTo(QapIO&ref)
  {
    ref.write_raw_string(IO.mem);
  }
};
//-------------------------------------------//
class TSizeIO:public QapIO{
public:
  int size;
  bool crashed;
public:
  TSizeIO():size(0),crashed(false),QapIO(){}
  void SavePOD(void*p,int count){size+=count;}
  void LoadPOD(void*p,int count){QapNoWay();Crash();}
  bool TryLoad(int count){QapNoWay();Crash();return false;}
  void Crash(){crashed=true;}
  bool IsCrashed(){return crashed;}
  bool IsSizeIO(){return true;}
  int GetSize()
  {
    return size;
  }
  void WriteTo(QapIO&ref)
  {
    if(!ref.IsSizeIO()){QapNoWay();Crash();return;}
    ref.SavePOD(nullptr,size);
  }
};
namespace detail
{
  struct yes_type
  {
    char padding[1];
  };
  struct no_type
  {
    char padding[8];
  };
  template<bool condition,typename true_t,typename false_t>struct select;
  template<typename true_t,typename false_t>struct select<true,true_t,false_t>
  {
    typedef true_t type;
  };
  template<typename true_t,typename false_t>struct select<false,true_t,false_t>
  {
    typedef false_t type;
  };
  template<class U,U x>struct test;
  template<class TYPE>
  static void TryDoReset(void*) {}
  template<class TYPE>
  static void TryDoReset(TYPE*Self,void(TYPE::ParentClass::*pDoReset)()=&TYPE::ParentClass::DoReset)
  {
    (Self->*pDoReset)();
  }
  template<class TYPE>
  static void FieldTryDoReset(TYPE&p,...) {}
  template<class TYPE,size_t SIZE>static void FieldTryDoReset(array<TYPE,SIZE>&arr)
  {
    for (int i=0;i<SIZE;i++)FieldTryDoReset(arr[i]);
  }
  static void FieldTryDoReset(unsigned short&ref)
  {
    ref=0;
  };
  static void FieldTryDoReset(short&ref)
  {
    ref=0;
  };
  static void FieldTryDoReset(bool&ref)
  {
    ref=0;
  };
  static void FieldTryDoReset(int&ref)
  {
    ref=0;
  };
  static void FieldTryDoReset(size_t&ref)
  {
    ref=0;
  };
  static void FieldTryDoReset(float&ref)
  {
    ref=0;
  };
  static void FieldTryDoReset(double&ref)
  {
    ref=0;
  };
  static void FieldTryDoReset(char&ref)
  {
    ref=0;
  };
  static void FieldTryDoReset(uchar&ref)
  {
    ref=0;
  };
};
namespace detail{
  template<typename T>
  struct has_ProxySys$$
  {
    template<class U>
    static no_type check(...);
    template<class U>
    static yes_type check(
      U*,
      typename U::ProxySys$$(*)=nullptr
    );
    static const bool value=sizeof(check<T>(nullptr))==sizeof(yes_type);
  };
};

template<typename TYPE,bool is_proxy>
struct ProxySys$$;
template<class TYPE>
struct Sys$${
  static void Save(TDataIO&IO,TYPE&ref)
  {
    ProxySys$$<TYPE,detail::has_ProxySys$$<TYPE>::value>::Save(IO,ref);
  }
  static void Load(TDataIO&IO,TYPE&ref)
  {
    ProxySys$$<TYPE,detail::has_ProxySys$$<TYPE>::value>::Load(IO,ref);
  }
};

template<class TYPE>void QapSave(TDataIO&IO,TYPE*){
  #ifdef _WIN32
  static_assert(false,"fail");
  #else
  QapNoWay();
  #endif
}
template<class TYPE>void QapLoad(TDataIO&IO,TYPE*){
  #ifdef _WIN32
  static_assert(false,"fail");
  #else
  QapNoWay();
  #endif
}

template<class TYPE>void QapSave(TDataIO&IO,TYPE&ref){Sys$$<TYPE>::Save(IO,ref);}
template<class TYPE>void QapLoad(TDataIO&IO,TYPE&ref){Sys$$<TYPE>::Load(IO,ref);}
template<class TYPE>string QapSaveToStr(TYPE&ref){TDataIO IO;QapSave(IO,ref);return IO.IO.mem;}
template<class TYPE>bool QapLoadFromStr(TYPE&ref,const string&data){TDataIO IO; IO.IO.mem=data;QapLoad(IO,ref);return !IO.crashed;}


template<class TYPE>string QapSaveToStrWithSizeOfType(TYPE&ref)
{
  int size_of_type=sizeof(TYPE);
  TDataIO IO;QapSave(IO,size_of_type);QapSave(IO,ref);return IO.IO.mem;
}
template<class TYPE>void QapLoadFromStrWithSizeOfType(TYPE&ref,const string&data){
  int size_of_type=-1;
  TDataIO IO;IO.IO.mem=data;QapLoad(IO,size_of_type);if(size_of_type!=sizeof(TYPE))return;QapLoad(IO,ref);
}

template<typename TYPE>
struct ProxySys$$<TYPE,true>
{
  static void Save(TDataIO&IO,TYPE&ref){
    TYPE::ProxySys$$::Save(IO,ref);
  }
  static void Load(TDataIO&IO,TYPE&ref){
    TYPE::ProxySys$$::Load(IO,ref);
  }
};

//class QapKeyboard;
/*template<>
struct Sys$$<QapKeyboard::TKeyState>{
  static void Save(TDataIO&IO,QapKeyboard::TKeyState&ref)
  {
    static_assert(QapKeyboard::TKeyState::MAX_KEY==263,"hm...");
    std::bitset<256+8> bs;
    for(int i=0;i<ref.MAX_KEY;i++){auto&ex=ref.data[i];QapAssert(1>=*(uchar*)(void*)&ex);bs[i]=ex;}
    IO.SavePOD(&bs,sizeof(bs));
  }
  static void Load(TDataIO&IO,QapKeyboard::TKeyState&ref)
  {
    static_assert(QapKeyboard::TKeyState::MAX_KEY==263,"hm...");
    std::bitset<256+8> bs;
    IO.LoadPOD(&bs,sizeof(bs));
    for(int i=0;i<ref.MAX_KEY;i++){auto&ex=ref.data[i];ex=bs[i];}
  }
};*/

//template<class TYPE>struct Sys$${};

//-->
#define SYS_RAW_POD_TYPE(QapKeyboard)\
  template<>\
  struct Sys$$<QapKeyboard>{\
    static void Save(TDataIO&IO,QapKeyboard&ref)\
    {\
      IO.SavePOD(&ref,sizeof(ref));\
    }\
    static void Load(TDataIO&IO,QapKeyboard&ref)\
    {\
      IO.LoadPOD(&ref,sizeof(ref));\
    }\
  };
//---
#define LIST(F)F(QapColor)F(vec2i)F(vec2f)F(vec2d)
LIST(SYS_RAW_POD_TYPE)
#undef LIST
//---
#undef SYS_RAW_POD_TYPE
//<--

template<class TYPE>
struct Sys$$<vector<TYPE>>{
  static void Save(TDataIO&IO,vector<TYPE>&ref)
  {
    int size=ref.size();
    IO.save(size);
    if(!size)return;
    for(int i=0;i<size;i++){
      auto&ex=ref[i];
      Sys$$<TYPE>::Save(IO,ex);
    }
  }
  static void Load(TDataIO&IO,vector<TYPE>&ref)
  {
    int size=0;
    IO.load(size);
    if(size<0||!IO.TryLoad(size)){IO.Crash();return;}
    ref.resize(size);
    for(int i=0;i<size;i++){
      auto&ex=ref[i];
      Sys$$<TYPE>::Load(IO,ex);
    }
  }
};

//-->
#define SYS_SIMPLE_TYPE(string)\
  template<>\
  struct Sys$$<string>{\
    static void Save(TDataIO&IO,string&ref)\
    {\
      IO.save(ref);\
    }\
    static void Load(TDataIO&IO,string&ref)\
    {\
      IO.load(ref);\
    }\
  };
//---
#define LIST(F)F(int)F(unsigned int)F(char)F(unsigned char)F(bool)F(int64)F(uint64)F(float)F(real)F(short)F(unsigned short)F(string)
#define F(TYPE)SYS_SIMPLE_TYPE(TYPE)
LIST(F)
#undef F
#undef LIST
//---
#undef SYS_SIMPLE_TYPE
//<--

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
    
    auto r=[&gen](){static std::uniform_real_distribution<double>dist(-1.0,1.0);return vec2d{dist(gen),dist(gen)};};
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
      while(player_colors.size()<w.slot2score.size()){
        player_colors.push_back(QapColor(255,rand()%256,rand()%256,rand()%256));
      }
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
      for (int p = 0; p < world.slot2score.size(); p++) {
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
  static unique_ptr<i_world> mk_world(int version){
    return make_unique<t_world_impl>();
  }
  #endif
};

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
typedef t_splinter::t_world t_world;
static t_world::t_cmd dna/*gpt5*/(const t_world& w, int player_id)
{
    t_world::t_cmd out{};
    const double MINR = 5.0;         // компактный треугольник
    const double LONGR_BASE = 60.0;  // умеренное «растяжение» в фазе рывка
    const double THREAT_R = 9.5;     // немного с запасом к порогу 8
    const int    PERIOD = 12;        // длина цикла походки (6+6 тиков)

    // --- удобные лямбды ---
    auto dot = [](const vec2d& a, const vec2d& b){ return a.x*b.x + a.y*b.y; };

    int base = player_id * 3;
    vec2d p0 = w.balls[base+0].pos;
    vec2d p1 = w.balls[base+1].pos;
    vec2d p2 = w.balls[base+2].pos;
    vec2d com = (p0 + p1 + p2) * (1.0/3.0);

    // --- 1) поиск ближайшей ВРАЖЕСКОЙ перемычки (отрезка) к нашему центру масс ---
    double bestSegDist = 1e18;
    vec2d targetMid = com; // по умолчанию никуда не идём
    {
        const int EDGES[3][2] = {{0,1},{1,2},{2,0}};
        for(int p=0;p<w.slot2score.size();p++){
            if(p==player_id || w.slot2deaded[p]) continue;
            int b = p*3;
            for(int e=0;e<3;e++){
                vec2d a = w.balls[b + EDGES[e][0]].pos;
                vec2d c = w.balls[b + EDGES[e][1]].pos;
                double d = t_splinter::Dist2Line(com, a, c); // 1e9 если проекция вне отрезка
                if(d < bestSegDist){
                    bestSegDist = d;
                    targetMid = (a + c) * 0.5; // берём середину, как простую цель
                }
            }
        }
        // fallback: если проекции ни на один отрезок не попали — идём к ближайшему живому сопернику
        if(bestSegDist > 1e17){
            double best = 1e18;
            for(int p=0;p<w.slot2score.size();p++){
                if(p==player_id || w.slot2deaded[p]) continue;
                vec2d c = w.get_center_of_mass(p);
                double d = (c - com).Mag();
                if(d < best){ best = d; targetMid = c; }
            }
        }
    }

    // --- 2) проверка угрозы: чужой шар близко к ЛЮБОЙ нашей перемычке? ---
    bool threat = false;
    {
        const vec2d A[3] = {p0,p1,p2};
        const int EDGES[3][2] = {{0,1},{1,2},{2,0}};
        for(int e=0;e<3 && !threat; e++){
            vec2d a = A[EDGES[e][0]];
            vec2d b = A[EDGES[e][1]];
            for(int i=0;i<(int)w.balls.size();i++){
                if(i/3 == player_id || w.slot2deaded[i/3]) continue;
                if(t_splinter::Dist2Line(w.balls[i].pos, a, b) <= THREAT_R){ threat = true; break; }
            }
        }
    }

    // --- значения по умолчанию (компакт + умеренно скользим) ---
    out.spring0_new_rest = MINR;
    out.spring1_new_rest = MINR;
    out.spring2_new_rest = MINR;
    out.friction0 = out.friction1 = out.friction2 = 0.30;

    // если угроза — просто сжаться и притормозить
    if(threat){
        out.friction0 = out.friction1 = out.friction2 = 0.99;
        return out;
    }

    // --- 3) направление движения к цели ---
    vec2d dir = targetMid - com;
    double L = dir.Mag();
    if(L > 1e-9) dir = dir * (1.0/L); else dir = vec2d(0,0);

    // выбираем «нос» треугольника — вершина, наиболее смотрящая в dir
    vec2d offs[3] = { p0 - com, p1 - com, p2 - com };
    double d0 = dot(offs[0], dir);
    double d1 = dot(offs[1], dir);
    double d2 = dot(offs[2], dir);

    if(d0==offs[0].Rot(dir).x){
      int gg=1;
    }else if(d0==offs[0].Rot(dir).y){
      int gg=2;
    }

    int front = 0;
    if(d1 > d0 && d1 >= d2) front = 1;
    else if(d2 >= d0 && d2 >= d1) front = 2;

    // чуточку уменьшим «растяжение», если мы ещё далеко от цели
    double LONGR = (bestSegDist < 60.0 ? LONGR_BASE : 40.0);

    // --- 4) простая двухфазная походка «инчворма» ---
    int half = PERIOD/2;                 // 6 тиков «вперёд», 6 тиков «подтянуть»
    bool extendPhase = ((w.tick / half) % 2) == 0;
    if(bestSegDist < 20.0){              // рядом с целью — более частые рывки
        half = 4;
        extendPhase = ((w.tick / half) % 2) == 0;
    }

    // индексы рёбер: e0=0-1, e1=1-2, e2=2-0
    auto set_edge_lengths_for_front = [&](int f, double len_ab, double len_bc, double len_ca){
        // раскладываем по e0/e1/e2
        // e0: 0-1
        // e1: 1-2
        // e2: 2-0
        // len_ab = длина у ребра, инцидентного (f) и (f+1)%3
        // удобнее задать явно в switch ниже
    };

    // Фаза «вытянуть нос»: две пружины у front — длинные; нос скользкий, база «тормозит»
    if(extendPhase){
        out.f(front) = 0.01;
        out.f((front+1)%3) = 0.99;
        out.f((front+2)%3) = 0.99;

        if(front==0){ out.spring0_new_rest = LONGR; out.spring2_new_rest = LONGR; /* e1=MINR */ }
        if(front==1){ out.spring0_new_rest = LONGR; out.spring1_new_rest = LONGR; /* e2=MINR */ }
        if(front==2){ out.spring1_new_rest = LONGR; out.spring2_new_rest = LONGR; /* e0=MINR */ }
    }
    // Фаза «подтянуть базу»: все пружины короткие, нос «цепляется», база скользит
    else{
        out.spring0_new_rest = MINR;
        out.spring1_new_rest = MINR;
        out.spring2_new_rest = MINR;
        out.f(front) = 0.99;
        out.f((front+1)%3) = 0.01;
        out.f((front+2)%3) = 0.01;
    }

    return out;
}

std::mt19937 gen;
//#include "adler20250907.inl"

bool read_exactly_or_eof(std::istream& is, char* buffer, std::size_t n) {
  std::size_t total_read = 0;
  while (total_read < n) {
    is.read(buffer + total_read, static_cast<std::streamsize>(n - total_read));
    std::streamsize got = is.gcount();
    if (got == 0) {
      return false;
    }
    total_read += static_cast<std::size_t>(got);
  }
  return true;
}

int main(int argc,char*argv[]){
  using namespace std::chrono_literals;
  
  #ifdef _WIN32
  _setmode(_fileno(stdout),_O_BINARY);
  _setmode(_fileno(stderr),_O_BINARY);
  _setmode(_fileno(stdin),_O_BINARY);
  #endif
  /*
  for(int i=0;i<100;i++){
    std::this_thread::sleep_for(1000ms);
  }*/
  uint32_t seed=0;
  if(!read_exactly_or_eof(cin,(char*)&seed,sizeof(seed))){
    int fail=1;
    return -1;
  }
  gen=std::mt19937(seed);
  t_splinter::t_world w;
  string recv_buffer;
  for(int iter=0;;iter++){
    uint32_t len=0;
    if(!read_exactly_or_eof(cin,(char*)&len,sizeof(len))){
      int fail=1;
      break;
    }
    recv_buffer.resize(len);
    if(!read_exactly_or_eof(cin,recv_buffer.data(),len)){
      int fail=1;
      break;
    }
    QapLoadFromStr(w,recv_buffer);
    t_splinter::t_world::t_cmd cmd;
    //cmd=dna_adler20250907(w,w.your_id);
    cmd=dna(w,w.your_id);
    string scmd=QapSaveToStr(cmd);
    string sscmd=QapSaveToStr(scmd);
    cout.write(sscmd.data(),sscmd.size());
    int gg=1;
  }
  return 0;
}
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>  // CommandLineToArgvW
#include <locale>

std::string utf16_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

int WINAPI QapLR_WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (wargv == nullptr) {
        MessageBoxA(nullptr, "Failed to parse command line", "Error", MB_ICONERROR);
        return 1;
    }

    std::vector<std::vector<char>> argv_storage;
    std::vector<char*> argv;

    for (int i = 0; i < argc; ++i) {
        std::string utf8 = utf16_to_utf8(wargv[i]);
        argv_storage.emplace_back(utf8.begin(), utf8.end());
        argv_storage.back().push_back('\0');
        argv.push_back(argv_storage.back().data());
    }
    int result=main(argc,argv.data());

    LocalFree(wargv);
    return result;
}
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
  return QapLR_WinMain(hInstance,hPrevInstance,lpCmdLine,nCmdShow);
}
#endif