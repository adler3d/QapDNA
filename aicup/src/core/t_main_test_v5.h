struct t_global_user{
  int user_id;
  string email;
  string sysname;
  string visname;
  string token;
  string created_at;
};
struct t_tracked_info {
    string label;           // "Pure Farmer", "R1-Winner-1", "S2-TOP3-5", ...
    string source_phase;    // фаза, в которой был отмечен
    int source_rank;        // место в той фазе
    map<string, int> phase2rank; // история рангов по фазам
};
struct t_season_coder{
  int user_id;
  vector<t_coder_rec::t_source> sarr;
  string last_submit_time;

  bool is_meta = false;               // true, если это из meta_pack
  string meta_name;                   // например, "Pure Farmer"
  map<string, int> phase2rank;        // "R1" → 42, "R2" → 15, ...
  optional<t_tracked_info> tracked;
};
typedef map<int,double> t_uid2score;
typedef map<int,string> t_uid2source_phase;
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
  t_uid2score uid2games;
  vector<t_game> games;
  t_uid2source_phase uid2source_phase;
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
void mark_top_n_as_tracked(t_season& season, const string& phase_id, int top_n = 6) {
    const t_phase* p = find_phase(season, phase_id);
    if (!p) return;

    // Получаем ранжированный список
    vector<pair<double, int>> ranked;
    for (int uid : p->uids) {
        double score = p->uid2score.count(uid) ? p->uid2score.at(uid) : 1500.0;
        ranked.emplace_back(-score, uid);
    }
    sort(ranked.begin(), ranked.end());

    int n = min(top_n, (int)ranked.size());
    for (int i = 0; i < n; ++i) {
        int uid = ranked[i].second;
        auto& coder = season.uid2scoder[uid];

        // Не перезаписываем существующую пометку (например, meta_pack)
        if (coder.tracked.has_value()) continue;

        coder.tracked = t_tracked_info{
            /*.label =*/ phase_id + "-TOP"+/*to_string(top_n) + "-" + */to_string(i+1),
            /*.source_phase =*/ phase_id,
            /*.source_rank =*/ i+1,
            /*.phase2rank =*/ {}
        };
    }
}
void record_all_tracked_ranks(t_season& season, const string& phase_id) {
    const t_phase* p = find_phase(season, phase_id);
    if (!p) return;

    // Собираем текущие ранги
    vector<pair<double, int>> ranked;
    for (int uid : p->uids) {
        double score = p->uid2score.count(uid) ? p->uid2score.at(uid) : 1500.0;
        ranked.emplace_back(-score, uid);
    }
    sort(ranked.begin(), ranked.end());

    unordered_map<int, int> uid_to_rank;
    for (size_t i = 0; i < ranked.size(); ++i) {
        uid_to_rank[ranked[i].second] = (int)i + 1;
    }

    // Обновляем историю для всех отслеживаемых
    for (auto& [uid, coder] : season.uid2scoder) {
        if (!coder.tracked.has_value()) continue;

        if (uid_to_rank.count(uid)) {
            coder.tracked->phase2rank[phase_id] = uid_to_rank[uid];
        } else {
            coder.tracked->phase2rank[phase_id] = -1; // вылетел
        }
    }
}
void print_all_tracked_summary(const t_main_test& mt, const t_season& season) {
    cout << "\n=== ALL TRACKED STRATEGIES SUMMARY ===\n";
    for (auto& [uid, coder] : season.uid2scoder) {
        if (!coder.tracked.has_value()) continue;

        auto& t = coder.tracked.value();
        cout << t.label << " (UID=" << uid << ")\n";
        // Печатаем историю по всем фазам
        for (auto& phase : {"S1", "R1", "S2", "R2", "SF", "F"}) {
            if (t.phase2rank.count(phase)) {
                int r = t.phase2rank.at(phase);
                cout << "  " << phase << ": " << (r == -1 ? "out" : "#" + to_string(r)) << "\n";
            }
        }
        cout << "---\n";
    }
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
        {"S1", "sandbox", 60, {}},
        {"R1", "round",   60, {{"S1", 900}}},
        {"S2", "sandbox", 60, {}},
        {"R2", "round",   60, {{"R1", 300}, {"S2", 60}}},
        {"SF", "sandbox", 60, {}},
        {"F",  "round",   60, {{"R2", 50},  {"SF", 10}}},
        {"S",  "sandbox", 60, {}}
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
                target->uid2source_phase[uid] = rule.from_phase_id;
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
std::vector<double> simulate_arena_v9(const std::vector<Strategy>& strategies,bool debug);
std::vector<double> simulate_arena(const std::vector<uint64_t>& strategies,bool debug=false) {
  return simulate_arena_v9((std::vector<Strategy>&)strategies,debug);
}

std::string print_strategy_v9(uint64_t u) {
    std::ostringstream oss;
    oss << "St | C-action | D-action | Move     | Act  \n";
    oss << "---------------------------------------------\n";
    for (int i = 0; i < 8; ++i) {
        uint8_t b = (u >> (8 * i)) & 0xFF;
        bool stayC = (b >> 6) & 1;
        bool stayD = (b >> 7) & 1;
        bool move = (b >> 5) & 1;
        bool atk = (b >> 4) & 1;
        bool j2 = (b >> 3) & 1;
        bool j1 = (b >> 2) & 1;

        auto action_str = [&](bool stay, bool j2, bool j1) -> std::string {
            if (stay) return "stay";
            if (j2) return "+2";
            if (j1) return "+1";
            return "stay";
        };

        oss << " " << i << " | " << action_str(stayC, j2, j1)
            << "       | " << action_str(stayD, j2, j1)
            << "       | " << (move ? "center" : "away ")
            << "  | " << (atk ? "atk" : "hid") << "\n";
    }
    return oss.str();
}
// Описание состояния
struct StateRule {
    bool stay_on_D;
    bool stay_on_C;
    bool move_to_center;
    bool attack_mode;
    bool jump_2;
    bool jump_1;
};
uint64_t build_strategy(const std::array<StateRule, 8>& rules) {
    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) {
        uint8_t b = 0;
        if (rules[i].stay_on_D) b |= 0x80;
        if (rules[i].stay_on_C) b |= 0x40;
        if (rules[i].move_to_center) b |= 0x20;
        if (rules[i].attack_mode) b |= 0x10;
        if (rules[i].jump_2) b |= 0x08;
        if (rules[i].jump_1) b |= 0x04;
        u |= static_cast<uint64_t>(b) << (8 * i);
    }
    return u;
}
std::string print_strategy_v8(uint64_t u) {
    std::ostringstream oss;
    oss << "State | Move          | Action   | Next\n";
    oss << "----------------------------------------\n";
    
    for (int i = 0; i < 8; ++i) {
        uint8_t byte = (u >> (8 * i)) & 0xFF;
        bool move_to_center = (byte >> 7) & 1;
        bool attack_mode = (byte >> 6) & 1;
        int next_state = (byte & 63) % 8;

        std::string move_str = move_to_center ? "to_center" : "from_enemy";
        std::string act_str = attack_mode ? "attack" : "hide";

        oss << "  " << i << "   | " << move_str << " | " << act_str << "  | " << next_state << "\n";
    }
    return oss.str();
}
std::string print_strategy_v7(const uint64_t& u) {
    const char actions[4] = {'G', 'H', 'S', '?'};
    std::string s = "Actions: ";
    for (int i = 0; i < 6; ++i) {
        uint8_t byte = (u >> (8 * i)) & 0xFF;
        int act = (byte >> 6) & 3;
        if (act > 2) act = 0;
        s += actions[act];
        //if (i < 5) s += '';
    }
    uint8_t seed = (u >> 56) & 0xFF;
    s += "|" + std::to_string(seed);
    return s;
}
void print_strategy(const uint64_t&u){
  cout<<print_strategy_v9(u)<<"\n";
}
void print_strategy_old(const uint64_t&u) {
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
#include <cmath>
#include <algorithm>

std::vector<double> simulate_arena_v9(const std::vector<Strategy>& strategies,bool) {
    struct Agent {
        int x = 0, y = 0;          // ×10 for precision
        int hp = 100;
        int energy = 100;
        int state = 0;
        bool alive = true;
        int ticks_survived = 0;
        int damage_dealt = 0;
        bool attacked_this_tick = false; // для определения "D"
    };
    const int N = static_cast<int>(strategies.size());
    if (N < 2) return std::vector<double>(N, 0.0);

    std::vector<double> total_score(N, 0.0);
    const int BATTLES = 1000;
    const int PLAYERS_PER_BATTLE = std::min(16, N);
    std::mt19937 rng(2025);

    for (int b = 0; b < BATTLES; ++b) {
        // Выбираем случайных игроков
        std::vector<int> indices(N);
        for (int i = 0; i < N; ++i) indices[i] = i;
        std::shuffle(indices.begin(), indices.end(), rng);
        indices.resize(PLAYERS_PER_BATTLE);

        std::vector<Agent> agents(PLAYERS_PER_BATTLE);
        std::vector<Strategy> strat(PLAYERS_PER_BATTLE);
        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            agents[i].x = (std::rand() % 200 - 100) * 10;
            agents[i].y = (std::rand() % 200 - 100) * 10;
            agents[i].state = 0;
            agents[i].attacked_this_tick = false;
            strat[i] = strategies[indices[i]];
        }

        int R = 1000; // radius ×10
        int tick = 0;
        int alive_count = PLAYERS_PER_BATTLE;

        while (alive_count > 1 && tick < 200) {
            if (tick % 10 == 0 && R > 100) R -= 100;

            // Сброс флагов атаки
            for (auto& a : agents) {
                if (a.alive) a.attacked_this_tick = false;
            }

            // Фаза движения и атаки
            for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
                if (!agents[i].alive) continue;
                auto& a = agents[i];
                a.ticks_survived++;
                uint8_t state_byte = strat[i][a.state];

                bool move_to_center = (state_byte >> 5) & 1;
                bool attack_mode = (state_byte >> 4) & 1;

                // Найти ближайшего врага
                int closest_dist_sq = 100000000;
                int dx_to_closest = 0, dy_to_closest = 0;
                bool found_enemy = false;
                for (int j = 0; j < PLAYERS_PER_BATTLE; ++j) {
                    if (i == j || !agents[j].alive) continue;
                    int dx = a.x - agents[j].x;
                    int dy = a.y - agents[j].y;
                    int dist_sq = dx*dx + dy*dy;
                    if (dist_sq < closest_dist_sq) {
                        closest_dist_sq = dist_sq;
                        dx_to_closest = dx;
                        dy_to_closest = dy;
                        found_enemy = true;
                    }
                }

                // Движение
                int move_dx = 0, move_dy = 0;
                if (move_to_center) {
                    move_dx = -a.x;
                    move_dy = -a.y;
                } else if (found_enemy && closest_dist_sq < 90000) {
                    move_dx = dx_to_closest;
                    move_dy = dy_to_closest;
                }

                if (move_dx != 0 || move_dy != 0) {
                    int len = static_cast<int>(std::sqrt(move_dx*move_dx + move_dy*move_dy));
                    if (len > 0 && a.energy >= 1) {
                        int step = std::min(50, len);
                        a.x += (move_dx * 50) / len;
                        a.y += (move_dy * 50) / len;
                        a.energy -= 1;
                    }
                }

                // Пассивное притяжение к центру
                int drift = 2 + (1000 - R) / 100;
                a.x -= (a.x * drift) / 1000;
                a.y -= (a.y * drift) / 1000;

                // Атака или укрытие
                if (attack_mode) {
                    for (int j = 0; j < PLAYERS_PER_BATTLE; ++j) {
                        if (i == j || !agents[j].alive) continue;
                        int dx = a.x - agents[j].x;
                        int dy = a.y - agents[j].y;
                        if (dx*dx + dy*dy <= 10000) {
                            agents[j].hp -= 10;
                            a.damage_dealt += 10;
                            a.attacked_this_tick = true;
                            if (a.energy >= 2) a.energy -= 2;
                            break; // одна атака за тик
                        }
                    }
                } else {
                    if (a.energy >= 1) a.energy -= 1;
                }

                // Урон от зоны
                if (a.x*a.x + a.y*a.y > R*R) {
                    a.hp -= 5;
                }

                if (a.hp <= 0) {
                    a.alive = false;
                    alive_count--;
                }
            }

            // Фаза обновления состояний
            for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
                if (!agents[i].alive) continue;
                auto& a = agents[i];
                uint8_t state_byte = strat[i][a.state];

                bool stay_on_D = (state_byte >> 7) & 1;
                bool stay_on_C = (state_byte >> 6) & 1;
                bool jump_2 = (state_byte >> 3) & 1;
                bool jump_1 = (state_byte >> 2) & 1;

                // Определяем "действие оппонента" как: был ли хоть один атакующий враг?
                bool opponent_attacked = false;
                for (int j = 0; j < PLAYERS_PER_BATTLE; ++j) {
                    if (i == j || !agents[j].alive) continue;
                    // Берём ближайшего или любого — для простоты любого
                    opponent_attacked = agents[j].attacked_this_tick;
                    break; // достаточно одного
                }

                bool stay = false;
                if (opponent_attacked) {
                    stay = stay_on_D;
                } else {
                    stay = stay_on_C;
                }

                if (!stay) {
                    if (jump_2) {
                        a.state = (a.state + 2) % 8;
                    } else if (jump_1) {
                        a.state = (a.state + 1) % 8;
                    }
                    // иначе остаёмся (но stay=false и нет jump → остаёмся)
                }
                // если stay=true → состояние не меняется
            }

            tick++;
        }

        // Подсчёт очков
        double total_raw = 0;
        std::vector<double> battle_score(PLAYERS_PER_BATTLE, 0.0);
        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            double score = agents[i].ticks_survived + agents[i].damage_dealt * 0.1;
            battle_score[i] = score;
            total_raw += score;
        }
        double avg = total_raw / PLAYERS_PER_BATTLE;
        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            total_score[indices[i]] += (battle_score[i] - avg);
        }
    }

    return total_score;
}
std::vector<double> simulate_arena_v8(const std::vector<Strategy>& strategies,bool) {
    struct Agent {
        int x = 0, y = 0;          // ×10 for precision
        int hp = 100;
        int energy = 100;
        int state = 0;
        bool alive = true;
        double raw_score = 0.0;
        int damage_dealt = 0;
        int ticks_survived = 0;
    };
    const int N = static_cast<int>(strategies.size());
    if (N < 2) return std::vector<double>(N, 0.0);

    std::vector<double> total_score(N, 0.0);
    const int BATTLES = 1000;
    const int PLAYERS_PER_BATTLE = std::min(16, N);

    for (int b = 0; b < BATTLES; ++b) {
        // Выбираем случайных игроков
        std::vector<int> indices;
        for (int i = 0; i < N; ++i) indices.push_back(i);
        std::shuffle(indices.begin(), indices.end(), std::mt19937(b));
        indices.resize(PLAYERS_PER_BATTLE);

        std::vector<Agent> agents(PLAYERS_PER_BATTLE);
        std::vector<Strategy> strat(PLAYERS_PER_BATTLE);
        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            agents[i].x = (std::rand() % 200 - 100) * 10; // [-1000, 1000]
            agents[i].y = (std::rand() % 200 - 100) * 10;
            agents[i].state = 0;
            strat[i] = strategies[indices[i]];
        }

        int R = 1000; // radius ×10
        int tick = 0;
        int alive_count = PLAYERS_PER_BATTLE;

        while (alive_count > 1 && tick < 200) {
            if (tick % 10 == 0 && R > 100) R -= 100; // сужение каждые 10 тиков

            // Обновляем агентов
            for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
                if (!agents[i].alive) continue;
                agents[i].ticks_survived++;

                auto& a = agents[i];
                uint8_t state_byte = strat[i][a.state];
                bool move_to_center = (state_byte >> 7) & 1;
                bool attack_mode = (state_byte >> 6) & 1;
                int next_state = (state_byte & 63) % 8;

                // Найти ближайшего врага
                int closest_dist = 1000000;
                int dx_to_closest = 0, dy_to_closest = 0;
                bool found_enemy = false;
                for (int j = 0; j < PLAYERS_PER_BATTLE; ++j) {
                    if (i == j || !agents[j].alive) continue;
                    int dx = a.x - agents[j].x;
                    int dy = a.y - agents[j].y;
                    int dist_sq = dx*dx + dy*dy;
                    if (dist_sq < closest_dist) {
                        closest_dist = dist_sq;
                        dx_to_closest = dx;
                        dy_to_closest = dy;
                        found_enemy = true;
                    }
                }

                // Желаемое движение
                int move_dx = 0, move_dy = 0;
                if (move_to_center) {
                    move_dx = -a.x;
                    move_dy = -a.y;
                } else if (found_enemy && closest_dist < 90000) { // радиус 300 (30×10)
                    move_dx = dx_to_closest;
                    move_dy = dy_to_closest;
                }

                // Нормализация и шаг
                int move_len_sq = move_dx*move_dx + move_dy*move_dy;
                if (move_len_sq > 0 && a.energy >= 1) {
                    int len = static_cast<int>(std::sqrt(move_len_sq));
                    int step = std::min(50, len); // max 5.0 → ×10 = 50
                    a.x += (move_dx * 50) / len;
                    a.y += (move_dy * 50) / len;
                    a.energy -= 1;
                }

                // Пассивное притяжение к центру (даже без энергии)
                int drift_strength = 2 + (1000 - R) / 100; // усиливается при сужении
                a.x -= (a.x * drift_strength) / 1000;
                a.y -= (a.y * drift_strength) / 1000;

                // Атака или укрытие
                bool attacked = false;
                if (attack_mode) {
                    for (int j = 0; j < PLAYERS_PER_BATTLE; ++j) {
                        if (i == j || !agents[j].alive) continue;
                        int dx = a.x - agents[j].x;
                        int dy = a.y - agents[j].y;
                        if (dx*dx + dy*dy <= 10000) { // радиус 100 (10×10)
                            agents[j].hp -= 10;
                            a.damage_dealt += 10;
                            attacked = true;
                        }
                    }
                    if (attacked && a.energy >= 2) a.energy -= 2;
                } else {
                    // hide: в этом тике неуязвим — реализуем через флаг позже
                    if (a.energy >= 1) a.energy -= 1;
                }

                // Урон от зоны
                int dist_to_center_sq = a.x*a.x + a.y*a.y;
                if (dist_to_center_sq > R*R) {
                    a.hp -= 5;
                }

                a.state = next_state;

                // Смерть
                if (a.hp <= 0) {
                    a.alive = false;
                    alive_count--;
                }
            }

            tick++;
        }

        // Подсчёт очков
        double total_raw = 0;
        std::vector<double> battle_score(PLAYERS_PER_BATTLE, 0.0);
        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            double score = agents[i].ticks_survived + agents[i].damage_dealt * 0.1;
            battle_score[i] = score;
            total_raw += score;
        }
        double avg = total_raw / PLAYERS_PER_BATTLE;
        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            total_score[indices[i]] += (battle_score[i] - avg);
        }
    }

    return total_score;
}
// Действия
enum Action { GATHER = 0, HOARD = 1, STEAL = 2 };
std::vector<double> simulate_arena_v7(const std::vector<Strategy>& strategies,bool debug) {
    const int n = static_cast<int>(strategies.size());
    if (n < 4) return std::vector<double>(n, 0.0);

    std::vector<int64_t> total_score(n, 0);
    const int PLAYERS_PER_BATTLE = 4;
    const int TICKS = 6;
    const int BATTLES = (n / PLAYERS_PER_BATTLE) * 100; // много боёв

    // Генератор для глобального порядка (но детерминированный)
    std::mt19937 global_rng(12345);

    // Создаём список индексов и перемешиваем
    std::vector<int> indices(n);
    for (int i = 0; i < n; ++i) indices[i] = i;

    for (int b = 0; b < BATTLES; ++b) {
        std::shuffle(indices.begin(), indices.end(), global_rng);

        for (int i = 0; i <= n - PLAYERS_PER_BATTLE; i += PLAYERS_PER_BATTLE) {
            std::array<int, PLAYERS_PER_BATTLE> group;
            for (int p = 0; p < PLAYERS_PER_BATTLE; ++p) {
                group[p] = indices[i + p];
            }

            // Состояния ресурсов
            std::array<int, PLAYERS_PER_BATTLE> resources = {0};
            int pool = 12;

            // Локальный генератор для краж (семя из стратегий)
            uint32_t seed = 0;
            for (int p = 0; p < PLAYERS_PER_BATTLE; ++p) {
                seed ^= strategies[group[p]][7]; // XOR всех байтов 7
            }
            std::mt19937 steal_rng(seed);

            for (int t = 0; t < TICKS; ++t) {
                std::array<Action, PLAYERS_PER_BATTLE> actions;
                // Выбор действия
                for (int p = 0; p < PLAYERS_PER_BATTLE; ++p) {
                    uint8_t byte = strategies[group[p]][t % 6]; // первые 6 байт = действия
                    actions[p] = static_cast<Action>((byte >> 6) & 3);
                    if (actions[p] > STEAL) actions[p] = GATHER;
                }

                // Фаза Gather
                int gather_count = 0;
                for (int p = 0; p < PLAYERS_PER_BATTLE; ++p) {
                    if (actions[p] == GATHER && pool > 0) {
                        gather_count++;
                    }
                }
                int taken = std::min(gather_count, pool);
                pool -= taken;
                // Раздаём Gather (в порядке игроков)
                int given = 0;
                for (int p = 0; p < PLAYERS_PER_BATTLE && given < taken; ++p) {
                    if (actions[p] == GATHER) {
                        resources[p]++;
                        given++;
                    }
                }

                // Фаза Steal
                for (int p = 0; p < PLAYERS_PER_BATTLE; ++p) {
                    if (actions[p] == STEAL) {
                        // Выбираем жертву
                        std::vector<int> candidates;
                        for (int q = 0; q < PLAYERS_PER_BATTLE; ++q) {
                            if (q != p && actions[q] != HOARD && resources[q] > 0) {
                                candidates.push_back(q);
                            }
                        }
                        if (!candidates.empty()) {
                            int victim = candidates[steal_rng() % candidates.size()];
                            resources[victim]--;
                            resources[p]++;
                        }
                    }
                }
            }

            // Начисляем очки (нуль-суммово: вычитаем среднее)
            int64_t sum = 0;
            for (int p = 0; p < PLAYERS_PER_BATTLE; ++p) {
                sum += resources[p];
            }
            double avg = static_cast<double>(sum) / PLAYERS_PER_BATTLE;
            for (int p = 0; p < PLAYERS_PER_BATTLE; ++p) {
                total_score[group[p]] += (resources[p] - avg);
            }
        }
    }

    std::vector<double> result(n);
    for (int i = 0; i < n; ++i) {
        result[i] = static_cast<double>(total_score[i]);
    }
    return result;
}
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
bool add_finished_game(const t_main_test& mt, t_season& season, bool* plastwin = nullptr, bool debug = false, bool need_shuffle = true) {
    t_phase* active = season.phases[season.current_phase_idx].get();
    if (active->uids.empty()) return false;
    if ((int)active->uids.size() < active->num_players) return false;

    // === Шаг 1: найдём минимальное число игр среди участников ===
    int min_games = INT_MAX;
    for (int uid : active->uids) {
        min_games = min(min_games, (int)active->uid2games[uid]);
    }


    // === Шаг 1: построим список (games_played, uid) и отсортируем по возрастанию ===
    vector<pair<int, int>> uid_by_games; // (games, uid)
    for (int uid : active->uids) {
        int games = active->uid2games[uid]; // если нет — создастся как 0 (OK)
        uid_by_games.emplace_back(games, uid);
    }
    sort(uid_by_games.begin(), uid_by_games.end());

    // === Шаг 2: возьмём первых num_players с наименьшим числом игр ===
    vector<int> players;
    players.reserve(active->num_players);
    for (int i = 0; i < active->num_players; ++i) {
        players.push_back(uid_by_games[i].second);
    }

    // Опционально: перемешать порядок (не влияет на баланс, но для разнообразия)
    if (need_shuffle) {
        shuffle(players.begin(), players.end(), default_random_engine(rand()));
    }
    // === Шаг 3: симуляция ===
    string&pid = active->phase_id;
    int R = (pid == "R1") ? 1 : (pid == "R2" ? 2 : (pid == "F" ? 3 : 0));

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

    // === Шаг 4: сохраняем игру ===
    t_game game;
    game.gd.arr.resize(players.size());
    t_finished_game fg;
    fg.game_id = game.gd.arr.size();
    fg.tick = 1000;
    fg.slot2score = r;
    fg.slot2ms.assign(players.size(), 100.0); // placeholder

    for (size_t i = 0; i < players.size(); ++i) {
        game.gd.arr[i].coder = mt.garr[players[i]].sysname;
    }

    game.fg = fg;
    game.status = "finished";
    game.finished_at = qap_time();
    active->games.push_back(game);

    // === Шаг 5: обновляем счётчики и рейтинг ===
    for (size_t i = 0; i < players.size(); ++i) {
        int uid = players[i];
        active->uid2games[uid]++; // ← ключевое: считаем игры

        double old_score = active->uid2score.count(uid) ? active->uid2score[uid] : 1500.0;
        active->uid2score[uid] = old_score + fg.slot2score[i];
    }

    return true;
}
bool add_finished_game_v0(const t_main_test&mt,t_season& season,bool*plastwin=nullptr,bool debug=false,bool need_shufle=true) {
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
int mt_games_per_phase=1000;//2*64000;
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
using Strategy = std::array<uint8_t, 8>;
std::uniform_int_distribution<uint32_t> bool_distr{0,1};
std::uniform_int_distribution<uint32_t> byte_dist(0, 255);
Strategy generate_reachable_strategy(std::mt19937& rng,int ATTEMPTS=100) {
    auto r=rnd();
    return (const Strategy&)r;
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
uint64_t adaptive_farmer = 
    (0ULL) |          // t0: G
    (0x40ULL << 8) |  // t1: H
    (0x80ULL << 16) | // t2: S
    (0ULL << 24) |    // t3: G
    (0ULL << 32) |    // t4: G
    (0x40ULL << 40) | // t5: H
    (0ULL << 48) |    // unused
    (123ULL << 56);   // seed
uint64_t make_strategy(char t0, char t1, char t2, char t3, char t4, char t5, uint8_t seed = 0) {
    uint64_t u = 0;
    auto a = [&](char c, int shift) {
        uint8_t v = (c == 'G' ? 0 : (c == 'H' ? 1 : 2)) << 6;
        u |= static_cast<uint64_t>(v) << shift;
    };
    a(t0, 0);
    a(t1, 8);
    a(t2, 16);
    a(t3, 24);
    a(t4, 32);
    a(t5, 40);
    u |= static_cast<uint64_t>(seed) << 56;
    return u;
}
void record_meta_ranks(t_season& season, const string& phase_id) {
  const t_phase* p = find_phase(season, phase_id);
  if (!p) return;

  // Собираем рейтинги
  vector<pair<double, int>> ranked;
  for (int uid : p->uids) {
      double score = p->uid2score.count(uid) ? p->uid2score.at(uid) : 1500.0;
      ranked.emplace_back(-score, uid);
  }
  sort(ranked.begin(), ranked.end());

  // Записываем ранг для каждого meta-участника
  for (size_t i = 0; i < ranked.size(); ++i) {
      int uid = ranked[i].second;
      auto it = season.uid2scoder.find(uid);
      if (it != season.uid2scoder.end() && it->second.is_meta) {
          it->second.phase2rank[phase_id] = (int)i + 1; // места с 1
      }
  }

  // Также запишем ранг для тех, кто НЕ в фазе (вылетел) → ранг = -1
  for (auto& [uid, coder] : season.uid2scoder) {
      if (coder.is_meta && coder.phase2rank.count(phase_id) == 0) {
          coder.phase2rank[phase_id] = -1; // вылетел
      }
  }
}
void print_meta_top16(const t_main_test& mt, const t_season& season, const string& current_phase) {
    const t_phase* p = find_phase(season, current_phase);
    if (!p /*|| p->type != "round"*/) return;

    // Собираем текущие ранги
    vector<pair<double, int>> ranked;
    for (int uid : p->uids) {
        double score = p->uid2score.count(uid) ? p->uid2score.at(uid) : 1500.0;
        ranked.emplace_back(-score, uid);
    }
    sort(ranked.begin(), ranked.end());

    unordered_map<int, int> uid_to_current_rank;
    for (size_t i = 0; i < ranked.size(); ++i) {
        uid_to_current_rank[ranked[i].second] = (int)i + 1;
    }

    // Собираем meta-стратегии
    vector<tuple<int, int, string, string, int>> meta_list;
    // (curr_rank, uid, name, source_phase, source_rank)

    for (auto& [uid, coder] : season.uid2scoder) {
        if (!coder.is_meta) continue;
        if (p->uids.count(uid) == 0) continue;

        int curr_rank = uid_to_current_rank[uid];
        string source_phase = p->uid2source_phase.count(uid) 
            ? p->uid2source_phase.at(uid) 
            : "unknown";

        // Получим ранг в фазе-источнике
        int source_rank = -1;
        if (coder.phase2rank.count(source_phase)) {
            source_rank = coder.phase2rank.at(source_phase);
        }

        meta_list.emplace_back(curr_rank, uid, coder.meta_name, source_phase, source_rank);
    }

    sort(meta_list.begin(), meta_list.end());

    cout << "\n=== META STRATEGIES TOP (current: " << current_phase << ") ===\n";
    int n = meta_list.size();
    for (int i = 0; i < n; ++i) {
        auto [curr, uid, name, src_phase, src_rank] = meta_list[i];
        cout << (i+1) << ". " << name 
             << " | curr: #" << curr 
             << " | from " << src_phase << ": " 
             << (src_rank == -1 ? "out" : "#" + to_string(src_rank))
             << " (UID=" << uid << ")\n";
    }
}
void print_meta_summary(const t_season& season) {
    cout << "\n=== META STRATEGY FULL HISTORY ===\n";
    for (auto& [uid, coder] : season.uid2scoder) {
        if (!coder.is_meta) continue;
        cout << coder.meta_name << " (UID=" << uid << "):\n";
        for (auto& phase : {"S1", "R1", "S2", "R2", "SF", "F"}) {
            if (coder.phase2rank.count(phase)) {
                int r = coder.phase2rank.at(phase);
                cout << "  " << phase << ": " << (r == -1 ? "out" : "#" + to_string(r)) << "\n";
            }
        }
        cout << "---\n";
    }
}
void record_all_ranks(t_season& season, const string& phase_id) {
    const t_phase* p = find_phase(season, phase_id);
    if (!p) return;

    // 1. Собираем ранги для участников текущей фазы
    vector<pair<double, int>> ranked;
    for (int uid : p->uids) {
        double score = p->uid2score.count(uid) ? p->uid2score.at(uid) : 1500.0;
        ranked.emplace_back(-score, uid);
    }
    sort(ranked.begin(), ranked.end());

    unordered_map<int, int> uid_to_rank;
    for (size_t i = 0; i < ranked.size(); ++i) {
        uid_to_rank[ranked[i].second] = (int)i + 1;
    }

    // 2. Обновляем ВСЕХ участников сезона
    for (auto& [uid, coder] : season.uid2scoder) {
        if (uid_to_rank.count(uid)) {
            coder.phase2rank[phase_id] = uid_to_rank[uid];
        } else {
            // Не в фазе → либо не прошёл отбор, либо не участвовал
            coder.phase2rank[phase_id] = -1;
        }
    }
}
void print_top_with_context(const t_main_test& mt, const t_season& season, const string& phase_id, int n = 16) {
    const t_phase* p = find_phase(season, phase_id);
    vector<pair<double, int>> ranked;
    for (int uid : p->uids) {
        double score = p->uid2score.count(uid) ? p->uid2score.at(uid) : 1500.0;
        ranked.emplace_back(-score, uid);
    }
    sort(ranked.begin(), ranked.end());

    cout << "\n=== TOP " << n << " in " << phase_id << " ===\n";
    for (int i = 0; i < min(n, (int)ranked.size()); ++i) {
        int uid = ranked[i].second;
        auto& coder = season.uid2scoder.at(uid);

        // Определим, откуда пришёл (если это раунд)
        string source = "n/a";
        if (p->type == "round" && p->uid2source_phase.count(uid)) {
            source = p->uid2source_phase.at(uid);
        }

        // Был ли в meta_pack? (можно хранить отдельно или по шаблону имени)
        string tag = "";
        // ... если нужно ...

        cout << (i+1) << ". UID=" << uid 
             << " | from " << source
             << " | prev_rank: " << (coder.phase2rank.count(source) ? to_string(coder.phase2rank.at(source)) : "?")
             << " " << tag << "\n";
    }
}
uint64_t ada=make_strategy('G', 'G', 'H', 'S', 'S', 'H', 42);
uint64_t meta_pack[8] = {
    make_strategy('G','G','G','G','G','G', 1),   // 1. Pure Farmer
    make_strategy('G','G','G','G','G','H', 2),   // 2. Late Farmer
    make_strategy('G','G','S','S','S','H', 3),   // 3. Opportunistic Thief
    make_strategy('G','S','S','S','S','S', 4),   // 4. Aggressive Thief
    make_strategy('G','H','G','G','G','H', 5),   // 5. Defensive Farmer
    make_strategy('G','S','G','S','G','S', 6),   // 6. Hybrid Swarmer
    make_strategy('G','H','H','H','H','H', 7),   // 7. Early Hoarder
    make_strategy('G','G','S','S','G','H', 8)    // 8. Meta Mimic (ТОП-1 clone)
};
struct Rule {
    bool move_to_center;   // true = toward center, false = away from closest enemy
    bool attack_mode;      // true = attack if close, false = hide
    int next_state;        // 0–7
};

using StrategyDesc = std::array<Rule, 8>;

uint64_t build_strategy(const StrategyDesc& desc) {
    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) {
        const Rule& r = desc[i];
        uint8_t byte = 0;
        if (r.move_to_center) byte |= (1 << 7);
        if (r.attack_mode)    byte |= (1 << 6);
        byte |= (r.next_state & 7); // 3 bits достаточно, но формат использует 6
        u |= static_cast<uint64_t>(byte) << (8 * i);
    }
    return u;
}
// Создаём "Тактика": первые 5 состояний — прячься и уходи, потом — атакуй и иди к центру
StrategyDesc tactician = {{
    {false, false, 1}, // 0: from_enemy, hide → 1
    {false, false, 2}, // 1: from_enemy, hide → 2
    {false, false, 3}, // 2: from_enemy, hide → 3
    {false, false, 4}, // 3: from_enemy, hide → 4
    {false, false, 5}, // 4: from_enemy, hide → 5
    {true,  true,  5}, // 5: to_center, attack → 5
    {true,  true,  5}, // 6: to_center, attack → 5
    {true,  true,  5}  // 7: to_center, attack → 5
}};
uint64_t grok() {
    std::array<StateRule, 8> rules = {{
        // state 0: старт → бежим в центр, атакуем, прыгаем в 1
        {false, false, true,  true,  false, true},  // stay_D=0, stay_C=0, center, attack, jump2=0, jump1=1
        // state 1: агрессия → к врагу, атакуем, прыгаем в 2
        {false, false, false, true,  false, true},  // stay нет, к врагу (не center), attack, jump1
        // state 2: финальная атака → к врагу, атакуем, прыгаем в 0
        {false, false, false, true,  true,  false}, // jump2 → +2 → 0
        // остальные — заглушки (не используются)
        {false, false, false, false, false, false},
        {false, false, false, false, false, false},
        {false, false, false, false, false, false},
        {false, false, false, false, false, false},
        {false, false, false, false, false, false}
    }};
    return build_strategy(rules);
}
uint64_t perplexity() {
    std::array<StateRule, 8> ai = {{
        {false, true,  true,  true,  false, true},  // счёт: идти к центру + атаковать + переход в след. состояние
        {true,  false, false, true,  false, true},  // оставаться при атаке, атаковать
        {false, true,  true,  true,  true,  false}, // идти к центру + атаковать + прыгать на 2 состояния
        {true,  true,  true,  false, false, false}, // сохраняться, идти к центру, не атаковать
        {true,  true,  false, true,  false, false}, // оставаться, атаковать
        {true,  true,  false, true,  true,  false}, // оставаться, атаковать, прыжок на 2
        {true,  true,  true,  false, false, true},  // оставаться, идти к центру, прыжок на 1
        {true,  true,  false, false, false, false}  // оставаться, не атаковать
    }};
    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) {
        uint8_t b = 0;
        if (ai[i].stay_on_D) b |= 0x80;
        if (ai[i].stay_on_C) b |= 0x40;
        if (ai[i].move_to_center) b |= 0x20;
        if (ai[i].attack_mode) b |= 0x10;
        if (ai[i].jump_2) b |= 0x08;
        if (ai[i].jump_1) b |= 0x04;
        u |= static_cast<uint64_t>(b) << (8 * i);
    }
    return u;
}
uint64_t deepseek(){
    std::array<StateRule, 8> ai = {{
        // state 0: Агрессивный старт - двигаемся к центру и атакуем
        {false, true, true, true, false, false},
        
        // state 1: Преследование - двигаемся к врагам и атакуем  
        {false, false, false, true, false, true},
        
        // state 2: Оборона при атаке - остаемся на D, двигаемся к центру, не атакуем
        {true, false, true, false, true, false},
        
        // state 3: Агрессивный ответ - атакуем при возможности
        {false, true, false, true, false, true},
        
        // state 4: Тактическое отступление - к центру, экономим энергию
        {true, true, true, false, true, false},
        
        // state 5: Фланговая атака - двигаемся к врагам, атакуем
        {false, false, false, true, false, false},
        
        // state 6: Контроль территории - в центре, готовы к атаке
        {true, true, true, true, false, true},
        
        // state 7: Финишный режим - максимально агрессивный
        {false, false, true, true, true, false}
    }};
    return build_strategy(ai);
}
uint64_t gemini() {
    std::array<StateRule, 8> ai = {{
        // State 0: Attack, Move to center, Next state is 1
        {false, false, true, true, false, true},
        // State 1: Attack, Move to center, Next state is 2
        {false, false, true, true, false, true},
        // State 2: Defend/Energy, Move to center, Next state is 3
        {false, false, true, false, false, true},
        // State 3: Defend/Energy, Move to center, Next state is 4
        {false, false, true, false, false, true},
        // State 4: Defend/Energy, Move to center, Next state is 5
        {false, false, true, false, false, true},
        // State 5: Defend/Energy, Move to center, Next state is 6
        {false, false, true, false, false, true},
        // State 6: Defend/Energy, Move to center, Next state is 7
        {false, false, true, false, false, true},
        // State 7: Defend/Energy, Move to center, Next state is 0
        {false, false, true, false, false, true}
    }};
    
    // Внимание: (uint64_t&)ai - это не стандартный способ конвертации std::array<StateRule, 8> 
    // в uint64_t. Мы используем предоставленную функцию build_strategy. 
    // Код в main использует (std::vector<Strategy>&)ais, предполагая, 
    // что Strategy = std::array<uint8_t, 8> и что структура StateRule 
    // скомпилирована в 8 байт (8*1). build_strategy выполняет нужную нам кодировку.
    
    // В исходном коде используется (uint64_t&)ai, что эквивалентно 
    // reinterpret_cast<uint64_t&>(ai). Это опасно и зависит от выравнивания, 
    // но если мы должны следовать логике main, 
    // нам нужно использовать build_strategy, а затем, возможно, reinterpret_cast.
    // Однако, build_strategy возвращает uint64_t, что безопаснее.
    
    // Используем build_strategy для корректного кодирования:
    return build_strategy(ai);
}
uint64_t gpt5_th() {
    std::array<StateRule, 8> rules{};

    // Состояние 0: активно в центр + атака; при первом "D" (кто-то атаковал) прыгаем на +1
    rules[0] = {
        /*stay_on_D=*/false,
        /*stay_on_C=*/true,
        /*move_to_center=*/true,
        /*attack_mode=*/true,
        /*jump_2=*/false,
        /*jump_1=*/true
    };

    // Состояния 1..7: остаёмся тут навсегда (оба stay=true), экономим энергию — без постоянного движения к центру, но с атакой по возможности
    for (int i = 1; i < 8; ++i) {
        rules[i] = {
            /*stay_on_D=*/true,
            /*stay_on_C=*/true,
            /*move_to_center=*/false,
            /*attack_mode=*/true,
            /*jump_2=*/false,
            /*jump_1=*/false
        };
    }

    return build_strategy(rules);
}
std::vector<pair<uint64_t, string>> make_meta_pack() {
    std::vector<pair<uint64_t, string>> pack;

    // 1. Агрессор
    StrategyDesc rambo = {{
        {true, true, 0}, {true, true, 0}, {true, true, 0},
        {true, true, 0}, {true, true, 0}, {true, true, 0},
        {true, true, 0}, {true, true, 0}
    }};
    pack.push_back({build_strategy(rambo),"rambo"});

    // 2. Уклонист
    StrategyDesc ghost = {{
        {false, false, 0}, {false, false, 0}, {false, false, 0},
        {false, false, 0}, {false, false, 0}, {false, false, 0},
        {false, false, 0}, {false, false, 0}
    }};
    pack.push_back({build_strategy(ghost),"ghost"});

    // 3. Тактик (см. выше)
    pack.push_back({build_strategy(tactician),"tactician"});

    StrategyDesc late_boss = {{
        {false, false, 1}, // 0: hide → 1
        {false, false, 2}, // 1: hide → 2
        {true,  false, 3}, // 2: to_center (без атаки) → 3
        {true,  true,  7}, // 3: attack → 7
        {true,  true,  7}, // 4: (не используется)
        {true,  true,  7}, // 5:
        {true,  true,  7}, // 6:
        {true,  true,  7}  // 7: attack forever
    }};
    pack.push_back({build_strategy(late_boss),"late_boss"});
    StrategyDesc boss = {{
        {true, true, 0}, // 0: hide → 1
        {false, false, 2}, // 1: hide → 2
        {true,  false, 3}, // 2: to_center (без атаки) → 3
        {true,  true,  7}, // 3: attack → 7
        {true,  true,  7}, // 4: (не используется)
        {true,  true,  7}, // 5:
        {true,  true,  7}, // 6:
        {true,  true,  7}  // 7: attack forever
    }};
    pack.clear();
    //pack.push_back({build_strategy(boss),"boss"});
    std::array<StateRule, 8> smart_tactician = {{
        // 0: начни с атаки, если враг атакует — оставайся, иначе переходи
        {false, true, true, true, false, true},
        // 1: hide, если враг атакует — уходи дальше (переходи), иначе stay
        {true, false, false, false, false, true},
        // 2: снова атакуй
        {false, true, true, true, false, false},
        // 3-7: финальная агрессия
        {true, true, true, true, false, false},
        {true, true, true, true, false, false},
        {true, true, true, true, false, false},
        {true, true, true, true, false, false},
        {true, true, true, true, false, false}
    }};
    pack.push_back({build_strategy(smart_tactician),"smart_tactician"});
    std::array<StateRule, 8> adaptive_duelist = {{
        // 0: Start aggressive
        {false, false, true, true, false, true},  // C: +1, D: +1 → never stay in 0!
        // 1: Assess — if enemy passive, attack; if aggressive, hide
        {true, false, true, true, false, false}, // C: stay (attack), D: +1 (hide)
        // 2: Hide mode
        {false, true, false, false, false, true}, // C: +1 (return to assess), D: stay (keep hiding)
        // 3: Final attack (loop here)
        {true, true, true, true, false, false},
        {true, true, true, true, false, false},
        {true, true, true, true, false, false},
        {true, true, true, true, false, false},
        {true, true, true, true, false, false}
    }};
    pack.push_back({build_strategy(smart_tactician),"adaptive_duelist"});/*
    pack.push_back({1749371283859903610,"S2_TOP1"});
    pack.push_back({3578499021006861433,"S2_TOP2"});
    pack.push_back({4196571752238511222,"S2_TOP3"});
    pack.push_back({5433937437461718646,"S1_TOP1"});
    pack.push_back({3372407492685672945,"S1_TOP2"});
    pack.push_back({1851330153364523319,"S1_TOP3"});
    pack.push_back({2355994713017364887,"SX_TOP1"});
    pack.push_back({1534944807218842006,"SX_TOP2"});
    pack.push_back({5235165796441134696,"SX_TOP3"});
    pack.push_back({3726203495355412911,"SN_TOP1"});
    pack.push_back({82510763999917668,"SN_TOP2"});
    pack.push_back({8904017326059109166,"SN_TOP3"});*/
    
    pack.push_back({gpt5_th(),"gpt5_th"});
    pack.push_back({gemini(),"gemini"});
    pack.push_back({deepseek(),"deepseek"});
    pack.push_back({perplexity(),"perplexity"});
    pack.push_back({grok(),"grok"});
    pack.push_back({1851330153364523319,"S1T3"});
    pack.push_back({3394414841364511096,"2025.10.27 18:25"});
    pack.push_back({35466097406721398,"2025.10.27 18:30"});
    pack.push_back({7230349191312921973,"2025.10.27 18:41"});
    pack.push_back({1484072599116778639,"2025.10.27 20:28"});
    pack.push_back({7861040937335919927,"2025.10.27 20:59"});

    pack.push_back({2230705993315794070,"2025.10.27 21:29"});
    pack.push_back({5983905796038006074,"2025.10.27 21:30"});

    pack.push_back({2028880712679374100,"2025.10.27 21:48"});
    pack.push_back({4583900382366612526,"2025.10.27 21:48"});
    return pack;
}
//std::cout << describe_strategy(u) << "\n";
void main_test(){
  srand(time(0));

  vector<pair<uint64_t, string>> meta_strategies = {
      {make_strategy('G','G','G','G','G','G', 1), "Pure Farmer"},
      {make_strategy('G','G','G','G','G','H', 2), "Late Farmer"},
      {make_strategy('G','G','S','S','S','H', 3), "Opportunistic Thief"},
      {make_strategy('G','S','S','S','S','S', 4), "Aggressive Thief"},
      {make_strategy('G','H','G','G','G','H', 5), "Defensive Farmer"},
      {make_strategy('G','S','G','S','G','S', 6), "Hybrid Swarmer"},
      {make_strategy('G','H','H','H','H','H', 7), "Early Hoarder"},
      {make_strategy('G','G','S','S','G','H', 8), "Meta Mimic"}
  };
  meta_strategies=make_meta_pack();
  t_main_test mt;
  mt.sarr.push_back(make_unique<t_season>());
  auto&s=*mt.sarr[0];
  init_splinter_2025_season(s);
  next_phase(s,"S1");
  //
  auto q = [&](uint64_t v, const string& name) {
    add_some_by_random(mt.garr, s);
    int uid = mt.garr.size() - 1;
    auto& coder = s.uid2scoder[uid];
    coder.sarr[0].cdn_bin_url.resize(8 * 4);
    for (int i = 0; i < 4; ++i)
      ((uint64_t*)&coder.sarr[0].cdn_bin_url[0])[i] = v;
    coder.is_meta = true;
    coder.meta_name = name;
    t_tracked_info t;
    t.label = name;
    t.source_phase = "init";
    t.source_rank = 0;
    t.phase2rank = {};
    coder.tracked=t;
  };
  
  for (int i = 0; i < 1; ++i) {
    for (auto& [v, name] : meta_strategies) {
      q(v, name);
    }
  }
  auto f=[&](uint64_t v){
    add_some_by_random(mt.garr,s);
    auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
    qwen.resize(8*4);for(int i=0;i<4;i++)((uint64_t*)&qwen[0])[i]=v;
  };
  auto g=[&](uint64_t v){
    add_some_by_random(mt.garr,s);
    auto&qwen=s.uid2scoder[mt.garr.size()-1].sarr[0].cdn_bin_url;
    qwen.resize(8*4);for(int i=0;i<4;i++)((uint64_t*)&qwen[0])[i]=4648736823098811908;
    ((uint64_t*)&qwen[0])[3]=v;
  };
  if(bool need_debug=false){
    vector<uint64_t> arr={};
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
  print_strategy(adaptive_farmer);
  cout<<"---\n";
  //for (int i = 0; i < 3;i++){
  //  g(generous_tft_num);g(opportunistic_num);g(fast_trust_num);g(ada);f(ada);
  //  for(auto v:meta_pack)q(v);
  //}
  for (int i = 0; i < 4000; ++i) {
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
  if(0)for(int i=0;i<mt_games_per_phase;i++){
    f(encode_strategy(generate_reachable_strategy(rng)));
    bool lastwin=false;
    if(add_finished_game(mt,s,&lastwin)){if(!lastwin){
      mt.garr.pop_back();
      s.phases[0]->uids.erase(mt.garr.size());
    }else{
      cout<<i<<"\n";
    }}
  }
  for(int i=0;i<mt_games_per_phase;i++){
    //if(rand()%3==0)f(encode_strategy(generate_reachable_strategy(rng)));
    add_finished_game(mt,s);
  }
  auto rmp=[&](const string&phase){
    record_all_ranks(s,phase);
    record_all_tracked_ranks(s,phase);
    mark_top_n_as_tracked(s,phase);
    record_meta_ranks(s,phase);
    print_top(mt,s,phase);
    print_meta_top16(mt,s,phase); 
    print_all_tracked_summary(mt,s);
    print_top_with_context(mt,s,phase,16);
  };
  rmp("S1");
  next_phase(s,"R1");
  play_all_game_iniside_round(mt,s,"R1");
  rmp("R1");
  next_phase(s,"S2");
  for(int i=0;i<mt_games_per_phase;i++){
    if(rand()%30==0)add_some_by_random(mt.garr,s);
    if(add_finished_game(mt,s)){}
    //update_elo(mt);
  }
  rmp("S2");
  next_phase(s,"R2");
  play_all_game_iniside_round(mt,s,"R2");
  rmp("R2");
  next_phase(s,"SF");
  for(int i=0;i<mt_games_per_phase*0.25;i++){
    if(rand()%500==0)add_some_by_random(mt.garr,s);
    add_finished_game(mt,s);
    //update_elo(mt);
  }
  rmp("SF"); 
  next_phase(s,"F");
  f(ada);
  play_all_game_iniside_round(mt,s,"F");
  rmp("F");
  print_top(mt,s,"F",60);
  next_phase(s,"S");
  for(int i=0;i<mt_games_per_phase;i++){
    if(rand()%500==0)add_some_by_random(mt.garr,s);
    add_finished_game(mt,s);
    //update_elo(mt);
  }
  rmp("S");
  //print_meta_summary();
  for(;;){
     this_thread::sleep_for(1000ms);
  }
  int gg=1;
  //next_phase(s,"frozen-end?");
}