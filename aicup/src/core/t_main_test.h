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
  bool hide = false;
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
        {"S1", "sandbox", 16, {}},
        {"R1", "round",   16, {{"S1", 900}}},
        {"S2", "sandbox", 16, {}},
        {"R2", "round",   16, {{"R1", 300}, {"S2", 60}}},
        {"SF", "sandbox", 16, {}},
        {"F",  "round",   16, {{"R2", 50},  {"SF", 10}}},
        {"S",  "sandbox", 16, {}}
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

using Strategy = std::array<uint8_t, 8>; // 8 states, 1 byte each
std::vector<double> simulate_arena_v11(const std::vector<Strategy>& strategies,bool debug);
std::vector<double> simulate_arena(const std::vector<uint64_t>& strategies,bool debug=false) {
  return simulate_arena_v11((std::vector<Strategy>&)strategies,debug);
}
uint64_t parse_strategy_v9(const std::string& s) {
    std::istringstream iss(s);
    std::string line;
    std::getline(iss, line); // "#|C|D|M|A|S"
    std::getline(iss, line); // "-----------"

    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) {
        std::getline(iss, line);
        char C, D, M, A, S;
        int state;

        auto a=split(line,"|");
        state=stoi(a[0]);C=a[1][0];D=a[2][0];M=a[3][0];A=a[4][0];S=a[5][0];
        //sscanf_s(line.c_str(),"%d|%c|%c|%c|%c|%c", &state, &C, &D, &M, &A, &S);

        auto decode_action = [](char c) -> std::tuple<bool, bool, bool> {
            if (c == 'S') return {true, false, false};
            if (c == '3') return {false, true, true};
            if (c == '2') return {false, true, false};
            if (c == '1') return {false, false, true};
            return {true, false, false}; // fallback
        };

        auto [stayC, j2C, j1C] = decode_action(C);
        auto [stayD, j2D, j1D] = decode_action(D);
        // jump флаги должны совпадать — берём из C (как в print)
        bool j2 = j2C, j1 = j1C;
        bool stay_on_C = stayC;
        bool stay_on_D = stayD;
        bool move_to_center = (M == 'C');
        bool attack_mode = (A == 'A');

        uint8_t special = (S == 'R') ? 0x01 :
                          (S == 'F') ? 0x02 :
                          (S == 'A') ? 0x03 : 0x00;

        uint8_t b = 0;
        if (stay_on_D) b |= 0x80;
        if (stay_on_C) b |= 0x40;
        if (move_to_center) b |= 0x20;
        if (attack_mode) b |= 0x10;
        if (j2) b |= 0x08;
        if (j1) b |= 0x04;
        b |= special; // bits 1:0

        u |= static_cast<uint64_t>(b) << (8 * i);
    }
    return u;
}
std::string print_strategy_v9(uint64_t u) {
    std::ostringstream oss;
    oss << "#|C|D|M|A|S\n";
    oss << "-----------\n";
    for (int i = 0; i < 8; ++i) {
        uint8_t b = (u >> (8 * i)) & 0xFF;
        bool stayC = (b >> 6) & 1;
        bool stayD = (b >> 7) & 1;
        bool move = (b >> 5) & 1;
        bool atk = (b >> 4) & 1;
        uint8_t offset = (b >> 2) & 0x03; // биты 3:2 → 0,1,2,3
        uint8_t special_mode = b & 0x03;   // биты 1:0

        char offset_char = '0' + static_cast<char>(offset); // '0','1','2','3'

        auto colC = stayC||!offset ? 'S' : offset_char;
        auto colD = stayD||!offset ? 'S' : offset_char;

        char move_char = move ? 'C' : 'A';
        char atk_char = atk ? 'A' : 'H';

        char spec_char = (special_mode == 0x01) ? 'R' :
                         (special_mode == 0x02) ? 'F' :
                         (special_mode == 0x03) ? 'A' : 'N';

        oss << i << "|" << colC << "|" << colD << "|" << move_char
            << "|" << atk_char << "|" << spec_char << "\n";
    }
    return oss.str();
}

void print_strategy(const uint64_t&u){
  cout<<print_strategy_v9(u)<<"\n";
}
std::vector<double> simulate_arena_v9(const std::vector<Strategy>& strategies,bool) {
    struct Agent {
        int x = 0, y = 0;          // ×10 for precision
        int hp = 100;
        int energy = 100;
        int state = 0;
        bool alive = true;
        int ticks_survived = 0;
        int damage_dealt = 0;
        int frags=0;
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
            agents[i].x = std::rand() % 2000 - 1000;
            agents[i].y = std::rand() % 2000 - 1000;
            agents[i].state = rand()%8;
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

                uint8_t special_mode = state_byte & 0x03; // биты 1:0
                bool use_rush      = (special_mode == 0x01);
                bool use_fury      = (special_mode == 0x02);
                bool avoid_others  = (special_mode == 0x03);
                int base_step = 50;
                if (use_rush && a.energy >= 2) {
                    base_step = 100; // удвоенное перемещение
                }
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
                
                if (avoid_others && !attack_mode) {
                    // Проверяем, есть ли агенты ближе 150
                    for (int j = 0; j < PLAYERS_PER_BATTLE; ++j) {
                        if (i == j || !agents[j].alive) continue;
                        int dx = a.x - agents[j].x;
                        int dy = a.y - agents[j].y;
                        int dist_sq = dx*dx + dy*dy;
                        if (dist_sq < 150*150 && dist_sq > 0) {
                            // Отталкивание: двигаемся ОТ агента
                            move_dx += dx;
                            move_dy += dy;
                        }
                    }
                }

                if (move_dx != 0 || move_dy != 0) {
                    int len = static_cast<int>(std::sqrt(move_dx*move_dx + move_dy*move_dy));
                    if (len > 0) {
                        int energy_cost = (use_rush ? 2 : 1);
                        if (a.energy >= energy_cost) {
                            a.x += (move_dx * base_step) / len;
                            a.y += (move_dy * base_step) / len;
                            a.energy -= energy_cost;
                        }
                    }
                }

                // Пассивное притяжение к центру
                int drift = 2 + (1000 - R) / 100;
                a.x -= (a.x * drift) / 1000;
                a.y -= (a.y * drift) / 1000;

                // Атака или укрытие
                if (attack_mode) {
                    int energy_cost = use_fury ? 3 : 2;
                    bool enought=a.energy >= energy_cost;
                    if (enought)
                    for (int j = 0; j < PLAYERS_PER_BATTLE; ++j) {
                        if (i == j || !agents[j].alive) continue;
                        int dx = a.x - agents[j].x;
                        int dy = a.y - agents[j].y;
                        if (dx*dx + dy*dy <= 10000) {
                            int damage = use_fury ? 50 : 10;
                            if(agents[j].hp<damage)a.frags++;
                            agents[j].hp -= damage;
                            a.damage_dealt += damage;
                            a.attacked_this_tick = true;

                            a.energy -= energy_cost;

                            break;
                        }
                    }
                    if (enought&&use_fury) {
                        a.hp -= 10; // самоповреждение
                        if (a.hp <= 0) {
                            a.alive = false;
                            alive_count--;
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
                    uint8_t offset = (state_byte >> 2) & 0x03; // биты 3 и 2 → 0..3
                    a.state = (a.state + offset) % 8;
                }
                // если stay=true → состояние не меняется
            }

            tick++;
        }

        // Подсчёт очков
        double total_raw = 0;
        std::vector<double> battle_score(PLAYERS_PER_BATTLE, 0.0);
        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            double score = agents[i].ticks_survived + agents[i].damage_dealt * 0.314+agents[i].frags*50;
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
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <sstream>

using Strategy = std::array<uint8_t, 8>;

struct t_pubg_world/* : i_world*/{
    struct Action {
        bool move_to_center = false;
        bool chase_enemy = false;
        bool repel_from_enemy = false;
        bool attack = false;
        bool use_fury = false;
        bool use_rush = false;
        bool avoid_others = false;
        bool use_dir=false;
        vec2i dir;
        int target=-1;
    };
    struct Agent {
        int x = 0, y = 0;
        int hp = 100;
        int energy = 100;
        int state = 0;
        bool alive = true;
        int ticks_survived = 0;
        int damage_dealt = 0;
        int frags = 0;
        bool attacked_this_tick = false;
        Strategy strat; // обычная стратегия
        //std::string manual_cmd; // для ручного управления
        bool is_manual = false;
        Action a;
    };

    std::vector<Agent> agents;
    int R = 1000;
    int tick = 0;
    int alive_count = 0;
    bool is_finished = false;
    std::mt19937 rng;

    // Для scoring
    std::vector<double> final_scores;

    void init(uint32_t seed, uint32_t num_players){
        rng.seed(seed);
        agents.resize(num_players);
        alive_count = num_players;
        tick = 0;
        is_finished = false;
        final_scores.clear();

        for (int i = 0; i < (int)num_players; ++i) {
            agents[i].x = std::rand() % 2000 - 1000;
            agents[i].y = std::rand() % 2000 - 1000;
            agents[i].state = std::rand() % 8;
            agents[i].alive = true;
            agents[i].hp = 100;
            agents[i].energy = 100;
            agents[i].ticks_survived = 0;
            agents[i].damage_dealt = 0;
            agents[i].frags = 0;
            agents[i].attacked_this_tick = false;
            agents[i].is_manual = false; // по умолчанию — автомат
        }
    }

    void use(int player, const std::string& cmd, std::string& outmsg){
        if (player < 0 || player >= (int)agents.size()) return;
    }
    void step(bool end=false){
        if (is_finished || alive_count <= 1 || tick >= 200) {
            is_finished = true;
            return;
        }

        if (tick % 10 == 0 && R > 100) R -= 100;

        for (auto& a : agents) {
            if (a.alive) a.attacked_this_tick = false;
        }

        // --- Фаза 1: определить действия для каждого агента ---
        //std::vector<Action> actions(agents.size());

        for (int i = 0; i < (int)agents.size(); ++i) {
            if (!agents[i].alive) continue;
            auto& a = agents[i];
            Action& act = a.a;//actions[i];

            if (a.is_manual) {
                
            } else {
                // Обычная стратегия → преобразуем state_byte в Action
                uint8_t state_byte = a.strat[a.state];
                act.move_to_center = (state_byte >> 5) & 1;
                act.attack = (state_byte >> 4) & 1;
                uint8_t special = state_byte & 0x03;
                act.use_rush = (special == 0x01);
                act.use_fury = (special == 0x02);
                act.avoid_others = (special == 0x03);
                act.move_to_center = (state_byte >> 5) & 1;
                act.repel_from_enemy = !act.move_to_center;
            }
        }

        // --- Фаза 2: выполнить действия (единый код для всех) ---
        for (int i = 0; i < (int)agents.size(); ++i) {
            if (!agents[i].alive) continue;
            auto& a = agents[i];
            Action& act = a.a;

            a.ticks_survived++;

            // Найти ближайшего врага (нужно для move_to_enemy и атаки)
            int closest_dist_sq = 100000000;
            int dx_to_closest = 0, dy_to_closest = 0;
            bool found_enemy = false;
            for (int j = 0; j < (int)agents.size(); ++j) {
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

            // --- Фаза движения ---
            int move_dx = 0, move_dy = 0;

            // 1. Определяем желаемое направление
            if (act.use_dir) {
                // Ручное управление: используем dir как вектор направления
                move_dx = act.dir.x;
                move_dy = act.dir.y;
            } else if (act.move_to_center) {
                move_dx = -a.x;
                move_dy = -a.y;
            } else if (act.chase_enemy && found_enemy) {
                move_dx = -dx_to_closest; 
                move_dy = -dy_to_closest;
            } else if (act.repel_from_enemy && found_enemy && closest_dist_sq < 90000) {
                move_dx = dx_to_closest;
                move_dy = dy_to_closest;
            }

            // 2. Избегание столкновений (добавляет к вектору)
            if (act.avoid_others && !act.attack) {
                for (int j = 0; j < (int)agents.size(); ++j) {
                    if (i == j || !agents[j].alive) continue;
                    int dx = a.x - agents[j].x;
                    int dy = a.y - agents[j].y;
                    int dist_sq = dx*dx + dy*dy;
                    if (dist_sq < 150*150 && dist_sq > 0) {
                        move_dx += dx;
                        move_dy += dy;
                    }
                }
            }

            // 3. Ограничиваем шаг до max_step
            int max_step = act.use_rush ? 100 : 50;
            int desired_len_sq = move_dx*move_dx + move_dy*move_dy;

            if (desired_len_sq > 0) {
                int actual_step = max_step;
                if (desired_len_sq < max_step * max_step) {
                    // Если цель ближе max_step — идём прямо к ней
                    actual_step = static_cast<int>(std::sqrt(desired_len_sq));
                }

                // Нормализуем и масштабируем
                double len = std::sqrt(desired_len_sq);
                int step_x = static_cast<int>((move_dx / len) * actual_step);
                int step_y = static_cast<int>((move_dy / len) * actual_step);

                // Трата энергии: 1 за обычный шаг, 2 за рывок
                int energy_cost = act.use_rush ? 2 : 1;
                if (a.energy >= energy_cost) {
                    a.x += step_x;
                    a.y += step_y;
                    a.energy -= energy_cost;
                }
            }

            // Пассивное притяжение
            int drift = 2 + (1000 - R) / 100;
            a.x -= (a.x * drift) / 1000;
            a.y -= (a.y * drift) / 1000;

            // Атака
            if (act.attack) {
                int energy_cost = act.use_fury ? 3 : 2;
                if (a.energy >= energy_cost) {
                    for (int j = 0; j < (int)agents.size(); ++j) {
                        if (i == j || !agents[j].alive) continue;
                        if(qap_check_id(agents,act.target)&&act.target!=j)continue;
                        int dx = a.x - agents[j].x;
                        int dy = a.y - agents[j].y;
                        if (dx*dx + dy*dy <= 10000) {
                            int damage = act.use_fury ? 50 : 10;
                            if (agents[j].hp <= damage) a.frags++;
                            agents[j].hp -= damage;
                            a.damage_dealt += damage;
                            a.attacked_this_tick = true;
                            a.energy -= energy_cost;
                            break;
                        }
                    }
                    if (act.use_fury) {
                        a.hp -= 10;
                        if (a.hp <= 0) {
                            a.alive = false;
                            alive_count--;
                        }
                    }
                }
            } else {
                if (a.energy >= 1) a.energy -= 1;
            }

            // Урон от зоны
            if (a.x*a.x + a.y*a.y > R*R) {
                a.hp -= 5;
            }
            if (a.hp <= 0 && a.alive) {
                a.alive = false;
                alive_count--;
            }
        }

        // --- Фаза 3: обновление состояний (только для автоматов) ---
        for (int i = 0; i < (int)agents.size(); ++i) {
            if (!agents[i].alive || agents[i].is_manual) continue;
            auto& a = agents[i];
            uint8_t state_byte = a.strat[a.state];
            bool stay_on_D = (state_byte >> 7) & 1;
            bool stay_on_C = (state_byte >> 6) & 1;

            bool opponent_attacked = false;
            for (int j = 0; j < (int)agents.size(); ++j) {
                if (i == j || !agents[j].alive) continue;
                opponent_attacked = agents[j].attacked_this_tick;
                break;
            }

            bool stay = opponent_attacked ? stay_on_D : stay_on_C;
            if (!stay) {
                uint8_t offset = (state_byte >> 2) & 0x03;
                a.state = (a.state + offset) % 8;
            }
        }

        tick++;
        if (alive_count <= 1 || tick >= 200||end) {
            is_finished = true;
            double total_raw = 0;
            int PLAYERS_PER_BATTLE = agents.size();
            final_scores.assign(PLAYERS_PER_BATTLE, 0.0);

            for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
                double score = agents[i].ticks_survived 
                             + agents[i].damage_dealt * 0.314 
                             + agents[i].frags * 50;
                final_scores[i] = score;
                total_raw += score;
            }

            double avg = total_raw / PLAYERS_PER_BATTLE;
            for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
                final_scores[i] -= avg; // именно так в оригинале
            }
        }
    }

    bool finished(){
        return is_finished;
    }

    void get_score(std::vector<double>& out){
        out = final_scores;
    }

    void is_alive(std::vector<int>& out){
        out.resize(agents.size());
        for (int i = 0; i < (int)agents.size(); ++i) {
            out[i] = agents[i].alive ? 1 : 0;
        }
    }

    void get_vpow(int player, std::string& out){
        if (player < 0 || player >= (int)agents.size()) {
            out = "{}";
            return;
        }
        std::ostringstream oss;
        oss << "{"
            << "\"x\":" << agents[player].x << ","
            << "\"y\":" << agents[player].y << ","
            << "\"hp\":" << agents[player].hp << ","
            << "\"energy\":" << agents[player].energy << ","
            << "\"alive\":" << (agents[player].alive ? "true" : "false")
            << "}";
        out = oss.str();
    }
    int get_tick(){
        return tick;
    }

    // Дополнительно: установить стратегию для игрока
    void set_strategy(int player, const Strategy& s) {
        if (player >= 0 && player < (int)agents.size()) {
            agents[player].strat = s;
            agents[player].is_manual = false;
        }
    }
};
vec2i to_vec2i(vec2d p){return vec2i(p.x,p.y);}
t_pubg_world::Action encode_action(uint64_t c,int target=-1,bool use_dir=false,vec2i dir={}) {
  t_pubg_world::Action a;
  a.attack = true;
  a.move_to_center = (c >> 0) & 1;
  a.repel_from_enemy = !a.move_to_center;
  uint8_t spe = (c >> 1) & 3;
  a.avoid_others = (spe == 1);
  a.use_fury = (spe == 2);
  a.use_rush = (spe == 3);
  a.target=target;//use_target?(c>>(1+2))&0xf:-1;
  a.use_dir=use_dir;
  a.dir=to_vec2i(vec2d(dir.x,dir.y).Norm()*100);
  return a;
}
void apply_action(t_pubg_world::Agent& a, uint64_t c,int target=-1,bool use_dir=false,vec2i dir={}) {
  a.a=encode_action(c,target,use_dir,dir);
  a.is_manual = true;
}
double evaluate_agent(const t_pubg_world::Agent& a) {
  if (!a.alive) return -1e6;
  return a.hp * 1.0+ a.energy * 0.5 + a.damage_dealt * 0.3 + a.frags * 50.0;
}
double evaluate_world_v1(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    const auto& me_before = world_before.agents[my_id];
    const auto& me_after = world_after.agents[my_id];

    // Если умер — большой штраф
    if (!me_after.alive) {
        return -1e6;
    }

    double score = 0.0;

    // 1. Базовые компоненты (как в scoring)
    score += me_after.hp * 1.0;
    score += me_after.energy * 0.5;
    score += me_after.damage_dealt * 0.3;
    score += me_after.frags * 50.0;

    // 2. Штраф за нахождение в зоне урона
    int R = world_after.R;
    if (me_after.x * me_after.x + me_after.y * me_after.y > R * R) {
        score -= 30.0; // сильный штраф
    }

    // 3. Позиционная оценка относительно врагов
    int my_hp = me_after.hp;
    int my_x = me_after.x;
    int my_y = me_after.y;

    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (j == my_id || !world_after.agents[j].alive) continue;
        const auto& enemy = world_after.agents[j];
        int dx = my_x - enemy.x;
        int dy = my_y - enemy.y;
        double dist = std::sqrt(dx*dx + dy*dy);

        if (dist < 1e-3) dist = 1e-3; // избегаем деления на ноль

        if (enemy.hp < my_hp) {
            // Враг слабее — хочется быть ближе
            // Бонус обратно пропорционален расстоянию
            double bonus = (my_hp - enemy.hp) * 10.0 / dist;
            score += bonus;
        } else {
            // Враг сильнее или равен — хочется быть дальше
            // Штраф обратно пропорционален расстоянию
            double penalty = (enemy.hp - my_hp + 1) * 5.0 / dist;
            score -= penalty;
        }
    }

    // 4. Избегание центра на ранних тиках (если не агрессивный)
    int tick = world_after.tick;
    double center_dist = std::sqrt(my_x * my_x + my_y * my_y);
    if (tick < 50 && center_dist < 300) { // 300 = 30.0 в реальных координатах
        // На ранних тиках центр = скопление врагов → штраф
        score -= (50 - tick) * 0.5; // чем раньше, тем больше штраф
    }

    // 5. Бонус за продвижение к слабым врагам (по сравнению с before)
    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (j == my_id || !world_after.agents[j].alive) continue;
        const auto& enemy_after = world_after.agents[j];
        if (enemy_after.hp >= me_before.hp) continue; // только слабые

        // Найдём врага в world_before (по индексу j)
        const auto& enemy_before = world_before.agents[j];
        if (!enemy_before.alive) continue;

        double dist_before = std::sqrt(
            (me_before.x - enemy_before.x) * (me_before.x - enemy_before.x) +
            (me_before.y - enemy_before.y) * (me_before.y - enemy_before.y)
        );
        double dist_after = std::sqrt(
            (my_x - enemy_after.x) * (my_x - enemy_after.x) +
            (my_y - enemy_after.y) * (my_y - enemy_after.y)
        );

        if (dist_after < dist_before) {
            // Мы приблизились к слабому врагу — бонус
            score += 5.0;
        }
    }

    return score;
}
t_pubg_world::Action choose_action_depth2(t_pubg_world& w, int agent_idx, int target = -1) {
    auto original_world = w;
    double best_score_a1 = -1e9;
    t_pubg_world::Action best_action_a1;

    for (uint64_t c1 = 0; c1 < 8; ++c1) {
        auto world1 = original_world;
        apply_action(world1.agents[agent_idx], c1, target);
        world1.step();

        if (!world1.agents[agent_idx].alive) {
            double score = -10000;
            if (score > best_score_a1) {
                best_score_a1 = score;
                best_action_a1 = encode_action(c1, target);
            }
            continue;
        }

        double best_score_a2 = -1e9;
        for (uint8_t c2 = 0; c2 < 8; ++c2) {
            auto world2 = world1;
            apply_action(world2.agents[agent_idx], c2, target);
            world2.step();
            double score = evaluate_agent(world2.agents[agent_idx]);
            if (score > best_score_a2) {
                best_score_a2 = score;
            }
        }

        if (best_score_a2 > best_score_a1) {
            best_score_a1 = best_score_a2;
            best_action_a1 = encode_action(c1, target);
        }
    }
    return best_action_a1;
}
#include "by_ai/t_main_test_strats.h"
std::vector<double> simulate_arena_v11(const std::vector<Strategy>& strategies, bool) {
    const int N = static_cast<int>(strategies.size());
    if (N < 2) return std::vector<double>(N, 0.0);

    std::vector<double> total_score(N, 0.0);
    const int BATTLES = 1;//1000;
    const int PLAYERS_PER_BATTLE = std::min(16, N);
    static std::mt19937 rng(2025);
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, INT_MAX);

    for (int b = 0; b < BATTLES; ++b) {
        std::vector<int> indices(N);
        for (int i = 0; i < N; ++i) indices[i] = i;
        std::shuffle(indices.begin(), indices.end(), rng);
        indices.resize(PLAYERS_PER_BATTLE);

        t_pubg_world w;
        w.init(dist(rng), PLAYERS_PER_BATTLE);

        // Устанавливаем стратегии, помечаем ручных
        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            uint64_t v = *reinterpret_cast<const uint64_t*>(&strategies[indices[i]]);
            if (v == 0||v==1||v==2||v==3||v==4||v==5||(v>=31456&&v<=31456+16)||(v>=32456&&v<=32456+16)) {
                w.agents[i].is_manual = true;
            } else {
                w.set_strategy(i, strategies[indices[i]]);
            }
        }

        while (!w.finished()) {
            // Для каждого ручного агента — выбираем лучшее действие
            for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
                if (!w.agents[i].is_manual || !w.agents[i].alive) continue;
                uint64_t v = *reinterpret_cast<const uint64_t*>(&strategies[indices[i]]);
                if(v==0){
                  auto original_world = w; // копия всего мира
                  double best_value = -1e9;
                  t_pubg_world::Action best_action;

                  // Перебираем 8 вариантов
                  for (uint64_t c = 0; c < 8; ++c) {
                      // Восстанавливаем мир
                      w = original_world;
                      auto& a = w.agents[i];

                      // Формируем действие
                      t_pubg_world::Action act;
                      act.attack = true;
                      act.move_to_center = (c >> 0) & 1;
                      act.repel_from_enemy = !act.move_to_center;
                      uint8_t spe = (c >> 1) & 3;
                      act.avoid_others = (spe == 1);
                      act.use_fury = (spe == 2);
                      act.use_rush = (spe == 3);

                      // Применяем действие
                      a.a = act;

                      w.step();if(!w.is_finished)w.step();

                      // Оценка результата
                      double value = 0;
                      auto& new_a = w.agents[i];
                      if (!new_a.alive) {
                          value = -1000; // большой штраф за смерть
                      } else {
                          // Эвристика: HP + энергия + урон + выживание
                          value = new_a.hp * 1.0
                                + new_a.energy * 0.5
                                + new_a.damage_dealt * 0.3
                                //+ new_a.ticks_survived * 2.0
                                + new_a.frags * 50.0;
                          //if (new_a.attacked_this_tick) value += 10;
                          //if (act.use_fury) value -= 5; // риск
                      }

                      if (value > best_value) {
                          best_value = value;
                          best_action = act;
                      }

                  }

                  // Восстанавливаем мир и применяем лучшее действие
                  w = original_world;
                  w.agents[i].a = best_action;
                  continue;
                }
                if(v==1){
                  algo_v1:
                  auto original_world = w;
                  double best_score_a1 = -1e9;
                  t_pubg_world::Action best_action_a1;

                  for (uint64_t c1 = 0; c1 < 8; ++c1) {
                      // --- Тик t+1 ---
                      auto world1 = original_world;
                      apply_action(world1.agents[i], c1);
                      world1.step();

                      if (!world1.agents[i].alive) {
                          // Если умер на t+1 — штраф
                          double score = -10000;
                          if (score > best_score_a1) {
                              best_score_a1 = score;
                              best_action_a1 = encode_action(c1);
                          }
                          continue;
                      }

                      // --- Тик t+2: перебираем все a2 ---
                      double best_score_a2 = -1e9;
                      for (uint8_t c2 = 0; c2 < 8; ++c2) {
                          auto world2 = world1;
                          apply_action(world2.agents[i], c2);
                          world2.step();

                          double score = evaluate_agent(world2.agents[i]);
                          if (score > best_score_a2) {
                              best_score_a2 = score;
                          }
                      }

                      // Оценка для a1 = лучший результат на t+2
                      if (best_score_a2 > best_score_a1) {
                          best_score_a1 = best_score_a2;
                          best_action_a1 = encode_action(c1);
                      }
                  }

                  // Применяем лучшее первое действие
                  w = original_world;
                  w.agents[i].a = best_action_a1;
                  continue;
                }
                if(v==2){
                  vector<int> earr;
                  auto&a=w.agents[i];
                  for (int j = 0; j < (int)w.agents.size(); ++j) {
                    if (i == j || !w.agents[j].alive) continue;
                    int dx = a.x - w.agents[j].x;
                    int dy = a.y - w.agents[j].y;
                    if (dx*dx + dy*dy <= 10000) {
                      earr.push_back(j);
                    }
                  }
                  if(earr.size()<2)goto algo_v1;
                  auto original_world = w;
                  double best_score_a1 = -1e9;
                  t_pubg_world::Action best_action_a1;
                  for(int eid=0;eid<earr.size();eid++)
                  for (uint64_t c1 = 0; c1 < 8; ++c1) {
                      // --- Тик t+1 ---
                      auto world1 = original_world;
                      apply_action(world1.agents[i],c1,earr[eid]);
                      world1.step();

                      if (!world1.agents[i].alive) {
                          // Если умер на t+1 — штраф
                          double score = -10000;
                          if (score > best_score_a1) {
                              best_score_a1 = score;
                              best_action_a1 = encode_action(c1,earr[eid]);
                          }
                          continue;
                      }

                      // --- Тик t+2: перебираем все a2 ---
                      double best_score_a2 = -1e9;
                      for (uint8_t c2 = 0; c2 < 8; ++c2) {
                          auto world2 = world1;
                          apply_action(world2.agents[i],c2,earr[eid]);
                          world2.step();

                          double score = evaluate_agent(world2.agents[i]);
                          if (score > best_score_a2) {
                              best_score_a2 = score;
                          }
                      }

                      // Оценка для a1 = лучший результат на t+2
                      if (best_score_a2 > best_score_a1) {
                          best_score_a1 = best_score_a2;
                          best_action_a1 = encode_action(c1,earr[eid]);
                      }
                  }

                  // Применяем лучшее первое действие
                  w = original_world;
                  w.agents[i].a = best_action_a1;
                  continue;
                }
                if(v==3){
                  vector<int> earr;
                  auto&a=w.agents[i];
                  for (int j = 0; j < (int)w.agents.size(); ++j) {
                    if (i == j || !w.agents[j].alive) continue;
                    int dx = a.x - w.agents[j].x;
                    int dy = a.y - w.agents[j].y;
                    if (dx*dx + dy*dy <= 10000) {
                      earr.push_back(j);
                    }
                  }
                  if(earr.size()<2)goto algo_v1;
                  auto original_world = w;
                  double best_score_a1 = -1e9;
                  t_pubg_world::Action best_action_a1;
                  for(int dx=-1;dx<=+1;dx++)for(int dy=-1;dy<=+1;dy++)
                  for(int eid=0;eid<earr.size();eid++)
                  for (uint64_t c1 = 0; c1 < 8; ++c1) {
                      auto a=encode_action(c1,earr[eid],true,vec2i(dx,dy));
                      if(a.avoid_others)continue;
                      auto world1 = original_world;
                      world1.agents[i].a=a;
                      world1.step();

                      if (!world1.agents[i].alive) {
                          // Если умер на t+1 — штраф
                          double score = -10000;
                          if (score > best_score_a1) {
                              best_score_a1 = score;
                              best_action_a1 = encode_action(c1,earr[eid],true,vec2i(dx,dy));
                          }
                          continue;
                      }

                      // --- Тик t+2: перебираем все a2 ---
                      double best_score_a2 = -1e9;
                      for(int dx=-1;dx<=+1;dx++)for(int dy=-1;dy<=+1;dy++)
                      for (uint8_t c2 = 0; c2 < 8; ++c2) {
                          auto a=encode_action(c2,earr[eid],true,vec2i(dx,dy));
                          if(a.avoid_others)continue;
                          auto world2 = world1;
                          world2.agents[i].a=a;
                          world2.step();

                          double score = evaluate_agent(world2.agents[i]);
                          if (score > best_score_a2) {
                              best_score_a2 = score;
                          }
                      }

                      // Оценка для a1 = лучший результат на t+2
                      if (best_score_a2 > best_score_a1) {
                          best_score_a1 = best_score_a2;
                          best_action_a1 = encode_action(c1,earr[eid],true,vec2i(dx,dy));
                      }
                  }

                  // Применяем лучшее первое действие
                  w = original_world;
                  w.agents[i].a = best_action_a1;
                  continue;
                }
                if(v==4){
                  vector<int> earr;
                  auto&a=w.agents[i];
                  for (int j = 0; j < (int)w.agents.size(); ++j) {
                    if (i == j || !w.agents[j].alive) continue;
                    int dx = a.x - w.agents[j].x;
                    int dy = a.y - w.agents[j].y;
                    if (dx*dx + dy*dy <= 300*300) {
                      earr.push_back(j);
                    }
                  }
                  if(earr.empty())earr.push_back(-1);
                  //if(earr.size()<2)goto algo_v1;
                  auto original_world = w;
                  double best_score_a1 = -1e9;
                  t_pubg_world::Action best_action_a1;
                  for(int dx=-1;dx<=+1;dx++)for(int dy=-1;dy<=+1;dy++)
                  for(int eid=0;eid<earr.size();eid++)
                  for (uint64_t c1 = 0; c1 < 8; ++c1) {
                      auto a=encode_action(c1,earr[eid],true,vec2i(dx,dy));
                      if(a.avoid_others)continue;
                      auto world1 = original_world;
                      world1.agents[i].a=a;
                      world1.step();

                      if (!world1.agents[i].alive) {
                          // Если умер на t+1 — штраф
                          double score = -10000;
                          if (score > best_score_a1) {
                              best_score_a1 = score;
                              best_action_a1 = encode_action(c1,earr[eid],true,vec2i(dx,dy));
                          }
                          continue;
                      }

                      // --- Тик t+2: перебираем все a2 ---
                      double best_score_a2 = -1e9;
                      for(int dx=-1;dx<=+1;dx++)for(int dy=-1;dy<=+1;dy++)
                      for (uint8_t c2 = 0; c2 < 8; ++c2) {
                          auto a=encode_action(c2,earr[eid],true,vec2i(dx,dy));
                          if(a.avoid_others)continue;
                          auto world2 = world1;
                          world2.agents[i].a=a;
                          world2.step();

                          double score = evaluate_agent(world2.agents[i]);
                          if (score > best_score_a2) {
                              best_score_a2 = score;
                          }
                      }

                      // Оценка для a1 = лучший результат на t+2
                      if (best_score_a2 > best_score_a1) {
                          best_score_a1 = best_score_a2;
                          best_action_a1 = encode_action(c1,earr[eid],true,vec2i(dx,dy));
                      }
                  }

                  // Применяем лучшее первое действие
                  w = original_world;
                  w.agents[i].a = best_action_a1;
                  continue;
                }
                if(v==5){
                  vector<int> earr;
                  auto&a=w.agents[i];
                  for (int j = 0; j < (int)w.agents.size(); ++j) {
                    if (i == j || !w.agents[j].alive) continue;
                    int dx = a.x - w.agents[j].x;
                    int dy = a.y - w.agents[j].y;
                    if (dx*dx + dy*dy <= 300*300) {
                      earr.push_back(j);
                    }
                  }
                  if(earr.empty())earr.push_back(-1);
                  //if(earr.size()<2)goto algo_v1;
                  auto original_world = w;
                  double best_score_a1 = -1e9;
                  t_pubg_world::Action best_action_a1;
                  for(int dx=-1;dx<=+1;dx++)for(int dy=-1;dy<=+1;dy++)
                  for(int eid=0;eid<earr.size();eid++)
                  for (uint64_t c1 = 0; c1 < 8; ++c1) {
                      auto a=encode_action(c1,earr[eid],true,vec2i(dx,dy));
                      if(a.avoid_others)continue;
                      auto world1 = original_world;
                      world1.agents[i].a=a;
                      world1.step();

                      if (!world1.agents[i].alive) {
                          // Если умер на t+1 — штраф
                          double score = -10000;
                          if (score > best_score_a1) {
                              best_score_a1 = score;
                              best_action_a1 = encode_action(c1,earr[eid],true,vec2i(dx,dy));
                          }
                          continue;
                      }

                      // --- Тик t+2: перебираем все a2 ---
                      double best_score_a2 = -1e9;
                      for(int dx=-1;dx<=+1;dx++)for(int dy=-1;dy<=+1;dy++)
                      for (uint8_t c2 = 0; c2 < 8; ++c2) {
                          auto a=encode_action(c2,earr[eid],true,vec2i(dx,dy));
                          if(a.avoid_others)continue;
                          auto world2 = world1;
                          world2.agents[i].a=a;
                          world2.step();

                          double score = evaluate_world_v1(w,world2,i);
                          if (score > best_score_a2) {
                              best_score_a2 = score;
                          }
                      }

                      // Оценка для a1 = лучший результат на t+2
                      if (best_score_a2 > best_score_a1) {
                          best_score_a1 = best_score_a2;
                          best_action_a1 = encode_action(c1,earr[eid],true,vec2i(dx,dy));
                      }
                  }

                  // Применяем лучшее первое действие
                  w = original_world;
                  w.agents[i].a = best_action_a1;
                  continue;
                }
                if(v>=31456&&v<=31456+32){
                  if(v==31456+0)w.agents[i].a=dna_by_adler(w,i); //1662.22 - 959.222
                  if(v==31456+2)w.agents[i].a=dna_by_deep_seek_with_sim(w,i); //1191.95
                  if(v==31456+3)w.agents[i].a=dna_by_perplexity(w,i);//253.851 - 251.56
                  if(v==31456+5)w.agents[i].a=dna_by_deepseek(w,i);//442.591 - 302.512
                  if(v==31456+6)w.agents[i].a=dna_by_deepseek_v2(w,i);//-90 - -37.517
                  if(v==31456+7)w.agents[i].a=dna_by_grok_v0(w,i);//148.492 - 244.452
                  if(v==31456+8)w.agents[i].a=dna_by_grok_ml(w,i);//123.647 - 182.096
                  if(v==31456+9)w.agents[i].a=dna_by_chatgpt(w,i);//275.681 - 254.329
                  if(v==31456+10)w.agents[i].a=dna_by_gemini(w,i);//206.683 - 180.969
                  if(v==31456+11)w.agents[i].a=dna_by_qwen_coder(w,i);//45.8937 - 125.676
                  if(v==31456+12)w.agents[i].a=dna_by_qwen_coder_v2(w,i);//309.471 - 281.652
                  if(v==31456+13)w.agents[i].a=dna_by_qwen_coder_v3(w,i);//161.061 - 204.852
                  if(v==31456+14)w.agents[i].a=dna_by_gpt5(w,i);//86.2268 - 58.5308
                  if(v==31456+15)w.agents[i].a=dna_by_alice(w,i);//288.103 - 285.115
                }
                if(v>=32456&&v<=32456+32){
                  #define F(ID,NAME){static auto p=NAME;if(v==32456+ID)w.agents[i].a=dna_by_adler_templ(w,i,p);}
                  F(1,evaluate_world_v1_by_perplexity);
                  F(2,evaluate_world_v1_by_grok);
                  F(3,evaluate_world_v1_by_deepseek);
                  F(4,evaluate_world_v1_by_gemini);
                  F(5,evaluate_world_v1_by_chatgpt);
                  F(6,evaluate_world_v1_by_alice);
                  F(7,evaluate_world_v1_by_qwenmax);

                  F(8,evaluate_world_v1_by_qwen3max_v2);
                  F(9,evaluate_world_v1_by_qwen3max_v3);
                  F(10,evaluate_world_v1_by_qwen3max_v4o);
                  F(11,evaluate_world_v1_by_qwen3max_v4i);
                  F(12,evaluate_world_v1_by_chatgpt_v2);
                  F(13,evaluate_world_v1_by_chatgpt_v2i);
                  F(14,evaluate_world_v1_by_grok_v2);
                  F(15,evaluate_world_v1_by_grok_v3);
                  F(16,evaluate_world_v1_by_grok_v3i);
                  F(17,evaluate_world_v1_by_gemini_v2);
                  F(18,evaluate_world_v1_by_alice_v2);
                  F(19,evaluate_world_v1_by_perplexity_v2);
                  #undef F
                }
            }

            // Теперь делаем настоящий шаг
            w.step();
        }

        std::vector<double> battle_score;
        w.get_score(battle_score);

        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            total_score[indices[i]] += battle_score[i];
        }
    }

    return total_score;
}
std::vector<double> simulate_arena_v10(const std::vector<Strategy>& strategies, bool) {
    const int N = static_cast<int>(strategies.size());
    if (N < 2) return std::vector<double>(N, 0.0);

    std::vector<double> total_score(N, 0.0);
    const int BATTLES = 1000;
    const int PLAYERS_PER_BATTLE = std::min(16, N);
    static std::mt19937 rng(2025);
    std::uniform_int_distribution<std::mt19937::result_type> dist(0,INT_MAX);
    for (int b = 0; b < BATTLES; ++b) {
        std::vector<int> indices(N);
        for (int i = 0; i < N; ++i) indices[i] = i;
        std::shuffle(indices.begin(), indices.end(), rng);
        indices.resize(PLAYERS_PER_BATTLE);
        t_pubg_world w;
        w.init(dist(rng), PLAYERS_PER_BATTLE);

        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            w.set_strategy(i, strategies[indices[i]]);
        }
        while (!w.finished()) {
          for(int i=0;i<PLAYERS_PER_BATTLE;i++){
            auto&v=(uint64_t&)strategies[indices[i]];
            if(v)continue;
            auto&a=w.agents[i];
            struct t_cmd{
              //uint8_t atk:1;
              uint8_t mov:1;
              uint8_t spe:2;
              uint8_t unk:4+1;
            };
            a.is_manual=true;
            std::vector<double> s;
            for(uint8_t c=0;c<8;c++){
              t_cmd cmd;((uint8_t&)cmd)=c;
              a.a.attack=true;//cmd.atk;
              a.a.move_to_center=cmd.mov;
              a.a.repel_from_enemy=!a.a.move_to_center;
              a.a.avoid_others=cmd.spe==1;
              a.a.use_fury=cmd.spe==2;
              a.a.use_rush=cmd.spe==3;
              auto tmp=w;
              tmp.step(true);
              s.clear();
              tmp.get_score(s);
              //тут мы должны вычислить очки и на основе этого выбрать лучшую команду для нас, а затем снаружи применить её.
            }
          }
          w.step();
        }

        std::vector<double> battle_score;
        w.get_score(battle_score);

        for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
            total_score[indices[i]] += battle_score[i];
        }
    }

    return total_score;
}

bool add_finished_game(const t_main_test& mt, t_season& season, bool* plastwin = nullptr, bool debug = false, bool need_shuffle = true) {
    t_phase* active = season.phases[season.current_phase_idx].get();
    if (active->uids.empty()) return false;
    if ((int)active->uids.size() < active->num_players) return false;

    // === Шаг 1: построим список (games_played, uid) и отсортируем по возрастанию ===
    vector<tuple<int,int,int>> uid_by_games; // (games, uid)
    for (int uid : active->uids) {
        int games = active->uid2games[uid]; // если нет — создастся как 0 (OK)
        uid_by_games.emplace_back(games,rand(),uid);
    }
    sort(uid_by_games.begin(), uid_by_games.end());

    // === Шаг 2: возьмём первых num_players с наименьшим числом игр ===
    vector<int> players;
    players.reserve(active->num_players);
    for (int i = 0; i < active->num_players; ++i) {
        players.push_back(std::get<2>(uid_by_games[i]));
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
    //static QapClock g_clock;
    //for(auto&ex:players)if(ex<=5)cout<<"bef game with adler"<<ex<<" "<<g_clock.MS()<<endl;
    auto r=simulate_arena(ais,debug);
    //for(auto&ex:players)if(ex<=5)cout<<"aft game with adler"<<ex<<" "<<g_clock.MS()<<endl;
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
    auto s=ranked[i].second;
    auto&sc=season.uid2scoder.at(s);
    auto&uid=ranked[i].second;
    auto git=p->uid2games.find(uid);
    auto g=git==p->uid2games.end()?0:git->second;
    cout << (i+1) << ". UID=" << uid
         << " score=" << -ranked[i].first << " "<< g<< " "<< tostr(season.uid2scoder.at(uid).sarr.back().cdn_bin_url) <<"\n";
  }
  detailed=std::min(detailed,(int)ranked.size());
  if(detailed)cout << "\n=== TOP "<<detailed<<" (of "<<ranked.size()<<") in " << phase_id << " ===\n";
  for(int i=0;i<detailed;i++){
    auto s=ranked[i].second;
    auto&sc=season.uid2scoder.at(s);
    auto&uid=ranked[i].second;
    auto git=p->uid2games.find(uid);
    auto g=git==p->uid2games.end()?0:git->second;
    cout << (i+1) << ". UID=" << uid
         << " score=" << -ranked[i].first << " "<< g<< " "<< tostr(season.uid2scoder.at(uid).sarr.back().cdn_bin_url) <<"\n";
  
    pri(season.uid2scoder.at(ranked[i].second).sarr.back().cdn_bin_url);
  }
}
int mt_games_per_phase=100;//2*64000;
void play_all_game_iniside_round(const t_main_test&mt,t_season& season, const string& phase_id,double koef) {
    t_phase* p = find_phase(season, phase_id);
    if (!p) return;
    for (int i = 0; i < mt_games_per_phase*koef; ++i) {
        add_finished_game(mt,season,0,false,false);
    }
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
        if (!coder.is_meta) continue;if (coder.hide) continue;
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
    auto get=[&](const string&src){
      return (uint64_t&)src[0];
    };
    cout << "\n=== META STRATEGIES TOP (current: " << current_phase << ") ===\n";
    int n = meta_list.size();
    for (int i = 0; i < n; ++i) {
        auto [curr, uid, name, src_phase, src_rank] = meta_list[i];
        auto&sc=season.uid2scoder.at(uid);
        string way;//="F"+to_string(i+1)+":";
        vector<string> f;
        for(auto&r:sc.phase2rank){
          f.push_back(r.first+"#"+to_string(r.second));
        }
        reverse(f.begin(),f.end());
        way+=join(f,"/");
        auto score=p->uid2score.count(uid)?p->uid2score.at(uid):1500;
        cout << (i+1) << ". " << name <<" "<<way<<" UID="<<uid<<" "<<(p->uid2games.count(uid)?p->uid2games.at(uid):0)<<" "<<score<<" "<<get(sc.sarr[0].cdn_bin_url)<<"\n";
        continue;
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
uint64_t generate_strategy(std::mt19937& rng,int ATTEMPTS=100) {
    return rnd();
}
std::vector<pair<string,string>> genpack;
vector<pair<uint64_t,string>> make_meta_pack(){
  vector<pair<uint64_t,string>> out;
  out.push_back({0,"Adler"});
  out.push_back({1,"Adler3D"});
  /*out.push_back({2,"Adler256D"});
  out.push_back({3,"Adler512D"});
  out.push_back({4,"Adler768D"});
  out.push_back({5,"Adler999D"});
  */
  auto F=[&](uint64_t ai,string name){
    out.push_back({ai,name});
  };
  /*
  1. dna_by_deep_seek_with_sim S1#1/R1#1 UID=3 2250 158696 31458
  2. dna_by_adler S1#5/R1#2 UID=2 2250 140469 31456
  3. Adler3D S1#2/R1#3 UID=1 2250 97427 1
  4. Adler S1#4/R1#4 UID=0 2250 93197.3 0
  5. dna_by_perplexity S1#3/R1#5 UID=4 2250 79316.6 31459
  6. dna_by_deepseek S1#6/R1#6 UID=5 2250 51635.8 31461
  7. dna_by_gemini S1#7/R1#7 UID=10 2250 1414.94 31466
  8. dna_by_gpt5 S1#9/R1#8 UID=14 2250 -32285.1 31470
  9. dna_by_qwen_coder_v2 S1#8/R1#9 UID=12 2250 -41221.9 31468
  10. dna_by_alice S1#11/R1#10 UID=15 2250 -57898.8 31471
  11. dna_by_grok_ml S1#13/R1#11 UID=8 2250 -59832.1 31464
  12. dna_by_grok_v0 S1#10/R1#12 UID=7 2250 -65577.9 31463
  13. dna_by_chatgpt S1#12/R1#13 UID=9 2250 -65999.4 31465
  14. dna_by_qwen_coder S1#14/R1#14 UID=11 2250 -72522.8 31467
  15. dna_by_qwen_coder_v3 S1#15/R1#15 UID=13 2250 -97915.2 31469
  16. dna_by_deepseek_v2 S1#16/R1#16 UID=6 2250 -104904 31462
  */
  F(31456+0,"dna_by_adler");
  F(31456+2,"dna_by_deep_seek_with_sim");
  
  F(31456+3,"dna_by_perplexity");
  F(31456+3,"dna_by_perplexity");
  F(31456+5,"dna_by_deepseek");
  F(31456+6,"dna_by_deepseek_v2");
  F(31456+7,"dna_by_grok_v0");
  F(31456+8,"dna_by_grok_ml");
  F(31456+9,"dna_by_chatgpt");
  F(31456+9,"dna_by_chatgpt");
  F(31456+10,"dna_by_gemini");
  F(31456+10,"dna_by_gemini");
  F(31456+11,"dna_by_qwen_coder");
  F(31456+12,"dna_by_qwen_coder_v2");
  F(31456+13,"dna_by_qwen_coder_v3");
  F(31456+14,"dna_by_gpt5");
  F(31456+15,"dna_by_alice");
  F(31456+15,"dna_by_alice");
  /*
  F(32456+1,"evaluate_world_v1_by_perplexity");
  F(32456+2,"evaluate_world_v1_by_grok");
  F(32456+3,"evaluate_world_v1_by_deepseek");
  F(32456+4,"evaluate_world_v1_by_gemini");
  F(32456+5,"evaluate_world_v1_by_chatgpt");
  F(32456+6,"evaluate_world_v1_by_alice");*/
  F(32456+7,"evaluate_world_v1_by_qwenmax");
  
  F(32456+8,"evaluate_world_v1_by_qwen3max_v2");
  F(32456+9,"evaluate_world_v1_by_qwen3max_v3");
  F(32456+10,"evaluate_world_v1_by_qwen3max_v4o");
  F(32456+11,"evaluate_world_v1_by_qwen3max_v4i");
  F(32456+12,"evaluate_world_v1_by_chatgpt_v2");
  F(32456+13,"evaluate_world_v1_by_chatgpt_v2i");
  F(32456+14,"evaluate_world_v1_by_grok_v2");
  F(32456+15,"evaluate_world_v1_by_grok_v3");
  F(32456+16,"evaluate_world_v1_by_grok_v3i");
  F(32456+17,"evaluate_world_v1_by_gimini_v2");
  F(32456+18,"evaluate_world_v1_by_alice_v2");
  F(32456+19,"evaluate_world_v1_by_perplexity_v2");
  /*
  
1. evaluate_world_v1_by_chatgpt SF#2/S2#2/S1#2/R2#4/R1#3/F#1 UID=4 880 121306 32461
2. evaluate_world_v1_by_grok SF#1/S2#1/S1#1/R2#1/R1#1/F#2 UID=1 880 114111 32458
3. evaluate_world_v1_by_perplexity SF#6/S2#6/S1#4/R2#6/R1#6/F#3 UID=0 880 85269.4 32457
4. evaluate_world_v1_by_deepseek SF#4/S2#4/S1#5/R2#3/R1#4/F#4 UID=2 880 84692.4 32459
5. evaluate_world_v1_by_qwenmax SF#5/S2#5/S1#6/R2#5/R1#5/F#5 UID=6 880 80996.8 32463
6. evaluate_world_v1_by_gemini SF#3/S2#3/S1#3/R2#2/R1#2/F#6 UID=3 880 76265.3 32460
7. evaluate_world_v1_by_alice SF#7/S2#7/S1#7/R2#7/R1#7/F#7 UID=5 880 43122.7 32462

1. evaluate_world_v1_by_chatgpt SF#2/S2#4/S1#2/R2#3/R1#2/F#1 UID=4 881 118235 32461
2. evaluate_world_v1_by_grok SF#1/S2#1/S1#1/R2#1/R1#1/F#2 UID=1 881 113840 32458
3. evaluate_world_v1_by_deepseek SF#6/S2#3/S1#4/R2#4/R1#4/F#3 UID=2 880 89594.7 32459
4. evaluate_world_v1_by_perplexity SF#5/S2#5/S1#6/R2#5/R1#6/F#4 UID=0 880 86062.9 32457
5. evaluate_world_v1_by_qwenmax SF#4/S2#6/S1#5/R2#6/R1#5/F#5 UID=6 880 80954.8 32463
6. evaluate_world_v1_by_gemini SF#3/S2#2/S1#3/R2#2/R1#3/F#6 UID=3 880 75696.4 32460
7. evaluate_world_v1_by_alice SF#7/S2#7/S1#7/R2#7/R1#7/F#7 UID=5 881 37995.7 32462
  
  */
  //return out;
F(2600625415943971123,"P70$F43:SF#15/S2#3/S1#115/R2#20/R1#431/F#43");
F(4006805700263227449,"P3$F5:SF#3/S2#3/S1#2/R2#9/R1#16/F#5");
F(3079947118580560306,"P82$F21:SF#9/S2#7/S1#82/R2#69/R1#23/F#21");
F(8373068495579472650,"P85$F34:SF#2/S2#75/S1#24/R2#18/R1#17/F#34");
F(8333640884354899108,"P112$F33:SF#114/S2#7/S1#30/R2#15/R1#8/F#33");
F(2051728264573823998,"P110$F25:SF#19/S2#18/S1#106/R2#49/R1#41/F#25");
F(2929071052112294734,"P61$F30:SF#37/S2#44/S1#10/R2#5/R1#59/F#30");
F(8106243647914800593,"P108$F22:SF#31/S2#108/S1#123/R2#25/R1#13/F#22");
F(3149175785920536109,"P1$F3:SF#24/S2#23/S1#13/R2#8/R1#319/F#3");
F(2722494966872291118,"P121$F30:SF#152/S2#169/S1#34/R2#8/R1#39/F#30");
F(3996160279591152571,"P96$F6:SF#71/S2#51/S1#72/R2#3/R1#16/F#6");
F(4500051439450918813,"P95$F4:SF#34/S2#71/S1#9/R2#9/R1#52/F#4");
F(8480565575224790567,"P2$F4:SF#9/S2#5/S1#26/R2#28/R1#28/F#4");
F(7922226651620865144,"P64$F33:SF#23/S2#26/S1#71/R2#27/R1#34/F#33");
F(8605028194857071659,"P58$F26:SF#25/S2#38/S1#48/R2#11/R1#36/F#26");
F(3348187038472013964,"P94$F3:SF#66/S2#27/S1#30/R2#20/R1#63/F#3");
F(1979089010313035082,"P65$F37:SF#72/S2#58/S1#42/R2#49/R1#358/F#37");
F(2269391064355256569,"P19$F21:SF#65/S2#63/S1#35/R2#27/R1#47/F#21");
F(8761529038682986805,"P77$F5:SF#8/S2#4/S1#9/R2#25/R1#10/F#5");
F(5634935468252743455,"P103$F43:SF#15/S2#86/S1#48/R2#33/R1#46/F#43");
F(8716459999899360518,"P88$F41:SF#69/S2#9/S1#43/R2#3/R1#398/F#41");
F(4906174980045686815,"P92$F49:SF#17/S2#109/S1#76/R2#27/R1#358/F#49");
F(518264757723947022,"P8$F10:SF#2/S2#14/S1#3/R2#7/R1#9/F#10");
F(3689351537514252555,"P76$F3:SF#4/S2#11/S1#13/R2#13/R1#11/F#3");
F(1696497682848118832,"P98$F28:SF#6/S2#32/S1#6/R2#7/R1#4/F#28");
F(358088453232469641,"P87$F40:SF#24/S2#45/S1#2/R2#23/R1#28/F#40");
F(9059962104126311850,"P106$F1:SF#36/S2#13/S1#7/R2#16/R1#22/F#1");
F(3565765928387819943,"P50$F8:SF#7/S2#15/S1#25/R2#2/R1#32/F#8");
F(3764455372616306544,"P81$F18:SF#40/S2#60/S1#25/R2#40/R1#59/F#18");
F(8732772320228686716,"P75$F2:SF#15/S2#8/S1#49/R2#22/R1#2/F#2");
F(615415217654952115,"P115$F47:SF#122/S2#37/S1#69/R2#9/R1#107/F#47");
F(374710556796455101,"P78$F7:SF#10/S2#15/S1#36/R2#15/R1#12/F#7");
F(3727338598896455997,"P71$F44:SF#33/S2#89/S1#39/R2#43/R1#12/F#44");
F(1853833947797224565,"P101$F39:SF#55/S2#85/S1#90/R2#49/R1#114/F#39");
F(1436675812939672311,"P32$F34:SF#10/S2#6/S1#17/R2#40/R1#1/F#34");
F(8700522515626403835,"P56$F20:SF#21/S2#19/S1#3/R2#21/R1#20/F#20");
F(5996890845327264373,"P97$F20:SF#111/S2#54/S1#13/R2#27/R1#91/F#20");
F(6844454606269348763,"P91$F48:SF#102/S2#96/S1#39/R2#19/R1#53/F#48");
F(4587054720487750387,"P55$F17:SF#30/S2#14/S1#30/R2#6/R1#24/F#17");
F(291904331307300408,"P80$F10:SF#74/S2#14/S1#16/R2#26/R1#3/F#10");
F(1728623839048049671,"P118$F10:SF#47/S2#56/S1#47/R2#15/R1#71/F#10");
F(3216796860147594219,"P122$F50:SF#53/S2#33/S1#11/R2#28/R1#5/F#50");
F(9088040212528640918,"P83$F29:SF#3/S2#59/S1#22/R2#11/R1#14/F#29");
F(1449450423663229787,"P21$F23:SF#4/S2#35/S1#27/R2#11/R1#374/F#23");
F(1522020940935623669,"P11$F13:SF#25/S2#22/S1#28/R2#26/R1#448/F#13");
F(3833454016922877817,"P0$F2:SF#8/S2#4/S1#9/R2#3/R1#3/F#2");
F(3150556648271988537,"P18$F20:SF#13/S2#28/S1#10/R2#22/R1#15/F#20");
F(8871890116301233587,"P66$F38:SF#11/S2#4/S1#4/R2#53/R1#2/F#38");
F(3271880416360100522,"P34$F36:SF#32/S2#128/S1#77/R2#51/R1#46/F#36");
F(8344945788573388734,"P63$F32:SF#31/S2#71/S1#36/R2#54/R1#13/F#32");
F(102240276918644762,"P47$F49:SF#29/S2#76/S1#23/R2#46/R1#463/F#49");
F(3536758854113636176,"P15$F17:SF#15/S2#36/S1#5/R2#4/R1#11/F#17");
F(3460849201766694060,"P27$F29:SF#48/S2#7/S1#6/R2#38/R1#12/F#29");
F(8084023361084594523,"P62$F31:SF#94/S2#82/S1#34/R2#25/R1#42/F#31");
F(683756081101346872,"P51$F11:SF#6/S2#13/S1#14/R2#12/R1#19/F#11");
F(3850319760998561640,"P114$F43:SF#227/S2#45/S1#145/R2#21/R1#50/F#43");
F(8870750368799424251,"P17$F19:SF#14/S2#44/S1#24/R2#31/R1#21/F#19");
F(3962975465164012838,"P22$F24:SF#28/S2#83/S1#41/R2#29/R1#13/F#24");
F(7442620279046613291,"P4$F6:SF#41/S2#20/S1#18/R2#6/R1#6/F#6");
F(4175726842541059153,"P109$F24:SF#6/S2#75/S1#2/R2#20/R1#385/F#24");
F(8379524054135420019,"P105$F46:SF#97/S2#26/S1#18/R2#17/R1#41/F#46");
F(8120394315975258126,"P26$F28:SF#71/S2#30/S1#99/R2#47/R1#359/F#28");
F(4302473631954582197,"P49$F4:SF#8/S2#24/S1#27/R2#39/R1#17/F#4");
F(212090336140879670,"P13$F15:SF#44/S2#12/S1#33/R2#16/R1#396/F#15");
F(3223024948381295921,"P116$F2:SF#22/S2#145/S1#17/R2#20/R1#7/F#2");
F(7836883138898444156,"P25$F27:SF#30/S2#62/S1#44/R2#34/R1#74/F#27");
F(2915851568428815399,"P120$F16:SF#10/S2#29/S1#36/R2#14/R1#352/F#16");
F(4499511218802027691,"P14$F16:SF#33/S2#2/S1#14/R2#15/R1#356/F#16");
F(1093060278561111804,"P113$F35:SF#10/S2#32/S1#14/R2#27/R1#70/F#35");
F(5436250638788403094,"P117$F8:SF#107/S2#113/S1#106/R2#33/R1#29/F#8");
F(826472083268504889,"P119$F12:SF#1/S2#2/S1#7/R2#1/R1#4/F#12");
F(8071974511728020083,"P69$F41:SF#87/S2#12/S1#16/R2#18/R1#14/F#41");
F(8409123960690519868,"P52$F12:SF#54/S2#57/S1#51/R2#28/R1#363/F#12");
F(2755702872347980549,"P54$F16:SF#17/S2#27/S1#24/R2#19/R1#41/F#16");
F(525358987593721591,"P7$F9:SF#16/S2#21/S1#4/R2#2/R1#444/F#9");
F(4430258625995802584,"P90$F45:SF#72/S2#31/S1#116/R2#46/R1#39/F#45");
F(8074142127377382841,"P60$F28:SF#64/S2#90/S1#72/R2#41/R1#350/F#28");
F(633935087540315413,"P39$F41:SF#21/S2#19/S1#43/R2#5/R1#19/F#41");
F(3785045801860686323,"P29$F31:SF#5/S2#16/S1#45/R2#10/R1#20/F#31");
F(2533341363646573488,"P28$F30:SF#17/S2#37/S1#22/R2#42/R1#58/F#30");
F(3505875955087978745,"P79$F8:SF#78/S2#26/S1#52/R2#24/R1#13/F#8");
F(7280766383815795891,"P46$F48:SF#159/S2#50/S1#111/R2#48/R1#65/F#48");
F(4250617614383712757,"P59$F27:SF#20/S2#31/S1#6/R2#32/R1#21/F#27");
F(1152085751713715843,"P31$F33:SF#128/S2#59/S1#21/R2#45/R1#117/F#33");
F(7004909902027374588,"P30$F32:SF#26/S2#34/S1#56/R2#18/R1#45/F#32");
F(4030818931071720317,"P10$F12:SF#7/S2#11/S1#19/R2#17/R1#4/F#12");
F(8080923084788673508,"P89$F43:SF#28/S2#23/S1#10/R2#29/R1#68/F#43");
F(9058764065358034539,"P5$F7:SF#42/S2#9/S1#31/R2#23/R1#395/F#7");
F(6127486445251208455,"P20$F22:SF#18/S2#75/S1#32/R2#32/R1#434/F#22");
F(3065109694916662809,"P72$F45:SF#65/S2#69/S1#11/R2#15/R1#417/F#45");
F(1543088340243188981,"P23$F25:SF#108/S2#142/S1#83/R2#50/R1#162/F#25");
F(8746318741595700650,"P37$F39:SF#68/S2#13/S1#16/R2#39/R1#14/F#39");
F(2118782767138934471,"P74$F50:SF#108/S2#91/S1#128/R2#42/R1#63/F#50");
F(1071063946873213563,"P93$F50:SF#30/S2#35/S1#20/R2#63/R1#32/F#50");
F(3472315222775129608,"P86$F36:SF#105/S2#47/S1#100/R2#32/R1#65/F#36");
F(7454877562384112944,"P24$F26:SF#36/S2#41/S1#50/R2#14/R1#37/F#26");
F(7605186896007146992,"P102$F40:SF#109/S2#128/S1#92/R2#23/R1#23/F#40");
F(8207599146966137136,"P35$F37:SF#69/S2#38/S1#37/R2#21/R1#68/F#37");
F(7420024698466272437,"P67$F39:SF#1/S2#16/S1#102/R2#56/R1#28/F#39");
F(481983214747415932,"P9$F11:SF#6/S2#8/S1#8/R2#12/R1#2/F#11");
F(2852222431183241661,"P48$F50:SF#34/S2#49/S1#29/R2#25/R1#35/F#50");
F(6591042691624237176,"P6$F8:SF#27/S2#27/S1#15/R2#49/R1#346/F#8");
F(5515274549505494775,"P100$F35:SF#1/S2#153/S1#28/R2#57/R1#77/F#35");
F(4769642512480278832,"P107$F20:SF#119/S2#92/S1#48/R2#11/R1#28/F#20");
F(4438360325863844911,"P43$F45:SF#19/S2#96/S1#11/R2#20/R1#8/F#45");
F(3045122694777801749,"P16$F18:SF#73/S2#60/S1#72/R2#36/R1#36/F#18");
F(3781969917583063870,"P45$F47:SF#35/S2#29/S1#20/R2#24/R1#410/F#47");
F(5093900432000630021,"P42$F44:SF#39/S2#58/S1#86/R2#60/R1#375/F#44");
F(3614478776726664725,"P36$F38:SF#31/S2#74/S1#30/R2#19/R1#5/F#38");
F(6884362462074309660,"P111$F27:SF#74/S2#148/S1#115/R2#32/R1#65/F#27");
F(8822346595876278077,"P53$F15:SF#79/S2#20/S1#37/R2#30/R1#88/F#15");
F(4542230141385651060,"P57$F25:SF#32/S2#79/S1#7/R2#10/R1#78/F#25");
F(1992411951459161112,"P33$F35:SF#79/S2#114/S1#84/R2#37/R1#56/F#35");
F(8444257093304344763,"P44$F46:SF#12/S2#26/S1#109/R2#83/R1#24/F#46");
F(8997477031145374745,"P41$F43:SF#23/S2#18/S1#25/R2#41/R1#32/F#43");
F(8501431019621410567,"P68$F40:SF#28/S2#56/S1#73/R2#22/R1#85/F#40");
F(5292112373119522237,"P12$F14:SF#61/S2#88/S1#60/R2#30/R1#81/F#14");
F(5637984672042323491,"P104$F44:SF#126/S2#78/S1#26/R2#22/R1#45/F#44");
F(3257874067989411303,"P84$F33:SF#39/S2#85/S1#135/R2#34/R1#42/F#33");
F(365140544936491441,"P99$F32:SF#138/S2#205/S1#121/R2#50/R1#126/F#32");
F(3458001445040763107,"P38$F40:SF#38/S2#42/S1#96/R2#87/R1#66/F#40");
F(8709724615131885704,"P40$F42:SF#47/S2#48/S1#58/R2#74/R1#370/F#42");
F(8887630167449802853,"P73$F47:SF#40/S2#130/S1#65/R2#105/R1#102/F#47");
auto&pack=out;

pack.push_back({1851330153364523319,"S1T3"});
    pack.push_back({3394414841364511096,"2025.10.27 18:25"});
    pack.push_back({35466097406721398,"2025.10.27 18:30"});
    pack.push_back({7230349191312921973,"2025.10.27 18:41"});
    pack.push_back({1484072599116778639,"2025.10.27 20:28"});
    pack.push_back({7861040937335919927,"2025.10.27 20:59"});

    pack.push_back({2230705993315794070,"2025.10.27 21:29"});
    pack.push_back({5983905796038006074,"2025.10.27 21:30"});

    pack.push_back({2028880712679374100,"2025.10.27 21:48"});
    pack.push_back({4583900382366612526,"2025.10.27 21:49"});

    pack.push_back({9016065344887682457,"2025.10.27 22:43"});

    pack.push_back({4514907602418277684,"2025.10.27 22:49"});

    pack.push_back({8520640178882611465,"2025.10.27 23:02"});//TOP1
    
    pack.push_back({3647455665317635386,"2025.10.28 00:07A"});
    pack.push_back({8664737233344879803,"2025.10.28 00:07B"});
    pack.push_back({2915529543113062660,"2025.10.28 00:07C"});

    pack.push_back({5132816298200869272,"28T011"});
    pack.push_back({9158987093226966667,"28T014"});

    pack.push_back({449296975738600220,"28T730F9"});

    pack.push_back({6237213067934923416,"28T737"});

    pack.push_back({5167947768330679742,"28T750A"});
    pack.push_back({6934259240249468717,"28T750B"});

    pack.push_back({1984716473859246653,"28T804A"});
    pack.push_back({2529909358112302879,"28T804B"});

    pack.push_back({621281987722609471,"28T812"});
    
    pack.push_back({7394413644357511870,"28T831:F8"});
    pack.push_back({4944450726795357241,"28T833:F5"});

    pack.push_back({4255380668872016778,"28T111F15"});
    
F(1701352111188098075,"28T950G141");
F(8289440453124306079,"28T950G99");
F(1924790453427444521,"28T950G173:full-state");
F(2925114617117833788,"28T950G65");
F(3207999305911896858,"28T950G94");
F(8226406958104322492,"28T950G100");
F(9155106235109806376,"28T950G68");
F(7425435343269792557,"28T950G93");
F(3832316146223114121,"28T950G66");
F(2467713695247922620,"28T950G174");
F(7218530421494536734,"28T950G101");
F(8086545983892035333,"28T950G102");
F(1047429132702200636,"28T950G235");
F(4362329267687332103,"28T950G206");
F(4588160971550837305,"28T950G148");
F(5411390761601607855,"28T950G193");
F(5262806553596797837,"28T950G83");
F(6785518398889070602,"28T950G60");
F(2426384222256048776,"28T950G69");
F(6574801550966537881,"28T950G175");
F(5340235872347317934,"28T950G213");
F(1746624300555508237,"28T950G67");
F(2170239097848347399,"28T950G56");
F(4079241109619506361,"28T950G178");
F(3115709323930049178,"28T950G209");
F(4908212798392912303,"28T950G199");
F(9046651494530240810,"28T950G157");
F(4808461338654169992,"28T950G121");
F(8511348911404638751,"28T950G211");
F(1096393344522009638,"28T950G236");
F(8554965858150604681,"28T950G192");
F(6056068867006742843,"28T950G190");
F(443640419708440860,"28T950G200");
F(5925348467154704798,"28T950G142");
F(1632343442380626876,"28T950G135");
F(667408751461220151,"28T950G212");
F(1484001769258571964,"28T950G167");
F(9056214625642232349,"28T950G237");
F(2673251070121951934,"28T950G210");
F(1530728771139355037,"28T950G201");
F(3974229430185569706,"28T950G108");
F(2849713322666236420,"28T950G117");
F(8587888113426716186,"28T950G198");
F(1988407117444249915,"28T950G53");
F(579604458055365032,"28T950G143");
F(6204958757810146361,"28T950G54");
F(8946421245921594647,"28T950G84");
F(832677943104516649,"28T950G131");
F(6205467358951924008,"28T950G214");
F(5853917057679625144,"28T950G185");
F(1808242147655166774,"28T950G169");
F(4127859756804947734,"28T950G85");
F(7605567361108346399,"28T950G194");
F(8407711465113155518,"28T950G129");
F(3611629715920008630,"28T950G195");
F(4219340186387762220,"28T950G160");
F(7178270222799820044,"28T950G35");
F(8865458659704580524,"28T950G170");
F(3436873070027804308,"28T950G179");
F(742280051586514484,"28T950G202");
F(2789742025176268173,"28T950G197");
F(5780982598908597770,"28T950G130");
F(4543973883346754878,"28T950G228");
F(3002229819170492063,"28T950G205");
F(5335759196704736298,"28T950G55");
F(829518873442073604,"28T950G207");
F(5345037426036115997,"28T950G191");
F(4659086026660854570,"28T950G186");
F(6894284373812325544,"28T950G188");
F(4656477572688583195,"28T950G204");
F(3210630772378326825,"28T950G176");
F(5780488916483113912,"28T950G187");
F(1814724834739369501,"28T950G45");
F(2381603466955601801,"28T950G208");
F(2798451998441544374,"28T950G132");
F(435852151732464302,"28T950G203");
F(8117811481656638620,"28T950G46");
F(7938722618509117962,"28T950G227");
F(1854495720906176956,"28T950G49");
F(2539019919510346506,"28T950G50");
F(1012314553872709821,"28T950G57");
F(6708486755009386136,"28T950G52");
F(4090741906513922310,"28T950G48");
F(1448231503493275060,"28T950G122");
F(7208924594834376326,"28T950G145");
F(2489991059260145196,"28T950G196");
F(1339873850586656575,"28T950G51");
F(841130290979605129,"28T950G238");
F(1096649066895010220,"28T950G146");
F(3166920902176611751,"28T950G189");
F(1518042056217279620,"28T950G161");
F(1589274479891320501,"28T950G171");
F(1641624878810017158,"28T950G144");
F(3284305744354673974,"28T950G172");
F(8587279926387565851,"28T950G58");
F(8618011158950647446,"28T950G242");
F(828414710119868935,"28T950G47");
F(1685239163075696157,"28T950G177");
F(2244266777339561109,"28T950G147");
F(7434603838585573663,"28T950G168");
F(6559585968684885039,"28T950G43");
F(6564588261091190442,"28T950G41");
F(2128284161203396781,"28T950G42");
F(8047656131694458122,"28T950G158");
F(2738763847797799055,"28T950G159");
F(5808920106938164779,"28T950G59");
F(1519703350465887901,"28T950G81");
F(5556323983190265357,"28T950G109");
F(732517226059091338,"28T950G62");
F(123878324783746712,"28T950G36");
F(3078894844825309337,"28T950G86");
F(479471221201507972,"28T950G162");
F(909260558574299149,"28T950G118");
F(2174489406620966926,"28T950G104");
F(299849040111608589,"28T950G61");
F(368757762073185310,"28T950G95");
F(6162155152179161997,"28T950G97");
F(5642772350223978638,"28T950G74");
F(4772494058181120924,"28T950G71");
F(4047947351473274635,"28T950G44");
F(355549792787983785,"28T950G180");
F(459661882848184629,"28T950G70");
F(5976342075430162840,"28T950G78");
F(8253733846539770009,"28T950G239");
F(1742996476207051791,"28T950G77");
F(9130298980599071503,"28T950G82");
F(1559671181246362168,"28T950G215");
F(7898286931471183255,"28T950G96");
F(1678744713256317709,"28T950G110");
F(8869331470995233463,"28T950G79");
F(8432434289376192909,"28T950G217");
    //for(auto&ex:genpack)pack.push_back(ex);
F(8015372731631031321,"28T11H167");
F(4331668166098578202,"28T11H195");
F(2098132876422546335,"28T11H197");
F(8511244229379183416,"28T11H189");
F(1304396322580675643,"28T11H198");
F(4084507129891673887,"28T11H190");
F(4227269396348610845,"28T11H177");
F(2234647025248710149,"28T11H204");
F(1277626503706267071,"28T11H202");
F(7861935562002558735,"28T11H207");
F(4253945334388956314,"28T11H201");
F(2033722369789134986,"28T11H185");
F(1052825987172604825,"28T11H184");
F(6394958145688334859,"28T11H200");
F(6246016162968977320,"28T11H203");
F(1953264445729367726,"28T11H192");
F(2706202324774761275,"28T11H199");
F(3170582920395427486,"28T11H196");
F(8986990881148526649,"28T11H178");
F(3941243380669488822,"28T11H191");
F(1654576666973795738,"28T11H180");
F(3918989333724725258,"28T11H186");
F(1576588834264006921,"28T11H181");
F(3476218354750663574,"28T11H179");
F(2671531372690427801,"28T11H205");
F(2679130111467290009,"28T11H182");
F(3137053716630023823,"28T11H206");
F(2561801273667822856,"28T11H168");
F(2170296569906686238,"28T11H169");
F(7609747721521281306,"28T11H183");

F(6212837779835720731,"28T4k245");
F(7136043903525338686,"28T4k276");
F(1525098717752076090,"28T4k244");
F(4545861577788950430,"28T4k246");
F(592894115062813978,"28T4k247");
F(4310816756901678518,"28T4k266");
F(6637257088542988089,"28T4k248");
F(5768324676192071480,"28T4k267");
F(5415626019968729626,"28T4k269");
F(7142535531263050031,"28T4k249");
F(4593443738769782555,"28T4k277");
F(4433558439330056367,"28T4k265");
F(4340103044693632142,"28T4k278");
F(1422067369924898847,"28T4k263");
F(1276775017767397310,"28T4k264");
F(5379648201193848477,"28T4k268");
F(1418453615231111581,"28T4k280");
F(269769221293766937,"28T4k279");
F(3795493378665769356,"28T4k270");
F(805645572355525655,"28T4k250");
F(4557375015278939557,"28T4k228");
F(5372335580261538314,"28T4k231");
F(146776131107764923,"28T4k229");
F(3147300075103463435,"28T4k251");
F(3970776864393734061,"28T4k281");
F(421418703689575486,"28T4k285");
F(1617686196610993449,"28T4k283");
F(895107980220241439,"28T4k282");
F(4441683414275134734,"28T4k287");
F(702372010132862990,"28T4k284");
F(2036540959771792644,"28T4k286");
F(3474613472291747355,"28T4k226");
F(3611664443632208954,"28T4k204");
F(4578769320796438044,"28T4k197");
F(4052726868366742046,"28T4k206");
F(1089428119695025082,"28T4k198");
F(691374196379299129,"28T4k208");
F(4015937879642152079,"28T4k205");
F(551986397469678116,"28T4k199");
F(1372780654100163741,"28T4k207");
F(8984136561539764764,"28T4k227");
F(8952044513777170696,"28T4k209");
F(1457229719076014138,"28T4k288");

F(2200056371502406970,"28T12h325");
F(1767397442918759063,"28T12h300");
F(340933943346341311,"28T12h301");
F(6060837100649672847,"28T12h319");
F(4181357109321275526,"28T12h306");
F(8763171566847215675,"28T12h324");
F(4263330074633117977,"28T12h320");
F(8916629455898616840,"28T12h270");
F(8586783989381739839,"28T12h316");
F(6717517478308561085,"28T12h292");
F(620999950541334330,"28T12h240");
F(6812540687280456602,"28T12h326");
F(4623646149886897306,"28T12h328");
F(2668792039715980425,"28T12h317");
F(7861042053582039215,"28T12h263");
F(2198653214297369381,"28T12h307");
F(2313526994092696106,"28T12h322");
F(3219330665692411678,"28T12h278");
F(8519820995765944745,"28T12h279");
F(688553963417197579,"28T12h289");
F(3887464618179972410,"28T12h318");
F(336491502342053772,"28T12h323");
F(2628145152065744824,"28T12h297");
F(3465816829278504222,"28T12h303");
F(1503932113963591973,"28T12h327");
F(305467505934012474,"28T12h298");
F(4132923850001482933,"28T12h283");
F(7646610661371153316,"28T12h264");
F(7255132468713301167,"28T12h241");
F(1444035333609622533,"28T12h291");
F(985174712546517001,"28T12h302");
F(1200797704696454030,"28T12h293");
F(1593213098376056331,"28T12h290");
F(352474378302917402,"28T12h305");
F(2308978980835710491,"28T12h304");
F(2924249297235155333,"28T12h242");
F(3975320056372136350,"28T12h321");
F(1478332738508905004,"28T12h308");
F(4434370704003328057,"28T12h243");
F(1102833253170286252,"28T12h280");
F(6822214138848362286,"28T12h299");
F(2066622455060763797,"28T12h271");
F(9067180376181770397,"28T12h311");

F(2395457730974195486,"A241");
F(4477450535232879510,"A216");
F(4267765610653813399,"A276");
F(3106722395763251501,"A242");
F(3866119682482511135,"A304");
F(5128819039599672334,"A50");
F(1782101762762502184,"A52");
F(1983835684061477560,"A141");
F(2926331206827719993,"A181");
F(478846820261955998,"A51");
F(2782470343405364783,"A217");
F(3825279242040117812,"A493");
F(7356952866224478109,"A0");
F(2611630108336422299,"A379");
F(2494784385334456377,"A354");
F(2132504202723342517,"A460");
F(7825974383875620767,"A277");
F(5699872788895398061,"A142");
F(2206279664145420221,"A380");
F(693412682702545946,"A396");
F(971102778835151909,"A143");
F(6708427807338211631,"A461");
F(7871732562423148333,"A441");
F(6710140193338637588,"A332");
F(661213344096269097,"A398");
F(2811694920532827285,"A97");
F(4401756512704923928,"A494");
F(7001715060518552095,"A305");
F(2422378941461778875,"A355");
F(6919822094977742761,"A144");
F(1998002978255614878,"A1");
F(836296972734910123,"A397");
F(412972969814013845,"A424");
F(3533105991163721880,"A2");
F(2164265975467560618,"A381");
F(4980558610881675180,"A453");
F(3137049174107315512,"A145");
F(5344418773900096906,"A480");
F(2817595488808279437,"A432");
F(3902155244397477006,"A444");
F(764809907860627997,"A98");
F(5627536554257772478,"A442");
F(1992925364057876538,"A454");
F(5849374630552149270,"A382");
F(2849439527911302541,"A278");
F(3979228325114157237,"A182");
F(1264466993675523995,"A100");
F(556489820621777317,"A486");
F(5061045460456399788,"A443");
F(4588115491486959243,"A433");
F(4116868081036565925,"A356");
F(1740426401149488681,"A491");
F(8870238078232370825,"A99");
F(727993488043310878,"A183");
F(2493328053329467785,"A487");
F(3286263986567660986,"A383");
F(5776470648390892420,"A401");
F(4362965209767615546,"A492");
F(6525266204494075302,"A399");
F(5300455473035500846,"A455");
F(6380551923844071437,"A434");
F(6056273486981573668,"A496");
F(1627804421061873469,"A446");
F(3768685673552744351,"A400");
F(467844633370376236,"A414");
F(3085827633520715398,"A415");
F(4025687320718418190,"A481");
F(1577736908968435643,"A462");
F(447280885309247644,"A495");
F(3790070625090563979,"A306");
F(7902169380358068366,"A357");
F(4401228756258718858,"A445");
F(8723038901618425095,"A473");
F(1674825632547021973,"A463");
F(5203370807634306215,"A243");
F(3831781461909650102,"A103");
F(1386867866819501717,"A447");
F(7793868014751455028,"A456");
F(5876750756423286797,"A359");
F(6481591041704623372,"A146");
F(5370578471076637204,"A457");
F(4181163025103077045,"A53");
F(2724162357681336716,"A358");
F(4626377251042825591,"A490");
F(1052782011084253111,"A435");
F(1636908236075911983,"A185");
F(6276622107093178810,"A147");
F(7460270571633915159,"A307");
F(2339953843393668901,"A280");
F(4433843328973609638,"A448");
F(8530408777410755879,"A279");
F(7249456993914412478,"A184");
F(3259293710176563645,"A281");
F(8933491891308734374,"A308");
F(1844805724342745014,"A6");
F(3055742117581846077,"A102");
F(871540447763189439,"A5");
F(293085258542186374,"A13");
F(3446477627191751740,"A54");
F(1301004113051342251,"A105");
F(5135558844058837176,"A107");
F(1055539001134810969,"A148");
F(2025604516163633061,"A3");
F(9112230267502994999,"A186");
F(3433266414862949817,"A244");
F(3801950424008183867,"A282");
F(1493545032684022575,"A101");
F(2208180397697537578,"A283");
F(9180961744077748909,"A245");
F(3951414267471471924,"A188");
F(1680504452086695605,"A104");
F(2701716781247519415,"A309");
F(7517438503792094214,"A11");
F(6361681468769268763,"A108");
F(5124823278263365244,"A106");
F(6511367074557734325,"A55");
F(2412819203791598983,"A284");
F(5439638452037058219,"A187");
F(3362266289141122713,"A436");
F(6433772701782472311,"A15");
F(3791780937068840597,"A464");
F(2024454598584893705,"A402");
F(5241974987582699067,"A219");
F(4006561333252802740,"A190");
F(7971137262290799386,"A482");
F(2953897285136886581,"A333");
F(7690286398502162765,"A4");
F(920713157872204150,"A16");
F(8960763597775725624,"A150");
F(6245942897444923513,"A21");
F(8714604381297129903,"A310");
F(4174901263510551941,"A193");
F(7591730561240890631,"A109");
F(5085721475016502405,"A218");
F(708817986801766840,"A220");
F(804193101307923366,"A7");
F(7682395603660837129,"A483");
F(4284994263794862459,"A59");
F(335282925271911612,"A111");
F(7731352605089889803,"A221");
F(1921649153452899676,"A56");
F(8675149440417887149,"A22");
F(1376774076247845464,"A200");
F(7461353768250983195,"A488");
F(7008223043964908966,"A189");
F(3644275891044118141,"A18");
F(6792089122887921578,"A58");
F(7381975514056834727,"A285");
F(4633946421757608460,"A10");
F(4088839838500468783,"A154");
F(8811950228165849899,"A247");
F(880018519443182378,"A311");
F(7831548960274406410,"A110");
F(8834147291563060766,"A416");
F(6419056899052484414,"A291");
F(4908722989777168143,"A151");
F(4931775388628752506,"A114");
F(2104668932981604026,"A360");
F(3161070293670040890,"A113");
F(8583103387654756157,"A426");
F(8406619041115147577,"A334");
F(7672177860204561802,"A466");
F(3377840291756989734,"A164");
F(3222964547980952584,"A250");
F(3124949747595287733,"A425");
F(3390660593353764116,"A9");
F(8724149315537501231,"A25");
F(7316207876658977850,"A192");
F(2791958426482530615,"A246");
F(7003787914789599534,"A14");
F(6631328072219231019,"A248");
F(6211936950291955257,"A222");
F(3395511913090940421,"A38");
F(7273130245125188133,"A288");
F(5559186946812294567,"A156");
F(1783509342731643398,"A336");
F(1461760204400827963,"A286");
F(1489700529780914312,"A8");
F(4133748694729636902,"A252");
F(2905702167918640406,"A314");
F(3845569093908254586,"A160");
F(5272384549465629304,"A20");
F(6234170284692487190,"A191");
F(2868341120295516798,"A80");
F(5795880465758759294,"A23");
F(3429495617691207050,"A437");
F(3926050394085465269,"A152");
F(6593278260601772325,"A12");
F(4723803915179404660,"A19");
F(1057571083213343771,"A226");
F(5161434991182024228,"A249");
F(6216777936621434005,"A66");
F(1885679093722470269,"A163");
F(8284713742805985913,"A211");
F(8801725580253294617,"A27");
F(2568792108633561972,"A40");
F(6928925556976416916,"A63");
F(2396873338922148981,"A26");
F(4707120003548776366,"A159");
F(611953929256960893,"A24");
F(3456316787171087221,"A32");
F(8841237287389901439,"A41");
F(3419690432274056347,"A115");
F(6822401567244573100,"A61");
F(2841534076975057710,"A57");
F(5256331352586878391,"A287");
F(7577690812740738614,"A112");
F(8200311052056808063,"A251");
F(7516302131282117546,"A206");
F(7717311913366468276,"A361");
F(4125426197986609019,"A17");
F(869835575940709143,"A77");
F(856576481373736564,"A60");
F(8016711469665027193,"A65");
F(2547101755329939737,"A197");
F(4002345552388714024,"A78");
F(4633155392196521780,"A335");
F(7005708460050883613,"A323");
F(912910960299936439,"A149");
F(8996356065862445241,"A158");
F(1422670194308183483,"A312");
F(93473976510456857,"A178");
F(8340796484897422885,"A33");
F(3114624887142951478,"A157");
F(8875864323001578613,"A67");
F(4819710158012622646,"A289");
F(2348923877058170025,"A479");
F(4842347998008060212,"A29");
F(8945313831493134381,"A317");
F(4704088683947173754,"A162");
F(6871160730194427782,"A363");
F(582699967300374909,"A35");
F(3891491812411406356,"A119");
F(1268724106429727097,"A227");
F(3867884240573519242,"A169");
F(4932978529181321247,"A64");
F(1938589188247858596,"A81");
F(2019661961481815339,"A31");
F(2894481726643839546,"A70");
F(8965894367398407736,"A69");
F(5741039345267931573,"A76");
F(1979139875250380189,"A120");
F(1699834339341401478,"A118");
F(2972428072463652733,"A198");
F(414113836145541049,"A196");
F(2416304414942644858,"A73");
F(4778728048032235962,"A135");
F(9211906004390723239,"A290");
F(2813630749624396472,"A71");
F(528934410851339132,"A201");
F(6439612259935980955,"A337");
F(4937639384659090606,"A256");
F(2090796257116516220,"A28");
F(2884580503780146301,"A472");
F(8256317240517688725,"A316");
F(3518456362232514680,"A155");
F(7012716476773564426,"A470");
F(3844478541056181803,"A79");
F(2704178192250049112,"A263");
F(476286036625526039,"A428");
F(2224286163690676621,"A121");
F(1828020958679739775,"A88");
F(584939962680302123,"A36");
F(121731950032275515,"A265");
F(3428675862778248085,"A318");
F(4332191267937599403,"A140");
F(5232352993446280614,"A339");
F(7636203016195828745,"A353");
F(8005495213564171380,"A202");
F(6505562126145954948,"A168");
F(2753163483784498555,"A194");
F(3251624769674609534,"A95");
F(349598137323965112,"A266");
F(5589591428433391485,"A166");
F(567777492891493750,"A42");
F(6234721016464609033,"A62");
F(2264563263109478580,"A404");
F(5852270783714899126,"A465");
F(5893056775566161173,"A204");
F(3128678496597518637,"A208");
F(4121390995151468326,"A292");
F(5694284241605172599,"A228");
F(5957804697246564982,"A87");
F(6678677208490061950,"A195");
F(815195229676830878,"A254");
F(4018148893529682200,"A123");
F(1234405534364795784,"A255");
F(2914795485528279413,"A47");
F(4868722687714793909,"A315");
F(5928474579608224153,"A338");
F(3242660104619577270,"A117");
F(8978299459917718667,"A165");
F(1547349109856141577,"A210");
F(714470397333023869,"A89");
F(599680149097747828,"A128");
F(3327890681448319094,"A340");
F(1812808720482061948,"A267");
F(32973496755497743,"A161");
F(3190879273780932220,"A72");
F(8722153512793228553,"A232");
F(4081396448205547384,"A37");
F(3188623170104229140,"A321");
F(2843248104593784375,"A74");
F(8418690605362277758,"A84");
F(1916859890287127958,"A458");
F(6152020123749538492,"A384");
F(1109099614317518198,"A86");
F(7843345375494144378,"A325");
F(7801456894117044122,"A92");
F(3820475562914949055,"A253");
F(3706792198209306379,"A230");
F(3824182702267106943,"A82");
F(6601169678485714951,"A207");
F(3119430800380530296,"A83");
F(8723611564184699567,"A235");
F(5226716037879703126,"A130");
F(5425199117586293771,"A68");
F(7335889449344108663,"A48");
F(4287171919154728382,"A403");
F(2007861661634814845,"A34");
F(640927131063827748,"A262");
F(6901242035103626110,"A137");
F(5163099360273629819,"A302");
F(8167074224009208184,"A75");
F(4197119751436391853,"A459");
F(8705242012840773021,"A322");
F(3911979320877541820,"A418");
F(3305472098713368087,"A90");
F(3049030746527061149,"A293");
F(6721478512987226639,"A313");
F(950342556877940830,"A275");
F(1267767663874963749,"A30");
F(2136999376043189652,"A94");
F(7563892439434557333,"A236");
F(4622141352971274249,"A478");
F(1018669078533263433,"A96");
F(5066096871762311447,"A341");
F(1799199398870009278,"A177");
F(2965718418777462649,"A133");
F(9031192089726356376,"A153");
F(3893944632137634426,"A179");
F(1850419809223473334,"A122");
F(2937497497373987962,"A132");
F(6912222764104552824,"A239");
F(1840328071678722166,"A39");
F(6205804857552739198,"A261");
F(3386735791585313397,"A175");
F(5590425207104352037,"A49");
F(8477750892753127804,"A273");
F(2745544010634329439,"A125");
F(4927328593861423531,"A46");
F(4753125463859399691,"A320");
F(583070007636459901,"A294");
F(6418576427925842814,"A203");
F(7245252471140212923,"A348");
F(8938241245857912228,"A474");
F(8813954663481572870,"A126");
F(6430861219817666935,"A209");
F(3449571438572802843,"A319");
F(7553225416647785243,"A116");
F(3361989430548828088,"A167");
F(823604888714485294,"A349");
F(7903654129594537611,"A449");
F(1724623665296400055,"A350");
F(6522751673048830091,"A477");
F(4245879393473135990,"A324");
F(741790525267918967,"A44");
F(1240249847470621375,"A362");
F(6320256811557848212,"A199");
F(1918269633349631165,"A212");
F(1690322295854156927,"A139");
F(4586970712822190455,"A326");
F(1831378010777075580,"A85");
F(941826770491409212,"A234");

F(2863245489654886062,"B210");
F(512866362805081741,"B303");
F(4285765157828038068,"B242");
F(7642106340715463870,"B354");
F(7289977160428383533,"B174");
F(7902460662277304762,"B0");
F(4479517219081683610,"B173");
F(8911815097756231439,"B50");
F(7244044600796073787,"B135");
F(5411984980026332555,"B94");
F(6308451323132325047,"B95");
F(5925952226828035976,"B211");
F(1412836966197374598,"B136");
F(53207198928037656,"B284");
F(4598525792431835942,"B241");
F(4598521600249761716,"B338");
F(572284764336641290,"B212");
F(9086351147631645736,"B339");
F(8000740594029563398,"B175");
F(751039031139707526,"B176");
F(4942715619989335065,"B340");
F(1514474705474031535,"B177");
F(5961922632863127705,"B318");
F(1382104941077485833,"B285");
F(1600842262516364317,"B178");
F(1996526174239600134,"B3");
F(4231239297330131211,"B319");
F(2961696236861260316,"B1");
F(680904578315923236,"B243");
F(910299098898771111,"B96");
F(4545298670918909719,"B51");
F(7834913766157865103,"B320");
F(7867052547475514790,"B99");
F(7793562206974261294,"B332");
F(3928601901639928620,"B213");
F(551454272875600443,"B4");
F(2999121010648286600,"B97");
F(5056488754882836235,"B347");
F(1946811836778756362,"B2");
F(2631551433267831581,"B179");
F(2885178001942133908,"B304");
F(7425932954201515564,"B355");
F(4510473822386529963,"B137");
F(2817576157278992568,"B244");
F(2782458339312277046,"B98");
F(1525383400751760046,"B264");
F(331598530663641099,"B138");
F(6600048520804961204,"B305");
F(2142076486391106380,"B100");
F(8518075687538856123,"B53");
F(4439814385533721140,"B348");
F(570615680792402470,"B286");
F(2931065990585536171,"B52");
F(2975785468718167212,"B248");
F(5438682899182127147,"B5");
F(333330300532570765,"B245");
F(3453479393447124389,"B321");
F(8493626007809501492,"B103");
F(6878751057558136248,"B55");
F(5281382243841558188,"B54");
F(1571272653235166500,"B102");
F(1714244277391727645,"B341");
F(3920113026361933195,"B246");
F(664626978024733768,"B56");
F(2637148394516476059,"B353");
F(9023893914736403725,"B101");
F(2598344193620647063,"B352");
F(414941482051249560,"B247");
F(1305539697615116696,"B139");
F(3020083336730663222,"B7");
F(26552275071432895,"B58");
F(3191774500494976533,"B6");
F(4644977542662981238,"B86");
F(2386739011432821948,"B106");
F(3623260572737936445,"B105");
F(7253749286418667300,"B57");
F(8031156568488179476,"B113");
F(7191133404668458408,"B59");
F(720029638089521421,"B349");
F(7325127361530716318,"B181");
F(1161725939610909302,"B73");
F(1232358394604384955,"B107");
F(2344417700466148660,"B10");
F(6006700782058541886,"B104");
F(2686777819967937085,"B61");
F(2074256836012689065,"B69");
F(5702812676520303755,"B9");
F(3043944840238560517,"B68");
F(8805683277467555500,"B63");
F(8408020004409998909,"B182");
F(6020246417579516223,"B60");
F(894266290745474712,"B75");
F(3930802637699419301,"B333");
F(6336066821582890878,"B78");
F(6606289495028087096,"B64");
F(2764655721801340859,"B180");
F(1986500594756845335,"B79");
F(5440737272476087742,"B65");
F(7865632025539542412,"B62");
F(170622928328944296,"B12");
F(7444240982020348072,"B214");
F(5783837681748049047,"B70");
F(205240046931638822,"B117");
F(3047297980132185272,"B8");
F(7582977452358854430,"B249");
F(3808418541623802029,"B116");
F(1215441076320087726,"B76");
F(8858917747411925108,"B18");
F(515388963524386204,"B216");
F(6187767587302801790,"B88");
F(1232367221850640906,"B14");
F(6489693231137912324,"B71");
F(7483984981582490421,"B13");
F(817418907700247098,"B215");
F(5168580709429640826,"B89");
F(3622303314215386934,"B111");
F(7379522683981681718,"B11");
F(773337170949981748,"B110");
F(5100636165220283574,"B93");
F(2144625648971237812,"B108");
F(4564236946410192653,"B28");
F(5350109789930682518,"B81");
F(2634133001033252244,"B21");
F(1343506169543069556,"B19");
F(1577739001838247284,"B35");
F(2144366185528953380,"B109");
F(1024309023112001317,"B36");
F(361722078685375272,"B128");
F(8269583693994461860,"B122");
F(6372164251975692715,"B90");
F(5552114421319948917,"B92");
F(8168160677915921692,"B30");
F(5662184537199611821,"B119");
F(3405591411627142779,"B32");
F(8783836732276674934,"B84");
F(3909805149224045077,"B115");
F(1266098308156054184,"B66");
F(1321307935283573915,"B20");
F(2940012987122542618,"B16");
F(4243613464403993726,"B118");
F(8953177311889262971,"B27");
F(3498538218711090038,"B131");
F(8635090818458135933,"B43");
F(1643610896929719925,"B42");
F(2428982879634719350,"B133");
F(5202231729599224695,"B129");
F(1285239677947031317,"B23");
F(5023595226410871893,"B112");
F(7880308092278037366,"B34");
F(379257639058295934,"B44");
F(3947493490786574457,"B132");
F(3372193518731163263,"B130");
F(4645239909416119688,"B114");
F(3459984393139748022,"B200");
F(1269760785689036052,"B15");
F(8219758916319263097,"B29");
F(5901765454304073918,"B46");
F(3862400515496887053,"B124");
F(943053757917371774,"B45");
F(5624803336821020790,"B120");
F(4341537933054078295,"B26");
F(8959144104682606709,"B37");
F(2785298585100834686,"B167");
F(3501835580901773431,"B38");
F(4796385117125158436,"B218");
F(4379790049793684156,"B25");
F(7999361248058946980,"B186");
F(404026565900581752,"B33");
F(919368545528926838,"B125");
F(1957129344634278006,"B123");
F(6279167025281385871,"B217");
F(5023782033514200504,"B142");
F(6795448714107181101,"B141");
F(7111862774033354363,"B127");
F(3336368461310415734,"B31");
F(1878062936384684410,"B47");
F(2183247746996911485,"B134");
F(695910906742521780,"B140");
F(3526682177620702079,"B41");
F(6200637160392498955,"B22");
F(5735344837995600975,"B17");
F(926961395223183140,"B185");
F(1009494724117738590,"B40");
F(1634572547497818132,"B157");
F(5342469981319271211,"B223");
F(3404806535260743086,"B183");
F(8168219631372737212,"B143");
F(5904563359448315162,"B189");
F(4921034396664533790,"B184");
F(9164828919947867005,"B150");
F(5964240579588734645,"B224");
F(8597688610347510696,"B148");
F(3990786963912083877,"B219");
F(3644067285003803012,"B153");
F(8400137685290271353,"B152");
F(469019946640308405,"B233");
F(5042654788629963528,"B187");
F(209782173383032713,"B154");
F(776410971254377086,"B151");
F(5946856072227533355,"B203");
F(5369430971806723630,"B145");
F(8809103656030257726,"B146");
F(6032099883368347572,"B156");
F(5322760173015411348,"B222");
F(5551007732465153978,"B350");
F(121649547123588245,"B155");
F(8492184277542196606,"B160");
F(2544554187157887100,"B161");
F(5849633794372812201,"B159");
F(4970101551350625150,"B158");
F(8831054306230338601,"B287");
F(2358773629826136379,"B144");
F(3319285980684621702,"B220");
F(6609108328257046580,"B149");
F(7132666844646347181,"B147");
F(4294554005760075183,"B306");
F(110734072368147247,"B307");
F(6825828724719375114,"B250");
F(1016818677269863957,"B325");
F(8293751698981802396,"B351");
F(84563505756120332,"B356");
F(7794049374293076511,"B288");
F(1339043784445268095,"B365");
F(3807238881469427068,"B364");
F(3830072195158379166,"B239");
F(8577457381368278679,"B322");
F(1712784592712829447,"B357");
F(6168555072822458779,"B291");
F(483887766998245528,"B358");
F(7640495968345198606,"B265");
F(6738838829753970366,"B290");
F(3324880622517298598,"B289");
F(8645538902055989175,"B292");
F(640178306790421773,"B360");
F(2657487392333257146,"B359");
F(4598840601173311407,"B293");
F(3797744555858012199,"B363");
F(8838953407806384300,"B361");
F(6064443851596507564,"B362");

F(4519662436002698555,"D184");
F(1229286475397035320,"D214");
F(6574724985894164538,"D201");
F(1661061062400412713,"D217");
F(332706206620518330,"D173");
F(1277994840958332205,"D110");
F(1701828603525668410,"D206");
F(4988878125323146398,"D154");
F(800632732223491483,"D0");
F(1839726375549019317,"D213");
F(8808283970813569720,"D189");
F(796913020222394890,"D215");
F(773792904846521787,"D216");
F(678100724473347903,"D50");
F(3348718480010063140,"D186");
F(594250490798044172,"D197");
F(4040354041410358949,"D210");
F(3980359765354032652,"D211");
F(183646721685399325,"D207");
F(4334730597848845844,"D77");
F(6456846985004131245,"D193");
F(2525708631987078057,"D156");
F(1726582217868328503,"D1");
F(6528053061432836282,"D202");
F(1783439648096716079,"D166");
F(7831616399403996090,"D185");
F(8483982404679852718,"D174");
F(3643210600933114431,"D2");
F(7979356102730938155,"D190");
F(8795023392700636811,"D198");
F(5020501738450849049,"D157");
F(1979383782946704302,"D172");
F(2465803939904175886,"D183");
F(2201999268778674202,"D175");
F(4152048398456793533,"D205");
F(8483221548866408250,"D124");
F(5298526241113782077,"D78");
F(3713793754182586808,"D179");
F(985789746444270526,"D194");
F(4659278492005698620,"D164");
F(6379674517407472911,"D200");
F(1413696707624574139,"D140");
F(2529617376873039801,"D208");
F(2302796718443269780,"D195");
F(8592078749122392490,"D155");
F(1489882352672575386,"D158");
F(445375383085982126,"D176");
F(8119783544621784201,"D79");
F(5478703417344854975,"D209");
F(5772887657393624213,"D199");
F(1488455656148988444,"D196");
F(6849748106464931752,"D171");
F(294167898572401564,"D191");
F(5628693292479940756,"D188");
F(2637178202057234858,"D177");
F(5555493705582728104,"D51");
F(3286551500109383334,"D192");
F(3647658597250054678,"D126");
F(3654959260115606302,"D212");
F(3894834722612471212,"D95");
F(4572589060184610439,"D159");
F(2309260407638608301,"D180");
F(3295268278938114058,"D203");
F(1803525323965087157,"D218");
F(7474918884385835791,"D204");
F(1947312291146642438,"D187");
F(483590800056673960,"D3");
F(4259008502773867678,"D52");
F(8556017579086657940,"D167");
F(548083557441603752,"D80");
F(3752372980267433863,"D125");
F(806161545071376286,"D4");
F(3932843936114889371,"D165");
F(1987808193861192602,"D168");
F(8365783252039529614,"D82");
F(7209228968730063529,"D127");
F(2101807920818891428,"D81");
F(6241217888360533933,"D96");
F(6205761336881335179,"D182");
F(8721819063347450264,"D144");
F(5312037508289206932,"D181");
F(3105585501251065225,"D111");
F(4366921042714638219,"D128");
F(601827942559586695,"D142");
F(2922340953723177625,"D143");
F(2960004660020975543,"D178");
F(7609156073164769589,"D53");
F(222935355048657070,"D5");
F(2970545664927159816,"D6");
F(6204398010265714204,"D145");
F(374712764295942038,"D97");
F(1723220108920171142,"D129");
F(429546985718381962,"D146");
F(650776760741747517,"D54");
F(3220983669738647478,"D141");
F(4590374013472553640,"D137");
F(2654692709366902581,"D98");
F(2904313428820581807,"D114");
F(1907630869065254958,"D7");
F(2987928175766175158,"D88");
F(5935599844821974668,"D113");
F(4660432463466862378,"D138");
F(4420332126672209052,"D117");
F(2022998588251264118,"D70");
F(2196937835646625659,"D43");
F(8653201299615863357,"D84");
F(9099924369772455095,"D151");
F(5500609502868948777,"D73");
F(4782841672273961848,"D94");
F(8739341433814521781,"D68");
F(7353005043501464741,"D34");
F(2683905956162972457,"D59");
F(5649025874480693029,"D160");
F(2078205908459224855,"D39");
F(499436072460497289,"D9");
F(4749706021023022779,"D64");
F(1175163062040739709,"D134");
F(5278889990022374554,"D90");
F(5124893642096736519,"D14");
F(1250947636926102709,"D63");
F(1636869157145902730,"D66");
F(192824992072290351,"D13");
F(4857233156316079622,"D67");
F(4170055510130194822,"D89");
F(6873405098185157038,"D100");
F(8197414619572472191,"D103");
F(4018353704411598011,"D61");
F(5270369793368082362,"D102");
F(7338743769480117173,"D91");
F(616508884100003903,"D139");
F(7748794459133080838,"D86");
F(4092941025152366206,"D40");
F(8446301888090358137,"D22");
F(6261756881964070149,"D107");
F(3287716687401860493,"D31");
F(9210172176634242957,"D62");
F(7315283608180129406,"D93");
F(4731415191493414014,"D136");
F(3642018014021446009,"D69");
F(8062576798302421366,"D49");
F(349955649417138039,"D25");
F(3889170442122306740,"D16");
F(1552999376798508843,"D21");
F(7794728742753555771,"D10");
F(1198042150851972982,"D45");
F(3079420925595948159,"D60");
F(4113551443423533173,"D116");
F(5729201068749827734,"D28");
F(3019747951994871159,"D44");
F(8234639355184571515,"D29");
F(2257150902502389879,"D35");
F(6596720446164050730,"D15");
F(7268350556172330805,"D18");
F(3399180373082062006,"D17");
F(577338146078996862,"D42");
F(6513392762869910282,"D147");
F(5282006273574795124,"D148");
F(2353412878862914495,"D72");
F(345984848655046014,"D46");
F(3999806831403471732,"D92");
F(8364334585842245045,"D83");
F(1999623077561641595,"D26");
F(1996915432568221560,"D30");
F(2649849773993841961,"D75");
F(4896292169814729851,"D47");
F(8511275459268467253,"D8");
F(8799319768542964347,"D65");
F(3878292333093847983,"D24");
F(4656459935912377126,"D74");
F(5030053767268343158,"D33");
F(8622167493552199972,"D101");
F(6388472488569164573,"D71");
F(6293007012871812222,"D41");
F(3173108482421044360,"D12");
F(7024339560032711566,"D99");
F(169769054898040249,"D106");
F(5920557069823907238,"D19");
F(2243731975324008197,"D37");
F(8383184935242501050,"D123");
F(6498488912209721413,"D76");
F(1357559500241794470,"D120");
F(6704802709251043221,"D27");
F(8914696315411377158,"D11");
F(6543822339994965813,"D87");
F(6187465011784201351,"D23");
F(7714796566480683528,"D36");
F(1315448162213702695,"D32");
F(2430629514020720534,"D85");
F(7640679406074340653,"D38");
F(6095433422459137010,"D163");
F(4179642515294214310,"D20");
F(166155294418611967,"D162");
F(8912404645837086245,"D115");
F(5943427692337439118,"D55");
F(7423423201646616310,"D108");
F(504968491975332859,"D133");
F(3601298870564621866,"D112");
F(8525714459383633036,"D130");
F(4806785019956242686,"D170");
F(541282394408382198,"D152");
F(6690120255495357836,"D161");
F(5745755156824745650,"D122");
F(1122525447442951857,"D119");
F(2267327496644663981,"D109");
F(2418008371377299702,"D153");
F(1717870602587035505,"D169");
F(6782259566974864888,"D57");
F(8314877019581198832,"D105");
F(3119953570597180252,"D48");
F(6807048626670298736,"D150");
F(9145219018249213438,"D135");
F(834875071801946097,"D56");
F(1723313340457945841,"D131");
F(1544268484345098928,"D149");
F(4284710231414211249,"D132");
F(1483686635734925054,"D121");
F(5795060796676571707,"D58");
F(8877748864175785212,"D118");
F(4201137891309670137,"D104");/*
//pack={};
F(4150925826796819880,"F");
F(4285765157828038068,"B242");*/

F(2861796350156418590,"G89$F1:SF#6/S2#8/S1#1/R2#1/R1#1/F#1");
F(2605734365107274142,"G101$F2:SF#1/S2#7/S1#3/R2#2/R1#3/F#2");
F(7249951795696926366,"G150$F1:SF#7/S2#14/S1#2/R2#5/R1#1/F#1");
F(1813672159794974094,"G102$F4:SF#8/S2#1/S1#2/R2#1/R1#1/F#4");
F(1018140384778993172,"G118$F4:SF#1/S2#2/S1#2/R2#3/R1#4/F#4");
F(2286731905200838190,"G130$F7:SF#2/S2#5/S1#6/R2#5/R1#6/F#7");
F(4150925826796819880,"G0$F1:SF#1/S2#1/S1#1/R2#1/R1#1/F#1");
F(6104714086929149982,"G161$F1:SF#3/S2#6/S1#4/R2#12/R1#8/F#1");
F(2787479228945548843,"G171$F19:SF#9/S2#9/S1#11/R2#8/R1#9/F#19");
F(2070930203622590254,"G169$F11:SF#5/S2#7/S1#8/R2#9/R1#11/F#11");
F(7826510406288545556,"G132$F12:SF#6/S2#8/S1#7/R2#10/R1#8/F#12");
F(557044317698868541,"G163$F13:SF#6/S2#11/S1#12/R2#8/R1#9/F#13");
F(3466747085076039611,"G1$F2:SF#2/S2#2/S1#2/R2#2/R1#2/F#2");
F(2203777101551907885,"G74$F4:SF#4/S2#3/S1#3/R2#2/R1#3/F#4");
F(8396965551922838186,"G170$F14:SF#14/S2#22/S1#18/R2#18/R1#18/F#14");
F(796636986096623791,"G139$F5:SF#16/S2#14/S1#9/R2#15/R1#13/F#5");
F(5660303083930456586,"G131$F8:SF#9/S2#14/S1#9/R2#11/R1#11/F#8");
F(2847693426163979656,"G140$F16:SF#11/S2#10/S1#14/R2#12/R1#14/F#16");
F(2634902118250132507,"G141$F17:SF#9/S2#20/S1#15/R2#9/R1#9/F#17");
F(482213862970953868,"G73$F3:SF#2/S2#2/S1#4/R2#3/R1#4/F#3");
F(6632120055918190383,"G90$F3:SF#1/S2#1/S1#4/R2#6/R1#5/F#3");
F(3197849103421018008,"G119$F15:SF#37/S2#8/S1#10/R2#9/R1#10/F#15");
F(6888610344207197116,"G103$F13:SF#7/S2#85/S1#9/R2#8/R1#9/F#13");
F(4944404388773368348,"G153$F25:SF#17/S2#16/S1#17/R2#18/R1#19/F#25");
F(570614572350008383,"G162$F12:SF#17/S2#15/S1#20/R2#21/R1#23/F#12");
F(8583026972194319135,"G2$F3:SF#3/S2#3/S1#3/R2#3/R1#3/F#3");
F(6240436085087429528,"G51$F7:SF#4/S2#3/S1#4/R2#4/R1#4/F#7");
F(6993926502742566814,"G172$F34:SF#30/S2#50/S1#29/R2#30/R1#28/F#34");
F(1925300485248653495,"G105$F18:SF#50/S2#17/S1#12/R2#11/R1#11/F#18");
F(3570792346025678089,"G50$F6:SF#3/S2#4/S1#5/R2#5/R1#5/F#6");
F(7246092965776070415,"G173$F37:SF#28/S2#20/S1#36/R2#32/R1#32/F#37");
F(8654792778248582541,"G154$F30:SF#191/S2#21/S1#27/R2#24/R1#24/F#30");
F(5949054706520693406,"G166$F32:SF#18/S2#19/S1#25/R2#23/R1#25/F#32");
F(5060690894857717660,"G5$F6:SF#23/S2#10/S1#4/R2#4/R1#5/F#6");
F(1529324600617028122,"G142$F25:SF#18/S2#26/S1#23/R2#22/R1#23/F#25");
F(3471209727536821276,"G91$F7:SF#9/S2#7/S1#10/R2#25/R1#11/F#7");
F(309392007424195605,"G3$F4:SF#18/S2#16/S1#5/R2#8/R1#6/F#4");
F(3934482049845888138,"G106$F20:SF#132/S2#25/S1#22/R2#16/R1#16/F#20");
F(5586775854641593887,"G4$F5:SF#50/S2#24/S1#13/R2#12/R1#4/F#5");
F(1732260267864359565,"G143$F28:SF#225/S2#125/S1#31/R2#26/R1#26/F#28");
F(3142412093820075406,"G155$F36:SF#57/S2#126/S1#35/R2#29/R1#32/F#36");
F(2609020868249729163,"G164$F22:SF#30/S2#83/S1#33/R2#31/R1#38/F#22");
F(3986843314393919130,"G120$F23:SF#36/S2#63/S1#47/R2#19/R1#20/F#23");
F(2602545832588497599,"G92$F10:SF#18/S2#19/S1#18/R2#136/R1#14/F#10");
F(989148960014731301,"G151$F19:SF#21/S2#219/S1#36/R2#68/R1#35/F#19");
F(3718607181200296893,"G165$F25:SF#219/S2#95/S1#42/R2#45/R1#41/F#25");
F(597102121543415482,"G75$F13:SF#79/S2#158/S1#105/R2#10/R1#13/F#13");
F(8038120647872490271,"G104$F14:SF#81/S2#155/S1#47/R2#42/R1#22/F#14");
F(3781702297415810091,"G52$F10:SF#205/S2#171/S1#70/R2#8/R1#12/F#10");
F(2686743766415180732,"G159$F44:SF#171/S2#109/S1#162/R2#37/R1#39/F#44");
F(3887255809790788030,"G158$F42:SF#195/S2#156/S1#175/R2#38/R1#38/F#42");
F(495724511796477887,"G152$F21:SF#33/S2#122/S1#37/R2#84/R1#41/F#21");
F(650553105220962885,"G6$F7:SF#211/S2#202/S1#221/R2#5/R1#238/F#7");
F(5858187092062257852,"G8$F9:SF#235/S2#198/S1#219/R2#7/R1#217/F#9");
F(2124062530136116085,"G93$F22:SF#21/S2#13/S1#12/R2#14/R1#16/F#22");
F(6685608602581222569,"G138$F45:SF#125/S2#200/S1#274/R2#38/R1#38/F#45");
F(918536790161037503,"G77$F15:SF#223/S2#204/S1#211/R2#13/R1#68/F#15");
F(484428640413685812,"G129$F43:SF#336/S2#157/S1#291/R2#29/R1#88/F#43");
F(5529298145432243748,"G95$F24:SF#176/S2#189/S1#270/R2#16/R1#33/F#24");
F(2390044018038347324,"G94$F23:SF#246/S2#134/S1#221/R2#15/R1#37/F#23");
F(453777966792055992,"G9$F10:SF#217/S2#209/S1#211/R2#6/R1#210/F#10");
F(6231925628522084537,"G111$F36:SF#220/S2#217/S1#280/R2#27/R1#88/F#36");
F(3870909060805172006,"G96$F25:SF#318/S2#224/S1#248/R2#18/R1#76/F#25");
F(290018899905883837,"G53$F14:SF#204/S2#269/S1#251/R2#10/R1#99/F#14");
F(1193522231534970296,"G108$F25:SF#263/S2#266/S1#283/R2#48/R1#112/F#25");
F(4081703032275419029,"G7$F8:SF#216/S2#229/S1#216/R2#11/R1#223/F#8");
F(8668026331984512682,"G11$F12:SF#218/S2#232/S1#224/R2#9/R1#220/F#12");
F(5929939619268858922,"G58$F21:SF#258/S2#282/S1#273/R2#17/R1#202/F#21");
F(5328923086521511071,"G134$F31:SF#31/S2#262/S1#101/R2#82/R1#50/F#31");
F(5115328675913995707,"G55$F18:SF#228/S2#224/S1#255/R2#9/R1#127/F#18");
F(3354646763651671221,"G128$F38:SF#331/S2#235/S1#299/R2#37/R1#105/F#38");
F(886113018418521000,"G54$F16:SF#181/S2#258/S1#266/R2#18/R1#107/F#16");
F(6081646561097099956,"G98$F35:SF#207/S2#243/S1#292/R2#26/R1#109/F#35");
F(8464562479525415976,"G76$F14:SF#191/S2#176/S1#261/R2#31/R1#86/F#14");
F(8693097761308364989,"G23$F24:SF#48/S2#155/S1#38/R2#16/R1#30/F#24");
F(1320748960121650948,"G84$F38:SF#46/S2#12/S1#19/R2#40/R1#14/F#38");
F(5391129229165083931,"G85$F40:SF#97/S2#116/S1#56/R2#43/R1#47/F#40");
F(1338481265175038373,"G99$F42:SF#17/S2#18/S1#72/R2#41/R1#31/F#42");
F(4862901614871738246,"G30$F31:SF#19/S2#14/S1#29/R2#27/R1#21/F#31");
F(703812623190687030,"G34$F35:SF#79/S2#101/S1#7/R2#17/R1#36/F#35");
F(1103108905084072983,"G80$F27:SF#273/S2#232/S1#278/R2#17/R1#95/F#27");
F(5684728687527165109,"G29$F30:SF#65/S2#45/S1#27/R2#14/R1#7/F#30");
F(4323550715809704362,"G39$F40:SF#73/S2#85/S1#28/R2#40/R1#24/F#40");
F(4310033170461057820,"G31$F32:SF#56/S2#150/S1#11/R2#23/R1#40/F#32");
F(33024120862308532,"G122$F30:SF#25/S2#137/S1#75/R2#88/R1#101/F#30");
F(7835727181263953853,"G56$F19:SF#320/S2#239/S1#274/R2#13/R1#247/F#19");
F(1020366450692159767,"G65$F34:SF#45/S2#56/S1#25/R2#30/R1#9/F#34");
F(7267751498714187036,"G66$F35:SF#80/S2#72/S1#44/R2#24/R1#21/F#35");
F(8273960531376023924,"G137$F40:SF#24/S2#76/S1#72/R2#164/R1#89/F#40");
F(8589592481001658779,"G86$F42:SF#120/S2#170/S1#12/R2#42/R1#57/F#42");
F(4855226478389176458,"G24$F25:SF#6/S2#68/S1#44/R2#20/R1#15/F#25");
F(1600215781639524222,"G37$F38:SF#15/S2#60/S1#18/R2#32/R1#13/F#38");
F(9018490819620271380,"G27$F28:SF#63/S2#43/S1#36/R2#22/R1#44/F#28");
F(7600106054373094814,"G168$F49:SF#33/S2#135/S1#75/R2#139/R1#71/F#49");
F(4484470595325941918,"G12$F13:SF#239/S2#223/S1#227/R2#10/R1#219/F#13");
F(6925824606918768020,"G68$F43:SF#50/S2#42/S1#86/R2#46/R1#46/F#43");
F(3366741777748422011,"G136$F39:SF#32/S2#244/S1#51/R2#138/R1#142/F#39");
F(2959264735930504986,"G32$F33:SF#16/S2#73/S1#14/R2#19/R1#16/F#33");
F(2934964163112367524,"G72$F50:SF#16/S2#9/S1#37/R2#45/R1#36/F#50");
F(671048122208900761,"G14$F15:SF#28/S2#44/S1#10/R2#47/R1#12/F#15");
F(678466377156989758,"G35$F36:SF#17/S2#89/S1#6/R2#21/R1#23/F#36");
F(4404028796790441341,"G42$F43:SF#102/S2#98/S1#32/R2#29/R1#37/F#43");
F(8494074179354315791,"G33$F34:SF#40/S2#33/S1#15/R2#24/R1#43/F#34");
F(5333494714051809691,"G25$F26:SF#31/S2#52/S1#9/R2#18/R1#28/F#26");
F(6885846387409900847,"G135$F38:SF#26/S2#109/S1#106/R2#160/R1#119/F#38");
F(2161543309792542229,"G97$F30:SF#13/S2#54/S1#63/R2#133/R1#48/F#30");
F(4960210199340022052,"G26$F27:SF#9/S2#42/S1#20/R2#26/R1#14/F#27");
F(740970962752718709,"G48$F49:SF#5/S2#82/S1#21/R2#37/R1#35/F#49");
F(8989027442366417213,"G71$F49:SF#84/S2#33/S1#20/R2#43/R1#29/F#49");
F(1213184264426707902,"G17$F18:SF#228/S2#226/S1#226/R2#34/R1#225/F#18");
F(9211644062496736522,"G121$F29:SF#24/S2#113/S1#53/R2#68/R1#80/F#29");
F(8452196198622121854,"G47$F48:SF#58/S2#31/S1#35/R2#41/R1#34/F#48");
F(6546359702978395174,"G174$F50:SF#42/S2#264/S1#93/R2#159/R1#80/F#50");
F(6841054859021658485,"G64$F33:SF#40/S2#66/S1#46/R2#32/R1#22/F#33");
F(2917893566859520921,"G133$F24:SF#27/S2#142/S1#136/R2#215/R1#112/F#24");
F(9207691117123750772,"G62$F30:SF#7/S2#8/S1#36/R2#93/R1#17/F#30");
F(4723469321509680511,"G38$F39:SF#71/S2#90/S1#59/R2#25/R1#27/F#39");
F(8887428201097007741,"G67$F38:SF#90/S2#58/S1#30/R2#48/R1#19/F#38");
F(1405728465287324853,"G20$F21:SF#212/S2#222/S1#235/R2#50/R1#233/F#21");
F(1098163959100761791,"G78$F23:SF#249/S2#270/S1#286/R2#28/R1#139/F#23");
F(2553949789431559037,"G49$F50:SF#33/S2#23/S1#16/R2#44/R1#8/F#50");
F(4942225563605743670,"G81$F29:SF#275/S2#299/S1#276/R2#24/R1#154/F#29");
F(1820894104445933695,"G43$F44:SF#41/S2#100/S1#8/R2#31/R1#31/F#44");
F(2333152679703637419,"G44$F45:SF#98/S2#134/S1#25/R2#42/R1#20/F#45");
F(8060446222941059477,"G21$F22:SF#81/S2#19/S1#57/R2#15/R1#11/F#22");
F(5486364037263792810,"G57$F20:SF#14/S2#87/S1#83/R2#115/R1#39/F#20");
F(3353313224674587957,"G59$F25:SF#304/S2#266/S1#276/R2#23/R1#256/F#25");
F(1627267719231077551,"G117$F50:SF#324/S2#325/S1#311/R2#50/R1#205/F#50");
F(3879328751351986598,"G28$F29:SF#70/S2#66/S1#24/R2#33/R1#9/F#29");
F(5068038733235436916,"G87$F43:SF#18/S2#32/S1#15/R2#54/R1#39/F#43");
F(5131953867512497530,"G45$F46:SF#29/S2#91/S1#49/R2#30/R1#26/F#46");
F(8437505907794709175,"G79$F24:SF#251/S2#311/S1#289/R2#27/R1#253/F#24");
F(3473774008200670005,"G19$F20:SF#226/S2#217/S1#232/R2#43/R1#227/F#20");
F(7294496620431285622,"G40$F41:SF#34/S2#34/S1#12/R2#39/R1#18/F#41");
F(3329873508734206071,"G46$F47:SF#76/S2#15/S1#34/R2#35/R1#32/F#47");
F(6075143701484739253,"G60$F27:SF#257/S2#303/S1#279/R2#21/R1#274/F#27");
F(2122422017600796536,"G127$F37:SF#20/S2#118/S1#29/R2#114/R1#76/F#37");
F(4468266434942864756,"G41$F42:SF#42/S2#22/S1#50/R2#36/R1#19/F#42");
F(8748337213204167102,"G13$F14:SF#221/S2#251/S1#241/R2#28/R1#241/F#14");
F(2989041023601363636,"G10$F11:SF#231/S2#244/S1#231/R2#38/R1#229/F#11");
F(866121219884743447,"G107$F21:SF#24/S2#144/S1#37/R2#158/R1#101/F#21");
F(306554361557248117,"G149$F44:SF#17/S2#107/S1#65/R2#135/R1#123/F#44");
F(4165065710198134459,"G83$F32:SF#282/S2#259/S1#292/R2#32/R1#287/F#32");
F(979906908977237111,"G145$F32:SF#28/S2#248/S1#133/R2#184/R1#111/F#32");
F(7442783389286233262,"G61$F28:SF#329/S2#304/S1#300/R2#29/R1#280/F#28");
F(7721496935226688551,"G16$F17:SF#234/S2#422/S1#257/R2#48/R1#249/F#17");
F(1291176215854546550,"G36$F37:SF#13/S2#13/S1#60/R2#54/R1#22/F#37");
F(3960398364568853512,"G15$F16:SF#245/S2#220/S1#236/R2#13/R1#275/F#16");
F(3463379813932495991,"G88$F50:SF#16/S2#198/S1#42/R2#110/R1#27/F#50");
F(3390745379459518375,"G63$F32:SF#280/S2#318/S1#303/R2#37/R1#283/F#32");
F(3251087903717613899,"G18$F19:SF#279/S2#395/S1#286/R2#46/R1#278/F#19");
F(4264162335739218733,"G22$F23:SF#321/S2#275/S1#252/R2#45/R1#250/F#23");
F(2881275273303326331,"G82$F31:SF#19/S2#79/S1#77/R2#162/R1#101/F#31");
F(7579937010582123291,"G156$F37:SF#34/S2#175/S1#211/R2#220/R1#203/F#37");
F(3463627049410234250,"G109$F29:SF#23/S2#148/S1#237/R2#198/R1#190/F#29");
F(3595645621052512379,"G167$F48:SF#31/S2#254/S1#159/R2#249/R1#211/F#48");
F(4556065406988068377,"G144$F31:SF#27/S2#277/S1#169/R2#231/R1#267/F#31");
F(407136125554947952,"G100$F48:SF#16/S2#264/S1#178/R2#240/R1#184/F#48");
F(6008671673854752759,"G113$F39:SF#16/S2#121/S1#164/R2#242/R1#285/F#39");
F(2940896466181451145,"G70$F46:SF#12/S2#230/S1#105/R2#181/R1#119/F#46");
F(778294301455108494,"G110$F33:SF#21/S2#107/S1#195/R2#195/R1#185/F#33");
F(3490363817441255322,"G69$F45:SF#8/S2#111/S1#176/R2#195/R1#192/F#45");
F(6609320355315523575,"G146$F36:SF#30/S2#148/S1#328/R2#324/R1#370/F#36");
F(8912762632453963007,"G125$F34:SF#21/S2#189/S1#145/R2#274/R1#340/F#34");
F(3672743462141956536,"G157$F40:SF#31/S2#241/S1#242/R2#-1/R1#354/F#40");
F(114574194329612713,"G115$F44:SF#20/S2#171/S1#246/R2#343/R1#160/F#44");
F(8216371041307461677,"G160$F47:SF#32/S2#149/S1#265/R2#277/R1#307/F#47");
F(3511973613950876662,"G123$F32:SF#19/S2#126/S1#278/R2#325/R1#293/F#32");
F(4098669987037067058,"G114$F40:SF#11/S2#163/S1#158/R2#294/R1#123/F#40");
F(4384276373779138992,"G148$F38:SF#23/S2#57/S1#173/R2#320/R1#260/F#38");
F(4317879354600001648,"G112$F38:SF#22/S2#251/S1#224/R2#330/R1#225/F#38");
F(8533235360389866490,"G147$F37:SF#29/S2#151/S1#324/R2#347/R1#387/F#37");
F(8594937048881303289,"G126$F35:SF#17/S2#313/S1#199/R2#277/R1#243/F#35");
F(8381524672502198309,"G124$F33:SF#9/S2#285/S1#183/R2#262/R1#287/F#33");
F(8876314962917486887,"G116$F46:SF#18/S2#131/S1#213/R2#266/R1#238/F#46");

F(1055840238156136244,"H50$F1:SF#1/S2#1/S1#1/R2#1/R1#1/F#1");
F(1874684940068064411,"H77$F3:SF#3/S2#2/S1#2/R2#2/R1#2/F#3");
F(7547837848081025977,"H0$F1:SF#1/S2#1/S1#1/R2#2/R1#1/F#1");
F(2128297223019833252,"H97$F10:SF#12/S2#4/S1#6/R2#4/R1#4/F#10");
F(804502883051908908,"H52$F5:SF#4/S2#3/S1#4/R2#2/R1#3/F#5");
F(3173101313325403929,"H1$F2:SF#2/S2#2/S1#2/R2#1/R1#2/F#2");
F(4228431011826834959,"H107$F7:SF#14/S2#9/S1#8/R2#7/R1#7/F#7");
F(4232569509848511903,"H51$F4:SF#5/S2#5/S1#5/R2#5/R1#5/F#4");
F(1053105276570068283,"H78$F5:SF#4/S2#5/S1#8/R2#8/R1#6/F#5");
F(8397373930971407638,"H98$F11:SF#8/S2#6/S1#11/R2#8/R1#8/F#11");
F(4726570720800821692,"H79$F10:SF#6/S2#3/S1#7/R2#5/R1#8/F#10");
F(758316917917300886,"H108$F15:SF#11/S2#6/S1#12/R2#9/R1#13/F#15");
F(8865636222597161737,"H96$F7:SF#5/S2#14/S1#9/R2#11/R1#11/F#7");
F(2308768918030320303,"H2$F3:SF#3/S2#4/S1#3/R2#4/R1#3/F#3");
F(2345049117251214607,"H3$F4:SF#4/S2#18/S1#4/R2#3/R1#4/F#4");
F(5744961578588373564,"H53$F7:SF#9/S2#8/S1#7/R2#10/R1#7/F#7");
F(6321462386026562063,"H109$F17:SF#17/S2#17/S1#16/R2#15/R1#18/F#17");
F(3794923186052361502,"H80$F12:SF#12/S2#11/S1#12/R2#10/R1#12/F#12");
F(4047450871072245902,"H110$F19:SF#19/S2#20/S1#17/R2#16/R1#19/F#19");
F(297529900167296669,"H99$F16:SF#17/S2#235/S1#16/R2#15/R1#16/F#16");
F(4196603552303883049,"H4$F5:SF#133/S2#198/S1#206/R2#5/R1#202/F#5");
F(8759030677695379846,"H67$F36:SF#86/S2#30/S1#65/R2#29/R1#20/F#36");
F(123661279624429855,"H87$F29:SF#77/S2#14/S1#21/R2#37/R1#18/F#29");
F(3257053517974412662,"H43$F44:SF#179/S2#70/S1#24/R2#30/R1#30/F#44");
F(8319646622140479405,"H115$F29:SF#26/S2#74/S1#79/R2#146/R1#30/F#29");
F(6895658261215663806,"H81$F15:SF#249/S2#290/S1#264/R2#18/R1#80/F#15");
F(4384912223974660647,"H74$F46:SF#87/S2#20/S1#59/R2#39/R1#23/F#46");
F(9101547593586063991,"H24$F25:SF#18/S2#52/S1#6/R2#46/R1#9/F#25");
F(5028115293849137179,"H27$F28:SF#62/S2#45/S1#37/R2#22/R1#42/F#28");
F(5910267345678859317,"H95$F47:SF#13/S2#48/S1#39/R2#113/R1#27/F#47");
F(6201575808228279159,"H46$F47:SF#44/S2#92/S1#33/R2#43/R1#19/F#47");
F(5663663405286567055,"H73$F43:SF#103/S2#26/S1#19/R2#23/R1#50/F#43");
F(2693453740718171035,"H71$F41:SF#48/S2#21/S1#51/R2#27/R1#18/F#41");
F(1751454447868641310,"H64$F30:SF#13/S2#55/S1#21/R2#38/R1#51/F#30");
F(7054413909368704655,"H92$F40:SF#169/S2#23/S1#34/R2#40/R1#57/F#40");
F(3552055447374493887,"H118$F39:SF#282/S2#123/S1#272/R2#22/R1#44/F#39");
F(5678205538609487485,"H101$F25:SF#20/S2#62/S1#36/R2#82/R1#34/F#25");
F(3928624385382645655,"H91$F34:SF#39/S2#252/S1#43/R2#46/R1#19/F#34");
F(3899030489288960653,"H23$F24:SF#11/S2#25/S1#12/R2#16/R1#17/F#24");
F(3454389863789197237,"H28$F29:SF#15/S2#32/S1#27/R2#24/R1#11/F#29");
F(3045959330076509960,"H31$F32:SF#87/S2#47/S1#23/R2#11/R1#15/F#32");
F(2473064534590119549,"H39$F40:SF#28/S2#48/S1#16/R2#35/R1#44/F#40");
F(674252893261415028,"H48$F49:SF#49/S2#20/S1#32/R2#39/R1#66/F#49");
F(2448357323234571543,"H75$F47:SF#26/S2#99/S1#66/R2#46/R1#66/F#47");
F(192650110637057834,"H44$F45:SF#91/S2#16/S1#13/R2#28/R1#28/F#45");
F(174053683085199231,"H25$F26:SF#9/S2#169/S1#25/R2#15/R1#25/F#26");
F(803728766711250548,"H22$F23:SF#25/S2#9/S1#29/R2#17/R1#41/F#23");
F(6880102155811429423,"H13$F14:SF#12/S2#50/S1#9/R2#21/R1#12/F#14");
F(1103504475513651102,"H69$F39:SF#96/S2#60/S1#53/R2#24/R1#17/F#39");
F(2870280662352674685,"H72$F42:SF#33/S2#29/S1#9/R2#34/R1#31/F#42");
F(7339489924792451966,"H36$F37:SF#23/S2#34/S1#30/R2#18/R1#43/F#37");
F(4557128488942840758,"H117$F36:SF#186/S2#266/S1#276/R2#26/R1#104/F#36");
F(7867846553950167674,"H33$F34:SF#80/S2#6/S1#18/R2#36/R1#16/F#34");
F(2113400791014638121,"H30$F31:SF#16/S2#61/S1#17/R2#37/R1#5/F#31");
F(8024620273672930489,"H105$F35:SF#216/S2#118/S1#266/R2#18/R1#56/F#35");
F(4454922840959235898,"H32$F33:SF#36/S2#13/S1#8/R2#14/R1#10/F#33");
F(8555817189914319734,"H40$F41:SF#83/S2#19/S1#21/R2#34/R1#24/F#41");
F(6409189486097431300,"H34$F35:SF#8/S2#37/S1#19/R2#25/R1#14/F#35");
F(520749899081805434,"H68$F37:SF#60/S2#11/S1#69/R2#35/R1#46/F#37");
F(6590860940844483496,"H111$F22:SF#22/S2#26/S1#80/R2#94/R1#26/F#22");
F(8164024191755498933,"H6$F7:SF#178/S2#211/S1#214/R2#6/R1#212/F#7");
F(5732581903939490719,"H20$F21:SF#81/S2#55/S1#22/R2#12/R1#31/F#21");
F(5293252145254700426,"H76$F49:SF#17/S2#191/S1#64/R2#49/R1#56/F#49");
F(1188730902150741672,"H21$F22:SF#75/S2#35/S1#15/R2#19/R1#8/F#22");
F(7448481077005088636,"H37$F38:SF#24/S2#79/S1#7/R2#32/R1#20/F#38");
F(8246717983321497981,"H16$F17:SF#42/S2#23/S1#34/R2#49/R1#50/F#17");
F(4430513287007198761,"H5$F6:SF#222/S2#217/S1#213/R2#7/R1#217/F#6");
F(9078735466835484057,"H29$F30:SF#32/S2#62/S1#44/R2#20/R1#45/F#30");
F(8437342265074391354,"H54$F10:SF#325/S2#268/S1#265/R2#43/R1#197/F#10");
F(7336457755204327565,"H70$F40:SF#131/S2#128/S1#70/R2#25/R1#30/F#40");
F(883686911089986168,"H26$F27:SF#10/S2#57/S1#14/R2#26/R1#47/F#27");
F(2202282664527407741,"H49$F50:SF#19/S2#119/S1#20/R2#50/R1#37/F#50");
F(348548787179636413,"H56$F14:SF#128/S2#167/S1#270/R2#11/R1#153/F#14");
F(6457703998022559103,"H45$F46:SF#79/S2#90/S1#39/R2#40/R1#34/F#46");
F(511253275049601701,"H55$F13:SF#281/S2#185/S1#273/R2#15/R1#118/F#13");
F(8536939575694987175,"H82$F17:SF#331/S2#229/S1#281/R2#32/R1#92/F#17");
F(2947145626326100857,"H47$F48:SF#84/S2#7/S1#38/R2#41/R1#13/F#48");
F(3393466121649417534,"H38$F39:SF#65/S2#29/S1#36/R2#31/R1#22/F#39");
F(2113894298706001789,"H61$F22:SF#82/S2#107/S1#36/R2#28/R1#57/F#22");
F(3790968985735988854,"H42$F43:SF#77/S2#176/S1#31/R2#33/R1#7/F#43");
F(7333350196932792845,"H85$F23:SF#326/S2#212/S1#280/R2#27/R1#95/F#23");
F(9040490543129711477,"H41$F42:SF#102/S2#96/S1#5/R2#23/R1#6/F#42");
F(6190578836130396333,"H8$F9:SF#221/S2#213/S1#216/R2#9/R1#224/F#9");
F(3803858845020026662,"H35$F36:SF#64/S2#39/S1#54/R2#42/R1#29/F#36");
F(6057935816467678245,"H93$F41:SF#273/S2#261/S1#279/R2#30/R1#104/F#41");
F(1459252459014466741,"H7$F8:SF#198/S2#214/S1#212/R2#8/R1#218/F#8");
F(3628865252392661160,"H119$F42:SF#207/S2#212/S1#302/R2#25/R1#98/F#42");
F(5452527855645120030,"H86$F24:SF#297/S2#198/S1#286/R2#25/R1#88/F#24");
F(3782278443420169884,"H120$F49:SF#304/S2#259/S1#317/R2#41/R1#131/F#49");
F(7295046547807034405,"H9$F10:SF#227/S2#224/S1#231/R2#10/R1#231/F#10");
F(2705915885656697387,"H57$F17:SF#313/S2#239/S1#276/R2#17/R1#244/F#17");
F(2037715725878768654,"H88$F30:SF#265/S2#255/S1#302/R2#22/R1#99/F#30");
F(484834567393656621,"H10$F11:SF#229/S2#256/S1#228/R2#38/R1#234/F#11");
F(4160548062032187583,"H94$F42:SF#285/S2#249/S1#287/R2#28/R1#98/F#42");
F(1998863865125553716,"H58$F19:SF#270/S2#256/S1#281/R2#19/R1#230/F#19");
F(7273109465161666959,"H116$F30:SF#16/S2#137/S1#65/R2#173/R1#142/F#30");
F(8294280482205099706,"H59$F20:SF#317/S2#269/S1#278/R2#18/R1#265/F#20");
F(1854940412780626605,"H90$F32:SF#344/S2#234/S1#297/R2#34/R1#122/F#32");
F(2927671566975511574,"H12$F13:SF#228/S2#303/S1#270/R2#13/R1#225/F#13");
F(1564241714450748470,"H106$F50:SF#285/S2#275/S1#313/R2#36/R1#149/F#50");
F(6489433029625586747,"H15$F16:SF#246/S2#265/S1#223/R2#29/R1#233/F#16");
F(8359909780521238715,"H60$F21:SF#265/S2#253/S1#292/R2#26/R1#281/F#21");
F(385930193147537971,"H112$F23:SF#27/S2#307/S1#147/R2#269/R1#223/F#23");
F(3130690930168187255,"H89$F31:SF#14/S2#108/S1#144/R2#164/R1#134/F#31");
F(1570997822677058089,"H63$F28:SF#301/S2#337/S1#291/R2#45/R1#289/F#28");
F(1637689907491850276,"H14$F15:SF#258/S2#240/S1#237/R2#45/R1#227/F#15");
F(1975737248889057046,"H66$F34:SF#276/S2#284/S1#283/R2#31/R1#293/F#34");
F(9127238747849389595,"H11$F12:SF#216/S2#223/S1#246/R2#27/R1#243/F#12");
F(8755129725345555454,"H114$F25:SF#25/S2#172/S1#248/R2#248/R1#332/F#25");
F(7516913352396395919,"H83$F19:SF#21/S2#226/S1#176/R2#188/R1#154/F#19");
F(4367687539542009006,"H62$F24:SF#261/S2#293/S1#285/R2#33/R1#290/F#24");
F(3033184967728183702,"H17$F18:SF#275/S2#273/S1#265/R2#47/R1#232/F#18");
F(4480335529198631173,"H65$F33:SF#309/S2#292/S1#308/R2#30/R1#291/F#33");
F(1468258875598835111,"H18$F19:SF#284/S2#284/S1#261/R2#48/R1#255/F#19");
F(2907733872218876692,"H19$F20:SF#270/S2#292/S1#239/R2#44/R1#238/F#20");
F(8117519767731466291,"H113$F24:SF#29/S2#167/S1#99/R2#301/R1#325/F#24");
F(8436765876878190218,"H84$F20:SF#15/S2#93/S1#240/R2#167/R1#166/F#20");
F(5706965155422864368,"H102$F27:SF#18/S2#287/S1#210/R2#327/R1#237/F#27");
F(8460995310135956728,"H100$F24:SF#21/S2#126/S1#234/R2#300/R1#276/F#24");
F(2172802421133947703,"H104$F30:SF#23/S2#49/S1#127/R2#332/R1#240/F#30");
F(3472618730511297456,"H103$F28:SF#19/S2#178/S1#205/R2#295/R1#227/F#28");

F(6128668115248818952,"I0$F1:SF#1/S2#6/S1#1/R2#1/R1#1/F#1");
F(439420496983639949,"I50$F2:SF#3/S2#3/S1#3/R2#1/R1#3/F#2");
F(7245168711413802045,"I51$F3:SF#2/S2#2/S1#2/R2#2/R1#2/F#3");
F(7933792344357477526,"I52$F4:SF#32/S2#17/S1#11/R2#4/R1#5/F#4");
F(1531251297711823236,"I53$F5:SF#102/S2#31/S1#4/R2#3/R1#4/F#5");
F(1250039296583618943,"I12$F13:SF#22/S2#20/S1#54/R2#42/R1#11/F#13");
F(1242512256101398644,"I22$F23:SF#31/S2#46/S1#13/R2#23/R1#2/F#23");
F(5777029612638244980,"I29$F30:SF#114/S2#77/S1#23/R2#18/R1#16/F#30");
F(6402204035707764854,"I45$F46:SF#108/S2#53/S1#17/R2#38/R1#50/F#46");
F(6511782673420989560,"I47$F48:SF#32/S2#59/S1#34/R2#27/R1#36/F#48");
F(7613636793265911708,"I71$F37:SF#86/S2#53/S1#13/R2#37/R1#37/F#37");
F(4489658443094165935,"I32$F33:SF#13/S2#7/S1#12/R2#12/R1#25/F#33");
F(9191058314999710363,"I74$F41:SF#15/S2#28/S1#18/R2#24/R1#19/F#41");
F(1046878831315876007,"I36$F37:SF#73/S2#119/S1#16/R2#22/R1#27/F#37");
F(8847334698443148070,"I38$F39:SF#58/S2#50/S1#28/R2#15/R1#8/F#39");
F(6808982852625265052,"I68$F32:SF#96/S2#66/S1#21/R2#15/R1#18/F#32");
F(3142710400879126392,"I41$F42:SF#56/S2#49/S1#6/R2#26/R1#23/F#42");
F(740582711428466200,"I16$F17:SF#35/S2#29/S1#33/R2#9/R1#22/F#17");
F(4995105704050981545,"I73$F40:SF#42/S2#20/S1#32/R2#38/R1#54/F#40");
F(9153569367176530607,"I25$F26:SF#107/S2#101/S1#8/R2#30/R1#5/F#26");
F(7032224441685276299,"I20$F21:SF#16/S2#69/S1#2/R2#5/R1#24/F#21");
F(2268152645159908980,"I72$F38:SF#74/S2#122/S1#46/R2#47/R1#33/F#38");
F(6346722883485660567,"I21$F22:SF#116/S2#22/S1#26/R2#20/R1#29/F#22");
F(8289166205765973639,"I23$F24:SF#45/S2#109/S1#5/R2#6/R1#20/F#24");
F(3437733648945280892,"I40$F41:SF#11/S2#45/S1#24/R2#33/R1#39/F#41");
F(5069465760854246783,"I11$F12:SF#37/S2#39/S1#15/R2#39/R1#13/F#12");
F(1400074843782592634,"I76$F47:SF#11/S2#128/S1#31/R2#49/R1#29/F#47");
F(598199502972074619,"I13$F14:SF#36/S2#54/S1#37/R2#40/R1#28/F#14");
F(6106318535761794941,"I33$F34:SF#42/S2#28/S1#4/R2#37/R1#21/F#34");
F(4592849422545091748,"I37$F38:SF#18/S2#66/S1#25/R2#32/R1#12/F#38");
F(305821536338867637,"I19$F20:SF#142/S2#4/S1#7/R2#35/R1#15/F#20");
F(2096275883118324136,"I35$F36:SF#26/S2#1/S1#11/R2#29/R1#26/F#36");
F(3558689596822668409,"I30$F31:SF#60/S2#56/S1#9/R2#36/R1#6/F#31");
F(6685062287260283941,"I48$F49:SF#40/S2#55/S1#3/R2#34/R1#17/F#49");
F(4739509601452783509,"I67$F31:SF#10/S2#131/S1#20/R2#29/R1#14/F#31");
F(883028403203869447,"I77$F50:SF#108/S2#27/S1#60/R2#28/R1#56/F#50");
F(8446862669126377599,"I42$F43:SF#33/S2#96/S1#20/R2#19/R1#7/F#43");
F(2057950962843528106,"I18$F19:SF#2/S2#61/S1#32/R2#10/R1#19/F#19");
F(2503784635662083705,"I62$F24:SF#49/S2#75/S1#56/R2#41/R1#22/F#24");
F(8773020570409973785,"I10$F11:SF#28/S2#8/S1#14/R2#8/R1#14/F#11");
F(4020118415354983716,"I43$F44:SF#20/S2#75/S1#31/R2#45/R1#46/F#44");
F(1945666774099718171,"I44$F45:SF#77/S2#79/S1#66/R2#14/R1#18/F#45");
F(7685704459781866239,"I69$F33:SF#5/S2#200/S1#205/R2#284/R1#156/F#33");
F(5447675345402400628,"I49$F50:SF#51/S2#62/S1#21/R2#24/R1#34/F#50");
F(6677530326874873210,"I57$F18:SF#4/S2#50/S1#40/R2#101/R1#66/F#18");
F(3877404073524290941,"I39$F40:SF#59/S2#134/S1#10/R2#13/R1#3/F#40");
F(5372040949963359607,"I46$F47:SF#126/S2#104/S1#44/R2#44/R1#73/F#47");
F(5093991137876316683,"I58$F19:SF#7/S2#229/S1#203/R2#212/R1#174/F#19");
F(3974004248333002359,"I28$F29:SF#24/S2#31/S1#41/R2#31/R1#10/F#29");
F(613637055009016440,"I31$F32:SF#9/S2#3/S1#27/R2#16/R1#4/F#32");
F(1248998254266902399,"I34$F35:SF#140/S2#102/S1#123/R2#48/R1#58/F#35");
F(6718861703579175730,"I75$F42:SF#6/S2#234/S1#223/R2#332/R1#231/F#42");
F(4049641368015345534,"I24$F25:SF#6/S2#161/S1#94/R2#152/R1#69/F#25");
F(3629170306362996652,"I1$F2:SF#209/S2#168/S1#216/R2#4/R1#218/F#2");
F(7611188529124347701,"I2$F3:SF#217/S2#225/S1#215/R2#3/R1#219/F#3");
F(4901136697975533939,"I70$F34:SF#17/S2#59/S1#134/R2#299/R1#238/F#34");
F(4129058409192043652,"I54$F8:SF#194/S2#205/S1#251/R2#6/R1#109/F#8");
F(3802808600353586230,"I3$F4:SF#208/S2#222/S1#223/R2#2/R1#226/F#4");
F(4195747080264955940,"I55$F16:SF#226/S2#283/S1#265/R2#11/R1#202/F#16");
F(5406091108340868414,"I59$F20:SF#276/S2#246/S1#260/R2#10/R1#243/F#20");
F(5638356690618577596,"I5$F6:SF#229/S2#231/S1#234/R2#41/R1#243/F#6");
F(4226635148494128808,"I6$F7:SF#218/S2#181/S1#233/R2#11/R1#235/F#7");
F(2314329707695715772,"I7$F8:SF#231/S2#284/S1#244/R2#7/R1#239/F#8");
F(4520312339023212969,"I56$F17:SF#353/S2#250/S1#273/R2#14/R1#268/F#17");
F(8116131801685644327,"I63$F27:SF#277/S2#354/S1#290/R2#27/R1#274/F#27");
F(3349949275526817468,"I4$F5:SF#242/S2#271/S1#241/R2#25/R1#244/F#5");
F(7975065811609482044,"I9$F10:SF#248/S2#234/S1#254/R2#21/R1#254/F#10");
F(8886243887826220935,"I66$F30:SF#265/S2#324/S1#283/R2#25/R1#276/F#30");
F(3134542423626761781,"I61$F23:SF#282/S2#240/S1#275/R2#30/R1#266/F#23");
F(5275975368906193067,"I8$F9:SF#253/S2#264/S1#251/R2#28/R1#241/F#9");
F(278198136637759014,"I14$F15:SF#237/S2#227/S1#238/R2#43/R1#246/F#15");
F(8379311440766321847,"I64$F28:SF#373/S2#251/S1#287/R2#32/R1#277/F#28");
F(1386906049866256052,"I60$F21:SF#267/S2#272/S1#269/R2#16/R1#265/F#21");
F(4057180517666868876,"I65$F29:SF#290/S2#308/S1#281/R2#39/R1#273/F#29");
F(4241965037862727263,"I17$F18:SF#285/S2#266/S1#279/R2#46/R1#280/F#18");
F(444731048967486637,"I26$F27:SF#298/S2#277/S1#309/R2#49/R1#284/F#27");
F(7289137621948319070,"I27$F28:SF#326/S2#281/S1#325/R2#47/R1#293/F#28");
F(8447193674411233352,"I15$F16:SF#288/S2#288/S1#304/R2#50/R1#307/F#16");

F(2022161639612168727,"K206$F4:SF#41/S2#33/S1#29/R2#4/R1#2/F#4");
F(2136180725277723014,"K201$F2:SF#30/S2#2/S1#2/R2#3/R1#7/F#2");
F(8511283413515004046,"K80$F2:SF#1/S2#4/S1#1/R2#1/R1#1/F#2");
F(1742331872028683310,"K203$F16:SF#10/S2#4/S1#27/R2#8/R1#11/F#16");
F(2236107839235716154,"K163$F1:SF#22/S2#1/S1#7/R2#3/R1#3/F#1");
F(6421907868264318107,"K150$F5:SF#10/S2#2/S1#2/R2#1/R1#2/F#5");
F(3058834008588623156,"K143$F9:SF#1/S2#8/S1#2/R2#4/R1#6/F#9");
F(5948984817210960430,"K199$F27:SF#15/S2#13/S1#43/R2#12/R1#11/F#27");
F(1629860095196085162,"K79$F1:SF#3/S2#2/S1#4/R2#8/R1#5/F#1");
F(1795295334279159044,"K181$F5:SF#1/S2#3/S1#17/R2#6/R1#5/F#5");
F(8518970088731785354,"K0$F1:SF#1/S2#1/S1#1/R2#1/R1#1/F#1");
F(2898191284304823214,"K202$F6:SF#9/S2#14/S1#31/R2#16/R1#15/F#6");
F(8190415147045106715,"K114$F4:SF#5/S2#5/S1#8/R2#7/R1#8/F#4");
F(3655326041580207642,"K141$F3:SF#11/S2#9/S1#3/R2#7/R1#8/F#3");
F(197169562965863561,"K176$F7:SF#16/S2#9/S1#13/R2#19/R1#22/F#7");
F(4291370988711077560,"K151$F10:SF#16/S2#9/S1#10/R2#8/R1#6/F#10");
F(3069490347229281977,"K189$F29:SF#33/S2#12/S1#14/R2#27/R1#23/F#29");
F(8552124084012212910,"K101$F4:SF#4/S2#17/S1#6/R2#10/R1#7/F#4");
F(9155330053304515848,"K52$F4:SF#6/S2#6/S1#3/R2#10/R1#6/F#4");
F(401963009761369499,"K208$F39:SF#52/S2#49/S1#50/R2#38/R1#28/F#39");
F(1547330718875328407,"K132$F17:SF#9/S2#2/S1#19/R2#5/R1#11/F#17");
F(6352614715565689230,"K194$F42:SF#20/S2#16/S1#46/R2#35/R1#31/F#42");
F(694996937355109817,"K212$F47:SF#25/S2#64/S1#57/R2#47/R1#43/F#47");
F(5241695375291453976,"K196$F9:SF#12/S2#30/S1#9/R2#20/R1#20/F#9");
F(3825093308631749895,"K195$F46:SF#23/S2#22/S1#28/R2#37/R1#37/F#46");
F(412407605357853496,"K204$F27:SF#62/S2#26/S1#70/R2#44/R1#49/F#27");
F(6601280592130816149,"K144$F10:SF#28/S2#14/S1#19/R2#21/R1#19/F#10");
F(2530267739734934204,"K147$F26:SF#31/S2#17/S1#18/R2#19/R1#21/F#26");
F(2600052556684937880,"K153$F27:SF#8/S2#17/S1#19/R2#18/R1#19/F#27");
F(1550988192424017718,"K174$F9:SF#1/S2#1/S1#1/R2#1/R1#1/F#9");
F(3928840485698554284,"K175$F22:SF#64/S2#22/S1#26/R2#39/R1#39/F#22");
F(3971705552564272533,"K197$F23:SF#32/S2#59/S1#65/R2#42/R1#53/F#23");
F(2898763031483272234,"K210$F14:SF#29/S2#28/S1#5/R2#8/R1#4/F#14");
F(8844850375727871631,"K142$F6:SF#5/S2#6/S1#6/R2#12/R1#14/F#6");
F(592315757920809880,"K50$F1:SF#1/S2#1/S1#1/R2#2/R1#1/F#1");
F(3163244831530486581,"K81$F4:SF#6/S2#1/S1#5/R2#6/R1#3/F#4");
F(5745869153682214825,"K129$F3:SF#4/S2#6/S1#14/R2#10/R1#9/F#3");
F(4943000806838775096,"K155$F32:SF#54/S2#32/S1#50/R2#29/R1#29/F#32");
F(2057874786464054454,"K191$F28:SF#5/S2#10/S1#27/R2#19/R1#16/F#28");
F(298401657656713141,"K168$F5:SF#24/S2#33/S1#2/R2#23/R1#19/F#5");
F(9128523349539716510,"K211$F20:SF#37/S2#40/S1#45/R2#17/R1#21/F#20");
F(341215326140895803,"K187$F6:SF#5/S2#5/S1#30/R2#7/R1#10/F#6");
F(4230036969763902890,"K169$F24:SF#7/S2#13/S1#16/R2#17/R1#15/F#24");
F(318111077962947620,"K190$F36:SF#36/S2#13/S1#5/R2#16/R1#26/F#36");
F(2499072888595548968,"K82$F6:SF#4/S2#6/S1#18/R2#5/R1#7/F#6");
F(5768871116621042574,"K182$F27:SF#96/S2#32/S1#106/R2#41/R1#49/F#27");
F(3606274686145472265,"K207$F22:SF#50/S2#17/S1#12/R2#29/R1#32/F#22");
F(5226824549916222733,"K134$F26:SF#17/S2#39/S1#21/R2#25/R1#25/F#26");
F(1527350212450273945,"K54$F6:SF#5/S2#4/S1#6/R2#4/R1#4/F#6");
F(3269348090195741094,"K188$F22:SF#37/S2#43/S1#9/R2#20/R1#18/F#22");
F(2092261888743980950,"K145$F13:SF#16/S2#15/S1#16/R2#17/R1#23/F#13");
F(2200419193116698271,"K115$F18:SF#35/S2#21/S1#23/R2#14/R1#18/F#18");
F(5228144428231440920,"K193$F22:SF#43/S2#27/S1#48/R2#34/R1#21/F#22");
F(4866778318463848378,"K179$F40:SF#40/S2#28/S1#50/R2#36/R1#35/F#40");
F(8299620835019003157,"K154$F28:SF#36/S2#37/S1#59/R2#63/R1#49/F#28");
F(1601055601986374440,"K53$F5:SF#3/S2#3/S1#5/R2#3/R1#3/F#5");
F(5299395628468218383,"K166$F40:SF#43/S2#33/S1#126/R2#43/R1#47/F#40");
F(3288776760969465245,"K51$F3:SF#4/S2#5/S1#4/R2#5/R1#5/F#3");
F(1232076124525634623,"K159$F26:SF#23/S2#9/S1#6/R2#14/R1#17/F#26");
F(4279838433084837291,"K171$F38:SF#14/S2#21/S1#21/R2#33/R1#32/F#38");
F(8145357683291613598,"K180$F49:SF#41/S2#61/S1#82/R2#47/R1#43/F#49");
F(4881686547788665861,"K209$F40:SF#24/S2#54/S1#23/R2#47/R1#44/F#40");
F(3860755577484170923,"K205$F38:SF#57/S2#71/S1#77/R2#65/R1#79/F#38");
F(2598071918448558861,"K200$F41:SF#49/S2#42/S1#17/R2#32/R1#35/F#41");
F(5025198125807372309,"K157$F48:SF#119/S2#36/S1#115/R2#38/R1#39/F#48");
F(2706439080732400772,"K152$F24:SF#13/S2#28/S1#24/R2#16/R1#15/F#24");
F(1960248414590928652,"K170$F33:SF#55/S2#49/S1#33/R2#26/R1#27/F#33");
F(5120100944323436719,"K146$F15:SF#25/S2#29/S1#28/R2#22/R1#17/F#15");
F(8709181585333818231,"K33$F34:SF#59/S2#16/S1#82/R2#40/R1#37/F#34");
F(3183552614667455998,"K110$F39:SF#19/S2#271/S1#250/R2#306/R1#167/F#39");
F(7969689113532702645,"K58$F12:SF#15/S2#161/S1#18/R2#11/R1#11/F#12");
F(8150787557788581022,"K104$F17:SF#51/S2#11/S1#69/R2#14/R1#16/F#17");
F(4601286318787073658,"K135$F31:SF#32/S2#52/S1#35/R2#44/R1#38/F#31");
F(6271305959555860159,"K164$F30:SF#44/S2#16/S1#21/R2#29/R1#30/F#30");
F(1620751192514368058,"K160$F45:SF#51/S2#83/S1#45/R2#224/R1#186/F#45");
F(5854525087432710029,"K57$F11:SF#8/S2#8/S1#20/R2#9/R1#9/F#11");
F(2397900720576934911,"K126$F43:SF#12/S2#231/S1#62/R2#325/R1#176/F#43");
F(398902011633481782,"K102$F10:SF#18/S2#9/S1#17/R2#22/R1#18/F#10");
F(1699044507034991541,"K55$F8:SF#14/S2#25/S1#10/R2#25/R1#12/F#8");
F(370484772824361268,"K198$F26:SF#50/S2#56/S1#94/R2#53/R1#76/F#26");
F(1669468386812627381,"K183$F36:SF#35/S2#29/S1#94/R2#83/R1#75/F#36");
F(8293403501025369004,"K172$F39:SF#65/S2#44/S1#52/R2#38/R1#34/F#39");
F(5181199465367367837,"K20$F21:SF#80/S2#62/S1#34/R2#8/R1#6/F#21");
F(8254061931943058088,"K165$F31:SF#52/S2#21/S1#24/R2#30/R1#32/F#31");
F(2272352593937766031,"K162$F50:SF#42/S2#56/S1#34/R2#44/R1#42/F#50");
F(2325961418833597051,"K34$F35:SF#85/S2#82/S1#9/R2#22/R1#55/F#35");
F(5873280809200206637,"K119$F31:SF#18/S2#238/S1#76/R2#123/R1#69/F#31");
F(3070363922034611977,"K103$F15:SF#14/S2#18/S1#20/R2#12/R1#13/F#15");
F(8224142657064213023,"K106$F32:SF#22/S2#208/S1#284/R2#202/R1#154/F#32");
F(8952060645484860020,"K35$F36:SF#44/S2#107/S1#74/R2#20/R1#26/F#36");
F(8838899300918298118,"K1$F2:SF#5/S2#3/S1#3/R2#2/R1#2/F#2");
F(3464416317891875448,"K38$F39:SF#76/S2#55/S1#57/R2#32/R1#15/F#39");
F(8739246146536614577,"K184$F43:SF#49/S2#192/S1#284/R2#279/R1#301/F#43");
F(656431029792935046,"K56$F9:SF#9/S2#10/S1#9/R2#7/R1#8/F#9");
F(5212695598331886615,"K118$F28:SF#121/S2#49/S1#57/R2#48/R1#30/F#28");
F(6727596486316024188,"K16$F17:SF#16/S2#36/S1#44/R2#39/R1#28/F#17");
F(1155818232529296054,"K156$F42:SF#96/S2#24/S1#66/R2#34/R1#40/F#42");
F(1787962576328198554,"K91$F35:SF#247/S2#53/S1#85/R2#47/R1#27/F#35");
F(666319544512366107,"K2$F3:SF#23/S2#7/S1#12/R2#3/R1#3/F#3");
F(5056238959685678862,"K167$F45:SF#47/S2#152/S1#49/R2#46/R1#45/F#45");
F(7439679661155227813,"K130$F12:SF#31/S2#29/S1#37/R2#39/R1#32/F#12");
F(4418317869166056564,"K36$F37:SF#61/S2#96/S1#66/R2#48/R1#41/F#37");
F(4234836207804486809,"K131$F15:SF#25/S2#43/S1#50/R2#32/R1#33/F#15");
F(5152209991137394325,"K17$F18:SF#117/S2#132/S1#67/R2#10/R1#7/F#18");
F(1283027511608763791,"K7$F8:SF#39/S2#151/S1#56/R2#37/R1#69/F#8");
F(1989278944500406811,"K133$F24:SF#18/S2#27/S1#18/R2#18/R1#19/F#24");
F(2249397537538379301,"K18$F19:SF#12/S2#162/S1#164/R2#167/R1#205/F#19");
F(7619821791473067677,"K84$F15:SF#17/S2#20/S1#20/R2#13/R1#14/F#15");
F(8726658914583212958,"K83$F13:SF#15/S2#13/S1#11/R2#11/R1#12/F#13");
F(4120018148809731754,"K125$F41:SF#13/S2#114/S1#79/R2#287/R1#225/F#41");
F(194270180617032311,"K178$F34:SF#52/S2#253/S1#210/R2#194/R1#158/F#34");
F(1885602321478915450,"K47$F48:SF#71/S2#70/S1#21/R2#50/R1#86/F#48");
F(4982405369800654093,"K192$F44:SF#44/S2#375/S1#266/R2#279/R1#283/F#44");
F(5362970779509273343,"K124$F40:SF#23/S2#133/S1#53/R2#315/R1#169/F#40");
F(875116251780418167,"K161$F48:SF#48/S2#122/S1#38/R2#67/R1#62/F#48");
F(2727893073611417468,"K46$F47:SF#75/S2#42/S1#92/R2#27/R1#14/F#47");
F(1061013646424486461,"K123$F39:SF#22/S2#251/S1#115/R2#293/R1#99/F#39");
F(1181720738071384890,"K138$F43:SF#27/S2#167/S1#41/R2#126/R1#101/F#43");
F(1973158553358703371,"K41$F42:SF#124/S2#130/S1#62/R2#12/R1#21/F#42");
F(8910155907819251831,"K77$F46:SF#43/S2#81/S1#70/R2#46/R1#62/F#46");
F(4873499731942200050,"K122$F38:SF#25/S2#271/S1#145/R2#299/R1#106/F#38");
F(193214979612767239,"K43$F44:SF#41/S2#79/S1#37/R2#31/R1#20/F#44");
F(1605564736644332475,"K116$F22:SF#99/S2#20/S1#46/R2#17/R1#17/F#22");
F(7310011657621240696,"K30$F31:SF#43/S2#64/S1#77/R2#17/R1#8/F#31");
F(7476914387906268596,"K29$F30:SF#56/S2#88/S1#41/R2#14/R1#33/F#30");
F(1442069037729993902,"K24$F25:SF#40/S2#13/S1#10/R2#38/R1#54/F#25");
F(6522643721635004020,"K22$F23:SF#116/S2#61/S1#5/R2#18/R1#4/F#23");
F(9159879082026208900,"K86$F21:SF#23/S2#40/S1#94/R2#145/R1#90/F#21");
F(364524733125757446,"K85$F18:SF#41/S2#26/S1#27/R2#17/R1#17/F#18");
F(1477311804412033172,"K107$F35:SF#12/S2#12/S1#213/R2#99/R1#60/F#35");
F(8125149089287271958,"K25$F26:SF#15/S2#6/S1#29/R2#25/R1#22/F#26");
F(8152389742655451518,"K173$F46:SF#57/S2#236/S1#213/R2#241/R1#204/F#46");
F(3875946869444578675,"K127$F44:SF#16/S2#270/S1#43/R2#207/R1#111/F#44");
F(4853494826092870685,"K78$F49:SF#38/S2#100/S1#42/R2#49/R1#33/F#49");
F(711107715663480475,"K105$F31:SF#8/S2#147/S1#223/R2#221/R1#48/F#31");
F(2889705554361659311,"K111$F42:SF#15/S2#76/S1#50/R2#337/R1#120/F#42");
F(9119269597927908985,"K49$F50:SF#37/S2#74/S1#50/R2#42/R1#30/F#50");
F(1036111620325264655,"K37$F38:SF#81/S2#65/S1#14/R2#13/R1#32/F#38");
F(6602049349324268410,"K39$F40:SF#18/S2#60/S1#16/R2#24/R1#16/F#40");
F(589162809581703540,"K40$F41:SF#38/S2#34/S1#4/R2#19/R1#31/F#41");
F(2293014471917532716,"K32$F33:SF#72/S2#29/S1#17/R2#35/R1#25/F#33");
F(6434929383499986989,"K60$F15:SF#95/S2#171/S1#197/R2#14/R1#107/F#15");
F(6588062371232880523,"K26$F27:SF#64/S2#27/S1#64/R2#16/R1#78/F#27");
F(8887989766627146406,"K140$F46:SF#60/S2#142/S1#181/R2#35/R1#39/F#46");
F(1533319797165204916,"K149$F50:SF#101/S2#86/S1#112/R2#42/R1#46/F#50");
F(8988088862477140410,"K136$F32:SF#33/S2#248/S1#69/R2#243/R1#171/F#32");
F(3266852091937236264,"K88$F25:SF#180/S2#237/S1#194/R2#25/R1#94/F#25");
F(2338777060732788087,"K42$F43:SF#14/S2#128/S1#101/R2#53/R1#56/F#43");
F(6144118856534931062,"K139$F45:SF#28/S2#124/S1#88/R2#89/R1#73/F#45");
F(1590440044305284247,"K89$F28:SF#25/S2#48/S1#69/R2#63/R1#73/F#28");
F(6323398404007230356,"K158$F49:SF#46/S2#185/S1#93/R2#207/R1#170/F#49");
F(510682984636358813,"K177$F33:SF#49/S2#255/S1#158/R2#103/R1#78/F#33");
F(6868085068297557276,"K63$F18:SF#17/S2#233/S1#205/R2#221/R1#128/F#18");
F(9093161204932696693,"K67$F25:SF#10/S2#196/S1#124/R2#183/R1#158/F#25");
F(4947600625979361144,"K48$F49:SF#32/S2#43/S1#47/R2#46/R1#12/F#49");
F(5898977159845649039,"K76$F45:SF#65/S2#69/S1#24/R2#39/R1#81/F#45");
F(1482009832293932278,"K120$F36:SF#24/S2#165/S1#211/R2#205/R1#142/F#36");
F(8895015617346169757,"K70$F35:SF#18/S2#74/S1#88/R2#56/R1#14/F#35");
F(1819507423235425713,"K185$F45:SF#44/S2#289/S1#302/R2#339/R1#244/F#45");
F(8529640420879332502,"K23$F24:SF#10/S2#11/S1#35/R2#11/R1#5/F#24");
F(6499820220146939284,"K21$F22:SF#49/S2#10/S1#72/R2#33/R1#59/F#22");
F(8158383941639935231,"K108$F36:SF#25/S2#29/S1#184/R2#259/R1#182/F#36");
F(2288425813991832438,"K148$F38:SF#22/S2#209/S1#46/R2#67/R1#72/F#38");
F(3977245052232935342,"K31$F32:SF#36/S2#23/S1#23/R2#15/R1#24/F#32");
F(4443401065390488586,"K65$F22:SF#20/S2#77/S1#215/R2#228/R1#245/F#22");
F(6306234012751190331,"K61$F16:SF#246/S2#216/S1#316/R2#15/R1#79/F#16");
F(3286548536785117810,"K109$F38:SF#13/S2#204/S1#208/R2#268/R1#86/F#38");
F(6177080717565639189,"K100$F50:SF#299/S2#318/S1#298/R2#48/R1#238/F#50");
F(4966076089002049445,"K87$F23:SF#172/S2#256/S1#245/R2#23/R1#51/F#23");
F(6907403965275331444,"K28$F29:SF#53/S2#83/S1#87/R2#23/R1#27/F#29");
F(6820492760253025266,"K99$F49:SF#13/S2#115/S1#155/R2#267/R1#208/F#49");
F(6225795837841847477,"K137$F39:SF#198/S2#252/S1#54/R2#36/R1#40/F#39");
F(7284337206296251195,"K97$F47:SF#24/S2#215/S1#260/R2#206/R1#207/F#47");
F(5057310003820178235,"K3$F4:SF#60/S2#46/S1#180/R2#4/R1#70/F#4");
F(4194833724496297049,"K121$F37:SF#282/S2#275/S1#302/R2#25/R1#131/F#37");
F(3354381128037441429,"K117$F27:SF#178/S2#179/S1#201/R2#27/R1#27/F#27");
F(269380174301698212,"K68$F26:SF#294/S2#242/S1#275/R2#23/R1#258/F#26");
F(8793852541381003199,"K62$F17:SF#263/S2#276/S1#229/R2#16/R1#191/F#17");
F(2195319211953369072,"K186$F46:SF#22/S2#369/S1#377/R2#-1/R1#350/F#46");
F(1101224299471989816,"K66$F23:SF#279/S2#238/S1#281/R2#17/R1#252/F#23");
F(6014371790667473786,"K75$F44:SF#112/S2#125/S1#162/R2#42/R1#25/F#44");
F(5729820952854599037,"K44$F45:SF#77/S2#20/S1#76/R2#43/R1#19/F#45");
F(6236487501678992042,"K92$F37:SF#249/S2#306/S1#299/R2#30/R1#230/F#37");
F(7511504211438806075,"K112$F46:SF#317/S2#254/S1#288/R2#33/R1#268/F#46");
F(3877095316539444524,"K6$F7:SF#220/S2#204/S1#223/R2#21/R1#204/F#7");
F(8114205203613237948,"K128$F45:SF#21/S2#67/S1#69/R2#283/R1#172/F#45");
F(8168499792844891574,"K113$F48:SF#289/S2#266/S1#314/R2#31/R1#251/F#48");
F(1176094135028483889,"K98$F48:SF#14/S2#219/S1#141/R2#217/R1#224/F#48");
F(3748731153553903406,"K8$F9:SF#250/S2#268/S1#239/R2#7/R1#229/F#9");
F(2123479519082202425,"K59$F14:SF#66/S2#218/S1#26/R2#12/R1#30/F#14");
F(3263227363297490847,"K94$F40:SF#11/S2#233/S1#187/R2#205/R1#235/F#40");
F(3427324263517792949,"K95$F43:SF#254/S2#265/S1#275/R2#39/R1#248/F#43");
F(251645394832481960,"K64$F21:SF#318/S2#270/S1#312/R2#40/R1#293/F#21");
F(7321864921329322815,"K9$F10:SF#255/S2#236/S1#222/R2#6/R1#235/F#10");
F(3613893305105283988,"K72$F38:SF#70/S2#31/S1#100/R2#27/R1#76/F#38");
F(3704868580122978104,"K90$F29:SF#211/S2#244/S1#218/R2#24/R1#105/F#29");
F(2873323238951176061,"K45$F46:SF#3/S2#139/S1#123/R2#34/R1#45/F#46");
F(2151121542623135032,"K12$F13:SF#214/S2#226/S1#255/R2#41/R1#233/F#13");
F(7753793761036552126,"K5$F6:SF#217/S2#253/S1#252/R2#5/R1#231/F#6");
F(5026113851711044134,"K19$F20:SF#227/S2#231/S1#240/R2#49/R1#222/F#20");
F(2398198096113594490,"K27$F28:SF#52/S2#98/S1#85/R2#26/R1#9/F#28");
F(5649886108713646009,"K4$F5:SF#248/S2#258/S1#219/R2#47/R1#234/F#5");
F(1818122004115187132,"K73$F39:SF#298/S2#343/S1#292/R2#22/R1#271/F#39");
F(1562284944082941989,"K96$F44:SF#283/S2#315/S1#282/R2#28/R1#223/F#44");
F(6819958275743296295,"K15$F16:SF#243/S2#203/S1#272/R2#36/R1#238/F#16");
F(5700807849837016356,"K13$F14:SF#222/S2#240/S1#227/R2#28/R1#236/F#14");
F(7525293199162176539,"K11$F12:SF#246/S2#257/S1#215/R2#9/R1#245/F#12");
F(2277830391838620548,"K71$F36:SF#406/S2#340/S1#329/R2#20/R1#291/F#36");
F(2755192645324719291,"K69$F31:SF#234/S2#308/S1#278/R2#19/R1#276/F#31");
F(5487397987021306916,"K93$F39:SF#251/S2#293/S1#269/R2#27/R1#236/F#39");
F(5697436825357599781,"K14$F15:SF#245/S2#262/S1#232/R2#30/R1#255/F#15");
F(3367113314165209225,"K10$F11:SF#364/S2#286/S1#234/R2#44/R1#270/F#11");
F(2605715183717137451,"K74$F40:SF#283/S2#302/S1#273/R2#29/R1#268/F#40");

F(411251116694972989,"L19$F48:SF#68/S2#42/S1#143/R2#42/R1#65/F#48");
F(3331023361875134394,"L6$F50:SF#101/S2#111/S1#6/R2#40/R1#48/F#50");
F(2175251140821924405,"L18$F23:SF#242/S2#77/S1#116/R2#14/R1#12/F#23");
F(766485366452993590,"L8$F32:SF#46/S2#135/S1#2/R2#13/R1#29/F#32");
F(187036541575247551,"L1$F24:SF#235/S2#169/S1#95/R2#11/R1#35/F#24");
F(4484241170572254991,"L3$F36:SF#4/S2#59/S1#64/R2#32/R1#19/F#36");
F(4526462777809837860,"L4$F23:SF#97/S2#65/S1#77/R2#32/R1#9/F#23");
F(151767480912150298,"L11$F50:SF#17/S2#57/S1#133/R2#35/R1#71/F#50");
F(6970734528675010074,"L16$F23:SF#230/S2#139/S1#174/R2#10/R1#74/F#23");
F(2031467004052975284,"L2$F7:SF#116/S2#78/S1#135/R2#14/R1#17/F#7");
F(3465581198990782396,"L0$F48:SF#267/S2#367/S1#196/R2#13/R1#73/F#48");
F(5951806109516713272,"L10$F40:SF#32/S2#64/S1#41/R2#27/R1#41/F#40");
F(6863265965551779867,"L7$F16:SF#305/S2#7/S1#140/R2#15/R1#68/F#16");
F(111553942308530366,"L12$F32:SF#389/S2#201/S1#202/R2#31/R1#78/F#32");
F(4512086942579038520,"L17$F29:SF#3/S2#59/S1#79/R2#67/R1#18/F#29");
F(3533381343196891577,"L5$F37:SF#57/S2#131/S1#163/R2#30/R1#46/F#37");
F(5564560794046967322,"L9$F35:SF#106/S2#150/S1#7/R2#48/R1#89/F#35");
F(871800773856202552,"L14$F28:SF#11/S2#188/S1#40/R2#62/R1#61/F#28");
F(2034844226790046487,"L15$F4:SF#52/S2#185/S1#36/R2#8/R1#20/F#4");
F(7070177787488446653,"L13$F10:SF#189/S2#91/S1#31/R2#1/R1#25/F#10");

F(6971978146927303039,"N327$F16:SF#329/S2#343/S1#412/R2#48/R1#305/F#16");
F(8968209116449933547,"N193$F9:SF#5/S2#143/S1#3/R2#70/R1#96/F#9");
F(9189890822537441767,"N36$F37:SF#35/S2#82/S1#55/R2#31/R1#29/F#37");
F(564403692922351971,"N37$F38:SF#123/S2#30/S1#111/R2#3/R1#21/F#38");
F(4713149102870561322,"N589$F20:SF#10/S2#344/S1#17/R2#236/R1#49/F#20");
F(5114691186506685295,"N602$F33:SF#76/S2#277/S1#239/R2#36/R1#310/F#33");
F(4377273927578615821,"N67$F36:SF#40/S2#105/S1#61/R2#29/R1#63/F#36");
F(5848805985793178851,"N189$F48:SF#17/S2#37/S1#174/R2#8/R1#40/F#48");
F(3510304088320060829,"N202$F39:SF#14/S2#88/S1#57/R2#26/R1#23/F#39");
F(1985021035023894243,"N223$F49:SF#154/S2#176/S1#249/R2#45/R1#189/F#49");
F(8363599805794707214,"N32$F33:SF#12/S2#22/S1#50/R2#34/R1#19/F#33");
F(5840887720581799439,"N599$F5:SF#7/S2#251/S1#55/R2#298/R1#85/F#5");
F(1235717678268898315,"N156$F31:SF#162/S2#92/S1#6/R2#13/R1#137/F#31");
F(4521384816780190571,"N529$F10:SF#10/S2#121/S1#106/R2#225/R1#14/F#10");
F(1483977374129329891,"N544$F10:SF#9/S2#83/S1#264/R2#316/R1#293/F#10");
F(2329523784042433255,"N14$F15:SF#20/S2#26/S1#21/R2#27/R1#3/F#15");
F(1979452682479559177,"N126$F46:SF#207/S2#49/S1#71/R2#2/R1#80/F#46");
F(3012391188661996003,"N72$F42:SF#85/S2#130/S1#6/R2#49/R1#54/F#42");
F(3354457666060776714,"N569$F7:SF#8/S2#290/S1#251/R2#77/R1#233/F#7");
F(7235603010799210223,"N594$F18:SF#288/S2#234/S1#213/R2#46/R1#108/F#18");
F(1699966331646584330,"N382$F3:SF#1/S2#1/S1#277/R2#51/R1#78/F#3");
F(1941404074161413775,"N162$F42:SF#174/S2#193/S1#10/R2#9/R1#20/F#42");
F(6519013152047968398,"N62$F29:SF#10/S2#84/S1#10/R2#24/R1#33/F#29");
F(3919265345930223851,"N83$F15:SF#91/S2#34/S1#58/R2#14/R1#24/F#15");
F(8924795664536192931,"N179$F7:SF#231/S2#77/S1#189/R2#26/R1#96/F#7");
F(279239180574679812,"N98$F6:SF#75/S2#90/S1#46/R2#38/R1#10/F#6");
F(2683959067181856820,"N637$F5:SF#265/S2#20/S1#286/R2#14/R1#2/F#5");
F(5722954338355788067,"N263$F31:SF#269/S2#9/S1#20/R2#38/R1#201/F#31");
F(5268130929443812139,"N237$F6:SF#218/S2#260/S1#132/R2#3/R1#10/F#6");
F(2390042738591023803,"N390$F12:SF#6/S2#239/S1#251/R2#107/R1#267/F#12");
F(8683921126624603119,"N197$F17:SF#11/S2#177/S1#44/R2#188/R1#89/F#17");
F(6848314951080301468,"N240$F27:SF#128/S2#46/S1#10/R2#11/R1#29/F#27");
F(8953191050976119691,"N228$F17:SF#215/S2#226/S1#227/R2#34/R1#61/F#17");
F(6457113302054152511,"N61$F26:SF#86/S2#2/S1#80/R2#27/R1#19/F#26");
F(1114774507536867939,"N171$F29:SF#9/S2#197/S1#144/R2#44/R1#116/F#29");
F(6857093899887591143,"N250$F28:SF#244/S2#42/S1#142/R2#8/R1#110/F#28");
F(8420079231450898083,"N255$F18:SF#227/S2#361/S1#271/R2#20/R1#106/F#18");
F(9195249098557555431,"N57$F16:SF#31/S2#21/S1#49/R2#47/R1#53/F#16");
F(2253879952431066599,"N64$F31:SF#14/S2#31/S1#91/R2#44/R1#30/F#31");
F(270525461379441463,"N297$F25:SF#355/S2#353/S1#97/R2#39/R1#180/F#25");
F(8702217591382562723,"N135$F13:SF#73/S2#157/S1#195/R2#43/R1#49/F#13");
F(1299887053918133385,"N109$F34:SF#18/S2#44/S1#75/R2#2/R1#30/F#34");
F(6341916241036193519,"N168$F18:SF#281/S2#64/S1#42/R2#14/R1#12/F#18");
F(489826487901376611,"N291$F36:SF#31/S2#198/S1#264/R2#47/R1#34/F#36");
F(8624485076218308843,"N18$F19:SF#9/S2#106/S1#108/R2#21/R1#60/F#19");
F(623536658174648716,"N159$F35:SF#44/S2#83/S1#249/R2#50/R1#59/F#35");
F(7872656679837785579,"N270$F13:SF#44/S2#135/S1#123/R2#14/R1#92/F#13");
F(7571164265161253795,"N274$F29:SF#10/S2#269/S1#53/R2#139/R1#199/F#29");
F(178869896636669927,"N183$F15:SF#273/S2#97/S1#172/R2#22/R1#50/F#15");
F(3795423220949020771,"N136$F14:SF#134/S2#94/S1#148/R2#26/R1#92/F#14");
F(5769479782876580131,"N680$F35:SF#468/S2#360/S1#384/R2#44/R1#58/F#35");
F(6109547733287654937,"N450$F6:SF#6/S2#67/S1#149/R2#228/R1#116/F#6");
F(774060889073208099,"N249$F15:SF#31/S2#187/S1#278/R2#21/R1#116/F#15");
F(552652560546486669,"N233$F38:SF#106/S2#2/S1#145/R2#31/R1#114/F#38");
F(1813295159154732707,"N187$F22:SF#103/S2#131/S1#210/R2#6/R1#122/F#22");
F(2884621773509431069,"N625$F15:SF#8/S2#237/S1#185/R2#129/R1#48/F#15");
F(5772510999022106155,"N150$F5:SF#22/S2#78/S1#38/R2#2/R1#23/F#5");
F(7829838309397312739,"N608$F6:SF#9/S2#195/S1#187/R2#147/R1#172/F#6");
F(2146281861337808520,"N17$F18:SF#29/S2#104/S1#32/R2#10/R1#1/F#18");
F(204474229522106396,"N272$F18:SF#2/S2#79/S1#172/R2#50/R1#151/F#18");
F(1504844356956392334,"N7$F8:SF#23/S2#20/S1#37/R2#19/R1#22/F#8");
F(3921002711482057711,"N54$F8:SF#17/S2#90/S1#34/R2#16/R1#21/F#8");
F(4628336264419626659,"N68$F37:SF#9/S2#42/S1#33/R2#20/R1#24/F#37");
F(664350064547924260,"N280$F17:SF#6/S2#223/S1#163/R2#125/R1#13/F#17");
F(7796925120318890019,"N276$F37:SF#31/S2#14/S1#102/R2#42/R1#106/F#37");
F(1380449528016823172,"N232$F37:SF#261/S2#92/S1#135/R2#11/R1#195/F#37");
F(7175756112445973128,"N273$F28:SF#9/S2#86/S1#75/R2#13/R1#5/F#28");
F(3163300288616693261,"N152$F9:SF#166/S2#17/S1#67/R2#15/R1#95/F#9");
F(950052694723598123,"N33$F34:SF#18/S2#9/S1#77/R2#13/R1#36/F#34");
F(1576877689016231852,"N311$F3:SF#61/S2#117/S1#216/R2#39/R1#178/F#3");
F(2460456850527880099,"N91$F35:SF#58/S2#16/S1#4/R2#18/R1#40/F#35");
F(3749068763543985383,"N621$F25:SF#11/S2#311/S1#49/R2#84/R1#188/F#25");
F(922502970502840039,"N185$F20:SF#45/S2#43/S1#223/R2#31/R1#98/F#20");
F(6023641115909509607,"N139$F27:SF#125/S2#109/S1#129/R2#44/R1#131/F#27");
F(5389244840713658859,"N141$F38:SF#11/S2#197/S1#23/R2#74/R1#111/F#38");
F(1892189409119569383,"N596$F21:SF#8/S2#270/S1#137/R2#62/R1#244/F#21");
F(2848566999914783637,"N154$F24:SF#9/S2#170/S1#18/R2#47/R1#140/F#24");
F(1606380120168944792,"N574$F7:SF#6/S2#101/S1#76/R2#317/R1#11/F#7");
F(2910221441309359267,"N157$F32:SF#53/S2#84/S1#130/R2#21/R1#53/F#32");
F(1482664427976337949,"N84$F20:SF#69/S2#39/S1#35/R2#16/R1#27/F#20");
F(8984760676974275311,"N22$F23:SF#77/S2#6/S1#40/R2#17/R1#17/F#23");
F(2235287628119501799,"N23$F24:SF#30/S2#90/S1#24/R2#28/R1#70/F#24");
F(6020278368440880924,"N100$F12:SF#44/S2#122/S1#115/R2#30/R1#36/F#12");
F(8083172698519323427,"N587$F15:SF#1/S2#159/S1#63/R2#124/R1#12/F#15");
F(2429510519831012102,"N195$F12:SF#274/S2#141/S1#141/R2#48/R1#71/F#12");
F(7173525829846828451,"N219$F17:SF#64/S2#5/S1#296/R2#27/R1#71/F#17");
F(9076211362231817707,"N275$F34:SF#207/S2#220/S1#250/R2#26/R1#116/F#34");
F(7101137269470810939,"N512$F28:SF#314/S2#285/S1#338/R2#10/R1#279/F#28");
F(3585726066387520910,"N116$F6:SF#42/S2#68/S1#195/R2#37/R1#21/F#6");
F(2984495074813100139,"N231$F29:SF#255/S2#54/S1#16/R2#21/R1#56/F#29");
F(468693443110258923,"N581$F10:SF#10/S2#291/S1#139/R2#319/R1#242/F#10");
F(2091777116720794147,"N575$F9:SF#9/S2#406/S1#138/R2#324/R1#166/F#9");
F(800540337778146663,"N673$F21:SF#162/S2#157/S1#318/R2#39/R1#93/F#21");
F(7486968419075518447,"N43$F44:SF#38/S2#46/S1#144/R2#30/R1#58/F#44");
F(7897734817720656788,"N125$F43:SF#60/S2#5/S1#53/R2#34/R1#39/F#43");
F(5719011851239569573,"N664$F26:SF#74/S2#481/S1#378/R2#48/R1#232/F#26");
F(962782634074650661,"N262$F21:SF#7/S2#111/S1#100/R2#116/R1#78/F#21");
F(7425350172804603050,"N21$F22:SF#15/S2#75/S1#38/R2#12/R1#43/F#22");
F(3741215182799785323,"N639$F27:SF#342/S2#51/S1#174/R2#50/R1#324/F#27");
F(6948630226366306211,"N545$F12:SF#4/S2#99/S1#367/R2#298/R1#67/F#12");
F(6606522453679941931,"N278$F9:SF#178/S2#65/S1#118/R2#8/R1#9/F#9");
F(8093357535087971559,"N190$F50:SF#30/S2#116/S1#183/R2#20/R1#33/F#50");
F(1306962577717005583,"N145$F43:SF#180/S2#47/S1#172/R2#27/R1#55/F#43");
F(2101603184066901660,"N65$F34:SF#3/S2#5/S1#120/R2#1/R1#15/F#34");
F(385946041368270055,"N120$F18:SF#63/S2#172/S1#86/R2#31/R1#118/F#18");
F(3792669893542755119,"N90$F32:SF#76/S2#81/S1#89/R2#35/R1#69/F#32");
F(3219856323353666325,"N296$F20:SF#207/S2#142/S1#169/R2#50/R1#105/F#20");
F(6885562923714696675,"N199$F20:SF#28/S2#68/S1#101/R2#27/R1#37/F#20");
F(2271275245108220771,"N169$F22:SF#49/S2#132/S1#240/R2#35/R1#38/F#22");
F(6344187818395773444,"N600$F12:SF#320/S2#239/S1#298/R2#34/R1#304/F#12");
F(7751336713618676643,"N222$F34:SF#320/S2#195/S1#204/R2#44/R1#225/F#34");
F(809817879231283943,"N13$F14:SF#139/S2#54/S1#76/R2#32/R1#23/F#14");
F(714167179396799207,"N669$F34:SF#9/S2#239/S1#97/R2#124/R1#212/F#34");
F(7181929998009706727,"N127$F48:SF#3/S2#140/S1#23/R2#16/R1#5/F#48");
F(9157542997387867235,"N74$F49:SF#80/S2#48/S1#59/R2#4/R1#39/F#49");
F(6679780980444702191,"N59$F19:SF#69/S2#30/S1#53/R2#15/R1#61/F#19");
F(3742544253384086319,"N177$F48:SF#82/S2#47/S1#111/R2#22/R1#81/F#48");
F(8997098146167679146,"N63$F30:SF#16/S2#16/S1#23/R2#42/R1#56/F#30");
F(4269487093927715171,"N111$F39:SF#21/S2#96/S1#72/R2#46/R1#4/F#39");
F(4248332245070779879,"N175$F38:SF#175/S2#83/S1#186/R2#41/R1#122/F#38");
F(8491032757751849863,"N158$F33:SF#121/S2#46/S1#77/R2#8/R1#49/F#33");
F(4082820745924846311,"N550$F7:SF#8/S2#249/S1#128/R2#78/R1#255/F#7");
F(8163339160101727011,"N123$F36:SF#68/S2#72/S1#19/R2#38/R1#74/F#36");
F(4489284411961579946,"N73$F43:SF#23/S2#132/S1#101/R2#35/R1#60/F#43");
F(6597285258635073767,"N132$F6:SF#47/S2#20/S1#146/R2#47/R1#29/F#6");
F(1286751406368037923,"N640$F11:SF#126/S2#281/S1#302/R2#46/R1#81/F#11");
F(6101944176304655769,"N166$F9:SF#139/S2#127/S1#86/R2#13/R1#59/F#9");
F(1365006991102279399,"N670$F44:SF#331/S2#91/S1#155/R2#45/R1#40/F#44");
F(8021488563518521059,"N19$F20:SF#92/S2#43/S1#64/R2#11/R1#25/F#20");
F(3905231587332488431,"N471$F10:SF#7/S2#44/S1#281/R2#185/R1#135/F#10");
F(9188192819929952099,"N8$F9:SF#69/S2#71/S1#52/R2#6/R1#28/F#9");
F(7433508039030022051,"N239$F26:SF#104/S2#61/S1#156/R2#44/R1#28/F#26");
F(9139629663887950179,"N86$F26:SF#112/S2#85/S1#3/R2#36/R1#48/F#26");
F(5752356929359124393,"N636$F30:SF#7/S2#180/S1#357/R2#92/R1#312/F#30");
F(4640468063191132903,"N112$F49:SF#67/S2#107/S1#37/R2#39/R1#40/F#49");
F(6042546381584335384,"N251$F43:SF#5/S2#245/S1#159/R2#30/R1#182/F#43");
F(6330434766812878755,"N201$F33:SF#19/S2#130/S1#66/R2#39/R1#48/F#33");
F(5323393252811738347,"N97$F5:SF#9/S2#43/S1#89/R2#70/R1#60/F#5");
F(6257537510040349995,"N174$F37:SF#11/S2#15/S1#205/R2#81/R1#141/F#37");
F(401237009749669099,"N181$F13:SF#92/S2#46/S1#145/R2#19/R1#7/F#13");
F(6116260544972916203,"N69$F39:SF#74/S2#35/S1#40/R2#11/R1#37/F#39");
F(5072576779023488367,"N577$F40:SF#340/S2#317/S1#324/R2#38/R1#338/F#40");
F(1549553158746700844,"N578$F9:SF#8/S2#140/S1#199/R2#125/R1#210/F#9");
F(3644855802223884011,"N117$F9:SF#27/S2#8/S1#68/R2#49/R1#25/F#9");
F(1465676506509553288,"N102$F16:SF#16/S2#36/S1#106/R2#50/R1#17/F#16");
F(2407000567012073774,"N134$F10:SF#106/S2#95/S1#178/R2#40/R1#76/F#10");
F(874886381958675716,"N622$F33:SF#257/S2#125/S1#381/R2#48/R1#333/F#33");
F(1415579543001631764,"N108$F32:SF#3/S2#16/S1#92/R2#8/R1#38/F#32");
F(7196258279025046435,"N93$F46:SF#70/S2#79/S1#1/R2#29/R1#20/F#46");
F(3557871878274769181,"N16$F17:SF#25/S2#88/S1#79/R2#23/R1#5/F#17");
F(1161565799937560,"N677$F21:SF#234/S2#142/S1#79/R2#30/R1#17/F#21");
F(1104603398613259495,"N122$F32:SF#122/S2#99/S1#199/R2#27/R1#119/F#32");
F(5372300195449422691,"N557$F9:SF#11/S2#136/S1#326/R2#226/R1#292/F#9");
F(5967673921075113447,"N211$F27:SF#98/S2#38/S1#171/R2#50/R1#198/F#27");
F(2596364330186337771,"N281$F18:SF#106/S2#150/S1#272/R2#42/R1#236/F#18");
F(1788278053443476975,"N284$F33:SF#183/S2#13/S1#289/R2#34/R1#62/F#33");
F(4196867202669814549,"N264$F35:SF#77/S2#38/S1#67/R2#12/R1#67/F#35");
F(5124079355045356327,"N196$F13:SF#127/S2#72/S1#55/R2#11/R1#103/F#13");
F(6606513701349886951,"N294$F24:SF#59/S2#50/S1#200/R2#28/R1#221/F#24");
F(6551457060938665763,"N119$F16:SF#9/S2#16/S1#33/R2#51/R1#23/F#16");
F(5199720578931518703,"N666$F40:SF#137/S2#114/S1#258/R2#49/R1#222/F#40");
F(1626148771465479945,"N167$F12:SF#131/S2#14/S1#49/R2#33/R1#35/F#12");
F(175372961370866723,"N140$F34:SF#151/S2#188/S1#104/R2#39/R1#46/F#34");
F(5083859236499442328,"N590$F24:SF#9/S2#133/S1#258/R2#61/R1#172/F#24");
F(988278487231065507,"N593$F14:SF#267/S2#80/S1#127/R2#45/R1#248/F#14");
F(6810083670808935919,"N20$F21:SF#83/S2#94/S1#61/R2#29/R1#30/F#21");
F(6638933914033652835,"N645$F30:SF#2/S2#181/S1#289/R2#294/R1#262/F#30");
F(1841819592459844771,"N595$F19:SF#2/S2#117/S1#190/R2#48/R1#302/F#19");
F(1835840043914038701,"N242$F49:SF#297/S2#59/S1#242/R2#18/R1#64/F#49");
F(7955723963704353515,"N164$F49:SF#164/S2#28/S1#91/R2#20/R1#15/F#49");
F(2083802289053325868,"N182$F14:SF#121/S2#219/S1#213/R2#36/R1#173/F#14");
F(8095522898304508579,"N4$F5:SF#59/S2#89/S1#18/R2#22/R1#53/F#5");
F(8497780555078068195,"N216$F9:SF#224/S2#263/S1#5/R2#24/R1#32/F#9");
F(1065188874236338442,"N146$F47:SF#31/S2#21/S1#70/R2#8/R1#26/F#47");
F(6272244762789283235,"N121$F20:SF#255/S2#264/S1#49/R2#5/R1#34/F#20");
F(372198636585041003,"N115$F5:SF#7/S2#70/S1#160/R2#128/R1#30/F#5");
F(4587033183079246371,"N26$F27:SF#48/S2#19/S1#25/R2#7/R1#32/F#27");
F(3080882278942316195,"N88$F30:SF#148/S2#174/S1#7/R2#21/R1#23/F#30");
F(4378989952812387759,"N92$F45:SF#119/S2#106/S1#60/R2#31/R1#14/F#45");
F(3132617844691924323,"N107$F26:SF#92/S2#168/S1#66/R2#23/R1#98/F#26");
F(4972926215010086060,"N184$F18:SF#6/S2#47/S1#296/R2#76/R1#187/F#18");
F(679327426830416739,"N283$F32:SF#229/S2#70/S1#116/R2#28/R1#187/F#32");
F(975639713069800227,"N563$F12:SF#5/S2#131/S1#65/R2#350/R1#191/F#12");
F(7798652509375641887,"N15$F16:SF#94/S2#10/S1#36/R2#25/R1#7/F#16");
F(6687303010888606755,"N289$F22:SF#256/S2#73/S1#117/R2#42/R1#6/F#22");
F(7205853116529579918,"N341$F23:SF#135/S2#52/S1#23/R2#48/R1#224/F#23");
F(1442599102020786219,"N514$F18:SF#299/S2#138/S1#314/R2#21/R1#227/F#18");
F(3259036114112493701,"N118$F10:SF#139/S2#7/S1#83/R2#25/R1#40/F#10");
F(1525171858168097431,"N635$F27:SF#123/S2#208/S1#51/R2#47/R1#238/F#27");
F(1739243855041281053,"N644$F6:SF#233/S2#160/S1#227/R2#32/R1#335/F#6");
F(5493057194128269482,"N665$F28:SF#274/S2#25/S1#32/R2#40/R1#129/F#28");
F(403681667224249571,"N648$F31:SF#4/S2#30/S1#347/R2#292/R1#68/F#31");
F(5932481947486664679,"N106$F22:SF#53/S2#48/S1#40/R2#3/R1#81/F#22");
F(1196967263117141424,"N667$F48:SF#911/S2#28/S1#679/R2#3/R1#560/F#48");
F(857120802588223983,"N258$F24:SF#208/S2#134/S1#220/R2#18/R1#155/F#24");
F(2698601731116125935,"N160$F38:SF#7/S2#139/S1#22/R2#121/R1#68/F#38");
F(391017243517601947,"N659$F23:SF#2/S2#319/S1#113/R2#215/R1#263/F#23");
F(6869965912113363431,"N188$F35:SF#163/S2#171/S1#191/R2#35/R1#132/F#35");
F(7187037615913650476,"N103$F17:SF#48/S2#92/S1#99/R2#29/R1#44/F#17");
F(5416428745411347951,"N200$F30:SF#3/S2#159/S1#255/R2#126/R1#185/F#30");
F(2863514986226403853,"N268$F6:SF#284/S2#89/S1#71/R2#7/R1#200/F#6");
F(1892202801374451619,"N661$F29:SF#9/S2#138/S1#109/R2#102/R1#171/F#29");
F(7706049887401823983,"N298$F28:SF#125/S2#14/S1#101/R2#48/R1#122/F#28");
F(8479572952228592527,"N413$F22:SF#302/S2#324/S1#304/R2#12/R1#283/F#22");
F(769948089825383437,"N220$F20:SF#65/S2#115/S1#241/R2#49/R1#195/F#20");
F(1442340947222412550,"N212$F2:SF#83/S2#88/S1#36/R2#2/R1#2/F#2");
F(7204713094437299563,"N11$F12:SF#68/S2#74/S1#62/R2#5/R1#52/F#12");
F(5674389205962072379,"N173$F34:SF#67/S2#149/S1#52/R2#29/R1#146/F#34");
F(4395864977135573255,"N533$F17:SF#194/S2#223/S1#215/R2#45/R1#296/F#17");
F(4239888391764984547,"N613$F19:SF#5/S2#73/S1#304/R2#332/R1#218/F#19");
F(6805323926476904675,"N526$F3:SF#5/S2#110/S1#233/R2#98/R1#104/F#3");
F(81967127988942393,"N632$F19:SF#39/S2#237/S1#60/R2#47/R1#322/F#19");
F(529563851414254615,"N31$F32:SF#89/S2#53/S1#4/R2#4/R1#41/F#32");
F(3950255471888966887,"N326$F8:SF#2/S2#193/S1#14/R2#126/R1#246/F#8");
F(3615309303764057519,"N430$F28:SF#281/S2#156/S1#343/R2#29/R1#302/F#28");
F(4639948695144982511,"N457$F12:SF#10/S2#230/S1#14/R2#258/R1#223/F#12");
F(7044812350640912611,"N87$F28:SF#67/S2#114/S1#77/R2#37/R1#87/F#28");
F(1446790399777246766,"N422$F1:SF#192/S2#95/S1#32/R2#2/R1#1/F#1");
F(322975241699092373,"N626$F42:SF#332/S2#270/S1#386/R2#41/R1#368/F#42");
F(4703757723595722339,"N27$F28:SF#17/S2#95/S1#91/R2#24/R1#27/F#28");
F(7185009795583997859,"N30$F31:SF#19/S2#133/S1#10/R2#9/R1#16/F#31");
F(7029343142494293731,"N124$F38:SF#110/S2#15/S1#87/R2#30/R1#28/F#38");
F(1791724424745411431,"N89$F31:SF#35/S2#41/S1#179/R2#34/R1#84/F#31");
F(3917093965541635484,"N153$F22:SF#183/S2#14/S1#9/R2#6/R1#119/F#22");
F(614497379108745327,"N515$F29:SF#315/S2#330/S1#258/R2#47/R1#285/F#29");
F(6275905591557114659,"N528$F9:SF#11/S2#21/S1#167/R2#209/R1#234/F#9");
F(379445299259057078,"N412$F13:SF#5/S2#43/S1#245/R2#324/R1#200/F#13");
F(9173956038440063851,"N344$F32:SF#215/S2#127/S1#339/R2#43/R1#267/F#32");
F(2451990069374772071,"N35$F36:SF#22/S2#63/S1#118/R2#35/R1#46/F#36");
F(581555031220108440,"N663$F15:SF#243/S2#211/S1#285/R2#26/R1#370/F#15");
F(573157197207924362,"N1$F2:SF#57/S2#17/S1#56/R2#40/R1#51/F#2");
F(855258454431697445,"N591$F43:SF#292/S2#188/S1#284/R2#50/R1#288/F#43");
F(1656321102598848526,"N282$F21:SF#186/S2#121/S1#233/R2#15/R1#227/F#21");
F(8229902752620168090,"N41$F42:SF#64/S2#84/S1#9/R2#33/R1#24/F#42");
F(4122575680812626723,"N221$F27:SF#1/S2#10/S1#75/R2#80/R1#150/F#27");
F(336988981416577899,"N567$F45:SF#168/S2#206/S1#197/R2#46/R1#333/F#45");
F(6409578876810498232,"N633$F30:SF#42/S2#128/S1#367/R2#48/R1#194/F#30");
F(2827715874205037803,"N511$F13:SF#8/S2#214/S1#117/R2#187/R1#162/F#13");
F(3034951877144875747,"N260$F15:SF#1/S2#263/S1#253/R2#94/R1#170/F#15");
F(7329896532083962653,"N598$F17:SF#222/S2#302/S1#327/R2#29/R1#332/F#17");
F(5384180009974329140,"N651$F39:SF#488/S2#239/S1#401/R2#38/R1#401/F#39");
F(4099468356300909207,"N286$F4:SF#240/S2#107/S1#299/R2#4/R1#3/F#4");
F(5388310489043243327,"N215$F8:SF#43/S2#135/S1#70/R2#50/R1#152/F#8");
F(8107726515129228651,"N407$F32:SF#29/S2#192/S1#50/R2#29/R1#274/F#32");
F(1017618559803533070,"N142$F39:SF#52/S2#69/S1#29/R2#46/R1#90/F#39");
F(8972924029976009059,"N230$F26:SF#10/S2#216/S1#101/R2#10/R1#141/F#26");
F(6568525999568143471,"N649$F50:SF#415/S2#5/S1#164/R2#39/R1#297/F#50");
F(3136811545677203483,"N627$F27:SF#19/S2#96/S1#120/R2#49/R1#68/F#27");
F(9194224775481801392,"N437$F49:SF#315/S2#298/S1#507/R2#36/R1#460/F#49");
F(3720858777336552687,"N209$F20:SF#176/S2#242/S1#67/R2#30/R1#51/F#20");
F(8975394452421243193,"N560$F40:SF#350/S2#201/S1#271/R2#46/R1#342/F#40");
F(7506443000030634015,"N144$F42:SF#57/S2#9/S1#34/R2#23/R1#12/F#42");
F(4928413672102718359,"N674$F41:SF#180/S2#106/S1#251/R2#36/R1#347/F#41");
F(1481749509839010996,"N285$F1:SF#78/S2#2/S1#30/R2#1/R1#2/F#1");
F(6459120176673879982,"N133$F7:SF#115/S2#33/S1#126/R2#20/R1#69/F#7");
F(6322596553054972139,"N85$F22:SF#5/S2#130/S1#118/R2#28/R1#15/F#22");
F(7534867583632942243,"N678$F23:SF#10/S2#177/S1#395/R2#356/R1#189/F#23");
F(4685226777233287019,"N612$F18:SF#9/S2#386/S1#31/R2#150/R1#181/F#18");
F(6951635316424727407,"N652$F40:SF#339/S2#285/S1#314/R2#28/R1#361/F#40");
F(3380837972584313879,"N580$F5:SF#7/S2#122/S1#270/R2#241/R1#96/F#5");
F(2080165486753686755,"N101$F13:SF#159/S2#54/S1#25/R2#19/R1#59/F#13");
F(6811746291585657403,"N676$F14:SF#364/S2#1/S1#335/R2#10/R1#162/F#14");
F(5150444225711068395,"N340$F15:SF#4/S2#8/S1#25/R2#41/R1#256/F#15");
F(8042634533317197419,"N488$F30:SF#9/S2#298/S1#331/R2#262/R1#295/F#30");
F(8389673845485945583,"N58$F17:SF#75/S2#22/S1#82/R2#19/R1#93/F#17");
F(6961208562561270243,"N70$F40:SF#101/S2#127/S1#70/R2#5/R1#49/F#40");
F(5027768565849330667,"N25$F26:SF#60/S2#16/S1#156/R2#26/R1#10/F#26");
F(4733887658236930852,"N675$F50:SF#284/S2#245/S1#373/R2#31/R1#413/F#50");
F(8378158225129936676,"N406$F4:SF#180/S2#203/S1#186/R2#46/R1#22/F#4");
F(5643143744832554731,"N60$F20:SF#47/S2#33/S1#14/R2#38/R1#16/F#20");
F(8797569001878062567,"N176$F43:SF#161/S2#24/S1#174/R2#38/R1#139/F#43");
F(3243241556377367535,"N110$F36:SF#13/S2#109/S1#54/R2#42/R1#33/F#36");
F(6733166273178046254,"N607$F48:SF#326/S2#242/S1#412/R2#50/R1#363/F#48");
F(1709470424525514991,"N265$F44:SF#313/S2#151/S1#179/R2#36/R1#9/F#44");
F(4076689014134152043,"N75$F50:SF#53/S2#38/S1#16/R2#30/R1#1/F#50");
F(5370118480291960815,"N66$F35:SF#46/S2#6/S1#55/R2#28/R1#45/F#35");
F(2695222379472423279,"N143$F40:SF#199/S2#62/S1#48/R2#50/R1#80/F#40");
F(6867797898913013603,"N194$F10:SF#12/S2#248/S1#182/R2#192/R1#88/F#10");
F(7274476286527692519,"N71$F41:SF#107/S2#45/S1#76/R2#34/R1#4/F#41");
F(1482922372492652168,"N198$F19:SF#65/S2#169/S1#25/R2#23/R1#28/F#19");
F(6346201887540007915,"N271$F17:SF#175/S2#229/S1#248/R2#25/R1#158/F#17");
F(2430658437119943143,"N186$F21:SF#63/S2#108/S1#20/R2#11/R1#28/F#21");
F(3565736470224268135,"N597$F50:SF#422/S2#295/S1#424/R2#50/R1#378/F#50");
F(2596954549085873557,"N217$F14:SF#225/S2#201/S1#66/R2#17/R1#107/F#14");
F(5843714397195406627,"N24$F25:SF#8/S2#61/S1#14/R2#20/R1#2/F#25");
F(8112515319110397539,"N99$F7:SF#56/S2#13/S1#19/R2#47/R1#23/F#7");
F(8696242228140265002,"N546$F14:SF#210/S2#240/S1#363/R2#24/R1#181/F#14");
F(5954717611911051899,"N415$F30:SF#468/S2#309/S1#417/R2#47/R1#333/F#30");
F(5058153380114933867,"N609$F44:SF#256/S2#169/S1#539/R2#48/R1#375/F#44");
F(2094818184214938087,"N620$F14:SF#12/S2#165/S1#177/R2#303/R1#212/F#14");
F(1530957930726434863,"N170$F23:SF#60/S2#117/S1#112/R2#43/R1#94/F#23");
F(3354109378108004111,"N293$F16:SF#334/S2#196/S1#135/R2#23/R1#49/F#16");
F(3890360104986937959,"N662$F40:SF#329/S2#263/S1#279/R2#36/R1#403/F#40");
F(1687803814111963363,"N537$F7:SF#5/S2#162/S1#59/R2#100/R1#246/F#7");
F(7681808709729330196,"N624$F26:SF#2/S2#154/S1#100/R2#305/R1#161/F#26");
F(1015678042086077155,"N147$F48:SF#45/S2#187/S1#157/R2#9/R1#137/F#48");
F(385063493999682311,"N658$F20:SF#8/S2#130/S1#246/R2#218/R1#116/F#20");
F(4952067386760509316,"N520$F25:SF#279/S2#335/S1#314/R2#15/R1#298/F#25");
F(927035827055558891,"N40$F41:SF#76/S2#113/S1#19/R2#1/R1#68/F#41");
F(8310397111282307811,"N295$F46:SF#134/S2#158/S1#184/R2#48/R1#53/F#46");
F(2035638396465936372,"N492$F45:SF#483/S2#353/S1#681/R2#17/R1#337/F#45");
F(7704335696184111459,"N55$F13:SF#58/S2#36/S1#17/R2#22/R1#25/F#13");
F(3032396569303280296,"N579$F36:SF#246/S2#317/S1#341/R2#48/R1#320/F#36");
F(3216202014514750231,"N256$F22:SF#93/S2#201/S1#150/R2#14/R1#128/F#22");
F(3462508385390720491,"N56$F15:SF#22/S2#133/S1#28/R2#41/R1#69/F#15");
F(5543787139900770221,"N248$F14:SF#35/S2#335/S1#10/R2#23/R1#198/F#14");
F(1371072372830836379,"N647$F21:SF#3/S2#192/S1#111/R2#164/R1#61/F#21");
F(187217324627795099,"N558$F20:SF#288/S2#236/S1#232/R2#34/R1#312/F#20");
F(4798381934677394159,"N679$F33:SF#239/S2#174/S1#372/R2#45/R1#217/F#33");
F(4353337243521982315,"N459$F27:SF#233/S2#315/S1#294/R2#49/R1#314/F#27");
F(8094963727413221168,"N476$F44:SF#802/S2#312/S1#842/R2#28/R1#418/F#44");
F(2724180267427776783,"N615$F50:SF#650/S2#274/S1#352/R2#25/R1#395/F#50");
F(2875906683250424039,"N12$F13:SF#5/S2#37/S1#43/R2#14/R1#20/F#13");
F(5700259379355873167,"N241$F46:SF#385/S2#261/S1#183/R2#43/R1#19/F#46");
F(1297153791816326755,"N38$F39:SF#47/S2#12/S1#74/R2#18/R1#9/F#39");
F(8955810916288373915,"N229$F25:SF#5/S2#72/S1#22/R2#177/R1#79/F#25");
F(4508805566779173731,"N210$F25:SF#254/S2#22/S1#180/R2#41/R1#85/F#25");
F(8061478326186105583,"N29$F30:SF#36/S2#28/S1#65/R2#2/R1#4/F#30");
F(2492488691074295535,"N5$F6:SF#14/S2#69/S1#27/R2#16/R1#12/F#6");
F(1009967290662264427,"N654$F50:SF#189/S2#270/S1#408/R2#33/R1#383/F#50");
F(2345812701602928526,"N487$F26:SF#322/S2#304/S1#326/R2#23/R1#304/F#26");
F(8036417388212727907,"N660$F26:SF#35/S2#113/S1#37/R2#48/R1#173/F#26");
F(2712093147305155747,"N234$F50:SF#142/S2#193/S1#133/R2#16/R1#30/F#50");
F(1214003276011231331,"N163$F43:SF#138/S2#258/S1#108/R2#25/R1#123/F#43");
F(872985459517360949,"N420$F49:SF#361/S2#321/S1#501/R2#44/R1#362/F#49");
F(384003089336920171,"N462$F35:SF#327/S2#347/S1#313/R2#48/R1#302/F#35");
F(4080283717147517849,"N261$F16:SF#290/S2#230/S1#222/R2#49/R1#163/F#16");
F(1804082188631958808,"N137$F17:SF#102/S2#6/S1#45/R2#35/R1#62/F#17");
F(8759852008168624231,"N49$F50:SF#28/S2#70/S1#26/R2#46/R1#44/F#50");
F(1846309142918153583,"N606$F47:SF#286/S2#325/S1#324/R2#46/R1#382/F#47");
F(4475770895066425699,"N105$F19:SF#22/S2#111/S1#71/R2#27/R1#8/F#19");
F(7816566907348008555,"N576$F39:SF#211/S2#334/S1#200/R2#37/R1#319/F#39");
F(2163815241389909141,"N438$F31:SF#340/S2#142/S1#314/R2#13/R1#338/F#31");
F(7627201730592339047,"N623$F45:SF#395/S2#213/S1#335/R2#31/R1#399/F#45");
F(3856498724676579375,"N244$F6:SF#191/S2#96/S1#282/R2#5/R1#67/F#6");
F(4289483586216341611,"N672$F19:SF#389/S2#152/S1#361/R2#30/R1#363/F#19");
F(8894176577457434211,"N203$F44:SF#102/S2#151/S1#209/R2#35/R1#81/F#44");
F(8712038765735731768,"N257$F23:SF#35/S2#252/S1#175/R2#7/R1#116/F#23");
F(6681229673293243191,"N218$F16:SF#9/S2#68/S1#29/R2#29/R1#135/F#16");
F(1952344184579367861,"N671$F45:SF#75/S2#258/S1#163/R2#37/R1#271/F#45");
F(219965999438586791,"N527$F8:SF#7/S2#225/S1#140/R2#333/R1#87/F#8");
F(311918654351762573,"N314$F31:SF#333/S2#431/S1#374/R2#18/R1#300/F#31");
F(2201765313073593763,"N279$F13:SF#232/S2#4/S1#53/R2#21/R1#88/F#13");
F(9064430923322257263,"N585$F43:SF#360/S2#223/S1#255/R2#37/R1#345/F#43");
F(4222533304344665531,"N359$F12:SF#185/S2#300/S1#56/R2#4/R1#3/F#12");
F(356957974934093807,"N339$F6:SF#202/S2#228/S1#264/R2#50/R1#176/F#6");
F(2117916557029111579,"N208$F15:SF#53/S2#42/S1#184/R2#6/R1#36/F#15");
F(6669119162370953191,"N619$F8:SF#7/S2#124/S1#373/R2#317/R1#232/F#8");
F(2672047168486210216,"N480$F34:SF#313/S2#311/S1#80/R2#21/R1#310/F#34");
F(4610078825725245611,"N6$F7:SF#32/S2#86/S1#84/R2#8/R1#14/F#7");
F(2246738856880917512,"N541$F42:SF#465/S2#341/S1#657/R2#42/R1#382/F#42");
F(8109390820310204299,"N460$F28:SF#227/S2#222/S1#256/R2#45/R1#308/F#28");
F(1597686095905714221,"N288$F14:SF#244/S2#409/S1#148/R2#12/R1#268/F#14");
F(3431807419682531059,"N368$F44:SF#429/S2#330/S1#397/R2#6/R1#350/F#44");
F(735802078288879501,"N584$F42:SF#419/S2#230/S1#266/R2#42/R1#323/F#42");
F(2457306835223527180,"N373$F1:SF#4/S2#90/S1#192/R2#61/R1#72/F#1");
F(7685740600946072611,"N501$F8:SF#8/S2#134/S1#278/R2#346/R1#145/F#8");
F(7513807846226738031,"N358$F1:SF#6/S2#130/S1#92/R2#323/R1#187/F#1");
F(3607198278715141948,"N51$F2:SF#12/S2#129/S1#13/R2#73/R1#10/F#2");
F(1256265171134476602,"N44$F45:SF#27/S2#18/S1#96/R2#37/R1#34/F#45");
F(5207395900535491695,"N629$F49:SF#381/S2#274/S1#359/R2#40/R1#402/F#49");
F(6106346384873572327,"N634$F31:SF#11/S2#362/S1#14/R2#217/R1#118/F#31");
F(7758857776205942844,"N617$F34:SF#315/S2#292/S1#319/R2#16/R1#340/F#34");
F(2156137897537390951,"N616$F25:SF#6/S2#243/S1#200/R2#73/R1#316/F#25");
F(1471376398923602023,"N586$F7:SF#7/S2#2/S1#82/R2#265/R1#15/F#7");
F(6946835694292893795,"N562$F9:SF#8/S2#8/S1#55/R2#302/R1#133/F#9");
F(1975164664220494823,"N155$F25:SF#6/S2#49/S1#133/R2#125/R1#160/F#25");
F(5209046232807139947,"N568$F49:SF#309/S2#215/S1#414/R2#47/R1#327/F#49");
F(4590089189191872422,"N342$F30:SF#255/S2#331/S1#326/R2#38/R1#241/F#30");
F(6010393559808505455,"N618$F48:SF#553/S2#303/S1#387/R2#47/R1#387/F#48");
F(6338857828186089254,"N561$F42:SF#270/S2#216/S1#323/R2#45/R1#344/F#42");
F(8989265418721363051,"N583$F33:SF#381/S2#336/S1#354/R2#48/R1#346/F#33");
F(1260202860910304623,"N48$F49:SF#52/S2#67/S1#67/R2#43/R1#45/F#49");
F(3859347966041461053,"N343$F31:SF#177/S2#221/S1#138/R2#16/R1#275/F#31");
F(27437239166975471,"N424$F9:SF#2/S2#254/S1#206/R2#52/R1#171/F#9");
F(2806335711638270747,"N130$F4:SF#2/S2#269/S1#418/R2#308/R1#398/F#4");
F(2238586480039568163,"N638$F23:SF#3/S2#76/S1#311/R2#192/R1#42/F#23");
F(1870455712462360204,"N592$F45:SF#327/S2#324/S1#288/R2#47/R1#340/F#45");
F(1366093217369912105,"N531$F42:SF#294/S2#253/S1#305/R2#49/R1#321/F#42");
F(7781986490134432309,"N451$F30:SF#346/S2#337/S1#320/R2#50/R1#309/F#30");
F(6320281155402432231,"N39$F40:SF#42/S2#15/S1#29/R2#36/R1#49/F#40");
F(8639378492559988843,"N588$F46:SF#282/S2#169/S1#297/R2#47/R1#362/F#46");
F(8291215798419656880,"N554$F45:SF#626/S2#37/S1#624/R2#31/R1#546/F#45");
F(3929431968438094261,"N656$F23:SF#301/S2#28/S1#243/R2#48/R1#84/F#23");
F(8118396461129075848,"N138$F18:SF#22/S2#55/S1#96/R2#10/R1#5/F#18");
F(513210598763819751,"N423$F5:SF#1/S2#92/S1#218/R2#129/R1#18/F#5");
F(4934337376489655082,"N315$F33:SF#365/S2#332/S1#445/R2#43/R1#389/F#33");
F(7452452612199097207,"N42$F43:SF#163/S2#167/S1#107/R2#38/R1#94/F#43");
F(5135871939726750629,"N389$F50:SF#473/S2#267/S1#442/R2#12/R1#456/F#50");
F(6565426208801497319,"N104$F18:SF#126/S2#18/S1#32/R2#36/R1#77/F#18");
F(8334767463473284643,"N161$F39:SF#152/S2#56/S1#24/R2#26/R1#88/F#39");
F(3910293439667578227,"N441$F36:SF#278/S2#294/S1#502/R2#41/R1#550/F#36");
F(1571894100801563495,"N650$F38:SF#499/S2#243/S1#251/R2#43/R1#368/F#38");
F(1551812831562762855,"N628$F33:SF#4/S2#38/S1#61/R2#355/R1#104/F#33");
F(3332731224200448806,"N601$F28:SF#411/S2#317/S1#364/R2#14/R1#320/F#28");
F(7477751071249226351,"N461$F31:SF#330/S2#300/S1#243/R2#43/R1#303/F#31");
F(1745184091361722748,"N539$F28:SF#356/S2#348/S1#373/R2#24/R1#357/F#28");
F(2967325968447649724,"N538$F24:SF#191/S2#97/S1#334/R2#3/R1#290/F#24");
F(8302123406509165555,"N309$F49:SF#464/S2#438/S1#448/R2#14/R1#294/F#49");
F(5841475589646325006,"N553$F42:SF#665/S2#324/S1#458/R2#17/R1#646/F#42");
F(8838932964196447672,"N383$F37:SF#292/S2#234/S1#312/R2#42/R1#282/F#37");
F(7096941182177455743,"N10$F11:SF#151/S2#139/S1#22/R2#42/R1#35/F#11");
F(3052959154564713776,"N354$F42:SF#456/S2#290/S1#479/R2#25/R1#337/F#42");
F(2683039098506595951,"N610$F45:SF#262/S2#296/S1#188/R2#40/R1#379/F#45");
F(648240668745204852,"N392$F33:SF#487/S2#337/S1#422/R2#34/R1#333/F#33");
F(2007055947179970587,"N267$F3:SF#1/S2#315/S1#361/R2#313/R1#325/F#3");
F(9042132699301350383,"N172$F33:SF#93/S2#113/S1#32/R2#39/R1#133/F#33");
F(841654273224289062,"N495$F14:SF#286/S2#320/S1#271/R2#47/R1#272/F#14");
F(8930158886545271831,"N329$F28:SF#348/S2#331/S1#442/R2#22/R1#485/F#28");
F(3856242260068100885,"N458$F21:SF#113/S2#232/S1#175/R2#50/R1#202/F#21");
F(4050547537331309875,"N505$F42:SF#459/S2#337/S1#344/R2#14/R1#444/F#42");
F(1545697003443026555,"N443$F40:SF#246/S2#291/S1#569/R2#25/R1#324/F#40");
F(914624147753754091,"N28$F29:SF#91/S2#41/S1#7/R2#15/R1#61/F#29");
F(9170274736300063091,"N386$F46:SF#618/S2#321/S1#519/R2#37/R1#480/F#46");
F(4594855149972443436,"N486$F50:SF#10/S2#589/S1#316/R2#-1/R1#641/F#50");
F(8343525369566213550,"N479$F26:SF#356/S2#343/S1#476/R2#11/R1#331/F#26");
F(507584218101605161,"N570$F27:SF#292/S2#236/S1#362/R2#32/R1#298/F#27");
F(1728094150915284079,"N3$F4:SF#31/S2#50/S1#63/R2#41/R1#56/F#4");
F(7692222305093629807,"N605$F45:SF#259/S2#322/S1#301/R2#43/R1#372/F#45");
F(7433199582889252531,"N549$F50:SF#444/S2#37/S1#406/R2#46/R1#626/F#50");
F(9026757059443905144,"N351$F21:SF#364/S2#370/S1#328/R2#44/R1#293/F#21");
F(9071788627860350512,"N428$F46:SF#405/S2#324/S1#466/R2#38/R1#485/F#46");
F(7670794721750374644,"N643$F50:SF#545/S2#235/S1#530/R2#7/R1#694/F#50");
F(5645057456388323064,"N535$F47:SF#879/S2#1/S1#888/R2#10/R1#631/F#47");
F(2024484565290917743,"N564$F45:SF#247/S2#191/S1#172/R2#43/R1#334/F#45");
F(12730816631873467,"N641$F14:SF#267/S2#224/S1#436/R2#12/R1#303/F#14");
F(7348848970993974648,"N34$F35:SF#10/S2#110/S1#86/R2#164/R1#47/F#35");
F(1020370448199395568,"N466$F41:SF#525/S2#322/S1#439/R2#19/R1#500/F#41");
F(3481704592336230821,"N403$F48:SF#674/S2#322/S1#418/R2#30/R1#417/F#48");
F(5147129997380779131,"N521$F32:SF#401/S2#332/S1#350/R2#13/R1#336/F#32");
F(5719686582449886315,"N47$F48:SF#13/S2#96/S1#54/R2#51/R1#18/F#48");
F(902540443390933299,"N357$F50:SF#432/S2#339/S1#180/R2#10/R1#381/F#50");
F(2817943741121981087,"N655$F20:SF#3/S2#224/S1#106/R2#246/R1#63/F#20");
F(6287602694285828868,"N180$F9:SF#118/S2#109/S1#315/R2#50/R1#140/F#9");
F(7194847010685220997,"N530$F40:SF#356/S2#106/S1#438/R2#35/R1#362/F#40");
F(3133994034810985976,"N395$F37:SF#534/S2#339/S1#378/R2#45/R1#457/F#37");
F(4703451289137721478,"N45$F46:SF#157/S2#117/S1#127/R2#48/R1#113/F#46");
F(3578967025126425101,"N313$F30:SF#351/S2#345/S1#398/R2#35/R1#287/F#30");
F(7421941540842510591,"N467$F43:SF#670/S2#4/S1#338/R2#13/R1#370/F#43");
F(8003867742286322821,"N305$F44:SF#418/S2#230/S1#359/R2#17/R1#378/F#44");
F(4261930553857362168,"N447$F46:SF#529/S2#334/S1#436/R2#37/R1#523/F#46");
F(2241151698011580387,"N603$F13:SF#10/S2#131/S1#98/R2#174/R1#32/F#13");
F(2864115180925812860,"N9$F10:SF#51/S2#140/S1#115/R2#39/R1#73/F#10");
F(6789192682072646647,"N206$F6:SF#13/S2#276/S1#483/R2#274/R1#318/F#6");
F(492112250470362032,"N374$F36:SF#462/S2#25/S1#351/R2#24/R1#478/F#36");
F(5116665310034078997,"N345$F44:SF#180/S2#54/S1#280/R2#29/R1#314/F#44");
F(580698477515781179,"N277$F1:SF#8/S2#478/S1#435/R2#-1/R1#311/F#1");
F(3451295400298439668,"N129$F2:SF#9/S2#323/S1#225/R2#191/R1#191/F#2");
F(7973434154462479667,"N78$F3:SF#11/S2#247/S1#250/R2#180/R1#202/F#3");
F(2816860473447433372,"N299$F36:SF#116/S2#251/S1#234/R2#15/R1#165/F#36");
F(3126385760968907164,"N614$F49:SF#643/S2#48/S1#776/R2#25/R1#579/F#49");
F(5204018819606729916,"N657$F14:SF#360/S2#259/S1#442/R2#27/R1#360/F#14");
F(8326136979854665613,"N204$F2:SF#7/S2#353/S1#335/R2#230/R1#275/F#2");
F(3474899139848248328,"N522$F41:SF#576/S2#353/S1#597/R2#19/R1#452/F#41");
F(5998019116126251575,"N301$F39:SF#394/S2#434/S1#16/R2#25/R1#282/F#39");
F(1080324596498915635,"N519$F50:SF#627/S2#4/S1#709/R2#22/R1#424/F#50");
F(1195185817891315960,"N371$F48:SF#507/S2#347/S1#387/R2#36/R1#486/F#48");
F(3360268039418683639,"N446$F44:SF#680/S2#17/S1#642/R2#24/R1#498/F#44");
F(2873918216441186160,"N266$F1:SF#3/S2#371/S1#309/R2#-1/R1#344/F#1");
F(6628772621912581751,"N559$F29:SF#376/S2#288/S1#527/R2#8/R1#385/F#29");
F(2409186635803079320,"N287$F11:SF#339/S2#436/S1#450/R2#32/R1#288/F#11");
F(6739667882669201213,"N95$F2:SF#57/S2#17/S1#7/R2#1/R1#1/F#2");
F(7167208250746887039,"N414$F25:SF#328/S2#351/S1#65/R2#46/R1#320/F#25");
F(1448514928040814213,"N472$F23:SF#162/S2#23/S1#330/R2#26/R1#307/F#23");
F(7660728426709387647,"N452$F39:SF#394/S2#228/S1#617/R2#2/R1#616/F#39");
F(2865240085063808149,"N388$F49:SF#93/S2#345/S1#377/R2#21/R1#330/F#49");
F(4990291861070638716,"N571$F42:SF#374/S2#341/S1#359/R2#31/R1#335/F#42");
F(6711902239088508942,"N425$F40:SF#6/S2#459/S1#464/R2#-1/R1#410/F#40");
F(675261777109404925,"N165$F2:SF#7/S2#88/S1#368/R2#348/R1#414/F#2");
F(3905253436590738675,"N445$F43:SF#559/S2#329/S1#493/R2#50/R1#407/F#43");
F(1766671173704512567,"N566$F50:SF#789/S2#322/S1#444/R2#18/R1#388/F#50");
F(2476474931528875928,"N483$F44:SF#407/S2#323/S1#331/R2#32/R1#446/F#44");
F(540564494089652615,"N269$F9:SF#388/S2#5/S1#337/R2#21/R1#289/F#9");
F(5427991110764229509,"N355$F47:SF#447/S2#350/S1#136/R2#18/R1#335/F#47");
F(4781802794541346552,"N454$F43:SF#494/S2#350/S1#368/R2#4/R1#424/F#43");
F(2858661450131587696,"N573$F50:SF#483/S2#301/S1#515/R2#24/R1#580/F#50");
F(5473925679583095935,"N366$F42:SF#399/S2#283/S1#430/R2#18/R1#476/F#42");
F(6470352286253796732,"N453$F42:SF#3/S2#386/S1#717/R2#-1/R1#493/F#42");
F(9011485642297781875,"N456$F50:SF#516/S2#17/S1#595/R2#9/R1#570/F#50");
F(4126471980175602573,"N337$F48:SF#427/S2#340/S1#354/R2#46/R1#395/F#48");
F(912941347042898288,"N114$F4:SF#5/S2#183/S1#296/R2#166/R1#322/F#4");
F(8764963120823038101,"N542$F48:SF#750/S2#355/S1#775/R2#15/R1#498/F#48");
F(4684703525255017335,"N252$F7:SF#3/S2#438/S1#534/R2#-1/R1#335/F#7");
F(5396189777855596284,"N353$F39:SF#68/S2#62/S1#427/R2#24/R1#347/F#39");
F(5876734521657204208,"N224$F4:SF#1/S2#467/S1#470/R2#292/R1#289/F#4");
F(6013269813849511800,"N290$F30:SF#5/S2#458/S1#341/R2#-1/R1#423/F#30");
F(6072937482919679260,"N416$F34:SF#383/S2#292/S1#369/R2#35/R1#334/F#34");
F(3337471663088869680,"N245$F8:SF#2/S2#478/S1#452/R2#-1/R1#427/F#8");
F(8844526531605498876,"N356$F49:SF#554/S2#166/S1#457/R2#31/R1#351/F#49");
F(4575429820171750790,"N604$F38:SF#391/S2#243/S1#346/R2#11/R1#383/F#38");
F(8132408800586172683,"N303$F42:SF#447/S2#306/S1#53/R2#27/R1#348/F#42");
F(3321518474421410683,"N393$F34:SF#584/S2#335/S1#712/R2#33/R1#396/F#34");
F(3225478448182205048,"N346$F45:SF#516/S2#248/S1#463/R2#22/R1#381/F#45");
F(6299527735806534935,"N421$F50:SF#424/S2#82/S1#343/R2#22/R1#557/F#50");
F(8200801712505823612,"N556$F49:SF#552/S2#309/S1#474/R2#42/R1#434/F#49");
F(3440286599277802532,"N259$F8:SF#3/S2#407/S1#453/R2#-1/R1#370/F#8");
F(6310208649041834855,"N508$F37:SF#324/S2#263/S1#320/R2#49/R1#314/F#37");
F(311077013050775924,"N540$F41:SF#399/S2#350/S1#798/R2#45/R1#533/F#41");
F(762238331631897651,"N375$F37:SF#611/S2#322/S1#688/R2#25/R1#415/F#37");
F(163551037848377599,"N148$F1:SF#3/S2#337/S1#339/R2#220/R1#242/F#1");
F(2818739780980247964,"N547$F41:SF#497/S2#333/S1#538/R2#22/R1#364/F#41");
F(5071905599466011439,"N525$F30:SF#380/S2#337/S1#347/R2#40/R1#335/F#30");
F(7813215501523091763,"N506$F46:SF#951/S2#332/S1#400/R2#34/R1#396/F#46");
F(3922190413334010633,"N302$F40:SF#431/S2#459/S1#367/R2#29/R1#138/F#40");
F(5067815484162322183,"N463$F36:SF#431/S2#359/S1#413/R2#24/R1#356/F#36");
F(7959929110048482224,"N81$F6:SF#10/S2#172/S1#263/R2#164/R1#176/F#6");
F(2683357484061625013,"N76$F1:SF#6/S2#98/S1#283/R2#186/R1#97/F#1");
F(1808216756695154493,"N631$F46:SF#454/S2#258/S1#480/R2#14/R1#469/F#46");
F(2700502852397650352,"N387$F47:SF#33/S2#343/S1#414/R2#32/R1#445/F#47");
F(1969248376738817049,"N582$F50:SF#528/S2#318/S1#504/R2#11/R1#465/F#50");
F(3517627638264957573,"N489$F39:SF#773/S2#352/S1#256/R2#49/R1#364/F#39");
F(6905158780789667703,"N312$F22:SF#346/S2#334/S1#347/R2#40/R1#301/F#22");
F(6298106993507829260,"N518$F45:SF#402/S2#340/S1#397/R2#37/R1#637/F#45");
F(8887945757574519548,"N384$F42:SF#541/S2#308/S1#537/R2#36/R1#490/F#42");
F(1851639299967232141,"N434$F43:SF#548/S2#193/S1#570/R2#37/R1#496/F#43");
F(6584008883020454128,"N493$F47:SF#456/S2#357/S1#722/R2#13/R1#617/F#47");
F(4963007467454531231,"N433$F42:SF#431/S2#334/S1#467/R2#31/R1#426/F#42");
F(1163156327707190136,"N391$F31:SF#478/S2#343/S1#456/R2#44/R1#557/F#31");
F(745127451131644317,"N552$F40:SF#444/S2#351/S1#436/R2#40/R1#416/F#40");
F(2875381023441751280,"N398$F41:SF#477/S2#193/S1#607/R2#40/R1#414/F#41");
F(5272382251969901460,"N347$F46:SF#412/S2#208/S1#490/R2#23/R1#408/F#46");
F(1072556705806445487,"N360$F30:SF#369/S2#358/S1#373/R2#26/R1#336/F#30");
F(6735985781261691248,"N523$F46:SF#434/S2#347/S1#452/R2#28/R1#547/F#46");
F(8822663378692485895,"N243$F3:SF#4/S2#128/S1#406/R2#323/R1#318/F#3");
F(8092185352509011444,"N348$F47:SF#385/S2#300/S1#596/R2#14/R1#395/F#47");
F(5283355085903773560,"N427$F44:SF#495/S2#351/S1#373/R2#34/R1#477/F#44");
F(6062426776212556344,"N330$F32:SF#711/S2#287/S1#352/R2#30/R1#352/F#32");
F(7598728916031463100,"N363$F38:SF#445/S2#78/S1#352/R2#46/R1#393/F#38");
F(3199259579860846205,"N439$F32:SF#8/S2#687/S1#453/R2#-1/R1#604/F#32");
F(4224163954487266223,"N300$F38:SF#183/S2#476/S1#455/R2#24/R1#283/F#38");
F(4130390209805353354,"N77$F2:SF#3/S2#243/S1#184/R2#148/R1#95/F#2");
F(8537215235282837499,"N436$F46:SF#465/S2#354/S1#537/R2#19/R1#312/F#46");
F(228977940916630768,"N396$F39:SF#378/S2#95/S1#364/R2#36/R1#504/F#39");
F(3363735130013983371,"N369$F46:SF#533/S2#324/S1#684/R2#24/R1#425/F#46");
F(9054843328079863160,"N214$F6:SF#2/S2#493/S1#441/R2#-1/R1#375/F#6");
F(1281856952763836407,"N432$F40:SF#674/S2#163/S1#445/R2#41/R1#453/F#40");
F(289728810112460329,"N338$F49:SF#300/S2#238/S1#323/R2#24/R1#464/F#49");
F(5309561550433508411,"N361$F33:SF#341/S2#351/S1#520/R2#42/R1#334/F#33");
F(2353983593478564724,"N513$F39:SF#387/S2#295/S1#400/R2#29/R1#475/F#39");
F(5183715873614691455,"N411$F50:SF#466/S2#341/S1#384/R2#24/R1#436/F#50");
F(812135624005737723,"N385$F44:SF#493/S2#56/S1#367/R2#35/R1#380/F#44");
F(6665458275209980052,"N497$F45:SF#653/S2#329/S1#813/R2#18/R1#476/F#45");
F(6066406596221012471,"N477$F47:SF#684/S2#343/S1#756/R2#32/R1#504/F#47");
F(8288659919874694045,"N253$F9:SF#5/S2#6/S1#117/R2#286/R1#321/F#9");
F(2908509475873183283,"N332$F37:SF#382/S2#294/S1#493/R2#28/R1#420/F#37");
F(6061582710817037363,"N399$F42:SF#375/S2#287/S1#389/R2#47/R1#515/F#42");
F(4546506936139338419,"N79$F4:SF#8/S2#138/S1#200/R2#133/R1#208/F#4");
F(1836174799294326131,"N503$F39:SF#666/S2#320/S1#538/R2#36/R1#484/F#39");
F(5651588651696262036,"N455$F48:SF#360/S2#330/S1#396/R2#8/R1#430/F#48");
F(8182596458964078769,"N53$F4:SF#5/S2#161/S1#500/R2#265/R1#151/F#4");
F(154657362085101109,"N517$F43:SF#1/S2#540/S1#546/R2#-1/R1#684/F#43");
F(1116618375900396955,"N352$F34:SF#217/S2#353/S1#439/R2#16/R1#321/F#34");
F(1860630038501868535,"N409$F48:SF#556/S2#338/S1#345/R2#32/R1#453/F#48");
F(5931300926746878207,"N381$F48:SF#618/S2#293/S1#364/R2#17/R1#374/F#48");
F(2545974154127243654,"N653$F44:SF#481/S2#68/S1#608/R2#8/R1#515/F#44");
F(8455254352489041660,"N367$F43:SF#265/S2#184/S1#227/R2#35/R1#461/F#43");
F(8022440469206817648,"N401$F45:SF#325/S2#317/S1#538/R2#32/R1#426/F#45");
F(5016860431194393997,"N668$F50:SF#423/S2#195/S1#511/R2#4/R1#624/F#50");
F(220997400520848261,"N474$F38:SF#727/S2#357/S1#582/R2#7/R1#392/F#38");
F(7854021041498620539,"N646$F50:SF#563/S2#239/S1#429/R2#7/R1#573/F#50");
F(8129000262129046267,"N484$F46:SF#439/S2#314/S1#583/R2#39/R1#558/F#46");
F(511227222694772361,"N490$F40:SF#431/S2#323/S1#435/R2#33/R1#586/F#40");
F(8190434408480406536,"N335$F42:SF#195/S2#95/S1#374/R2#12/R1#387/F#42");
F(4175219954602374054,"N324$F46:SF#347/S2#290/S1#354/R2#38/R1#436/F#46");
F(8664387654503064951,"N642$F48:SF#524/S2#221/S1#462/R2#8/R1#430/F#48");
F(792100838633141627,"N551$F38:SF#499/S2#217/S1#435/R2#24/R1#403/F#38");
F(9198715782307534709,"N435$F45:SF#557/S2#326/S1#792/R2#49/R1#564/F#45");
F(772783016012181623,"N482$F42:SF#323/S2#285/S1#429/R2#29/R1#433/F#42");
F(644670683625430520,"N464$F39:SF#721/S2#321/S1#537/R2#3/R1#467/F#39");
F(4965288163480580083,"N473$F37:SF#613/S2#223/S1#706/R2#30/R1#451/F#37");
F(315661847148834572,"N379$F42:SF#391/S2#49/S1#464/R2#22/R1#347/F#42");
F(5891329811766277129,"N378$F41:SF#313/S2#254/S1#449/R2#26/R1#344/F#41");
F(4655323296217840244,"N543$F49:SF#606/S2#328/S1#528/R2#30/R1#417/F#49");
F(4029410564858725111,"N494$F49:SF#461/S2#351/S1#503/R2#29/R1#568/F#49");
F(2782166033290776988,"N205$F5:SF#8/S2#382/S1#362/R2#225/R1#87/F#5");
F(1001306046946950399,"N323$F44:SF#467/S2#325/S1#534/R2#11/R1#412/F#44");
F(4646139680800388728,"N418$F42:SF#555/S2#336/S1#563/R2#39/R1#506/F#42");
F(4154339695075067387,"N536$F49:SF#443/S2#316/S1#327/R2#2/R1#502/F#49");
F(3920437585013903103,"N364$F39:SF#389/S2#289/S1#477/R2#10/R1#373/F#39");
F(4946173987978413684,"N52$F3:SF#18/S2#175/S1#225/R2#205/R1#226/F#3");
F(1683280826135353243,"N46$F47:SF#11/S2#180/S1#151/R2#84/R1#166/F#47");
F(5904019310676815115,"N394$F35:SF#570/S2#257/S1#459/R2#18/R1#480/F#35");
F(3529990749597931251,"N485$F48:SF#437/S2#82/S1#341/R2#34/R1#396/F#48");
F(6348720499250912244,"N318$F36:SF#412/S2#58/S1#327/R2#46/R1#341/F#36");
F(7167204240177564415,"N331$F35:SF#294/S2#311/S1#468/R2#41/R1#391/F#35");
F(3672419234057897392,"N316$F34:SF#533/S2#277/S1#471/R2#22/R1#331/F#34");
F(4703470324554144752,"N292$F46:SF#1/S2#446/S1#417/R2#271/R1#271/F#46");
F(7896259150418693424,"N440$F35:SF#371/S2#286/S1#405/R2#35/R1#512/F#35");
F(6602094205947747854,"N630$F50:SF#559/S2#5/S1#809/R2#3/R1#515/F#50");
F(8759923024768479132,"N572$F49:SF#435/S2#327/S1#321/R2#23/R1#374/F#49");
F(4389993234557191999,"N400$F43:SF#350/S2#139/S1#330/R2#25/R1#472/F#43");
F(18593339105695088,"N376$F39:SF#366/S2#314/S1#76/R2#50/R1#445/F#39");
F(702086658064200077,"N498$F46:SF#837/S2#243/S1#851/R2#44/R1#698/F#46");
F(7856065900195296624,"N370$F47:SF#396/S2#272/S1#591/R2#28/R1#409/F#47");
F(1041219017996005640,"N365$F41:SF#401/S2#262/S1#654/R2#43/R1#365/F#41");
F(4159368419632550064,"N499$F47:SF#7/S2#382/S1#518/R2#-1/R1#578/F#47");
F(2160903926791744759,"N408$F47:SF#446/S2#340/S1#777/R2#31/R1#371/F#47");
F(973731831287723391,"N555$F47:SF#691/S2#302/S1#399/R2#44/R1#610/F#47");
F(1062107166238982135,"N113$F3:SF#1/S2#45/S1#265/R2#202/R1#245/F#3");
F(73806746709290291,"N322$F43:SF#417/S2#346/S1#391/R2#19/R1#303/F#43");
F(2466899868603731725,"N349$F48:SF#383/S2#306/S1#471/R2#33/R1#312/F#48");
F(3586393985980523790,"N431$F38:SF#513/S2#322/S1#509/R2#45/R1#346/F#38");
F(6713470137107431804,"N235$F4:SF#6/S2#415/S1#391/R2#-1/R1#417/F#4");
F(7025139898112826141,"N504$F41:SF#449/S2#308/S1#738/R2#43/R1#519/F#41");
F(8574883931448413436,"N207$F7:SF#3/S2#418/S1#408/R2#309/R1#254/F#7");
F(3793226313975467805,"N328$F27:SF#429/S2#371/S1#388/R2#26/R1#295/F#27");
F(318136492065622267,"N213$F3:SF#10/S2#300/S1#91/R2#309/R1#236/F#3");
F(3383349604615923509,"N496$F40:SF#8/S2#664/S1#531/R2#-1/R1#417/F#40");
F(5669254482802066175,"N611$F50:SF#911/S2#232/S1#585/R2#14/R1#689/F#50");
F(5904517948385532056,"N304$F43:SF#380/S2#452/S1#472/R2#34/R1#279/F#43");
F(3767669145160414704,"N500$F50:SF#593/S2#334/S1#615/R2#42/R1#647/F#50");
F(5605686416080307983,"N516$F42:SF#427/S2#255/S1#413/R2#31/R1#448/F#42");
F(7504737296503615996,"N449$F49:SF#476/S2#348/S1#422/R2#15/R1#391/F#49");
F(3110969788987702260,"N50$F1:SF#13/S2#114/S1#2/R2#124/R1#146/F#1");
F(4410803324925933048,"N410$F49:SF#477/S2#343/S1#507/R2#45/R1#472/F#49");
F(3143843064061638959,"N310$F50:SF#444/S2#256/S1#473/R2#20/R1#427/F#50");
F(2705378852203399152,"N380$F46:SF#361/S2#230/S1#477/R2#33/R1#338/F#46");
F(6579852108081993843,"N333$F40:SF#565/S2#346/S1#430/R2#39/R1#461/F#40");
F(2918473224841808884,"N319$F39:SF#432/S2#362/S1#468/R2#41/R1#265/F#39");
F(6427213298561652275,"N321$F42:SF#6/S2#364/S1#572/R2#-1/R1#474/F#42");
F(4884778740694078623,"N317$F35:SF#416/S2#5/S1#419/R2#42/R1#374/F#35");
F(162725006780084525,"N0$F1:SF#2/S2#118/S1#103/R2#78/R1#96/F#1");
F(6348126789368627007,"N491$F43:SF#8/S2#793/S1#813/R2#-1/R1#632/F#43");
F(3604602010319212147,"N419$F43:SF#399/S2#286/S1#660/R2#27/R1#505/F#43");
F(790153500799470647,"N350$F50:SF#414/S2#298/S1#538/R2#31/R1#438/F#50");
F(1476168234702819229,"N307$F46:SF#67/S2#467/S1#360/R2#19/R1#298/F#46");
F(4748792438837705715,"N548$F49:SF#755/S2#321/S1#505/R2#5/R1#612/F#49");
F(3836321544192291824,"N429$F49:SF#278/S2#355/S1#406/R2#42/R1#364/F#49");
F(7154640845146380536,"N2$F3:SF#16/S2#85/S1#165/R2#111/R1#124/F#3");
F(2237774461685208504,"N507$F49:SF#593/S2#331/S1#597/R2#13/R1#628/F#49");
F(3478269047515932916,"N226$F6:SF#12/S2#335/S1#212/R2#-1/R1#384/F#6");
F(7139633482268560624,"N481$F40:SF#530/S2#328/S1#575/R2#40/R1#452/F#40");
F(8819210638257563448,"N524$F50:SF#616/S2#222/S1#683/R2#33/R1#421/F#50");
F(3809822290527352952,"N336$F44:SF#466/S2#35/S1#404/R2#6/R1#376/F#44");
F(5892159917841671091,"N468$F44:SF#8/S2#357/S1#534/R2#5/R1#416/F#44");
F(5369727736269862643,"N149$F4:SF#2/S2#287/S1#295/R2#289/R1#346/F#4");
F(43954068738675635,"N94$F1:SF#1/S2#174/S1#178/R2#181/R1#257/F#1");
F(2269545555047048692,"N442$F38:SF#453/S2#345/S1#263/R2#32/R1#415/F#38");
F(3153944895803561011,"N565$F49:SF#550/S2#132/S1#821/R2#13/R1#419/F#49");
F(5306386929190509040,"N227$F7:SF#2/S2#423/S1#272/R2#274/R1#298/F#7");
F(6644819501431981581,"N448$F47:SF#636/S2#256/S1#429/R2#46/R1#610/F#47");
F(5470826830328383344,"N417$F39:SF#418/S2#348/S1#509/R2#29/R1#471/F#39");
F(3734735633812367152,"N308$F47:SF#482/S2#75/S1#358/R2#45/R1#366/F#47");
F(5878455138114809464,"N465$F40:SF#512/S2#294/S1#491/R2#39/R1#435/F#40");
F(8701560452810479894,"N238$F8:SF#4/S2#183/S1#720/R2#358/R1#566/F#8");
F(272237508481017599,"N372$F49:SF#378/S2#334/S1#332/R2#31/R1#378/F#49");
F(3367071910560805232,"N478$F48:SF#467/S2#359/S1#416/R2#43/R1#595/F#48");
F(632548700693034541,"N405$F50:SF#623/S2#338/S1#632/R2#46/R1#563/F#50");
F(1672929928408096511,"N80$F5:SF#4/S2#189/S1#147/R2#183/R1#193/F#5");
F(2365840924787935228,"N320$F41:SF#7/S2#462/S1#461/R2#-1/R1#438/F#41");
F(4835795424288854783,"N534$F43:SF#473/S2#7/S1#360/R2#11/R1#438/F#43");
F(4786221383811561971,"N532$F50:SF#547/S2#269/S1#498/R2#14/R1#496/F#50");
F(8242483872146784761,"N247$F10:SF#7/S2#369/S1#463/R2#-1/R1#396/F#10");
F(3414950441655667896,"N510$F49:SF#383/S2#358/S1#746/R2#30/R1#670/F#49");
F(8653806520219804659,"N325$F48:SF#472/S2#333/S1#503/R2#27/R1#383/F#48");
F(8751403729385497855,"N426$F42:SF#517/S2#321/S1#360/R2#32/R1#455/F#42");
F(3298943709841410815,"N509$F43:SF#780/S2#26/S1#611/R2#16/R1#482/F#43");
F(8254583490658920692,"N191$F3:SF#1/S2#295/S1#288/R2#254/R1#292/F#3");
F(2242599758264077173,"N469$F46:SF#724/S2#345/S1#610/R2#17/R1#508/F#46");
F(6283499006169064561,"N96$F3:SF#14/S2#343/S1#377/R2#293/R1#279/F#3");
F(1218584237053276716,"N470$F48:SF#392/S2#358/S1#318/R2#25/R1#519/F#48");
F(177445338045763963,"N362$F34:SF#454/S2#316/S1#335/R2#39/R1#377/F#34");
F(3811214349732827639,"N306$F45:SF#326/S2#321/S1#287/R2#12/R1#326/F#45");
F(2022695808634026871,"N502$F38:SF#481/S2#315/S1#436/R2#38/R1#352/F#38");
F(884229678266679047,"N225$F5:SF#11/S2#405/S1#452/R2#-1/R1#423/F#5");
F(715824920640516208,"N444$F42:SF#3/S2#536/S1#316/R2#-1/R1#538/F#42");
F(5569326446179867312,"N128$F1:SF#1/S2#358/S1#369/R2#185/R1#235/F#1");
F(6234481150811274505,"N178$F3:SF#1/S2#275/S1#240/R2#300/R1#368/F#3");
F(1521428050261533104,"N475$F42:SF#771/S2#268/S1#548/R2#9/R1#525/F#42");
F(5876412243541689651,"N377$F40:SF#610/S2#77/S1#566/R2#28/R1#469/F#40");
F(2013175459466588276,"N402$F46:SF#360/S2#334/S1#445/R2#48/R1#369/F#46");
F(7875800110995943292,"N254$F11:SF#13/S2#471/S1#446/R2#-1/R1#402/F#11");
F(4312845269840852389,"N236$F5:SF#1/S2#209/S1#551/R2#325/R1#448/F#5");
F(4999366177680470008,"N397$F40:SF#450/S2#228/S1#479/R2#17/R1#371/F#40");
F(730546249486635108,"N404$F49:SF#1/S2#946/S1#944/R2#-1/R1#-1/F#49");
F(2812846724864755592,"N82$F9:SF#9/S2#221/S1#351/R2#265/R1#215/F#9");
F(8558069938371913469,"N246$F9:SF#3/S2#378/S1#528/R2#-1/R1#441/F#9");
F(8968931612157900443,"N334$F41:SF#759/S2#160/S1#414/R2#25/R1#519/F#41");
F(2670996655014939780,"N151$F7:SF#11/S2#394/S1#528/R2#-1/R1#527/F#7");
F(794705242758927593,"N192$F5:SF#6/S2#812/S1#772/R2#-1/R1#625/F#5");
F(6148613380688053096,"N131$F5:SF#10/S2#190/S1#633/R2#351/R1#476/F#5");

F(4214592713333346159,"NN14$F1:SF#24/S2#11/S1#6/R2#1/R1#3/F#1");
F(3578133025786179621,"NN72$F26:SF#31/S2#5/S1#28/R2#21/R1#15/F#26");
F(2704761444930158648,"NN147$F49:SF#29/S2#22/S1#30/R2#16/R1#6/F#49");
F(3744811243287553595,"NN22$F39:SF#172/S2#53/S1#55/R2#28/R1#33/F#39");
F(3057978882323318714,"NN60$F44:SF#19/S2#24/S1#4/R2#42/R1#47/F#44");
F(4498584458679892372,"NN62$F49:SF#14/S2#13/S1#17/R2#57/R1#28/F#49");
F(4807645409586740234,"NN13$F49:SF#23/S2#37/S1#95/R2#36/R1#56/F#49");
F(8473819078341107242,"NN28$F22:SF#94/S2#28/S1#28/R2#24/R1#44/F#22");
F(4195804511425862203,"NN74$F1:SF#101/S2#49/S1#15/R2#3/R1#1/F#1");
F(8369684406172610558,"NN119$F7:SF#59/S2#71/S1#17/R2#2/R1#5/F#7");
F(8937723292883972139,"NN56$F49:SF#41/S2#82/S1#20/R2#11/R1#28/F#49");
F(3541263461328893067,"NN78$F15:SF#54/S2#18/S1#258/R2#27/R1#115/F#15");
F(9046946089716627957,"NN80$F14:SF#22/S2#65/S1#46/R2#11/R1#14/F#14");
F(3636513346743449971,"NN128$F2:SF#57/S2#10/S1#46/R2#2/R1#2/F#2");
F(683473126911976489,"NN55$F45:SF#59/S2#11/S1#22/R2#21/R1#51/F#45");
F(907894410840470934,"NN50$F12:SF#31/S2#98/S1#14/R2#32/R1#60/F#12");
F(4228905885252586485,"NN42$F25:SF#3/S2#7/S1#10/R2#18/R1#42/F#25");
F(8642987661815649341,"NN151$F24:SF#34/S2#13/S1#11/R2#11/R1#17/F#24");
F(582431857002163502,"NN27$F20:SF#65/S2#1/S1#7/R2#22/R1#6/F#20");
F(3800611596107412087,"NN112$F18:SF#70/S2#10/S1#11/R2#5/R1#4/F#18");
F(6321122834796590647,"NN46$F49:SF#7/S2#196/S1#90/R2#49/R1#15/F#49");
F(3293601555448279861,"NN88$F33:SF#122/S2#74/S1#23/R2#17/R1#34/F#33");
F(8116909241244531214,"NN66$F43:SF#6/S2#76/S1#23/R2#29/R1#50/F#43");
F(9204912856719718577,"NN144$F8:SF#18/S2#52/S1#15/R2#4/R1#10/F#8");
F(4037809080436198431,"NN19$F15:SF#46/S2#67/S1#18/R2#25/R1#6/F#15");
F(2991930514629139879,"NN82$F38:SF#32/S2#97/S1#132/R2#37/R1#61/F#38");
F(2755472314120683519,"NN17$F13:SF#70/S2#64/S1#40/R2#17/R1#25/F#13");
F(2608772873577240506,"NN100$F47:SF#46/S2#45/S1#11/R2#25/R1#15/F#47");
F(3857948979496039578,"NN70$F10:SF#42/S2#400/S1#148/R2#6/R1#69/F#10");
F(9158489250620256633,"NN103$F23:SF#41/S2#104/S1#11/R2#6/R1#27/F#23");
F(830483111773350044,"NN135$F37:SF#113/S2#95/S1#116/R2#24/R1#37/F#37");
F(2104372845788819726,"NN95$F18:SF#117/S2#22/S1#42/R2#34/R1#59/F#18");
F(1869074646707414201,"NN131$F47:SF#105/S2#217/S1#91/R2#31/R1#128/F#47");
F(8769688483052355343,"NN102$F16:SF#24/S2#45/S1#84/R2#11/R1#1/F#16");
F(3570825335105984830,"NN57$F3:SF#188/S2#146/S1#158/R2#5/R1#41/F#3");
F(3357748597096264751,"NN94$F14:SF#87/S2#91/S1#35/R2#10/R1#57/F#14");
F(5200021978061090582,"NN37$F43:SF#53/S2#81/S1#89/R2#29/R1#35/F#43");
F(2048425106537282418,"NN113$F33:SF#90/S2#311/S1#159/R2#21/R1#15/F#33");
F(1422617951160056118,"NN142$F3:SF#40/S2#263/S1#114/R2#7/R1#16/F#3");
F(429565990317948203,"NN68$F4:SF#325/S2#376/S1#233/R2#19/R1#43/F#4");
F(8083988162486946037,"NN44$F42:SF#119/S2#211/S1#30/R2#50/R1#32/F#42");
F(3764222229240822132,"NN85$F21:SF#121/S2#32/S1#36/R2#14/R1#17/F#21");
F(2181004963463052175,"NN91$F39:SF#3/S2#51/S1#33/R2#38/R1#73/F#39");
F(8901678733245100277,"NN126$F50:SF#57/S2#166/S1#15/R2#49/R1#13/F#50");
F(3677258354193283742,"NN122$F21:SF#55/S2#13/S1#11/R2#25/R1#12/F#21");
F(8185635684111158702,"NN49$F11:SF#44/S2#31/S1#81/R2#40/R1#58/F#11");
F(6322530694744062270,"NN129$F4:SF#52/S2#99/S1#49/R2#8/R1#14/F#4");
F(8158604522317689786,"NN150$F34:SF#335/S2#156/S1#18/R2#21/R1#23/F#34");
F(3110142072024481301,"NN77$F23:SF#64/S2#126/S1#19/R2#6/R1#9/F#23");
F(484434366860765720,"NN25$F2:SF#120/S2#99/S1#16/R2#2/R1#2/F#2");
F(3533677488782784824,"NN109$F22:SF#59/S2#107/S1#104/R2#16/R1#2/F#22");
F(771271913706303908,"NN138$F24:SF#14/S2#117/S1#45/R2#29/R1#44/F#24");
F(1633523196169432879,"NN127$F45:SF#114/S2#12/S1#183/R2#30/R1#154/F#45");
F(2921175292036592702,"NN115$F35:SF#112/S2#50/S1#59/R2#17/R1#104/F#35");
F(8074809130360072105,"NN81$F26:SF#52/S2#191/S1#98/R2#36/R1#72/F#26");
F(1869322170494487463,"NN90$F16:SF#43/S2#87/S1#28/R2#37/R1#56/F#16");
F(8915284691268672933,"NN15$F6:SF#1/S2#6/S1#91/R2#5/R1#26/F#6");
F(2915923464379774245,"NN41$F22:SF#147/S2#4/S1#82/R2#31/R1#59/F#22");
F(2843230469308317561,"NN84$F18:SF#59/S2#43/S1#21/R2#7/R1#15/F#18");
F(2737755310522722572,"NN116$F1:SF#34/S2#292/S1#257/R2#3/R1#2/F#1");
F(8757028057945802845,"NN106$F45:SF#213/S2#198/S1#116/R2#37/R1#94/F#45");
F(1591219777721741610,"NN39$F3:SF#146/S2#94/S1#71/R2#10/R1#36/F#3");
F(4139131370571839864,"NN76$F11:SF#55/S2#79/S1#39/R2#14/R1#68/F#11");
F(4370318161371410497,"NN36$F36:SF#128/S2#127/S1#199/R2#35/R1#181/F#36");
F(4500562797800339294,"NN120$F12:SF#149/S2#95/S1#204/R2#6/R1#56/F#12");
F(2986790993471933273,"NN61$F46:SF#28/S2#1/S1#64/R2#30/R1#61/F#46");
F(3162509568757803624,"NN133$F40:SF#173/S2#129/S1#237/R2#15/R1#16/F#40");
F(7723726378715450235,"NN65$F39:SF#56/S2#86/S1#30/R2#19/R1#65/F#39");
F(3976984302900941982,"NN96$F25:SF#181/S2#318/S1#150/R2#47/R1#52/F#25");
F(3494543285441097767,"NN52$F35:SF#72/S2#144/S1#43/R2#46/R1#77/F#35");
F(3938185067766834312,"NN136$F45:SF#206/S2#30/S1#127/R2#47/R1#22/F#45");
F(1610105445478250789,"NN155$F43:SF#196/S2#80/S1#145/R2#43/R1#10/F#43");
F(9166239149900919331,"NN149$F22:SF#78/S2#186/S1#172/R2#34/R1#19/F#22");
F(3997644989679891262,"NN111$F41:SF#245/S2#119/S1#11/R2#47/R1#108/F#41");
F(6172243797782890039,"NN47$F3:SF#1/S2#101/S1#56/R2#11/R1#25/F#3");
F(8372496449047718757,"NN10$F13:SF#135/S2#20/S1#161/R2#21/R1#48/F#13");
F(3203480138124705463,"NN141$F43:SF#85/S2#217/S1#100/R2#31/R1#95/F#43");
F(6376381540453465147,"NN67$F2:SF#99/S2#66/S1#39/R2#3/R1#11/F#2");
F(4225526905999353723,"NN117$F41:SF#345/S2#406/S1#308/R2#35/R1#32/F#41");
F(1595728390377326197,"NN148$F9:SF#120/S2#228/S1#339/R2#39/R1#38/F#9");
F(435565969977249547,"NN98$F25:SF#242/S2#15/S1#175/R2#8/R1#40/F#25");
F(8242831102903389720,"NN158$F13:SF#212/S2#282/S1#172/R2#4/R1#14/F#13");
F(8680696743666722715,"NN11$F27:SF#138/S2#168/S1#98/R2#24/R1#47/F#27");
F(9021014326471433985,"NN92$F28:SF#170/S2#124/S1#202/R2#50/R1#163/F#28");
F(4433546926024703418,"NN73$F27:SF#11/S2#113/S1#24/R2#7/R1#18/F#27");
F(1782955292999556345,"NN152$F25:SF#308/S2#52/S1#263/R2#45/R1#40/F#25");
F(3075127246420670863,"NN40$F19:SF#34/S2#10/S1#78/R2#12/R1#25/F#19");
F(943239307364020532,"NN69$F6:SF#80/S2#39/S1#69/R2#13/R1#10/F#6");
F(3856907656146466607,"NN24$F48:SF#95/S2#84/S1#75/R2#37/R1#29/F#48");
F(1436988691883042602,"NN18$F14:SF#301/S2#134/S1#66/R2#32/R1#160/F#14");
F(2751154557719704181,"NN83$F6:SF#209/S2#292/S1#211/R2#44/R1#89/F#6");
F(5664199831036251001,"NN89$F9:SF#180/S2#295/S1#273/R2#25/R1#95/F#9");
F(3332129548102563118,"NN93$F37:SF#27/S2#120/S1#32/R2#17/R1#39/F#37");
F(3563816962127367309,"NN110$F12:SF#236/S2#110/S1#45/R2#9/R1#13/F#12");
F(8578259956801211825,"NN7$F5:SF#17/S2#28/S1#1/R2#2/R1#6/F#5");
F(4547568922277524763,"NN0$F6:SF#56/S2#205/S1#29/R2#30/R1#51/F#6");
F(1451929415066744184,"NN157$F38:SF#60/S2#322/S1#109/R2#30/R1#135/F#38");
F(5149920318253120433,"NN58$F9:SF#174/S2#90/S1#111/R2#23/R1#8/F#9");
F(7436578330337967730,"NN105$F8:SF#195/S2#212/S1#219/R2#3/R1#2/F#8");
F(6447798682526492528,"NN31$F42:SF#193/S2#128/S1#206/R2#47/R1#40/F#42");
F(8141706977649321975,"NN30$F34:SF#61/S2#174/S1#145/R2#40/R1#98/F#34");
F(3709055751755543551,"NN1$F12:SF#63/S2#220/S1#101/R2#39/R1#55/F#12");
F(1559714368675125150,"NN23$F43:SF#62/S2#45/S1#96/R2#50/R1#88/F#43");
F(8113862769104131370,"NN26$F16:SF#56/S2#114/S1#13/R2#10/R1#14/F#16");
F(8304157743565789370,"NN34$F25:SF#93/S2#44/S1#55/R2#27/R1#8/F#25");
F(8580776910193571257,"NN108$F34:SF#397/S2#302/S1#223/R2#45/R1#84/F#34");
F(1443981561852665502,"NN125$F46:SF#3/S2#338/S1#143/R2#105/R1#104/F#46");
F(1782074307999702201,"NN33$F15:SF#17/S2#1/S1#30/R2#10/R1#19/F#15");
F(517079093421175927,"NN143$F48:SF#221/S2#118/S1#207/R2#37/R1#128/F#48");
F(3942158742513728115,"NN20$F17:SF#132/S2#152/S1#61/R2#18/R1#23/F#17");
F(6070026523484428421,"NN156$F8:SF#203/S2#262/S1#283/R2#11/R1#23/F#8");
F(8082399975162804559,"NN45$F47:SF#10/S2#220/S1#116/R2#45/R1#60/F#47");
F(1935206940133567260,"NN75$F10:SF#98/S2#121/S1#138/R2#10/R1#61/F#10");
F(3755226947258514711,"NN8$F7:SF#71/S2#211/S1#183/R2#16/R1#53/F#7");
F(7673015807258091518,"NN54$F19:SF#139/S2#421/S1#398/R2#43/R1#87/F#19");
F(4520235184399809940,"NN121$F48:SF#211/S2#235/S1#138/R2#49/R1#122/F#48");
F(2914165032304851765,"NN124$F35:SF#196/S2#233/S1#191/R2#40/R1#116/F#35");
F(3472042976799563111,"NN9$F10:SF#25/S2#105/S1#102/R2#11/R1#34/F#10");
F(8049370163349123780,"NN154$F37:SF#223/S2#92/S1#348/R2#33/R1#41/F#37");
F(6313773668399920629,"NN71$F17:SF#256/S2#61/S1#133/R2#16/R1#78/F#17");
F(1686057867234971189,"NN130$F39:SF#129/S2#290/S1#275/R2#44/R1#154/F#39");
F(3209410378613555081,"NN43$F37:SF#28/S2#88/S1#1/R2#32/R1#20/F#37");
F(2826597228994853040,"NN29$F26:SF#116/S2#65/S1#23/R2#36/R1#57/F#26");
F(8438157397946823807,"NN134$F20:SF#140/S2#74/S1#105/R2#25/R1#76/F#20");
F(968610813722953833,"NN132$F20:SF#65/S2#99/S1#341/R2#4/R1#4/F#20");
F(3024506533924988919,"NN99$F16:SF#207/S2#382/S1#101/R2#13/R1#28/F#16");
F(2124143198846597625,"NN32$F47:SF#20/S2#40/S1#83/R2#122/R1#134/F#47");
F(4506835177739859609,"NN16$F11:SF#148/S2#196/S1#253/R2#3/R1#31/F#11");
F(7691144412739285756,"NN51$F14:SF#195/S2#78/S1#214/R2#18/R1#54/F#14");
F(933949062497703296,"NN21$F25:SF#292/S2#112/S1#165/R2#29/R1#36/F#25");
F(7943511301547625609,"NN53$F7:SF#17/S2#176/S1#102/R2#24/R1#75/F#7");
F(3545853567818227113,"NN5$F33:SF#186/S2#144/S1#227/R2#46/R1#76/F#33");
F(2921205876397407109,"NN145$F46:SF#135/S2#49/S1#118/R2#34/R1#16/F#46");
F(4391979502236215306,"NN123$F34:SF#732/S2#410/S1#217/R2#29/R1#24/F#34");
F(8576680693070984759,"NN107$F5:SF#177/S2#117/S1#213/R2#21/R1#50/F#5");
F(8858888998121859973,"NN59$F26:SF#184/S2#79/S1#242/R2#31/R1#83/F#26");
F(2284511003760006500,"NN38$F50:SF#22/S2#202/S1#121/R2#113/R1#137/F#50");
F(1811917090613388392,"NN79$F5:SF#380/S2#320/S1#330/R2#5/R1#73/F#5");
F(557642002602471605,"NN35$F27:SF#40/S2#77/S1#57/R2#40/R1#96/F#27");
F(5867193805684550838,"NN6$F4:SF#82/S2#614/S1#101/R2#17/R1#37/F#4");
F(4412244728790731164,"NN118$F42:SF#214/S2#164/S1#222/R2#46/R1#69/F#42");
F(8977712763206249897,"NN63$F6:SF#110/S2#255/S1#343/R2#18/R1#70/F#6");
F(4066492671407033466,"NN97$F26:SF#111/S2#245/S1#181/R2#4/R1#8/F#26");
F(8873618702612629430,"NN140$F9:SF#92/S2#215/S1#211/R2#8/R1#16/F#9");
F(950934232542551675,"NN87$F2:SF#317/S2#229/S1#252/R2#3/R1#7/F#2");
F(7785993466247526781,"NN146$F30:SF#204/S2#391/S1#208/R2#43/R1#44/F#30");
F(8541421078401858687,"NN104$F14:SF#344/S2#194/S1#296/R2#13/R1#77/F#14");
F(8604524454271138425,"NN137$F9:SF#203/S2#150/S1#194/R2#7/R1#24/F#9");
F(8181404631515411837,"NN4$F30:SF#58/S2#159/S1#143/R2#43/R1#52/F#30");
F(2966578330246131761,"NN2$F19:SF#181/S2#184/S1#92/R2#42/R1#61/F#19");
F(1530676447418850685,"NN114$F13:SF#85/S2#176/S1#191/R2#9/R1#158/F#13");
F(1026387134037572594,"NN101$F7:SF#340/S2#599/S1#165/R2#50/R1#146/F#7");
F(1885630973625922711,"NN12$F38:SF#348/S2#82/S1#104/R2#33/R1#81/F#38");
F(7452168460625585082,"NN64$F29:SF#226/S2#197/S1#243/R2#50/R1#56/F#29");
F(7428184375167437931,"NN86$F31:SF#463/S2#220/S1#391/R2#23/R1#14/F#31");
F(8194610886543027564,"NN153$F13:SF#396/S2#631/S1#281/R2#45/R1#227/F#13");
F(8753055440524574454,"NN139$F11:SF#281/S2#259/S1#322/R2#34/R1#183/F#11");
F(5150278062150941994,"NN3$F27:SF#436/S2#427/S1#389/R2#47/R1#151/F#27");
F(1812555305555998057,"NN48$F5:SF#361/S2#465/S1#544/R2#37/R1#227/F#5");
  return out;
};
int main_test_gen(){
  srand(time(0));
  auto meta_strategies=make_meta_pack();
  t_main_test mt;
  mt.sarr.push_back(make_unique<t_season>());
  auto&s=*mt.sarr[0];
  init_splinter_2025_season(s);
  next_phase(s,"S1");
  //
  auto q = [&](uint64_t v, const string& name,string*psrc=nullptr,string phase="init") {
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
    t.source_phase = phase;
    t.source_rank = 0;
    t.phase2rank = {};
    coder.tracked=t;
    if(psrc)coder.sarr[0].cdn_bin_url=*psrc;
    return &coder;
  };
  bool hide=false;
  for (int i = 0; i < 1; ++i) {
    for (auto& [v, name] : meta_strategies) {
      if(v==2600625415943971123)hide=true;
      q(v, name)->hide=hide;
    }
    int g=-1;
    for(auto&ex:genpack)q(0,"genpack"+IToS(++g)+"$"+ex.first,&ex.second);
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

  std::random_device rd;
  std::mt19937 rng(rd());
  print_strategy(parse_strategy_v9(
"#|C|D|M|A|S\n"
"-----------\n"
"0|1|1|C|H|N\n"
"1|1|S|C|A|F\n"
"2|2|2|C|H|N\n"
"3|S|S|C|A|N\n"
"4|2|2|A|A|N\n"
"5|S|S|C|A|N\n"
"6|2|2|C|A|N\n"
"7|S|S|C|A|N\n"));
  print_strategy(3223024948381295921);
  cout<<"---\n";

  auto rmp=[&](const string&phase){
    record_all_ranks(s,phase);
    record_all_tracked_ranks(s,phase);
    mark_top_n_as_tracked(s,phase);
    record_meta_ranks(s,phase);
    print_top(mt,s,phase);
    print_meta_top16(mt,s,phase); 
    //print_all_tracked_summary(mt,s);
    //print_top_with_context(mt,s,phase,16);
  };
  auto SG=[&](const string&phase,double koef=1.0){
    if(phase=="S1-"){
      for(int i=0;i<59;i++){
        add_some_by_random(mt.garr,s);
      }
      for(int i=0;i<1000;i++){
        bool lastwin=false;
        add_some_by_random(mt.garr,s);
        if(add_finished_game(mt,s,&lastwin)){if(!lastwin){
          mt.garr.pop_back();
          s.phases[0]->uids.erase(mt.garr.size());
          if(i%100==0)cout<<i<<" - dropped\n";
        }else{
          cout<<i<<" - new\n";
          auto src=((uint64_t*)&s.uid2scoder[mt.garr.size()-1].sarr.back().cdn_bin_url[0])[0];
          mt.garr.pop_back();
          s.phases[0]->uids.erase(mt.garr.size());
          q(src,"lastwin"+IToS(i),0,"lastwin");
        }}
      }
      s.phases[0]->uid2score={};
      s.phases[0]->uid2games={};
    }
    for(int i=0;i<mt_games_per_phase*koef;i++){
      //if(rand()%500==0)add_some_by_random(mt.garr,s);
      if(add_finished_game(mt,s,0,false,false)){}
    }
  };
  string cur;
  auto SP=[&](double koef=1.0){
    next_phase(s,cur);
    SG(cur,koef);
    rmp(cur);
  };
  auto RP=[&](double koef){
    next_phase(s,cur);
    play_all_game_iniside_round(mt,s,cur,koef);
    rmp(cur);
  };
  int num_of_starts=900*1;
  for (int i = 0; i < num_of_starts; ++i) {
    auto e=generate_strategy(rng);
    if(i%100==0)std::cout << "Strategy " << i << ": " << e << "\n";
    //print_strategy(e);
    f(e);
  }
  mt_games_per_phase=((mt.garr.size()*1.0/16)+1)*3;//*110;//*5*15;
  auto N=min<double>(mt.garr.size(),900);
  cur="S1";SP(2.0);
  cur="R1";RP(2*900.0/N);
  cur="S2";SP(1.0);
  cur="R2";RP(2*360.0/N);
  cur="SF";SP(1.0);
  cur="F";RP(2*60.0/N);
  print_top(mt,s,"F",60);
  {
    auto*p=find_phase(s,cur);
    vector<pair<double, int>> ranked;
    for (int uid : p->uids) {
      double score = p->uid2score.count(uid) ? p->uid2score.at(uid) : 1500.0;
      ranked.emplace_back(-score, uid);
    }
    sort(ranked.begin(), ranked.end());
    int N=std::min(50,(int)ranked.size());
    for(int i=0;i<N;i++){
      auto uid=ranked[i].second;
      auto&sc=s.uid2scoder[uid];
      if(sc.tracked&&sc.tracked->source_phase=="init")continue;
      string way="F"+to_string(i+1)+":";
      vector<string> f;
      for(auto&r:sc.phase2rank){
        f.push_back(r.first+"#"+to_string(r.second));
      }
      reverse(f.begin(),f.end());
      way+=join(f,"/");
      genpack.push_back({way,s.uid2scoder[uid].sarr.back().cdn_bin_url});
    }
    print_meta_top16(mt,s,cur);
  }
  /*
  next_phase(s,"S");
  for(int i=0;i<mt_games_per_phase;i++){
    if(rand()%500==0)add_some_by_random(mt.garr,s);
    add_finished_game(mt,s);
  }
  rmp("S");*/
  //print_meta_summary();
  /*for(;;){
     this_thread::sleep_for(1000ms);
  }*/
  int gg=1;
  //next_phase(s,"frozen-end?");
  return 0;
}

void main_test(){
  for(;;){
    auto news=main_test_gen();
  }
}