struct t_global_user{
  int user_id;
  string email;
  string sysname;
  string visname;
  string token;
  string created_at;
};
struct t_season_coder{
  int user_id;
  vector<t_coder_rec::t_source> sarr;
  string last_submit_time;
};
typedef map<int,double> t_uid2score;
struct t_phase {
  struct QualificationRule {
    string from_phase_id;
    int top_n=50;
  };
  // type=="sandbox"|"round"
  string type;
  string phase_id;
  string world_exec;
  int num_players = 2;
  bool is_active = false;
  vector<QualificationRule> qualifying_from;
  set<int> uids;
  t_uid2score uid2score;
  vector<t_game> games;
};
typedef map<int,t_season_coder> t_uid2scoder;
struct t_season {
  string season_id; // "splinter_2025"
  string world_base; // "t_splinter"
  string title;
  t_uid2scoder uid2scoder;
  vector<unique_ptr<t_phase>> phases;
  size_t current_phase_idx = 0;
  //mutex participants_mtx;
};
struct t_main_test{
  vector<t_global_user> garr;
  vector<unique_ptr<t_season>> sarr;
};
t_phase* find_phase(t_season& season, const string& phase_id) {
  for (auto& p : season.phases) {
    if (p->phase_id == phase_id) return p.get();
  }
  return nullptr;
}
const t_phase* find_phase(const t_season& season, const string& phase_id) {
  for (auto& p : season.phases) {
    if (p->phase_id == phase_id) return p.get();
  }
  return nullptr;
}
void ensure_user_in_season(t_season& season, int user_id) {
    t_phase* active = season.phases[season.current_phase_idx].get();
    if (season.uid2scoder.count(user_id) == 0) {
        season.uid2scoder[user_id] = t_season_coder{user_id, {}, ""};
        active->uids.insert(user_id);
    }
}
void add_some_by_random(vector<t_global_user>& garr, t_season& season) {
    int new_uid = garr.size();
    garr.push_back({
        new_uid,
        "user" + to_string(new_uid) + "@test.com",
        "user" + to_string(new_uid),
        "User" + to_string(new_uid),
        sha256(to_string(new_uid)), // простой токен
        qap_time()
    });
    ensure_user_in_season(season, new_uid);

    // Добавим случайную версию кода
    auto& coder = season.uid2scoder[new_uid];
    t_coder_rec::t_source src;
    src.time = qap_time();
    src.status = "ok:compiled";
    auto&ai=src.cdn_bin_url;ai.resize(4*4);
    if(rand()%2==0){for(int i=0;i<4;i++)((int*)&ai[0])[i]=rand();}//"bin/" + to_string(new_uid) + "_v0.bin";
    else {auto r=rand();for(int i=0;i<4;i++)((int*)&ai[0])[i]=r;}
    coder.sarr.push_back(src);
    coder.last_submit_time = src.time;
}
void init_splinter_2025_season(t_season& s) {
    s.season_id = "splinter_2025";
    s.world_base = "t_splinter";
    s.title = "Splinter 2025";

    // Определяем фазы в порядке их появления
    vector<tuple<string, string, int, vector<t_phase::QualificationRule>>> phase_specs = {
        // {phase_id, type, num_players, qualifying_from}
        {"S1", "sandbox", 3, {}},
        {"R1", "round",   3, {{"S1", 900}}},
        {"S2", "sandbox", 3, {}},
        {"R2", "round",   3, {{"R1", 300}, {"S2", 60}}},
        {"SF", "sandbox", 3, {}},
        {"F",  "round",   3, {{"R2", 50},  {"SF", 10}}},
        {"S",  "sandbox", 3, {}}
    };

    for (auto& [phase_id, type, num_players, rules] : phase_specs) {
        auto phase = make_unique<t_phase>();
        phase->phase_id = phase_id;
        phase->type = type;
        phase->num_players = num_players;
        phase->qualifying_from = rules;
        // uids и uid2score заполнятся позже при активации
        s.phases.push_back(std::move(phase));
    }
}
void next_phase(t_season& season, const string& phase_id) {
    // Найдём фазу по ID
    t_phase* target = nullptr;
    for (auto& p : season.phases) {
        if (p->phase_id == phase_id) {
            target = p.get();
            break;
        }
    }
    if (!target) {
        cerr << "next_phase: phase not found: " << phase_id << "\n";
        return;
    }

    // Деактивировать предыдущую
    if (season.current_phase_idx < season.phases.size()) {
        season.phases[season.current_phase_idx]->is_active = false;
    }

    // Активировать новую
    target->is_active = true;
    // Найдём индекс
    for (size_t i = 0; i < season.phases.size(); ++i) {
        if (season.phases[i].get() == target) {
            season.current_phase_idx = i;
            break;
        }
    }

    // === Проведём отбор участников ===
    target->uids.clear();
    target->uid2score.clear();

    if (target->type == "sandbox") {
        // В песочницу — все, кто есть в сезоне
        for (const auto& [uid, _] : season.uid2scoder) {
            target->uids.insert(uid);
        }
    } else if (target->type == "round") {
        set<int> already_qualified;

        for (const auto& rule : target->qualifying_from) {
            t_phase* src_phase = nullptr;
            for (auto& p : season.phases) {
                if (p->phase_id == rule.from_phase_id) {
                    src_phase = p.get();
                    break;
                }
            }
            if (!src_phase) continue;

            // Собираем кандидатов из исходной фазы
            vector<pair<double, int>> ranked;
            for (int uid : src_phase->uids) {
                // Пропускаем тех, кто уже отобран
                if (already_qualified.count(uid)) continue;

                double score = src_phase->uid2score.count(uid) 
                    ? src_phase->uid2score.at(uid) 
                    : 1500.0;
                ranked.emplace_back(-score, uid);
            }
            sort(ranked.begin(), ranked.end());

            // Берём до rule.top_n новых участников
            int added = 0;
            for (auto& [neg_score, uid] : ranked) {
                if (added >= rule.top_n) break;
                target->uids.insert(uid);
                already_qualified.insert(uid);
                added++;
            }
        }
    }

    cout << "Activated phase: " << phase_id 
         << " with " << target->uids.size() << " participants\n";
}
void next_phase_v0(t_season& season, const string& new_phase_id) {
  // 1. Деактивировать текущую фазу
  if (season.current_phase_idx < season.phases.size()) {
      season.phases[season.current_phase_idx]->is_active = false;
  }

  // 2. Если фаза уже существует — активировать её
  for (size_t i = 0; i < season.phases.size(); ++i) {
      if (season.phases[i]->phase_id == new_phase_id) {
          season.current_phase_idx = i;
          season.phases[i]->is_active = true;
          return;
      }
  }

  // 3. Иначе — создать новую фазу
  auto phase = make_unique<t_phase>();
  phase->phase_id = new_phase_id;
  phase->world_exec = season.world_base + "_v1"; // можно усложнить позже

  // Определим тип и правила отбора по соглашению:
  if (new_phase_id == "S1" || new_phase_id == "S2" || new_phase_id == "SF" || new_phase_id == "S") {
    phase->type = "sandbox";
    phase->num_players = 2;
    // В песочницу — все участники сезона
    for (const auto& [uid, _] : season.uid2scoder) {
      phase->uids.insert(uid);
    }
  } else if (new_phase_id == "R1") {
    phase->type = "round";
    phase->qualifying_from = {{"S1", 900}};
  } else if (new_phase_id == "R2") {
    phase->type = "round";
    phase->qualifying_from = {{"R1", 300}, {"S2", 60}};
  } else if (new_phase_id == "F") {
    phase->type = "round";
    phase->qualifying_from = {{"R2", 50}, {"SF", 10}};
  }

  // Проведём отбор, если это не песочница
  if (phase->type == "round") {
    for (const auto& rule : phase->qualifying_from) {
      t_phase* src_phase = find_phase(season, rule.from_phase_id);
      if (!src_phase) continue;

      // Сортируем по рейтингу (uid2score)
      vector<pair<double, int>> ranked;
      for (int uid : src_phase->uids) {
        double score = src_phase->uid2score.count(uid) ? src_phase->uid2score.at(uid) : 1500.0;
        ranked.emplace_back(-score, uid); // минус для сортировки по убыванию
      }
      sort(ranked.begin(), ranked.end());

      // Берём top_n
      int n = min(rule.top_n, (int)ranked.size());
      for (int i = 0; i < n; ++i) {
        phase->uids.insert(ranked[i].second);
      }
    }
  }

  season.phases.push_back(std::move(phase));
  season.current_phase_idx = season.phases.size() - 1;
  season.phases.back()->is_active = true;
}
#include <vector>
#include <cstdint>

std::vector<double> simulate_arena(const std::vector<int>& slot2start) {
    const int n = static_cast<int>(slot2start.size());
    if (n < 2) {
        return std::vector<double>(n, 0.0);
    }

    // Преобразуем вход в 16-битные стратегии (гарантируем unsigned 16-bit)
    std::vector<uint16_t> strategies;
    strategies.reserve(n);
    for (int s : slot2start) {
        strategies.push_back(static_cast<uint16_t>(s & 0xFFFF));
    }

    std::vector<int64_t> total_score(n, 0); // используем int64 для избежания переполнения

    // Вспомогательные константы
    const int C = 0; // cooperate
    const int D = 1; // defect

    // Функция выбора действия по стратегии, тику и истории
    auto get_action = [&](uint16_t strat, int tick, int opp_prev) -> int {
        int rule = (strat >> (2 * tick)) & 3;
        switch (rule) {
            case 0: return D;                     // Defect
            case 1: return C;                     // Cooperate
            case 2: return (tick == 0) ? C : opp_prev; // Mirror
            case 3: return (tick == 0) ? D : (1 - opp_prev); // Opposite
            default: return D; // unreachable
        }
    };

    // Функция подсчёта очков за ход
    auto payoff = [&](int a, int b) -> int {
        // a, b ∈ {C=0, D=1}
        if (a == C && b == C) return 2;
        if (a == C && b == D) return 0;
        if (a == D && b == C) return 3;
        return 1; // D vs D
    };

    // Сыграем все пары
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            uint16_t sA = strategies[i];
            uint16_t sB = strategies[j];

            int scoreA = 0, scoreB = 0;
            int prevA = C; // значение не используется в тике 0, но инициализируем
            int prevB = C;

            for (int tick = 0; tick < 5; ++tick) {
                int actA = get_action(sA, tick, prevB);
                int actB = get_action(sB, tick, prevA);

                scoreA += payoff(actA, actB);
                scoreB += payoff(actB, actA);

                prevA = actA;
                prevB = actB;
            }

            int delta = scoreA - scoreB;
            total_score[i] += delta;
            total_score[j] -= delta;
        }
    }

    // Преобразуем в double
    std::vector<double> result(n);
    for (int i = 0; i < n; ++i) {
        result[i] = static_cast<double>(total_score[i]);
    }
    return result;
}
void add_finished_game(const t_main_test&mt,t_season& season) {
    t_phase* active = season.phases[season.current_phase_idx].get();
    if (active->uids.empty()) return;
    auto&pid=active->phase_id;
    int R=pid=="R1"?1:("R2"==pid?2:("F"==pid?3:0));
    // Выберем случайных игроков
    vector<int> candidates(active->uids.begin(), active->uids.end());
    if (candidates.size() < (size_t)active->num_players) return;

    shuffle(candidates.begin(), candidates.end(), default_random_engine(rand()));
    vector<int> players(candidates.begin(), candidates.begin() + active->num_players);
    
    t_game game;game.gd.arr.resize(players.size());
    // Сымитируем результат
    t_finished_game fg;
    fg.game_id = rand();
    fg.tick = 1000;
    fg.slot2score.resize(players.size());
    fg.slot2ms.resize(players.size());
    /*
    for (size_t i = 0; i < players.size(); ++i) {
        fg.slot2score[i] = ((double)(rand() % 10001)-5000)*0.125*0.5;
        fg.slot2ms[i] = 100.0 + (rand() % 500);
        game.gd.arr[i].coder=mt.garr[players[i]].sysname;
    }*/
    /*
    vector<double> raw(players.size());
    double sum = 0.0;
    for (size_t i = 0; i < players.size(); ++i) {
        raw[i] = (double)(rand() % 10001 - 5000); // -5000..+5000
        sum += raw[i];
    }*/
    vector<int> ais(players.size());
    for (size_t i = 0; i < players.size(); ++i) {
      ais[i]=((int*)&season.uid2scoder[players[i]].sarr.back().cdn_bin_url[0])[R];//mt.garr[players[i]]
    }
    auto r=simulate_arena(ais);
    // Нормализовать, чтобы сумма = 0
    //double avg = sum / players.size();
    for (size_t i = 0; i < players.size(); ++i) {
        fg.slot2score[i] = r[i];//(raw[i] - avg) * 0.0625; // масштаб
        game.gd.arr[i].coder=mt.garr[players[i]].sysname;
    }

    // Сохраним игру
    game.fg = fg;
    game.status = "finished";
    game.finished_at = qap_time();
    active->games.push_back(game);

    // Обновим рейтинг (упрощённо)
    for (size_t i = 0; i < players.size(); ++i) {
        int uid = players[i];
        double old_score = active->uid2score.count(uid) ? active->uid2score[uid] : 1500.0;
        active->uid2score[uid] = old_score + fg.slot2score[i]; // условно
    }
}
void print_top(const t_main_test&mt,const t_season& season, const string& phase_id) {
  auto*p = find_phase(season, phase_id);
  if (!p) { cout << "Phase " << phase_id << " not found\n"; return; }
  cout << "Phase " << phase_id << " has " << p->games.size() << " games\n";
  auto&pid=p->phase_id;
  int R=pid=="R1"?1:("R2"==pid?2:("F"==pid?3:0));
  vector<pair<double, int>> ranked;
  for (int uid : p->uids) {
    double score = p->uid2score.count(uid) ? p->uid2score.at(uid) : 1500.0;
    ranked.emplace_back(-score, uid);
  }
  sort(ranked.begin(), ranked.end());

  cout << "\n=== TOP 1000 (of "<<ranked.size()<<") in " << phase_id << " ===\n";
  int n = min(1000, (int)ranked.size());
  for (int i = 0; i < n; ++i) {
    int g=0;
    auto s=ranked[i].second;
    auto&sc=season.uid2scoder.at(s);
    auto&uid=sc.user_id;
    auto&sn=mt.garr[uid].sysname;
    for(int j=0;j<p->games.size();j++)
    {
      auto&ex=p->games[j];
      for(int z=0;z<ex.gd.arr.size();z++){
        if(ex.gd.arr[z].coder==sn)g++;
      }
    }
    auto tostrR=[](const string&s,int R){
      return to_string(((int*)&s[0])[R]);
    };
    auto tostr=[&](const string&s){
      return tostrR(s,0)+","+tostrR(s,1)+","+tostrR(s,2)+","+tostrR(s,3);
    };
    cout << (i+1) << ". UID=" << ranked[i].second
         << " score=" << -ranked[i].first << " "<< g<< " "<< tostr(season.uid2scoder.at(ranked[i].second).sarr.back().cdn_bin_url) <<"\n";
  }
}
int mt_games_per_phase=8*16000;
void play_all_game_iniside_round(const t_main_test&mt,t_season& season, const string& phase_id) {
    t_phase* p = find_phase(season, phase_id);
    if (!p) return;
    for (int i = 0; i < mt_games_per_phase; ++i) {
        add_finished_game(mt,season);
    }
}
void main_test(){
  srand(time(0));
  t_main_test mt;
  mt.sarr.push_back(make_unique<t_season>());
  auto&s=*mt.sarr[0];
  init_splinter_2025_season(s);
  next_phase(s,"S1");
  //
  auto f=[&](int v){
    add_some_by_random(mt.garr,s);
    auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
    qwen.resize(4*4);for(int i=0;i<4;i++)((int*)&qwen[0])[i]=v;
  };
  for(auto v:{640,896,736}){f(v);f(v);f(v);}

  add_some_by_random(mt.garr,s);
  {auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
  qwen.resize(4*4);for(int i=0;i<4;i++)((int*)&qwen[0])[i]=640;((int*)&qwen[0])[3]=896;}
  add_some_by_random(mt.garr,s);
  {auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
  qwen.resize(4*4);for(int i=0;i<4;i++)((int*)&qwen[0])[i]=640;((int*)&qwen[0])[3]=896;}
  add_some_by_random(mt.garr,s);
  {auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
  qwen.resize(4*4);for(int i=0;i<4;i++)((int*)&qwen[0])[i]=640;((int*)&qwen[0])[3]=896;}
  add_some_by_random(mt.garr,s);
  {auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
  qwen.resize(4*4);for(int i=0;i<4;i++)((int*)&qwen[0])[i]=640;((int*)&qwen[0])[3]=736;}
  add_some_by_random(mt.garr,s);
  {auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
  qwen.resize(4*4);for(int i=0;i<4;i++)((int*)&qwen[0])[i]=640;((int*)&qwen[0])[3]=736;}
  add_some_by_random(mt.garr,s);
  {auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
  qwen.resize(4*4);for(int i=0;i<4;i++)((int*)&qwen[0])[i]=640;((int*)&qwen[0])[3]=736;}
  for(int i=0;i<4;i++){
  add_some_by_random(mt.garr,s);
  {auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
  qwen.resize(4*4);
  ((int*)&qwen[0])[0]=0;
  ((int*)&qwen[0])[1]=31755;
  ((int*)&qwen[0])[2]=31747;
  ((int*)&qwen[0])[3]=640;}}

  for(int i=0;i<mt_games_per_phase;i++){
    if(rand()%10==0)add_some_by_random(mt.garr,s);
    add_finished_game(mt,s);
    //update_elo(mt);
  }

  print_top(mt,s,"S1");
  next_phase(s,"R1");
  play_all_game_iniside_round(mt,s,"R1");
  print_top(mt,s,"R1");
  next_phase(s,"S2");
  for(int i=0;i<mt_games_per_phase;i++){
    if(rand()%30==0)add_some_by_random(mt.garr,s);
    add_finished_game(mt,s);
    //update_elo(mt);
  }
  print_top(mt,s,"S2");
  next_phase(s,"R2");
  play_all_game_iniside_round(mt,s,"R2");
  print_top(mt,s,"R2");
  next_phase(s,"SF");
  for(int i=0;i<mt_games_per_phase;i++){
    if(rand()%500==0)add_some_by_random(mt.garr,s);
    add_finished_game(mt,s);
    //update_elo(mt);
  }
  print_top(mt,s,"SF");
  next_phase(s,"F");
  play_all_game_iniside_round(mt,s,"F");
  print_top(mt,s,"F");
  next_phase(s,"S");
  for(int i=0;i<mt_games_per_phase;i++){
    if(rand()%500==0)add_some_by_random(mt.garr,s);
    add_finished_game(mt,s);
    //update_elo(mt);
  }
  print_top(mt,s,"S");
  for(;;){
    this_thread::sleep_for(1000ms);
  }
  int gg=1;
  //next_phase(s,"frozen-end?");
}