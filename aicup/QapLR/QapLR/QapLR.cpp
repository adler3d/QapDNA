#if(0)
//#include "socket_adapter.cpp"
#include "counter.cpp"
#else
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #undef UNICODE
  #include <windows.h>
  #include <intrin.h>
#endif
#include <string>
void LOG(const std::string&str);
#include "../../src/core/netapi.h"
#include "stdio_zchan_reader.inl"
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
//#include <tchar.h>
#ifndef _WIN32
#ifndef QAP_UNIX
#include <emscripten.h>
#endif
#else
#include <d3d9.h>
#pragma comment(lib,"d3d9")
#endif
#include <vector>
#include <map>
#include <functional>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <stdlib.h>
#include <atomic>
#include <thread>
#include <time.h>
#include <chrono>
std::vector<std::string> g_log;
enum TMouseButton{mbLeft=257,mbRight=258,mbMiddle=259,};
auto now=std::chrono::high_resolution_clock::now;
typedef std::chrono::duration<double, std::milli> dms;
double dmsc(dms diff){return diff.count();}
using namespace std;
#include "qap_time.inl"
void LOG(const string&str){cerr<<"["<<qap_time()<<"] "<<(str)<<endl;}
#include "thirdparty/sweepline/sweepline.hpp"
#define QAP_DEBUG
#ifdef _WIN32
class QapClock{
public:
  typedef long long int int64;
  int64 freq,beg,tmp;
  bool run;
public:
  QapClock(){QueryPerformanceFrequency((LARGE_INTEGER*)&freq);run=false;tmp=0;Start();}
  void Start(){QueryPerformanceCounter((LARGE_INTEGER*)&beg);run=true;}
  void Stop(){QueryPerformanceCounter((LARGE_INTEGER*)&tmp);run=false;tmp-=beg;}
  double Time(){if(run)QueryPerformanceCounter((LARGE_INTEGER*)&tmp);return run?double(tmp-beg)/double(freq):double(tmp)/double(freq);}
  double MS()
  {
    double d1000=1000.0;
    if(run)QueryPerformanceCounter((LARGE_INTEGER*)&tmp);
    if(run)return (double(tmp-beg)*d1000)/double(freq);
    if(!run)return (double(tmp)*d1000)/double(freq);
    return 0;
  }
  static int64 qpc(){int64 tmp;QueryPerformanceCounter((LARGE_INTEGER*)&tmp);return tmp;}
};
QapClock g_sys_clock;
#define QAP_EM_LOG(TEXT)
#else
#ifndef __EMSCRIPTEN__
//https://github.com/copilot/share/c05603ac-4240-8476-b813-be01a48a201d
#include <chrono>
class QapClock{
public:
  typedef long long int int64;
  int64 freq,beg,tmp;
  bool run;
  typedef std::chrono::high_resolution_clock clock;
  typedef std::chrono::time_point<clock> time_point;
  time_point t_beg,t_tmp;
public:
  QapClock(){Start();}
  #define F(diff)std::chrono::duration_cast<std::chrono::microseconds>(diff).count()
  void Start(){t_beg=clock::now();run=true;}
  void Stop(){t_tmp=clock::now();run=false;tmp=F(t_tmp-t_beg);}
  double MS(){
    if(run){t_tmp=clock::now();return F(t_tmp-t_beg)*0.001;}
    return double(tmp)*0.001;
  }
  static int64 qpc(){return F(clock::now().time_since_epoch());}
  #undef F
};
#define QAP_EM_LOG(TEXT)
#else
class QapClock{
public:
  typedef long long int int64;
  int64 freq,beg,tmp;
  bool run;
public:
  QapClock(){Start();}
  void Start(){beg=qpc();run=true;}
  void Stop(){run=false;tmp=qpc()-beg;}
  double MS(){
    if(run){tmp=qpc();return (tmp-beg)*0.001;}
    return tmp*0.001;
  }
  static int64 qpc(){return EM_ASM_INT({return (1000*performance.now())|0;});}
};
#define QAP_EM_LOG(TEXT)EM_ASM({console.log(UTF8ToString($0));},string(TEXT).c_str());
#endif //__EMSCRIPTEN__
#endif //_WIN32
#ifndef _WIN32
#define VK_SLEEP          0x5F
#define VK_NUMPAD0        0x60
#define VK_NUMPAD1        0x61
#define VK_NUMPAD2        0x62
#define VK_NUMPAD3        0x63
#define VK_NUMPAD4        0x64
#define VK_NUMPAD5        0x65
#define VK_NUMPAD6        0x66
#define VK_NUMPAD7        0x67
#define VK_NUMPAD8        0x68
#define VK_NUMPAD9        0x69
#define VK_MULTIPLY       0x6A
#define VK_ADD            0x6B
#define VK_SEPARATOR      0x6C
#define VK_SUBTRACT       0x6D
#define VK_DECIMAL        0x6E
#define VK_DIVIDE         0x6F
#define VK_F1             0x70
#define VK_F2             0x71
#define VK_F3             0x72
#define VK_F4             0x73
#define VK_F5             0x74
#define VK_F6             0x75
#define VK_F7             0x76
#define VK_F8             0x77
#define VK_F9             0x78
#define VK_F10            0x79
#define VK_F11            0x7A
#define VK_F12            0x7B
#define VK_F13            0x7C
#define VK_F14            0x7D
#define VK_F15            0x7E
#define VK_F16            0x7F
#define VK_F17            0x80
#define VK_F18            0x81
#define VK_F19            0x82
#define VK_F20            0x83
#define VK_F21            0x84
#define VK_F22            0x85
#define VK_F23            0x86
#define VK_F24            0x87
#define VK_ESCAPE         0x1B

#define VK_CONVERT        0x1C
#define VK_NONCONVERT     0x1D
#define VK_ACCEPT         0x1E
#define VK_MODECHANGE     0x1F

#define VK_SPACE          0x20
#define VK_PRIOR          0x21
#define VK_NEXT           0x22
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_SELECT         0x29
#define VK_PRINT          0x2A
#define VK_EXECUTE        0x2B
#define VK_SNAPSHOT       0x2C
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E
#define VK_HELP           0x2F
#define VK_CLEAR          0x0C
#define VK_RETURN         0x0D
#define VK_BACK           0x08
#define VK_TAB            0x09
#define VK_SHIFT          0x10
#define VK_CONTROL        0x11
#define VK_MENU           0x12
#define VK_PAUSE          0x13
#define VK_CAPITAL        0x14

#endif
#ifdef __EMSCRIPTEN__
static string file_get_contents(const string&fn){
  QAP_EM_LOG("file_get_contents:"+fn);
  int length=EM_ASM_INT({
    let key=UTF8ToString($0);
    let val=localStorage.getItem(key);
    if(val===null)return 0;
    return lengthBytesUTF8(val);
  },fn.c_str());
  if(!length)return {};
  string out;out.resize(length);
  EM_ASM({
    let key=UTF8ToString($0);
    let val=localStorage.getItem(key);
    if(val!==null)stringToUTF8(val,$1,$2+1);
  },fn.c_str(),out.data(),length);
  return out;
}
static bool file_put_contents(const string&fn,const string&mem){
  QAP_EM_LOG("file_put_contents:"+fn);
  EM_ASM({
    let key=UTF8ToString($0);
    let val=UTF8ToString($1);
    localStorage.setItem(key,val);
  },fn.c_str(),mem.c_str());
  return true;
}
#else
static bool file_put_contents(const std::string&FN,const std::string&mem){
  std::ofstream file(FN,std::ios::binary|std::ios::trunc);
  if(!file.is_open())return false;
  file.write(mem.data(),static_cast<std::streamsize>(mem.size()));
  return file.good();
}
static std::string file_get_contents(const std::string&fn){
  std::ifstream file(fn,std::ios::binary);
  if(!file.is_open())return{};
  file.seekg(0,std::ios::end);
  std::streamsize size=file.tellg();
  if(size<0)return{};
  file.seekg(0,std::ios::beg);
  std::string buffer(static_cast<size_t>(size),'\0');
  if(!file.read(&buffer[0],size))return{};
  return buffer;
}
#endif

template<class TYPE>
void QapPopFront(vector<TYPE>&arr)
{
  int last=0;
  for(int i=1;i<arr.size();i++)
  {
    auto&ex=arr[i];
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
static vector<string> split(const string&s,const string&needle)
{
  vector<string> arr;
  if(s.empty())return arr;
  size_t p=0;
  for(;;){
    auto pos=s.find(needle,p);
    if(pos==std::string::npos){arr.push_back(s.substr(p));return arr;}
    arr.push_back(s.substr(p,pos-p));
    p=pos+needle.size();
  }
  return arr;
}
static string join(const vector<string>&arr,const string&glue)
{
  string out;
  size_t c=0;
  size_t dc=glue.size();
  for(int i=0;i<arr.size();i++){if(i)c+=dc;c+=arr[i].size();}
  out.reserve(c);
  for(int i=0;i<arr.size();i++){if(i)out+=glue;out+=arr[i];}
  return out;
}
struct TScreenMode{
  int W,H,BPP,Freq;
};
struct TSys{int UPS=64;TScreenMode SM;bool UPS_enabled=true;bool NeedClose=false;void ResetClock(){}}; TSys Sys;
static const int Sys_UPD=64;
#include "qap_assert.inl"
template<typename TYPE>
class QapPool{
public:
  struct Rec
  {
    bool used;
    TYPE data;
    Rec():used(false){}
  };
  vector<Rec>Arr;
  int Size;
  int MaxSize;
public:
  QapPool(int MaxSize=0):Size(0),MaxSize(MaxSize){Arr.resize(MaxSize);}
  void NewInstance(TYPE*&pVar)
  {
    QapAssert(Size<MaxSize);
    for(int i=0;i<Arr.size();i++)
    {
      if(!Arr[i].used)
      {
        Arr[i].used=true;
        Size++;
        pVar=&Arr[i].data;
        return;
      }
    }
  }
  void FreeInstance(TYPE*&pVar){
    QapAssert(Size>0);
    int id=int((int)pVar-(int)&Arr[0].data)/sizeof(Arr[0]);
    for(int i=id;i<Arr.size();i++)
    {
      if(&Arr[i].data==pVar)
      {
        Arr[i].used=false;
        Size--;
        pVar=NULL;
        return;
      }
    }
    QapAssert(pVar=NULL);
  }
  template<typename FUNC>
  void ForEach(FUNC&Func)
  {
    int c=Size;
    for(int i=0;i<Arr.size();i++)
    {
      if(!c)break;
      Rec&it=Arr[i];
      if(it.used)
        Func(&it.data);
    }
  }
};
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <set>
struct ProgramArgs {
  enum class Mode {
    Normal,
    ReplayIn,
    ReplayOut
  } mode = Mode::Normal;
  // Обязательные для Normal и ReplayOut
  string world_name;
  int num_players = 0;
  uint64_t seed_initial = 0;
  uint64_t seed_strategies = 0;
  // Файлы
  optional<string> replay_in_file;   // только для ReplayIn
  optional<string> replay_out_file;  // только для ReplayOut
  optional<string> state_file;       // опционально: файл начального состояния
  // GUI
  bool gui_mode = false;
  vector<string> player_names;
  int debug = -1;
  bool remote = false;
  int ports_from = -1;
  // Служебное
  bool show_help = false;
  bool show_version = false;
};
ProgramArgs g_args;
#include "qap_vec.inl"
inline bool CD_Rect2Point(vec2d A,vec2d B,vec2d P)
{
  vec2d &p=P;vec2d a(min(A.x,B.x),min(A.y,B.y)),b(max(A.x,B.x),max(A.y,B.y));
  return InDip(a.x,p.x,b.x)&&InDip(a.y,p.y,b.y);
}
#ifdef _WIN32
FILETIME get_systime_percise_as_filetime(){FILETIME ft;GetSystemTimePreciseAsFileTime(&ft);return ft;}
SYSTEMTIME filetime_to_systime(const FILETIME&ft){SYSTEMTIME st;FileTimeToSystemTime(&ft,&st);return st;}
SYSTEMTIME get_localtime(const SYSTEMTIME&src){SYSTEMTIME out;SystemTimeToTzSpecificLocalTime(0,&src,&out);return out;}
SYSTEMTIME filetime_to_localtime(const FILETIME&ft){return get_localtime(filetime_to_systime(ft));}
SYSTEMTIME get_percise_localtime(){return filetime_to_localtime(get_systime_percise_as_filetime());}
string systime_to_str(const SYSTEMTIME&st){
  #define F(VAR,FIELD)auto&VAR=st.FIELD;
  F(Y,wYear);
  F(M,wMonth);
  F(D,wDay);
  F(h,wHour);
  F(m,wMinute);
  F(s,wSecond);
  F(x,wMilliseconds);
  #undef F
  char buff[]="YYYY.MM.DD hh:mm:ss.xxx";
  //           0123456789 123456789 12
  constexpr int q=10;constexpr int Z='0';
  #define F()buff[id--]=Z+v%q;v/=q;
  {int v=Y;int id=+3;F()F()F()F()}
  {int v=M;int id=+6;F()F()      }
  {int v=D;int id=+9;F()F()      }
  {int v=h;int id=12;F()F()      }
  {int v=m;int id=15;F()F()      }
  {int v=s;int id=18;F()F()      }
  {int v=x;int id=22;F()F()F()   }
  #undef F
  return buff;
}
//string systime_to_str_v1(const SYSTEMTIME&st){
//  #define F(VAR,FIELD,OFFSET)auto VAR=st.FIELD+OFFSET;
//  F(Y,wYear,0);
//  F(M,wMonth,0);
//  F(D,wDay,0);
//  F(h,wHour,0);
//  F(m,wMinute,0);
//  F(s,wSecond,0);
//  F(ms,wMilliseconds,0);
//  #undef F
//  char buff[32];
//  sprintf(&buff[0],"%04u.%02u.%02u %02u:%02u:%02u.%03u\0",
//    Y,M,D,
//    h,m,s,ms
//  );
//  return buff;
//}
string filetime_to_localstr(const FILETIME&ft){return systime_to_str(filetime_to_localtime(ft));}
string local_cur_date_str_v4(){auto lt=get_percise_localtime();return systime_to_str(lt);}
#endif
template<class TYPE>
inline static TYPE max(const TYPE&a,const TYPE&b){
  return a>b?a:b;
}
template<class TYPE>
inline static TYPE min(const TYPE&a,const TYPE&b){
  return a<b?a:b;
}
template<typename TYPE,size_t COUNT>
inline size_t lenof(TYPE(&)[COUNT]){
  return COUNT;
}
inline static int max(int a,int b){
  return a>b?a:b;
}
inline static int min(int a,int b){
  return a<b?a:b;
}
inline static float max(float a,float b){
  return a>b?a:b;
}
inline static float min(float a,float b){
  return a<b?a:b;
}
inline static double max(double a,double b){
  return a>b?a:b;
}
inline static double min(double a,double b){
  return a<b?a:b;
}
#ifdef _WIN32
inline TScreenMode GetScreenMode(){
  HDC DC=GetDC(0);
  TScreenMode Res={GetDeviceCaps(DC,HORZRES),GetDeviceCaps(DC,VERTRES),GetDeviceCaps(DC,BITSPIXEL),GetDeviceCaps(DC,VREFRESH)};
  ReleaseDC(0,DC);
  return Res;
}
static bool IsKeyDown(int vKey){int i=GetAsyncKeyState(vKey);return i<0;}
class TQuad{
public:
  typedef TQuad SelfClass;
  int x,y,w,h;
  TQuad(){DoReset();}
  TQuad(const TQuad&){QapDebugMsg("TQuad is noncopyable");DoReset();}
  TQuad(TQuad&&R){operator=(std::move(R));}
  void operator=(TQuad&&R){
    if(&R==this)return;
    x=std::move(R.x);y=std::move(R.y);w=std::move(R.w);h=std::move(R.h);
  }
  void DoReset(){x=320;y=240;w=320;h=240;}
  void Set(int X,int Y,int W,int H){x=X;y=Y;w=W;h=H;}
  vec2i&GetPos(){return*(vec2i*)&x;}
  vec2i&GetSize(){return*(vec2i*)&w;}
  void SetAs(const vec2i&pos,const vec2i&size){GetPos()=pos;GetSize()=size;}
  RECT GetWinRect()const{return {x,y,x+w,y+h};}
  void SetWinRect(const RECT&rect){Set(rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top);}
  void SetSize(const SIZE&size){w=size.cx;h=size.cy;}
  static TQuad getFullScreen(){TQuad tmp;auto mode=GetScreenMode();tmp.Set(0,0,mode.W,mode.H);return tmp;}
  void setAtCenter(const TQuad&q){QapAssert(q.w>=w);QapAssert(q.h>=h);x=(q.w-w)/2;y=(q.h-h)/2;}
  void subpos(const TQuad&q){x-=q.x;y-=q.y;}
  void setAtCenterScreen(){setAtCenter(getFullScreen());}
};
#endif
#include "qap_io.inl"
#ifdef _WIN32
struct QapMat4:public D3DMATRIX{
  QapMat4(){}
  QapMat4(
    float m00,float m01,float m02,float m03,
    float m10,float m11,float m12,float m13,
    float m20,float m21,float m22,float m23,
    float m30,float m31,float m32,float m33
  ){
    m[0][0]=m00; m[0][1]=m01; m[0][2]=m02; m[0][3]=m03;
    m[1][0]=m10; m[1][1]=m11; m[1][2]=m12; m[1][3]=m13;
    m[2][0]=m20; m[2][1]=m21; m[2][2]=m22; m[2][3]=m23;
    m[3][0]=m30; m[3][1]=m31; m[3][2]=m32; m[3][3]=m33;
  }
  friend QapMat4 operator+(const QapMat4&m,const QapMat4&n){
    QapMat4 O;
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)O.m[i][j]=m.m[i][j]+n.m[i][j];
    return O;
  }
  friend QapMat4 operator-(const QapMat4&m,const QapMat4&n){
    QapMat4 O;
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)O.m[i][j]=m.m[i][j]-n.m[i][j];
    return O;
  }
  friend QapMat4 operator-(const QapMat4&m){
    QapMat4 O;
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)O.m[i][j]=-m.m[i][j];
    return O;
  }
  friend QapMat4 operator*(const QapMat4&m,const QapMat4&n){
    return QapMat4(
      m.m[0][0]*n.m[0][0]+m.m[0][1]*n.m[1][0]+m.m[0][2]*n.m[2][0]+m.m[0][3]*n.m[3][0],
      m.m[0][0]*n.m[0][1]+m.m[0][1]*n.m[1][1]+m.m[0][2]*n.m[2][1]+m.m[0][3]*n.m[3][1],
      m.m[0][0]*n.m[0][2]+m.m[0][1]*n.m[1][2]+m.m[0][2]*n.m[2][2]+m.m[0][3]*n.m[3][2],
      m.m[0][0]*n.m[0][3]+m.m[0][1]*n.m[1][3]+m.m[0][2]*n.m[2][3]+m.m[0][3]*n.m[3][3],
      m.m[1][0]*n.m[0][0]+m.m[1][1]*n.m[1][0]+m.m[1][2]*n.m[2][0]+m.m[1][3]*n.m[3][0],
      m.m[1][0]*n.m[0][1]+m.m[1][1]*n.m[1][1]+m.m[1][2]*n.m[2][1]+m.m[1][3]*n.m[3][1],
      m.m[1][0]*n.m[0][2]+m.m[1][1]*n.m[1][2]+m.m[1][2]*n.m[2][2]+m.m[1][3]*n.m[3][2],
      m.m[1][0]*n.m[0][3]+m.m[1][1]*n.m[1][3]+m.m[1][2]*n.m[2][3]+m.m[1][3]*n.m[3][3],
      m.m[2][0]*n.m[0][0]+m.m[2][1]*n.m[1][0]+m.m[2][2]*n.m[2][0]+m.m[2][3]*n.m[3][0],
      m.m[2][0]*n.m[0][1]+m.m[2][1]*n.m[1][1]+m.m[2][2]*n.m[2][1]+m.m[2][3]*n.m[3][1],
      m.m[2][0]*n.m[0][2]+m.m[2][1]*n.m[1][2]+m.m[2][2]*n.m[2][2]+m.m[2][3]*n.m[3][2],
      m.m[2][0]*n.m[0][3]+m.m[2][1]*n.m[1][3]+m.m[2][2]*n.m[2][3]+m.m[2][3]*n.m[3][3],
      m.m[3][0]*n.m[0][0]+m.m[3][1]*n.m[1][0]+m.m[3][2]*n.m[2][0]+m.m[3][3]*n.m[3][0],
      m.m[3][0]*n.m[0][1]+m.m[3][1]*n.m[1][1]+m.m[3][2]*n.m[2][1]+m.m[3][3]*n.m[3][1],
      m.m[3][0]*n.m[0][2]+m.m[3][1]*n.m[1][2]+m.m[3][2]*n.m[2][2]+m.m[3][3]*n.m[3][2],
      m.m[3][0]*n.m[0][3]+m.m[3][1]*n.m[1][3]+m.m[3][2]*n.m[2][3]+m.m[3][3]*n.m[3][3]
    );
  }
  friend QapMat4 operator*(const QapMat4&m,float x){
    QapMat4 O;
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)O.m[i][j]=x*m.m[i][j];
    return O;
  }
};
inline QapMat4 Transpose(const QapMat4&m){
  return QapMat4(
    m.m[0][0],m.m[1][0],m.m[2][0],m.m[3][0],
    m.m[0][1],m.m[1][1],m.m[2][1],m.m[3][1],
    m.m[0][2],m.m[1][2],m.m[2][2],m.m[3][2],
    m.m[0][3],m.m[1][3],m.m[2][3],m.m[3][3]
  );
}
inline QapMat4 RotateZ(float angle){
  float c=cosf(angle),s=sinf(angle);
  return QapMat4(
    +c,+s,+0,+0,
    -s,+c,+0,+0,
    +0,+0,+1,+0,
    +0,+0,+0,+1
  );
}
inline QapMat4 Translate(float x,float y,float z){
  return QapMat4(
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    x,y,z,1
  );
}
inline QapMat4 Scale(float x,float y,float z){
  return QapMat4(
    x,0,0,0,
    0,y,0,0,
    0,0,z,0,
    0,0,0,1
  );
}
inline QapMat4 Identity4(){
  return QapMat4(
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1
  );
}


class QapD3DPresentParameters{
public:
  typedef QapD3DPresentParameters SelfClass;
  D3DPRESENT_PARAMETERS pp;
  void DoReset(){pp=Zero(pp);}
  QapD3DPresentParameters(const QapD3DPresentParameters&){QapDebugMsg("QapD3DPresentParameters is noncopyable");DoReset();}
  QapD3DPresentParameters(){DoReset();}
  template<class TYPE> static TYPE&Zero(TYPE&inout){ZeroMemory(&inout,sizeof(inout)); return inout;}
  D3DPRESENT_PARAMETERS&SetToDef(HWND hWnd,bool VSync=true){
    Zero(pp);
    TScreenMode SM=GetScreenMode();
    pp.BackBufferWidth=SM.W; pp.BackBufferHeight=SM.H; pp.BackBufferFormat=D3DFMT_X8R8G8B8; pp.BackBufferCount=1;
    pp.SwapEffect=D3DSWAPEFFECT_COPY;
    pp.PresentationInterval=VSync?D3DPRESENT_INTERVAL_ONE:D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.hDeviceWindow=hWnd; pp.Windowed=true;
    pp.FullScreen_RefreshRateInHz=pp.Windowed?0:SM.Freq;
    return pp;
  }
};

class QapD3D9{
public:
  typedef QapD3D9 SelfClass;
  IDirect3D9* pD3D;
  void DoReset(){pD3D=nullptr;}
  QapD3D9(const QapD3D9&){QapDebugMsg("QapD3D9 is noncopyable");DoReset();}
  QapD3D9(){DoReset();}
  QapD3D9(QapD3D9&&Ref){DoReset(); oper_set(std::move(Ref));}
  ~QapD3D9(){Free();}
  void oper_set(QapD3D9&&Ref){Free(); this->pD3D=Ref.pD3D; Ref.pD3D=nullptr;}
  void operator=(QapD3D9&&Ref){oper_set(std::move(Ref));}
  void Init(){
    Free();
    pD3D=Direct3DCreate9(D3D_SDK_VERSION);
    if(!pD3D)QapDebugMsg("Cannot create Direct3D9");
  }
  void Free(){if(pD3D)pD3D->Release();}
};

class QapD3D9Resource{
public:
  virtual void OnLost()=0;
  virtual void OnReset()=0;
};

class QapD3D9ResourceList{
public:
  typedef QapD3D9ResourceList SelfClass;
  vector<QapD3D9Resource*> Arr;
  bool Lost;
  void DoReset(){Arr=vector<QapD3D9Resource*>(); Lost=false;}
  QapD3D9ResourceList(const QapD3D9ResourceList&){QapDebugMsg("QapD3D9ResourceList is noncopyable");DoReset();}
  QapD3D9ResourceList(){DoReset();}
  QapD3D9ResourceList(QapD3D9ResourceList&&Ref){DoReset(); oper_set(std::move(Ref));}
  ~QapD3D9ResourceList(){Free();}
  void oper_set(QapD3D9ResourceList&&Ref){
    Free();
    this->Arr=std::move(Ref.Arr);
    this->Lost=std::move(Ref.Lost);
  }
  void operator=(QapD3D9ResourceList&&Ref){oper_set(std::move(Ref));}
  void Free(){Lost=false; QapAssert(Arr.empty());}
  void OnLost(){Lost=true; for(int i=0;i<Arr.size();i++)Arr[i]->OnLost();}
  void OnReset(){Lost=false; for(int i=0;i<Arr.size();i++)Arr[i]->OnReset();}
  bool HasRes(QapD3D9Resource*pRes,int*pIndex=nullptr){
    for(int i=0;i<Arr.size();i++)if(Arr[i]==pRes){if(pIndex)*pIndex=i;return true;}
    if(pIndex)*pIndex=-1; return false;
  }
  void Reg(QapD3D9Resource*pRes){QapAssert(bool(pRes)); QapAssert(!HasRes(pRes)); Arr.push_back(pRes);}
  void UnReg(QapD3D9Resource*pRes){
    int i=-1; bool flag=HasRes(pRes,&i);
    if(!flag){QapAssert(flag); return;}
    std::swap(Arr[i],Arr.back()); Arr.pop_back();
  }
};

class QapD3DDev9{
public:
  typedef QapD3DDev9 SelfClass;
  QapD3DPresentParameters PresParams;
  IDirect3DDevice9* pDev;
  QapD3D9ResourceList Resources;
  bool DrawPass;
  void DoReset(){PresParams=QapD3DPresentParameters(); pDev=nullptr; Resources=QapD3D9ResourceList(); DrawPass=false;}
  QapD3DDev9(const QapD3DDev9&){QapDebugMsg("QapD3DDev9 is noncopyable");DoReset();}
  QapD3DDev9(){DoReset();}
  QapD3DDev9(QapD3DDev9&&Ref){DoReset(); oper_set(std::move(Ref));}
  ~QapD3DDev9(){Free();}
  void oper_set(QapD3DDev9&&Ref){
    Free();
    this->PresParams=std::move(Ref.PresParams);
    this->pDev=std::move(Ref.pDev); Ref.pDev=nullptr;
    this->Resources=std::move(Ref.Resources);
  }
  void operator=(QapD3DDev9&&Ref){oper_set(std::move(Ref));}
  void Init(HWND hWnd,QapD3D9&D3D){
    QapAssert(D3D.pD3D); Free();
    D3D.pD3D->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,hWnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&PresParams.pp,&pDev);
    if(!pDev)QapDebugMsg("Cannot create Direct3DDevice9");
  }
  void Free(){if(pDev)pDev->Release();}
  void ResetStates(){
    pDev->SetRenderState(D3DRS_LIGHTING,false);
    pDev->SetRenderState(D3DRS_ZENABLE,false);
    pDev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    pDev->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
    pDev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
    pDev->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    pDev->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
    pDev->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
  }
  bool BeginScene(){
    if(Resources.Lost){EndScene(); return false;}
    pDev->BeginScene();
    DrawPass=true;
    ResetStates();
    return true;
  }
  bool EndScene(){
    DrawPass=false;
    if(Resources.Lost){
      HRESULT hr=pDev->TestCooperativeLevel();
      if(hr!=D3DERR_DEVICENOTRESET)return false;
      pDev->Reset(&PresParams.pp);
      Resources.OnReset();
      return false;
    }
    pDev->EndScene();
    return !Resources.Lost;
  }
  bool Present(const RECT*pSource=nullptr,const RECT*pDest=nullptr){
    if(!Resources.Lost){
      Resources.Lost=D3DERR_DEVICELOST==pDev->Present(pSource,pDest,NULL,NULL);
      if(Resources.Lost)Resources.OnLost();
    }
    return !Resources.Lost;
  }
  enum BlendType{BT_NONE,BT_SUB,BT_ADD};
  enum AlphaType{AM_NONE,AM_GEQUAL_ONE,AM_NEQUAL_MAX};
  void Blend(BlendType Mode){
    pDev->SetRenderState(D3DRS_ALPHABLENDENABLE,Mode!=BT_NONE);
    if(Mode==BT_NONE)return;
    if(Mode==BT_SUB){
      pDev->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
      pDev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
      pDev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
      pDev->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
      pDev->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
      pDev->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
      pDev->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
      pDev->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
      pDev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
      pDev->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
      return;
    }
    if(Mode==BT_ADD){
      pDev->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
      pDev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
      return;
    }
  }
  void Alpha(AlphaType Mode){
    pDev->SetRenderState(D3DRS_ALPHATESTENABLE,Mode!=AM_NONE);
    if(Mode==AM_NONE)return;
    if(Mode==AM_GEQUAL_ONE){
      pDev->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATEREQUAL);
      pDev->SetRenderState(D3DRS_ALPHAREF,1);
      return;
    }
    if(Mode==AM_NEQUAL_MAX){
      pDev->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_NOTEQUAL);
      pDev->SetRenderState(D3DRS_ALPHAREF,255);
      return;
    }
  }
  static void OrthoLH(D3DMATRIX&out,float w,float h,float zn,float zf){
    QapMat4 mat=QapMat4(
      2.0/w,0,0,0,
      0,2.0/h,0,0,
      0,0,1/(zf-zn),0,
      0,0,-zn/(zf-zn),1
    );
    out=mat*Identity4();
  }
  void Set2D(vec2d CamPos=vec2d(0.0,0.0),real zoom=1.0,real ang=0.0,vec2i*pSize=nullptr){
    auto&pp=PresParams.pp;
    vec2i v=pSize?*pSize:vec2i(pp.BackBufferWidth,pp.BackBufferHeight);
    QapMat4 matProj(Identity4()), matView(Identity4());
    if((v.x%2)||(v.y%2)){
      if(v.x%2)CamPos.x+=0.5f;
      if(v.y%2)CamPos.y+=0.5f;
    }
    QapMat4 matWorld(Translate(-CamPos.x,-CamPos.y,0)*Scale(zoom,zoom,1.0)*RotateZ(ang));
    OrthoLH(matProj,float(v.x),float(v.y),-1.0,1.0);
    pDev->SetTransform(D3DTS_PROJECTION,&matProj);
    pDev->SetTransform(D3DTS_VIEW,&matView);
    pDev->SetTransform(D3DTS_WORLD,&matWorld);
  }
  void Clear2D(const QapColor&Color){pDev->Clear(0,NULL,D3DCLEAR_TARGET,Color,1.0f,0);}
};
#endif
class QapKeyboard
{
public:
  struct TKeyState
  {
    static const int MAX_KEY=263;
    bool data[MAX_KEY];
    TKeyState(){SetToDef();}
    void SetToDef(){for(int i=0;i<MAX_KEY;i++)data[i]=false;}
    bool&operator[](int index){
      if(!InDip<int>(0,index,MAX_KEY-1)){QapDebugMsg("bad index");static bool tmp;return tmp;}
      return data[index];
    }
    bool&operator()(int index){return (*this)[index];}
    const bool&operator()(int index)const{
      if(!InDip<int>(0,index,MAX_KEY-1)){QapDebugMsg("bad index");static bool tmp;return tmp;}
      return data[index];
    }
    const bool&operator[](int index)const{
      if(!InDip<int>(0,index,MAX_KEY-1)){QapDebugMsg("bad index");static bool tmp;return tmp;}
      return data[index];
    }
  };
public:
  typedef QapKeyboard SelfClass;
public:
  #define DEF_PRO_TEMPLATE_DATAIO()
  #define DEF_PRO_CLASSNAME()QapKeyboard
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(int,LastKey,0)\
  ADD(char,LastChar,0)\
  ADD(bool,News,false)\
  ADD(TKeyState,Down,{})\
  ADD(TKeyState,Changed,{})\
  //===
  #include "defprovar.inl"
  //===
public:
  vec2d MousePos;int zDelta=0;
public:
  void KeyUpdate(int Key, bool Value) {
    if (Value) LastKey = Key;
    Down[Key] = Value;
    Changed[Key] = true;
  }
  void CharUpdate(char c) { LastChar = c; News = true; }
  void Sync() { News = false; Changed.SetToDef(); }
  bool OnDown(int index) const { return Changed[index] && Down[index]; }
  bool OnUp(int index) const { return Changed[index] && !Down[index]; }
  vec2d get_dir_from_wasd_and_arrows(bool wasd=true,bool arrows=true)const{
    vec2d dp;
    auto dir_x=vec2d(1,0);
    auto dir_y=vec2d(0,1);
    #define F(dir,key_a,key_b)if((arrows&&Down[key_a])||(wasd&&(Down[key_b]))){dp+=dir;}
    F(-dir_x,VK_LEFT,'A');
    F(+dir_x,VK_RIGHT,'D');
    F(+dir_y,VK_UP,'W');
    F(-dir_y,VK_DOWN,'S');
    #undef F
    return dp;
  }
};
QapKeyboard*kb_v2,*kb_v3;
#ifdef _WIN32
class GlobalEnv{
public:
  HINSTANCE hInstance, hPrevInstance;
  LPSTR lpCmdLine;
  int nCmdShow;
  GlobalEnv(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
    :hInstance(hInstance),hPrevInstance(hPrevInstance),lpCmdLine(lpCmdLine),nCmdShow(nCmdShow)
  { GlobalEnv::Get(this); }
  static const GlobalEnv&Get(GlobalEnv*ptr=nullptr){
    static GlobalEnv*pGE=nullptr;
    if(!pGE)std::swap(pGE,ptr);
    QapAssert(pGE&&!ptr);
    return *pGE;
  }
private:
  GlobalEnv(const GlobalEnv&);
  void operator=(const GlobalEnv&);
};
class TD3DGameBox{
public:
  class TForm{
  public:
    class WndClassPair{
    public:
      WNDPROC Proc; HWND hWnd; int ID; bool Used;
      WndClassPair():Proc(nullptr),hWnd(nullptr),ID(0),Used(false){}
      WndClassPair(WNDPROC Proc,HWND hWnd,int ID,bool Used):Proc(Proc),hWnd(hWnd),ID(ID),Used(Used){}
      void Init(TForm*Owner){
        WndProcHeap::Get().Load(*this);
        QapAssert(!Used); QapAssert(!hWnd); Used=true;
        Proc((HWND)Owner,WMU_INIT,0,0);
      }
      void Free(TForm*Owner){
        QapAssert(!hWnd); QapAssert(Used); Used=false;
        Proc((HWND)Owner,WMU_FREE,0,0);
        WndProcHeap::Get().Save(*this);
      }
    };
    class WndProcHeap{
    public:
      vector<WndClassPair> Arr;
      void Load(WndClassPair&WinPair){QapAssert(!Arr.empty()); WndClassPair&WCP=Arr.back(); WinPair=std::move(WCP); Arr.pop_back();}
      void Save(WndClassPair&WinPair){Arr.push_back(std::move(WinPair));}
      static WndProcHeap&Get(){
        static WndProcHeap Heap; static bool flag=false;
        if(!flag){
          vector<WndClassPair>&Arr=Heap.Arr;
          {WndClassPair tmp(WndProc<0>,nullptr,0,false); Arr.push_back(tmp);}
          {WndClassPair tmp(WndProc<1>,nullptr,1,false); Arr.push_back(tmp);}
          flag=true;
        }
        return Heap;
      }
    };
    typedef TD3DGameBox OwnerClass;
    typedef TForm SelfClass;
    TD3DGameBox*Owner;
    ATOM ClassAtom;
    string ClassName;
    WndClassPair WinPair;
    void DoReset(){Owner=nullptr; ClassAtom=0; ClassName=""; WinPair=WndClassPair();}
    TForm(const TForm&){QapDebugMsg("TForm is noncopyable");DoReset();}
    TForm(){DoReset();}
    ~TForm(){if(WinPair.hWnd)Free();}
    void operator=(TD3DGameBox*Owner){this->Owner=Owner;}
    void Init(const string&name){
      WinPair.hWnd=nullptr;
      auto tmp=CreateWindowA((LPCSTR)ClassAtom,name.c_str(),WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,320,240,NULL,NULL,GlobalEnv::Get().hInstance,NULL);
      WinPair.hWnd=tmp; QapAssert(tmp);
    }
    void Free(){QapAssert(DestroyWindow(WinPair.hWnd)); WinPair.hWnd=nullptr;}
    void Reg(){
      WinPair.Init(this);
      ClassName="TD3DGameBox_"+IToS(WinPair.ID);
      WNDCLASSEX wcx; ZeroMemory(&wcx,sizeof(wcx));
      wcx.cbSize=sizeof(wcx); wcx.hInstance=GlobalEnv::Get().hInstance;
      wcx.hIcon=LoadIcon(0,IDI_ASTERISK); wcx.hIconSm=wcx.hIcon;
      wcx.hCursor=LoadCursor(0,IDC_ARROW);
      wcx.hbrBackground=(HBRUSH)GetStockObject(NULL_BRUSH);
      wcx.style=0;
      wcx.lpfnWndProc=WinPair.Proc;
      wcx.lpszClassName=ClassName.c_str();
      ClassAtom=RegisterClassExA(&wcx);
    }
    bool UnReg(){
      WinPair.Free(this);
      bool flag=UnregisterClassA((LPCSTR)ClassAtom,GlobalEnv::Get().hInstance);
      ClassAtom=0; ClassName.clear();
      return flag;
    }
    static const UINT WMU_INIT=WM_USER+1234;
    static const UINT WMU_FREE=WM_USER+4321;
    template<int Index>
    static LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){
      if(msg==WM_INPUTLANGCHANGEREQUEST) return DefWindowProcA(hWnd,msg,wParam,lParam);
      if(msg==WM_INPUTLANGCHANGE) return DefWindowProcA(hWnd,msg,wParam,lParam);
      if(msg==WM_WINDOWPOSCHANGING){static bool flag=true; if(flag)return 0;}
      static TForm*pForm=nullptr;
      if(!pForm){QapAssert(WMU_INIT==msg); pForm=(TForm*)hWnd; return 0;}
      if(WMU_FREE==msg){QapAssert(pForm); QapAssert(pForm->Owner); QapAssert(hWnd==((HWND)pForm)); pForm=nullptr; return 0;}
      QapAssert(pForm); QapAssert(pForm->Owner);
      long UD=GetWindowLongA(hWnd,GWL_USERDATA);
      if(!UD){SetWindowLong(hWnd,GWL_USERDATA,(long)pForm);}
      else{long pF=(long)pForm; QapAssert(pF==UD);}
      if(msg==WM_WINDOWPOSCHANGING){QapAssert("WTF?"); return 0;}
      auto result=pForm->Owner->WndProc(hWnd,msg,wParam,lParam);
      return result;
    }
  };
  typedef TD3DGameBox SelfClass;
  string Caption;
  bool NeedClose,Runned,Visible,FullScreen;
  TQuad Quad;
  TForm Form;
  TForm&win=Form;
  QapKeyboard Keyboard;
  int zDelta;
  vec2i mpos;
  void DoReset(){
    Caption="TD3DGameBox"; NeedClose=false; Runned=false; Visible=true; FullScreen=true;
    detail::FieldTryDoReset(Quad); Form=this; detail::FieldTryDoReset(Keyboard); zDelta=0; detail::FieldTryDoReset(mpos);
  }
  TD3DGameBox(const TD3DGameBox&){QapDebugMsg("TD3DGameBox is noncopyable");DoReset();}
  TD3DGameBox(){DoReset();}
  TD3DGameBox(TD3DGameBox&&_Right){operator=(std::move(_Right));}
  void operator=(TD3DGameBox&&_Right){
    if(&_Right==this)return;
    Caption=std::move(_Right.Caption); NeedClose=std::move(_Right.NeedClose); Runned=std::move(_Right.Runned);
    Visible=std::move(_Right.Visible); FullScreen=std::move(_Right.FullScreen); Quad=std::move(_Right.Quad);
    Form=std::move(_Right.Form); Keyboard=std::move(_Right.Keyboard); zDelta=std::move(_Right.zDelta); mpos=std::move(_Right.mpos);
  }
  bool IsWindow(){return Form.WinPair.hWnd;}
  void Init(const string&name){if(IsWindow())return; Form.Reg(); Form.Init(name); UpdateWnd(); Runned=true;}
  void UpdateWnd(){HWND&hWnd=Form.WinPair.hWnd; SetWindowTextA(hWnd,Caption.c_str());}
  void WindowMode(){
    HWND&hWnd=Form.WinPair.hWnd; TScreenMode SM=GetScreenMode();
    DWORD Style=true?WS_OVERLAPPED:WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN;
    SetWindowLong(hWnd,GWL_STYLE,Style);
    RECT Rect=Quad.GetWinRect(); AdjustWindowRect(&Rect,Style,false);
    SetWindowPos(hWnd,0,Quad.x,Quad.y,Quad.w,Quad.h,SWP_FRAMECHANGED|SWP_NOOWNERZORDER);
    ShowWindow(hWnd,SW_MAXIMIZE); ShowWindow(hWnd,Visible?SW_SHOW:SW_HIDE);
  }
  ~TD3DGameBox(){Free();}
  void Free(){
    if(!IsWindow())return;
    Runned=false; Form.Free(); Form.UnReg(); NeedClose=false;
  }
  void Close(){
    NeedClose=true;
  }
  void ScanWinPlacement(){
    WINDOWPLACEMENT PL; GetWindowPlacement(Form.WinPair.hWnd,&PL);
    Quad.SetWinRect(PL.rcNormalPosition); QapAssert(Quad.x>-1000);
  }
  static bool Equal(RECT&a,RECT&b){
    return a.left==b.left&&a.top==b.top&&a.right==b.right&&a.bottom==b.bottom;
  }
  static void KeyboardUpdate(QapKeyboard&Keyboard,const HWND hWnd,const UINT msg,const WPARAM wParam,const LPARAM lParam){
    bool any_key_up=(msg==WM_KEYUP)||(msg==WM_SYSKEYUP);
    bool any_key_down=(msg==WM_KEYDOWN)||(msg==WM_SYSKEYDOWN);
    //if(msg==WM_ACTIVATE){Keyboard=std::move(QapKeyboard());}
    if(any_key_up||any_key_down){
      bool value=any_key_down;
      auto&KB=Keyboard; auto&Down=Keyboard.Down;
      auto KeyUpdate=[&KB,&Down](int key,const bool value)->void{
        if(Down[key]==value)return;
        if(value!=IsKeyDown(key))return;
        KB.KeyUpdate(key,value);
      };
      struct TSysKey{int Key,LKey,RKey;} key_array[]={{VK_SHIFT,VK_LSHIFT,VK_RSHIFT},{VK_CONTROL,VK_LCONTROL,VK_RCONTROL},{VK_MENU,VK_LMENU,VK_RMENU}};
      for(int i=0;i<lenof(key_array);i++){
        auto&ex=key_array[i];
        if(wParam==ex.Key){KeyUpdate(ex.LKey,value); KeyUpdate(ex.RKey,value);}
      }
    }
    if((msg==WM_KEYUP)&&(wParam==VK_SNAPSHOT)){Keyboard.KeyUpdate(VK_SNAPSHOT,true); Keyboard.KeyUpdate(VK_SNAPSHOT,false);}
    if(any_key_up){Keyboard.KeyUpdate(wParam,false);}
    if(any_key_down){Keyboard.KeyUpdate(wParam,true);}
    if(msg==WM_LBUTTONUP){
      ReleaseCapture();
      g_log.push_back(FToS(g_sys_clock.MS())+":mbLeft-up");
      Keyboard.KeyUpdate(mbLeft,false);
    }
    if(msg==WM_RBUTTONUP){
      g_log.push_back(FToS(g_sys_clock.MS())+":mbRight-up");
      Keyboard.KeyUpdate(mbRight,false);
    }
    if(msg==WM_MBUTTONUP){Keyboard.KeyUpdate(mbMiddle,false);}
    if(msg==WM_LBUTTONDOWN){
      SetCapture(hWnd);
      g_log.push_back(FToS(g_sys_clock.MS())+":mbLeft-down");
      Keyboard.KeyUpdate(mbLeft,true);
    }
    if(msg==WM_RBUTTONDOWN){
      g_log.push_back(FToS(g_sys_clock.MS())+":mbRight-down");
      Keyboard.KeyUpdate(mbRight,true);
    }
    if(msg==WM_MBUTTONDOWN){Keyboard.KeyUpdate(mbMiddle,true);}
    if((msg==WM_CHAR)||(msg==WM_SYSCHAR)){Keyboard.CharUpdate(wParam);}
  }
  LRESULT WndProc(const HWND hWnd,const UINT msg,const WPARAM wParam,const LPARAM lParam){
    KeyboardUpdate(Keyboard,hWnd,msg,wParam,lParam);
    if(msg==WM_MOUSEWHEEL){zDelta=GET_WHEEL_DELTA_WPARAM(wParam);}
    if(msg==WM_CHAR){auto kbl=GetKeyboardLayout(0); auto ENGLISH=0x409; auto RUSSIAN=0x419; char c=wParam; int gg=1;}
    if(msg==WM_CLOSE){Close(); if(Runned)return 0;}
    if(msg==WM_SHOWWINDOW){Visible=wParam;}
    if(msg==WM_ERASEBKGND){DoPaint();}
    //if(Keyboard.OnUp(VK_ESCAPE)){Close();}
    return DefWindowProcA(hWnd,msg,wParam,lParam);
  }
  void DoPaint(){
    HWND hWnd=Form.WinPair.hWnd; HDC DC=GetDC(hWnd);
    RECT Rect; GetClientRect(Form.WinPair.hWnd,&Rect);
    FillRect(DC,&Rect,(HBRUSH)(COLOR_BTNFACE)); ReleaseDC(hWnd,DC);
  }
  vec2i GetClientSize(){
    RECT Rect; GetClientRect(Form.WinPair.hWnd,&Rect);
    return vec2i(Rect.right-Rect.left,Rect.bottom-Rect.top);
  }
  vec2i GetMousePos(){return mpos;}
  vec2i unsafe_GetMousePos(){
    POINT P; GetCursorPos(&P); auto hWnd=Form.WinPair.hWnd; ScreenToClient(hWnd,&P);
    vec2i p(P.x,-P.y); vec2i z=GetClientSize();
    return p-vec2i(z.x/2,-z.y/2);
  }
};
#endif
#ifdef _WIN32
void RegTex(...){}
void UnRegTex(...){}
#else
#ifndef QAP_UNIX
void bindTex(/*QapDev&qDev,*/int Tex){
  EM_ASM({bindTex(qDev,$0);},Tex);
}
#endif
typedef unsigned int DWORD;
#endif
void RegTexMem(...){}
void UnRegTexMem(...){}
class QapTexMem
{
public:
  QapColor*pBits;
  int W,H;
  string Name;
  int ID;
public:
  QapTexMem(const string&name):pBits(NULL),W(0),H(0),Name(name){RegTexMem(this);};
  QapTexMem(const string&name,int w,int h,QapColor *pbits):pBits(pbits),W(w),H(h),Name(name){RegTexMem(this);};
  void SaveToFile(const string&FN);
  ~QapTexMem(){UnRegTexMem(this);delete[] pBits;};
  QapTexMem*Clone(){
    return new QapTexMem(Name+".Clone",W,H,(QapColor*)memcpy((void*)new QapColor[W*H],pBits,sizeof(QapColor)*W*H));
  }
  QapColor get_color_at(int x,int y)const{return pBits[x+y*W];}
public:
  class IPixelVisitor{
  public:
    virtual void Visit(const vec2i&p){}
  };
  class ITraceVisitor{
  public:
    virtual bool Visit(const vec2i&p){return true;}
    virtual bool NextWave(){return true;}
  };
  inline bool IsValid(const vec2i&p){return InDip(0,p.x,W-1)&&InDip(0,p.y,H-1);}
  inline QapColor&operator[](const vec2i&p){return pBits[p.x+p.y*W];}
  void Accept(const vec2i&p,IPixelVisitor&Visitor)
  {
    #define N(A,B)
    #define F(A,B){vec2i np=p+vec2i(A,B);if(IsValid(np))Visitor.Visit(np);};;;
    N(-1,-1)F(+0,-1)N(+1,-1)
    F(-1,+0)N(+0,+0)F(+1,+0)
    N(-1,+1)F(+0,+1)N(+1,+1)
    #undef F
    #undef N
  }
  QapTexMem*GenEdge(const vec2i&p,const QapColor&color=0xFF000000)
  {
    //15:27 06.07.2011
    QapTexMem*pDest=Clone()->Clear(0xffffffff);
    class TraceVisitor:public ITraceVisitor{
    public:
      QapTexMem*pSrc;
      QapTexMem*pDest;
      QapColor color;
      TraceVisitor(QapTexMem*pSrc,QapTexMem*pDest,const QapColor&color):pSrc(pSrc),pDest(pDest){}
    public:
      bool Visit(const vec2i&p){
        QapColor&S=pSrc->operator[](p);
        QapColor&D=pDest->operator[](p);
        //if(&D==&S)exit(0);
        bool flag=S==color;
        if(flag)D=0xffffffff;
        if(!flag)D=0xff000000;
        return flag;
      }
    }TV(this,pDest,color);
    Accept(p,TV);
    return pDest->InvertRGB();
  }
  void Accept(const vec2i&p,ITraceVisitor&Visitor)
  {  
    class Tracer{
    public:
      vector<vec2i>NArr,DArr,Arr,IA;
      vector<int>Check;
    public:
      ITraceVisitor&Visitor;
      QapTexMem*pMem;
      Tracer(ITraceVisitor&Visitor,QapTexMem*pMem,const vec2i&p):pMem(pMem),Visitor(Visitor){NArr.push_back(p);Init();}
      void Init()
      {
        Check.resize(pMem->W*pMem->H);
        for(int i=0;i<Check.size();i++)Check[i]=1;
      }
    public:
      class PixelVisitor:public IPixelVisitor{
      public:
        Tracer*pT;
        PixelVisitor(Tracer*pT):pT(pT){}
        ~PixelVisitor(){}
        void Visit(const vec2i&p){
          vector<vec2i>*Arr[2]={&pT->DArr,&pT->NArr};
          bool Accepted=!!pT->Visitor.Visit(p);
          Arr[Accepted]->push_back(p);
        }
      };
      int GetID(const vec2i&p){return p.x+p.y*pMem->W;}
      void Run()
      {
        while(!NArr.empty()&&Visitor.NextWave())
        {
          Arr=NArr;NArr.clear();DArr.clear();
          for(int j=0;j<Arr.size();j++)
          {
            vec2i&p=Arr[j];
            int ID=GetID(p);
            if(ID<0||!Check[ID])continue;
            PixelVisitor PV(this);
            pMem->Accept(p,PV);
            Check[ID]=false;
          }
        }
      }
    } tracer(Visitor,this,p);
    tracer.NArr.push_back(p);
    tracer.Run();  
  }
public:
  QapTexMem*FillBorder(int x,int y,QapTexMem*Source,int n=4)
  {
    if(!Source||!Source->pBits)return this;
    int sW=Source->W;
    int sH=Source->H;
    int mX=min(W,x+sW);
    int mY=min(H,y+sH);
    QapColor*pT=this->pBits;
    QapColor*pS=Source->pBits;
    #define F(i,j)pS[(i-x)+(j-y)*sW]
    for(int k=1;k<=n;k++)
    {
      {int j=00+y;if(InDip<int>(0,j-k,H-1))for(int i=max(0,x);i<mX;i++){pT[i+(j-k)*W]=F(i,j);}}
      {int j=mY-1;if(InDip<int>(0,j+k,H-1))for(int i=max(0,x);i<mX;i++){pT[i+(j+k)*W]=F(i,j);}}
      {int i=00+x;if(InDip<int>(0,i-k,W-1))for(int j=max(0,y);j<mY;j++){pT[(i-k)+j*W]=F(i,j);}}
      {int i=mX-1;if(InDip<int>(0,i+k,W-1))for(int j=max(0,y);j<mY;j++){pT[(i+k)+j*W]=F(i,j);}}
    }
    #undef F
    return this;
  }
  QapTexMem*FillMem(int x,int y,QapTexMem*Source)
  {
    if(!Source||!Source->pBits)return this;
    int sW=Source->W;
    int sH=Source->H;
    int mX=min(W,x+sW);
    int mY=min(H,y+sH);
    QapColor*pT=this->pBits;
    QapColor*pS=Source->pBits;
    for(int j=max(0,y);j<mY;j++){
      for(int i=max(0,x);i<mX;i++){
        pT[i+j*W]=pS[(i-x)+(j-y)*sW];
      }
    }
    return this;
  }
  QapTexMem*Clear(const QapColor&color=0xFF000000){
    for(int i=0;i<W*H;i++)pBits[i]=color;
     return this;
  }
  QapTexMem*FillLine(const int Line,const QapColor&Color){
    for(int i=0;i<W;i++){QapColor&pix=pBits[Line*W+i];pix=Color;}
    return this;
  }
  QapTexMem*Circle(const QapColor&color,int x,int y,int r){
    vec2i pos(x,y);
    int rr=r*r;
    int i0=max(0,x-r);
    int iz=min(W-1,x+r);
    int j0=max(0,y-r);
    int jz=min(H-1,y+r);
    for(int j=j0;j<=jz;j++)
      for(int i=i0;i<=iz;i++)
      {
        int mm=abs(r-(vec2i(i,j)-pos).SqrMag());
        if(mm<rr)
        {
          QapColor&FragColor=pBits[j*W+i];
          FragColor=color;
        }
      }
    return this;
  }
  QapTexMem*Draw(int x,int y,QapTexMem*Source,const QapColor&color){
    if(!Source||!Source->pBits)return this;
    int sW=Source->W;
    int sH=Source->H;
    int mX=min(W,x+sW);
    int mY=min(H,y+sH);
    QapColor*pT=this->pBits;
    QapColor*pS=Source->pBits;
    for(int j=max(0,y);j<mY;j++){
      for(int i=max(0,x);i<mX;i++){
        QapColor src=Source->pBits[(i-x)+(j-y)*sW]*color;
        QapColor&dest=pBits[j*W+i];
        dest=QapColor::Mix(dest,src,real(src.a)/255.0);
        //dest.a=;
      }
    }
    return this;
  }
  QapTexMem*Circle(const QapColor&color,int x,int y,int r,int hs){
    vec2i pos(x,y);
    int minr=r-hs;
    int maxr=r+hs;
    int r1=minr*minr;
    int r2=maxr*maxr;
    int i0=max(0,x-r-hs);
    int iz=min(W-1,x+r+hs);
    int j0=max(0,y-r-hs);
    int jz=min(H-1,y+r+hs);
    for(int j=j0;j<=jz;j++)
      for(int i=i0;i<=iz;i++)
      {
        int mm=abs(r-(vec2i(i,j)-pos).SqrMag());
        if(InDip(r1,mm,r2))
        {
          QapColor&FragColor=pBits[j*W+i];
          FragColor=color;
        }
      }
    return this;
  }
  QapTexMem*FillColomn(const int Colomn,const QapColor&Color){
    for(int i=0;i<H;i++)pBits[W*i+Colomn]=Color;
    return this;
  }
  QapTexMem*CalcAlphaToRGB_and_set_new_alpha(uchar new_alpha=uchar(0xff)){
    for(int i=0;i<W*H;i++){auto a=pBits[i].a;pBits[i]=QapColor(new_alpha,a,a,a);}
    return this;
  }
  QapTexMem*CalcAlphaToRGB_and_keep_alpha(){
    for(int i=0;i<W*H;i++){auto a=pBits[i].a;pBits[i]=QapColor(a,a,a,a);}
    return this;
  }
  QapTexMem*CalcAlpha(){
    for(int i=0;i<W*H;i++)pBits[i].a=pBits[i].GetLuminance();
    return this;
  }
  template<class FUNC>
  QapTexMem*ForEachPixel(FUNC&&func){
    for(int i=0;i<W*H;i++)func(pBits[i]);
    return this;
  }
  QapTexMem*CopyAlpha(QapTexMem*Alpha){
    if(Alpha->W!=W||Alpha->H!=H)return this;
    for(int i=0;i<W*H;i++)pBits[i].a=Alpha->pBits[i].a;
    return this;
  }
  QapTexMem*CalcAlpha(const QapColor&Color,int threshold=0){
    for(int i=0;i<W*H;i++)pBits[i].a=pBits[i]==Color?0:255;
    const QapColor&c=Color;
    int t=threshold;
    int t3=t*3;
    for(int i=0;i<W*H;i++){
      QapColor&p=pBits[i];
      #define F(r)int f##r=abs(int(p.r)-int(c.r));
      F(r);F(g);F(b);
      #undef F
      //int esqr=fr*fr+fg*fg+fb*fb;
      int sum=fr+fg+fb;
      p.a=sum>t3?255:t3?255*sum/t3:0;
    }
    return this;
  }
  QapTexMem*FillChannel(const QapColor&Source,DWORD BitMask){
    for(int i=0;i<W*H;i++){QapColor&C=pBits[i];C=(~BitMask&C)|(BitMask&Source);};
    return this;
  }
  QapTexMem*InvertRGB(){for(int i=0;i<W*H;i++){QapColor&C=pBits[i];C.r=~C.r;C.g=~C.g;C.b=~C.b;}return this;}
  Dip2i GetDipAlpha(){Dip2i Dip(256,-1);for(int i=0;i<W*H;i++)Dip.Take(pBits[i].a);return Dip;}
  Dip2i GetDipR(){Dip2i Dip(256,-1);for(int i=0;i<W*H;i++)Dip.Take(pBits[i].r);return Dip;}
  Dip2i GetDipG(){Dip2i Dip(256,-1);for(int i=0;i<W*H;i++)Dip.Take(pBits[i].g);return Dip;}
  Dip2i GetDipB(){Dip2i Dip(256,-1);for(int i=0;i<W*H;i++)Dip.Take(pBits[i].b);return Dip;}
  QapTexMem*NormRGB(){
    typedef Dip2i::Transform D2iXF;
    Dip2i Cur[4]={GetDipR(),GetDipG(),GetDipB(),GetDipAlpha()};
    Dip2i cur(256,-1);for(int i=0;i<4;i++){cur.Take(Cur[i].a);cur.Take(Cur[i].b);}
    Dip2i norm(0,255);
    D2iXF xf(cur,norm);
    D2iXF arr[4]={D2iXF(Cur[0],norm),D2iXF(Cur[1],norm),D2iXF(Cur[2],norm),D2iXF(Cur[3],norm)};
    for(int i=0;i<W*H;i++){
      QapColor&C=pBits[i];
      C.r=uchar(xf*C.r);
      C.g=uchar(xf*C.g);
      C.b=uchar(xf*C.b);
    }
    return this;
  }
  QapTexMem*NormAlpha(){
    Dip2i Cur=GetDipAlpha();
    Dip2i Norm(0,255);
    Dip2i::Transform xf(Cur,Norm);
    for(int i=0;i<W*H;i++)
    {
      QapColor&C=pBits[i];
      C.a=uchar(xf*C.a);
    }
    return this;
  }
  QapTexMem*InvertY(){
    QapColor*line=new QapColor[W];
    size_t Size=sizeof(QapColor)*W;
    for(int i=0;i<H/2;i++)
    {
      int a(W*(i)),b(W*(H-i-1));
      memcpy((void*)line,&pBits[a],Size);
      memcpy((void*)&pBits[a],&pBits[b],Size);
      memcpy((void*)&pBits[b],line,Size);
    }
    delete[] line;
    return this;
  }
  QapTexMem*InvertX(){
    for(int i=0;i<H;i++)
    {
      QapColor*line=&pBits[i];
      for(int j=0;j<W/2;j++)
      {
        int a(j),b(H-j-1);
        std::swap(line[a],line[b]);
      }
    }
    return this;
  }
  static float gauss(float x,float sigma)
  {
    float x_sqr=x*x;
    float sigma_sqr=sigma*sigma;
    float sqrt_value=1.0/sqrt(2.0*Pi*sigma_sqr);
    float exp_value=exp(-x_sqr/(2.0*sigma_sqr));
    return sqrt_value*exp_value;
  }/*
  static void PascalRow(IntArray&arr,int n)
  {
    arr.resize(n+1);
    for(int i=0;i<arr.size();i++)arr[i]=0;
    int*c=&arr[0];
    c[0]=1;
    for(int j=1;j<n;j++)for(int i=j;i>=1;i--)c[i]=c[i-1]+c[i];
  }*/
  private:
    inline vec4f tex2Df(vec4f*C,const vec2i&uv){return C[Clamp(uv.x,int(0),int(W-1))+W*Clamp(uv.y,int(0),int(H-1))];}
    inline vec4f tex2D(QapColor*C,const vec2i&uv){return vec4f(C[Clamp(uv.x,int(0),int(W-1))+W*Clamp(uv.y,int(0),int(H-1))]);}
    inline vec4i tex2Di(QapColor*C,const vec2i&uv){return vec4i(C[Clamp(uv.x,int(0),int(W-1))+W*Clamp(uv.y,int(0),int(H-1))]);}
    void DirBlur(vec4f*SysMem,vec4f*SysOut,float radius,const vec2i&texrad){
      float sigma=radius/3.0;
      vector<float> garr;
      for(int x=0;x<=radius;x++){
        garr.push_back(gauss(float(x),sigma));
      }
      float*g=&garr[0];
      for(int j=0;j<H;j++)
        for(int i=0;i<W;i++)
        {
          vec4f&FragColor=SysOut[j*W+i];
          vec2i texcoord(i,j);
          float sum=0.0;
          int x=1;
          vec4f value(0,0,0,0);
          for(int x=-radius;x<+radius-1;x++)if(x)
          {
            float currentScale=g[abs(x)];
            sum+=currentScale;
            vec2i dudv=x*texrad;
            value+=currentScale*tex2Df(SysMem,texcoord+dudv);
          }
          value+=(1.f-sum)*tex2Df(SysMem,texcoord);
          FragColor=value;
        }
    }
  public:
  QapTexMem*Blur(const float&radius)
  {
    static vec4f SysMem[1024*1024*4];
    vec4f*tmp=&SysMem[W*H];
    for(int i=0;i<W*H;i++)SysMem[i]=vec4f(pBits[i].b,pBits[i].g,pBits[i].r,pBits[i].a);
    //memcpy_s(SysMem,sizeof(SysMem),pBits,W*H*sizeof(QapColor));//copy image
    DirBlur(SysMem,tmp,radius,vec2i(1,0));
    DirBlur(tmp,SysMem,radius,vec2i(0,1));
    for(int i=0;i<W*H;i++)pBits[i]=QapColor(SysMem[i].a,SysMem[i].r,SysMem[i].g,SysMem[i].b);
    return this;
  }
};
class QapTex{
public:
  #ifdef _WIN32
  IDirect3DTexture9*Tex;
  #else
  int Tex;
  #endif
  int W,H;
  string Name;
  int ID;
public:
  #ifdef _WIN32
  QapTex(QapTexMem*TexMem,IDirect3DTexture9*Tex):Tex(Tex){W=TexMem->W;H=TexMem->H;Name=TexMem->Name;RegTex(this);};
  ~QapTex(){
    UnRegTex(this);
    Tex->Release();
    Tex=0;
  };
  #else
  QapTex(QapTexMem*TexMem,int Tex):Tex(Tex){W=TexMem->W;H=TexMem->H;Name=TexMem->Name;/*RegTex(this);*/};
  ~QapTex(){
  };
  #endif
};
struct t_quad{
  vec2d pos;
  vec2d size;
  t_quad&set(const vec2d&p,const vec2d&s){pos=p;size=s;return *this;}
  vec2d get_vertex_by_dir(const vec2d&dir)const{return pos+vec2d::sign(dir).Mul(size)*0.5;}
  vec2d get_left_top()const{return pos+vec2d(-1,+1).Mul(size)*0.5;}
  void set_pos_by_vertex_and_dir(const vec2d&vertex,const vec2d&dir)
  {
    auto d=vec2d::sign(dir);
    //QapAssert(d.x&&d.y);
    pos=vertex+d.Mul(size)*0.5;
  }
  t_quad add_size(real ds)const{t_quad q=*this;q.size+=vec2d(ds,ds);return q;}
  t_quad add_size(vec2d size)const{t_quad q=*this;q.size+=size;return q;}
  t_quad add_pos(const vec2d&pos)const{t_quad q=*this;q.pos+=pos;return q;}
  vec2d clamp(const vec2d&point)const
  {
    return clamp(point-pos,size)+pos;
  }
  static vec2d clamp(const vec2d&p,const vec2d&size)
  {
    return vec2d(Clamp<real>(p.x,-size.x*0.5,+size.x*0.5),Clamp<real>(p.y,-size.y*0.5,+size.y*0.5));
  }
};

static bool point_in_quad(t_quad q, vec2d p)
{
  auto s=q.size*0.5;
  auto o=p-q.pos;
  return abs(s.x)>=abs(o.x)&&abs(s.y)>=abs(o.y);
}

static vec2d join_quads_by_y(const vector<t_quad>&arr)
{
  real total_y=0; real max_x=0;
  for(int i=0;i<arr.size();i++){auto&s=arr[i].size;total_y+=s.y;max_x=std::max<real>(max_x,s.x);}
  return vec2d(max_x,total_y);
}
static vec2d join_quads_by_x(const vector<t_quad>&arr)
{
  real total_x=0; real max_y=0;
  for(int i=0;i<arr.size();i++){auto&s=arr[i].size;total_x+=s.x;max_y=std::max<real>(max_y,s.y);}
  return vec2d(total_x,max_y);
}
static vec2d join_quads_by_dir(const vector<t_quad>&arr,char dir='x')
{
  if(dir=='x')return join_quads_by_x(arr);
  if(dir=='y')return join_quads_by_y(arr);
  QapNoWay();
  return {};
}
static vec2d join_quads_by_y_ex(const vector<t_quad>&arr,real space){return join_quads_by_y(arr)+vec2d(0,space*std::max<int>(0,int(arr.size())-1));}
static vec2d join_quads_by_x_ex(const vector<t_quad>&arr,real space){return join_quads_by_x(arr)+vec2d(space*std::max<int>(0,int(arr.size())-1),0);}

static vec2d join_quads_by_dir_ex(const vector<t_quad>&arr,char dir='x',real space=0)
{
  if(dir=='x')return join_quads_by_x_ex(arr,space);
  if(dir=='y')return join_quads_by_y_ex(arr,space);
  QapNoWay();
  return {};
}
static void layout_by_dir(vector<t_quad>&arr,char dir='x',real space=0)
{
  QapAssert(dir=='x'||dir=='y');
  t_quad q;
  q.size=join_quads_by_dir_ex(arr,dir,space);
  vec2d vdir=dir=='x'?vec2i(+1,0):vec2d(0,-1);
  auto p=q.get_vertex_by_dir(-vdir);
  for(int i=0;i<arr.size();i++)
  {
    auto&ex=arr[i];
    ex.set_pos_by_vertex_and_dir(p,vdir);
    auto dp=dir=='x'?vec2d(space+ex.size.x,0):vec2d(0,-space-ex.size.y);
    p+=dp;
  }
}
static vector<t_quad> split_quad_by_dir(t_quad q,char dir='x',int n=2)
{
  vector<t_quad> arr;
  vec2d dv=vec2d(dir=='x'?1:0,dir=='y'?1:0);
  vec2d dn=vec2d(dv.y,dv.x);
  auto d=dv*(1.0/n)+dn;
  auto size=q.size.Mul(d);
  for(int i=0;i<n;i++)qap_add_back(arr).size=size;
  layout_by_dir(arr,dir,0.0);
  return arr;
}
class QapDev{
public:
  struct Ver
  {
    float x,y,z;
    QapColor color;
    float tu,tv;
    Ver():x(0),y(0),z(0),tu(0),tv(0) {}
    Ver(float x,float y,const QapColor&color,float u=0.0,float v=0.0):x(x),y(y),z(0),color(color),tu(u),tv(v) {}
    Ver(const vec2f&pos,const QapColor&color,float u=0.0,float v=0.0):x(pos.x),y(pos.y),z(0),color(color),tu(u),tv(v) {}
    Ver(const vec2f&pos,const QapColor&color,const vec2f&texcoord):x(pos.x),y(pos.y),z(0),color(color),tu(texcoord.x),tv(texcoord.y) {}
    vec2f&get_pos()
    {
      return *(vec2f*)&x;
    }
    vec2f&get_pos()const
    {
      return *(vec2f*)&x;
    }
    vec2f&get_tpos()
    {
      return *(vec2f*)&tu;
    }
    vec2f&get_tpos()const
    {
      return *(vec2f*)&tu;
    }
  };
  struct BatchScope
  {
    QapDev&RD;
    bool flag;
    BatchScope(QapDev&RD):RD(RD)
    {
      flag=!RD.Batching;
      if(flag)RD.BeginBatch();
    }
    ~BatchScope()
    {
      if(flag)RD.EndBatch();
    }
  };
  struct BatchScopeEx
  {
    QapDev&RD;
    bool flag;
    bool drawpass;
    BatchScopeEx(QapDev&RD,bool drawpass):RD(RD)
    {
      if(!drawpass)return;
      flag=!RD.Batching;
      if(flag)RD.BeginBatch();
    }
    ~BatchScopeEx()
    {
      if(!drawpass)return;
      if(flag)RD.EndBatch();
    }
  };
  struct ViewportScope
  {
    QapDev&RD;
    const t_quad&viewport;
    bool auto_clamp;
    int vpos;
    ViewportScope(QapDev&RD,const t_quad&viewport):RD(RD),viewport(viewport),vpos(0)
    {
      vpos=0;
      auto_clamp=RD.auto_clamp;
      if(auto_clamp)vpos=RD.VPos;
      RD.push_viewport(viewport);
    }
    ~ViewportScope()
    {
      if(auto_clamp)
      {
        for(int i=vpos;i<RD.VPos;i++)
        {
          auto&p=RD.VBA[i].get_pos();
          p=RD.viewport.clamp(p);
        }
      }
      RD.pop_viewport();
    }
  };
public:
  typedef QapDev SelfClass;
public:
#ifdef _WIN32
  class TDynamicResource:public QapD3D9Resource{
  public:
    typedef QapDev OwnerClass;
    void operator=(OwnerClass*pOwner){
      this->pOwner=pOwner;
    }
  public:
    TDynamicResource():pOwner(nullptr),flag(false),pDev(nullptr){
      if(pDev)pDev->Resources.Reg(this);
    };
    ~TDynamicResource(){
      Free();
    };
  public:
    void Free(){
      if(pDev){pDev->Resources.UnReg(this);pDev=nullptr;}
    }
  public:
    QapD3DDev9*pDev;
    void Mount(OwnerClass*pOwner,QapD3DDev9*pDev)
    {
      QapAssert(pOwner);
      QapAssert(pDev);
      this->pOwner=pOwner;
      this->pDev=pDev;
      if(pDev)pDev->Resources.Reg(this);
    }
  public:
    OwnerClass*pOwner;
    bool flag;
    void OnLost()
    {
      if(pOwner)
      {
        flag=bool(pOwner->VB);
        pOwner->Free();
      }
      else
      {
        QapDebugMsg("fail");
        flag=false;
      }
    }
    void OnReset()
    {
      if(pOwner&&flag)pOwner->ReInit();
    }
  };
public:
  typedef QapD3DDev9::BlendType BlendType;
  typedef QapD3DDev9::AlphaType AlphaType;
public:
  TDynamicResource DynRes;
  IDirect3DDevice9*pDev;
  IDirect3DVertexBuffer9*VB;
  IDirect3DIndexBuffer9*IB;
#else
  vector<Ver> VB;
  vector<int> IB;
#endif
  Ver*VBA{};
  int*IBA{};
  QapColor color;
  int VPos=0;
  int IPos=0;
  int MaxVPos=0;
  int MaxIPos=0;
  int DIPs=0;
  int Verts=0;
  int Tris=0;
  //BlendType BlendMode;
  //AlphaMode AlphaMode;
  bool Batching=false;
  bool Textured=false;
  bool use_xf=false;
  b2Transform xf,txf;
  vector<t_quad> stack;
  t_quad viewport;
  bool auto_clamp=false;
  bool use_viewport=false;
  //static const DWORD FVF=D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1;
public:
  QapDev():color(0xFFFFFFFF),VB{},IB{},VBA{},IBA{},VPos(0),IPos(0),MaxVPos(0),MaxIPos(0),Batching(false)/*,BlendMode(BT_SUB),AlphaMode(AM_NONE)*/{}
  ~QapDev(){Free();}
public:
  #ifdef _WIN32
  static constexpr DWORD FVF=D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1;
  #endif
public:
  void ReInit(){Init(MaxVPos,MaxIPos);}
  #ifdef _WIN32
  void MountDev(QapD3DDev9&Dev)
  {
    this->pDev=Dev.pDev;
    this->DynRes.Mount(this,&Dev);
  }
  #endif
  void Init(int VCount,int ICount)
  {
    #ifdef _WIN32
    if(VB)return;
    #else
    if(VB.size())return;
    #endif
    MaxVPos=VCount; MaxIPos=ICount;
    #ifdef _WIN32
    pDev->CreateVertexBuffer(VCount*sizeof(Ver),D3DUSAGE_DYNAMIC,FVF,D3DPOOL_DEFAULT,&VB,NULL);
    pDev->CreateIndexBuffer(ICount*sizeof(int),D3DUSAGE_DYNAMIC,D3DFMT_INDEX32,D3DPOOL_DEFAULT,&IB,NULL);
    #else
    VB.resize(VCount);
    IB.resize(ICount);
    #endif
    VBA=0; IBA=0; VPos=0; IPos=0; DIPs=0; Verts=0; Tris=0;
    xf.set_ident();
    txf.set_ident();
  }
  void Free(){
    VB={};IB={};VPos=0;IPos=0;Batching=false;//BlendMode=BT_SUB;AlphaMode=AM_NONE;
    #ifdef _WIN32
    if(VB){
      VB->Release();
      VB=nullptr;
      VBA=nullptr;
    }
    if(IB){
      IB->Release();
      IB=nullptr;
      IBA=nullptr;
    }
    DynRes.Free();
    #endif
  };
public:
  void push_viewport(const t_quad&quad){
    stack.push_back(viewport);
    viewport=quad;
  }
  void pop_viewport(){
    QapAssert(!stack.empty());
    viewport=stack.back();
    stack.pop_back();
  }
public:
  void BeginBatch()
  {
    Batching=true;Textured=true;
    VBA=0;IBA=0;VPos=0;IPos=0;//Test it
    #ifndef _WIN32
    if(!IB.size()||!VB.size())return;
    IBA=IB.data();
    VBA=VB.data();
    #else
    IB->Lock(0,sizeof(int)*MaxIPos,(void**)&IBA,0);
    VB->Lock(0,sizeof(Ver)*MaxVPos,(void**)&VBA,0);
    #endif
  };
  void EndBatch()
  {
    Batching=false;
    #ifndef _WIN32
    if(!IB.size()||!VB.size())return;
    #else
    IB->Unlock(); VB->Unlock();  
    #endif
    DIP();
    VBA=0;IBA=0;
  }
  void DIP()
  {
    #ifdef _WIN32
    pDev->SetFVF(FVF);
    pDev->SetStreamSource(0,VB,0,sizeof(Ver));
    pDev->SetIndices(IB);
    pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,VPos,0,IPos/3);
    #else
    #ifndef QAP_UNIX
    auto&qDev=*this;
    EM_ASM({
      if(qDev.gl&&qDev.prog)qDev.DIP_v2(qDev,$0,$1,$2,$3);
    },int(qDev.VB.data()),int(qDev.IB.data()),qDev.VPos,qDev.IPos);
    #endif
    #endif
    DIPs++;Verts+=VPos;Tris+=IPos/3;
  }
  bool IsBatching(){return Batching;}
  int GetIPos(){return IPos;}
  int GetVPos(){return VPos;}
  int GetDIPs(){return DIPs;}
  int GetVerts(){return Verts;}
  int GetTris(){return Tris;}
  const QapColor&GetColor(){return color;}
  void NextFrame(){
    DIPs=0;Verts=0;Tris=0;
    #ifdef _WIN32
    SetBlendMode(BlendMode);/*SetAlphaMode(AlphaMode);*/
    #endif
  }
  #ifdef _WIN32
  BlendType BlendMode=QapD3DDev9::BT_SUB;
  void SetBlendMode(BlendType Mode){
    DynRes.pDev->Blend(BlendMode=Mode);
  }
  #endif
public:
  void HackMode(bool Textured){this->Textured=Textured;}
  #ifdef _WIN32
  void BindTex(int Stage,QapTex*Tex){pDev->SetTexture(Stage,Tex?Tex->Tex:NULL);txf.set_ident();}
  #else
  #ifdef QAP_UNIX
  void BindTex(int Stage,QapTex*pTex){}
  #else
  void BindTex(int Stage,QapTex*pTex){bindTex(pTex?pTex->Tex:0);txf.set_ident();}
  #endif
  #endif
public:
  inline Ver&AddVertexRaw(){return VBA[VPos++];}
  inline int AddVertex(const Ver&Source)
  {
    Ver&Dest=VBA[VPos];
    Dest=Source;
    if(use_xf){auto&v=Dest.get_pos();v=xf*v;}
    if(Textured){auto&v=Dest.get_tpos();v=txf*v;}
    return VPos++;
  }
  inline void AddTris(int A,int B,int C)
  {
    IBA[IPos++]=A;
    IBA[IPos++]=B;
    IBA[IPos++]=C;
  };
public:
  inline void SetColor(const QapColor&C){color=C;}
  inline void SetTransform(b2Transform const&val){xf=val;}
  inline b2Transform GetTransform(){return xf;}
  inline void SetTextureTransform(b2Transform const&val){txf=val;}
  inline b2Transform GetTextureTransform(){return txf;}
  inline real GetZoom(){return vec2d(xf.r.col1).Mag();}
public:
  //BlendType GetBlendMode(){return BlendMode;}
  //AlphaMode GetAlphaMode(){return AlphaMode;}
  //void SetBlendMode(BlendType Mode){Blend(BlendMode=Mode);}
  //void SetAlphaMode(AlphaMode Mode){Alpha(AlphaMode=Mode);}
public:
  inline int AddVertex(float x,float y,const QapColor&c,float u,float v)
  {
    QapDev::Ver tmp;
    tmp.x=x;
    tmp.y=y;
    tmp.z=0;
    tmp.color=c;
    tmp.tu=u;
    tmp.tv=v;
    auto id=AddVertex(tmp);
    return id;
  }
  inline int AddVertex(const vec2f&pos,const QapColor&c,float u,float v)
  {
    QapDev::Ver tmp;
    tmp.get_pos()=pos;
    tmp.z=0;
    tmp.color=c;
    tmp.tu=u;
    tmp.tv=v;
    auto id=AddVertex(tmp);
    return id;
  }
  inline int AddVertex(const vec2f&pos,const QapColor&c,const vec2f&tpos)
  {
    QapDev::Ver tmp;
    tmp.get_pos()=pos;
    tmp.z=0;
    tmp.color=c;
    tmp.get_tpos()=tpos;
    auto id=AddVertex(tmp);
    return id;
  }
public:
  void DrawCircleOld(const vec2d&pos,real r,real ang,real ls,int seg)
  {
    static vector<vec2d> PA;
    PA.resize(seg);
    for(int i=0;i<seg;i++)
    {
      vec2d v=Vec2dEx((real)i/(real)seg*2.0*Pi,r);
      PA[i]=pos+v;
    };
    DrawPolyLine(PA,ls,true);
  }
  void DrawCircle(const vec2d&pos,real r,real ang,real ls,int seg)
  {
    DrawCircleEx(pos,r-ls*0.5,r+ls*0.5,seg,ang);
  }
  void DrawCircleEx(const vec2d&pos,real r0,real r1,int seg,real ang)
  {
    int n=seg;
    if(n<=0)return;
    BatchScope Scope(*this);
    static vector<int> VID;
    VID.resize(n*2);
    for(int i=0;i<n;i++)
    {
      vec2d v=Vec2dEx(ang+Pi2*(real(i)/real(n)),1);
      VID[0+i]=AddVertex(pos+v*r0,color,0,0);
      VID[n+i]=AddVertex(pos+v*r1,color,0,0);
    }
    for(int i=0;i<n;i++)
    {
      int a=VID[0+(i+0)%n];
      int b=VID[0+(i+1)%n];
      int c=VID[n+(i+0)%n];
      int d=VID[n+(i+1)%n];
      AddTris(a,b,d);
      AddTris(d,c,a);
    }
  }
public:
  void DrawQuad(float x,float y,float w, float h)
  {  
    BatchScope Scope(*this);
    {
      #define F(X,Y,U,V){AddVertex(Ver(x+(X)*w,y+(Y)*h,color,U,V));}
      vec2d O(x,y);
      int n=VPos;
      F(-0.5f,-0.5f,0.0f,1.0f);
      F(+0.5f,-0.5f,1.0f,1.0f);
      F(+0.5f,+0.5f,1.0f,0.0f);
      F(-0.5f,+0.5f,0.0f,0.0f); 
      AddTris(n+1,n+0,n+3);
      AddTris(n+3,n+2,n+1);
      #undef F
    }
  }
  void DrawQuad(float x,float y,float w, float h, float a)
  {
    BatchScope Scope(*this);
    {
      #define F(X,Y,U,V){AddVertex(Ver(O+vec2d(X*w,Y*h).UnRot(OZ),color,U,V));}
      vec2d OZ=Vec2dEx(a,1.0);
      vec2d O(x,y);
      int n=VPos;
      F(-0.5f,-0.5f,0.0f,1.0f);
      F(+0.5f,-0.5f,1.0f,1.0f);
      F(+0.5f,+0.5f,1.0f,0.0f);
      F(-0.5f,+0.5f,0.0f,0.0f); 
      AddTris(n+1,n+0,n+3);
      AddTris(n+3,n+2,n+1);
      #undef F
    }
  }
  void DrawLine(vec2d a, vec2d b, double ls)
  {
    BatchScope Scope(*this);
    {
      vec2d dir=(b-a).Norm();
      vec2d dv=vec2d(-dir.y,dir.x)*(ls*0.5);
      int n=VPos;
      #define F(P,U,V){AddVertex(Ver(P,color,U,V));}
      F(a-dv,0.0f,1.0f);
      F(a+dv,0.0f,0.0f);
      F(b+dv,1.0f,0.0f);
      F(b-dv,1.0f,1.0f);
      #undef F
      AddTris(n+0,n+1,n+2);
      AddTris(n+0,n+2,n+3);
    }
  }
  void DrawTrigon(const vec2d&A,const vec2d&B,const vec2d&C)
  {
    BatchScope Scope(*this);
    {
      AddTris(
        AddVertex(Ver(A,color)),
        AddVertex(Ver(B,color)),
        AddVertex(Ver(C,color))
      );
    }
  }
  void DrawConvex(const vector<vec2d>&Points)
  {
    BatchScope Scope(*this);
    {
      if(Points.empty())return;
      int c=Points.size();
      int n=VPos;
      for(int i=0;i<c;i++)AddVertex(Ver(Points[i],color,0.5,0.5));
      for(int i=2;i<c;i++)AddTris(n,n+i-1,n+i-0);
    }
  }
public:
  template<typename TYPE>
  void DrawPolyLine(const vector<TYPE>&PA,const real&LineSize,const bool&Loop)
  {
    if(PA.empty())return;
    BatchScope Scope(*this);
    {
      int Count=PA.size();
      int c=Loop?Count:Count-1;
      for(int i=0;i<c;i++)
      {
        TYPE const&a=PA[(i+0)%Count];
        TYPE const&b=PA[(i+1)%Count];
        TYPE n=vec2d(b-a).Ort().SetMag(LineSize);
        int A[4]={
        #define F(pos)AddVertex(Ver(pos,color,0.5f,0.5f))
          F(a+n),
          F(b-n),
          F(a-n),
          F(b+n),
        #undef F
        };
        AddTris(A[0],A[1],A[2]);
        AddTris(A[0],A[1],A[3]);
      }
    }
  }
  template<typename TYPE>
  void DrawLineList(const vector<TYPE>&PA,const real&LineSize)
  {
    if(PA.empty())return;
    BatchScope Scope(*this);
    {
      int Count=PA.size();
      for(int i=0;i<Count;i+=2)
      {
        TYPE const&a=PA[i+0];
        TYPE const&b=PA[i+1];
        TYPE n=vec2d(b-a).Ort().SetMag(LineSize);
        int A[4]={
        #define F(A)AddVertex(Ver(A,color,0.5,0.5))
          F(a+n),
          F(b-n),
          F(a-n),
          F(b+n),
        #undef F
        };
        AddTris(A[0],A[1],A[2]);
        AddTris(A[0],A[1],A[3]);
      }
    }
  }
  /*template<typename TYPE>
  void DrawMesh(const vector<TYPE>&VA,const vector<int>&IA)
  {
    if(VA.empty())return;
    BatchScope Scope(*this);
    {
      int base=GetVPos();
      static vector<int> VID;VID.resize(VA.size());
      for(int i=0;i<VA.size();i++)VID[i]=AddVertex(MakeVer(VA[i],color,p.x,p.y));
      for(int i=0;i<IA.size();i+=3)AddTris(VID[IA[i+0]],VID[IA[i+1]],VID[IA[i+2]]);
    }
  }*/
  struct t_geom{
    #define DEF_PRO_TEMPLATE_DATAIO()
    #define DEF_PRO_CLASSNAME()t_geom
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(vector<vec2d>,VA,{})\
    ADD(vector<int>,IA,{})\
    //===
    #include "defprovar.inl"
    //===
    void AddTris(int a,int b,int c)
    {
      qap_add_back(IA)=a;
      qap_add_back(IA)=b;
      qap_add_back(IA)=c;
    }
    void AddVertex(real x,real y){VA.push_back(vec2d(x,y));}
    void AddVertex(const vec2d&pos){VA.push_back(pos);}
    int AddVertexAndRetVID(const vec2d&pos){VA.push_back(pos);return VA.size()-1;}
    void add(const t_geom&geom)
    {
      QapAssert(0==geom.IA.size()%3);
      auto vpos=VA.size();
      for(int i=0;i<geom.VA.size();i++)VA.push_back(geom.VA[i]);
      for(int i=0;i<geom.IA.size();i++)IA.push_back(vpos+geom.IA[i]);
    }
    void add_with_offset(const t_geom&geom,const vec2d&pos)
    {
      auto vpos=VA.size();
      for(int i=0;i<geom.VA.size();i++)
      {
        auto&ex=geom.VA[i];
        AddVertex(pos+ex);
      }
      QapAssert(0==geom.IA.size()%3);
      auto&IA=geom.IA;
      for(int i=0;i<IA.size();i+=3)
      {
        auto&a=IA[i+0];
        auto&b=IA[i+1];
        auto&c=IA[i+2];
        AddTris(vpos+a,vpos+b,vpos+c);
      }
    }
  };
  static t_geom GenGeomQuad(const vec2d&pos,float w,float h){return GenGeomQuad(pos.x,pos.y,w,h);}
  static t_geom GenGeomQuad(float x,float y,float w,float h)
  {
    t_geom geom;
    #define F(X,Y,U,V){geom.AddVertex(x+(X)*w,y+(Y)*h);}
    vec2d O(x,y);
    F(-0.5f,-0.5f,0.0f,1.0f);
    F(+0.5f,-0.5f,1.0f,1.0f);
    F(+0.5f,+0.5f,1.0f,0.0f);
    F(-0.5f,+0.5f,0.0f,0.0f); 
    geom.AddTris(1,0,3);
    geom.AddTris(3,2,1);
    #undef F
    return geom;
  }
  static t_geom GenGeomCircleSolid(real r,int seg,real ang)
  {
    t_geom geom;
    int n=seg;
    if(n<=0)return t_geom();
    for(int i=0;i<n;i++)
    {
      vec2d v=Vec2dEx(ang+Pi2*(real(i)/real(n)),1);
      qap_add_back(geom.VA)=v*r;
    }
    for(int i=2;i<n;i++)
    {
      geom.AddTris(0,i-1,i);
    }
    return geom;
  }
  static t_geom GenGeomCircleEx(real r0,real r1,int seg,real ang)
  {
    t_geom geom;
    int n=seg;
    if(n<=0)return t_geom();
    geom.VA.resize(n*2);
    for(int i=0;i<n;i++)
    {
      vec2d v=Vec2dEx(ang+Pi2*(real(i)/real(n)),1);
      geom.VA[i+0]=v*r0;
      geom.VA[i+n]=v*r1;
    }
    for(int i=0;i<n;i++)
    {
      int a=0+(i+0)%n;
      int b=0+(i+1)%n;
      int c=n+(i+0)%n;
      int d=n+(i+1)%n;
      geom.AddTris(a,b,d);
      geom.AddTris(d,c,a);
    }
    return geom;
  }
  template<typename TYPE>
  static t_geom GenGeomLineList(const vector<TYPE>&PA,const real&LineSize)
  {
    t_geom geom;
    if(PA.empty())return geom;
    int Count=PA.size();
    for (int i=0;i+1<Count;i+=2)
    {
      auto&a=PA[i+0];
      auto&b=PA[i+1];
      TYPE n=vec2d(b-a).Ort().SetMag(LineSize*0.5);
      int A=geom.AddVertexAndRetVID(a+n);
      int B=geom.AddVertexAndRetVID(b-n);
      int C=geom.AddVertexAndRetVID(a-n);
      int D=geom.AddVertexAndRetVID(b+n);
      geom.AddTris(A,B,C);
      geom.AddTris(A,B,D);
    }
    return geom;
  }
  void DrawGeomWithOffset(t_geom&geom,const vec2d&pos)
  {
    auto&qDev=*this;
    QapDev::BatchScope Scope(qDev);
    auto vpos=qDev.VPos;
    for(int i=0;i<geom.VA.size();i++)
    {
      auto&ex=geom.VA[i];
      //auto&v=qDev.AddVertexRaw();
      //v.get_pos()=pos+ex;
      //v.color=qDev.color;
      qDev.AddVertex(pos+ex,qDev.color,0,0);
    }
    QapAssert(0==geom.IA.size()%3);
    auto&IA=geom.IA;
    for(int i=0;i<IA.size();i+=3)
    {
      auto&a=IA[i+0];
      auto&b=IA[i+1];
      auto&c=IA[i+2];
      qDev.AddTris(vpos+a,vpos+b,vpos+c);
    }
  }
};
#ifdef _WIN32
class IResource
{
public:
  virtual void OnLost()=0;
  virtual void OnReset()=0;
  static IResource* Reg(IResource *A);
  static IResource* UnReg(IResource *A);
};
class QapBitmapInfo{
public:
  BITMAPINFO BI;
  BITMAPINFOHEADER&BH;
public:
  QapBitmapInfo(int W,int H):BH(BI.bmiHeader){
    ZeroMemory(&BI,sizeof(BI));
    BH.biSize=sizeof(BI.bmiHeader);
    BH.biWidth=W;BH.biHeight=H;
    BH.biPlanes=1;BH.biBitCount=32;
    BH.biSizeImage=W*H*4;
  }
};
#else
#endif
struct QapFont
{
  QapTex*Tex;
  int W[256],H[256],Size;
  QapFont():Tex(NULL),Size(0){};
  QapFont(QapTex*Tex,int Size):Tex(Tex),Size(Size){};
  ~QapFont(){
    if(Tex)delete Tex;
  };
  void Transmit(QapFont&ref)
  {
    if(Tex)delete Tex;
    Tex=ref.Tex;ref.Tex=NULL;
    for(int i=0;i<256;i++)W[i]=ref.W[i];
    for(int i=0;i<256;i++)H[i]=ref.H[i];
    Size=ref.Size;
  }
  QapFont(QapFont&ref):Tex(NULL),Size(0){Transmit(ref);}
  QapFont&operator=(QapFont&ref){Transmit(ref);return *this;}
  #ifdef _WIN32
  QapTexMem*CreateFontMem(string Name,int Size,bool Bold,int TexSize,HWND Sys_hWnd)
  {
    QapColor*pix=new QapColor[TexSize*TexSize];
    this->Size=TexSize;
    {
      HDC DC=GetDC(Sys_hWnd);
      int W=Bold?FW_BOLD:FW_NORMAL;
      int H=-MulDiv(Size,GetDeviceCaps(DC,LOGPIXELSY),72);
      {
        HFONT FNT=CreateFontA(H,0,0,0,W,0,0,0,RUSSIAN_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,Name.c_str());
        {
          HDC MDC=CreateCompatibleDC(DC);
          {
            HBITMAP BMP=CreateCompatibleBitmap(DC,TexSize,TexSize);
            SelectObject(MDC,BMP);
            RECT Rect; SetRect(&Rect,0,0,TexSize,TexSize);
            FillRect(MDC,&Rect,(HBRUSH)GetStockObject(BLACK_BRUSH));
            SelectObject(MDC,FNT);
            SetBkMode(MDC,TRANSPARENT);
            SetTextColor(MDC,0xFFFFFF);
            for(int i=0;i<256;i++)TextOutA(MDC,i%16*(TexSize/16),i/16*(TexSize/16),(LPCSTR)&i,1);
            {QapBitmapInfo QBI(TexSize,TexSize);GetDIBits(MDC,BMP,0,TexSize,pix,&QBI.BI,DIB_RGB_COLORS);}
            for(int i=0;i<TexSize*TexSize;i++){pix[i].a=pix[i].r;pix[i].r=255;pix[i].g=255;pix[i].b=255;}
            for(int i=0;i<256;i++){SIZE cs;GetTextExtentPoint32A(MDC,(LPCSTR)&i,1,&cs);this->W[i]=cs.cx;this->H[i]=cs.cy;}
            DeleteObject(BMP);
          }
          DeleteDC(MDC);
        }
        DeleteObject(FNT); 
      }
      ReleaseDC(Sys_hWnd,DC);
    }
    //for(int i=0;i<TexSize*TexSize;i++){
    //  QapAssert(pix[i]==QapColor(0x00ffffff));
    //}
    QapTexMem*pMem=new QapTexMem("Font_"+Name+"_"+to_string(TexSize),TexSize,TexSize,(QapColor*)pix);
    return pMem;
  }
  #else
  #ifndef QAP_UNIX
  QapTexMem*CreateFontMem(string Name,int Size,bool Bold,int TexSize,...)
  {
    QapColor*pix=new QapColor[TexSize*TexSize];
    this->Size=TexSize;
    emscripten_run_script(string("g_font=initFont("+to_string(Size)+",'"+Name+"',"+to_string(TexSize)+","+to_string(int(Bold))+");").c_str());
    EM_ASM({
      initFont_v2(g_font,$0,$1,$2);
    },int(pix),int(&W[0]),int(&H[0]));
    QapTexMem*pMem=new QapTexMem("Font_"+Name+"_"+to_string(TexSize),TexSize,TexSize,(QapColor*)pix);
    pMem->InvertY();
    return pMem;
  }
  #else
  QapTexMem*CreateFontMem(string Name,int Size,bool Bold,int TexSize,...){return {};}
  #endif
  #endif
};
#ifdef _WIN32
QapTex*GenTextureMipMap(QapDev&qDev,QapTexMem*&Tex,int MaxLevelCount=16)//only D3DFMT_A8R8G8B8
{
  if(!Tex)return NULL;
  int &W=Tex->W;int &H=Tex->H;QapColor *&pBits=Tex->pBits;
  if(0){fail:QapDebugMsg("NPOT texture \""+Tex->Name+"\"("+IToS(W)+"x"+IToS(H)+")");return NULL;}
  int SW=1,SWC=0;for(int &i=SWC;SW<W;i++)SW*=2; if(SW>W)goto fail;
  int SH=1,SHC=0;for(int &i=SHC;SH<H;i++)SH*=2; if(SH>H)goto fail;
  int Levels=min(MaxLevelCount,min(SWC,SHC));
  IDirect3DTexture9 *tex;
  qDev.pDev->CreateTexture(SW,SH,Levels,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex,NULL);
  D3DLOCKED_RECT rect[16]; struct QapARGB{uchar B,G,R,A;}; QapARGB*pBitsEx[16];
  for(int i=0;i<Levels;i++){tex->LockRect(i,&rect[i],NULL,0);pBitsEx[i]=(QapARGB*)rect[i].pBits;};
  {
    QapColor*pDestBits=(QapColor*)rect[0].pBits;
    memcpy_s(pDestBits,SW*SH*sizeof(QapColor),pBits,SW*SH*sizeof(QapColor));
    SW/=2; SH/=2;
  }
  for(int k=1;k<Levels;k++)
  {
    QapARGB *PC=(QapARGB*)rect[k].pBits;
    for(int j=0;j<SH;j++)
      for(int i=0;i<SW;i++)
      {
        #define F(X,Y)pBitsEx[k-1][X+2*Y*SW+2*i+4*j*SW]
          QapARGB A[4]={F(0,0),F(0,1),F(1,0),F(1,1)};
        #undef F
        float AF[4]={0,0,0,0};
        for(int t=0;t<4;t++)
        {
          AF[0]+=A[t].R;
          AF[1]+=A[t].G;
          AF[2]+=A[t].B;
          AF[3]+=A[t].A;
        };
        for(int t=0;t<4;t++)AF[t]*=0.25*(1.0/255.0);
        QapColor PCC=D3DCOLOR_COLORVALUE(AF[0],AF[1],AF[2],AF[3]);
        *PC=*((QapARGB*)&PCC);
        PC++;
      }
    SW/=2; SH/=2;
  }
  for(int i=0;i<Levels;i++)tex->UnlockRect(i);
  //MACRO_ADD_LOG("Loaded \""+Tex->Name+"\"",lml_HINT);
  QapTex*pTex=new QapTex(Tex,tex);
  delete Tex;Tex=NULL;
  return pTex;
}
#else
#ifdef QAP_UNIX
QapTex*GenTextureMipMap(QapDev&qDev,QapTexMem*&pMem,int MaxLevelCount=16){return {};}
#else
QapTex*GenTextureMipMap(QapDev&qDev,QapTexMem*&pMem,int MaxLevelCount=16){
  int&W=pMem->W;int&H=pMem->H;QapColor*&pBits=pMem->pBits;
  auto tex=EM_ASM_INT({
    return makeTexture(qDev,$0,$1,$2);
  },W,H,int(pBits));
  auto*pTex=new QapTex(pMem,tex);
  delete pMem;pMem=NULL;
  return pTex;
}
#endif
#endif
QapTexMem*BlurTexture(QapTexMem*Tex,int PassCount)//only D3DFMT_A8R8G8B8
{
  int &W=Tex->W;int &H=Tex->H;QapColor *&pBits=Tex->pBits;
  #define BlurLog(X,Y)//MACRO_ADD_LOG(X,Y)
  BlurLog("Blur \""+Tex->Name+"\" x"+IToS(PassCount),lml_HINT);
  struct QapARGB{uchar B,G,R,A;};
  static QapARGB VoidMem[2048*2048*4];
  #define qap_memcpy_s(DEST,DSIZE,SRC,SIZE)memcpy(DEST,SRC,SIZE)
  qap_memcpy_s(VoidMem,sizeof(VoidMem),pBits,W*H*sizeof(QapARGB));
  static int BBM[9]={
    1,2,1, 
    2,4,2, 
    1,2,1};
  #define INIT_STATIC_VAR(TYPE,VALUE,INIT_CODE)static TYPE VALUE;{static bool _STATIC_SYS_FLAG=true;if(_STATIC_SYS_FLAG){INIT_CODE;_STATIC_SYS_FLAG=false;};};
    INIT_STATIC_VAR(int,MartixSum,for(int i=0;i<9;i++)MartixSum+=BBM[i];);
  #undef INIT_STATIC_VAR
  int PosRange[9]={
    -W-1,-W,-W+1,
    -1,0,+1,
    +W-1,+W,+W+1};
  float inv_255=1.0/255;
  for(int PassId=0;PassId<PassCount;PassId++)
  {
    QapARGB *PC=0;
    QapARGB *VM=0;
    for(int j=1;j<H-1;j++)
      for(int i=1;i<W-1;i++)
      {
        PC=(QapARGB*)pBits+j*W+i; VM=(QapARGB*)VoidMem+j*W+i;
        float AF[4]={0,0,0,0};
        for(int t=0;t<9;t++)
        {
          QapARGB T=*(VM+PosRange[t]);
          AF[0]+=T.R*BBM[t];
          AF[1]+=T.G*BBM[t];
          AF[2]+=T.B*BBM[t];
          AF[3]+=T.A*BBM[t];
        };    
        for(int i=0;i<4;i++)AF[i]/=MartixSum*255.0;
        #define F(r,g,b,a)QapColor((DWORD)((r)*255.f),(DWORD)((g)*255.f),(DWORD)((b)*255.f),(DWORD)((a)*255.f))
        QapColor PCC=F(AF[3],AF[0],AF[1],AF[2]);
        #undef F
        *PC=*((QapARGB*)&PCC);
      }
    //PassId++;
    qap_memcpy_s(VoidMem,sizeof(VoidMem),pBits,W*H*sizeof(QapARGB));
    #undef qap_memcpy_s
  }
  BlurLog("Blur \""+Tex->Name+"\" x"+IToS(PassCount),lml_HINT);
  #undef BlurLog
  return Tex;
}
void DrawQapText(QapDev&qDev,QapFont&Font,float X,float Y,const string&Text)
{
  static QapColor CT[]={
    0xFF252525,0xFFFF0000,0xFF00FF00,0xFFFFFF00,
    0xFF0000FF,0xFFFF00FF,0xFF00FFFF,0xFFFFFFFF,
    0xFFFFFFA8,0xFFFFA8FF,
    0xFFFF8000,0xFF0080FF,0xFFA0A0A0,0xFF808080,0xFFF0F000,0xFF00F0F0,
  };
  bool _4it=!qDev.IsBatching();
  if(_4it)qDev.BeginBatch();
  int QuadCount=0;
  int VPos=qDev.GetVPos();
  {
    float xp=0; int i=0;
    while(i<(int)Text.length())
    {
      if(Text[i]!='^')
      {
        int I=(uchar)Text[i];
        float s=((float)(I%16))/16,t=((float)(I/16))/16;
        float cx=(float)Font.W[I],cy=(float)Font.H[I],ts=(float)Font.Size;
        #define F(var,x,y,z,color,u,v)int var=qDev.AddVertex(QapDev::Ver(X+x,Y+y,color,u,v));
          F(A,xp+0,-cy,0,qDev.GetColor(),s,1-t-cy/ts);
          F(B,xp+cx,-cy,0,qDev.GetColor(),s+cx/ts,1-t-cy/ts);
          F(C,xp+cx,0,0,qDev.GetColor(),s+cx/ts,1-t);
          F(D,xp+0,0,0,qDev.GetColor(),s,1-t);
        #undef F
        qDev.AddTris(A,B,C);
        qDev.AddTris(C,D,A);
        xp+=cx; QuadCount++; i++; continue;
      };
      i++; if(i>(int)Text.length())continue;
      if((Text[i]>='0')&&(Text[i]<='9')){qDev.SetColor(CT[Text[i]-'0']); i++; continue;};
      if((Text[i]>='A')&&(Text[i]<='F')){qDev.SetColor(CT[Text[i]-'A'+10]); i++; continue;};
    }
  };
  if(_4it)qDev.EndBatch();
}
string Q3TextToNormal(const string&Text)
{
  string s; int i=0;
  while(i<(int)Text.length())
  {
    if(Text[i]!='^'){s.push_back(Text[i++]);continue;}
    i++;if(i>(int)Text.length())continue;
    if((Text[i]>='0')&&(Text[i]<='9')){i++;continue;};
    if((Text[i]>='A')&&(Text[i]<='F')){i++;continue;};
  }
  return s;
}
int GetQ3TextLength(const QapFont &Font,const string &Text)
{
  float xp=0; int i=0;
  while(i<(int)Text.size())
  {
    if(Text[i]!='^')
    {
      int I=(uchar)Text[i];
      float cx=(float)Font.W[I];
      xp+=cx; i++; continue;
    };
    i++; if(i>(int)Text.size())continue;
    if((Text[i]>='0')&&(Text[i]<='9')){i++; continue;};
    if((Text[i]>='A')&&(Text[i]<='F')){i++; continue;};
  }
  return xp;
}
class TextRender{
public:
  QapDev*RD;
  bool dummy=false;
  struct TextLine{
  public:
    string text;
    int x,y;
  public:
    TextLine(int x,int y,const string&text):x(x),y(y),text(text){}
  public:
    void DrawRaw(QapDev&qDev,QapFont*Font,int dv){DrawQapText(qDev,*Font,x+dv+0.5,y-dv+0.5,Q3TextToNormal(text));}
    void DrawSys(QapDev&qDev,QapFont*Font,int dv){DrawQapText(qDev,*Font,x+dv+0.5,y-dv+0.5,text);}
  };
  vector<TextLine> LV;
  TextRender(QapDev*RD):RD(RD){}
public:
  int x,y,ident,bx;
  QapFont*NormFont;
  QapFont*BlurFont;
public:
  void BeginScope(int X,int Y,QapFont*NormFont,QapFont*BlurFont){
    bx=X;x=X;y=Y;ident=24;this->NormFont=NormFont;this->BlurFont=BlurFont;
  }
  void BR(){y-=ident;x=bx;}
  void AddText(const string&text)
  {
    LV.push_back(TextLine(x,y,text));BR();
  }
  int text_len(const string&text){return GetQ3TextLength(*NormFont,text);}
  void AddTextNext(const string&text)
  {
    LV.push_back(TextLine(x,y,text));x+=GetQ3TextLength(*NormFont,text);
  }
  void EndScope(){
    if(dummy)return;
    {
      RD->BindTex(0,BlurFont->Tex);
      RD->SetColor(0xff000000);
      RD->BeginBatch();
      for(int i=0;i<LV.size();i++)LV[i].DrawRaw(*RD,BlurFont,1.0);
      RD->EndBatch();
    }
    {
      RD->BindTex(0,NormFont->Tex);
      RD->SetColor(0xffffffff);
      RD->BeginBatch();
      for(int i=0;i<LV.size();i++)LV[i].DrawSys(*RD,NormFont,0.0);
      RD->EndBatch();
    }
  }
};
#ifdef _WIN32
#else
QapDev qDev;
#endif
class QapAtlas{
public:
  int W,H;
  int X,Y;
  int Ident,dY;
  QapTexMem*pMem{};
  QapTex*pTex{};
  struct TFrame{
    QapAtlas*atlas;
    int x,y,w,h;
    TFrame():atlas(NULL),x(0),y(0),w(0),h(0){}
    TFrame(QapAtlas*atlas,QapTexMem*Mem,int x,int y):atlas(atlas),x(x),y(y),w(Mem->W),h(Mem->H){}
    void Bind(QapDev&qDev){atlas->Bind(qDev,this);}
  };
  QapPool<TFrame>pool;
  vector<TFrame*>frames;
public:
  QapAtlas():pMem(NULL),pTex(NULL),W(1024*2),H(1024*2),X(0),Y(0),Ident(8),dY(0),pool(256){
    pMem=new QapTexMem("Atlas.qap",W,H,new QapColor[W*H]);
  }
  TFrame*AddFrame(QapTexMem*Mem)
  {
    if(!Mem||!Mem->pBits)return NULL;
    if(X<Ident||Ident+X+Mem->W>W)
    {
      QapAssert(W>=Mem->W);
      X=Ident;Y+=dY+Ident;dY=Mem->H;
      pMem->FillLine(Y-Ident/2,0xff000000);
    }
    {
      TFrame*pFrame;
      pool.NewInstance(pFrame);
      *pFrame=TFrame(this,Mem,X,Y);
      frames.push_back(pFrame);
      pMem->FillBorder(X,Y,Mem);
      pMem->FillMem(X,Y,Mem);
      X+=Mem->W+Ident;dY=max(dY,Mem->H);
      //QAP_EM_LOG("AddFrame done");
      return pFrame;
    }
  }
  QapTex*AddTex(QapDev&qDev,QapTexMem*Mem)
  {
    AddFrame(Mem);
    return GenTextureMipMap(qDev,Mem);
  }
  QapTex*GenTex(QapDev&qDev){return pTex=GenTextureMipMap(qDev,pMem);}
  void Bind(QapDev&qDev,TFrame*frame)
  {
    b2Transform xf;
    float inv_w=1.f/float(W);
    float inv_h=1.f/float(H);
    xf.p=vec2f(float(frame->x)*inv_w,float(frame->y)*inv_h);
    xf.r=MakeZoomTransform(vec2d(float(frame->w)*inv_w,float(frame->h)*inv_h)).r;
    qDev.SetTextureTransform(xf);
  }
};
QapKeyboard kb;
struct t_global_img{
  string fn;
  std::function<void(const string&,int,int,int)> on_load;
  bool done=false;
};
struct t_global_url{
  string url;
  std::function<void(const string&,int,int)> on_load;
  bool done=false;
};
map<string,t_global_img> g_global_imgs;
map<string,t_global_url> g_global_urls;
extern "C" {
  int qap_on_load_img(char*pfn,int ptr,int w,int h){
    string fn=pfn;
    QAP_EM_LOG("on_load:"+fn);
    auto it=g_global_imgs.find(fn);
    if(it==g_global_imgs.end())return 0;
    QAP_EM_LOG("on_load_bef:"+fn);
    it->second.on_load(fn,ptr,w,h);
    QAP_EM_LOG("on_load_aft:"+fn);
    it->second.done=true;
    return 0;
  }
  int qap_on_load_url(char*purl,int ptr,int size){
    string url=purl;
    QAP_EM_LOG("on_load_url:"+url);
    auto it=g_global_urls.find(url);
    if(it==g_global_urls.end())return 0;
    QAP_EM_LOG("on_load_url_bef:"+url);
    it->second.on_load(url,ptr,size);
    QAP_EM_LOG("on_load_url_aft:"+url);
    it->second.done=true;
    return 0;
  }
}
#if(defined(_WIN32)||defined(QAP_UNIX))
#include "thirdparty/lodepng.cpp"
struct ImgLoader{
  struct string
  {
    const char*ptr;
    int size;
  };
  struct TextureMemory
  {
    void*ptr;
    int W;
    int H;
    TextureMemory(){ptr=0;W=0;H=0;}
  };
  struct IEnv
  {
    virtual void InitMemory(TextureMemory&out,int W,int H)=0;
    virtual void FreeMemory(TextureMemory&in)=0;
  };
  virtual void LoadTextureFromFile(IEnv&Env,TextureMemory&Mem,string FileName)=0;
  virtual void SaveTextureToFile(IEnv&Env,TextureMemory&Mem,string FileName)=0;
  virtual bool KeepExt(string ext)=0;
};
struct TLoaderEnv:ImgLoader::IEnv
{
  static ImgLoader::string SToS(const std::string&in)
  {
    ImgLoader::string tmp;
    if(in.empty()){tmp.ptr=0;tmp.size=0;return tmp;}
    tmp.ptr=&in[0];
    tmp.size=in.size();
    return tmp;
  }
  typedef ImgLoader::TextureMemory TextureMemory;
  //using ImgLoader::TextureMemory;
  void InitMemory(TextureMemory&out,int W,int H)
  {
    if(W*H<=0)return;
    assert(!out.ptr);
    assert(!out.W);
    assert(!out.H);
    out.ptr=new int[W*H];
    out.W=W;
    out.H=H;
    int*p=(int*)out.ptr;
    for(int i=0;i<W*H;i++)p[i]=0;
  }
  void FreeMemory(TextureMemory&in)
  {
    int*ptr=(int*)in.ptr;
    delete[] ptr;
    in.ptr=0;
    in.W=0;
    in.H=0;
  }
  struct bgra{unsigned char b,g,r,a;};
  bool LoadTextureFromFile_v2(TextureMemory&mem,const std::string&fn)
  {
    return load_tex(mem,fn.c_str());
  }
  bool load_tex(TextureMemory&mem,const char*fn)
  {
    std::vector<unsigned char> image;
    unsigned cx=0,cy=0;
    decodeOneStep(fn,image,cx,cy);
    if(!cx||!cy)return false;
    InitMemory(mem,cx,cy);auto*arr=(bgra*)mem.ptr;
    for(unsigned y=0;y<cy;y++)
    for(unsigned x=0;x<cx;x++)
    {
      auto&out=arr[x+cx*y];
      auto&inp=*(bgra*)(void*)&image[4*cx*y+4*x+0];
      out=inp;
      std::swap(out.r,out.b);
    }
    return true;
  }
  static void decodeOneStep(const char*fn,std::vector<unsigned char>&image,unsigned&width,unsigned&height)
  {
    unsigned error=lodepng::decode(image,width,height,fn);
    //if(error)QapDebugMsg("encoder error "+std::to_string(error)+": "+lodepng_error_text(error));
  }
  void SaveTextureToFile_v2(TextureMemory&mem,const string&fn){
    assert(mem.ptr);
    save_tex(mem.W,mem.H,(bgra*)mem.ptr,fn.c_str());
  }
  //void save_tex(int cx,int cy,const vector<QapColor>&arr,const char*fn)
  void save_tex(int cx,int cy,bgra*arr,const char*fn)
  {
    std::vector<unsigned char> image;
    image.resize(cx*cy*4);
    for(unsigned y=0;y<cy;y++)
    for(unsigned x=0;x<cx;x++)
    {
      auto c=arr[x+cx*y];
      auto&out=*(bgra*)(void*)&image[4*cx*y+4*x+0];
      out=c;
      std::swap(out.r,out.b);
    }
    encodeOneStep(fn,image,cx,cy);
  }
  static void encodeOneStep(const char*fn,std::vector<unsigned char>&image,unsigned width,unsigned height)
  {
    unsigned error=lodepng::encode(fn,image,width,height);
    //if(error)QapDebugMsg("encoder error "+std::to_string(error)+": "+lodepng_error_text(error));
  }
};
template<class FUNC>
QapTexMem*LoadTexture(const string&FN,FUNC&&func)
{
  static TLoaderEnv env;
  TLoaderEnv::TextureMemory mem;
  env.LoadTextureFromFile_v2(mem,FN);
  if(!mem.ptr){QapDebugMsg("Error Loading \""+FN+"\"");return NULL;}
  QapTexMem*pTex=new QapTexMem(FN);
  pTex->W=mem.W;
  pTex->H=mem.H;
  pTex->pBits=(QapColor*)mem.ptr;
  //pTex->Clear(QapColor(255,0,0,0));
  //pTex->Clear(0xffff00ff);
  func(FN,int(pTex->pBits),mem.W,mem.H);
  return pTex;
}
#else
template<class FUNC>
QapTexMem*LoadTexture(string fn,FUNC&&func){
  auto&m=g_global_imgs[fn];
  m.fn=fn;
  m.on_load=std::move(func);
  EM_ASM({
    loadTexture_v2(UTF8ToString($0));
  },int(fn.c_str()));
  return nullptr;
}
#endif
struct i_world{
  virtual ~i_world(){};
  virtual void use(int player,const string&cmd,string&outmsg)=0;
  virtual void step()=0;
  virtual bool finished()=0;
  virtual void get_score(vector<double>&out)=0;
  virtual void is_alive(vector<int>&out)=0;
  virtual void get_vpow(int player,string&out)=0;
  virtual void init(uint32_t seed,uint32_t num_players)=0;
  virtual int init_from_config(const string&cfg,string&outmsg)=0;
  virtual unique_ptr<i_world> clone()=0;
  virtual void renderV0(QapDev&qDev){}
  virtual int get_render_api_version(){return 0;}
  virtual int get_tick()=0;
};
unique_ptr<i_world> TGame_mk_world(const string&world);
struct GameSession {
    unique_ptr<i_world> world;
    vector<unique_ptr<i_world>> ws;
    vector<unique_ptr<i_world>> ws2;
    struct TickState {
        int tick = 0;
        vector<bool> received;          // получена ли команда от игрока i
        vector<string> commands;        // команды от игроков
        vector<string> error_msgs;      // outmsg от use() — для лога
        vector<bool> alive;             // кто жив на начало тика
    };
    TickState current_tick;
    vector<TickState> history;
    //vector<vector<string>> replay;
    QapClock clock;
    mutable mutex mtx;
    vector<bool> connected;
    atomic_bool end=false;
    struct i_connection{
      enum t_net_state{nsDef=0,nsOff=1,nsErr=-1};
      int network_state=nsDef;
      virtual void send(const string&msg)=0;
      virtual void send_seed(const string&msg){send(msg);}
      virtual void send_vpow(const string&msg){send(msg);}
      virtual void send_tick(int tick,double ms){}
      virtual void err(const string&msg)=0;
      virtual void off()=0;
    };
    i_connection*pcon=nullptr;
    vector<i_connection*> carr;
    void set_connected(int player_id, bool state) {
        if(end)return;
        lock_guard<mutex> lock(mtx);
        if (player_id >= 0 && player_id < (int)connected.size()) {
            connected[player_id] = state;
        }
    }
    void save(){
      if(!g_args.replay_out_file||!g_args.replay_out_file->size())return;
      vector<vector<string>> tick2slot2cmd;
      for(auto&ex:history)tick2slot2cmd.push_back(ex.commands);
      auto s=QapSaveToStr(tick2slot2cmd);
      file_put_contents(*g_args.replay_out_file,s);
    }
    void init(){
      world=TGame_mk_world(g_args.world_name);
      world->init(g_args.seed_initial,g_args.num_players);
      carr.resize(g_args.num_players,nullptr);
      connected.assign(g_args.num_players,false);
      start_new_tick();
      if(g_args.gui_mode||g_args.replay_in_file)ws.push_back(world->clone());
    }
    void submit_command(int player_id, const string& cmd) {
      if(end)return;
      lock_guard<mutex> lock(mtx);
      if(!qap_check_id(current_tick.received,player_id))return;
      if(!current_tick.alive[player_id])return;
      if(current_tick.received[player_id]){
        auto msg="\nWarning: another cmd at the same turn!\n";
        current_tick.error_msgs[player_id]+=msg;
        if(carr[player_id])carr[player_id]->err(msg);
        return;
      }
      current_tick.commands[player_id] = cmd;
      current_tick.received[player_id] = true;
    }
    void update(){
      if(end)return;
      if(!try_step())return;
      if(g_args.gui_mode)ws.push_back(world->clone());
      if(bool done=is_finished()){
        {
          lock_guard<mutex> lock(mtx);
          for(auto&ex:carr)if(ex)ex->off();
        }
        string report=generate_report();
        cerr<<report;
        cerr<<"time:"<<clock.MS()<<endl;
        save();
        end=true;
      }else{
        send_vpow_to_all();
      }
    }

    void start_new_tick() {
        if(end)return;
        lock_guard<mutex> lock(mtx);
        if (!history.empty()) {
            current_tick.tick = history.back().tick + 1;
        } else {
            current_tick.tick = 0;
        }
        vector<int> alive_int;
        world->is_alive(alive_int);
        current_tick.alive.assign(alive_int.begin(), alive_int.end());
        int n = (int)alive_int.size();
        current_tick.received.assign(n, false);
        current_tick.commands.assign(n, "");
        current_tick.error_msgs.assign(n, "");
    }

    bool try_step() {
        if(end)return false;
        if(bool inside_lock=true)
        {
          lock_guard<mutex> lock(mtx);
          bool all_received = true;bool all_failed=true;
          for (int i = 0; i < (int)current_tick.alive.size(); ++i) {
              if(carr[i]&&carr[i]->network_state==carr[i]->nsDef)all_failed=false;
              if((current_tick.alive[i]&&connected[i]&&!current_tick.received[i])||!carr[i]){
                  all_received = false;
                  break;
              }
          }
          if (!all_received||all_failed) return false;
          for (int i = 0; i < (int)current_tick.commands.size(); ++i) {
              if(!current_tick.alive[i]||!connected[i])continue;
              string err;
              world->use(i,current_tick.commands[i],err);
              if(carr[i]&&err.size())carr[i]->err(err);
              current_tick.error_msgs[i]+=err;
          }
          history.push_back(current_tick);
          world->step();
        }
        start_new_tick();
        return true;
    }

    bool is_finished() const {
        lock_guard<mutex> lock(mtx);
        if (world->finished()) return true;
        vector<int> world_alive;
        world->is_alive(world_alive);
        int active = 0;
        for (int i = 0; i < g_args.num_players; ++i) {
            if (connected[i] && world_alive[i]&&(carr[i]?carr[i]->network_state==carr[i]->nsDef:true)) {
                active++;
            }
        }
        return active < 2;
    }
    bool is_inited()const{
      lock_guard<mutex> lock(mtx);
      if(world->finished())return true;
      int n=0;
      for(auto&ex:carr)if(ex)n++;
      return n==g_args.num_players;
    }
    void send_seed_to_all(){
      if(end)return;
      lock_guard<mutex> lock(mtx);
      vector<int> is_alive;world->is_alive(is_alive);
      string seed(sizeof(uint32_t),0);
      *(uint32_t*)seed.data()=g_args.seed_strategies;
      for(int i=0;i<g_args.num_players;i++){
        if(is_alive[i]&&connected[i]){
          carr[i]->send_seed(seed);
        }
      }
    }
    void send_vpow_to_all(){
      if(end)return;
      auto beg=clock.MS();
      vector<string> messages;
      vector<bool> should_send;
      vector<bool> should_off;
      {
        lock_guard<mutex> lock(mtx);
        if (end) return;
        vector<int> is_alive;
        world->is_alive(is_alive);
        messages.resize(g_args.num_players);
        should_send.assign(g_args.num_players, false);
        should_off.assign(g_args.num_players, false);

        for (int i = 0; i < g_args.num_players; i++) {
          if (is_alive[i] && connected[i]) {
            string vpow;
            world->get_vpow(i, vpow);
            messages[i] = QapSaveToStr(vpow);
            should_send[i] = true;
          } else if (!is_alive[i]) {
            should_off[i] = true;
          }
        }
      }
      if(pcon)pcon->send_tick(current_tick.tick,clock.MS()-beg);
      for (int i = 0; i < g_args.num_players; i++) {
        if (should_send[i]) {
          carr[i]->send(messages[i]);
        }
        if (should_off[i]) {
          carr[i]->off();
        }
      }
      //if(end)return;
      //lock_guard<mutex> lock(mtx);
      //vector<int> is_alive;world->is_alive(is_alive);
      //string vpow;
      //for(int i=0;i<g_args.num_players;i++){
      //  if(is_alive[i]&&connected[i]){
      //    vpow.clear();
      //    world->get_vpow(i,vpow);
      //    string svpow=QapSaveToStr(vpow);
      //    if(end)return;
      //    carr[i]->send(svpow);
      //  }
      //  if(!is_alive[i]){
      //    if(end)return;
      //    carr[i]->off();
      //  }
      //}
    }
    // Генерация отчёта (вызывается после завершения)
    string generate_report() const {
        lock_guard<mutex> lock(mtx);
        vector<double> scores;world->get_score(scores);
        ostringstream oss;
        oss << "Game finished at tick " << (history.empty() ? 0 : history.back().tick) << "\n";
        oss << "SeedInit:  " << g_args.seed_initial << "\n";
        oss << "SeedStrat: " << g_args.seed_strategies << "\n";
        oss << "=== Scores ===\n";
        for(int i=0;i<scores.size();i++){
          oss << scores[i] <<"\n";
        }
        oss << "=== Error log ===\n";
        for (const auto& tick : history) {
            for (int i = 0; i < (int)tick.error_msgs.size(); ++i) {
                if (!tick.error_msgs[i].empty()) {
                    oss << "[Tick " << tick.tick << "][Player " << i << "] " << tick.error_msgs[i] << "\n";
                }
            }
        }
        return oss.str();
    }
    string gen_result(){
      vector<double> scores;world->get_score(scores);
      vector<string> out;
      for(auto&ex:scores)out.push_back(to_string(ex));
      return join(out,",");
    }
} session;
#ifdef _WIN32
#include "inetdownloader.hpp"
#endif
#include "by_ai/mapgen.cpp"
#include "../../src/core/aicup_structs.inl"
string g_host="185.92.223.117";
QapClock g_clock;
class TGame{
public:
  typedef QapAtlas::TFrame TFrame;
public:
  class ILevel{
  public:
    virtual void Render(QapDev&qDev)=0;
    virtual void Update(TGame*Game)=0;
    virtual bool Win()=0;
    virtual bool Fail()=0;
    virtual void AddText(TextRender&TR){}
    virtual ~ILevel(){}
  };
  class ILevelFactory{
  public:
    virtual ILevel*Build(TGame*Game)=0;
  };
  struct TLevelInfo{
    string Name;
    ILevelFactory&Factory;
    TLevelInfo(const string&Name,ILevelFactory&Factory):Name(Name),Factory(Factory){}
  };
  template<typename TYPE>
  class TLevelFactory:public ILevelFactory{
  public:
    virtual ILevel*Build(TGame*Game){
      auto*Result=new TYPE();
      Result->Init(Game);
      return Result;
    }
  };/*
  template<typename TYPE>
  class AutoPtr:public std::auto_ptr<TYPE>{
  public:
    TYPE*operator->(){return get();}
    operator bool(){return 
  };*/
public:
  class TCounterInc{
  public:
    int Value=0;
    int Minimum=0;
    int Maximum=32;
  public:
    TCounterInc(){;}
    TCounterInc(int Value,int Minimum,int Maximum){this->Value=Value;this->Minimum=Minimum;this->Maximum=Maximum;}
    void Start(){Value=Minimum;}
    void Stop(){Value=Maximum;}
    operator bool(){return Value<Maximum;}
    void operator++(){Value++;}
    void operator++(int){Value++;}
    int DipSize(){return Maximum-Minimum;}
    operator string(){return IToS(Value)+"/"+IToS(DipSize());}
  };  
  class TCounterIncEx{
  public:
    int Value=0;
    int Minimum=0;
    int Maximum=32;
    bool Runned=false;
  public:
    TCounterIncEx(){;}
    TCounterIncEx(int Value,int Minimum,int Maximum){;this->Value=Value;this->Minimum=Minimum;this->Maximum=Maximum;}
    void Start(){Value=Minimum;Runned=true;}
    void Stop(){Value=Minimum;Runned=false;}
    operator bool(){return Value<Maximum;}
    void operator++(){if(Runned)Value++;}
    void operator++(int){if(Runned)Value++;}
    int DipSize(){return Maximum-Minimum;}
    operator string(){return IToS(Value)+"/"+IToS(DipSize());}
  };
public:
#include "LevelPack.inl"
class IOnClick;
class TMenu;
struct MenuItem{
public:
  string Caption;
  IOnClick*OnClick;
  MenuItem(const string&Caption,IOnClick*OnClick){this->Caption=Caption;this->OnClick=OnClick;};
};
class IOnClick{
public:
  virtual void Call(TMenu*EX)=0;
  virtual bool IsEnabled(TMenu*EX){return true;}
};
class TMenu{
private:
  bool in_game;
public:
  bool InGame(){return in_game;}
  void Up(){in_game=false;}
  void Down(){if(Game->Level.get())in_game=true;}
public:
  vector<MenuItem>Items;
  string Caption;
  TGame*Game;
  std::unique_ptr<TMenu>OldMenu;
  vec2d oldmp;
  int CurID;
  TMenu(TGame*Game,const string&Caption){this->Game=Game;this->Caption=Caption;};
  void Add(const string&s,IOnClick*p){Items.push_back(MenuItem(s,p));}
  void Back(){
    QapAssert(OldMenu.get());
    Game->Menu.release();
    Game->Menu.reset(OldMenu.release());
    //create memory leak
    delete this;
  }
  void AddText(real&y,TextRender&TE,const string&s,const real dy=32){
    real x=GetQ3TextLength(*TE.NormFont,s);
    TE.LV.push_back(TextRender::TextLine(-x*0.5,y+TE.NormFont->H[0]*0.5,s));y-=dy;
  };
  void Render(QapDev&qDev,TextRender&TE)
  {
    if(!Game->FrameMenuItem||!Game->Atlas.pTex)return;
    const real dy=32;
    real y=+0.5*Items.size()*dy;
    qDev.BindTex(0,Game->Atlas.pTex);
    Game->FrameMenuItem->Bind(qDev);
    qDev.SetColor(0x80ffffff);
    qDev.DrawQuad(0,y-dy*CurID,Game->FrameMenuItem->w,Game->FrameMenuItem->h,0);
    for(int i=0;i<Items.size();i++)AddText(y,TE,string(Items[i].OnClick->IsEnabled(this)?CurID==i?"^3":"^7":"^D")+Items[i].Caption,dy);
  }
  void Update(TGame*Game)
  {
    CurID%=Items.size();
    const real dy=32;
    real y=+0.5*Items.size()*dy;
    auto mp=kb.MousePos;
    vec2d hmis=vec2d(Game->FrameMenuItem->w,dy)*0.5;
    if(mp!=oldmp)
    {
      for(int i=0;i<Items.size();i++)
      {
        if(Items[i].OnClick->IsEnabled(this))if(CD_Rect2Point(-hmis+vec2d(0,y),hmis+vec2d(0,y),mp))CurID=i;
        y-=dy;
      }
      oldmp=mp;
    }
    for(int i=CurID;i<CurID+Items.size();i++)if(Items[i%Items.size()].OnClick->IsEnabled(this)){CurID=i;break;}
    if(kb.OnDown(VK_DOWN)){
      CurID++;
      for(int i=CurID;i<CurID+Items.size();i++)if(Items[i%Items.size()].OnClick->IsEnabled(this)){CurID=i;break;}
    }
    if(kb.OnDown(VK_UP)){
      CurID--;
      for(int i=CurID+Items.size();i>CurID;i--)
        if(Items[i%Items.size()].OnClick->IsEnabled(this))
        {
          CurID=i;break;
        }
    }
    CurID+=Items.size();
    CurID%=Items.size();
    if(kb.OnDown(VK_RETURN)){
      Items[CurID].OnClick->Call(this);
    }
    if(kb.OnDown(mbLeft)){
      vec2d dY(0,0.5*Items.size()*dy-CurID*dy);
      if(Items[CurID].OnClick->IsEnabled(this))if(CD_Rect2Point(-hmis+dY,hmis+dY,mp))
      {
        Items[CurID].OnClick->Call(this);
      }
    }
  }
};
public:
  struct t_frame{
    TFrame*pF=0;
    TFrame*pS=0;
    string name,file,fn;
    int mode=0;
    operator bool()const{return pF&&pS;}
  };
#define ADDFRAME(NAME,FILE,MODE)TFrame*Frame##NAME;TFrame*Frame##NAME##_s;t_frame frame_##NAME={0,0,#NAME,FILE,"",MODE};
  FRAMESCOPE(ADDFRAME)
#undef ADDFRAME
public:
  TCounterIncEx WaitWin=TCounterIncEx(0,0,Sys.UPS*2);
  TCounterIncEx WaitFail=TCounterIncEx(0,0,Sys.UPS*2);
  TCounterInc LevelCounter=TCounterInc(0,0,0);
public:
  vector<TLevelInfo>LevelsInfo;
  std::unique_ptr<ILevel>Level;
  std::unique_ptr<TMenu>Menu;
public:
  QapAtlas Atlas;
  QapDev qDev;
  QapFont NormFont;
  QapFont BlurFont;
  QapTex*th_rt_tex{};
  QapTex*th_rt_tex_full{};
public:
  TGame(){}
public:
  QapTexMem*AddBorder(QapTexMem*pMem,int dHS=8,const QapColor&Color=0xffffffff)
  {
    int dS=dHS*2;
    QapTexMem*sm=new QapTexMem("ShadowBot",pMem->W+dS,pMem->H+dS,NULL);
    sm->pBits=new QapColor[sm->W*sm->H];
    sm->Clear(Color);
    sm->FillMem(dHS,dHS,pMem);
    //sm->FillBorder(dHS,dHS,pMem,dHS);
    return sm;
  }
  QapTexMem*GenShadow(QapTexMem*pMem,int dHS=8)
  {
    int dS=dHS*2;
    vec4f acc={0,0,0,0};
    for(int x:{0,1})for(int y:{0,1})acc+=pMem->get_color_at(x*(pMem->W-1),y*(pMem->H-1));
    acc*=0.25;
    QapColor c=acc.GetColor();
    auto*sm=AddBorder(pMem,dHS,c);
    sm->CalcAlpha(0xffffffff);
    sm->FillChannel(0x00ffffff,0x00ffffff);
    BlurTexture(sm,dHS);
    return sm;
  }
  TFrame*GenShadowFrame(QapTexMem*pMem,int dHS=8)
  {
    QapTexMem*sm=GenShadow(pMem,dHS);
    TFrame*FrameX=Atlas.AddFrame(sm);
    delete sm;
    return FrameX;
  }
  string GFX_DIR="GFX/v4/";
  void LoadFrames(bool need_save_atlas=0,bool need_rewrite_tex=0)
  {
    #ifndef QAP_UNIX
    auto m2=[&](QapTexMem*p){return p->CalcAlpha()->FillChannel(0xffffffff,0x00ffffff);};
    static int frames=0;static bool done=false;
    {
      static auto on_load_tex=[&](string fn){
        frames--;
        QAP_EM_LOG("on_load_tex:"+fn);
        if(frames||done)return;
        QAP_EM_LOG("on_load_tex_done_at:"+fn);
        done=true;
        Atlas.GenTex(qDev);
        QAP_EM_LOG("on_load_tex_done_at_aft_GenTex:"+fn);
      };
      #define F(NAME,FILE,MODE)frames++;
      FRAMESCOPE(F);
      #undef F
      #define F(NAME,FILE,MODE){\
        t_frame&f=frame_##NAME;f.fn=GFX_DIR+ FILE ".png";\
        LoadTexture(f.fn,[&](const string&fn,int ptr,int w,int h){\
          QAP_EM_LOG("on_LoadTexture:"+fn+" "+IToS(w)+" "+IToS(h));\
          QapTexMem*pMem=new QapTexMem(fn+"_"+to_string(w),w,h,(QapColor*)ptr);\
          if(MODE==2)pMem=m2(pMem);\
          Frame##NAME=Atlas.AddFrame(pMem);\
          if(MODE==2)pMem->CalcAlphaToRGB_and_set_new_alpha()->InvertRGB();\
          if(MODE==2)Frame##NAME##_s=GenShadowFrame(pMem);\
          f.pF=Frame##NAME;f.pS=Frame##NAME##_s;\
          delete pMem;\
          on_load_tex(fn);\
        });\
      }
      FRAMESCOPE(F);
      #undef F
    }
    #endif
  }
  #ifdef _WIN32
  HWND Sys_HWND=0;
  #else
  int Sys_HWND=0;
  #endif
  void Init()
  {
    srand(time(NULL));
    
    QAP_EM_LOG("before CreateFontMem");
    {
      QapTexMem*pNormMem=NormFont.CreateFontMem("Consolas",14,false,512,Sys_HWND);
      QapTexMem*pBlurMem=pNormMem->Clone();
      //pBlurMem->Blur(10);
      //pBlurMem->Blur(4);
      BlurTexture(pBlurMem,4);
      BlurFont=NormFont;
      BlurFont.Tex=GenTextureMipMap(qDev,pBlurMem);
      NormFont.Tex=GenTextureMipMap(qDev,pNormMem);
      //SysFont=FontCreate("Arial",16,false,512);
    }
    QAP_EM_LOG("before LoadFrames");
    LoadFrames();
    
    QAP_EM_LOG("before LoadTexture");
    if(1)
    {
      #ifndef QAP_UNIX
      LoadTexture(GFX_DIR+"market_car_v2_512.png",[&](const string&fn,int ptr,int w,int h){
        QapTexMem*pMem=new QapTexMem(fn+"_"+to_string(w),w,h,(QapColor*)ptr);
        th_rt_tex=GenTextureMipMap(qDev,pMem);
      });
      //th_rt_tex=GenTextureMipMap(ptm);
      
      LoadTexture(GFX_DIR+"market_car_v2_full_512.png",[&](const string&fn,int ptr,int w,int h){
        QapTexMem*pMem=new QapTexMem(fn+"_"+to_string(w),w,h,(QapColor*)ptr);
        th_rt_tex_full=GenTextureMipMap(qDev,pMem);
      });
      //ptm=LoadTexture("GFX\\market_car_v2_full.png");
      //th_rt_tex_full=GenTextureMipMap(ptm);
      #endif
    }
    QAP_EM_LOG("before qDev.Init();");
    qDev.Init(1024*32,1024*32*2);
    QAP_EM_LOG("after qDev.Init();");
    InitLevelsInfo();
    //RestartLevel();
    InitMenuSystem();
    update_user_name(true);
    RestartLevel();
    Menu->Down();
    
    QAP_EM_LOG("after init");
  }
  void InitLevelsInfo()
  {
    #define ADDLEVEL(CLASS){static TLevelFactory<CLASS>tmp;LevelsInfo.push_back(TLevelInfo(#CLASS,tmp));}
    LEVEL_LIST(ADDLEVEL);
    #undef ADDLEVEL
    LevelCounter.Maximum=LevelsInfo.size();
  }
  void NewGame(){
    LevelCounter.Value=0;
    RestartLevel();
  }
  void RestartLevel()
  {
    if(!LevelCounter){
      return;
    };
    Level.reset(LevelsInfo[LevelCounter.Value].Factory.Build(this));
    WaitWin.Stop();
    WaitFail.Stop();
  }
  void InitMenuSystem(){
    static class TOnResume:public IOnClick{
    public:
      void Call(TMenu*EX){
        EX->Game->Menu->Down();
      }
      virtual bool IsEnabled(TMenu*EX){return EX->Game->Level.get();}
    } OnResume;
    static class TOnRestartLevel:public IOnClick{
    public:
      void Call(TMenu*EX){
        EX->Game->RestartLevel();
        EX->Game->Menu->Down();
      }
      virtual bool IsEnabled(TMenu*EX){return EX->Game->LevelCounter.Value;}
    } OnRestartLevel;
    static class TOnNewGame:public IOnClick{
    public:
      void Call(TMenu*EX){
        EX->Game->NewGame();
        EX->Game->Menu->Down();
      }
    } OnNewGame;
    static class TOnBack:public IOnClick{
    public:
      void Call(TMenu*EX){
        EX->Back();
      }
    } OnBack;
    static class TOnNothing:public IOnClick{
    public:
      void Call(TMenu*EX){}
      virtual bool IsEnabled(TMenu*EX){return false;}
    } OnNothing;
    static class TOnUserName:public IOnClick{
    public:
      void Call(TMenu*EX){
        EX->Game->user_name_scene=true;
        EX->Game->user_name="";
      }
    } OnUserName;
    static class TOnSettings:public IOnClick{
    public:
      void Call(TMenu*EX){
        auto&Menu=EX->Game->Menu;
        auto OldMenu=std::unique_ptr<TMenu>(Menu.release());
        Menu.reset(new TMenu(EX->Game,"Settings"));
        Menu->Add("Under construction",&OnNothing);
        Menu->Add("Back",&OnBack);
        Menu->OldMenu=std::unique_ptr<TMenu>(OldMenu.release());
        Menu->Up();
      }
      //virtual bool IsEnabled(TMenu*EX){return false;}
    } OnSettings;
    static class TOnLevel:public IOnClick{
    public:
      void Call(TMenu*EX){
        auto&m=*EX->Game->Menu.get();
        //auto&lvls=EX->Game->LevelsInfo;
        //m.Items[m.CurID].Caption
        EX->Game->LevelCounter.Value=m.CurID;
        EX->Game->RestartLevel();
        m.Down();
      }
      //virtual bool IsEnabled(TMenu*EX){return false;}
    } TOnLevel;
    static class TOnLevels:public IOnClick{
    public:
      void Call(TMenu*EX){
        auto&Menu=EX->Game->Menu;
        auto OldMenu=std::unique_ptr<TMenu>(Menu.release());
        Menu.reset(new TMenu(EX->Game,"Levels"));
        for(int i=0;i<EX->Game->LevelsInfo.size();i++){
          auto&ex=EX->Game->LevelsInfo[i];
          Menu->Add(ex.Name,&TOnLevel);
        }
        Menu->Add("Back",&OnBack);
        Menu->OldMenu=std::unique_ptr<TMenu>(OldMenu.release());
        Menu->Up();
      }
      //virtual bool IsEnabled(TMenu*EX){return false;}
    } OnLevels;
    static class TOnAbout:public IOnClick{
    public:
      void Call(TMenu*EX){
        auto&Menu=EX->Game->Menu;
        auto OldMenu=std::unique_ptr<TMenu>(Menu.release());
        Menu.reset(new TMenu(EX->Game,"About"));
        Menu->Add("^2Aut^2hor ^2: ^8Ad^8ler^33D",&OnNothing);
        Menu->Add("^2Co^2de ^2: ^8Ad^8ler^33D",&OnNothing);
        Menu->Add("^2A^2r^2t ^2: ^8Ad^8ler^33D",&OnNothing);
        Menu->Add("Back",&OnBack);
        Menu->OldMenu=std::unique_ptr<TMenu>(OldMenu.release());
        Menu->Up();
      }
      //virtual bool IsEnabled(TMenu*EX){return false;}
    } OnAbout;
    static class TOnExit:public IOnClick{
    public:
      void Call(TMenu*EX){
        #ifdef _WIN32
        Sys.NeedClose=true;
        #endif
      }
    } OnExit;
    Menu.reset(new TMenu(this,"Main menu"));
    Menu->Add("Resume",&OnResume);
    Menu->Add("Restart level",&OnRestartLevel);
    Menu->Add("New game",&OnNewGame);
    Menu->Add("Levels",&OnLevels);
    Menu->Add("Change user_name",&OnUserName);
    Menu->Add("Settings",&OnSettings);
    Menu->Add("About",&OnAbout);
    Menu->Add("Exit",&OnExit);
    LevelCounter.Value=0;
    Menu->Down();
    //Menu->Up();
  }
  void Resume()
  {

  }
  #if(defined(_WIN32)||defined(QAP_UNIX))
  string user_name=to_string(g_clock.MS()*1000);
  #else
  string user_name=to_string(([](){return (unsigned long int)EM_ASM_INT({
    let arr=new Uint32Array(1);window.crypto.getRandomValues(arr);
    return arr[0];
  });})());
  #endif
  bool user_name_scene=false;
  void InputUserNameRender(){
    TextRender TE(&qDev);
    vec2d hs=vec2d(Sys.SM.W,Sys.SM.H)*0.5;
    real ident=24.0;
    real Y=0;
    qDev.SetColor(0xff000000);
    TE.BeginScope(-hs.x+ident,+hs.y-ident,&NormFont,&BlurFont);
    {
      const string PreesR=" ^7(^3press ^2R^7 ^3or ^2F9^7)";
      const string PreesSpace=" ^7(^3press ^2Space^7)";
      const string PreesEnter=" ^7(^3press ^2Return^7)";
      string BEG="^7";
      string SEP=" ^2: ^8";
      TE.AddText("^7The ^2Market Game");
      TE.AddText("");
      TE.AddText("^7Type your ^8user_name ^7 in english and press enter!");
      TE.AddText("^8user_name ^2: ^7"+user_name);
    }
    TE.EndScope();
  }
  bool need_init=false;
  bool check_char(char c){
    return InDip('a',c,'z')||InDip('A',c,'Z')||/*InDip('а',c,'я')||InDip('А',c,'Я')||*/InDip('0',c,'9');//||c=='ё'||c=='Ё';
  }
  void update_user_name(bool init){
    string un2=file_get_contents(user_name_fn);
    string un,un3;
    for(auto&c:un2)if(check_char(c))un.push_back(c);
    for(auto&c:user_name)if(check_char(c))un3.push_back(c);
    user_name=un.empty()?(un3.empty()?"unk_hacker"+IToS(g_clock.MS()):un3):un;
    if(init){file_put_contents(user_name_fn,user_name);}
  }
  string user_name_fn="user_name.txt";
  void InputUserNameUpdate(){
    if(need_init){
      need_init=false;
      update_user_name(false);
      user_name_scene=user_name.empty();
    }
    bool done=false;
    if(kb.News){
      auto c=kb.LastChar;
      if(check_char(c))user_name.push_back(c);
      if(user_name.size()>15)done=true;
    }
    if(kb.OnDown(VK_BACK))if(user_name.size())user_name.pop_back();
    if(kb.OnDown(VK_RETURN)||done){
      if(user_name.size()){
        user_name_scene=false;
        file_put_contents(user_name_fn,user_name);
      }
    }
  }
  bool RenderScene_debug=false;
  void RenderScene()
  {
    /*
    qDev.BindTex(0,0);
    qDev.SetColor(0xff000000);
    qDev.DrawQuad(0,0,512,512);
    if(!EndScene())return;
    Present();
    return;*/
    if(user_name_scene)return InputUserNameRender();
    if(bool need_draw_tank_hodun_rt=true)if(th_rt_tex)if(!Menu->InGame()){
      qDev.BindTex(0,th_rt_tex);
      qDev.SetColor(0xffffffff);
      qDev.DrawQuad(-512,0,th_rt_tex->W,th_rt_tex->H,0);
      qDev.BindTex(0,0);
    }
    if(RenderScene_debug)QAP_EM_LOG("before kb.A");
    if(kb.Down['A']&&!Menu->InGame()){
      if(1){
        qDev.BindTex(0,0);
        qDev.SetColor(0xffffffff);
        qDev.DrawQuad(kb.MousePos.x,kb.MousePos.y,96,96,0);
        qDev.SetColor(0xffff0000);
        qDev.DrawQuad(kb.MousePos.x,kb.MousePos.y,64,64,0);
      }
      qDev.BindTex(0,Atlas.pTex);
      //qDev.SetBlendMode(BT_SUB);
      qDev.SetColor(0xffffffff);
      qDev.DrawQuad(0.5,0.5,Atlas.W,Atlas.H,0);
    }
    if(RenderScene_debug)QAP_EM_LOG("after kb.A");
    if(!Menu)return;
    if(Menu->InGame())
    {
      auto check_frames=[&](){
        for(auto&ex:g_global_imgs)if(!ex.second.done){QAP_EM_LOG("inside check_frames::fail");return false;}
        return true;
      };
      if(Level.get())if(check_frames()){
        if(RenderScene_debug)QAP_EM_LOG("before Level->Render");
        Level->Render(qDev);
      }
    }else{
      TextRender TE(&qDev);
      qDev.SetColor(0xff000000);
      TE.BeginScope(0,0,&NormFont,&BlurFont);
      Menu->Render(qDev,TE);
      TE.EndScope();
    };
    {
      if(RenderScene_debug)QAP_EM_LOG("before RenderText");
      RenderText(qDev);
      if(RenderScene_debug)QAP_EM_LOG("after RenderText");
    }
  }
  #ifdef _WIN32
  void Render()
  {
    //if(!qDev.pDev->BeginScene())return;
    //qDev.Set2D();
    ////QapDX::Clear2d(1?QapColor(180,180,180):0xffc8c8c8);
    //{int v=231*0+206*0+210;qDev.pDev->Clear(QapColor(v,v,v));}
    //qDev.NextFrame();
    //RenderScene();
    //if(!qDev.pDev->EndScene())return;
    //qDev.Present();
  }
  #endif
  void RenderText(QapDev&qDev)
  {
    TextRender TE(&qDev);
    vec2d hs=vec2d(Sys.SM.W,Sys.SM.H)*0.5;
    real ident=24.0;
    qDev.SetColor(0xff000000);
    TE.BeginScope(-hs.x+ident,+hs.y-ident,&NormFont,&BlurFont);
    {
      const string PreesR=" ^7(^3press ^2R^7 ^3or ^2F9^7)";
      const string PreesSpace=" ^7(^3press ^2Space^7)";
      const string PreesEnter=" ^7(^3press ^2Return^7)";
      string BEG="^7";
      string SEP=" ^2: ^8";
      TE.AddText("^7The ^2Game");
      TE.AddText("");
      TE.AddText("^8user_name ^2: ^7"+user_name);
      TE.AddText("");
      #define GOO(TEXT,VALUE)TE.AddText(string(BEG)+string(TEXT)+string(SEP)+string(VALUE));
      GOO("Level",string(LevelCounter)+" ["+string(LevelCounter?LevelsInfo[LevelCounter.Value].Name:"noname")+"]");
      #undef GOO
      TE.AddText("");
      if(Level.get())Level->AddText(TE);
      TE.AddText("");
      if(WaitWin.Runned)TE.AddText("^2You win!"+PreesEnter);
      if(WaitFail.Runned)TE.AddText("^1You lose!"+PreesR);
      if(!WaitFail||!WaitWin){TE.AddText("^7game over!");}
    }
    TE.EndScope();
    if(bool need_draw_font=false){
      qDev.color=0xFF000000;
      qDev.BindTex(0,BlurFont.Tex);
      qDev.DrawQuad(1.5,-0.5,-512,512,Pi);
      qDev.color=0xFFFFFFFF;
      qDev.BindTex(0,NormFont.Tex);
      qDev.DrawQuad(0.5,0.5,-512,512,Pi);
    }
  }
  void Collide()
  {    
  }
  void NextLevel()
  {
    LevelCounter++;
    RestartLevel();
  }
  void OnWin(){
    //Level.reset();
  }
  void OnFail(){
    //Level.reset();
  }
  void Win(){WaitWin.Start();}
  void Fail(){WaitFail.Start();}
  void ReloadWinFail(){WaitFail.Stop();WaitWin.Stop();}
  void Update()
  {
    if(user_name_scene)return InputUserNameUpdate();
    QapAssert(Menu.get());
    //if(kb.OnDown(VK_ESCAPE)){if(Menu->InGame()){Menu->Up();}else{Menu->Down();}}
    //kb.UpdateMouse();
    if(Menu->InGame())
    {
      if(Level.get())
      {
        //QAP_EM_LOG("bef_update_level");
        Level->Update(this);
        //QAP_EM_LOG("aft_update_level");
        {WaitFail++;WaitWin++;}
        if(!WaitWin.Runned&&!WaitFail.Runned)
        {
          bool w=Level->Win();bool f=Level->Fail();
          if(f)Fail();
          if(w&&!f)Win();
        }else{
          if(!WaitFail)OnFail();
          if(!WaitWin)OnWin();
        }
      };
      if(kb.OnDown('R')){RestartLevel();}
      if(WaitFail.Runned||WaitWin.Runned)
      {
        if(WaitWin.Runned&&kb.Down[VK_RETURN]){NextLevel();}
      }
    }else{
      Menu->Update(this);
    }
  }
  void Free()
  {
    /*FreeFont(NormFont);FreeFont(BlurFont);
    UnloadTextures();*/
  }
  template<class FUNC>
  static void wget(const string&host,const string&dir,FUNC&&cb){
    #ifndef QAP_UNIX
    //emscripten_run_script(string("console.log('host = "+host+"');").c_str());
    auto url=host+dir;
    #ifndef _WIN32
    static int counter=0;counter++;
    auto fn=std::to_string(counter)+" "+url;
    auto&m=g_global_urls[fn];
    m.url=fn;
    m.on_load=std::move(cb);
    EM_ASM({
      fetchFile_v2(UTF8ToString($0));
    },int(fn.c_str()));
    return;
    #else
    QapClock clock;
    DownLoader dl(host,dir,"");
    dl.port=80;
    dl.start();
    for(;dl.update();){
      Sleep(0);
      if(clock.MS()>5000.0)break;
    }
    auto s=dl.GetContent(dl.data,true);
    dl.stop();
    cb(url,int(s.data()),s.size());
    #endif
    #endif
  }
  static string get_host(){
    static string host="185.92.223.117";static bool need_init=true;
    if(need_init){
      //wget("adler3d.github.io","/qap_vm/trash/test2025/game_host.txt",cb);
    }
    need_init=false;
    if(host.size()&&host.back()=='\n')host.pop_back();
    return host;
  }
};
TGame Game;
void update_last_char_from_keyboard(QapKeyboard&kb){
  kb.News=false;
  for(int key=0;key<QapKeyboard::TKeyState::MAX_KEY;key++){
    if(!(kb.Changed[key]&&kb.Down[key]))continue;
    if((key>='A'&&key<='Z')||(key>='a'&&key<='z')){
      bool shift=kb.Down[VK_SHIFT];
      char c=shift?toupper(key):tolower(key);
      kb.LastChar=c;kb.News=true;
      return;
    }
    if(key>='0'&&key<='9'){kb.LastChar=(char)key;kb.News=true;return;}
    if(key==VK_SPACE){kb.LastChar=' ';kb.News=true;return;}
    // printable ascii
    if(key>=32&&key<=126){kb.LastChar=char(key);kb.News=true;return;}
    if(key>=VK_NUMPAD0&&key<=VK_NUMPAD9){kb.LastChar='0'+(key-VK_NUMPAD0);kb.News=true;return;}
    if(key>='0'&&key<='9'){kb.LastChar=key;kb.News=true;return;}
  }
}
#ifndef _WIN32
#ifndef QAP_UNIX
void update_kb(){
  EM_ASM({update_kb($0,$1);},int(&kb.Down[0]),int(&kb.Changed[0]));
  kb.MousePos.x=+EM_ASM_INT({return g_mpos.x;})-Sys.SM.W/2;
  kb.MousePos.y=-EM_ASM_INT({return g_mpos.y;})+Sys.SM.H/2;
  update_last_char_from_keyboard(kb);
}
extern "C" {
  int render(int nope){
    Game.RenderScene();
    return 0;
  }
  int update(int nope){
    //QAP_EM_LOG("Game.RenderScene();");
    //Game.RenderScene();
    //QAP_EM_LOG("update_kb();");
    update_kb();
    Game.Update();
    return 0;
  }
}
void init(){
  qDev.Init(1024*64,1024*64*3);
  Game.Init();
}
extern "C" {
  int EMSCRIPTEN_KEEPALIVE qap_main(char*phost){
    g_host=phost;
    EM_ASM({console.log("QapLRv0.03");});
    EM_ASM({let d=document.body;d.innerHTML='<canvas id="glcanvas" width="'+window.innerWidth+'" height="'+window.innerHeight+'"></canvas>';});
    //EM_ASM({let d=document.body;d.innerHTML='<canvas id="glcanvas" width="'+d.Width+'" height="'+d.height+'"></canvas>';});
    Sys.SM.W=EM_ASM_INT({return window.innerWidth;});
    Sys.SM.H=EM_ASM_INT({return window.innerHeight;});
    srand(time(NULL));
    EM_ASM(main(););
    init();
    const char*argv[]={"./QapLR -i replay0.t_cdn_game_v0"};int argc=lenof(argv);
    //QapLR_main(argc,argv);
    return 0;
  }
}
//git pull&&emcc -o QapLR.html QapLR.cpp -s ASSERTIONS -s EXPORTED_FUNCTIONS='["_qap_main","_render","_update","_qap_on_load_img","_qap_on_load_url"]' --shell-file html_template/shell_minimal.html -s EXPORTED_RUNTIME_METHODS='["cwrap","ccall"]' -s INITIAL_MEMORY=2147483648 -s ALLOW_MEMORY_GROWTH=1 -gsource-map -O0
#endif
#else
class TD3DGameBoxBuilder
{
public:
public:
  typedef TD3DGameBoxBuilder SelfClass;
public:
  TD3DGameBox win;
  QapD3D9 D9;
  QapD3DDev9 D9Dev;
  QapDev qDev;
  int SleepMs;
public:
  void DoReset()
  {
    {
    }
    detail::FieldTryDoReset(this->win);
    detail::FieldTryDoReset(this->D9);
    detail::FieldTryDoReset(this->D9Dev);
    detail::FieldTryDoReset(this->qDev);
    (this->SleepMs)=(16);
  }
public:
  TD3DGameBoxBuilder(const TD3DGameBoxBuilder&)
  {
    QapDebugMsg("TD3DGameBoxBuilder"" is noncopyable");
    DoReset();
  };
  TD3DGameBoxBuilder()
  {
    DoReset();
  };
public:
  TD3DGameBoxBuilder(TD3DGameBoxBuilder&&_Right)
  {
    operator=(std::move(_Right));
  }
  void operator=(TD3DGameBoxBuilder&&_Right)
  {
    if(&_Right==this)return;
    {
    }
    this->win=std::move(_Right.win);
    this->D9=std::move(_Right.D9);
    this->D9Dev=std::move(_Right.D9Dev);
    this->qDev=std::move(_Right.qDev);
    this->SleepMs=std::move(_Right.SleepMs);
  }
public:
  void DoNice()
  {
    auto&builder=*this;
    builder.init("2025.05.28 21:30:14.960");
    builder.win.WindowMode();
    builder.init_d3d();
    UpdateWindow(builder.win.Form.WinPair.hWnd);
    builder.mainloop_v0(Sys.UPS);
  }
public:
  void SceneUpdate()
  {
    SceneUpdateEx();
    SceneRenderEx();
  }
public:
  void SceneUpdateEx()
  {
    SceneDoMove();
  }
  void SceneRenderEx()
  {
    auto&Dev=D9Dev;
    vec2i ClientSize=win.GetClientSize();
    if(!Dev.BeginScene())return;
    {
      D3DVIEWPORT9 ViewPort={0,0,(DWORD)ClientSize.x,(DWORD)ClientSize.y,-1.f,+1.f};
      Dev.pDev->SetViewport(&ViewPort);
      Dev.Set2D(vec2i(0,0),1.0,0,&ClientSize);
      Dev.Clear2D(0xffc8c8c8);
      qDev.NextFrame();
      SceneDoDraw();
    }
    if(!Dev.EndScene())return;
    RECT Rect={0,0,ClientSize.x,ClientSize.y};
    Dev.Present(&Rect,&Rect);
  }
public:
  virtual void DoMove() {};
  virtual void DoDraw() {};
public:
  void SceneDoMove()
  {
    auto&Dev=D9Dev;
    if(!qDev.DynRes.pDev)
    {
      qDev.MountDev(Dev);
      qDev.Init(1024*1024,1024*1024*3);
    }
    DoMove();
  }
  void SceneDoDraw()
  {
    DoDraw();
  }
public:
  void init_d3d()
  {
    HWND hwnd=win.Form.WinPair.hWnd;
    D9.Init();
    D9Dev.PresParams.SetToDef(hwnd,true);
    D9Dev.Init(hwnd,D9);
  }
  void init(const string&caption)
  {
    win.Caption=caption;
    win.Init(caption);
    win.Visible=false;
    win.WindowMode();
    init_d3d();
    win.Visible=true;
    win.WindowMode();
    UpdateWindow(win.Form.WinPair.hWnd);
  }
  /*
  void at_iter_begin(){
    HWND&hWnd=win.Form.WinPair.hWnd;
    win.zDelta=0;kb;
    QapAssert(!win.Keyboard.Down[mbLeft]);
    QapAssert(!win.Keyboard.Changed[mbLeft]);
    win.Keyboard.Sync();
    QapAssert(!win.Keyboard.Down[mbLeft]);
    QapAssert(!win.Keyboard.Changed[mbLeft]);
    if(GetForegroundWindow()==hWnd){
      win.mpos=win.unsafe_GetMousePos();
    }
  }
  std::atomic<bool> finished=false;
  static void app_mainloop(TD3DGameBoxBuilder*pbox){
    auto&box=*pbox;auto&win=box.win;auto&finished=box.finished;
    QapKeyboard tmp;
    for(;!finished;){

      QapAssert(!win.Keyboard.Down[mbLeft]);
      QapAssert(!win.Keyboard.Changed[mbLeft]);
      box.SceneUpdate();
      QapAssert(!win.Keyboard.Down[mbLeft]);
      QapAssert(!win.Keyboard.Changed[mbLeft]);
      box.at_iter_begin();
      //Sleep(0);
    }
    win.Runned=false;
    finished=true;
  }
  void loop()
  {
    std::thread th(app_mainloop,this);
    auto&NeedClose=(std::atomic<bool>&)win.NeedClose;
    for(;!finished&&!NeedClose;)
    {
      MSG msg;msg.pt={0,0};
      auto bRet=GetMessageA(&msg,0,0,0);
      if(!bRet)break;
      if(bRet==-1){
        QapDebugMsg("handle the error and possibly exit");
        break;
      }
      if(bool msg_debug=false)qap_add_back(win.msglog).load(msg,0);
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      QapAssert(!win.Keyboard.Down[mbLeft]);
      QapAssert(!win.Keyboard.Changed[mbLeft]);
    }
    finished=true;
    th.join();
    //for(int i=0;win.Runned;i++)
    //{
    //  win.Update();
    //  if(win.NeedClose)win.Runned=false;
    //}
  }
  */
public:
  typedef QapDev::t_geom t_geom;
public:
#if(0)
  struct t_simbol{
    struct t_quad{
      vec2i p;
      vec2i s;
      void set(int x,int y,int w,int h){p.x=x;p.y=y;s.x=w;s.y=h;}
    };
    vector<t_quad> arr;
  };
  struct qap_text{
    struct t_simbol{
      struct t_quad{
        vec2i p;
        vec2i s;
        void set(int x,int y,int w,int h){p.x=x;p.y=y;s.x=w;s.y=h;}
      };
      t_geom geom;
      real ps;
      vector<t_quad> arr;
      void make_geom(real ps)
      {
        this->ps=ps;
        for(int j=0;j<arr.size();j++)
        {
          auto&ex=arr[j];
          auto p=vec2d(ex.p.x,-ex.p.y)*ps+(vec2d(ex.s)-vec2d(1,1))*ps*0.5;
          auto sx=ps*ex.s.x;
          auto sy=ps*ex.s.y;
          geom.add(QapDev::GenGeomQuad(p,sx,sy));
        }
      }
    };
    struct t_mem{
      QapTexMem out;
      QapFontInfo info;
      vector<t_simbol> arr;
      vector<unsigned char> x_chars;
      //int def_text_size;
      real ms;
      t_mem():ms(0){}
      void prepare(HWND hwnd){if(arr.empty())init(hwnd);}
      void init(HWND hwnd)
      {
        QapAssert(arr.empty());
        if(!out.W)CreateFontMem(hwnd,out,info,"system",8,false,512,true);
        arr.resize(256);
        for(int i=0;i<256;i++)
        {
          unsigned char c=i;
          auto&cinfo=arr[c];
          cinfo.ps=-1;
          auto size=info.Coords[c];
          auto p=vec2i(c%16,c/16);
          int n=out.W/16;
          auto s=out.Arr.get();
          auto buffx=cinfo.arr;
          auto buffy=cinfo.arr;
          //if(use_x)
          {
            for(int y=0;y<n;y++)for(int x=0;x<size.x;x++)
            {
              auto addr=p.x*n+x+(p.y*n+y)*out.W;
              auto&ex=s[addr];
              if(!ex.a)continue;
              int sx=1;
              for(int cx=x+1;cx<size.x;cx++){
                if(!s[addr-x+cx].a)break;
                sx++;
              }
              qap_add_back(buffx).set(x,y,sx,1);
              if(sx>1)x+=sx-1;
            }
          }
          //if(!use_x)
          {
            for(int x=0;x<size.x;x++)for(int y=0;y<n;y++)
            {
              auto addr=p.x*n+x+(p.y*n+y)*out.W;
              auto&ex=s[addr];
              if(!ex.a)continue;
              int sy=1;
              for(int cy=y+1;cy<n;cy++){
                auto naddr=p.x*n+x+(p.y*n+cy)*out.W;
                if(!s[naddr].a)break;
                sy++;
              }
              qap_add_back(buffy).set(x,y-1,1,-sy);
              if(sy>1){
                y+=sy-1;
              }
            }
          }
          bool best_x=buffx.size()<buffy.size();
          if(best_x){
            qap_add_back(x_chars)=c;
          }
          cinfo.arr=best_x?buffx:buffy;
        }
      }
    };
    static t_mem&get_mem(HWND hwnd){static t_mem mem;mem.prepare(hwnd);return mem;}
    static vec2i get_size(QapDev&qDev,const string&text,int cell_size=32)
    {
      auto&mem=get_mem(qDev.DynRes.pDev->PresParams.pp.hDeviceWindow);
      QapFontInfo&info=mem.info;
      vector<t_simbol>&arr=mem.arr;
      auto ps=cell_size*0.125*0.5;
      auto offset=vec2i_zero;
      for(int i=0;i<text.size();i++)
      {
        unsigned char c=text[i];
        auto size=info.Coords[c];
        offset.x+=size.x*ps;
      }
      return vec2i(offset.x,info.Coords[' '].y);
    }
    static void draw(QapDev&qDev,int px,int py,const string&text,int cell_size=32){draw(qDev,vec2i(px,py),text,cell_size);}
    static void draw(QapDev&qDev,const vec2d&pos,const string&text,int cell_size=32){draw(qDev,vec2i(pos.x,pos.y),text,cell_size);}
    static void draw(QapDev&qDev,const vec2i&pos,const string&text,int cell_size=32)
    {
      auto&mem=get_mem(qDev.DynRes.pDev->PresParams.pp.hDeviceWindow);
      QapFontInfo&info=mem.info;
      vector<t_simbol>&arr=mem.arr;
      auto ps=cell_size*0.125*0.5;
      auto offset=pos;
      for(int i=0;i<text.size();i++)
      {
        unsigned char c=text[i];
        auto size=info.Coords[c];
        auto&ex=arr[c];
        if(ex.ps<0)ex.make_geom(ps);
        if(ex.ps==ps)
        {
          qDev.DrawGeomWithOffset(ex.geom,offset);
        }
        if(ex.ps!=ps)
        {
          auto&quads=ex.arr;
          for(int j=0;j<quads.size();j++)
          {
            auto&ex=quads[j];
            auto p=vec2d(ex.p.x,-ex.p.y)*ps+(vec2d(ex.s)-vec2d(1,1))*ps*0.5;
            auto sx=ps*ex.s.x;
            auto sy=ps*ex.s.y;
            qDev.DrawQuad(offset+p,sx,sy);
          }
        }
        offset.x+=size.x*ps;
      }
    }
  };
#endif
public:
  vec2d get_dir_from_keyboard_wasd_and_arrows()
  {
    auto&kb=win.Keyboard;
    vec2d dp={};
    auto dir_x=vec2d(1,0);
    auto dir_y=vec2d(0,1);
    #define F(dir,key_a,key_b)if(kb.Down[key_a]||kb.Down[key_b]){dp+=dir;}
    F(-dir_x,VK_LEFT,'A');
    F(+dir_x,VK_RIGHT,'D');
    F(+dir_y,VK_UP,'W');
    F(-dir_y,VK_DOWN,'S');
    #undef F
    return dp;
  }
public:
  QapKeyboard&kb=win.Keyboard;
  bool&UPS_enabled=Sys.UPS_enabled;
  bool ResetFlag=false;
  void mainloop_v0(int&UPS){
    kb_v2= new QapKeyboard();
    kb_v3= new QapKeyboard();
    auto sync_kb_and_mouse=[&](){
      win.mpos=win.unsafe_GetMousePos();
      kb.MousePos=win.mpos;
      kb.Sync();
      win.zDelta=0;
    };
    MSG msg;
    auto&IsQuit=win.NeedClose;
    IsQuit=false;
    real dt=real(1000.0)/real(UPS),t=0;QapClock LoopClock;
    int WarningCounter=0;bool new_state=true;
    while(!IsQuit&&!Sys.NeedClose)
    {  
      LoopClock.Stop();t+=LoopClock.MS();LoopClock.Start();
      QapClock UpdateClock,RenderClock;
      if(UPS_enabled){
        for(;(t>0)&&!IsQuit&&UPS_enabled;t-=dt)
        {
          UpdateClock.Start();
          sync_kb_and_mouse();
          struct t_kb{uchar data[sizeof(kb)];};
          ((t_kb&)(*kb_v3))=(t_kb&)kb;
          while(PeekMessage(&msg,win.Form.WinPair.hWnd,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}
          ((t_kb&)(*kb_v2))=(t_kb&)kb;
          //if(paused)PrevPauseCounter=PauseCounter;
          //if(!paused&&(PauseCounter-PrevPauseCounter>1)){exit(0);}  // - костыль для проверки слишком быстрого нажатия паузы
          //PrevPauseCounter=PauseCounter;
          SceneUpdateEx();new_state=true;
          UpdateClock.Stop();
          if(ResetFlag){ResetFlag=false;t=0;break;};
          if(t/dt>real(UPS))if(UpdateClock.MS()*UPS>1000.0){WarningCounter++;/*MACRO_ADD_LOG("("+FToS(t)+"ms)Too much load on the CPU",lml_WARNING);*/t=0;break;}
        }
        {
          RenderClock.Start();
          if(new_state){SceneRenderEx();}new_state=false;
          RenderClock.Stop();
          if(RenderClock.MS()>500.0){WarningCounter++;/*MACRO_ADD_LOG("("+FToS(RenderClock.MS())+"ms)Too much load on the GPU",lml_WARNING);*/}
        }
        if(WarningCounter>64){/*MACRO_ADD_LOG("Too weak system",lml_ERROR);*/break;}
      }else{
        sync_kb_and_mouse();
        while(PeekMessage(&msg,win.Form.WinPair.hWnd,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}
        SceneUpdateEx();
        SceneRenderEx();
      }
    }
  }
};
//auto&g_qDev=qDev;
auto&g_kb=kb;
class TWin32Game:public TD3DGameBoxBuilder{
public:
  bool need_init=true;
  void auto_init(){
    if(!need_init)return;
    need_init=false;
    Game.Sys_HWND=win.Form.WinPair.hWnd;
    Game.qDev.MountDev(D9Dev);
    Game.qDev.Init(1024*64,1024*64*3);
    Game.Init();
  }
  void DoMove()override{
    auto_init();
    struct t_kb{uchar data[sizeof(kb)];};
    ((t_kb&)g_kb)=(t_kb&)win.Keyboard;
    g_kb.MousePos=win.mpos;
    Game.Update();
  };
  void DoDraw()override{
    auto_init();
    Game.qDev.NextFrame();
    Game.RenderScene();
  };
  void Free(){Game.qDev.Free();}
};

#include <shellapi.h>  // CommandLineToArgvW
#include <locale>

// Альтернатива без codecvt (рекомендуется)
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
        argv_storage.back().push_back('\0'); // гарантируем null-терминатор
        argv.push_back(argv_storage.back().data());
    }
    int QapLR_main(int argc,char*argv[]);
    int result=QapLR_main(argc,argv.data());

    LocalFree(wargv); // освобождаем память, выделенную CommandLineToArgvW
    return result;
}
int QapLR_DoNice(){
  TScreenMode SM=GetScreenMode();
  Sys.SM=SM;
  TWin32Game Game;
  Game.DoNice();
  Game.Free();
	return 0;
}
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
  GlobalEnv global_env(hInstance,hPrevInstance,lpCmdLine,nCmdShow);
  void test();test();
  return QapLR_WinMain(hInstance,hPrevInstance,lpCmdLine,nCmdShow);
  QapLR_DoNice();
}
#endif
#ifdef QAP_UNIX
#include <iostream>
int main(int argc, char* argv[]){
  //cerr<<"QAP_UNIX==true"<<endl;
  void test();test();
  int QapLR_main(int argc,char*argv[]);
  return QapLR_main(argc,argv);
}
#endif

const char* VERSION = "QapLR v0.1.0";
const char* USAGE = R"(Usage:
  Normal mode:     ./QapLR <world> <players> <seed_init> <seed_strat> [OPTIONS]
  Replay-in mode:  ./QapLR -i <file> [OPTIONS]
  Replay-out mode: ./QapLR -o <file> <world> <players> <seed_init> <seed_strat> [OPTIONS]

Options:
  -i, --replay-in FILE      Play back a recorded replay
  -o, --replay-out FILE     Record a replay to FILE after game
  -s, --state-file FILE     Use custom initial state (use '-' for stdin)
  -g, --gui                 Enable graphical mode
  -n, --player-names a b c  Provide player names (requires --gui)
  -d, --debug 0             Enable debug(repeat) mode for player 0
  -r, --remote              Enable remote mode
  -p, --ports_from 31000    Use ports 31000..31003 if players==4
  -h, --help                Show this help
  -v, --version             Show version
)";

void printHelp(const char*prog) {
    cerr << "QapLR : Local Runner\n\n";
    cerr << USAGE <<endl;
}

bool parseArgs(int argc,char*argv[],ProgramArgs&args){
    // Проверка --help / --version ДО всего
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            args.show_help = true;
            return true;
        }
        if (arg == "--version" || arg == "-v") {
            args.show_version = true;
            return true;
        }
    }

    // Отображение коротких ↔ длинных флагов
    std::map<std::string, std::string> short_to_long = {
        {"-i", "--replay-in"},
        {"-o", "--replay-out"},
        {"-s", "--state-file"},
        {"-g", "--gui"},
        {"-n", "--player-names"},
        {"-d", "--debug"},
        {"-r", "--remote"},
        {"-p", "--ports_from"},
        {"-h", "--help"},
        {"-v", "--version"}
    };

    // Список всех известных флагов (для проверки)
    std::set<std::string> known_flags;
    for(auto&[s,f]:short_to_long)known_flags.insert(f);

    // Вектор "токенов": каждый — либо флаг, либо позиционный аргумент
    struct Token {
        enum { FLAG, POSITIONAL } type;
        std::string value;
        std::vector<std::string> payload; // для --player-names и т.п.
    };
    std::vector<Token> tokens;

    // Первый проход: разбор на токены с учётом потребления аргументов флагами
    for (int i = 1; i < argc; ) {
        std::string arg = argv[i];

        // Преобразуем короткий флаг в длинный
        if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
            if (short_to_long.count(arg)) {
                arg = short_to_long[arg];
            } else {
                std::cerr << "Unknown option: " << argv[i] << "\n";
                return false;
            }
        }

        if (arg.size() >= 2 && arg.substr(0, 2) == "--") {
            if (!known_flags.count(arg)) {
                std::cerr << "Unknown option: " << arg << "\n";
                return false;
            }

            Token tok{Token::FLAG, arg};

            if(arg=="--replay-in"||arg=="--replay-out"||arg=="--state-file"||arg=="--debug"||arg=="--ports_from"){
                if (i + 1 >= argc) {
                    std::cerr << arg << " requires an argument\n";
                    return false;
                }
                tok.payload.push_back(argv[++i]);
            }
            else if (arg == "--player-names") {
                int j = i + 1;
                while (j < argc) {
                    std::string next = argv[j];
                    // Если следующий — флаг (начинается с -), останавливаемся
                    if (next.size() >= 1 && next[0] == '-') {
                        // Проверим, не короткий ли это флаг
                        if (next.size() >= 2 && next[1] != '-' && short_to_long.count(next)) {
                            break;
                        }
                        // Или длинный флаг
                        if (next.substr(0, 2) == "--" && known_flags.count(next)) {
                            break;
                        }
                    }
                    tok.payload.push_back(next);
                    ++j;
                }
                if (tok.payload.empty()) {
                    std::cerr << "--player-names requires at least one name\n";
                    return false;
                }
                i = j - 1; // компенсируем ++i ниже
            }
            // --gui, --help, --version — без аргументов

            tokens.push_back(tok);
            ++i;
        } else {
            // Позиционный аргумент
            tokens.push_back({Token::POSITIONAL, arg});
            ++i;
        }
    }

    // Теперь извлекаем данные из токенов
    std::vector<std::string> positional_args;

    for (const auto& tok : tokens) {
        if (tok.type == Token::FLAG) {
            const std::string& flag = tok.value;
            if (flag == "--replay-in") {
                args.mode = ProgramArgs::Mode::ReplayIn;
                args.replay_in_file = tok.payload[0];
            } else if (flag == "--replay-out") {
                args.mode = ProgramArgs::Mode::ReplayOut;
                args.replay_out_file = tok.payload[0];
            } else if (flag == "--state-file") {
                args.state_file = tok.payload[0];
            } else if (flag == "--gui") {
                args.gui_mode = true;
            } else if (flag == "--player-names") {
                args.player_names = tok.payload;
            } else if (flag == "--debug") {
                args.debug = std::stoi(tok.payload[0]);
            } else if (flag == "--remote") {
                args.remote = true;
            } else if (flag == "--ports_from") {
                args.ports_from = std::stoi(tok.payload[0]);
            }
        } else {
            positional_args.push_back(tok.value);
        }
    }

    // === Обработка режимов ===

    // Для Normal и ReplayOut: должно быть ровно 4 позиционных аргумента
    if (positional_args.size() != 4) {
        std::cerr << "Expected exactly 4 positional arguments: <world> <players> <seed_init> <seed_strat>\n";
        std::cerr << "Got: ";
        for (const auto& a : positional_args) std::cerr << "'" << a << "' ";
        std::cerr << "\n";
        return false;
    }

    // Парсим позиционные
    args.world_name = positional_args[0];

    try {
        args.num_players = std::stoi(positional_args[1]);
        if (args.num_players <= 0) throw std::exception{};
    } catch (...) {
        std::cerr << "Invalid number of players: " << positional_args[1] << "\n";
        return false;
    }

    auto parse_seed = [](const std::string& s) -> uint64_t {
        if (s.empty()) throw std::exception{};
        char* end;
        unsigned long long val = std::strtoull(s.c_str(), &end, 10);
        if (*end != '\0') throw std::exception{};
        return static_cast<uint64_t>(val);
    };

    try {
        args.seed_initial = parse_seed(positional_args[2]);
        args.seed_strategies = parse_seed(positional_args[3]);
    } catch (...) {
        std::cerr << "Seeds must be non-negative integers\n";
        return false;
    }

    // Проверка имён игроков
    if (!args.player_names.empty()) {
        if (static_cast<int>(args.player_names.size()) != args.num_players) {
            std::cerr << "Number of player names (" << args.player_names.size()
                      << ") does not match num_players (" << args.num_players << ")\n";
            return false;
        }
    }

    if (args.mode == ProgramArgs::Mode::ReplayIn) {
        //if (!positional_args.empty()) {
        //    std::cerr << "Replay-in mode accepts no positional arguments\n";
        //    return false;
        //}
        // player_names можно проверить позже (по данным из реплея)
        return true;
    }

    return true;
}
int QapLR_main(int argc,char*argv[]){
    /*
    for(int i=0;i<1000;i++){
      Sleep(16);
    }*/
    ProgramArgs&args=g_args;
    if (!parseArgs(argc, argv, args)) {
        return 1;
    }

    if (args.show_help) {
        cerr << VERSION << "\n";
        printHelp("./QapLR");
        return 0;
    }
    if (args.show_version) {
        cerr << VERSION << "\n";
        return 0;
    }

    bool debug=args.debug>=0&&args.debug<args.num_players;
    switch (args.mode) {
        case ProgramArgs::Mode::ReplayIn:
            cerr << "Replaying from: " << *args.replay_in_file << "\n";
            if (args.gui_mode) cerr << "GUI enabled\n";
            if (debug) cerr << "debug/repeat enabled for player "+to_string(args.debug)+"\n";
            break;

        case ProgramArgs::Mode::ReplayOut:
            cerr << "Recording replay to: " << *args.replay_out_file << "\n";
            [[fallthrough]];
        case ProgramArgs::Mode::Normal:
            cerr << "World: " << args.world_name << "\n";
            cerr << "Players: " << args.num_players << "\n";
            cerr << "Seed (initial): " << args.seed_initial << "\n";
            cerr << "Seed (strategies): " << args.seed_strategies << "\n";
            if (args.state_file) {
                cerr << "Custom state file: " << *args.state_file << "\n";
            }
            if (args.gui_mode) cerr << "GUI enabled\n";
            break;
    }

    if(args.replay_in_file){
      auto s=file_get_contents(*args.replay_in_file);
      t_cdn_game replay;
      //QapLoadFromStr(replay,s);
      t_cdn_game_builder b{replay};
      b.feed(s);
      g_args.num_players=replay.gd.arr.size();
      g_args.seed_initial=replay.gd.seed_initial;
      g_args.seed_strategies=replay.gd.seed_strategies;
      g_args.world_name=replay.gd.world;
      //QapLoadFromStr(session.replay,s);
      g_args.gui_mode=true;
      session.init();//auto w=session.world->clone();int i=-1;session.ws2.push_back(w->clone());
#if(0)
      auto&arr=replay.tick2cmds;
      for(int tick=0;tick<arr.size();tick++){
        auto&ex=arr[tick];
        //i++;if(i>30)break;
        int n=0;
        for(int i=0;i<ex.size();i++){
          string outerr;
          session.world->use(i,ex[i],outerr);
          if(outerr.size()){
            int gg=1;
            n++;
          }
        }
        if(n==ex.size()){
          int fail=1;
        }
        session.world->step();
        session.ws.push_back(session.world->clone());
      }
#endif
      //auto replay2=session.replay;replay2={};
      //auto mem=file_get_contents("replay14.cmds");
      //QapLoadFromStr(replay2,mem);int j=-1;
      //for(auto&ex:replay2){
      //  j++;auto&ey=session.replay[j];
      //  for(int i=0;i<g_args.num_players;i++){
      //    if(ey[i]!=ex[i]||1){
      //      int what=1;
      //      TGame::t_splinter::t_world::t_cmd a,b;
      //      QapLoadFromStr(a,ey[i]);
      //      QapLoadFromStr(b,ex[i]);
      //      int gg=1;
      //    }
      //    string outerr;
      //    w->use(i,ex[i],outerr);
      //  }
      //  w->step();
      //  session.ws2.push_back(w->clone());
      //  string vpow2,vpow1;
      //  session.ws[j+1]->get_vpow(0,vpow1);
      //  w->get_vpow(0,vpow2);
      //  if(vpow2!=vpow1){
      //    int wtf=1;
      //  }
      //}
      #ifdef _WIN32
      return QapLR_DoNice();
      #else
      return 0;
      #endif
    }
    session.init();
    struct t_player{
      unique_ptr<t_server_api> server;
      int client_id=-1;
      bool broken=false;
      string recv_buffer;
      struct t_conn:GameSession::i_connection{
        t_player*p=nullptr;
        void send(const string&msg)override{if(p)p->server->send_to_client(p->client_id,msg);};
        void err(const string&msg)override{};
        void off()override{
          if(!p)return;
          if(network_state==nsDef)network_state=nsOff;
          if(p->server)p->server->disconnect_client(p->client_id);
        };
      } conn;
      struct t_conn2:GameSession::i_connection{
        t_player*p=nullptr;
        void send(const string&msg)override{if(p)zchan_write("p"+to_string(p->client_id),msg);};
        void send_seed(const string&msg)override{if(p)zchan_write("seed/"+to_string(p->client_id),msg);};
        void send_tick(int tick,double ms)override{zchan_write("new_tick",to_string(tick)+","+to_string(ms));};
        void err(const string&msg)override{if(p)zchan_write("err"+to_string(p->client_id),msg);};
        void off()override{
          if(!p)return;
          if(network_state==nsDef)network_state=nsOff;
          if(p->server)p->server->disconnect_client(p->client_id);
        };
      } conn2;
    };
    vector<t_player> players;
    players.resize(args.num_players);
    auto kill=[&](t_player&p,int i){
      p.broken=true;
      if(p.conn.network_state==p.conn.nsDef)p.conn.network_state=p.conn.nsErr;
      p.conn.p=nullptr;
      session.set_connected(i,false);
      session.update();
    };
    if (!args.player_names.empty()) {
        cerr << "Player names: ";
        for (const auto& n : args.player_names) cerr << n << " ";
        cerr << "\n";
    }
    if (args.remote){
      if (debug) cerr << "debug/repeat ignored?\n";
      std::cerr << "Remote protocol over stdin/stdout enabled\n";
      t_player::t_conn2 con;session.pcon=&con;
      // --- Zchan reader из stdin ---
      std::atomic<bool> reader_running{true};
      emitter_on_data_decoder decoder;
      std::thread stdin_reader([&]() {
        decoder.cb = [&](const std::string& z, const std::string& payload) {
          if (z.size() >= 5 && z.substr(0, 5) == "drop/") {
            // Обработка отключения игрока по сигналу от t_node (TL/ML и т.п.)
            if (std::all_of(z.begin() + 5, z.end(), ::isdigit)) {
              int player_index = std::stoi(z.substr(5));
              if (qap_check_id(players,player_index)) {
                auto& p = players[player_index];
                if (!p.broken) {
                  std::cerr << "Player " << player_index << " dropped by t_node\n";
                  kill(p,player_index);
                }
              }
            }
          } else if (z.size() >= 1 && z[0] == 'p' && std::all_of(z.begin() + 1, z.end(), ::isdigit)) {
            // Обработка игровых данных от игрока
            int player_index = std::stoi(z.substr(1));
            //std::cerr << "Player " << player_index << " payload.len= (" << payload.size() << ")\n";
            if (qap_check_id(players,player_index)) {
              auto& p = players[player_index];
              if (p.broken) return;

              p.recv_buffer.append(payload);
              while (!session.end) {
                if (p.recv_buffer.size() < sizeof(uint32_t)) break;
                uint32_t len = *reinterpret_cast<const uint32_t*>(p.recv_buffer.data());
                if (len > 1024 * 1024) {
                  std::cerr << "Player " << player_index << " sent too large packet (" << len << ")\n";
                  kill(p,player_index);
                  zchan_write("violate/" + std::to_string(player_index), "oversized packet");
                  break;
                }
                size_t packet_size = sizeof(uint32_t) + len;
                if (p.recv_buffer.size() < packet_size) break;

                std::string cmd(p.recv_buffer.data() + sizeof(uint32_t), len);
                //std::cerr << "Player " << player_index << " cmd.len= (" << cmd.size() << ")\n";
                session.submit_command(player_index, cmd);
                session.update();

                if (!session.end) {
                  p.recv_buffer.erase(0, packet_size);
                }
              }
            }
          }
          // Игнорируем неизвестные zchan
        };
        InputReader reader;
        char buffer[4096];
        while (reader_running){
          std::streamsize avail = reader.available();
          if (avail > 0) {
            std::streamsize toRead = std::min(avail, static_cast<std::streamsize>(sizeof(buffer)));
            //LOG("bef read avail="+to_string(avail));
            std::streamsize n = reader.read(buffer, toRead);
            //LOG("aft read n="+to_string(n));
            if (n <= 0) break;
            auto r=decoder.feed(buffer, static_cast<size_t>(n));
            //LOG("decoder.feed(buf,"+to_string(n)+")="+r.to_str()+";buff="+string(buffer,n));
          } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
        }
        //char buffer[4096];
        //while (reader_running) {
        //  LOG("bef read");
        //  std::cin.read(buffer, sizeof(buffer));
        //  std::streamsize n = std::cin.gcount();
        //  LOG("gcount="+to_string(n));
        //  if (n <= 0) break; // EOF или ошибка
        //  auto r=decoder.feed(buffer, static_cast<size_t>(n));
        //  LOG("decoder.feed(buf,"+to_string(n)+")="+r.to_str()+";buff="+string(buffer,n));
        //}
      });

      for (int i = 0; i < players.size(); ++i) {
        auto&p=players[i];
        session.carr[i]=&p.conn2;
        p.conn2.p=&p;
      }

      for (int i = 0; i < players.size(); ++i) {
        players[i].client_id = i;
        players[i].broken = false;
        session.set_connected(i, true);
      }

      session.send_seed_to_all();
      session.send_vpow_to_all();
      session.clock.Start();

      while (!session.end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        //LOG("decoder.buf.len="+to_string(decoder.buffer.size()));
      }
      zchan_write("result",session.gen_result());
      zchan_write("finished",qap_time());
      reader_running = false;
      if (stdin_reader.joinable()) {
        stdin_reader.join();
      }
      for(;;){
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        LOG("infinity waiting loop");
      }
      return 0;
    }
    if(args.ports_from>=0){
      cerr << "Ports-from enabled\n";
      for(int i=0;i<players.size();i++){
        auto&p=players[i];
        p.server=make_unique<t_server_api>(args.ports_from+i);
        auto&server=*p.server;
        server.onClientConnected = [&,i](int client_id, socket_t socket, const std::string& ip) {
          std::cerr << "[" << client_id << "] connected from IP " << ip <<"\" to socket at port "<<server.port<<endl;
          if(p.client_id>=0)return;
          p.client_id=client_id;
          p.conn.p=&p;
          session.carr[i]=&p.conn;
          session.set_connected(i,true);
          //---
          int n=0,a=players.size();
          for(auto&ex:players)if(ex.client_id>=0){n++;if(ex.broken)a--;}
          bool full=n==players.size();
          if(full){
            session.send_seed_to_all();
            session.send_vpow_to_all();
            session.clock.Start();
          }
          if(bool debug=false){
            p.conn.send("hi");
            int gg=1;
          }
        };

        server.onClientDisconnected = [&,i](int client_id) {
          if(session.end)return;
          std::cerr << "[" << client_id << "] disconnected from socket at port "<<server.port<<endl;
          if(p.client_id!=client_id)return;
          kill(p,i);
        };

        server.onClientData = [&,i](int client_id, const std::string& data, std::function<void(const std::string&)> send) {
          if(session.end)return;
          //std::cerr << "[" << client_id << "] received from socket at port "<<server.port<<": " << data<<endl;
          if (p.broken || p.client_id != client_id) return;
          p.recv_buffer.append(data);
          while(!session.end){
            if(p.recv_buffer.size()<sizeof(uint32_t))break;
            uint32_t len=*reinterpret_cast<const uint32_t*>(p.recv_buffer.data());
            if (len > 1024 * 1024) {
              cerr << "Player " << i << " sent too large packet (" << len << ")\n";
              kill(p,i);
              break;
            }
            size_t packet_size=sizeof(uint32_t)+len;
            if (p.recv_buffer.size()<packet_size)break;
            string cmd(p.recv_buffer.data() + sizeof(uint32_t), len);
            session.submit_command(i,cmd);
            session.update();
            if(!session.end)p.recv_buffer.erase(0,packet_size);
          }
        };
        server.start();
      }
    }
    auto shutdown=[&]{
      #ifdef _WIN32
      cerr << "=== QapLR: TerminateProcess(GetCurrentProcess(),0) ===" << endl;
      TerminateProcess(GetCurrentProcess(),0);
      exit(0);
      return 0;
      #endif
      {
        lock_guard<mutex> lock(session.mtx);
        session.end=true;
        for(auto&ex:session.carr)if(ex)ex->off();
      }
      for(auto&ex:players)ex.server=nullptr;
      cerr << "=== QapLR: shutting down ===" << endl;
    };
    #ifdef _WIN32
    if(args.gui_mode){
      auto rv=QapLR_DoNice();
      shutdown();
      return rv;
    }
    #else
    if(args.gui_mode){cerr<<"gui_mode ignored"<<endl;}args.gui_mode=false;
    #endif
    if(!args.gui_mode){
      for(;!session.is_inited();){
        this_thread::sleep_for(16ms);
      }
      for(;;){
        bool done=session.is_finished();
        if(done)break;
        this_thread::sleep_for(16ms);
      }
    }
    shutdown();
    return 0;
}
void test(){
  std::optional<int> opt;
  return;/*
  typedef TGame::Level_SplinterWorld::t_world t_world;
  t_world w;
  TGame::Level_SplinterWorld::init_world(w);
  auto mem=QapSaveToStr(w);
  cerr<<"a=["<<mem<<"]"<<endl;
  file_put_contents("out.t_world",mem);*/
}
unique_ptr<i_world> TGame_mk_world(const string&world){
  return TGame::mk_world(world);
}
#endif