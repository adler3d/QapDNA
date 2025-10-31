/*
#include <iostream>
using namespace std;
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <random>

#include <memory>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <sstream>
#include <climits>
template<class TYPE>static bool qap_check_id(const vector<TYPE>&arr,int id){return id>=0&&id<arr.size();}
class vec2i{
public:
  typedef vec2i SelfClass;
  int x, y;
  vec2i():x(0),y(0){}
  vec2i(int x,int y):x(x),y(y){}
  friend vec2i operator*(int u,const vec2i&v){return vec2i(u*v.x,u*v.y);}
  friend vec2i operator*(const vec2i&v,int u){return vec2i(u*v.x,u*v.y);}
  friend vec2i operator/(const vec2i&v,int d){return vec2i(v.x/d,v.y/d);}
  friend vec2i operator+(const vec2i&u,const vec2i&v){return vec2i(u.x+v.x,u.y+v.y);}
  friend vec2i operator-(const vec2i&u,const vec2i&v){return vec2i(u.x-v.x,u.y-v.y);}
  void operator+=(const vec2i&v){x+=v.x;y+=v.y;}
  void operator-=(const vec2i&v){x-=v.x;y-=v.y;}
  int SqrMag()const{return x*x+y*y;}
  float Mag()const{return sqrt(float(x*x+y*y));}
  vec2i operator+()const{return vec2i(+x,+y);}
  vec2i operator-()const{return vec2i(-x,-y);}
  friend bool operator==(const vec2i&u,const vec2i&v){return (u.x==v.x)&&(u.y==v.y);}
  friend bool operator!=(const vec2i&u,const vec2i&v){return (u.x!=v.x)||(u.y!=v.y);}
};
using Strategy = std::array<uint8_t, 8>;

struct t_pubg_world{
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
                // Обычная стратегия > преобразуем state_byte в Action
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
*//*
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
        // На ранних тиках центр = скопление врагов > штраф
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
  auto r=sqrt(dir.x*dir.x+dir.y*dir.y);
  auto k=!r?0:100.0/r;
  a.dir=vec2i(dir.x*k,dir.y*k);
  return a;
}
t_pubg_world::Action dna_by_adler(const t_pubg_world&orig,int i){
  auto w=orig;
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
  double best_score_a1 = -1e9;
  t_pubg_world::Action best_action_a1;
  for(int dx=-1;dx<=+1;dx++)for(int dy=-1;dy<=+1;dy++)
  for(int eid=0;eid<earr.size();eid++)
  for (uint64_t c1 = 0; c1 < 8; ++c1) {
      auto a=encode_action(c1,earr[eid],true,vec2i(dx,dy));
      if(a.avoid_others)continue;
      auto world1 = orig;
      world1.agents[i].a=a;
      world1.step();

      if (!world1.agents[i].alive) {
          double score = -10000;
          if (score > best_score_a1) {
              best_score_a1 = score;
              best_action_a1 = encode_action(c1,earr[eid],true,vec2i(dx,dy));
          }
          continue;
      }
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
      if (best_score_a2 > best_score_a1) {
          best_score_a1 = best_score_a2;
          best_action_a1 = encode_action(c1,earr[eid],true,vec2i(dx,dy));
      }
  }
  return best_action_a1;
}*/
t_pubg_world::Action dna_by_perplexity(const t_pubg_world& orig, int i) {
    t_pubg_world::Action act;
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
t_pubg_world::Action dna_by_deepseek(const t_pubg_world& orig, int i) {
    t_pubg_world::Action action;
    
    // Если это агент 0 (наш целевой агент)
    if (true) {
        // Агрессивная стратегия для максимизации счёта
        action.attack = true;
        action.use_fury = true;  // Используем ярость для максимального урона
        action.use_rush = true;  // Используем рывок для быстрого перемещения
        action.chase_enemy = true;  // Преследуем врагов
        action.move_to_center = false;
        action.repel_from_enemy = false;
        action.avoid_others = false;
        action.use_dir = false;
        
        // Находим ближайшего живого врага
        int closest_dist_sq = 100000000;
        int closest_enemy = -1;
        
        for (int j = 0; j < (int)orig.agents.size(); ++j) {
            if (i == j || !orig.agents[j].alive) continue;
            int dx = orig.agents[i].x - orig.agents[j].x;
            int dy = orig.agents[i].y - orig.agents[j].y;
            int dist_sq = dx*dx + dy*dy;
            if (dist_sq < closest_dist_sq) {
                closest_dist_sq = dist_sq;
                closest_enemy = j;
            }
        }
        
        // Если нашли врага, устанавливаем его как цель
        if (closest_enemy != -1) {
            action.target = closest_enemy;
            
            // Вычисляем направление к врагу для точного движения
            int dx = orig.agents[closest_enemy].x - orig.agents[i].x;
            int dy = orig.agents[closest_enemy].y - orig.agents[i].y;
            
            // Нормализуем направление (грубая нормализация)
            if (dx != 0 || dy != 0) {
                int len = std::max(1, (int)std::sqrt(dx*dx + dy*dy));
                action.use_dir = true;
                action.dir.x = (dx * 100) / len;  // Масштабируем для лучшего движения
                action.dir.y = (dy * 100) / len;
            }
        }
        
        // Дополнительная логика для выживания
        if (orig.agents[i].hp < 30) {
            // При низком HP становимся осторожнее
            action.use_fury = false;
            action.avoid_others = true;
            
            // Двигаемся к центру для безопасности
            if (orig.agents[i].x*orig.agents[i].x + orig.agents[i].y*orig.agents[i].y > 
                orig.R*orig.R*0.25) {
                action.move_to_center = true;
                action.chase_enemy = false;
            }
        }
        
        // Энергоменеджмент
        if (orig.agents[i].energy < 20) {
            action.use_fury = false;
            action.use_rush = false;
        }
        
    } else {
        // Для других агентов - базовая стратегия выживания
        action.attack = true;
        action.chase_enemy = true;
        action.move_to_center = orig.agents[i].x*orig.agents[i].x + 
                               orig.agents[i].y*orig.agents[i].y > orig.R*orig.R*0.64;
        action.use_fury = orig.agents[i].hp > 50 && orig.agents[i].energy > 30;
        action.use_rush = orig.agents[i].energy > 20;
        action.avoid_others = true;
        action.use_dir = false;
    }
    
    return action;
}
t_pubg_world::Action dna_by_grok_v0(const t_pubg_world& world, int i) {
    const auto& me = world.agents[i];
    t_pubg_world::Action act;
    act.use_dir = true;
    act.dir = vec2i(0, 0);

    if (!me.alive) return act;

    const int R = world.R;
    const int tick = world.tick;
    const int N = world.agents.size();

    // === 1. Собираем информацию ===
    vec2i center(0, 0);
    vec2i pos(me.x, me.y);
    int dist_to_center_sq = pos.SqrMag();
    bool in_safe_zone = dist_to_center_sq <= R * R;

    // Найдём ближайшего живого врага
    int best_enemy = -1;
    int min_dist_sq = 1e9;
    vec2i to_enemy(0, 0);
    int enemy_hp = 0;
    bool enemy_in_range = false;

    // Также ищем самого слабого врага в радиусе атаки
    int weakest_in_range = -1;
    int min_hp_in_range = 1000;
    vec2i to_weakest(0, 0);
    int weak_dist_sq = 0;

    for (int j = 0; j < N; ++j) {
        if (j == i || !world.agents[j].alive) continue;
        vec2i diff(me.x - world.agents[j].x, me.y - world.agents[j].y);
        int d_sq = diff.SqrMag();
        int hp = world.agents[j].hp;

        if (d_sq < min_dist_sq) {
            min_dist_sq = d_sq;
            best_enemy = j;
            to_enemy = diff;
        }

        if (d_sq <= 10000 && hp < min_hp_in_range) {
            min_hp_in_range = hp;
            weakest_in_range = j;
            to_weakest = diff;
            weak_dist_sq = d_sq;
        }

        if (d_sq <= 10000) enemy_in_range = true;
    }

    // === 2. Решения по движению ===
    vec2i desired_dir(0, 0);
    bool should_rush = false;
    bool should_fury = false;

    // Приоритет 1: Оставаться в зоне!
    if (!in_safe_zone || dist_to_center_sq > (R * 3 / 4) * (R * 3 / 4)) {
        desired_dir = -pos; // бежим в центр
        should_rush = me.energy >= 20; // рывок, если далеко и есть энергия
    }
    // Приоритет 2: Атаковать слабого врага в радиусе
    else if (weakest_in_range != -1 && me.energy >= 3) {
        act.attack = true;
        act.target = weakest_in_range;
        should_fury = (min_hp_in_range <= 50 && me.hp > 40) || (me.energy >= 10 && min_hp_in_range <= 30);
        // Подходим ближе, если не в упор
        if (weak_dist_sq > 2500) {
            desired_dir = -to_weakest;
            should_rush = me.energy >= 15;
        }
    }
    // Приоритет 3: Преследовать ближайшего, если он близко и мы сильны
    else if (best_enemy != -1 && min_dist_sq < 300*300 && me.hp > 30) {
        desired_dir = -to_enemy;
        should_rush = me.energy >= 10 && min_dist_sq > 100*100;
        act.attack = min_dist_sq <= 10000 && me.energy >= 2;
    }
    // Приоритет 4: Отходить от врагов, если слаб
    else if (me.hp < 40 && best_enemy != -1 && min_dist_sq < 200*200) {
        desired_dir = to_enemy; // отталкиваемся
        should_rush = me.energy >= 15;
    }
    // Иначе: двигаться к центру умеренно
    else {
        desired_dir = -pos;
        if (dist_to_center_sq > (R * R) / 4) {
            should_rush = me.energy >= 10;
        }
    }

    // === 3. Избегаем столкновений ===
    vec2i avoid(0, 0);
    for (int j = 0; j < N; ++j) {
        if (j == i || !world.agents[j].alive) continue;
        vec2i d(me.x - world.agents[j].x, me.y - world.agents[j].y);
        int dsq = d.SqrMag();
        if (dsq > 0 && dsq < 120*120) {
            avoid += d;
        }
    }
    if (avoid.SqrMag() > 0) {
        desired_dir += avoid;
        should_rush = false; // не рвёмся в толпу
    }

    // === 4. Финализация действия ===
    if (desired_dir.SqrMag() > 0) {
        // Нормализуем до длины ~50 (или 100 при рывке)
        int max_step = should_rush && me.energy >= 2 ? 100 : 50;
        double len = desired_dir.Mag();
        if (len > 0) {
            act.dir = vec2i(static_cast<int>(desired_dir.x / len * max_step),
                            static_cast<int>(desired_dir.y / len * max_step));
        }
    }

    act.use_rush = should_rush && me.energy >= 2;
    act.use_fury = should_fury && me.energy >= 3;

    // Экономим энергию на атаку, если нужно
    if (act.attack && !act.use_fury && me.energy < 3) {
        act.attack = false;
    }

    return act;
}
t_pubg_world::Action dna_by_grok_ml(const t_pubg_world& world, int i) {
    const auto& me = world.agents[i];
    t_pubg_world::Action act{};
    act.use_dir = true;
    act.dir = vec2i(0, 0);
    if (!me.alive) return act;

    // === ML-оптимизированные гиперпараметры (из GA) ===
    const float  CENTER_FRAC      = 0.73f;   // > CF*R > бежим в центр
    const int    RUSH_ENERGY_MIN  = 19;      // минимум энергии для рывка
    const int    FURY_ENEMY_HP    = 52;      // fury, если враг ? HP
    const int    FURY_SELF_HP     = 40;      // fury, если мы ? HP
    const int    CHASE_MAX_SQ     = 82341;   // преследуем, если dist? < это
    const int    AVOID_MIN_SQ     = 21234;   // избегаем, если dist? < это
    const int    RUSH_CHASE_ENERGY= 21;      // рывок при погоне

    const int R = world.R;
    const int N = world.agents.size();
    vec2i pos(me.x, me.y);
    int pos_sq = pos.SqrMag();
    bool in_zone = pos_sq <= R * R;
    int center_threshold_sq = static_cast<int>((R * CENTER_FRAC) * (R * CENTER_FRAC));

    // === 1. Сбор данных о врагах ===
    struct Enemy {
        int id, hp, dsq;
        vec2i dir;
    };
    std::vector<Enemy> enemies;
    int closest_dsq = INT_MAX;
    vec2i to_closest(0,0);
    int weakest_in_range = -1;
    int min_hp_range = 101;
    vec2i to_weak(0,0);
    int weak_dsq = 0;

    for (int j = 0; j < N; ++j) {
        if (j == i || !world.agents[j].alive) continue;
        vec2i d(me.x - world.agents[j].x, me.y - world.agents[j].y);
        int dsq = d.SqrMag();
        int hp = world.agents[j].hp;

        enemies.push_back({j, hp, dsq, d});

        if (dsq < closest_dsq) {
            closest_dsq = dsq;
            to_closest = d;
        }

        if (dsq <= 10000 && hp < min_hp_range) {
            min_hp_range = hp;
            weakest_in_range = j;
            to_weak = d;
            weak_dsq = dsq;
        }
    }

    // === 2. Принятие решений ===
    vec2i move_dir(0, 0);
    bool do_rush = false;
    bool do_fury = false;
    bool do_attack = false;
    int target_id = -1;

    // ПРИОРИТЕТ 1: ВЫЖИТЬ — ЗОНА!
    if (!in_zone || pos_sq > center_threshold_sq) {
        move_dir = -pos;
        do_rush = me.energy >= RUSH_ENERGY_MIN;
    }
    // ПРИОРИТЕТ 2: ДОБИТЬ СЛАБОГО
    else if (weakest_in_range != -1 && me.energy >= 3) {
        do_attack = true;
        target_id = weakest_in_range;
        do_fury = (min_hp_range <= FURY_ENEMY_HP && me.hp >= FURY_SELF_HP) ||
                  (me.energy >= 10 && min_hp_range <= 30);

        if (weak_dsq > 2500) {
            move_dir = -to_weak;
            do_rush = me.energy >= RUSH_CHASE_ENERGY;
        }
    }
    // ПРИОРИТЕТ 3: ПРЕСЛЕДОВАТЬ БЛИЗКОГО
    else if (closest_dsq < CHASE_MAX_SQ && me.hp > 35) {
        move_dir = -to_closest;
        do_attack = closest_dsq <= 10000 && me.energy >= 2;
        do_rush = me.energy >= RUSH_CHASE_ENERGY && closest_dsq > 10000;
    }
    // ПРИОРИТЕТ 4: УБЕГАТЬ, ЕСЛИ СЛАБ
    else if (me.hp < 38 && closest_dsq < 200*200) {
        move_dir = to_closest;
        do_rush = me.energy >= RUSH_ENERGY_MIN;
    }
    // ПО УМОЛЧАНИЮ: к центру
    else {
        move_dir = -pos;
        do_rush = me.energy >= RUSH_ENERGY_MIN && pos_sq > (R*R)/4;
    }

    // === 3. ИЗБЕГАНИЕ СТОЛКНОВЕНИЙ ===
    vec2i avoid(0, 0);
    for (const auto& e : enemies) {
        if (e.dsq > 0 && e.dsq < AVOID_MIN_SQ) {
            avoid += e.dir;
        }
    }
    if (avoid.SqrMag() > 0) {
        move_dir += avoid;
        do_rush = false; // не рвёмся в толпу
    }

    // === 4. ФОРМИРОВАНИЕ ДЕЙСТВИЯ ===
    if (move_dir.SqrMag() > 0) {
        int max_step = do_rush && me.energy >= 2 ? 100 : 50;
        double len = move_dir.Mag();
        if (len > 0) {
            act.dir = vec2i(
                static_cast<int>(move_dir.x / len * max_step),
                static_cast<int>(move_dir.y / len * max_step)
            );
        }
    }

    act.use_rush = do_rush && me.energy >= 2;
    act.use_fury = do_fury && me.energy >= 3;
    act.attack = do_attack && me.energy >= (act.use_fury ? 3 : 2);
    act.target = target_id;

    return act;
}
t_pubg_world::Action dna_by_chatgpt(const t_pubg_world& orig,int i){
    t_pubg_world::Action act;
    if(!qap_check_id(orig.agents,i)) return act;
    const auto& me = orig.agents[i];
    if(!me.alive) return act;

    // Если мы вне круга — первоочередно возвращаемся в центр
    long long dist2 = 1LL*me.x*me.x + 1LL*me.y*me.y;
    long long R2 = 1LL*orig.R*orig.R;
    if(dist2 > R2){
        act.move_to_center = true;
        act.avoid_others = true;
        return act;
    }

    // Найдём ближайшего живого противника
    int best = -1;
    int best_ds = INT_MAX;
    int alive_cnt = 0;
    for(int j=0;j<(int)orig.agents.size();++j){
        if(j==i) continue;
        if(!orig.agents[j].alive) continue;
        ++alive_cnt;
        int dx = me.x - orig.agents[j].x;
        int dy = me.y - orig.agents[j].y;
        int ds = dx*dx + dy*dy;
        if(ds < best_ds){
            best_ds = ds;
            best = j;
        }
    }

    // Если нет врагов — держимся ближе к центру и избегаем толпы
    if(best == -1){
        act.move_to_center = true;
        act.avoid_others = true;
        return act;
    }

    // Поведение при низком HP: отбегаем и пытаемся выжить
    if(me.hp <= 30){
        act.repel_from_enemy = true;
        act.avoid_others = true;
        // также стремимся в центр, чтобы не получить урон от зоны
        if(dist2 > (orig.R-150)*(orig.R-150)) act.move_to_center = true;
        return act;
    }

    // Решения при наличии цели
    const auto& tgt = orig.agents[best];

    // Если цель в радиусе атаки (<=100 units) — атакуем.
    if(best_ds <= 10000){
        act.attack = true;
        act.target = best;

        // Если можем добить fury-ударом — используем его (50 dmg), но только если у нас достаточно HP
        // Fury отнимает 10 HP у исполнителя, потому требуем запас HP > 20
        if(tgt.hp <= 50 && me.hp > 25 && me.energy >= 3){
            act.use_fury = true;
            act.use_rush = false;
        } else {
            // Стандартная атака, но если цель далеко в пределах досягаемости и у нас мало энергии — не использовать fury
            act.use_fury = false;
            // если цель близка и у нас энергии мало — не тратить rush
            act.use_rush = false;
        }

        // Если рядом много агентов — избегаем столкновений
        act.avoid_others = true;
        return act;
    }

    // Если цель далеко — преследуем её. Используем рывок если есть энергия.
    act.chase_enemy = true;
    if(me.energy >= 4) act.use_rush = true;
    act.avoid_others = true;

    // Если много ближних союзников/врагов — немного отталкиваемся, чтобы избежать фрага от окружения
    // (репел — будет использован движением/логикой мира при необходимости)
    // Оставим move_to_center=false чтобы не конфликтовать с преследованием.

    return act;
}
t_pubg_world::Action dna_by_gemini(const t_pubg_world&orig,int i){
    t_pubg_world::Action act;
    const t_pubg_world::Agent& self = orig.agents[i];
    
    // Всегда стремимся к центру, чтобы не умереть от зоны,
    // но это будет переопределено погоней/отступлением, если они активны.
    act.move_to_center = true; 
    
    // --- 1. Найти ближайшего живого врага и расстояние до него ---
    int closest_dist_sq = 100000000;
    int closest_j = -1;
    for (int j = 0; j < (int)orig.agents.size(); ++j) {
        if (i == j || !orig.agents[j].alive) continue;
        const t_pubg_world::Agent& enemy = orig.agents[j];
        int dx = self.x - enemy.x;
        int dy = self.y - enemy.y;
        int dist_sq = dx*dx + dy*dy;
        if (dist_sq < closest_dist_sq) {
            closest_dist_sq = dist_sq;
            closest_j = j;
        }
    }
    
    bool found_enemy = closest_j != -1;
    int ATTACK_RANGE_SQ = 100 * 100; // 10000

    if (found_enemy) {
        const t_pubg_world::Agent& enemy = orig.agents[closest_j];

        // --- 2. Стратегия атаки/отступления ---
        
        // Условие для Яростной атаки (макс. урон, но стоит HP)
        // Требуется достаточно HP и энергии для Ярости (минимум 3)
        bool can_fury_attack = self.energy >= 3 && self.hp > 30; // 30 - безопасный порог
        
        // Отступаем, если HP слишком низкое, даже если есть энергия
        bool need_to_repel = self.hp <= 30;
        
        if (closest_dist_sq <= ATTACK_RANGE_SQ && can_fury_attack) {
            // Атака Яростью, если враг близко и можно себе это позволить
            act.attack = true;
            act.use_fury = true; 
            act.target = closest_j; // Цель для атаки
            act.move_to_center = false; // Отменяем движение к центру в пользу атаки
            
            // Если в радиусе атаки - не используем рывок для экономии энергии
            act.use_rush = false; 
            
        } else if (closest_dist_sq > ATTACK_RANGE_SQ && !need_to_repel && self.energy >= 2) {
            // Преследование и Рывок, если враг далеко и есть энергия
            act.chase_enemy = true;
            act.use_rush = true; // Быстрое сближение (тратит 2 энергии)
            act.move_to_center = false;
            
        } else if (need_to_repel) {
            // Отступление (repel)
            act.repel_from_enemy = true;
            act.move_to_center = false;
            act.use_rush = self.energy >= 2; // Рывок для отступления, если есть энергия
            act.avoid_others = true; // Избегать других агентов
            
        } else {
            // Дефолтное движение (к центру или простое движение)
            // act.move_to_center = true; // Уже установлено
            // Добавляем избегание, чтобы не попасть в окружение, пока не готовы к атаке
            act.avoid_others = true;
        }
    } else {
        // Если нет врагов - просто идем к центру и экономим энергию/восстанавливаем HP
        // act.move_to_center = true; // Уже установлено
        act.use_rush = false; // Экономим энергию, т.к. нет цели
        act.avoid_others = true; // На всякий случай
    }

    return act;
}
t_pubg_world::Action dna_by_qwen_coder(const t_pubg_world& orig, int i) {
    t_pubg_world::Action act = {};
    auto& me = orig.agents[i];
    
    if (!me.alive) return act;

    // Найти ближайшего врага
    int closest_dist_sq = 100000000;
    int dx_to_closest = 0, dy_to_closest = 0;
    bool found_enemy = false;
    for (int j = 0; j < (int)orig.agents.size(); ++j) {
        if (i == j || !orig.agents[j].alive) continue;
        int dx = me.x - orig.agents[j].x;
        int dy = me.y - orig.agents[j].y;
        int dist_sq = dx*dx + dy*dy;
        if (dist_sq < closest_dist_sq) {
            closest_dist_sq = dist_sq;
            dx_to_closest = dx;
            dy_to_closest = dy;
            found_enemy = true;
        }
    }

    // Приоритет выживания: если HP низкий или вне зоны
    bool in_safe_zone = (me.x*me.x + me.y*me.y <= orig.R*orig.R);
    bool low_hp = (me.hp < 50);
    
    if (!in_safe_zone || low_hp) {
        // Двигаться к центру, использовать рывок если далеко
        act.move_to_center = true;
        if (me.x*me.x + me.y*me.y > (orig.R-200)*(orig.R-200)) {
            act.use_rush = true;
        }
        return act;
    }

    // Если рядом враг и здоровье позволяет - атаковать
    if (found_enemy && closest_dist_sq <= 10000 && me.energy >= 2) {
        act.attack = true;
        act.use_fury = (me.hp > 70 && me.energy >= 3); // использовать fury если много HP
        if (found_enemy) {
            // Найти врага в радиусе атаки и установить как цель
            for (int j = 0; j < (int)orig.agents.size(); ++j) {
                if (i == j || !orig.agents[j].alive) continue;
                int dx = me.x - orig.agents[j].x;
                int dy = me.y - orig.agents[j].y;
                if (dx*dx + dy*dy <= 10000) {
                    act.target = j;
                    break;
                }
            }
        }
        return act;
    }

    // Если враг рядом, но не в радиусе атаки - преследовать
    if (found_enemy && closest_dist_sq <= 250000) {
        act.chase_enemy = true;
        act.use_rush = (closest_dist_sq > 50000 && me.energy >= 2);
        return act;
    }

    // Иначе - избегать других, двигаться к центру
    act.avoid_others = true;
    act.move_to_center = true;
    act.use_rush = (me.energy > 50); // использовать рывок если много энергии
    
    return act;
}
t_pubg_world::Action dna_by_qwen_coder_v2(const t_pubg_world& orig, int i) {
    t_pubg_world::Action act = {};
    auto& me = orig.agents[i];
    
    if (!me.alive) return act;

    // Найти ближайшего врага и посчитать общее количество врагов вблизи
    int closest_dist_sq = 100000000;
    int dx_to_closest = 0, dy_to_closest = 0;
    bool found_enemy = false;
    int enemies_nearby = 0;
    int closest_enemy_idx = -1;
    
    for (int j = 0; j < (int)orig.agents.size(); ++j) {
        if (i == j || !orig.agents[j].alive) continue;
        int dx = me.x - orig.agents[j].x;
        int dy = me.y - orig.agents[j].y;
        int dist_sq = dx*dx + dy*dy;
        if (dist_sq < closest_dist_sq) {
            closest_dist_sq = dist_sq;
            dx_to_closest = dx;
            dy_to_closest = dy;
            found_enemy = true;
            closest_enemy_idx = j;
        }
        if (dist_sq <= 200*200) {
            enemies_nearby++;
        }
    }

    // Приоритет выживания: если HP низкий или вне зоны
    bool in_safe_zone = (me.x*me.x + me.y*me.y <= orig.R*orig.R);
    bool low_hp = (me.hp < 40);
    bool low_energy = (me.energy < 20);
    
    // Если вне зоны - срочно двигаться к центру
    if (!in_safe_zone) {
        act.move_to_center = true;
        act.use_rush = (me.energy >= 2);
        return act;
    }

    // Если мало HP - избегать врагов, двигаться к центру
    if (low_hp) {
        act.move_to_center = true;
        act.avoid_others = true;
        act.use_rush = (me.energy >= 2 && (me.x*me.x + me.y*me.y) > (orig.R-150)*(orig.R-150));
        return act;
    }

    // Если враг в радиусе атаки
    if (found_enemy && closest_dist_sq <= 10000) {
        // Если врагов много рядом или мое HP не намного больше, использовать fury осторожно
        bool should_use_fury = (me.energy >= 3 && me.hp > 60 && 
                               (enemies_nearby == 1 || (closest_enemy_idx != -1 && orig.agents[closest_enemy_idx].hp < me.hp - 30)));
        
        act.attack = true;
        act.use_fury = should_use_fury;
        act.target = closest_enemy_idx;
        return act;
    }

    // Если враг рядом (но не в радиусе атаки) - атаковать или преследовать
    if (found_enemy && closest_dist_sq <= 22500) { // 150^2
        // Если энергии много и враг слаб - преследовать с рывком
        if (me.energy > 30 && (!low_hp || orig.agents[closest_enemy_idx].hp < 40)) {
            act.chase_enemy = true;
            act.use_rush = (me.energy >= 2);
        } else {
            // Если слаб или мало энергии - отступить
            act.repel_from_enemy = true;
        }
        return act;
    }

    // Если врагов много рядом - избегать
    if (enemies_nearby > 2) {
        act.avoid_others = true;
        act.use_rush = (me.energy >= 2 && enemies_nearby > 3);
        return act;
    }

    // Иначе - двигаться к центру, использовать рывок если далеко и энергии много
    act.move_to_center = true;
    act.use_rush = (me.energy > 70 && (me.x*me.x + me.y*me.y) > (orig.R-100)*(orig.R-100));
    
    return act;
}
t_pubg_world::Action dna_by_qwen_coder_v3(const t_pubg_world& orig, int i) {
    t_pubg_world::Action act = {};
    auto& me = orig.agents[i];
    
    if (!me.alive) return act;

    // Найти ближайшего врага и оценить угрозу
    int closest_dist_sq = 100000000;
    int dx_to_closest = 0, dy_to_closest = 0;
    bool found_enemy = false;
    int enemies_in_attack_range = 0;
    int enemies_in_chase_range = 0;
    int closest_enemy_idx = -1;
    int total_enemy_hp = 0;
    
    for (int j = 0; j < (int)orig.agents.size(); ++j) {
        if (i == j || !orig.agents[j].alive) continue;
        int dx = me.x - orig.agents[j].x;
        int dy = me.y - orig.agents[j].y;
        int dist_sq = dx*dx + dy*dy;
        
        if (dist_sq < closest_dist_sq) {
            closest_dist_sq = dist_sq;
            dx_to_closest = dx;
            dy_to_closest = dy;
            found_enemy = true;
            closest_enemy_idx = j;
        }
        
        if (dist_sq <= 10000) { // В радиусе атаки
            enemies_in_attack_range++;
            total_enemy_hp += orig.agents[j].hp;
        } else if (dist_sq <= 22500) { // В радиусе преследования
            enemies_in_chase_range++;
            total_enemy_hp += orig.agents[j].hp;
        }
    }

    // Оценка общей угрозы
    bool high_threat = (enemies_in_attack_range > 0 || (enemies_in_chase_range > 1 && me.hp < 70));
    bool very_high_threat = (enemies_in_attack_range > 1 || (enemies_in_attack_range == 1 && orig.agents[closest_enemy_idx].hp > me.hp));
    bool safe_condition = (me.hp > 60 && me.energy > 50 && enemies_in_chase_range == 0);

    // Приоритет 1: Выживание - если вне зоны, срочно двигаться к центру
    bool in_safe_zone = (me.x*me.x + me.y*me.y <= orig.R*orig.R);
    if (!in_safe_zone) {
        act.move_to_center = true;
        act.use_rush = (me.energy >= 2);
        return act;
    }

    // Приоритет 2: Если высокая угроза, избегать или отступать
    if (very_high_threat) {
        act.repel_from_enemy = true;
        act.avoid_others = true;
        act.use_rush = (me.energy >= 2);
        return act;
    }

    if (high_threat) {
        // Если враг в атакующем радиусе и мы сильнее, контратаковать
        if (enemies_in_attack_range > 0 && me.hp > orig.agents[closest_enemy_idx].hp + 20 && me.energy >= 2) {
            act.attack = true;
            act.use_fury = (me.hp > 70 && me.energy >= 3 && orig.agents[closest_enemy_idx].hp > 40);
            act.target = closest_enemy_idx;
            return act;
        } else {
            // Иначе - отступить
            act.repel_from_enemy = true;
            return act;
        }
    }

    // Приоритет 3: Атаковать, если безопасно
    if (found_enemy && closest_dist_sq <= 10000 && safe_condition) {
        act.attack = true;
        // Использовать fury, если враг сильный, а мы в хорошем состоянии
        act.use_fury = (me.energy >= 3 && orig.agents[closest_enemy_idx].hp > 50 && me.hp > 80);
        act.target = closest_enemy_idx;
        return act;
    }

    // Приоритет 4: Преследовать врага, если безопасно и есть энергия
    if (found_enemy && closest_dist_sq <= 22500 && safe_condition) {
        act.chase_enemy = true;
        act.use_rush = (me.energy >= 2 && closest_dist_sq > 10000);
        return act;
    }

    // Приоритет 5: Позиционирование
    // Если здоровье и энергия высоки, можно агрессивно двигаться к центру или преследовать
    if (safe_condition) {
        act.move_to_center = true;
        act.use_rush = (me.energy > 80); // Использовать рывок если энергии очень много
    } else {
        // Иначе - осторожное движение к центру
        act.move_to_center = true;
        act.avoid_others = (enemies_in_chase_range > 0);
    }

    return act;
}
t_pubg_world::Action dna_by_gpt5(const t_pubg_world& orig, int i){
  t_pubg_world::Action act;
  const auto& me = orig.agents[i];

  act.use_dir = true;
  act.dir = vec2i(0,0);
  act.target = -1;

  // Поиск ближайшего врага и цели в радиусе атаки
  int closest_idx = -1;
  int closest_d2  = 2147483647;
  vec2i away(0,0);

  int best_in_range = -1;
  int best_in_range_hp = 1000000000;

  for (int j = 0; j < (int)orig.agents.size(); ++j){
    if (j == i) continue;
    const auto& e = orig.agents[j];
    if (!e.alive) continue;

    int dx = me.x - e.x;
    int dy = me.y - e.y;
    int d2 = dx*dx + dy*dy;

    if (d2 < closest_d2){
      closest_d2 = d2;
      closest_idx = j;
      away = vec2i(dx,dy);
    }

    if (d2 <= 10000){ // <= 100 ед.
      if (e.hp < best_in_range_hp){
        best_in_range_hp = e.hp;
        best_in_range = j;
      }
    }

    // Доп. раздвигание, если подбираются ближе 150
    if (d2 < 150*150){
      act.dir.x += dx;
      act.dir.y += dy;
    }
  }

  // Работа с границей круга
  long long R = orig.R;
  long long x = me.x, y = me.y;
  long long d2center = x*x + y*y;
  long long safeR = R - 60; if (safeR < 100) safeR = 100;

  if (d2center > safeR*safeR){
    // Сильнее тянемся к центру, можно ускориться
    act.dir.x += int(-me.x*2);
    act.dir.y += int(-me.y*2);
    if (me.energy >= 2) act.use_rush = true;
  } else {
    // Лёгкое притяжение к центру
    act.dir.x += int(-me.x/4);
    act.dir.y += int(-me.y/4);
  }

  // Если враг слишком близко — уходим (китим)
  if (closest_idx != -1 && closest_d2 < 200*200){
    act.dir.x += away.x * 2;
    act.dir.y += away.y * 2;
    if (me.energy >= 2) act.use_rush = true;
  }

  // Решение об атаке
  if (best_in_range != -1 && me.energy >= 2){
    act.attack = true;
    act.target = best_in_range;

    // Добивание фурией при шансе на фраг
    if (best_in_range_hp <= 50 && me.energy >= 3 && me.hp > 12){
      act.use_fury = true;
    }

    // В ход атаки не тратим лишнюю энергию на перемещение
    act.use_rush = false;
    act.dir = vec2i(0,0);
  } else {
    // Без атаки — мягко избегаем скученности
    act.avoid_others = true;
  }

  // Если всё тихо и мы глубоко в зоне — стоим, экономим энергию
  if (closest_d2 > 250*250 && d2center < (R - 200) * (long long)(R - 200)){
    act.use_rush = false;
    act.dir = vec2i(0,0);
  }

  return act;
}
t_pubg_world::Action dna_by_deepseek_v2(const t_pubg_world& orig, int i) {
    t_pubg_world::Action action;
    
    // Определяем нашего агента по стратегии 31456 (а не по индексу!)
    bool is_our_agent = true;
    /*uint64_t strategy_value = *reinterpret_cast<const uint64_t*>(&orig.agents[i].strat);
    if (strategy_value == 31456) {
        is_our_agent = true;
    }*/
    
    if (is_our_agent) {
        // АГРЕССИВНАЯ СТРАТЕГИЯ ДЛЯ МАКСИМИЗАЦИИ СЧЁТА
        action.attack = true;
        action.use_fury = true;
        action.use_rush = true;
        action.chase_enemy = true;
        action.move_to_center = false;
        action.repel_from_enemy = false;
        action.avoid_others = false;
        action.use_dir = false;
        
        // Находим самого слабого врага для быстрого убийства
        int weakest_enemy = -1;
        int min_hp = 1000;
        int closest_dist_sq = 100000000;
        
        for (int j = 0; j < (int)orig.agents.size(); ++j) {
            if (i == j || !orig.agents[j].alive) continue;
            
            int dx = orig.agents[i].x - orig.agents[j].x;
            int dy = orig.agents[i].y - orig.agents[j].y;
            int dist_sq = dx*dx + dy*dy;
            
            // Приоритет слабым врагам для быстрых убийств
            if (orig.agents[j].hp < min_hp || 
                (orig.agents[j].hp == min_hp && dist_sq < closest_dist_sq)) {
                min_hp = orig.agents[j].hp;
                weakest_enemy = j;
                closest_dist_sq = dist_sq;
            }
        }
        
        if (weakest_enemy != -1) {
            action.target = weakest_enemy;
            
            // Направленное движение к цели
            int dx = orig.agents[weakest_enemy].x - orig.agents[i].x;
            int dy = orig.agents[weakest_enemy].y - orig.agents[i].y;
            
            if (dx != 0 || dy != 0) {
                double len = std::sqrt(dx*dx + dy*dy);
                if (len > 0) {
                    action.use_dir = true;
                    action.dir.x = static_cast<int>((dx / len) * 100);
                    action.dir.y = static_cast<int>((dy / len) * 100);
                }
            }
        }
        
        // АДАПТИВНАЯ ЛОГИКА ВЫЖИВАНИЯ
        auto& me = orig.agents[i];
        
        // При критическом HP - отступаем и лечимся
        if (me.hp < 25) {
            action.use_fury = false;
            action.attack = false;
            action.chase_enemy = false;
            action.move_to_center = true;
            action.avoid_others = true;
        }
        // При среднем HP - умеренная агрессия
        else if (me.hp < 50) {
            action.use_fury = (me.energy > 40);
        }
        
        // Управление энергией
        if (me.energy < 15) {
            action.use_fury = false;
            action.use_rush = false;
        } else if (me.energy < 30) {
            action.use_fury = false;
        }
        
        // Избегание зоны урона
        int dist_to_center_sq = me.x*me.x + me.y*me.y;
        if (dist_to_center_sq > orig.R*orig.R * 0.36) { // 60% радиуса
            action.move_to_center = true;
            action.chase_enemy = false;
        }
        
    } else {
        // Для других агентов - случайная стратегия (чтобы не мешали нашему агенту)
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, 1);
        
        action.attack = dist(rng);
        action.chase_enemy = dist(rng);
        action.move_to_center = dist(rng);
        action.use_fury = false;
        action.use_rush = false;
        action.avoid_others = true;
        action.use_dir = false;
    }
    
    return action;
}
//https://ideone.com/m8KiHV

double evaluate_simple_impl(const t_pubg_world& sim, int my_id);
double evaluate_world_v2(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    const auto& me_before = world_before.agents[my_id];
    const auto& me_after = world_after.agents[my_id];
    return evaluate_simple_impl(world_after,my_id);
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
    auto&me=me_after;
    score = 1.15*0.125*125*(/*me.energy*0.075+*/me.ticks_survived*0.5 + me.damage_dealt * 0.314*0.25 + me.frags * 50 +me.hp*2.5);
    // 2. Штраф за нахождение в зоне урона
    int R = world_after.R;
    if (me_after.x * me_after.x + me_after.y * me_after.y > R * R) {
        score -= 3.0; // сильный штраф
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
            score += bonus*25.5;
        } else {
            // Враг сильнее или равен — хочется быть дальше
            // Штраф обратно пропорционален расстоянию
            double penalty = (enemy.hp - my_hp + 1) * 5.0 / dist;
            score -= penalty*10;
        }
    }

    // 4. Избегание центра на ранних тиках (если не агрессивный)
    int tick = world_after.tick;
    double center_dist = std::sqrt(my_x * my_x + my_y * my_y);
    if (tick < 50 && center_dist < 300) { // 300 = 30.0 в реальных координатах
        // На ранних тиках центр = скопление врагов > штраф
        score -= (50 - tick) * 0.5*0.00125; // чем раньше, тем больше штраф
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
            score += 5;
        }
    }

    return score;
}
t_pubg_world::Action encode_action_v2(uint64_t c,int target=-1,bool use_dir=false,vec2i dir={}) {
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
  auto r=sqrt(dir.x*dir.x+dir.y*dir.y);
  auto k=!r?0:100.0/r;
  a.dir=vec2i(dir.x*k,dir.y*k);
  return a;
}
t_pubg_world::Action dna_by_adler(const t_pubg_world&orig,int i){
  auto w=orig;
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
  double best_score_a1 = -1e9;
  t_pubg_world::Action best_action_a1;
  for(int dx=-1;dx<=+1;dx++)for(int dy=-1;dy<=+1;dy++)
  for(int eid=0;eid<earr.size();eid++)
  for (uint64_t c1 = 0; c1 < 8; ++c1) {
      auto a=encode_action_v2(c1,earr[eid],true,vec2i(dx,dy));
      if(a.avoid_others)continue;
      auto world1 = orig;
      world1.agents[i].a=a;
      world1.step();

      if (!world1.agents[i].alive) {
          double score = -10000;
          if (score > best_score_a1) {
              best_score_a1 = score;
              best_action_a1 = encode_action_v2(c1,earr[eid],true,vec2i(dx,dy));
          }
          continue;
      }
      double best_score_a2 = -1e9;
      for(int dx=-1;dx<=+1;dx++)for(int dy=-1;dy<=+1;dy++)
      for (uint8_t c2 = 0; c2 < 8; ++c2) {
          auto a=encode_action_v2(c2,earr[eid],true,vec2i(dx,dy));
          if(a.avoid_others)continue;
          auto world2 = world1;
          world2.agents[i].a=a;
          world2.step();

          double score = evaluate_world_v2(w,world2,i);
          if (score > best_score_a2) {
              best_score_a2 = score;
          }
      }
      if (best_score_a2 > best_score_a1) {
          best_score_a1 = best_score_a2;
          best_action_a1 = encode_action_v2(c1,earr[eid],true,vec2i(dx,dy));
      }
  }
  return best_action_a1;
}

double evaluate_simple(const t_pubg_world& orig, int my_id, const t_pubg_world::Action& action) {
    t_pubg_world sim = orig;
    sim.agents[my_id].a = action;
    sim.step(true);
    double evaluate_simple_impl(const t_pubg_world& sim, int my_id);
    return evaluate_simple_impl(sim,my_id);
};
double evaluate_simple_impl(const t_pubg_world& sim, int my_id) {
    auto& me = sim.agents[my_id];
    if (!me.alive) return -1e9;
    
    // Основной scoring из симулятора
    double score = 125*(me.ticks_survived*0.5 + me.damage_dealt * 0.314*0.25 + me.frags * 50 +me.hp*2.5);
    
    // Бонус за приближение к слабым врагам
    int weakest_enemy_hp = 1000;
    int dist_to_weakest_sq = 100000000;
    
    for (int j = 0; j < (int)sim.agents.size(); ++j) {
        if (j == my_id || !sim.agents[j].alive) continue;
        
        if (sim.agents[j].hp < weakest_enemy_hp) {
            weakest_enemy_hp = sim.agents[j].hp;
            int dx = me.x - sim.agents[j].x;
            int dy = me.y - sim.agents[j].y;
            dist_to_weakest_sq = dx*dx + dy*dy;
        }
    }
    
    // Большой бонус за близость к слабому врагу
    if (weakest_enemy_hp < 1000) {
        double dist_bonus = 0.5*1000.0 / (1.0 + std::sqrt(dist_to_weakest_sq));
        score += dist_bonus;
        
    }
    
    return score;
}
t_pubg_world::Action dna_by_deep_seek_with_sim(const t_pubg_world& orig, int i) {
  t_pubg_world::Action dna(const t_pubg_world& orig, int i);
  return dna(orig,i);
}	
t_pubg_world::Action dna(const t_pubg_world& orig, int i) {
    if (!orig.agents[i].alive) return {};
    
    double best_score = -1e9;
    t_pubg_world::Action best_action;
    
    // Перебираем все комбинации флагов
    //for (int attack = 0; attack <= 1; attack++)
    {
        for (int use_fury = 0; use_fury <= 1; use_fury++) {
            for (int use_rush = 0; use_rush <= 1; use_rush++) {
                for (int chase_enemy = 0; chase_enemy <= 1; chase_enemy++) {
                    for (int move_to_center = 0; move_to_center <= 1; move_to_center++) {
                        for (int avoid_others = 0; avoid_others <= 1; avoid_others++) {
                            
                            // Пропускаем противоречивые комбинации
                            if (move_to_center && chase_enemy) continue;
                            
                            t_pubg_world::Action action;
                            action.attack = true;//attack;
                            action.use_fury = use_fury;
                            action.use_rush = use_rush;
                            action.chase_enemy = chase_enemy;
                            action.move_to_center = move_to_center;
                            action.avoid_others = avoid_others;
                            action.repel_from_enemy = !move_to_center;
                            
                            // Тестируем 8 основных направлений
                            std::vector<vec2i> dirs = {
                                {100,0}, {-100,0}, {0,100}, {0,-100},
                                {70,70}, {70,-70}, {-70,70}, {-70,-70}
                            };
                            
                            for (const auto& dir : dirs) {
                                action.use_dir = true;
                                action.dir = dir;
                                
                                double score = evaluate_simple(orig, i, action);
                                if (score > best_score) {
                                    best_score = score;
                                    best_action = action;
                                }
                            }
                            
                            // Тестируем без направления
                            action.use_dir = false;
                            double score = evaluate_simple(orig, i, action);
                            if (score > best_score) {
                                best_score = score;
                                best_action = action;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return best_action;
}
t_pubg_world::Action dna_by_alice(const t_pubg_world& orig, int i) {
    t_pubg_world::Action act;
    const auto& agent = orig.agents[i];

    // 1. Если HP низкое — отступаем от врагов и идём к центру (безопаснее)
    if (agent.hp < 30) {
        act.repel_from_enemy = true;
        act.move_to_center = true;
        act.attack = false;  // не атакуем, экономим энергию
        return act;
    }

    // 2. Ищем ближайшего живого врага
    int closest_dist_sq = 100000000;
    int dx_to_closest = 0, dy_to_closest = 0;
    int closest_enemy_idx = -1;  // сохраняем индекс ближайшего врага
    bool found_enemy = false;

    for (int j = 0; j < (int)orig.agents.size(); ++j) {
        if (i == j || !orig.agents[j].alive) continue;
        int dx = agent.x - orig.agents[j].x;
        int dy = agent.y - orig.agents[j].y;
        int dist_sq = dx * dx + dy * dy;

        if (dist_sq < closest_dist_sq) {
            closest_dist_sq = dist_sq;
            dx_to_closest = dx;
            dy_to_closest = dy;
            closest_enemy_idx = j;
            found_enemy = true;
        }
    }

    // 3. Если враг близко — атакуем
    if (found_enemy && closest_dist_sq <= 10000) {  // дистанция атаки (100^2)
        act.attack = true;
        act.target = closest_enemy_idx;  // указываем конкретного врага

        // Если у врага мало HP — используем ярость для добивания
        if (orig.agents[closest_enemy_idx].hp <= 50) {
            act.use_fury = true;  // +50 урона, но -10 HP и -3 энергии
        } else {
            act.use_fury = false;
        }

        // Не отступаем, если атакуем
        act.repel_from_enemy = false;
    } else {
        // 4. Если врага нет близко — идём к центру и ищем цель
        act.move_to_center = true;
        act.chase_enemy = true;  // если вдруг появится враг
        act.attack = false;
    }

    // 5. Избегаем столкновений с другими агентами
    act.avoid_others = true;

    // 6. Используем рывок, только если нужно быстро добраться до врага или уйти от зоны
    if (found_enemy && closest_dist_sq > 25000 && agent.energy >= 2) {
        act.use_rush = true;  // быстрее двигаемся
    } else {
        act.use_rush = false;
    }

    return act;
}
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//1524.36
double evaluate_world_v1_by_perplexity(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    const auto& before = world_before.agents[my_id];
    const auto& after = world_after.agents[my_id];

    if (!after.alive) return -10000.0;

    double score = 0.0;
    score += (after.ticks_survived - before.ticks_survived) * 1.0;
    score += (after.damage_dealt - before.damage_dealt) * 0.314;
    score += (after.frags - before.frags) * 50.0;
    score += (after.energy - before.energy) * 0.1;
    score += (after.hp - before.hp) * 0.5;

    int dist_before_sq = before.x * before.x + before.y * before.y;
    int dist_after_sq = after.x * after.x + after.y * after.y;
    int R = world_after.R;

    if (dist_after_sq < R * R) {
        score += 5.0;
    } else {
        score -= 5.0;
    }

    double dist_before = std::sqrt(static_cast<double>(dist_before_sq));
    double dist_after = std::sqrt(static_cast<double>(dist_after_sq));
    if (dist_after < dist_before) {
        score += 2.0;
    } else {
        score -= 2.0;
    }

    return score;
}
//1442.93
double evaluate_world_v1_by_grok(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (!qap_check_id(world_after.agents, my_id)) return -1e9;
    const auto& me_before = world_before.agents[my_id];
    const auto& me_after = world_after.agents[my_id];

    if (!me_after.alive) return -1e9;  // Dead = worst

    double score = 0.0;

    // 1. Survival bonus (ticks survived)
    score += me_after.ticks_survived * 2.0;

    // 2. HP (scaled)
    score += me_after.hp * 3.0;

    // 3. Energy (important for movement and attack)
    score += me_after.energy * 0.5;

    // 4. Frags (huge reward)
    score += me_after.frags * 500.0;

    // 5. Damage dealt (encourage aggression)
    score += me_after.damage_dealt * 1.0;

    // 6. Zone safety: penalize being outside
    int dist_sq = me_after.x * me_after.x + me_after.y * me_after.y;
    int R = world_after.R;
    if (dist_sq > R * R) {
        double excess = sqrt(dist_sq) - R;
        score -= excess * 10.0;  // heavy penalty
    } else {
        // Bonus for being deep inside safe zone
        double safety = (R - sqrt(dist_sq)) / float(R);
        score += safety * 50.0;
    }

    // 7. Enemy threat assessment: count nearby alive enemies
    int nearby_enemies = 0;
    int closest_enemy_dist = 1e9;
    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (j == my_id || !world_after.agents[j].alive) continue;
        int dx = me_after.x - world_after.agents[j].x;
        int dy = me_after.y - world_after.agents[j].y;
        int d2 = dx*dx + dy*dy;
        if (d2 <= 300*300) nearby_enemies++;
        if (d2 < closest_enemy_dist) closest_enemy_dist = d2;
    }

    // Prefer to be close to enemies if HP and energy allow attack
    if (me_after.hp >= 60 && me_after.energy >= 3 && closest_enemy_dist <= 100*100) {
        score += (10000.0 - closest_enemy_dist) / 10000.0 * 100.0;  // attack opportunity
    }

    // 8. Encourage eliminating weak enemies
    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (j == my_id || !world_after.agents[j].alive) continue;
        const auto& e = world_after.agents[j];
        if (e.hp <= 50) {
            int dx = me_after.x - e.x;
            int dy = me_after.y - e.y;
            int d2 = dx*dx + dy*dy;
            if (d2 <= 200*200) {
                score += (50.0 - e.hp) * 2.0;  // bonus for being near killable target
            }
        }
    }

    // 9. Bonus for central position (anticipate zone shrink)
    double center_dist = sqrt(dist_sq);
    score -= center_dist * 0.01;  // slight pull toward center

    // 10. Compare to before: reward improvement
    score += (me_after.hp - me_before.hp) * 5.0;
    score += (me_after.energy - me_before.energy) * 0.3;
    score += (me_after.frags - me_before.frags) * 600.0;
    score += (me_after.damage_dealt - me_before.damage_dealt) * 1.2;

    return score;
}
//1028.38
double evaluate_world_v1_by_deepseek(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (!qap_check_id(world_after.agents, my_id) || !world_after.agents[my_id].alive) {
        return -10000.0; // Сильный штраф за смерть
    }

    const auto& agent_before = world_before.agents[my_id];
    const auto& agent_after = world_after.agents[my_id];
    
    double score = 0.0;
    
    // Выживание - самый важный фактор
    score += agent_after.ticks_survived * 10.0;
    
    // Здоровье - критически важно
    score += agent_after.hp * 100.0;
    
    // Энергия - позволяет действовать
    score += agent_after.energy * 5.0;
    
    // Убийства и урон - агрессивная стратегия
    score += agent_after.frags * 500.0;
    score += agent_after.damage_dealt * 15.0;
    
    // Позиционирование - оставаться ближе к центру
    int dist_to_center = agent_after.x * agent_after.x + agent_after.y * agent_after.y;
    int safe_zone_radius = world_after.R * world_after.R;
    
    if (dist_to_center > safe_zone_radius) {
        score -= (dist_to_center - safe_zone_radius) * 0.01; // Штраф за нахождение вне зоны
    } else {
        score += (safe_zone_radius - dist_to_center) * 0.005; // Бонус за близость к центру
    }
    
    // Бонус за уменьшение количества живых врагов
    int enemies_before = 0;
    int enemies_after = 0;
    
    for (int i = 0; i < world_before.agents.size(); ++i) {
        if (i != my_id && world_before.agents[i].alive) enemies_before++;
        if (i != my_id && world_after.agents[i].alive) enemies_after++;
    }
    
    score += (enemies_before - enemies_after) * 300.0; // Бонус за убитых врагов
    
    // Штраф за потерю здоровья
    int hp_loss = agent_before.hp - agent_after.hp;
    if (hp_loss > 0) {
        score -= hp_loss * 200.0;
    }
    
    // Бонус за атаки в этом тике
    if (agent_after.attacked_this_tick) {
        score += 50.0;
    }
    
    // Штраф за использование Fury (самоурон)
    if (agent_after.hp < agent_before.hp && agent_after.attacked_this_tick) {
        // Проверяем, не был ли это самоурон от Fury
        bool took_damage_from_fury = (agent_before.hp - agent_after.hp == 10);
        if (took_damage_from_fury) {
            score -= 100.0;
        }
    }
    
    return score;
}
//1412.31
double calculate_raw_score(const t_pubg_world::Agent& agent) {
    // Формула счета из t_pubg_world::step()
    return (double)agent.ticks_survived + 
           (double)agent.damage_dealt * 0.314 + 
           (double)agent.frags * 50.0;
}
double evaluate_world_v1_by_gemini(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (!qap_check_id(world_after.agents, my_id)) {
        // Если агент не существует, или это не должно произойти в логике
        return -1e9;
    }

    const auto& agent_before = world_before.agents[my_id];
    const auto& agent_after = world_after.agents[my_id];

    if (!agent_after.alive) {
        // Если агент умер на этом шаге, это очень плохо.
        return -10000.0;
    }
    
    // Вычисляем изменение raw-счета за один шаг.
    // Это максимизирует ticks_survived (1 за тик), damage_dealt (x0.314), frags (x50).
    // Так как симуляция в dna_by_adler идет на 2 шага, это будет 2-шаговое изменение.
    double score_after = calculate_raw_score(agent_after);
    double score_before = calculate_raw_score(agent_before);

    double delta_score = score_after - score_before;

    // Дополнительные бонусы/штрафы, которые могут быть важны для выживания:
    // 1. Здоровье: чем больше, тем лучше.
    double hp_bonus = (agent_after.hp - agent_before.hp) * 0.1;

    // 2. Энергия: позволяет делать действия.
    double energy_bonus = (agent_after.energy - agent_before.energy) * 0.05;

    // 3. Близость к центру (R): нахождение в зоне выгодно.
    // R - радиус зоны, уменьшается со временем. Позиция (x,y).
    // Дистанция до центра: dist_sq. Урон при dist_sq > R*R.
    double dist_sq_after = (double)agent_after.x * agent_after.x + (double)agent_after.y * agent_after.y;
    double R_sq = (double)world_after.R * world_after.R;
    double zone_proximity_bonus = (R_sq - dist_sq_after) * 0.00001; // Чем ближе к центру, тем выше бонус.
    
    // 4. Наличие врагов: важно, чтобы они были в радиусе атаки (100^2 = 10000).
    int enemies_in_range = 0;
    double closest_dist_sq = 1e9;
    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (my_id == j || !world_after.agents[j].alive) continue;
        int dx = agent_after.x - world_after.agents[j].x;
        int dy = agent_after.y - world_after.agents[j].y;
        int dist_sq = dx*dx + dy*dy;
        if (dist_sq <= 10000) {
            enemies_in_range++;
        }
        closest_dist_sq = min(closest_dist_sq, (double)dist_sq);
    }
    // Бонус за возможность атаковать
    double attack_range_bonus = enemies_in_range * 1.0; 
    
    // Учитывая, что dna_by_adler ищет "локальный" максимум за 2 тика, 
    // необходимо, чтобы итоговый счет был достаточно велик, чтобы выбраться 
    // из локальных минимумов.

    double total_evaluation = delta_score + hp_bonus + energy_bonus + zone_proximity_bonus + attack_range_bonus;

    return total_evaluation;
}
//1564.06
double evaluate_world_v1_by_chatgpt(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    // Защита от некорректного id
    if (my_id < 0 || my_id >= (int)world_before.agents.size() ||
        my_id >= (int)world_after.agents.size()) return 0.0;

    const auto &b = world_before.agents[my_id];
    const auto &a = world_after.agents[my_id];

    // Если в "после" игра завершилась, учтём итоговую формулу ближе к финальной метрике:
    // финальный score = ticks_survived + damage_dealt*0.314 + frags*50 (потом вычитается среднее)
    // здесь мы используем приближённую разницу этой величины
    double score_delta = 0.0;

    // Сильный штраф, если умерли в результате действия
    if (!a.alive && b.alive) {
        return -1e6; // очень плохо — уходим от этого действия в приоритете
    }
    // Большой бонус, если до были мертвы, а теперь ожили (маловероятно) или если выжили
    if (a.alive && !b.alive) {
        score_delta += 1e5;
    }

    // Основная приближённая метрика (схожа с финальной): ticks_survived, damage_dealt, frags
    score_delta += (a.ticks_survived - b.ticks_survived) * 1.0;
    score_delta += (a.damage_dealt - b.damage_dealt) * 0.314;
    score_delta += (a.frags - b.frags) * 50.0;

    // Дополнительные полезные признаки: текущее HP и энергия — положительно влияют на шанс дальнейшего выживания
    score_delta += (a.hp - b.hp) * 0.5;     // вес hp умеренный
    score_delta += (a.energy - b.energy) * 0.1; // энергия менее важна

    // Поощряем сокращение расстояния к центру зоны (чтобы избежать урона от зоны)
    int before_dist2 = b.x*b.x + b.y*b.y;
    int after_dist2  = a.x*a.x + a.y*a.y;
    // Маленький бонус за уменьшение дистанции (центр обычно безопаснее)
    score_delta += (before_dist2 - after_dist2) * 1e-6;

    // Наказываем улучшение состояния соперников (чтобы выбирать действия, которые вредят врагам)
    double enemies_delta = 0.0;
    int n = (int)world_before.agents.size();
    for (int i = 0; i < n; ++i) {
        if (i == my_id) continue;
        const auto &eb = world_before.agents[i];
        const auto &ea = world_after.agents[i];
        // Если соперник ожил — сильный минус
        if (ea.alive && !eb.alive) enemies_delta += 500.0;
        // Если соперник умер — бонус (нам хорошо, если враг погиб)
        if (!ea.alive && eb.alive) enemies_delta -= 500.0;
        // Увеличение их ключевых показателей уменьшает нашу относительную пользу
        enemies_delta += (ea.ticks_survived - eb.ticks_survived) * 0.5;
        enemies_delta += (ea.damage_dealt - eb.damage_dealt) * 0.314 * 0.8;
        enemies_delta += (ea.frags - eb.frags) * 50.0 * 0.8;
        enemies_delta += (ea.hp - eb.hp) * 0.1;
    }
    // Учтём (вычтем) влияние соперников — домножаем на небольшой коэффициент
    score_delta -= enemies_delta * 0.8;

    // Если наш игрок жив и имеет шанс наносить урон (energy/hp), чуть повышаем оценку
    if (a.alive) {
        score_delta += (a.hp > 40 ? 5.0 : 0.0);
        score_delta += (a.energy > 10 ? 2.0 : 0.0);
    }

    // Финальный скейлинг — ничего экстремального
    return score_delta;
}
//1213.04
double evaluate_world_v1_by_alice(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (my_id < 0 || my_id >= (int)world_after.agents.size()) {
        return -1e9; // Недопустимый ID — очень плохая оценка
    }

    const auto& agent_before = world_before.agents[my_id];
    const auto& agent_after = world_after.agents[my_id];

    // Если агент погиб, оценка резко отрицательная
    if (!agent_after.alive) {
        return -10000.0;
    }

    // Базовые параметры
    double hp_diff = agent_after.hp - agent_before.hp;
    double energy_diff = agent_after.energy - agent_before.energy;
    double damage_diff = agent_after.damage_dealt - agent_before.damage_dealt;
    double frags_diff = agent_after.frags - agent_before.frags;

    // Веса для разных факторов (можно настраивать)
    const double HP_WEIGHT = 0.1;
    const double ENERGY_WEIGHT = 0.05;
    const double DAMAGE_WEIGHT = 0.3;
    const double FRAGS_WEIGHT = 50.0;
    const double TICK_WEIGHT = 0.2;

    double score = 0.0;

    // Здоровье: поощряем сохранение и прирост
    score += hp_diff * HP_WEIGHT;

    // Энергия: важна, но не критична
    score += energy_diff * ENERGY_WEIGHT;

    // Нанесённый урон
    score += damage_diff * DAMAGE_WEIGHT;

    // Фраги — очень ценны
    score += frags_diff * FRAGS_WEIGHT;

    // Выживание один дополнительный тик
    score += TICK_WEIGHT;

    // Бонус за нахождение в безопасной зоне (ближе к центру)
    int r_before = agent_before.x * agent_before.x + agent_before.y * agent_before.y;
    int r_after = agent_after.x * agent_after.x + agent_after.y * agent_after.y;
    if (r_after < r_before) {
        score += 0.5; // Немного поощряем движение к центру
    }

    // Штраф за удаление от центра (если зона сузилась)
    if (r_after > world_after.R * world_after.R) {
        score -= 1.0; // Наказываем за выход из безопасной зоны
    }

    // Дополнительно: если атаковал и нанёс урон — бонус
    if (agent_after.attacked_this_tick && damage_diff > 0) {
        score += 1.0;
    }

    return score;
}
//1851.88
double evaluate_world_v1_by_qwenmax(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (!qap_check_id(world_after.agents, my_id)) {
        return -1e9;
    }

    const auto& a = world_after.agents[my_id];

    // If dead, huge penalty
    if (!a.alive) {
        // Penalize based on how early: fewer ticks = worse
        // Max ticks = 200, so death at tick T gives -(200 - T)
        double penalty = -10000.0 + (world_after.tick * 10.0); // slightly less bad if died late
        return penalty;
    }

    // Proxy for final score components
    double score = 0.0;

    // Reward survival time (ticks_survived)
    score += a.ticks_survived;

    // Reward damage dealt
    score += a.damage_dealt * 0.314;

    // Reward frags heavily
    score += a.frags * 50.0;

    // Bonus for being alive and healthy
    score += a.hp * 0.5;          // HP is valuable
    score += a.energy * 0.1;      // Energy enables future actions

    // Bonus for being near center (safe from zone damage)
    int dist_sq = a.x * a.x + a.y * a.y;
    int R = world_after.R;
    if (dist_sq <= R * R) {
        // Safe zone: extra bonus
        score += 10.0;
    } else {
        // In danger zone: penalize
        score -= 20.0;
    }

    // Optional: reward distance from enemies if low HP (defensive play)
    // But for simplicity and aggression (frags matter a lot), we skip this.

    return score;
}
/*
evaluate_world_v1_by_perplexity
evaluate_world_v1_by_grok
evaluate_world_v1_by_deepseek
evaluate_world_v1_by_gemini
evaluate_world_v1_by_chatgpt
evaluate_world_v1_by_alice
evaluate_world_v1_by_qwenmax
*/
template<class FUNC>
t_pubg_world::Action dna_by_adler_templ(const t_pubg_world&orig,int i,FUNC&&evaluate_world_v1){
  auto w=orig;
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
  double best_score_a1 = -1e9;
  t_pubg_world::Action best_action_a1;
  for(int dx=-1;dx<=+1;dx++)for(int dy=-1;dy<=+1;dy++)
  for(int eid=0;eid<earr.size();eid++)
  for (uint64_t c1 = 0; c1 < 8; ++c1) {
      auto a=encode_action(c1,earr[eid],true,vec2i(dx,dy));
      if(a.avoid_others)continue;
      auto world1 = orig;
      world1.agents[i].a=a;
      world1.step();

      if (!world1.agents[i].alive) {
          double score = -10000;
          if (score > best_score_a1) {
              best_score_a1 = score;
              best_action_a1 = encode_action(c1,earr[eid],true,vec2i(dx,dy));
          }
          continue;
      }
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
      if (best_score_a2 > best_score_a1) {
          best_score_a1 = best_score_a2;
          best_action_a1 = encode_action(c1,earr[eid],true,vec2i(dx,dy));
      }
  }
  return best_action_a1;
}
double evaluate_world_v1_by_qwen3max_v2(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (!qap_check_id(world_after.agents, my_id)) return -1e9;
    const auto& me = world_after.agents[my_id];
    if (!me.alive) return -1e9; // death = worst outcome

    double score = 0.0;

    // 1. Survival is baseline — already alive, so we build on that
    // No need to add ticks_survived — it increases uniformly per tick

    // 2. Immediate combat opportunities
    int close_enemies = 0;
    int attackable_enemies = 0;
    int vulnerable_enemies = 0; // hp <= 50
    int total_enemy_hp = 0;
    int nearest_dist_sq = INT_MAX;

    for (const auto& other : world_after.agents) {
        if (&other == &me || !other.alive) continue;

        int dx = me.x - other.x;
        int dy = me.y - other.y;
        int dist_sq = dx*dx + dy*dy;
        nearest_dist_sq = std::min(nearest_dist_sq, dist_sq);

        if (dist_sq <= 300*300) {
            close_enemies++;
            total_enemy_hp += other.hp;
            if (other.hp <= 50) vulnerable_enemies++;
        }
        if (dist_sq <= 100*100) { // within attack range
            attackable_enemies++;
        }
    }

    // Reward being near enemies you can kill
    score += vulnerable_enemies * 30.0;   // high value: potential frags
    score += attackable_enemies * 10.0;   // opportunity to deal damage now

    // Penalize isolation (no enemies nearby = no frags)
    if (close_enemies == 0) {
        score -= 5.0;
    }

    // 3. Personal state
    score += me.hp * 0.3;                 // HP matters, but less than frags
    score += me.energy * 0.2;             // energy enables next actions

    // 4. Zone safety
    int my_dist_sq = me.x*me.x + me.y*me.y;
    int R = world_after.R;
    if (my_dist_sq > R*R) {
        score -= 25.0; // zone damage is lethal over time — avoid now
    } else {
        score += 5.0;
    }

    // 5. Aggression bonus: if we just dealt damage or got a frag, reinforce
    // But we can't see "this tick" stats in world_after unless stored.
    // Instead, infer from state: high frags/damage relative to tick?
    // For 2-step lookahead, this is less reliable — skip.

    // 6. Critical: avoid being surrounded
    if (close_enemies >= 3 && me.hp < 60) {
        score -= 20.0; // high risk of death next tick
    }

    // 7. Distance to nearest enemy: too far = bad, too close = risky if low HP
    if (nearest_dist_sq < INT_MAX) {
        double dist = std::sqrt(nearest_dist_sq);
        if (me.hp > 70) {
            // Aggressive: reward closeness
            score += std::max(0.0, 20.0 - dist * 0.1);
        } else {
            // Defensive: penalize extreme closeness
            if (dist < 50) score -= 10.0;
        }
    }

    return score;
}
double evaluate_world_v1_by_qwen3max_v3(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (!qap_check_id(world_after.agents, my_id)) return -1e9;
    const auto& me = world_after.agents[my_id];
    if (!me.alive) return -1e9;

    const double R = static_cast<double>(world_after.R);
    const double my_dist = std::sqrt(me.x * me.x + me.y * me.y);
    const double zone_safety = std::max(0.0, 1.0 - (my_dist - R) / (R + 1e-3)); // ~1 if inside, ~0 if far outside
    // Альтернатива: exp(-std::max(0.0, my_dist - R) / 200.0)

    double score = 0.0;

    // 1. Survivability: smooth function of HP and zone
    double hp_factor = static_cast<double>(me.hp) / 100.0; // [0,1]
    score += 100.0 * hp_factor * zone_safety; // alive + safe = high base

    // 2. Energy enables future actions
    score += 10.0 * (static_cast<double>(me.energy) / 100.0);

    // 3. Enemy interaction: smooth attraction/repulsion based on threat & opportunity
    double enemy_influence = 0.0;
    int enemy_count = 0;

    for (const auto& other : world_after.agents) {
        if (&other == &me || !other.alive) continue;
        enemy_count++;

        double dx = static_cast<double>(me.x - other.x);
        double dy = static_cast<double>(me.y - other.y);
        double dist = std::sqrt(dx*dx + dy*dy) + 1e-3; // avoid div by zero

        double other_hp = static_cast<double>(other.hp);
        double threat = (100.0 - other_hp) / 100.0; // weak enemy = less threat, more opportunity
        double opportunity = (100.0 - other_hp) / 100.0; // same: low HP → high kill potential

        // Attack desirability: strong when enemy is close AND weak
        double attack_potential = opportunity * std::exp(-dist / 80.0); // effective range ~80

        // Danger from strong enemies: repel if they are close and healthy
        double danger = (other_hp / 100.0) * std::exp(-dist / 60.0); // strong enemies dangerous up close

        // Net effect: attract to weak, repel from strong
        enemy_influence += attack_potential - 0.7 * danger;
    }

    // Scale enemy influence by number of enemies (avoid overvaluing in 1v1 vs 1v15)
    if (enemy_count > 0) {
        enemy_influence /= std::sqrt(enemy_count); // diminishing returns
    }

    score += 80.0 * enemy_influence;

    // 4. Reward progress toward objectives (implicit)
    // Frags and damage are lagging indicators — but we can estimate kill potential:
    // For each enemy, probability of kill ≈ max(0, (me_damage_potential - other.hp) / me_damage_potential)
    // But without knowing next action, approximate via proximity to low-HP enemies:
    double kill_potential = 0.0;
    for (const auto& other : world_after.agents) {
        if (&other == &me || !other.alive) continue;
        double dist = std::sqrt(std::pow(me.x - other.x, 2) + std::pow(me.y - other.y, 2)) + 1e-3;
        double hp_ratio = std::max(0.0, 1.0 - other.hp / 100.0);
        kill_potential += hp_ratio * std::exp(-dist / 70.0);
    }
    score += 40.0 * kill_potential;

    // 5. Small bonus for being alive longer (but don't overdo — it's implicit in HP/zone)
    score += 0.5 * me.ticks_survived;

    return score;
}
double evaluate_world_v1_by_qwen3max_v4i(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    // === ПАРАМЕТРЫ ОЦЕНОЧНОЙ ФУНКЦИИ (для тюнинга) ===
    // DEF_CONST(HP_DANGER_THRESHOLD, 10, 25, 10, 50, linear)
    constexpr int HP_DANGER_THRESHOLD = 30;

    // DEF_CONST(MIN_FRAG_WORTH_RISK, 9, 0.85, 0.7, 0.95, linear)
    constexpr double MIN_FRAG_WORTH_RISK = 20.95;

    // DEF_CONST(OPPORTUNITY_REWARD_SCALE, 8, 100, 50, 150, linear)
    constexpr double OPPORTUNITY_REWARD_SCALE = 115.0;

    // DEF_CONST(OPPORTUNITY_DISTANCE_SCALE, 8, 800, 300, 1200, linear)
    constexpr double OPPORTUNITY_DISTANCE_SCALE = 800.0;

    // DEF_CONST(ZONE_PENALTY, 7, 30, 10, 50, linear)
    constexpr double ZONE_PENALTY = 0.0;

    // === БАЗОВАЯ ПРОВЕРКА ===
    if (!qap_check_id(world_after.agents, my_id)) {
        return -1e9;
    }
    const auto& me = world_after.agents[my_id];
    if (!me.alive) {
        return -1e9;
    }

    // === 1. ОПРЕДЕЛЕНИЕ СОСТОЯНИЯ АГЕНТА ===
    const bool is_in_danger = (me.hp < HP_DANGER_THRESHOLD);
    const double my_dist_to_center = std::sqrt(static_cast<double>(me.x * me.x + me.y * me.y));
    const bool is_in_safe_zone = (my_dist_to_center <= static_cast<double>(world_after.R));

    // === 2. ПОИСК САМОЙ ЛАКОМОЙ ЦЕЛИ ===
    double best_opportunity = -1.0; // -1 означает "нет цели"
    double best_dist = 1e9;

    for (const auto& other : world_after.agents) {
        if (&other == &me || !other.alive) continue;

        const double dx = static_cast<double>(me.x - other.x);
        const double dy = static_cast<double>(me.y - other.y);
        const double dist = std::sqrt(dx * dx + dy * dy);

        // Игнорируем слишком далёких (за пределами арены)
        if (dist > 1000.0) continue;

        // "Лакомость" = насколько враг слаб + насколько он близко
        const double hp_factor = (100.0 - static_cast<double>(other.hp)) / 100.0; // [0,1]
        const double proximity_factor = std::max(0.0, 1.0 - dist / OPPORTUNITY_DISTANCE_SCALE); // [0,1]
        const double opportunity = hp_factor * proximity_factor;

        if (opportunity > best_opportunity) {
            best_opportunity = opportunity;
            best_dist = dist;
        }
    }

    // === 3. ФОРМИРОВАНИЕ ОЦЕНКИ ===
    double score = 0.0;

    // Градиент к цели (слабый, но глобальный)
    if (best_opportunity > 0.0) {
        score += best_opportunity * OPPORTUNITY_REWARD_SCALE;

        // Но если мы в опасности — проверяем, стоит ли рисковать
        if (is_in_danger && best_dist < 200.0) {
            // Рискуем ТОЛЬКО если цель почти мертва
            if (best_opportunity < MIN_FRAG_WORTH_RISK) {
                score -= 200.0; // жёсткий штраф: запрет на самоубийственный раш
            }
            // Иначе — оставляем высокую оценку: фраг того стоит
        }
    }

    // Штраф за зону (вторичный приоритет)
    if (!is_in_safe_zone) {
        score -= ZONE_PENALTY;
    }

    // Небольшая поддержка выживания и ресурсов
    score += me.energy * 0.1;
    score += me.ticks_survived * 0.2;

    return score;
}
double evaluate_world_v1_by_qwen3max_v4o(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    // === ПАРАМЕТРЫ ОЦЕНОЧНОЙ ФУНКЦИИ (для тюнинга) ===
    // DEF_CONST(HP_DANGER_THRESHOLD, 10, 25, 10, 50, linear)
    constexpr int HP_DANGER_THRESHOLD = 25;

    // DEF_CONST(MIN_FRAG_WORTH_RISK, 9, 0.85, 0.7, 0.95, linear)
    constexpr double MIN_FRAG_WORTH_RISK = 0.85;

    // DEF_CONST(OPPORTUNITY_REWARD_SCALE, 8, 100, 50, 150, linear)
    constexpr double OPPORTUNITY_REWARD_SCALE = 100.0;

    // DEF_CONST(OPPORTUNITY_DISTANCE_SCALE, 8, 800, 300, 1200, linear)
    constexpr double OPPORTUNITY_DISTANCE_SCALE = 800.0;

    // DEF_CONST(ZONE_PENALTY, 7, 30, 10, 50, linear)
    constexpr double ZONE_PENALTY = 30.0;

    // === БАЗОВАЯ ПРОВЕРКА ===
    if (!qap_check_id(world_after.agents, my_id)) {
        return -1e9;
    }
    const auto& me = world_after.agents[my_id];
    if (!me.alive) {
        return -1e9;
    }

    // === 1. ОПРЕДЕЛЕНИЕ СОСТОЯНИЯ АГЕНТА ===
    const bool is_in_danger = (me.hp < HP_DANGER_THRESHOLD);
    const double my_dist_to_center = std::sqrt(static_cast<double>(me.x * me.x + me.y * me.y));
    const bool is_in_safe_zone = (my_dist_to_center <= static_cast<double>(world_after.R));

    // === 2. ПОИСК САМОЙ ЛАКОМОЙ ЦЕЛИ ===
    double best_opportunity = -1.0; // -1 означает "нет цели"
    double best_dist = 1e9;

    for (const auto& other : world_after.agents) {
        if (&other == &me || !other.alive) continue;

        const double dx = static_cast<double>(me.x - other.x);
        const double dy = static_cast<double>(me.y - other.y);
        const double dist = std::sqrt(dx * dx + dy * dy);

        // Игнорируем слишком далёких (за пределами арены)
        if (dist > 1000.0) continue;

        // "Лакомость" = насколько враг слаб + насколько он близко
        const double hp_factor = (100.0 - static_cast<double>(other.hp)) / 100.0; // [0,1]
        const double proximity_factor = std::max(0.0, 1.0 - dist / OPPORTUNITY_DISTANCE_SCALE); // [0,1]
        const double opportunity = hp_factor * proximity_factor;

        if (opportunity > best_opportunity) {
            best_opportunity = opportunity;
            best_dist = dist;
        }
    }

    // === 3. ФОРМИРОВАНИЕ ОЦЕНКИ ===
    double score = 0.0;

    // Градиент к цели (слабый, но глобальный)
    if (best_opportunity > 0.0) {
        score += best_opportunity * OPPORTUNITY_REWARD_SCALE;

        // Но если мы в опасности — проверяем, стоит ли рисковать
        if (is_in_danger && best_dist < 200.0) {
            // Рискуем ТОЛЬКО если цель почти мертва
            if (best_opportunity < MIN_FRAG_WORTH_RISK) {
                score -= 200.0; // жёсткий штраф: запрет на самоубийственный раш
            }
            // Иначе — оставляем высокую оценку: фраг того стоит
        }
    }

    // Штраф за зону (вторичный приоритет)
    if (!is_in_safe_zone) {
        score -= ZONE_PENALTY;
    }

    // Небольшая поддержка выживания и ресурсов
    score += me.energy * 0.1;
    score += me.ticks_survived * 0.2;

    return score;
}
// --- CHATGPT_V2: gradient-based local evaluation ---
#define DEF_CONST(NAME, IMPORTANCE, BASE, FROM, TO, FUNC) \
    const double NAME = BASE; /* importance="IMPORTANCE", range=[FROM..TO], shaping=FUNC */

double evaluate_world_v1_by_chatgpt_v2(const t_pubg_world& w_before, const t_pubg_world& w_after, int my_id) {
    if (my_id < 0 || my_id >= (int)w_before.agents.size()) return 0.0;
    const auto &b = w_before.agents[my_id];
    const auto &a = w_after.agents[my_id];

    // === PARAMETER DEFINITION ===
    DEF_CONST(K_HP_GRADIENT,        0.9,  0.5,    0.1,  1.0,  linear);   // важность выживания
    DEF_CONST(K_ENERGY_GRADIENT,    0.3,  0.1,    0.01, 0.2,  linear);   // бонус за энергию
    DEF_CONST(K_DAMAGE_DELTA,       0.8,  0.314,  0.1,  0.5,  linear);   // вклад урона
    DEF_CONST(K_FRAG_BONUS,         1.0,  50.0,   20.0, 80.0, linear);   // награда за фраг
    DEF_CONST(K_DISTANCE_CENTER,    0.3,  1e-6,   1e-7, 1e-5, log);     // мягкий градиент к центру
    DEF_CONST(K_ENEMY_PENALTY,      0.7,  0.8,    0.3,  1.5,  linear);   // сколько вычитаем за улучшение врагов
    DEF_CONST(K_NEARBY_BONUS,       0.5,  200.0,  50.0, 400.0, linear); // стремление к "сладкой" цели
    DEF_CONST(K_DANGER_BARRIER,     1.0,  40.0,   20.0, 80.0, linear);  // большой порог hp как “барьер”
    DEF_CONST(K_DANGER_PENALTY,     1.0, -400.0, -1000.0, -100.0, linear); // штраф за переход через барьер

    // === STEP 1: базовые метрики ===
    if (!a.alive && b.alive) return -1e6;  // смерть — огромный штраф
    double score = 0.0;

    // финальные приращения “основной метрики”
    score += (a.ticks_survived - b.ticks_survived) * 1.0;
    score += (a.damage_dealt - b.damage_dealt) * K_DAMAGE_DELTA;
    score += (a.frags - b.frags) * K_FRAG_BONUS;
    score += (a.hp - b.hp) * K_HP_GRADIENT;
    score += (a.energy - b.energy) * K_ENERGY_GRADIENT;

    // === STEP 2: градиент к центру ===
    int before_dist2 = b.x*b.x + b.y*b.y;
    int after_dist2  = a.x*a.x + a.y*a.y;
    score += (before_dist2 - after_dist2) * K_DISTANCE_CENTER;

    // === STEP 3: мягкий барьер по HP ===
    if (a.hp < K_DANGER_BARRIER && a.alive) {
        // штраф по экспоненте: чем меньше HP, тем больше страх
        double t = std::max(0.0, (K_DANGER_BARRIER - a.hp) / K_DANGER_BARRIER);
        score += K_DANGER_PENALTY * t * t;
    }

    // === STEP 4: найти "лакомую цель" (ближайшего слабого врага) ===
    double best_target_value = -1e9;
    int n = (int)w_before.agents.size();
    for (int i = 0; i < n; ++i) {
        if (i == my_id) continue;
        const auto &eb = w_before.agents[i];
        const auto &ea = w_after.agents[i];
        if (!ea.alive) continue;

        // дистанция после
        double dx = a.x - ea.x;
        double dy = a.y - ea.y;
        double dist = std::sqrt(dx*dx + dy*dy) + 1.0;

        // ценность цели: слабые, но живые враги ближе — привлекательнее
        double target_value = (150.0 - ea.hp) / dist;

        // бонус усиливается, если у цели низкая энергия (легче добить)
        target_value += (100.0 - ea.energy) / (2.0 * dist);

        if (target_value > best_target_value)
            best_target_value = target_value;
    }

    if (best_target_value > -1e8) {
        score += best_target_value * (K_NEARBY_BONUS / 100.0);
    }

    // === STEP 5: штраф за улучшение состояния врагов ===
    double enemy_improve = 0.0;
    for (int i = 0; i < n; ++i) {
        if (i == my_id) continue;
        const auto &eb = w_before.agents[i];
        const auto &ea = w_after.agents[i];

        if (!eb.alive && ea.alive) enemy_improve += 1000.0; // враг ожил — плохо
        if (eb.alive && !ea.alive) enemy_improve -= 1000.0; // враг умер — отлично

        enemy_improve += (ea.hp - eb.hp) * 0.5;
        enemy_improve += (ea.damage_dealt - eb.damage_dealt) * 0.314;
        enemy_improve += (ea.frags - eb.frags) * 50.0;
    }
    score -= enemy_improve * K_ENEMY_PENALTY;

    // === STEP 6: стабилизирующий бонус за живучесть ===
    if (a.alive)
        score += std::log1p((double)a.hp + a.energy) * 2.0;

    return score;
}
double evaluate_world_v1_by_chatgpt_v2i(const t_pubg_world& w_before, const t_pubg_world& w_after, int my_id) {
    if (my_id < 0 || my_id >= (int)w_before.agents.size()) return 0.0;
    const auto &b = w_before.agents[my_id];
    const auto &a = w_after.agents[my_id];

    // === PARAMETER DEFINITION ===
    DEF_CONST(K_HP_GRADIENT,        0.9,  0.5,    0.1,  1.0,  linear);   // важность выживания
    DEF_CONST(K_ENERGY_GRADIENT,    0.3,  0.1,    0.01, 0.2,  linear);   // бонус за энергию
    DEF_CONST(K_DAMAGE_DELTA,       0.8,  0.314,  0.1,  0.5,  linear);   // вклад урона
    DEF_CONST(K_FRAG_BONUS,         1.0,  50.0,   20.0, 80.0, linear);   // награда за фраг
    DEF_CONST(K_DISTANCE_CENTER,    0.3,  1.6e-4,   1e-7, 1e-5, log);     // мягкий градиент к центру
    DEF_CONST(K_ENEMY_PENALTY,      0.7,  1.51,    0.3,  1.5,  linear);   // сколько вычитаем за улучшение врагов
    DEF_CONST(K_NEARBY_BONUS,       0.5,  285.0,  50.0, 400.0, linear); // стремление к "сладкой" цели
    DEF_CONST(K_DANGER_BARRIER,     1.0,  30.0,   20.0, 80.0, linear);  // большой порог hp как “барьер”
    DEF_CONST(K_DANGER_PENALTY,     1.0, -400.0, -1000.0, -100.0, linear); // штраф за переход через барьер

    // === STEP 1: базовые метрики ===
    if (!a.alive && b.alive) return -1e6;  // смерть — огромный штраф
    double score = 0.0;

    // финальные приращения “основной метрики”
    score += (a.ticks_survived - b.ticks_survived) * 1.0;
    score += (a.damage_dealt - b.damage_dealt) * K_DAMAGE_DELTA;
    score += (a.frags - b.frags) * K_FRAG_BONUS;
    score += (a.hp - b.hp) * K_HP_GRADIENT;
    score += (a.energy - b.energy) * K_ENERGY_GRADIENT;

    // === STEP 2: градиент к центру ===
    int before_dist2 = b.x*b.x + b.y*b.y;
    int after_dist2  = a.x*a.x + a.y*a.y;
    score += (before_dist2 - after_dist2) * K_DISTANCE_CENTER;

    // === STEP 3: мягкий барьер по HP ===
    if (a.hp < K_DANGER_BARRIER && a.alive) {
        // штраф по экспоненте: чем меньше HP, тем больше страх
        double t = std::max(0.0, (K_DANGER_BARRIER - a.hp) / K_DANGER_BARRIER);
        score += K_DANGER_PENALTY * t * t;
    }

    // === STEP 4: найти "лакомую цель" (ближайшего слабого врага) ===
    double best_target_value = -1e9;
    int n = (int)w_before.agents.size();
    for (int i = 0; i < n; ++i) {
        if (i == my_id) continue;
        const auto &eb = w_before.agents[i];
        const auto &ea = w_after.agents[i];
        if (!ea.alive) continue;

        // дистанция после
        double dx = a.x - ea.x;
        double dy = a.y - ea.y;
        double dist = std::sqrt(dx*dx + dy*dy) + 1.0;

        // ценность цели: слабые, но живые враги ближе — привлекательнее
        double target_value = (150.0 - ea.hp) / dist;

        // бонус усиливается, если у цели низкая энергия (легче добить)
        target_value += (100.0 - ea.energy) / (2.0 * dist);

        if (target_value > best_target_value)
            best_target_value = target_value;
    }

    if (best_target_value > -1e8) {
        score += best_target_value * (K_NEARBY_BONUS / 100.0);
    }

    // === STEP 5: штраф за улучшение состояния врагов ===
    double enemy_improve = 0.0;
    for (int i = 0; i < n; ++i) {
        if (i == my_id) continue;
        const auto &eb = w_before.agents[i];
        const auto &ea = w_after.agents[i];

        if (!eb.alive && ea.alive) enemy_improve += 1000.0; // враг ожил — плохо
        if (eb.alive && !ea.alive) enemy_improve -= 1000.0; // враг умер — отлично

        enemy_improve += (ea.hp - eb.hp) * 0.5;
        enemy_improve += (ea.damage_dealt - eb.damage_dealt) * 0.314;
        enemy_improve += (ea.frags - eb.frags) * 50.0;
    }
    score -= enemy_improve * K_ENEMY_PENALTY;

    // === STEP 6: стабилизирующий бонус за живучесть ===
    if (a.alive)
        score += std::log1p((double)a.hp + a.energy) * 2.0;

    return score;
}
double evaluate_world_v1_by_grok_v2(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (!qap_check_id(world_after.agents, my_id)) return -1e18;
    const auto& me_b = world_before.agents[my_id];
    const auto& me = world_after.agents[my_id];
    if (!me.alive) return -1e18;

    double score = 0.0;

    // === 1. БАЗОВЫЕ СТАТЫ ===
    score += me.ticks_survived * 2.5;           // выживание
    score += me.hp * 3.5;                       // здоровье — критично
    score += me.energy * 0.6;                   // энергия для манёвров
    score += me.frags * 600.0;                  // **ФРАГИ — ГЛАВНОЕ**
    score += me.damage_dealt * 1.3;             // урон

    // === 2. ЗОНА ===
    int R = world_after.R;
    double dist_to_center = sqrt(me.x*me.x + me.y*me.y);
    if (dist_to_center > R) {
        score -= (dist_to_center - R) * 25.0;   // **Жёсткая смерть в зоне**
    } else {
        double safety_ratio = (R - dist_to_center) / R;
        score += safety_ratio * 80.0;           // бонус за центр
        score += pow(safety_ratio, 2) * 100.0;  // **квадратичный бонус за глубокий центр**
    }

    // === 3. ПОЗИЦИОНИРОВАНИЕ ===
    score -= dist_to_center * 0.015;            // тяга к центру

    // === 4. ВРАГИ ===
    int alive_count = 0;
    int weak_near = 0, attackable = 0;
    double closest_dist = 1e9;
    for (int j = 0; j < world_after.agents.size(); ++j) {
        if (j == my_id || !world_after.agents[j].alive) continue;
        alive_count++;
        int dx = me.x - world_after.agents[j].x;
        int dy = me.y - world_after.agents[j].y;
        double d = sqrt(dx*dx + dy*dy);
        if (d < closest_dist) closest_dist = d;

        if (d <= 150 && world_after.agents[j].hp <= 50) weak_near++;
        if (d <= 100) attackable++;
    }

    // === 5. АГРЕССИЯ ===
    if (me.hp >= 65 && me.energy >= 3 && attackable > 0) {
        score += 150.0;                         // **ГОТОВ К АТАКЕ**
        if (me.hp >= 75) score += 100.0;        // можно и fury
    }

    // === 6. ФИНИШ СЛАБЫХ ===
    score += weak_near * 300.0;                 // **УБИТЬ РАНЕНОГО = ПРИОРИТЕТ**

    // === 7. ПОСЛЕДНИЙ БОЙ (1v1) ===
    if (alive_count == 1) {                     // мы + 1 враг
        score += 1000.0;                        // **МАКСИМАЛЬНЫЙ ПРИОРИТЕТ ПОБЕДЫ**
        if (closest_dist <= 200) score += 500.0;
        if (me.hp > 30) score += 300.0;
    }

    // === 8. ДЕЛЬТА (улучшение) ===
    score += (me.frags - me_b.frags) * 800.0;
    score += (me.damage_dealt - me_b.damage_dealt) * 1.8;
    score += (me.hp - me_b.hp) * 6.0;

    return score;
}
double evaluate_world_v1_by_grok_v3(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (!qap_check_id(world_after.agents, my_id)) return -1e18;
    const auto& me_b = world_before.agents[my_id];
    const auto& me = world_after.agents[my_id];
    if (!me.alive) return -1e18;

    // === ТЮНИНГОВЫЕ КОНСТАНТЫ: DEF_CONST(param, importance, base_value, tune_from, tune_to, func_type) ===
    // importance: вес в общем score (sum~100 для баланса)
    // base: стартовая ценность
    // tune_from/to: диапазон для grid-search/GA
    // func: как применять (sigmoid/exp/inv_dist/linear/quad)
    
    // 1. HP_THRESH: БОЛЬШОЙ ПОРОГ + градиент (не лезем <40HP=стена)
    // Почему: 40HP — точка невозврата (fury= -10HP). Sigmoid: крутой подъём >40.
    // Imp=15 (выживание#1), base=40, tune=20-60, func=sigmoid( (HP-base)/10 )
    double HP_THRESH = 40.0;  // [20,60] imp=15
    double hp_norm = 1.0 / (1.0 + exp( -(me.hp - HP_THRESH)/10.0 ));  // 0..<40, 1>60

    // 2. ZONE_PEN_SCALE: ВЗРЫВНОЙ ГРАДИЕНТ у края зоны
    // Почему: exp((dist-R)/scale) — мягко внутри, -inf снаружи. Scale= R/20~50.
    // Imp=12, base=25.0, [10,40], exp
    double ZONE_PEN_SCALE = 25.0;  // [10,40] imp=12
    double dist_center = sqrt(double(me.x*me.x + me.y*me.y));
    double R = world_after.R;
    double zone_dist_excess = max(0.0, dist_center - R);
    double zone_penalty = exp(zone_dist_excess / ZONE_PEN_SCALE) - 1.0;  // 0 внутри, exp снаружи
    double zone_norm = 1.0 / (1.0 + zone_penalty);  // 1=safe, 0=dead

    // 3. WEAK_ATTRACT: ГРАДИЕНТ К Лакомой ЦЕЛИ (самый слабый враг <300dist)
    // Почему: 1/dist * low_hp_bonus — тянет как магнит. Base=400 (сильнее zone).
    // Imp=20 (killer!), [200,600], inv_dist
    double WEAK_ATTRACT = 400.0;  // [200,600] imp=20

    // 4. FRAG_DELTA_REWARD: ЛИНЕЙНЫЙ БОНУС ЗА ФРАГ В 2 ШАГА
    // Почему: 1 фраг~ +50*final, delta*900 ~ win.
    // Imp=25 (главное!), [600,1200], linear
    double FRAG_DELTA_REWARD = 900.0;  // [600,1200] imp=25

    // 5. ENERGY_SCALE: ЛИНЕЙНЫЙ ГРАДИЕНТ ЭНЕРГИИ
    // Imp=6, [0.5,1.2], linear
    double ENERGY_SCALE = 0.8;  // [0.5,1.2] imp=6
    double energy_norm = me.energy / 100.0;

    // 6. CENTER_QUAD_BIAS: КВАДРАТИЧНЫЙ ТЯГА К ЦЕНТРУ (предугад. shrink)
    // Почему: -dist^2 мягко тянет в core.
    // Imp=8, [0.015,0.035], quad
    double CENTER_QUAD_BIAS = 0.025;  // [0.015,0.035] imp=8
    double center_bias = -dist_center * dist_center * CENTER_QUAD_BIAS / 1e6;  // norm -1..0

    // 7. ATTACK_OPP_SCALE: Бонус за ПРОБИВ в range (<100dist, enemy<60hp)
    // Imp=14, [150,350], near_bonus
    double ATTACK_OPP_SCALE = 250.0;  // [150,350] imp=14

    // === ОСНОВНОЙ SCORE: sum(imp * norm_feat) ===
    double score = 0.0;

    // БАЗА: HP + ZONE + ENERGY + CENTER
    score += 15 * hp_norm;
    score += 12 * zone_norm;
    score += 6 * energy_norm;
    score += 8 * (center_bias + 1.0);  // shift to 0-1

    // === ЛАКОМЫЕ ЦЕЛИ: Weakest nearby ===
    int weakest_id = -1;
    double weakest_hp = 101;
    double min_dist_to_weak = 1e9;
    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (j == my_id || !world_after.agents[j].alive) continue;
        double e_hp = world_after.agents[j].hp;
        if (e_hp >= weakest_hp) continue;  // ищем САМЫЙ слабый
        int dx = me.x - world_after.agents[j].x;
        int dy = me.y - world_after.agents[j].y;
        double d = sqrt(double(dx*dx + dy*dy));
        if (d > 350) continue;  // nearby only
        weakest_hp = e_hp;
        weakest_id = j;
        min_dist_to_weak = d;
    }
    if (weakest_id != -1) {
        // ГРАДИЕНТ: + attract / dist * low_hp_bonus
        double low_bonus = (100.0 - weakest_hp) / 100.0;
        double attract_grad = WEAK_ATTRACT / (min_dist_to_weak + 10.0) * low_bonus / 1000.0;  // norm 0-1
        score += 20 * attract_grad;
    }

    // === АТАК. ОППОРТУНИТИ ===
    int attackable_weak = 0;
    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (j == my_id || !world_after.agents[j].alive) continue;
        int dx = me.x - world_after.agents[j].x;
        int dy = me.y - world_after.agents[j].y;
        double d = sqrt(double(dx*dx + dy*dy));
        if (d <= 100 && world_after.agents[j].hp <= 60) {
            attackable_weak++;
            break;
        }
    }
    if (attackable_weak > 0 && me.hp >= 50 && me.energy >= 3) {
        score += 14 * (1.0 - min_dist_to_weak / 100.0);  // closer=better
    }

    // === ДЕЛЬТА: УЛУЧШЕНИЯ ЗА 2 ШАГА ===
    score += (me.frags - me_b.frags) * FRAG_DELTA_REWARD / 2.0;  // /2 norm
    score += (me.damage_dealt - me_b.damage_dealt) * 1.5;
    score += (me.hp - me_b.hp) * 0.05;  // small

    // === 1v1 БОНУС (если осталось 2 alive) ===
    int alive_enemies = 0;
    for (auto& a : world_after.agents) if (a.alive && &a != &me) alive_enemies++;
    if (alive_enemies == 1) score += 500.0;  // WIN SO CLOSE

    return score * 10.0;  // scale to match prev ~1k-10k
}
double evaluate_world_v1_by_grok_v3i(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (!qap_check_id(world_after.agents, my_id)) return -1e18;
    const auto& me_b = world_before.agents[my_id];
    const auto& me = world_after.agents[my_id];
    if (!me.alive) return -1e18;

    // === ТЮНИНГОВЫЕ КОНСТАНТЫ: DEF_CONST(param, importance, base_value, tune_from, tune_to, func_type) ===
    // importance: вес в общем score (sum~100 для баланса)
    // base: стартовая ценность
    // tune_from/to: диапазон для grid-search/GA
    // func: как применять (sigmoid/exp/inv_dist/linear/quad)
    
    // 1. HP_THRESH: БОЛЬШОЙ ПОРОГ + градиент (не лезем <40HP=стена)
    // Почему: 40HP — точка невозврата (fury= -10HP). Sigmoid: крутой подъём >40.
    // Imp=15 (выживание#1), base=40, tune=20-60, func=sigmoid( (HP-base)/10 )
    double HP_THRESH = 40.0;  // [20,60] imp=15
    double hp_norm = 1.0 / (1.0 + exp( -(me.hp - HP_THRESH)/10.0 ));  // 0..<40, 1>60

    // 2. ZONE_PEN_SCALE: ВЗРЫВНОЙ ГРАДИЕНТ у края зоны
    // Почему: exp((dist-R)/scale) — мягко внутри, -inf снаружи. Scale= R/20~50.
    // Imp=12, base=25.0, [10,40], exp
    double ZONE_PEN_SCALE = 10.0;  // [10,40] imp=12
    double dist_center = sqrt(double(me.x*me.x + me.y*me.y));
    double R = world_after.R;
    double zone_dist_excess = max(0.0, dist_center - R);
    double zone_penalty = exp(zone_dist_excess / ZONE_PEN_SCALE) - 1.0;  // 0 внутри, exp снаружи
    double zone_norm = 1.0 / (1.0 + zone_penalty);  // 1=safe, 0=dead

    // 3. WEAK_ATTRACT: ГРАДИЕНТ К Лакомой ЦЕЛИ (самый слабый враг <300dist)
    // Почему: 1/dist * low_hp_bonus — тянет как магнит. Base=400 (сильнее zone).
    // Imp=20 (killer!), [200,600], inv_dist
    double WEAK_ATTRACT = 400.0;  // [200,600] imp=20

    // 4. FRAG_DELTA_REWARD: ЛИНЕЙНЫЙ БОНУС ЗА ФРАГ В 2 ШАГА
    // Почему: 1 фраг~ +50*final, delta*900 ~ win.
    // Imp=25 (главное!), [600,1200], linear
    double FRAG_DELTA_REWARD = 900.0;  // [600,1200] imp=25

    // 5. ENERGY_SCALE: ЛИНЕЙНЫЙ ГРАДИЕНТ ЭНЕРГИИ
    // Imp=6, [0.5,1.2], linear
    double ENERGY_SCALE = 0.8;  // [0.5,1.2] imp=6
    double energy_norm = me.energy / 100.0;

    // 6. CENTER_QUAD_BIAS: КВАДРАТИЧНЫЙ ТЯГА К ЦЕНТРУ (предугад. shrink)
    // Почему: -dist^2 мягко тянет в core.
    // Imp=8, [0.015,0.035], quad
    double CENTER_QUAD_BIAS = 0.015*0;  // [0.015,0.035] imp=8
    double center_bias = -dist_center * dist_center * CENTER_QUAD_BIAS / 1e6;  // norm -1..0

    // 7. ATTACK_OPP_SCALE: Бонус за ПРОБИВ в range (<100dist, enemy<60hp)
    // Imp=14, [150,350], near_bonus
    double ATTACK_OPP_SCALE = 150.0;  // [150,350] imp=14

    // === ОСНОВНОЙ SCORE: sum(imp * norm_feat) ===
    double score = 0.0;

    // БАЗА: HP + ZONE + ENERGY + CENTER
    score += 15 * hp_norm;
    score += 12 * zone_norm;
    score += 6 * energy_norm;
    score += 8 * (center_bias + 1.0);  // shift to 0-1

    // === ЛАКОМЫЕ ЦЕЛИ: Weakest nearby ===
    int weakest_id = -1;
    double weakest_hp = 101;
    double min_dist_to_weak = 1e9;
    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (j == my_id || !world_after.agents[j].alive) continue;
        double e_hp = world_after.agents[j].hp;
        if (e_hp >= weakest_hp) continue;  // ищем САМЫЙ слабый
        int dx = me.x - world_after.agents[j].x;
        int dy = me.y - world_after.agents[j].y;
        double d = sqrt(double(dx*dx + dy*dy));
        if (d > 350) continue;  // nearby only
        weakest_hp = e_hp;
        weakest_id = j;
        min_dist_to_weak = d;
    }
    if (weakest_id != -1) {
        // ГРАДИЕНТ: + attract / dist * low_hp_bonus
        double low_bonus = (100.0 - weakest_hp) / 100.0;
        double attract_grad = WEAK_ATTRACT / (min_dist_to_weak + 10.0) * low_bonus / 1000.0;  // norm 0-1
        score += 20 * attract_grad;
    }

    // === АТАК. ОППОРТУНИТИ ===
    int attackable_weak = 0;
    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (j == my_id || !world_after.agents[j].alive) continue;
        int dx = me.x - world_after.agents[j].x;
        int dy = me.y - world_after.agents[j].y;
        double d = sqrt(double(dx*dx + dy*dy));
        if (d <= 100 && world_after.agents[j].hp <= 60) {
            attackable_weak++;
            break;
        }
    }
    if (attackable_weak > 0 && me.hp >= 50 && me.energy >= 3) {
        score += 14 * (1.0 - min_dist_to_weak / 100.0);  // closer=better
    }

    // === ДЕЛЬТА: УЛУЧШЕНИЯ ЗА 2 ШАГА ===
    score += (me.frags - me_b.frags) * FRAG_DELTA_REWARD / 2.0;  // /2 norm
    score += (me.damage_dealt - me_b.damage_dealt) * 1.5;
    score += (me.hp - me_b.hp) * 0.05;  // small

    // === 1v1 БОНУС (если осталось 2 alive) ===
    int alive_enemies = 0;
    for (auto& a : world_after.agents) if (a.alive && &a != &me) alive_enemies++;
    if (alive_enemies == 1) score += 500.0;  // WIN SO CLOSE

    return score * 10.0;  // scale to match prev ~1k-10k
}
double evaluate_world_v1_by_gemini_v2(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    if (!qap_check_id(world_after.agents, my_id)) {
        return -1e9; // Агент исчез
    }

    const auto& agent_before = world_before.agents[my_id];
    const auto& agent_after = world_after.agents[my_id];

    // --- 1. ЖЕСТКИЙ ПОРОГ: ВЫЖИВАНИЕ (Самый сильный градиент/порог) ---
    // Смерть - самый большой штраф.
    if (!agent_after.alive) {
        return -1000000.0; 
    }

    // --- 2. ГЛАВНЫЙ ГРАДИЕНТ: ИЗМЕНЕНИЕ СЧЕТА (Самая лакомая цель) ---
    // Формула счета: ticks_survived + damage_dealt * 0.314 + frags * 50
    double score_after = (double)agent_after.ticks_survived + (double)agent_after.damage_dealt * 0.314 + (double)agent_after.frags * 50.0;
    double score_before = (double)agent_before.ticks_survived + (double)agent_before.damage_dealt * 0.314 + (double)agent_before.frags * 50.0;
    
    double delta_raw_score = score_after - score_before; // Основной градиент к победе

    // --- 3. ЗАЩИТНЫЙ ПОРОГ (ИЗБЕГАНИЕ РИСКА) ---

    // A. Штраф/Бонус по HP (Градиент выживания)
    // Учитываем изменение HP: чем больше HP, тем лучше, но целенаправленно торгуем им.
    // Коэффициент 0.2: 1 HP ~ 0.2 балла. Атака на 10 урона дает 3.14 балла. 
    // То есть, мы готовы потерять 10 HP (2.0 балла) чтобы нанести 7-8 урона противнику (2.19 - 2.51 балла).
    // Баланс: Мы готовы потерять здоровье, если это приводит к большему урону.
    const double HP_TRADE_OFF_COEFF = 0.20; 
    double hp_delta_bonus = (agent_after.hp - agent_before.hp) * HP_TRADE_OFF_COEFF;
    
    // B. Штраф за нахождение вне зоны (Граница выживания)
    // Штраф, который превышает все, кроме смерти, чтобы агент всегда двигался в зону.
    double dist_sq_after = (double)agent_after.x * agent_after.x + (double)agent_after.y * agent_after.y;
    double R_sq = (double)world_after.R * world_after.R;
    double zone_damage_penalty = 0.0;
    if (dist_sq_after > R_sq) {
        // Урон от зоны = 5 HP. 5 * 0.2 = 1.0. 
        // Мы хотим, чтобы наказание было в несколько раз выше, чтобы избежать зоны.
        // За каждый тик вне зоны, мы теряем ~10-20 баллов.
        zone_damage_penalty = -20.0 - (std::sqrt(dist_sq_after) - world_after.R) * 0.1; 
    }

    // --- 4. ВСПОМОГАТЕЛЬНЫЕ ГРАДИЕНТЫ (Комфорт и подготовка) ---

    // C. Энергия: позволяет атаковать/двигаться.
    const double ENERGY_COEFF = 0.10; // 1 Energy ~ 0.1 балла. 
    double energy_delta_bonus = (agent_after.energy - agent_before.energy) * ENERGY_COEFF;

    // D. Градиент к ближайшему врагу (только если не используется целевая атака)
    // Этот градиент важен, если агент не может атаковать, чтобы он двигался в сторону битвы.
    // Находим ближайшего врага
    double closest_dist_sq = 1e9;
    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (my_id == j || !world_after.agents[j].alive) continue;
        int dx = agent_after.x - world_after.agents[j].x;
        int dy = agent_after.y - world_after.agents[j].y;
        closest_dist_sq = min(closest_dist_sq, (double)(dx*dx + dy*dy));
    }

    // Бонус за приближение: 10000 - это радиус атаки. 
    // Этот градиент помогает преодолеть расстояние до врага.
    double proximity_bonus = 0.0;
    if (closest_dist_sq < 300 * 300) { // Актуально только на ближней/средней дистанции
        proximity_bonus = (300.0 * 300.0 - closest_dist_sq) * 0.00005;
    }


    // --- ИТОГОВАЯ ОЦЕНКА ---
    double total_evaluation = delta_raw_score
                            + hp_delta_bonus
                            + energy_delta_bonus
                            + zone_damage_penalty
                            + proximity_bonus;

    return total_evaluation;
}

double evaluate_world_v1_by_alice_v2(
    const t_pubg_world& world_before,
    const t_pubg_world& world_after,
    int my_id
) {
    // === НАСТРАИВАЕМЫЕ КОНСТАНТЫ (можно подбирать через оптимизацию) ===
    #define DEF_CONST(name, importance, base, from, to, func) \
        const double name = base; // importance: [0..1], func: linear

    DEF_CONST(HP_OWN_WEIGHT,      0.9,  0.10,  0.01, 0.30, linear);
    DEF_CONST(HP_ENEMY_WEIGHT,   0.8,  0.08,  0.02, 0.20, linear);
    DEF_CONST(FRAG_WEIGHT,       1.0, 50.0, 30.0, 80.0, linear);
    DEF_CONST(DAMAGE_WEIGHT,     0.7,  0.30,  0.10, 0.60, linear);
    DEF_CONST(TICK_WEIGHT,       0.6,  0.20,  0.05, 0.40, linear);
    DEF_CONST(ENERGY_WEIGHT,     0.3,  0.03,  0.01, 0.10, linear);
    DEF_CONST(CENTER_WEIGHT,     0.5,  0.40,  0.10, 0.80, linear);
    DEF_CONST(THREAT_WEIGHT,     0.8, -0.60, -1.00,-0.20, linear); // штраф
    DEF_CONST(ATTACK_BONUS,      0.4,  1.20,  0.50, 2.00, linear);
    DEF_CONST(SAFETY_RADIUS,     0.7, 800.0, 600.0,1000.0, linear); // R при котором начинаем ценить центр

    #undef DEF_CONST

    // === ПРОВЕРКИ ===
    if (my_id < 0 || my_id >= (int)world_after.agents.size()) return -1e9;
    const auto& me_before = world_before.agents[my_id];
    const auto& me_after  = world_after.agents[my_id];

    if (!me_after.alive) return -10000.0; // смерть — катастрофа

    double score = 0.0;

    // === 1. СВОЁ СОСТОЯНИЕ ===
    double hp_diff = me_after.hp - me_before.hp;
    double energy_diff = me_after.energy - me_before.energy;
    double damage_diff = me_after.damage_dealt - me_before.damage_dealt;
    double frags_diff = me_after.frags - me_before.frags;

    score += hp_diff         * HP_OWN_WEIGHT;
    score += energy_diff     * ENERGY_WEIGHT;
    score += damage_diff     * DAMAGE_WEIGHT;
    score += frags_diff      * FRAG_WEIGHT;
    score += TICK_WEIGHT; // за каждый прожитый тик

    // Бонус за атаку с уроном
    if (me_after.attacked_this_tick && damage_diff > 0) {
        score += ATTACK_BONUS;
    }

    // === 2. ПОЛОЖЕНИЕ НА КАРТЕ (градиент к центру) ===
    int r2_after = me_after.x*me_after.x + me_after.y*me_after.y;
    int R = world_after.R;

    if (r2_after < R*R) {
        // Чем ближе к центру — тем лучше (плавный градиент)
        double dist_norm = std::sqrt(r2_after) / R;
        score += CENTER_WEIGHT * (1.0 - dist_norm); // max при dist=0
    } else {
        // Вне зоны — сильный штраф, растущий с удалением
        double over_dist = std::sqrt(r2_after) - R;
        score += CENTER_WEIGHT * (-1.0 - 0.01 * over_dist);
    }

    // === 3. СОСТОЯНИЕ ВРАГОВ (угроза и выгода) ===
    double total_threat = 0.0;
    double potential_frag_value = 0.0;

    for (int j = 0; j < (int)world_after.agents.size(); ++j) {
        if (j == my_id || !world_after.agents[j].alive) continue;

        const auto& enemy = world_after.agents[j];
        int dx = me_after.x - enemy.x;
        int dy = me_after.y - enemy.y;
        double dist = std::sqrt(dx*dx + dy*dy);

        // Угроза: чем ближе и здоровее враг — тем опаснее
        double threat = (enemy.hp > 0) ? (enemy.hp / (dist + 10.0)) : 0.0;
        total_threat += threat;

        // Возможность фрага: если можем добить — это ценно
        if (enemy.hp <= 50 && dist <= 100) { // в зоне атаки
            potential_frag_value += FRAG_WEIGHT * 0.5; // половинный бонус за потенциальный фраг
        }
    }

    score += potential_frag_value;
    score += total_threat * THREAT_WEIGHT; // штраф за угрозу

    // === 4. БАЛАНС РИСКА/ВЫГОДЫ ===
    // Если мы теряем HP, но при этом убиваем/можем убить врага — это оправдано
    if (hp_diff < 0 && frags_diff > 0) {
        score -= hp_diff * 0.5 * HP_OWN_WEIGHT; // частично компенсируем потерю HP
    }

    // Если теряем HP без результата — штраф
    if (hp_diff < 0 && frags_diff == 0 && damage_diff < 10) {
        score += hp_diff * 0.8 * HP_OWN_WEIGHT; // усиленный штраф
    }

    return score;
}
#define DEF_CONST(param, importance, base, from, to, func) \
    constexpr double param = base; /* importance: importance, range: [from, to], func: func */

double evaluate_world_v1_by_perplexity_v2(const t_pubg_world& world_before, const t_pubg_world& world_after, int my_id) {
    // Константы для настройки и масштабирования
    DEF_CONST(DEATH_PENALTY, 10.0, 10000.0, 5000.0, 20000.0, linear)
    DEF_CONST(TICKS_SURVIVED_WEIGHT, 1.0, 1.0, 0.5, 2.0, linear)
    DEF_CONST(DAMAGE_DEALT_WEIGHT, 0.7, 0.314, 0.1, 1.0, linear)
    DEF_CONST(FRAGS_WEIGHT, 0.9, 50.0, 10.0, 100.0, linear)
    DEF_CONST(ENERGY_WEIGHT, 0.2, 0.1, 0.05, 0.5, linear)
    DEF_CONST(HP_WEIGHT, 1.6, 0.5, 0.2, 1.5, linear)
    DEF_CONST(SAFE_ZONE_BONUS, 0.5, 5.0, 1.0, 10.0, linear)
    DEF_CONST(SAFE_ZONE_PENALTY, 0.7, -5.0, -1.0, -10.0, linear)
    DEF_CONST(CENTER_APPROACH_BONUS, 0.3, 2.0, 0.5, 5.0, linear)
    DEF_CONST(CENTER_MOVE_PENALTY, 0.3, -2.0, -0.5, -5.0, linear)
    DEF_CONST(LOW_HP_THRESHOLD, 0.9, 30.0, 10.0, 50.0, linear)
    DEF_CONST(ENEMY_THREAT_WEIGHT, 1.0, -20.0, -50.0, -10.0, linear)  // штраф за близкого сильного врага

    const auto& before = world_before.agents[my_id];
    const auto& after = world_after.agents[my_id];

    // Большой штраф за смерть
    if (!after.alive) return -DEATH_PENALTY;

    double score = 0.0;

    // 1. Выживание — плюс за прирост выживших тиков
    score += (after.ticks_survived - before.ticks_survived) * TICKS_SURVIVED_WEIGHT;

    // 2. Нанесённый урон — мотивировать искать цель для атаки
    score += (after.damage_dealt - before.damage_dealt) * DAMAGE_DEALT_WEIGHT;

    // 3. Фраги — крайне важны для финального результата
    score += (after.frags - before.frags) * FRAGS_WEIGHT;

    // 4. Сохранение энергии — важно для возможности двигаться и атаковать
    score += (after.energy - before.energy) * ENERGY_WEIGHT;

    // 5. Здоровье — сильный порог, урон сильно бьёт по баллу
    score += (after.hp - before.hp) * HP_WEIGHT;

    // 6. Бонус за нахождение внутри безопасной зоны
    int dist_after_sq = after.x * after.x + after.y * after.y;
    int R = world_after.R;
    if (dist_after_sq < R * R) {
        score += SAFE_ZONE_BONUS;
    } else {
        score += SAFE_ZONE_PENALTY;
    }

    // 7. Поощрение движения к центру, чтобы избегать зоны урона (градиент)
    int dist_before_sq = before.x * before.x + before.y * before.y;
    double dist_before = std::sqrt(static_cast<double>(dist_before_sq));
    double dist_after = std::sqrt(static_cast<double>(dist_after_sq));
    if (dist_after < dist_before) {
        score += CENTER_APPROACH_BONUS;
    } else {
        score += CENTER_MOVE_PENALTY;
    }

    // 8. Учитываем угрозу от близких врагов (улучшение соображения врагов)
    // Найдём здоровье ближайшего вражеского агента
    int min_enemy_hp = 10000;
    int threat_dist_sq = 1000000;
    for (const auto& enemy : world_after.agents) {
        if (&enemy == &after || !enemy.alive) continue;
        int dx = after.x - enemy.x;
        int dy = after.y - enemy.y;
        int dist_sq = dx*dx + dy*dy;
        if (dist_sq < threat_dist_sq) {
            threat_dist_sq = dist_sq;
            min_enemy_hp = enemy.hp;
        }
    }
    // Если у врага много хп и он близко — минус в score
    if (threat_dist_sq < 400*400 && min_enemy_hp > LOW_HP_THRESHOLD) {
        score += ENEMY_THREAT_WEIGHT * (min_enemy_hp / 100.0);
    }

    return score;
}
