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
uint64_t rnd(){
  return 
    (uint64_t(rand())<<48)|
    (uint64_t(rand())<<32)|
    (uint64_t(rand())<<16)|
    (uint64_t(rand())<<0);
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
    auto&ai=src.cdn_bin_url;ai.resize(8*4);
    if(rand()%5==1){for(int i=0;i<4;i++)((uint64_t*)&ai[0])[i]=rnd();}//"bin/" + to_string(new_uid) + "_v0.bin";
    else {auto r=rnd();for(int i=0;i<4;i++)((uint64_t*)&ai[0])[i]=r;}
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
        {"S1", "sandbox", 30*2, {}},
        {"R1", "round",   30*2, {{"S1", 900}}},
        {"S2", "sandbox", 30*2, {}},
        {"R2", "round",   30*2, {{"R1", 300}, {"S2", 60}}},
        {"SF", "sandbox", 30*2, {}},
        {"F",  "round",   30*2, {{"R2", 50},  {"SF", 10}}},
        {"S",  "sandbox", 30*2, {}}
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
using Strategy = std::array<uint8_t, 8>; // 8 states, 1 byte each
std::vector<double> simulate_arena_v6(const std::vector<Strategy>& strategies,bool debug);
std::vector<double> simulate_arena(const std::vector<uint64_t>& strategies,bool debug=false) {
  return simulate_arena_v6((std::vector<Strategy>&)strategies,debug);
}
std::vector<double> simulate_arena_8byte(const std::vector<Strategy>& strategies) {
    const int n = static_cast<int>(strategies.size());
    if (n < 2) {
        return std::vector<double>(n, 0.0);
    }

    std::vector<int64_t> total_score(n, 0);

    const int C = 0;
    const int D = 1;

    // Вспомогательная функция: извлечь действие и переходы из байта состояния
    auto decode_state = [](uint8_t state_byte) -> std::tuple<int, int, int> {
        int action = (state_byte >> 7) & 1;
        int next_if_C = (state_byte >> 4) & 7;   // bits 6-4
        int next_if_D = (state_byte >> 0) & 15;  // bits 3-0 → но только 0-7 нужны
        if (next_if_D > 7) next_if_D = 7;        // защита от некорректных данных
        return {action, next_if_C, next_if_D};
    };

    // Функция подсчёта очков за ход
    auto payoff = [&](int a, int b) -> int {
        if (a == C && b == C) return 2;
        if (a == C && b == D) return 0;
        if (a == D && b == C) return 3;
        return 1; // D vs D
    };

    // Сыграем все пары
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            int stateA = 0, stateB = 0;
            int scoreA = 0, scoreB = 0;

            for (int tick = 0; tick < 8; ++tick) {
                auto [actA, nextA_C, nextA_D] = decode_state(strategies[i][stateA]);
                auto [actB, nextB_C, nextB_D] = decode_state(strategies[j][stateB]);

                scoreA += payoff(actA, actB);
                scoreB += payoff(actB, actA);

                // Обновляем состояния по действиям оппонентов
                stateA = (actB == C) ? nextA_C : nextA_D;
                stateB = (actA == C) ? nextB_C : nextB_D;
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

void print_strategy(const uint64_t&u) {
  auto&s=(const Strategy&)u;
  cout<<u<<"\n";
  std::cout << "S|A|C|D\n";
  std::cout << "-------\n";
  for (int i = 0; i < 8; ++i) {
      uint8_t b = s[i];
      int action = (b >> 7) & 1;
      int nextC = (b >> 4) & 7;
      int nextD = (b >> 0) & 15;
      if (nextD > 7) nextD = 7;
      std::cout << "" << i << "|" << (action ? 'D' : 'C')
                << "|" << nextC << "|" << nextD << "\n";
  }
}
#include <vector>
#include <array>
#include <cstdint>

using Strategy = std::array<uint8_t, 8>;

std::vector<double> simulate_arena_v6(const std::vector<Strategy>& strategies,bool debug) {
    const int n = static_cast<int>(strategies.size());
    if (n < 2) return std::vector<double>(n, 0.0);

    std::vector<int64_t> total_score(n, 0);
    const int TOTAL_TICKS = 10; // 2 test + 8 scored
    const int SCORED_TICKS_START = 2;

    auto decode = [](uint8_t b) -> std::tuple<int, int, int> {
        int a = (b >> 7) & 1;
        int nC = (b >> 4) & 7;
        int nD = (b >> 0) & 15;
        if (nD > 7) nD = 7;
        return {a, nC, nD};
    };

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            int sA = 0, sB = 0;
            int scoreA = 0, scoreB = 0;

            for (int t = 0; t < TOTAL_TICKS; ++t) {
                auto [aA, nA_C, nA_D] = decode(strategies[i][sA]);
                auto [aB, nB_C, nB_D] = decode(strategies[j][sB]);

                // Only score ticks 2-9
                if (t >= SCORED_TICKS_START) {
                    if (aA == 0 && aB == 0) {
                        scoreA += 3; scoreB += 3;      // C/C = high reward
                    } else if (aA == 0 && aB == 1) {
                        scoreA += 0; scoreB += 1;      // C/D = low exploitation
                    } else if (aA == 1 && aB == 0) {
                        scoreA += 1; scoreB += 0;
                    } else {
                        scoreA -= 2; scoreB -= 2;      // D/D = punishment
                    }
                }

                // State transitions happen on every tick (including test phase)
                sA = (aB == 0) ? nA_C : nA_D;
                sB = (aA == 0) ? nB_C : nB_D;
            }

            int delta = scoreA - scoreB;
            total_score[i] += delta;
            total_score[j] -= delta;
        }
    }

    std::vector<double> res(n);
    for (int i = 0; i < n; ++i) res[i] = static_cast<double>(total_score[i]);
    return res;
}
std::vector<double> simulate_arena_v5(const std::vector<Strategy>& strategies,bool debug) {
    const int n = static_cast<int>(strategies.size());
    if (n < 2) return std::vector<double>(n, 0.0);

    std::vector<int64_t> total_score(n, 0);
    const int TICKS = 8;

    auto decode = [](uint8_t b) -> std::tuple<int, int, int> {
        int a = (b >> 7) & 1;
        int nC = (b >> 4) & 7;
        int nD = (b >> 0) & 15;
        if (nD > 7) nD = 7;
        return {a, nC, nD};
    };

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            int sA = 0, sB = 0;
            int scoreA = 0, scoreB = 0;
            auto aiA=strategies[i];auto aiB=strategies[j];
            if(debug){print_strategy((int64_t&)aiA);print_strategy((int64_t&)aiB);}
            for (int t = 0; t < TICKS; ++t) {
                auto [aA, nA_C, nA_D] = decode(aiA[sA]);
                auto [aB, nB_C, nB_D] = decode(aiB[sB]);

                if (aA == 0 && aB == 0) {
                    scoreA += 1; scoreB += 1;      // C/C
                } else if (aA == 0 && aB == 1) {
                    scoreA -= 1; scoreB += 1;      // C/D
                } else if (aA == 1 && aB == 0) {
                    scoreA += 1; scoreB -= 1;      // D/C
                } else {
                    scoreA -= 3; scoreB -= 3;      // D/D — catastrophic!
                }

                sA = (aB == 0) ? nA_C : nA_D;
                sB = (aA == 0) ? nB_C : nB_D;
            }

            int delta = scoreA - scoreB;
            total_score[i] += delta;
            total_score[j] -= delta;
        }
    }

    std::vector<double> res(n);
    for (int i = 0; i < n; ++i) res[i] = static_cast<double>(total_score[i]);
    return res;
}
std::vector<double> simulate_arena_v4(const std::vector<Strategy>& strategies) {
    const int n = static_cast<int>(strategies.size());
    if (n < 2) return std::vector<double>(n, 0.0);

    std::vector<int64_t> total_score(n, 0);
    const int TICKS = 8;

    auto decode = [](uint8_t b) -> std::tuple<int, int, int> {
        int a = (b >> 7) & 1;
        int nC = (b >> 4) & 7;
        int nD = (b >> 0) & 15;
        if (nD > 7) nD = 7;
        return {a, nC, nD};
    };

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            int sA = 0, sB = 0;
            int scoreA = 0, scoreB = 0;

            for (int t = 0; t < TICKS; ++t) {
                auto [aA, nA_C, nA_D] = decode(strategies[i][sA]);
                auto [aB, nB_C, nB_D] = decode(strategies[j][sB]);

                if (aA == 0 && aB == 0) {
                    scoreA += 2; scoreB += 2;
                } else if (aA == 0 && aB == 1) {
                    scoreA -= 1; scoreB += 3;
                } else if (aA == 1 && aB == 0) {
                    scoreA += 3; scoreB -= 1;
                } else {
                    scoreA -= 2; scoreB -= 2; // mutual defection is painful!
                }

                sA = (aB == 0) ? nA_C : nA_D;
                sB = (aA == 0) ? nB_C : nB_D;
            }

            int delta = scoreA - scoreB;
            total_score[i] += delta;
            total_score[j] -= delta;
        }
    }

    std::vector<double> res(n);
    for (int i = 0; i < n; ++i) res[i] = static_cast<double>(total_score[i]);
    return res;
}
std::vector<double> simulate_arena_v3(const std::vector<Strategy>& strategies) {
    const int n = static_cast<int>(strategies.size());
    if (n < 2) return std::vector<double>(n, 0.0);

    std::vector<int64_t> total_score(n, 0);
    const int TICKS = 20; // достаточно для раскрытия стратегий

    auto decode = [](uint8_t b) -> std::tuple<int, int, int> {
        int a = (b >> 7) & 1;
        int nC = (b >> 4) & 7;
        int nD = (b >> 0) & 15;
        if (nD > 7) nD = 7;
        return {a, nC, nD};
    };

    // Базовая матрица, но с растущим штрафом за D/D
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            int sA = 0, sB = 0;
            int scoreA = 0, scoreB = 0;

            for (int t = 0; t < TICKS; ++t) {
                auto [aA, nA_C, nA_D] = decode(strategies[i][sA]);
                auto [aB, nB_C, nB_D] = decode(strategies[j][sB]);

                int payoffA = 0, payoffB = 0;
                const int phase = (t < 10) ? 0 : 1; // early / late

                if (aA == 0 && aB == 0) {
                    // C/C
                    payoffA = payoffB = (phase == 0) ? 2 : 4; // поздняя кооперация = супервыгода
                } else if (aA == 0 && aB == 1) {
                    // C/D
                    payoffA = (phase == 0) ? 0 : -3; // позднее предательство = жёсткое наказание C
                    payoffB = (phase == 0) ? 3 : 1; // позднее D почти бесполезно
                } else if (aA == 1 && aB == 0) {
                    payoffA = (phase == 0) ? 3 : 1;
                    payoffB = (phase == 0) ? 0 : -3;
                } else {
                    // D/D
                    payoffA = payoffB = (phase == 0) ? 1 : -4; // поздняя война = катастрофа
                }

                scoreA += payoffA;
                scoreB += payoffB;

                sA = (aB == 0) ? nA_C : nA_D;
                sB = (aA == 0) ? nB_C : nB_D;
            }

            int delta = scoreA - scoreB;
            total_score[i] += delta;
            total_score[j] -= delta;
        }
    }

    std::vector<double> res(n);
    for (int i = 0; i < n; ++i) res[i] = static_cast<double>(total_score[i]);
    return res;
}
std::vector<double> simulate_arena_v2(const std::vector<Strategy>& strategies) {
    const int n = static_cast<int>(strategies.size());
    if (n < 2) {
        return std::vector<double>(n, 0.0);
    }

    std::vector<int64_t> total_score(n, 0);
    const int TICKS = 12;
    const int MAX_RESOURCE = 100;
    const int L = 1; // Cooperate
    const int H = 3; // Defect

    auto decode_state = [](uint8_t b) -> std::tuple<int, int, int> {
        int act = (b >> 7) & 1;
        int nextC = (b >> 4) & 7;
        int nextD = (b >> 0) & 15;
        if (nextD > 7) nextD = 7;
        return {act, nextC, nextD};
    };

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            int stateA = 0, stateB = 0;
            int totalA = 0, totalB = 0;
            int resource = MAX_RESOURCE;

            for (int tick = 0; tick < TICKS; ++tick) {
                auto [actA, nextA_C, nextA_D] = decode_state(strategies[i][stateA]);
                auto [actB, nextB_C, nextB_D] = decode_state(strategies[j][stateB]);

                int reqA = (actA == 0) ? L : H;
                int reqB = (actB == 0) ? L : H;
                int total_req = reqA + reqB;

                int gainA = 0, gainB = 0;
                if (total_req <= resource) {
                    gainA = reqA;
                    gainB = reqB;
                    resource -= total_req;
                } else {
                    // collapse: nobody gets anything, resource degrades
                    resource = std::max(0, resource - 10);
                }

                // Resource regenerates (but capped)
                resource = std::min(MAX_RESOURCE, resource + 5);

                totalA += gainA;
                totalB += gainB;

                // Update states based on opponent's action
                stateA = (actB == 0) ? nextA_C : nextA_D;
                stateB = (actA == 0) ? nextB_C : nextB_D;
            }

            int delta = totalA - totalB;
            total_score[i] += delta;
            total_score[j] -= delta;
        }
    }

    std::vector<double> result(n);
    for (int i = 0; i < n; ++i) {
        result[i] = static_cast<double>(total_score[i]);
    }
    return result;
}
bool add_finished_game(const t_main_test&mt,t_season& season,bool*plastwin=nullptr,bool debug=false,bool need_shufle=true) {
    t_phase* active = season.phases[season.current_phase_idx].get();
    if (active->uids.empty()) return false;
    // Выберем случайных игроков
    vector<int> candidates(active->uids.begin(), active->uids.end());
    if (candidates.size() < (size_t)active->num_players) return false;
    auto&pid=active->phase_id;
    int R=pid=="R1"?1:("R2"==pid?2:("F"==pid?3:0));

    if(need_shufle)shuffle(candidates.begin(), candidates.end(), default_random_engine(rand()));
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
    int last=mt.garr.size()-1;int last_pid=-1;
    vector<uint64_t> ais(players.size());
    for (size_t i = 0; i < players.size(); ++i) {
      auto uid=players[i];if(uid==last)last_pid=uid;
      ais[i]=((uint64_t*)&season.uid2scoder[uid].sarr.back().cdn_bin_url[0])[R];//mt.garr[players[i]]
    }
    if(last_pid==-1&&plastwin){
      last_pid=0;
      players[0]=last;
      ais[0]=((uint64_t*)&season.uid2scoder[last].sarr.back().cdn_bin_url[0])[R];//mt.garr[players[i]]
    }
    if(last_pid==-1&&plastwin){*plastwin=false;return false;}
    auto r=simulate_arena(ais,debug);
    if(plastwin){
      auto max=r[last_pid];
      for(int i=0;i<r.size();i++)if(r[i]>max){*plastwin=false;return true;}
      *plastwin=true;return true;
    }
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
    return true;
}
void print_strategy_v0(const uint64_t&u) {
    auto&s=(const Strategy&)u;
    std::cout << "State | Action | Next if C | Next if D\n";
    std::cout << "--------------------------------------\n";
    for (int i = 0; i < 8; ++i) {
        uint8_t b = s[i];
        int action = (b >> 7) & 1;
        int nextC = (b >> 4) & 7;
        int nextD = (b >> 0) & 15;
        if (nextD > 7) nextD = 7;
        std::cout << "  " << i << "   |    " << (action ? 'D' : 'C')
                  << "   |     " << nextC << "      |     " << nextD << "\n";
    }
}
void print_top(const t_main_test&mt,const t_season& season, const string& phase_id,int detailed=0) {
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

  auto tostrR=[](const string&s,int R){
    return to_string(((uint64_t*)&s[0])[R]);
  };
  auto tostr=[&](const string&s){
    return tostrR(s,0)+","+tostrR(s,1)+","+tostrR(s,2)+","+tostrR(s,3);
  };
  auto pri=[&](const string&s){
    print_strategy(((uint64_t*)&s[0])[R]);
  };
  cout << "\n=== TOP 1000 (of "<<ranked.size()<<") in " << phase_id << " ===\n";
  int n = min(1000, (int)ranked.size());
  if(!detailed)for (int i = 0; i < n; ++i) {
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
    cout << (i+1) << ". UID=" << ranked[i].second
         << " score=" << -ranked[i].first << " "<< g<< " "<< tostr(season.uid2scoder.at(ranked[i].second).sarr.back().cdn_bin_url) <<"\n";
  }
  if(detailed)cout << "\n=== TOP "<<detailed<<" (of "<<ranked.size()<<") in " << phase_id << " ===\n";
  for(int i=0;i<detailed;i++){
    auto s=ranked[i].second;
    auto&sc=season.uid2scoder.at(s);
    auto&uid=sc.user_id;
    auto&sn=mt.garr[uid].sysname;
    int g=0;
    for(int j=0;j<p->games.size();j++)
    {
      auto&ex=p->games[j];
      for(int z=0;z<ex.gd.arr.size();z++){
        if(ex.gd.arr[z].coder==sn)g++;
      }
    }
    cout << (i+1) << ". UID=" << ranked[i].second
         << " score=" << -ranked[i].first << " "<< g<< " "<< tostr(season.uid2scoder.at(ranked[i].second).sarr.back().cdn_bin_url) <<"\n";
  
    pri(season.uid2scoder.at(ranked[i].second).sarr.back().cdn_bin_url);
  }
}
int mt_games_per_phase=3000;//2*64000;
void play_all_game_iniside_round(const t_main_test&mt,t_season& season, const string& phase_id) {
    t_phase* p = find_phase(season, phase_id);
    if (!p) return;
    for (int i = 0; i < mt_games_per_phase; ++i) {
        add_finished_game(mt,season);
    }
}
uint64_t encode_strategy(const Strategy& s) {
    uint64_t x = 0;
    for (int i = 0; i < 8; ++i) {
        x |= static_cast<uint64_t>(s[i]) << (8 * i);
    }
    return x;
}
#include <array>
#include <random>
#include <queue>
#include <unordered_set>
#include <cstdint>

using Strategy = std::array<uint8_t, 8>;
std::uniform_int_distribution<uint32_t> bool_distr{0,1};
std::uniform_int_distribution<uint32_t> byte_dist(0, 255);
Strategy generate_reachable_strategy(std::mt19937& rng,int ATTEMPTS=100) {
    Strategy strat;

    // 1. Сначала генерируем случайную стратегию
    for (int i = 0; i < 8; ++i) {
        // Гарантируем, что next_if_D ∈ [0,7]
        uint8_t b = byte_dist(rng);
        b = (b & 0xF0) | (b & 7); // обнуляем биты 3-0, кроме младших 3
        strat[i] = b;
    }

    // 2. Проверим, сколько состояний достижимо
    auto count_reachable = [](const Strategy& s) -> int {
        std::queue<int> q;
        std::unordered_set<int> visited;
        q.push(0);
        visited.insert(0);

        while (!q.empty()) {
            int state = q.front(); q.pop();
            uint8_t b = s[state];
            int nextC = (b >> 4) & 7;
            int nextD = (b >> 0) & 7;

            if (visited.find(nextC) == visited.end()) {
                visited.insert(nextC);
                q.push(nextC);
            }
            if (visited.find(nextD) == visited.end()) {
                visited.insert(nextD);
                q.push(nextD);
            }
        }
        return static_cast<int>(visited.size());
    };

    // 3. Если <4 состояний — модифицируем до выполнения условия
    for (int attempt = 0; attempt < ATTEMPTS; ++attempt) {
        if (count_reachable(strat) >= 4) {
            return strat;
        }

        // Иначе — сделаем стратегию более связной:
        // Выберем случайное состояние и "подключим" его к графу
        std::uniform_int_distribution<int> state_dist(1, 7); // не трогаем 0
        int target = state_dist(rng);
        // Найдём достижимое состояние и заставим его вести в target
        auto reachable = [&]() {
            std::queue<int> q;
            std::unordered_set<int> vis;
            q.push(0); vis.insert(0);
            while (!q.empty()) {
                int u = q.front(); q.pop();
                uint8_t b = strat[u];
                int nC = (b >> 4) & 7;
                int nD = (b >> 0) & 7;
                if (!vis.count(nC)) { vis.insert(nC); q.push(nC); }
                if (!vis.count(nD)) { vis.insert(nD); q.push(nD); }
            }
            return vis;
        }();

        if (!reachable.empty()) {
            // Выбираем случайное достижимое состояние
            std::vector<int> rvec(reachable.begin(), reachable.end());
            std::uniform_int_distribution<size_t> idx_dist(0, rvec.size() - 1);
            int from = rvec[idx_dist(rng)];

            // Модифицируем его: направим один из переходов в target
            uint8_t& b = strat[from];
            bool to_C = bool_distr(rng);
            if (to_C) {
                b = (b & 0x8F) | ((target & 7) << 4); // обновить next_if_C
            } else {
                b = (b & 0xF8) | (target & 7);        // обновить next_if_D
            }
        }
    }

    // Если не получилось — вернём гарантированно связную стратегию
    // Пример: цикл 0->1->2->3->0 и остальные ведут в цикл
    strat = {{
        (1 << 7) | (1 << 4) | 0, // 0: D, C→1, D→0
        (0 << 7) | (2 << 4) | 0, // 1: C, C→2, D→0
        (1 << 7) | (3 << 4) | 0, // 2: D, C→3, D→0
        (0 << 7) | (0 << 4) | 0, // 3: C, C→0, D→0
        (0 << 7) | (0 << 4) | 0, // 4: C → 0
        (1 << 7) | (1 << 4) | 2, // 5: D → 1 или 2
        (0 << 7) | (3 << 4) | 1, // 6: C → 3 или 1
        (1 << 7) | (2 << 4) | 3  // 7: D → 2 или 3
    }};
    return strat;
}
Strategy smart_defect = {{
    136, // 0
    18,  // 1
    136, // 2
    136, // 3
    136, // 4
    136, // 5
    136, // 6
    136  // 7
}};auto smart_defect_num=encode_strategy(smart_defect);
Strategy evolved_defect = {{
    // state 0: D, C→0, D→1
    (1 << 7) | (0 << 4) | 1,  // 0b10000001 = 129

    // state 1: D, C→2, D→1
    (1 << 7) | (2 << 4) | 1,  // 0b10100001 = 161

    // state 2: C, C→2, D→3
    (0 << 7) | (2 << 4) | 3,  // 0b00100011 = 35

    // state 3: D, C→2, D→1
    (1 << 7) | (2 << 4) | 1,  // 161

    // states 4-7: копии для запаса
    129, 161, 35, 161
}};auto evolved_defect_num=encode_strategy(evolved_defect);
Strategy generous_tft = {{
    (0<<7) | (0<<4) | 1, // 0: C, C→0, D→1
    (0<<7) | (0<<4) | 2, // 1: C, C→0, D→2
    (1<<7) | (0<<4) | 2,  // 2: D, C→0, D→2
    // остальные состояния — дублируем состояние 2
    (1<<7) | (0<<4) | 2,
    (1<<7) | (0<<4) | 2,
    (1<<7) | (0<<4) | 2,
    (1<<7) | (0<<4) | 2,
    (1<<7) | (0<<4) | 2
}};auto generous_tft_num=encode_strategy(generous_tft);
Strategy opportunistic = {{
    (1<<7) | (1<<4) | 0, // 0: D, C→1, D→0
    (1<<7) | (2<<4) | 0, // 1: D, C→2, D→0
    (0<<7) | (2<<4) | 3, // 2: C, C→2, D→3
    (1<<7) | (2<<4) | 3, // 3: D, C→2, D→3
    // дублируем состояние 3 для остальных
    (1<<7) | (2<<4) | 3,
    (1<<7) | (2<<4) | 3,
    (1<<7) | (2<<4) | 3,
    (1<<7) | (2<<4) | 3
}};auto opportunistic_num=encode_strategy(opportunistic);
Strategy fast_trust = {{
    (1<<7) | (1<<4) | 2, // 0: D, C→1, D→2
    (0<<7) | (1<<4) | 3, // 1: C, C→1, D→3
    (1<<7) | (1<<4) | 2, // 2: D, C→1, D→2
    (1<<7) | (1<<4) | 3, // 3: D, C→1, D→3
    // дублируем состояние 3
    (1<<7) | (1<<4) | 3,
    (1<<7) | (1<<4) | 3,
    (1<<7) | (1<<4) | 3,
    (1<<7) | (1<<4) | 3
}};auto fast_trust_num=encode_strategy(fast_trust);
void main_test(){
  srand(time(0));
  t_main_test mt;
  mt.sarr.push_back(make_unique<t_season>());
  auto&s=*mt.sarr[0];
  init_splinter_2025_season(s);
  next_phase(s,"S1");
  //
  auto f=[&](uint64_t v){
    add_some_by_random(mt.garr,s);
    auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
    qwen.resize(8*4);for(int i=0;i<4;i++)((uint64_t*)&qwen[0])[i]=v;
  };
  auto g=[&](uint64_t v){
    add_some_by_random(mt.garr,s);
    auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
    qwen.resize(8*4);for(int i=0;i<4;i++)((uint64_t*)&qwen[0])[i]=17704973477822747953;//11862906452079601044;
    ((uint64_t*)&qwen[0])[3]=v;
  };
  if(bool need_debug=false){
    vector<uint64_t> arr={10718655634695365604,15285484270475338485,10646706089358430435,15250269517162603733,14993245063643207107,13021462144910132437,11931397651803128195,12737770700718220167,13891097911437283831,10885202661334942615,14177810702655558643,4451348087297811172,7906944492203672448,5608731371183225728,2642259179277676672,7486696633507010176,8075527647935679360,66766676723661440,2072788820660943488,9048409811577739136,2885245123565801344,4408883118433902720,7603855964228054150,4336170244670235776,723743699652795008,229765848683329920,5700721759252788096,5998293031694250624,3010178042627491456,1983909116295460736,252871049944592000,1294520640945675392,4243556697580918656,7812391375092321152,515492949867972224,1813386474483702912,5615149979883691906,4230070370550241152,3922366829034216064,3767069945764529280,9838263505978397320,896,7463340112207481728,4751112615510285956,8246654018076697472,5699357595674300544,5928216327139513216,7829630163198508672,3519039080811754880,7266333385006464896,1133853070717368448,4396200552778515840,2415669074382585830,3735011636066787968,1911051347177592544,8972146072450720486,7860789912883303296,5774764210959302784,5628224853152333184,5328152572811897252};
    for(auto v:arr){
      f(v);
    }
    add_finished_game(mt,s,0);
    print_top(mt,s,"S1");
    print_top(mt,s,"S1",16);
  }
  /*
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
  ((int*)&qwen[0])[3]=640;}}*/
  std::random_device rd;
  std::mt19937 rng(rd());
  print_strategy(8331416061813804612);
  /*print_strategy(15114226384578384596);
  print_strategy(16550877896664843427);
  print_strategy(5699357595674300544);//S1
  print_strategy(13236221706665091520);
  print_strategy(10774515300639679988);
  print_strategy(4408883118433902720);//R1
  print_strategy(2830574760533630848);
  print_strategy(10646706089358430435);
  print_strategy(1133853070717368448);//R2
  print_strategy(1813386474483702912);
  print_strategy(5998293031694250624);
  //print_strategy(10653216006750651862);
  for(;;){
    this_thread::sleep_for(1000ms);
  }*/
  //f(peaceful_num);
  cout<<"---\n";
  for (int i = 0; i < 3;i++){g(generous_tft_num);g(opportunistic_num);g(fast_trust_num);}
  for (int i = 0; i < 50000; ++i) {
    Strategy s = generate_reachable_strategy(rng);
    uint64_t e=encode_strategy(s);
    if(i%100==0)std::cout << "Strategy " << i << ": " << e << "\n";
    //print_strategy(e);
    f(e);
  }
  /*
  for(int i=0;i<60;i++){
    if(rand()%3==0)add_some_by_random(mt.garr,encode_strategy(generate_reachable_strategy(rng)));
    add_finished_game(mt,s);
  }*/
  for(int i=0;i<mt_games_per_phase;i++){
    f(encode_strategy(generate_reachable_strategy(rng)));
    bool lastwin=false;
    if(add_finished_game(mt,s,&lastwin)){if(!lastwin){
      mt.garr.pop_back();
      s.phases[0]->uids.erase(mt.garr.size());
    }else{
      cout<<i<<"\n";
    }}
  }
  
  for(auto v:{7384285570446534816ull,3767069945764529280ull,evolved_defect_num,smart_defect_num,640ull,896ull,736ull,6948492336081867392ull,4985045709669101440ull,7463340112207481728ull,
    4751112615510285956ull,8246654018076697472ull,4667762370575748992ull,
    5699357595674300544ull,5928216327139513216ull,5925973217203212672ull,8950679693664478848ull,3711005733351205856ull,
    7829630163198508672ull,3519039080811754880ull,7266333385006464896ull,1133853070717368448ull,4396200552778515840ull,5058139181186697156ull
    }){
    cout<<v<<"\n";
    print_strategy(v);
    f(v);//f(v);f(v);
  }
  for(int i=0;i<mt_games_per_phase;i++){
    if(rand()%3==0)f(encode_strategy(generate_reachable_strategy(rng)));
    add_finished_game(mt,s);
  }
  print_top(mt,s,"S1");
  next_phase(s,"R1");
  play_all_game_iniside_round(mt,s,"R1");
  print_top(mt,s,"R1");
  next_phase(s,"S2");
  for(int i=0;i<mt_games_per_phase;i++){
    if(rand()%30==0)add_some_by_random(mt.garr,s);
    if(add_finished_game(mt,s)){}
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
  print_top(mt,s,"F",6);
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