#include "netapi.h"
#include "FromQapEng.h"
/*

*/
using namespace std;
string local_main_ip_port="127.0.0.1:31456";
//vector<string> split(...){return {};}
//#define QapAssert(...)
string generate_token(string coder_name,string timestamp) {
  return "token:"+to_string((rand()<<16)+rand())+coder_name+timestamp;
  //return sha256( random_bytes(32) + timestamp + coder_name );
}
struct t_client{
  struct t_input{
    struct t_visible_part_of_world{};
    struct t_allowed_cmds{};
    t_visible_part_of_world vpow;
    t_allowed_cmds cmds;
    string status="ok";
  };
  struct t_ai{
    struct t_cmd{};
    t_cmd decide_next_action(t_input::t_visible_part_of_world w,t_input::t_allowed_cmds cmds){
      return {};
    };
  };
  template<class TYPE>TYPE parse(string s){return {};}
  template<class TYPE>string serialize(TYPE&&ref){return "nope";}
  string main_ip_port=1?local_main_ip_port:"82.208.124.191:31456";
  t_ai ai;
  t_net_api api;
  void worker(string rw){
    string line;
    while(api.readline_from_socket(rw,line)){
      auto inp=parse<t_input>(line);
      if(inp.status!="ok")break;
      auto cmd=ai.decide_next_action(inp.vpow,inp.cmds);
      string serialized=serialize(cmd);
      api.write_to_socket(rw,serialized+"\n");
    }
    api.close_socket(rw);
  }
  int max_games=10;
  string token="default";
  string coder="unk";
  string email="email@example.com";
  int main(){
    load_config();
    if(token=="default"){
      api.write_to_socket(main_ip_port,"hi_i_default_and_idk_me_token,"+coder+","+to_string(max_games)+","+email+"\n");
      string resp;
      if(!api.readline_from_socket(main_ip_port,resp))return -1;
      if(resp=="u_banned")return -2;
      if(resp=="u_already_say_this")return -3;
      token=resp;
      save_config();
    }else{
      api.write_to_socket(main_ip_port,"hi_i_have_token,"+token+","+coder+","+to_string(max_games));
    }
    for(;;){
      string line;
      if(!api.readline_from_socket(main_ip_port,line))return 0;
      if(line=="ok")continue;
      if(line=="fail")return -4;
      if(line=="req_limit_reached")return -5;
      auto v=split(line,",");
      if(v[0]=="game_aborted")continue;
      auto sock=line;
      std::thread t([&,sock](){worker(sock);});
      t.detach();
    }
    return 0;
  }
  string config_fn="t_client.cfg";
  void save_config(){
    vector<string> arr;
    arr.push_back(to_string(max_games));
    arr.push_back(token);
    arr.push_back(coder);
    arr.push_back(email);
    file_put_contents(config_fn,join(arr,","));
  }
  void load_config(){
    auto s=file_get_contents(config_fn);
    auto arr=split(s,",");
    if(arr.size()!=4)return;
    max_games=std::stoi(arr[0]);
    token=arr[1];
    coder=arr[2];
    email=arr[3];
  }
};
struct t_main{
  struct t_coder{
    string coder;
    string token="def";
    string email;
    int max_games=10;
    string ip;
  };
  int main(){
    t_server_api server(31465);
    map<string,bool> ip2ban;
    map<string,t_coder> coder2acc;
    map<int,string> cid2ip;
    map<string,int> ip2n;
    server.onClientConnected=[&](int client_id,socket_t socket,const std::string&ip){
      std::cout << "[" << client_id << "] connected from IP " << ip << "\n";
      if(ip2ban[ip]){
        std::string greeting="u_banned\n";
        send(socket,greeting.c_str(),static_cast<int>(greeting.size()),0);
        server.disconnect_clients_by_ip(ip);
        return;
      }
      cid2ip[client_id]=ip;
    };

    server.onClientDisconnected = [&](int client_id) {
      std::cout << "[" << client_id << "] disconnected\n";
    };
    const int max_games=16;
    server.onClientData=[&](int client_id,const std::string& data,std::function<void(const std::string&)> send){
      std::cout << "[" << client_id << "] received: " << data;
      auto v=split(data,",");
      if(v[0]=="hi_i_default_and_idk_me_token"){
        #define NO_WAY(ANSWER){send(string(ANSWER)+"\n");server.disconnect_clients_by_ip(cid2ip[client_id]);return;}
        if(v.size()!=4){NO_WAY("wrong_number_of_params");}
        if(v[1].empty()){NO_WAY("your_name_is_empty");}
        if(v[1].size()>42){NO_WAY("your_name_is_too_long");}
        if(v[3].empty()){NO_WAY("your_email_is_empty");}
        if(v[3].size()>42){NO_WAY("your_email_is_too_long");}
        //(имя_кодера,сколько_одновременно_игр_он_тянет,email)
        auto&acc=coder2acc[v[1]];
        if(!acc.coder.empty()){NO_WAY("coder_name_already_used");}
        auto&ip=cid2ip[client_id];
        if(ip2n[ip]){NO_WAY("u_already_say_this");}
        ip2n[ip]++;
        acc.max_games=std::max(1,std::min(max_games,std::stoi(v[2])));
        acc.email=v[3];
        acc.ip=ip;
        acc.token=generate_token(acc.coder,"2025.08.16 22:05:03.625"+acc.ip);
        return;
      }else if(v[0]=="hi_i_have_token"){
        //(токен,имя_кодера,сколько_одновременно_игр_он_тянет)
        if(v.size()!=4){NO_WAY("wrong_number_of_params");}
        auto it=find_if(coder2acc.begin(),coder2acc.end(),[&](auto&p){
          return p.second.token==v[1];
        });
        if(it==coder2acc.end()){NO_WAY("invalid_token");}
        auto&acc=it->second;
        if(acc.coder!=v[2]){NO_WAY("wrong_coder");}
        acc.max_games=std::max(1,std::min(max_games,std::stoi(v[3])));
        send("ok\n");
        return;
        #undef NO_WAY
      }

      send("Message received\n");
    };

    server.start();

    std::cout << "Press Enter to stop server...\n";
    std::string dummy;
    std::getline(std::cin, dummy);

    std::cout << "Disconnecting clients from IP 127.0.0.1\n";
    server.disconnect_clients_by_ip("127.0.0.1");

    server.stop();
  }
};

int main() {
  t_net_api api;
  //string line;
  //if (api.readline_from_socket("127.0.0.1:80", line)) {
  //  std::cout << "Received: " << line << std::endl;
  //}

  api.write_to_socket("127.0.0.1:31456", "POST / HTTP/1.1\r\n\r\n\r\n");
  string line;
  for(;;){
    line.clear();
    api.readline_from_socket("127.0.0.1:31456",line);
  }
  //api.close_socket("82.208.124.191:80");

  return 0;
}
