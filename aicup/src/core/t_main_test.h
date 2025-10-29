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
std::vector<double> simulate_arena_v9(const std::vector<Strategy>& strategies,bool debug);
std::vector<double> simulate_arena(const std::vector<uint64_t>& strategies,bool debug=false) {
  return simulate_arena_v9((std::vector<Strategy>&)strategies,debug);
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

        auto colC = stayC ? 'S' : offset_char;
        auto colD = stayD ? 'S' : offset_char;

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
  return {};
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
  };
  
  for (int i = 0; i < 1; ++i) {
    for (auto& [v, name] : meta_strategies) {
      q(v, name);
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
  print_strategy(17305345489350727204);
  cout<<"---\n";
  //for (int i = 0; i < 3;i++){
  //  g(generous_tft_num);g(opportunistic_num);g(fast_trust_num);g(ada);f(ada);
  //  for(auto v:meta_pack)q(v);
  //}
  /*
  for(int i=0;i<59;i++){
    add_some_by_random(mt.garr,encode_strategy(generate_reachable_strategy(rng)));
  }*/
  if(0)for(int i=0;i<60*1000;i++){
    //f(encode_strategy(generate_reachable_strategy(rng)));
    bool lastwin=false;
    if(add_finished_game(mt,s,&lastwin)){if(!lastwin){
      mt.garr.pop_back();
      s.phases[0]->uids.erase(mt.garr.size());
    }else{
      cout<<i<<"\n";
    }}
  }
  /*for(int i=0;i<mt_games_per_phase;i++){
    //if(rand()%3==0)f(encode_strategy(generate_reachable_strategy(rng)));
    add_finished_game(mt,s);
  }*/
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
  int num_of_starts=900;
  for (int i = 0; i < num_of_starts; ++i) {
    auto e=generate_strategy(rng);
    if(i%100==0)std::cout << "Strategy " << i << ": " << e << "\n";
    //print_strategy(e);
    f(e);
  }
  mt_games_per_phase=(mt.garr.size()*1.0/16)+1;
  cur="S1";SP(2.0);
  cur="R1";RP(2*360.0/900.0);
  cur="S2";SP(1.0);
  cur="R2";RP(2*360.0/900.0);
  cur="SF";SP(1.0);
  cur="F";RP(2*60.0/900.0);
  print_top(mt,s,"F",60);
  {
    auto*p=find_phase(s,cur);
    vector<pair<double, int>> ranked;
    for (int uid : p->uids) {
      double score = p->uid2score.count(uid) ? p->uid2score.at(uid) : 1500.0;
      ranked.emplace_back(-score, uid);
    }
    sort(ranked.begin(), ranked.end());
    for(int i=0;i<50;i++){
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