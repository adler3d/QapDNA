#pragma once

//#ifndef DEF_BUILD_MICRO
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#pragma comment(lib,"Ws2_32.lib")
//#endif

struct GWSSResponse//ответ на сканирование серверов
{
  int ver;//версия 100 => 1.0
  unsigned sid;//идентификатор сервера
  sockaddr_in sock_addr;//адрес сокета
};

struct t_mega_log_item
{
  int alloc_id;
};

static vector<t_mega_log_item>&get_megalog()
{
  static vector<t_mega_log_item> arr;
  if (!arr.empty())
  {
    return arr;
  }
  arr.reserve(1024*1024*32);
  return arr;
}

static unsigned capture_tag(const string&src,string&dst,const string&tag_op,const string&tag_cl)//return offset after read
{
  unsigned offs=src.find(tag_op);
  unsigned offe=src.find(tag_cl);
  if (offs==src.npos||offe==src.npos)return src.npos;
  dst=src.substr(offs+tag_op.size(),offe-offs-tag_op.size());
  return offe+tag_cl.size();
}

class Socket//Сокет
{
public:
  SOCKET sock;//сокет
  sockaddr_in sock_addr;//адресс сокета
  std::string ibuffer;//буффер входных данных
  std::string rbuffer;//зарезервировано для внутреннего использования(буффер для чтения из сокета)
  bool connected;//состояние "соединение установлено",для TCP-сокетов
  void*userdata;
  int reserved;//размер резервируемый в буфере
  std::string name()//строковое представление адреса сокета ip:port
  {
    char buff[16];
    return std::string(inet_ntoa(sock_addr.sin_addr))+std::string(":")+std::string(itoa(ntohs(sock_addr.sin_port),buff,10));
  }
  void Init()
  {
    connected=0;
    sock=SOCKET_ERROR;
    ZeroMemory(&sock_addr,sizeof(sock_addr));
    ibuffer.clear();
    userdata=NULL;
    ibuffer.clear();
    reserved=64*1024;//64*1024;//64K
    rbuffer.clear();
    rbuffer.reserve(reserved);
  }
  void Free()
  {
    connected=0;
    if (sock!=SOCKET_ERROR)
    {
      closesocket(sock);
      sock=SOCKET_ERROR;
    }
    ZeroMemory(&sock_addr,sizeof(sock_addr));
    ibuffer.clear();
    rbuffer.clear();
    userdata=NULL;
  }
  Socket()
  {
    Init();
  }
  ~Socket()
  {
    Free();
  }
public:
#ifdef WIN32
  typedef int t_sock_len;
#else
  typedef socklen_t t_sock_len;
#endif
public:
  int rcvbuf()
  {
    int buffsize=0;
    t_sock_len size=sizeof(buffsize);
    getsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char*)&buffsize,&size);
    return buffsize;
  }
  void rcvbuf(int newsize)
  {
    int size=sizeof(newsize);
    setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char*)&newsize,size);
  }
  int sndbuf()
  {
    int buffsize=0;
    t_sock_len size=sizeof(buffsize);
    getsockopt(sock,SOL_SOCKET,SO_SNDBUF,(char*)&buffsize,&size);
    return buffsize;
  }
  void sndbuf(int newsize)
  {
    int size=sizeof(newsize);
    setsockopt(sock,SOL_SOCKET,SO_SNDBUF,(char*)&newsize,size);
  }
  void setbuf(int newsize)
  {
    rcvbuf(newsize);
    sndbuf(newsize);
    reserved=newsize;
    rbuffer.reserve(newsize);
  }
  /************************************************************************/
  /* упаковывает source добавляя заголовок с размером                    */
  /************************************************************************/
  inline void pack(std::string&dest,const std::string&source)
  {
    int hdr=source.size();
    dest.clear();
    dest.append((char*)&hdr,sizeof(int));
    dest.append(source);
    if (reserved<hdr+4)
      reserved=hdr+4;
  }
  /************************************************************************/
  /* распаковывает из входного буфера данные dest                       */
  /************************************************************************/
  inline int unpack(std::string&dest)
  {
    int hdr=header_read();
    if (hdr==-1)return hdr;
    if (ibuffer.size()>=hdr)
    {
      dest=ibuffer.substr(0,hdr);
      ibuffer=ibuffer.substr(hdr);
    }
    else
    {
      header_write(hdr);
      hdr=-1;
    }
    return hdr;
  }
  /************************************************************************/
  /* отправляет неупакованные данные по TCP                              */
  /************************************************************************/
  inline void set_auto_flush(bool f)
  {
    BOOL flag=f;
    setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char*)&flag,sizeof(flag));
    //flag=false;
    //setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char*)&flag,sizeof(flag));
  }
  inline int send_raw(const std::string&source)
  {
    int ret=0;
    int pos=0;
    int size=source.size();
    auto*pSource=source.c_str();
    for (;;)
    {
      ret=::send(sock,&pSource[pos],size,0);
      if (ret==SOCKET_ERROR)break;
      pos+=ret;
      size-=ret;
      if (!size)break;
    }
    return ret;
  }
  /************************************************************************/
  /* отправляет упакованные данные по TCP                               */
  /************************************************************************/
  inline int send(const std::string&source)
  {
    pack(rbuffer,source);//упаковка данных заголовком
    return send_raw(rbuffer);//отправка данных
  }
  /************************************************************************/
  /* отправляет неупакованные данные по UDP(модифицирует источник)  */
  /************************************************************************/
  inline int sendto_raw(std::string&source,const sockaddr_in&to)
  {
    int ret=0;
    for (;;)
    {
      source=source.substr(ret);
      ret=::sendto(sock,source.c_str(),source.size(),0,(sockaddr*)&to,sizeof(sockaddr_in));
      if (ret==SOCKET_ERROR)break;
      if (ret==source.size())break;
    }
    return ret;
  }
  /************************************************************************/
  /* отправляет упакованные данные по UDP                               */
  /************************************************************************/
  inline int sendto(const std::string&source,const sockaddr_in&to)
  {
    pack(rbuffer,source);//упаковка данных заголовком
    return sendto_raw(rbuffer,to);//отправка данных
  }
  /************************************************************************/
  /* извлекает заголовок с размером                                     */
  /************************************************************************/
  inline int header_read()
  {
    int hdr=-1;//размер блока неопределенн
    if (ibuffer.size()>=4)
    {
      hdr=*(int*)ibuffer.data();//чтение размера блока
      if (rbuffer.capacity()<hdr+4)
      {
        reserved=hdr+4;
        rbuffer.reserve(reserved);
      }
      ibuffer=ibuffer.substr(4);
    }
    return hdr;
  }
  /************************************************************************/
  /* помещает в начало заголовок с размером                             */
  /************************************************************************/
  inline void  header_write(int hdr)
  {
    ibuffer.insert(0,(char*)&hdr,4);
  }
  /************************************************************************/
  /* получает неупакованные данные по TCP                               */
  /************************************************************************/
  inline int recv_raw()
  {
    if (rbuffer.capacity()<reserved)
      rbuffer.reserve(reserved);
    char*pbuff=const_cast<char*>(rbuffer.data());
    int ret=::recv(sock,pbuff,rbuffer.capacity(),0);
#ifdef WIN32
    if ((ret==SOCKET_ERROR)&&(WSAGetLastError()==WSAEMSGSIZE))
    {
      ret=rbuffer.capacity();
    }
    else
#endif
      if (!ret||(ret==SOCKET_ERROR))
      {
        return ret;
      }
    ibuffer.append(pbuff,ret);//присоединение прочитанных данных
    return ret;
  }
  /************************************************************************/
  /* получает неупакованные данные по TCP                               */
  /************************************************************************/
  inline int recv_raw_ex()
  {
    if (rbuffer.capacity()<reserved)
    {
      rbuffer.reserve(reserved);
    }
    char*pbuff=const_cast<char*>(rbuffer.data());
    int ret=::recv(sock,pbuff,rbuffer.capacity(),0);
#ifdef WIN32
    {
      if ((SOCKET_ERROR==ret)&&(WSAEMSGSIZE==WSAGetLastError()))
      {
        ret=rbuffer.capacity();
        ibuffer.append(pbuff,ret);
        return ret;
      }
    }
#endif
    if (!ret||(SOCKET_ERROR==ret))
    {
      ibuffer.append(pbuff,ret);
      return ret;
    }
  }
  /************************************************************************/
  /* получает неупакованные данные по UDP                               */
  /************************************************************************/
  inline int recvfrom_raw(sockaddr_in&from)
  {
    if (rbuffer.capacity()<reserved)
      rbuffer.reserve(reserved);
    char*pbuff=const_cast<char*>(rbuffer.data());
    t_sock_len fromlen=sizeof(from);
    int ret=::recvfrom(sock,pbuff,rbuffer.capacity(),0,(sockaddr*)&from,&fromlen);
#ifdef WIN32
    if ((ret==SOCKET_ERROR)&&(WSAGetLastError()==WSAEMSGSIZE))
    {
      ret=rbuffer.capacity();
    }
    else
#endif
      if (!ret||(ret==SOCKET_ERROR))
      {
        return ret;
      }
    ibuffer.append(pbuff,ret);//присоединение прочитанных данных
    return ret;
  }
  /************************************************************************/
  /* устанавливает ip в адресе сокета                                   */
  /************************************************************************/
  int setaddr(const std::string&addr)
  {
#ifdef WIN32
    ADDRINFO net_hint,*net_result=NULL;
    ZeroMemory(&net_hint,sizeof(net_hint));
    net_hint.ai_family=AF_INET;
    net_hint.ai_protocol=IPPROTO_TCP;
    auto err_code=getaddrinfo(addr.c_str(),NULL,&net_hint,&net_result);
    if (err_code==SOCKET_ERROR)return -1;
    if (!net_result)return -1;
    sock_addr=*(sockaddr_in*)net_result->ai_addr;
    freeaddrinfo(net_result);
    return 0;
#else
    HOSTENT*hst;
    //Преобразование IP -адреса. SERVERADDR - адрес сервера
    if (inet_addr(addr.c_str())!=INADDR_NONE)
      sock_addr.sin_addr.s_addr=inet_addr(addr.c_str());
    else//Осуществляем попытку получения IP-адреса по доменному имени
      if (hst=gethostbyname(addr.c_str()))
        ((unsigned long*)&sock_addr.sin_addr)[0]=
          ((unsigned long**)hst->h_addr_list)[0][0];
      else
      {
        return 1;
      }
    return 0;
#endif
  }
  /************************************************************************/
  /* устанавливает неблокирующий режим                                    */
  /************************************************************************/
  int setnoblock(bool on)
  {
#ifdef WIN32
    u_long cmd=on;
    if (ioctlsocket(sock,FIONBIO,&cmd))return -1;
    return 0;
#else
    int f;
    if ((f=fcntl(sock,F_GETFL,0))==-1)return -1;
    if (on)
      f|=O_NONBLOCK;
    else
      f&=~O_NONBLOCK;
    if (fcntl(sock,F_SETFL,f)==-1)return -1;
    return 0;
#endif
  }
};

class ServerCallback
{
public:
  /****************************************************************************************************/
  /*запрещено вызывать: Free,Stop,recv_one(client),recv_all(),kick_one(client),kick_all()          */
  /****************************************************************************************************/
  virtual void OnRecvPack(Socket*client,std::string&data) {}//приём данных от клиента
  virtual void OnAddClient(Socket*client) {}//подключение нового клиента
  /****************************************************************************************************/
  /*запрещено вызывать: всё                   */
  /****************************************************************************************************/
  virtual void OnDeleteClient(Socket*client) {}//отключение клиента
};
#if(0)
class GatewayServer
{
public:
  GatewayServer()
  {
    Init();
  }
  ~GatewayServer()
  {
    Free();
  }
  void Init()
  {
    callback=NULL;
    listener=NULL;
    detector=NULL;
    runned=0;
    port=0;
    FD_ZERO(&fdsClients);
    userdata=NULL;
    forcebind=0;
    sid=((unsigned)rand())+100;
    raw=false;
  }
  void Free()
  {
    stop();
    callback=NULL;
    userdata=NULL;
  }
public:
  void start_tcp()
  {
    if (!port||runned)return;

    //TCP socket
    if (listener)
    {
      delete listener;
      listener=NULL;
    }
    listener=new Socket;
    listener->sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (listener->sock==INVALID_SOCKET)return;
    listener->sock_addr.sin_family=AF_INET;
    listener->sock_addr.sin_port=htons(port);//не забываем о сетевом порядке!!!
    listener->sock_addr.sin_addr.s_addr=0;
    if (bind(listener->sock,(sockaddr*)&listener->sock_addr,sizeof(listener->sock_addr))==SOCKET_ERROR)
    {
#ifdef WIN32
      if (!forcebind)
#endif
        return;
#ifdef WIN32
      listener->sock_addr.sin_port=0;//любой
      if (bind(listener->sock,(sockaddr*)&listener->sock_addr,sizeof(listener->sock_addr))==SOCKET_ERROR)return;
      char info[256];
      sockaddr_in*local=(sockaddr_in*)(info+sizeof(CSADDR_INFO));
      sockaddr_in*remote=(sockaddr_in*)(info+sizeof(CSADDR_INFO)+sizeof(sockaddr_in));
      ZeroMemory(info,sizeof(info));
      ((CSADDR_INFO*)info)->LocalAddr.lpSockaddr=(sockaddr*)local;
      ((CSADDR_INFO*)info)->LocalAddr.iSockaddrLength=sizeof(*local);
      ((CSADDR_INFO*)info)->RemoteAddr.lpSockaddr=(sockaddr*)remote;
      ((CSADDR_INFO*)info)->RemoteAddr.iSockaddrLength=sizeof(*remote);
      int size=sizeof(info);
      int ret=getsockopt(listener->sock,SOL_SOCKET,SO_BSP_STATE,(char*)&info,&size);
      if (ret==SOCKET_ERROR)return;
      listener->sock_addr.sin_port=((sockaddr_in*)((CSADDR_INFO*)info)->LocalAddr.lpSockaddr)->sin_port;
#endif
    }
    if (listen(listener->sock,1)==SOCKET_ERROR)return;
    listener->connected=1;
    runned=1;
  }
  void start_udp()
  {
    //UDP socket
    if (detector)
    {
      delete detector;
      detector=NULL;
    }
    detector=new Socket;
    detector->sock=socket(AF_INET,SOCK_DGRAM,0);
    detector->sock_addr.sin_family=AF_INET;
    detector->sock_addr.sin_port=htons(port);//не забываем о сетевом порядке!!!
    detector->sock_addr.sin_addr.s_addr=htonl(INADDR_ANY);//означает любой адрес
    u_long brcst=1;//Set socket to allow broadcast
    if (setsockopt(detector->sock,SOL_SOCKET,SO_BROADCAST,(char*)&brcst,sizeof(brcst))==SOCKET_ERROR)return;
    u_long yes=1;//переиспользование адреса сокета
    if (setsockopt(detector->sock,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(yes))==SOCKET_ERROR)return;
    if (bind(detector->sock,(sockaddr*)&detector->sock_addr,sizeof(detector->sock_addr))==SOCKET_ERROR)return;
  }
  void start()
  {
    QapDebugMsg("deprecate");
    start_tcp();
    start_udp();
  }
  void stop_tcp()
  {
    if (listener)
    {
      delete listener;  //TCP socket
      listener=nullptr;
    }
    kick_all();
    runned=0;
  }
  void stop_udp()
  {
    if (detector)
    {
      delete detector;  //UDP socket
      detector=nullptr;
    }
  }
  void stop()
  {
    stop_tcp();
    stop_udp();
  }
  Socket*getClientBySock(SOCKET sock)
  {
    for (int i=0;i<clients.size();i++)
    {
      if (clients[i]->sock==sock)
      {
        return clients[i];
      }
    }
    return NULL;
  }
public:
  void update_tcp()
  {
    accept_all();
    recv_all();
  }
public:
  void accept_all()//принять входящих(редко)
  {
    if (!runned||!listener||!listener->connected)return;
    fd_set fds_src;
    FD_ZERO(&fds_src);
    FD_SET(listener->sock,&fds_src);
    timeval tval={0,0};
    fd_set fds_r=fds_src,fds_e=fds_src;
    int ret=select(listener->sock+1,&fds_r,NULL,&fds_e,&tval);
    if (ret==SOCKET_ERROR)return;//произошла ошибка select
    if (!ret)return;//нет данных
    if (FD_ISSET(listener->sock,&fds_r))
    {
      /*
      *If listen has been called and a connection is pending,accept will succeed.
      *Data is available for reading(includes OOB data if SO_OOBINLINE is enabled).
      *Connection has been closed/reset/terminated.
      */
      Socket  *incom=new Socket;
#ifdef WIN32
      int addr_size=sizeof(incom->sock_addr);
#else
      socklen_t addr_size=sizeof(incom->sock_addr);
#endif
      incom->sock=accept(listener->sock,(sockaddr*)&incom->sock_addr,&addr_size);
      if (incom->sock==SOCKET_ERROR)
      {
        delete incom;
        incom=NULL;
        delete listener;
        listener=NULL;
        return;
      }
      incom->connected=1;
      highclient=incom->sock;
      clients.push_back(incom);
      FD_SET(incom->sock,&fdsClients);
      if (callback)callback->OnAddClient(incom);
    }
    if (FD_ISSET(listener->sock,&fds_e))
    {
      /*
      *If processing a connect call(nonblocking),connection attempt failed.
      *OOB data is available for reading(only if SO_OOBINLINE is disabled).
      */
      delete listener;
      listener=NULL;
      return;
    }
  }
public:
  void send_one(const std::string&message,Socket*client)//отправка данных клиенту
  {
    if (!runned)return;
    if (!client||!client->connected)return;
    if (raw)
    {
      if (client->send_raw(message)!=SOCKET_ERROR)return;
    }
    if (!raw)
    {
      if (client->send(message)!=SOCKET_ERROR)return;
    }
    kick_one(client);
  }
  void send_all(const std::string&message)//отправка данных всем клиентам
  {
    if (!runned)return;
    string msg;
    for (int i=0;i<clients.size();++i)
    {
      Socket *client=clients[i];
      if (!client||!client->connected)continue;
      if (raw)
      {
        msg=message;
        if (client->send_raw(msg)==SOCKET_ERROR)
        {
          kick_one(client);
          --i;
        }
        return;
      }
      if (!raw)
      {
        if (client->send(message)==SOCKET_ERROR)
        {
          kick_one(client);
          --i;
        }
        return;
      }
      int end_loop=1;
    }
    int gg=1;
  }
  void recv_one(Socket*client)//при наличии данных,получение их от клиента
  {
    if (!runned)return;
    if (!client||!client->connected)return;
    fd_set fds_src;
    FD_ZERO(&fds_src);
    FD_SET(client->sock,&fds_src);
    timeval tval={0,0};
    int ret=0;
    for (;;)
    {
      fd_set fds_r=fds_src,fds_e=fds_src;
      int ret=select(client->sock+1,&fds_r,NULL,&fds_e,&tval);
      if (!ret||(ret==SOCKET_ERROR))return;//нет данных или произошла ошибка select
      if (FD_ISSET(client->sock,&fds_r))
      {
        /*
        *If listen has been called and a connection is pending,accept will succeed.
        *Data is available for reading(includes OOB data if SO_OOBINLINE is enabled).
        *Connection has been closed/reset/terminated.
        */
        if (client->recv_raw()<1)
        {
          kick_one(client);
          return;
        }

        string msg;
        if (raw)
        {
          msg=client->ibuffer;
          client->ibuffer.clear();
          if (callback)callback->OnRecvPack(client,msg);//передача данных на обработку
        }
        else
        {
          while (client->unpack(msg)!=-1)
          {
            if (callback)callback->OnRecvPack(client,msg);//передача данных на обработку
          }
        }
      }
      if (FD_ISSET(client->sock,&fds_e))
      {
        /*
        *If processing a connect call(nonblocking),connection attempt failed.
        *OOB data is available for reading(only if SO_OOBINLINE is disabled).
        */
        kick_one(client);
        return;
      }
    }
  }
  void recv_all()//при наличии данных,получение их от всех клиентов
  {
    if (!runned)return;
    timeval tval={0,0};
    int ret=0;
    for (;;)
    {
      fd_set fds_r=fdsClients,fds_e=fdsClients;
      int ret=select(highclient+1,&fds_r,NULL,&fds_e,&tval);
      if (!ret||(ret==SOCKET_ERROR))return;//нет данных или произошла ошибка select
      for (int cid=0;cid<clients.size();cid++)
      {
        if (clients[cid]&&FD_ISSET(clients[cid]->sock,&fds_r))
        {
          /*
          *If listen has been called and a connection is pending,accept will succeed.
          *Data is available for reading(includes OOB data if SO_OOBINLINE is enabled).
          *Connection has been closed/reset/terminated.
          */
          Socket*client=clients[cid];
          if (!client||!client->connected)continue;
          if (client->recv_raw()<1)
          {
            kick_one(client);
            return;
          }
          string msg;
          if (raw)
          {
            msg=client->ibuffer;
            client->ibuffer.clear();
            if (callback)callback->OnRecvPack(client,msg);//передача данных на обработку
          }
          if (!raw)
          {
            for (;;)
            {
              bool flag=client->unpack(msg)!=-1;
              if (!flag)break;
              if (!callback)continue;
              callback->OnRecvPack(client,msg);//передача данных на обработку
            }
          }
        }
      }
      for (int cid=0;cid<clients.size();cid++)
      {
        bool flag=clients[cid]&&FD_ISSET(clients[cid]->sock,&fds_e);
        if (!flag)continue;
        /*
        *If processing a connect call(nonblocking),connection attempt failed.
        *OOB data is available for reading(only if SO_OOBINLINE is disabled).
        */
        kick_one(clients[cid]);
        --cid;
      }
    }
  }
  void kick_one(Socket*client)//отключить клиента
  {
    for (int i=0;i<clients.size();i++)
    {
      if (clients[i]!=client)continue;
      Socket*client=clients[i];
      if (callback)callback->OnDeleteClient(client);
      FD_CLR(client->sock,&fdsClients);
      delete clients[i];
      clients[i]=NULL;
      int size=clients.size()-1;
      clients[i]=clients[size];
      clients.resize(size);
      break;
    }
  }
  void kick_all()//отключить всех клиентов
  {
    while (clients.size())
    {
      Socket*client=clients.back();
      if (callback)callback->OnDeleteClient(client);
      delete client;
      client=NULL;
      clients.back()=NULL;
      clients.pop_back();
    }
    FD_ZERO(&fdsClients);
  }
  void update_lan()//отвечает всем клиентам на запросы поиска сервера
  {
    if (!runned||!detector||(detector->sock==SOCKET_ERROR))return;
    fd_set fds_src;
    FD_ZERO(&fds_src);
    FD_SET(detector->sock,&fds_src);
    timeval tval={0,0};
    int ret=0;
    sockaddr_in sender_addr;//адресс отправителя
    int addr_size=sizeof(sender_addr);
    for (;;)
    {
      fd_set fds_r=fds_src,fds_e=fds_src;
      int ret=select(detector->sock+1,&fds_r,NULL,&fds_e,&tval);
      if (!ret||(ret==SOCKET_ERROR))return;//нет данных или произошла ошибка select
      if (FD_ISSET(detector->sock,&fds_r))
      {
        /*
        *If listen has been called and a connection is pending,accept will succeed.
        *Data is available for reading(includes OOB data if SO_OOBINLINE is enabled).
        *Connection has been closed/reset/terminated.
        */
        if (detector->recvfrom_raw(sender_addr)<1)
        {
          delete detector;
          detector=NULL;
          return;
        }
        string msg;
        while (detector->unpack(msg)!=-1)
        {
          //Отправка ответа ищущему клиенту
          if (msg!="scanning servers")continue;
          GWSSResponse resp;
          resp.ver=100;
          resp.sid=sid;
          resp.sock_addr=listener->sock_addr;
          detector->sendto(string((char*)&resp,sizeof(resp)),sender_addr);
        }
      }
      if (FD_ISSET(detector->sock,&fds_e))
      {
        delete detector;
        detector=NULL;
        return;
      }
    }
  }
public:
  Socket* operator[](unsigned num)
  {
    return clients[num];
  }
  unsigned count()
  {
    return clients.size();
  }
  operator bool()
  {
    return runned;
  }
public:
  Socket    *listener;//TCP-сокет для приёма входящих подключений
  Socket    *detector;//UDP-сокет для обнаружения себя клиентами
  std::vector< Socket*> clients;//список клиентов
  ServerCallback   *callback;//обработчик событий сервера
  bool     runned;//состояние "работает"
  unsigned    port;//порт для приёма входящих подключений
  void     *userdata;
  bool     forcebind;//в случае занятого порта: использовать случайных порт
  unsigned    sid;//иденитификатор
  bool     raw;//используются сырые(не упакованные)данные для обмена информацией
private:
  fd_set    fdsClients;//вспомогательная структура с массивом сокетов клиентов
  SOCKET    highclient;
};
#endif
class ClientCallback
{
public:
  /************************************************************************/
  /*запрещено вызывать: connect_begin(),recv_from_server()                */
  /************************************************************************/
  virtual void OnRecvPack(std::string&data) {}//приём данных от сервера
  virtual void OnDisconnect() {}//отключение от сервера
};

struct GCServer
{
  unsigned sid;//идентиф.
  Socket s;//сокет
  struct State
  {
bool   local :
    1;//в локальной сети
bool   net :
    1;//в сети интернет
unsigned char res0 :
    6;
    unsigned char res1;
    unsigned short res2;
  }state;
  enum Place
  {
    //AND  if(state& mask)
    AND_U,//undefined(никто)
    AND_L,//установлен local
    AND_N,//установлен net
    AND_LN,//только те у кого установлены(L,N)
    //OR  if(state | mask)
    OR_U=0x100,//те у кого что-нибудь установлено
    OR_L,//все
    OR_N,//все
    OR_LN,//все
    //XOR  if(state ^ mask)
    XOR_U=0x200,//те у кого что-нибудь установлено
    XOR_L,//только те у кого не(L,!N)
    XOR_N,//только те у кого не(!L,N)
    XOR_LN,//только те у кого не(L,N)
    //EQUAL if(!(state ^ mask))
    EQ_U=0x300,//только те у кого(!L,!N)
    EQ_L,//только те у кого(L,!N)
    EQ_N,//только те у кого(!L,N)
    EQ_LN//только те у кого(L,N)
  };
public:
  GCServer()
  {
    Init();
  }
  ~GCServer()
  {
    Free();
  }
  void Init()
  {
    sid=0;
    state.local=0;
    state.net=0;
    state.res0=0;
    state.res1=0;
    state.res2=0;
  }
  void Free()
  {
    sid=0;
    state.local=0;
    state.net=0;
  }
public:
  struct t_mode_funcs
  {
    static bool func_AND_U(unsigned left,unsigned right)
    {
      return left&right;
    }
    static bool func_OR_U(unsigned left,unsigned right)
    {
      return left|right;
    }
    static bool func_XOR_U(unsigned left,unsigned right)
    {
      return left^right;
    }
    static bool func_EQ_U(unsigned left,unsigned right)
    {
      return !(left^right);
    }
  };
  static bool use_mode(unsigned mode,unsigned left,unsigned right)
  {
#define F(MODE)if(GCServer::MODE==mode)return t_mode_funcs::func_##MODE(left,right)
    F(AND_U);
    F(OR_U);
    F(XOR_U);
    F(EQ_U);
#undef F
    return false;
  }
};

#ifdef WIN32
class WinSockUtils
{
public:
  DWORD dwRes;//Результат старта WinSock
public:
  WinSockUtils()
  {
    dwRes=0;
    WSADATA wsaData;//Данные для WinSock
    WORD wVersion=MAKEWORD(2,2);
    dwRes=WSAStartup(wVersion,&wsaData);
    srand(time(NULL));
  }
  ~WinSockUtils()
  {
    if (dwRes)return;
    WSACleanup();
  }
};
static WinSockUtils g_gateway_h_var_win_sock_utils;
#endif

class GatewayClient//
{
public:
  GatewayClient()
  {
    Init();
  }
  ~GatewayClient()
  {
    Free();
  }
  void Init()
  {
    callback=NULL;
    runned=0;
    scanning_lan=0;
    port=0;
    server=NULL;
    sender=NULL;
    userdata=NULL;
  }
  void Free()
  {
    stop();
    callback=NULL;
    userdata=NULL;
  }
public:
  void start()//создание сокетов
  {
    if (!port||runned)return;
    //TCP socket
    if (server)
    {
      delete server;
      server=NULL;
    }
    server=new GCServer;
    //UDP socket
    if (sender)
    {
      delete sender;
      sender=NULL;
    }
    sender=new Socket;
    sender->sock=socket(AF_INET,SOCK_DGRAM,0);
    sender->sock_addr.sin_family=AF_INET;
    sender->sock_addr.sin_port=htons(port);//не забываем о сетевом порядке!!!
    sender->sock_addr.sin_addr.s_addr=htonl(INADDR_BROADCAST);//означает широковещательный адрес
    u_long brcst=1;//Set socket to allow broadcast
    if (setsockopt(sender->sock,SOL_SOCKET,SO_BROADCAST,(char*)&brcst,sizeof(brcst))==SOCKET_ERROR)return;
    runned=1;
  }
  void stop()//удаление сокетов
  {
    if (!runned)return;
    scan_lan_end();
    clear_servers();
    connect_end();
    if (server)
    {
      delete server;
      server=NULL;
    }
    if (sender)
    {
      delete sender;
      sender=NULL;
    }
    runned=0;
  }
  void clear_servers(unsigned who=GCServer::OR_LN)//очистка серверов
  {
    unsigned mode=who&0x300;
    unsigned mask=who&0x3;
    for (int skeep=0,i=servers.size()-1;servers.size()-skeep>0;i=servers.size()-skeep-1)
    {
      auto res=GCServer::use_mode(mode,*(unsigned*)&servers[i]->state,mask);
      if (!res)
      {
        skeep++;
        continue;
      }
      delete servers[i];
      servers[i]=NULL;
      int size=servers.size()-1;
      servers[i]=servers[size];
      servers.resize(size);
    }
  }
public:
  void scan_lan_begin()
  {
    if (!runned||!sender||scanning_lan)return;
    scan_lan_result();//выборка старых необработанных ответов от серверов
    scan_lan_end();//очистка старых необработанных данных
    scanning_lan=1;
    string msg="scanning servers";//запрос по широковещательному адресу,на поиск серверов
    if (sender->sendto(msg,sender->sock_addr)==SOCKET_ERROR)
    {
      delete sender;
      sender=NULL;
      return;
    }
  }
  void scan_lan_result()
  {
    if (!runned||!sender)return;
    fd_set fds_src;
    FD_ZERO(&fds_src);
    FD_SET(sender->sock,&fds_src);
    timeval tval={0,0};
    int ret=0;
    sockaddr_in detector_addr;//адресс отправителя
    int addr_size=sizeof(detector_addr);
    for (;;)
    {
      fd_set fds_r=fds_src,fds_e=fds_src;
      int ret=::select(sender->sock+1,&fds_r,NULL,&fds_e,&tval);
      if (!ret||(ret==SOCKET_ERROR))
      {
        if (!scanning_lan)
        {
          //выбросить результаты
          sender->ibuffer.clear();
        }
        return;//нет данных или произошла ошибка select
      }
      if (FD_ISSET(sender->sock,&fds_r))
      {
        if (sender->recvfrom_raw(detector_addr)<1)
        {
          delete sender;
          sender=NULL;
          return;
        }
        if (!scanning_lan)
        {
          continue;
        }
        int hdr=-1;//размер блока неопределенн
        string msg;
        while ((hdr=sender->unpack(msg))!=-1)
        {
          if (hdr<sizeof(GWSSResponse))continue;
          GWSSResponse resp=*(GWSSResponse*)msg.data();
          if (resp.ver!=100)
          {
            QapDebugMsg("wrong version"+IToS(resp.ver));
            continue;
          }
          GCServer*server=new GCServer;
          server->state.local=1;
          server->sid=resp.sid;
          server->s.sock_addr=resp.sock_addr;
          server->s.sock_addr.sin_addr=detector_addr.sin_addr;
          for (int i=0;i<servers.size();i++)
          {
            if (servers[i]->sid!=server->sid)continue;
            servers[i]->state.local=1;
            servers[i]->s.sock_addr=server->s.sock_addr;
            delete server;
            server=NULL;
            break;
          }
          if (server)servers.push_back(server);
        }
      }
      if (FD_ISSET(sender->sock,&fds_e))
      {
        delete sender;
        sender=NULL;
        return;
      }
    }
  }
  void scan_lan_end()
  {
    scanning_lan=0;
  }
public:
  void select(int num)//из servers,servers[num],вызывает connect_end
  {
    if (!runned||(num<0)||(num>=servers.size()))return;
    connect_end();
    if (server)
    {
      delete server;
      server=NULL;
    }
    server=new GCServer;
    *server=*servers[num];
    server->s.ibuffer.clear();
    server->s.sock=SOCKET_ERROR;
    server->s.connected=0;
  }
  void connect_begin()//client.sock set !SOCKET_ERROR
  {
    if (!runned||!server||server->s.connected||(server->s.sock!=SOCKET_ERROR))return;
    server->s.sock=socket(AF_INET,SOCK_STREAM,0);
    if (server->s.sock==INVALID_SOCKET)return;
    if (server->s.setnoblock(1))return;
    connect(server->s.sock,(sockaddr*)&server->s.sock_addr,sizeof(server->s.sock_addr));//пытаемся установить соединение
    if (server->s.setnoblock(0))return;
  }
  bool connect_result()
  {
    if (!runned||!server)return false;
    fd_set fds_src;
    FD_ZERO(&fds_src);
    FD_SET(server->s.sock,&fds_src);
    timeval tval={0,0};
    int ret=0;
    for (;;)
    {
      fd_set fds_w=fds_src,fds_e=fds_src;
      int ret=::select(server->s.sock+1,NULL,&fds_w,&fds_e,&tval);
      if (!ret||(ret==SOCKET_ERROR))return false;
      if (FD_ISSET(server->s.sock,&fds_w))
      {
        return server->s.connected=1;
      }
      if (FD_ISSET(server->s.sock,&fds_e))
      {
        if (server)
        {
          delete server;
          server=NULL;
        }
        return false;
      }
    }
    return false;
  }
  void connect_end()//client.sock set SOCKET_ERROR
  {
    bool flag=server&&server->s.connected;
    if (!flag)return;
    server->s.connected=0;
    closesocket(server->s.sock);
    server->s.sock=SOCKET_ERROR;
    if (callback)callback->OnDisconnect();
  }
public:
  void send_to_server(const std::string&message)
  {
    QapDebugMsg("2014.06.14\n No way.\n use recv_from_server_raw and send_to_server_raw");
    return;
    QapDebugMsg("2014.11.07\n Implementation was removed.");
  }
  void recv_from_server()
  {
    QapDebugMsg("2014.06.14\n No way.\n use recv_from_server_raw and send_to_server_raw");
    return;
    QapDebugMsg("2014.11.07\n Implementation was removed.");
  }
public:
  bool connecting()
  {
    return runned&&server&&!server->s.connected&&(server->s.sock!=SOCKET_ERROR);
  }
  bool connected()
  {
    return runned&&server&&server->s.connected;
  }
public:
  GCServer*operator[](unsigned num)
  {
    return servers[num];
  }
  unsigned count()
  {
    return servers.size();
  }
  operator bool()
  {
    return runned;
  }
public:
  GCServer*server;//активный сервер
  Socket*sender;//UDP-сокет для широковещательного опроса серверов при их поиске
  std::vector<GCServer*> servers;//найденые серверы
  ClientCallback*callback;
  bool runned;
  bool scanning_lan;
  unsigned port;
  void*userdata;
public:
  void send_to_server_raw(const std::string&message)
  {
    if (!runned||!server||!server->s.connected)return;
    if (server->s.send_raw(message)==SOCKET_ERROR)
    {
      connect_end();
    }
  }
  void recv_from_server_raw()
  {
    if (!runned||!server||!server->s.connected)return;
    fd_set fds_src;
    FD_ZERO(&fds_src);
    FD_SET(server->s.sock,&fds_src);
    timeval tval={0,0};
    int ret=0;
    for (;;)
    {
      fd_set fds_r=fds_src;
      fd_set fds_e=fds_src;
      int ret=::select(server->s.sock+1,&fds_r,NULL,&fds_e,&tval);
      if (!ret||(ret==SOCKET_ERROR))return;//нет данных или произошла ошибка select
      if (FD_ISSET(server->s.sock,&fds_r))
      {
        if (server->s.recv_raw()<1)
        {
          connect_end();
          return;
        }
        auto&msg=server->s.ibuffer;
        if (callback)callback->OnRecvPack(msg);
        msg.clear();
      }
      if (FD_ISSET(server->s.sock,&fds_e))
      {
        connect_end();
        return;
      }
    }
  }
};