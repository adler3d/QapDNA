#include "Gateway.h"
#include "qap_tools.h"
#define ADD_EVENT(MSG)
class DownLoader{
  Socket shost;
  bool active;
  // запрещено копировать
  DownLoader(const DownLoader& rhs):data(webhost){}
  DownLoader& operator=(const DownLoader& rhs){return *this;}
public:
  string &data;
  string webhost; // myserver.ru
  string webpath; // /inbox/index.php?id=1&uid=234#head
  string cookie;  // user_id=10
  int port=80;
  DownLoader():shost(),active(0),data(shost.ibuffer){}
  DownLoader(const string&webhost, const string&webpath,const string&cookie):
    shost(),active(0),data(shost.ibuffer),webhost(webhost),webpath(webpath),cookie(cookie){}
  bool start(){
    shost.Init();
    ADD_EVENT("start");Sleep(delay);
    if(shost.setaddr(webhost))return 0;
    shost.sock_addr.sin_port = htons(port);
    shost.sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(shost.sock == INVALID_SOCKET)return 0;
    if(shost.setnoblock(1))return 0;
    connect(shost.sock, (sockaddr *)&shost.sock_addr, sizeof(shost.sock_addr)); // пытаемся установить соединение
    if(shost.setnoblock(0))return 0;
    active = 1;
    return true;
  }
  static const int delay=0;
  bool update(){
    if (!active) return active;

    fd_set fds_src;
    FD_ZERO(&fds_src);
    FD_SET(shost.sock,&fds_src);
    fd_set fds_r=fds_src, fds_w=fds_src, fds_e=fds_src;
    if(shost.connected)FD_ZERO(&fds_w);
    timeval tv={0,0};
    int ret = ::select(shost.sock+1,&fds_r,&fds_w,&fds_e,&tv);
    if(ret){ // есть события
      if(FD_ISSET(shost.sock,&fds_r)){ // пришли данные || соединение закрыто
        int ret=shost.recv_raw();
        if(ret<1){ // соединение закрыто, данные переданы
          active = 0;
          ADD_EVENT("close");Sleep(delay);
          return active;
        }
      }
      if(FD_ISSET(shost.sock,&fds_w)){ // соединение установлено
      //if(0){
        shost.connected=1;
        string req(
          "GET "+webpath+" HTTP/1.1\r\n"
          "Host: "+webhost+"\r\n"
          "User-Agent: QapCpp2Net v0.0\r\n"
          "Accept: text/html;q=0.9,*/*;q=0.8\r\n"
          "Accept-Language: ru-ru,ru;q=0.8,en-us;q=0.5,en;q=0.3\r\n"
          //"Accept-Encoding: gzip, deflate\r\n"
          "Accept-Charset: windows-1251,utf-8;q=0.7,*;q=0.7\r\n"
          +(cookie.empty()?"":"Cookie: "+cookie+"\r\n")+
          "Connection: close\r\n"
          "Cache-Control: max-age=0\r\n\r\n"
        );
        if(shost.send_raw(req) == SOCKET_ERROR){// ошибка передачи, повтор подключения
          active = 0;
          ADD_EVENT("send error");Sleep(delay);
          return active;
        }else if(0){
          shutdown(shost.sock,SD_SEND);
          ADD_EVENT("shutdown");Sleep(delay);
        }
      }else{
        /*string KeepAlive("Connection: Keep-Alive\n\n");
        ADD_EVENT("keep-alive");Sleep(delay);
        if(shost.send_raw(KeepAlive) == SOCKET_ERROR){// ошибка передачи, повтор подключения
          active = 0;
          ADD_EVENT("fail");Sleep(delay);
          return active;
        }else if(0){
          shutdown(shost.sock,SD_SEND);
          ADD_EVENT("shutdown");Sleep(delay);
        }*/
      };
      if(FD_ISSET(shost.sock,&fds_e)){ // соединение прервано, повтор подключения
        active = 0;
        ADD_EVENT("fail");Sleep(delay);
        return active;
      }
    }
    return 1;
  }
  static bool is_chunked(const string&header)
  {
    Extractor::Gripper tag_te("\n","chunked");
    string low_header=LowStr(header);
    Extractor e(low_header);
    string TransferEncoding=e.extract(tag_te);
    string te=LowStr(" Transfer-Encoding : ");
    string line_te=StrReplace(te," ","");
    string line=StrReplace(TransferEncoding," ","");
    return line==te;
  }
  static string GetContent(const string&data,bool withoutheader=false)
  {
    const Extractor::Gripper tag_http("HTTP/1.1 ","\r\n\r\n");
    const Extractor::Gripper tag_te("transfer-encoding: ","\r\n");
    const Extractor::Gripper tag_line("\r\n","\r\n");
    Extractor e(data);
    string http=e.extract(tag_http);
    if(http.empty())return data;
    http+="\r\n\r\n";
    string http_lowstr=LowStr(http);
    string te=Extractor(http_lowstr).extract(tag_te);
    if(LowStr(te)!="chunked"){
      auto arr=split(data,"\r\n\r\n");
      QapPopFront(arr);
      return join(arr,"\r\n\r\n");
    }
    e.pos-=tag_line.before.size();
    string result=withoutheader?"":tag_http.grip(Extractor(data).extract(tag_http));
    while(e){
      string bytes=e.extract(tag_line);
      if(bytes.empty())break;
      int count=HToI("0x"+bytes);
      result.append(data,e.pos,count);
      e.pos+=count;
      if(!count)break;
    }
    return result;
  }
  void stop(){
    ADD_EVENT("stop");Sleep(delay);
    shost.Free();
  }
};
#undef ADD_EVENTl