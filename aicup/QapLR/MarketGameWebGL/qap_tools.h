//-------------------------------------------//
inline real RndReal(const real&min,const real&max){return min+(max-min)*(real)rand()/(real)RAND_MAX;};
inline int RndInt(const int&min,const int&max){return min+rand()%(max-min);};
//-------------------------------------------//
inline string CToS(const char&val){string tmp="";tmp.push_back(val);return tmp;};
//inline string IToS(const int&val){char c[16];_itoa_s(val,c,10);return string(c);}
inline string HToS(const int&val){char c[16];_itoa_s(val,c,16);return "0x"+string(c);}
inline string IToSex(const int&val){char c[40];sprintf_s(c,"%08i",val);int i=0;while(c[i]=='0')c[i++]='-';return string(c);}
//inline string FToS(const real&val){char c[64];if(abs(val)>1e9){_snprintf_s(c,32,32,"%e",val);}else{sprintf_s(c,"%.2f",val);}return string(c);}
inline string BToS(const bool&val){return val?"true":"false";};
inline string IToH(const int&val){char d[255]; sprintf_s(d,sizeof(d),"0x%08X",val); return string(d);}
//-------------------------------------------//
inline bool SToF(const string&str,real&val){sscanf_s(str.c_str(),"%lf",&val);return true;}
inline bool SToB(const string&str,bool&val){val=str=="true";return true;}
inline bool SToC(const string&str,char&val){if(str.size()!=1)return false;val=str[0];return true;}
inline bool HToI(const string&str,int&val){sscanf_s(str.c_str(),"0x%08X",&val); return true;}
inline string SToS(const string&s){return s;}
inline string SToSex(const string&S,const int&l){string s(l,' ');for(size_t i=0;i<S.size();i++)s[i]=S[i];return s;}
inline int SToI(const string&S){int i;sscanf_s(S.c_str(),"%i",&i);return i;};
//-------------------------------------------//
inline string GetFileName(const string&S)
{
  if(S.empty())return S; string Temp=""; int i=S.size()-1; //только не надо думать, что везде такие костыли :)
  for(;i>=0;i--)if(S[i]=='\\')goto Exit;
  return Temp;
Exit:
  for(size_t j=i+1;j<S.size();j++)Temp.push_back(S[j]); return Temp;
}
//-------------------------------------------//
inline string ChangeFileExt(const string&FN,const string&Ext)
{
  string tmp=Ext; bool flag=false;
  for(int i=FN.size()-1;i>=0;i--)
  {
    if(flag)tmp=FN[i]+tmp;
    if(FN[i]=='.')flag=true;
  }
  return tmp;
}
//-------------------------------------------//
inline string ScanParam(const string&Before,const string&Where,const string&After,int&Pos)
{
  int E=Where.find(Before, Pos);
  if(E==-1){Pos=-1; return "";}
  Pos=Where.find(Before, Pos)+Before.length();
  E=Where.find(After,Pos);
  if(E==-1){Pos=-1; return "";}
  string result; result.assign(Where,Pos,E-Pos);
  return result;
}
//-------------------------------------------//
inline string StrReplace(string text,const string&was,const string&now)
{
  for(uint i=0;i=text.find(was,i),i!=string::npos;)
  {
    text.replace(i,was.length(),now);
    i+=now.length();
  }
  return text;
}
//-------------------------------------------//
inline string GetDataTimeStr()
{
  class UG{public:static inline string X(int val,string fs){char c[40];sprintf_s(c,fs.c_str(),val);return string(c);}};
  SYSTEMTIME ST,LT;
  GetLocalTime(&LT);
  GetSystemTime(&ST);ST.wHour=LT.wHour;
  string Str;
  #define F(VAL,D)UG::X(ST.VAL,"%0"#D"i")
    Str=F(wYear,2)+"-"+F(wMonth,2)+"-"+F(wDay,2)+" "
      +F(wHour,2)+"-"+F(wMinute,2)+"-"+F(wSecond,2)+"-"+F(wMilliseconds,3);
  #undef F
  Str.resize(Str.size()-1);
  return Str;
}
//-------------------------------------------//
inline string GetFileExt(const string&FN){int e=0;string ext=ScanParam(".",FN+"\n","\n",e);return ext;}
inline string LowStr(const string&s){string str(s);for(size_t i=0;i<str.length();i++)if(isupper(str[i]))str[i]=tolower(str[i]);return str;}

class Extractor{
public:
  class Gripper{
  public:
    string before;
    string after;
    Gripper(const string&before,const string&after):before(before),after(after){}
    string grip(const string&source)const{return before+source+after;}
  };
public:
  int pos;
  string source;
  Extractor(const string&source):pos(0),source(source){}
  string extract(const Gripper&ref){
    string s=ScanParam(ref.before,source,ref.after,pos);
    if(pos>=0)pos+=s.size()+ref.after.size();
    return s;
  }
  template<class TYPE>
  void extract_all(TYPE&visitor,const Gripper&ref)
  {
    while(pos>=0){
      string s=extract(ref);
      if(pos<0)break;
      visitor.accept(s);
    }
  }
  operator bool(){return pos>=0;}
};
inline int HToI(const string&str){int val;sscanf_s(str.c_str(),"0x%08X",&val); return val;}
inline int HToI_raw(const string&str){int val;sscanf_s(str.c_str(),"%08X",&val); return val;}
inline string CToH_raw(const unsigned char&val){char d[3]; sprintf_s(d,sizeof(d),"%02X",val); return string(d);}
template<size_t COUNT> inline string SToS(const char(&lzstr)[COUNT]){static_assert(COUNT,"WTF?");return string(&lzstr[0],size_t(COUNT-1));}
inline int SToI_fast(const string&S){
  //return atoi(S.c_str());
  int i;sscanf_s(S.c_str(),"%i",&i);return i;
  //stringstream ss(S);
  //int i=0;ss>>i;
  //return i;
};