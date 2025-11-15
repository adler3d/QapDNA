#include <unordered_map>
#include <set>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include "netapi.h"
using std::set;
using std::function;
struct ReliableServerClient {
  emitter_on_data_decoder dec;
  set<int64_t> received_seqs;
  function<void(const string&, const string&, function<void(const string&,const string&)>)> user_cb;
  function<void(const string&, const string&)> send_func; // должен совпадать с send_to_client

  ReliableServerClient(function<void(const string&,const string&,function<void(const string&,const string&)>)> usercb,
                       function<void(const string&,const string&)> sendf)
    : user_cb(usercb), send_func(sendf)
  {
    dec.cb = [this](const string& z, const string& raw) {
      ReliableMsg msg;
      if(!reliable_decode(raw,msg))return;
      if(msg.type!="MSG")return;
      if(!received_seqs.count(msg.seq)){
        received_seqs.insert(msg.seq);
        user_cb(msg.zchan,msg.payload,send_func);
      }
      ReliableMsg ack = {"ACK",msg.seq,msg.zchan,""};
      send_func(msg.zchan,reliable_encode(ack));
    };
  }

  void feed(const char*data, size_t len) {
    dec.feed(data,len);
  }
};
/*
struct ReliableMsg {
  std::string type; // "MSG" | "ACK"
  int64_t seq;
  std::string zchan;
  std::string payload;
};
*/
class ReliableEmitterToClient {
public:
  emitter_on_data_decoder decoder;
  std::mutex mtx;
  std::atomic<int64_t> next_seq{1};
  std::unordered_map<int64_t,std::pair<std::string,std::string>> pending; // seq -> {zchan, payload}
  std::set<int64_t> received_ack; // полученные подтверждения
  std::set<int64_t> received_msg_seq; // для дубли-детекта входящих MSG (если понадобится)
  std::function<void(const std::string&,const std::string&)> send_func; // t_server_api.send_to_client
  std::function<void(const string& z, const string& payload)> cb;
  std::thread resend_thread;
  std::atomic<bool> running{true};

  int resend_interval_ms = 500;
  int ack_timeout_ms = 3000;

  void init(std::function<void(const std::string&,const std::string&)> sendf)
  {
    send_func=(sendf);
    // входящие данные от клиента — парсим как ReliableMsg
    decoder.cb = [this](const std::string& zchan, const std::string& raw) {
      ReliableMsg msg;
      if (!reliable_decode(raw,msg)) return;
      if (msg.type == "ACK") {
        std::lock_guard<std::mutex> lock(mtx);
        received_ack.insert(msg.seq);
        pending.erase(msg.seq);
      }
      // (обработка входящих MSG от клиента — если нужна, можно добавить)
      if (msg.type == "MSG") {
        std::lock_guard<std::mutex> lock(mtx);
        if(received_msg_seq.count(msg.seq))return;
        received_msg_seq.insert(msg.seq);
        if(cb)cb(msg.zchan,msg.payload);
      }
    };

    resend_thread = std::thread([this]{
      while(running){
        std::this_thread::sleep_for(std::chrono::milliseconds(resend_interval_ms));
        std::lock_guard<std::mutex> lock(mtx);
        // resend всех неподтверждённых
        for(auto&[seq, pair]:pending){
          if (received_ack.count(seq)) continue; // получил ACK — не шлём
          ReliableMsg msg = {"MSG", seq, pair.first, pair.second};
          // отправить клиенту
          send_func(pair.first, reliable_encode(msg));
        }
      }
    });
  }

  ~ReliableEmitterToClient() {
    running = false;
    if (resend_thread.joinable()) resend_thread.join();
  }

  // Сервер отправляет клиенту —
  // заводим seq, добавляем в pending, шлём
  void write(const std::string& zchan, const std::string& payload) {
    int64_t seq = next_seq++;
    ReliableMsg msg = {"MSG", seq, zchan, payload};
    {
      std::lock_guard<std::mutex> lock(mtx);
      pending[seq] = {zchan, payload};
    }
    send_func(zchan, reliable_encode(msg));
  }

  // Обработка сырых данных от клиента (вызывать из server.onClientData)
  emitter_on_data_decoder::t_parse_result feed(const char* data, size_t len) {
    return decoder.feed(data, len);
  }
};