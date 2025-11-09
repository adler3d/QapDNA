struct t_pubg{
  struct t_pubg_world{
    struct Action {
      #define DEF_PRO_COPYABLE()
      #define DEF_PRO_CLASSNAME()Action
      #define DEF_PRO_VARIABLE(ADD)\
      ADD(bool,move_to_center,false)\
      ADD(bool,chase_enemy,false)\
      ADD(bool,repel_from_enemy,false)\
      ADD(bool,attack,false)\
      ADD(bool,use_fury,false)\
      ADD(bool,use_rush,false)\
      ADD(bool,avoid_others,false)\
      ADD(bool,use_dir,false)\
      ADD(vec2i,dir,{})\
      ADD(int,target,-1)\
      //===
      #include "defprovar.inl"
      //===
    };
    struct Agent {
      #define DEF_PRO_COPYABLE()
      #define DEF_PRO_CLASSNAME()Agent
      #define DEF_PRO_VARIABLE(ADD)\
      ADD(int,x,0)\
      ADD(int,y,0)\
      ADD(int,hp,100)\
      ADD(int,energy,100)\
      ADD(int,state,0)\
      ADD(bool,alive,true)\
      ADD(int,ticks_survived,0)\
      ADD(int,damage_dealt,0)\
      ADD(int,frags,0)\
      ADD(bool,attacked_this_tick,false)\
      ADD(bool,is_manual,true)\
      ADD(Action,a,{})\
      //===
      #include "defprovar.inl"
      //===
    };
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()t_pubg_world
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(int,your_id,-1)\
    ADD(vector<Agent>,agents,{})\
    ADD(int,R,1000)\
    ADD(int,tick,0)\
    ADD(int,alive_count,0)\
    ADD(bool,is_finished,false)\
    //===
    #include "defprovar.inl"
    //===

    mt19937 rng;

    void init(uint64_t seed, uint64_t num_players){
      static uniform_int_distribution<int>dist(-1000,1000-1);
      static uniform_int_distribution<int>dist8(0,7);
      rng.seed(seed);
      agents.resize(num_players);
      alive_count = num_players;
      tick = 0;
      is_finished = false;

      for (int i = 0; i <num_players; ++i) {
        auto&b=agents[i];
        b.x = dist(rng);
        b.y = dist(rng);
        b.state = dist8(rng);
        b.alive = true;
        b.hp = 100;
        b.energy = 100;
        b.ticks_survived = 0;
        b.damage_dealt = 0;
        b.frags = 0;
        b.attacked_this_tick = false;
        b.is_manual = false; // по умолчанию — автомат
      }
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
      //vector<Action> actions(agents.size());

      for (int i = 0; i < (int)agents.size(); ++i) {
        if (!agents[i].alive) continue;
        auto& a = agents[i];
        Action& act = a.a;//actions[i];

        if (a.is_manual) {
        
        } else {
          // Обычная стратегия → преобразуем state_byte в Action
          /*uint8_t state_byte = a.strat[a.state];
          act.move_to_center = (state_byte >> 5) & 1;
          act.attack = (state_byte >> 4) & 1;
          uint8_t special = state_byte & 0x03;
          act.use_rush = (special == 0x01);
          act.use_fury = (special == 0x02);
          act.avoid_others = (special == 0x03);
          act.move_to_center = (state_byte >> 5) & 1;
          act.repel_from_enemy = !act.move_to_center;*/
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
      /*for (int i = 0; i < (int)agents.size(); ++i) {
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
      }*/

      tick++;
      if (alive_count <= 1 || tick >= 200||end) {
        is_finished = true;
      }
    }
    bool finished(){return is_finished;}
    void get_score(vector<double>& out){
      double total_raw = 0;
      int PLAYERS_PER_BATTLE = agents.size();
      out.assign(PLAYERS_PER_BATTLE, 0.0);
      for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
        double score = agents[i].ticks_survived 
                + agents[i].damage_dealt * 0.314 
                + agents[i].frags * 50;
        out[i] = score;
        total_raw += score;
      }
      double avg = total_raw / PLAYERS_PER_BATTLE;
      for (int i = 0; i < PLAYERS_PER_BATTLE; ++i) {
        out[i] -= avg;
      }
    }
    void is_alive(vector<int>& out){
      out.resize(agents.size());
      for (int i = 0; i < (int)agents.size(); ++i) {
        out[i] = agents[i].alive ? 1 : 0;
      }
    }
    typedef Action t_cmd;
  };
  typedef t_pubg_world t_world;
  static void init_world(t_world&world,mt19937&gen,uint32_t num_players) {
    static uniform_int_distribution<uint64_t>dist(0,UINT64_MAX);
    world.init(dist(gen),num_players);
  }
  #ifdef I_WORLD
  struct t_world_impl:i_world{
    t_world w;
    mt19937 gen;
    ~t_world_impl(){};
    void use(int player,const string&cmd,string&outmsg)override{
      t_world::t_cmd out;
      bool ok=QapLoadFromStr(out,cmd);
      if(!ok){outmsg="cmd load failed";return;}else{outmsg.clear();}
      w.agents[player].a=out;
    }
    void step()override{w.step();}
    bool finished()override{return w.is_finished;}
    void get_score(vector<double>&out)override{w.get_score(out);}
    void is_alive(vector<int>&out)override{w.is_alive(out);}
    void get_vpow(int player,string&out)override{
      w.your_id=player;
      out=QapSaveToStr(w);
    }
    void init(uint32_t seed,uint32_t num_players)override{
      gen=mt19937(seed);
      init_world(w,gen,num_players);
    }
    int init_from_config(const string&cfg,string&outmsg)override{
      outmsg="no impl";
      return 0;
    }
    string diff(const string&vpow)override{
      return {};
    }
    unique_ptr<i_world> clone()override{return make_unique<t_world_impl>(*this);}
    void renderV0(QapDev&qDev)override{
      static vector<QapColor> player_colors = {
        // Ваши исходные
        0xFFFF8080, // красный
        0xFF80FF80, // зелёный
        0xFF8080FF, // синий
        0xFFFFFF80, // жёлтый

        // Дополнительные — 28 штук
        0xFFFF80FF, // розовый (magenta)
        0xFF80FFFF, // голубой (cyan)
        0xFFFFFF00, // чистый жёлтый (немного ярче)
        0xFFFF0000, // чистый красный
        0xFF00FF00, // чистый зелёный
        0xFF0000FF, // чистый синий
        0xFF800080, // тёмная маджента
        0xFF008080, // тёмный голубой (бирюзовый)
        0xFF808000, // оливковый
        0xFFFFA500, // оранжевый
        0xFFDA70D6, // орхидея
        0xFF9370DB, // средний фиолетовый
        0xFF4B0082, // индиго
        0xFF483D8B, // тёмный сланцевый серо-синий
        0xFF00CED1, // тёмный бирюзовый
        0xFF2E8B57, // морская волна
        0xFF8B4513, // седло-коричневый
        0xFFA0522D, // сиена
        0xFFCD853F, // перу
        0xFFD2691E, // горький шоколад
        0xFFB22222, // кирпичный красный
        0xFF8B0000, // тёмно-красный
        0xFF228B22, // тёмно-зелёный
        0xFF006400, // тёмный зелёный лесной
        0xFF191970, // тёмно-синий (midnight blue)
        0xFF000080, // тёмно-синий (navy)
        0xFF4682B4, // стальной синий
        0xFF6495ED, // василёк
        0xFF7B68EE, // средний сланцевый синий
        0xFFBA55D3, // умеренный орхидейный
        0xFFFF6347, // томатный
        0xFF32CD32  // лаймовый зелёный
      };
  
      // Добавляем случайные цвета при необходимости
      while(player_colors.size() < w.agents.size()){
        player_colors.push_back(QapColor(255, rand()%256, rand()%256, rand()%256));
      }
  
      auto&world = w;
  
      // --- Рисуем арену ---
      qDev.SetColor(0x40000000);
      qDev.DrawCircle(vec2d{}, world.R, 0, 2, 64);
  
      // --- Рисуем центр арены ---
      qDev.SetColor(0x8000FF00); // полупрозрачный зелёный
      qDev.DrawCircle(vec2d{}, 5, 0, 2, 32);
  
      // --- Рисуем агентов ---
      for (int i = 0; i < world.agents.size(); i++) {
        const auto& agent = world.agents[i];
        if (!agent.alive) continue;
    
        vec2d pos = vec2d(agent.x, agent.y);
    
        // Основной цвет агента
        QapColor agent_color = agent.alive ? player_colors[i] : QapColor::HalfMix(player_colors[i],0x40000000);
        qDev.SetColor(agent_color);
    
        // Рисуем основное тело агента
        double agent_radius = 8.0;
        qDev.DrawCircleEx(pos, 0, agent_radius, 16, 0);
    
        // Рисуем контур агента
        qDev.SetColor(0xFF000000); // чёрный контур
        qDev.DrawCircleEx(pos, agent_radius, agent_radius + 1.0, 16, 0);
    
        // Рисуем HP-бар
        double hp_ratio = (double)agent.hp / 100.0;
        QapColor hp_color = hp_ratio > 0.5 ? 0xFF00FF00 : (hp_ratio > 0.25 ? 0xFFFF8000 : 0xFFFF0000);
        qDev.SetColor(hp_color);
        vec2d hp_bar_pos = pos + vec2d(0, -15);
        qDev.DrawRect(hp_bar_pos - vec2d(10, 2), hp_bar_pos + vec2d(10 * hp_ratio, 2));
    
        // Рисуем energy-бар
        double energy_ratio = (double)agent.energy / 100.0;
        qDev.SetColor(0xFF0080FF); // синий для энергии
        vec2d energy_bar_pos = pos + vec2d(0, -10);
        qDev.DrawRect(energy_bar_pos - vec2d(10, 2), energy_bar_pos + vec2d(10 * energy_ratio, 2));
    
        // Рисуем индикатор состояния
        if (agent.attacked_this_tick) {
          qDev.SetColor(0xFFFF0000); // красный при атаке
          qDev.DrawCircleEx(pos, agent_radius + 2, agent_radius + 4, 8, 0);
        }
    
        // Рисуем направление действия если используется
        if (agent.a.use_dir) {
          vec2d dir_vec = vec2d(agent.a.dir.x, agent.a.dir.y).Norm();
          qDev.SetColor(0xFF00FFFF); // голубой для направления
          qDev.DrawLine(pos, pos + dir_vec * 30, 2.0);
        }
    
        // Рисуем индикаторы действий
        double action_indicator_radius = 4.0;
        vec2d indicator_center = pos + vec2d(-15, 15);
    
        // Move to center
        if (agent.a.move_to_center) {
          qDev.SetColor(0xFF0080FF);
          qDev.DrawCircleEx(indicator_center, 0, action_indicator_radius, 8, 0);
        }
    
        // Chase enemy
        if (agent.a.chase_enemy) {
          qDev.SetColor(0xFF00FF00);
          qDev.DrawCircleEx(indicator_center + vec2d(10, 0), 0, action_indicator_radius, 8, 0);
        }
    
        // Repel from enemy
        if (agent.a.repel_from_enemy) {
          qDev.SetColor(0xFFFF0000);
          qDev.DrawCircleEx(indicator_center + vec2d(20, 0), 0, action_indicator_radius, 8, 0);
        }
    
        // Attack
        if (agent.a.attack) {
          qDev.SetColor(0xFFFF8000);
          qDev.DrawCircleEx(indicator_center + vec2d(0, 10), 0, action_indicator_radius, 8, 0);
        }
    
        // Use fury
        if (agent.a.use_fury) {
          qDev.SetColor(0xFFFF00FF);
          qDev.DrawCircleEx(indicator_center + vec2d(10, 10), 0, action_indicator_radius, 8, 0);
        }
    
        // Use rush
        if (agent.a.use_rush) {
          qDev.SetColor(0xFF8000FF);
          qDev.DrawCircleEx(indicator_center + vec2d(20, 10), 0, action_indicator_radius, 8, 0);
        }
    
        // Avoid others
        if (agent.a.avoid_others) {
          qDev.SetColor(0xFFFFFF00);
          qDev.DrawCircleEx(indicator_center + vec2d(0, 20), 0, action_indicator_radius, 8, 0);
        }
    
        // Рисуем линию к цели если есть
        if (agent.a.target >= 0 && agent.a.target < world.agents.size()) {
          const auto& target_agent = world.agents[agent.a.target];
          if (target_agent.alive) {
            qDev.SetColor(0xFF808080); // серая линия к цели
            qDev.DrawLine(pos, vec2d(target_agent.x, target_agent.y), 1.0);
          }
        }
      }
  
      qDev.SetColor(0xFFFFFFFF);
    }
    int get_tick()override{return w.tick;}
  };
  static unique_ptr<i_world> mk_world(int version){
    return make_unique<t_world_impl>();
  }
  #endif
};
