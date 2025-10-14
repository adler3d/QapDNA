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
using namespace std;

#include "qap_assert.inl"
#include "qap_vec.inl"
#include "qap_io.inl"

#include "t_splinter.inl"

#include <io.h>
#include <fcntl.h>
typedef t_splinter::t_world t_world;
static t_world::t_cmd dna/*gpt5*/(const t_world& w, int player_id)
{
    t_world::t_cmd out{};
    const double MINR = 5.0;         // компактный треугольник
    const double LONGR_BASE = 60.0;  // умеренное Ђраст€жениеї в фазе рывка
    const double THREAT_R = 9.5;     // немного с запасом к порогу 8
    const int    PERIOD = 12;        // длина цикла походки (6+6 тиков)

    // --- удобные л€мбды ---
    auto dot = [](const vec2d& a, const vec2d& b){ return a.x*b.x + a.y*b.y; };

    int base = player_id * 3;
    vec2d p0 = w.balls[base+0].pos;
    vec2d p1 = w.balls[base+1].pos;
    vec2d p2 = w.balls[base+2].pos;
    vec2d com = (p0 + p1 + p2) * (1.0/3.0);

    // --- 1) поиск ближайшей ¬–ј∆≈— ќ… перемычки (отрезка) к нашему центру масс ---
    double bestSegDist = 1e18;
    vec2d targetMid = com; // по умолчанию никуда не идЄм
    {
        const int EDGES[3][2] = {{0,1},{1,2},{2,0}};
        for(int p=0;p<4;p++){
            if(p==player_id || w.slot2deaded[p]) continue;
            int b = p*3;
            for(int e=0;e<3;e++){
                vec2d a = w.balls[b + EDGES[e][0]].pos;
                vec2d c = w.balls[b + EDGES[e][1]].pos;
                double d = t_splinter::Dist2Line(com, a, c); // 1e9 если проекци€ вне отрезка
                if(d < bestSegDist){
                    bestSegDist = d;
                    targetMid = (a + c) * 0.5; // берЄм середину, как простую цель
                }
            }
        }
        // fallback: если проекции ни на один отрезок не попали Ч идЄм к ближайшему живому сопернику
        if(bestSegDist > 1e17){
            double best = 1e18;
            for(int p=0;p<4;p++){
                if(p==player_id || w.slot2deaded[p]) continue;
                vec2d c = w.get_center_of_mass(p);
                double d = (c - com).Mag();
                if(d < best){ best = d; targetMid = c; }
            }
        }
    }

    // --- 2) проверка угрозы: чужой шар близко к ЋёЅќ… нашей перемычке? ---
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

    // --- значени€ по умолчанию (компакт + умеренно скользим) ---
    out.spring0_new_rest = MINR;
    out.spring1_new_rest = MINR;
    out.spring2_new_rest = MINR;
    out.friction0 = out.friction1 = out.friction2 = 0.30;

    // если угроза Ч просто сжатьс€ и притормозить
    if(threat){
        out.friction0 = out.friction1 = out.friction2 = 0.99;
        return out;
    }

    // --- 3) направление движени€ к цели ---
    vec2d dir = targetMid - com;
    double L = dir.Mag();
    if(L > 1e-9) dir = dir * (1.0/L); else dir = vec2d(0,0);

    // выбираем Ђносї треугольника Ч вершина, наиболее смотр€ща€ в dir
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

    // чуточку уменьшим Ђраст€жениеї, если мы ещЄ далеко от цели
    double LONGR = (bestSegDist < 60.0 ? LONGR_BASE : 40.0);

    // --- 4) проста€ двухфазна€ походка Ђинчвормаї ---
    int half = PERIOD/2;                 // 6 тиков ЂвперЄдї, 6 тиков Ђподт€нутьї
    bool extendPhase = ((w.tick / half) % 2) == 0;
    if(bestSegDist < 20.0){              // р€дом с целью Ч более частые рывки
        half = 4;
        extendPhase = ((w.tick / half) % 2) == 0;
    }

    // индексы рЄбер: e0=0-1, e1=1-2, e2=2-0
    auto set_edge_lengths_for_front = [&](int f, double len_ab, double len_bc, double len_ca){
        // раскладываем по e0/e1/e2
        // e0: 0-1
        // e1: 1-2
        // e2: 2-0
        // len_ab = длина у ребра, инцидентного (f) и (f+1)%3
        // удобнее задать €вно в switch ниже
    };

    // ‘аза Ђвыт€нуть носї: две пружины у front Ч длинные; нос скользкий, база Ђтормозитї
    if(extendPhase){
        out.f(front) = 0.01;
        out.f((front+1)%3) = 0.99;
        out.f((front+2)%3) = 0.99;

        if(front==0){ out.spring0_new_rest = LONGR; out.spring2_new_rest = LONGR; /* e1=MINR */ }
        if(front==1){ out.spring0_new_rest = LONGR; out.spring1_new_rest = LONGR; /* e2=MINR */ }
        if(front==2){ out.spring1_new_rest = LONGR; out.spring2_new_rest = LONGR; /* e0=MINR */ }
    }
    // ‘аза Ђподт€нуть базуї: все пружины короткие, нос Ђцепл€етс€ї, база скользит
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
  _setmode(_fileno(stdout),_O_BINARY);
  _setmode(_fileno(stderr),_O_BINARY);
  _setmode(_fileno(stdin),_O_BINARY);
  /*
  for(int i=0;i<100;i++){
    std::this_thread::sleep_for(1000ms);
  }*/
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

// јльтернатива без codecvt (рекомендуетс€)
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
    int result=main(argc,argv.data());

    LocalFree(wargv); // освобождаем пам€ть, выделенную CommandLineToArgvW
    return result;
}
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
  return QapLR_WinMain(hInstance,hPrevInstance,lpCmdLine,nCmdShow);
}
#endif