#include <unordered_map>
#include <set>
#include <thread>
#include <chrono>
#include <atomic>
#include "netapi.h" // для qap_zchan_write/emitter_on_data_decoder/SOCKET

struct ReliableMsg {
  std::string type; // "MSG" | "ACK"
  int64_t seq;
  std::string zchan;
  std::string payload;
};

inline std::string reliable_encode(const ReliableMsg& m) {
  return m.type + "|" + std::to_string(m.seq) + "|" + m.zchan + "|" + m.payload;
}
inline bool reliable_decode(const std::string& data, ReliableMsg& out) {
  size_t p1 = data.find('|'), p2 = data.find('|', p1+1), p3 = data.find('|', p2+1);
  if (p1==std::string::npos||p2==std::string::npos||p3==std::string::npos) return false;
  out.type = data.substr(0,p1);
  out.seq = std::stoll(data.substr(p1+1,p2-p1-1));
  out.zchan = data.substr(p2+1,p3-p2-1);
  out.payload = data.substr(p3+1);
  return true;
}

class ReliableSocketWithDecoder {
public:
  SocketWithDecoder base;
  std::atomic<int64_t> next_seq{1};
  std::mutex mtx;
  std::unordered_map<int64_t,std::pair<std::string,std::string>> pending; // seq -> {zchan,payload}
  std::set<int64_t> received_seqs;
  std::function<void(const std::string&,const std::string&)> user_cb;
  std::thread resend_thread;
  std::atomic<bool> running{true};
  int resend_interval_ms = 500; // 0.5 сек
  int ack_timeout_ms = 3000; // 3 сек

  ReliableSocketWithDecoder(const std::string& ep, std::function<void(const std::string&, const std::string&)>&& cb, bool withoutconnect)
      : base(ep, [this,cb](const std::string& z, const std::string& raw){
          ReliableMsg msg;
          if (!reliable_decode(raw,msg)) return;
          if (msg.type=="MSG") {
            {
              std::lock_guard<std::mutex> lock(this->mtx);
              if (received_seqs.count(msg.seq)) return; // дубли-детект
              received_seqs.insert(msg.seq);
            }
            cb(msg.zchan, msg.payload);
            // Отправить ACK
            ReliableMsg ack = {"ACK",msg.seq,msg.zchan,""};
            this->base.try_write(msg.zchan,reliable_encode(ack));
          } else if (msg.type=="ACK") {
            std::lock_guard<std::mutex> lock(this->mtx);
            pending.erase(msg.seq);
          }
        },withoutconnect)
  {
    resend_thread = std::thread([this]{
      while(running){
        std::this_thread::sleep_for(std::chrono::milliseconds(resend_interval_ms));
        std::lock_guard<std::mutex> lock(this->mtx);
        for(auto&[seq,dat]:pending) {
          // resend все неподтверждённые
          ReliableMsg msg = {"MSG", seq, dat.first, dat.second};
          base.try_write(dat.first, reliable_encode(msg));
        }
      }
    });
  }

  ~ReliableSocketWithDecoder() {
    running = false;
    if(resend_thread.joinable()) resend_thread.join();
  }

  void write(const std::string& zchan, const std::string& payload) {
    int64_t seq = next_seq++;
    ReliableMsg msg = {"MSG", seq, zchan, payload};
    {
      std::lock_guard<std::mutex> lock(mtx);
      pending[seq] = {zchan,payload};
    }
    base.try_write(zchan, reliable_encode(msg));
  }
};
#include "ReliableClientDecoder.h"