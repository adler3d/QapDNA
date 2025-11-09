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
#include <queue>
using namespace std;

#include "qap_assert.inl"
#include "qap_vec.inl"
#include "qap_io.inl"

#include "t_pubg.inl"
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
typedef t_pubg::t_world t_world;
t_world::Action dna_by_perplexity(const t_world& orig, int i) {
    t_world::Action act;
    if (!orig.agents[i].alive) return act; // dead agents do nothing

    int closest_enemy = -1;
    int closest_dist_sq = 1000000000;

    const auto& me = orig.agents[i];

    // Find the closest alive enemy
    for (int j = 0; j < (int)orig.agents.size(); ++j) {
        if (i == j) continue;
        if (!orig.agents[j].alive) continue;
        int dx = orig.agents[j].x - me.x;
        int dy = orig.agents[j].y - me.y;
        int dist_sq = dx*dx + dy*dy;
        if (dist_sq < closest_dist_sq) {
            closest_dist_sq = dist_sq;
            closest_enemy = j;
        }
    }

    if (closest_enemy >= 0) {
        // Chase and attack the closest enemy
        act.chase_enemy = true;
        act.attack = true;
        act.target = closest_enemy;

        // Use fury and rush if energy allows for maximum damage and mobility
        if (me.energy > 15) {
            act.use_fury = true;
            act.use_rush = true;
        } else if (me.energy > 5) {
            act.use_rush = true;
        }
    } else {
        // No enemies found, move toward center to avoid zone damage
        act.move_to_center = true;
    }

    return act;
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
  t_world w;
  string recv_buffer;
  for(int iter=0;;iter++){
    uint32_t len=0;
    if(!read_exactly_or_eof(cin,(char*)&len,sizeof(len))){
      int fail=1;
      break;
    }
    recv_buffer.resize(len);
    if(!read_exactly_or_eof(cin,&recv_buffer[0],len)){
      int fail=1;
      break;
    }
    QapLoadFromStr(w,recv_buffer);
    t_world::t_cmd cmd;
    //cmd=dna_adler20250907(w,w.your_id);
    cmd=dna_by_perplexity(w,w.your_id);
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