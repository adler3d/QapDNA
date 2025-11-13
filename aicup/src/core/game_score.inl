//2025.11.13 17:21:10.860
struct t_score {
  double rating;
  double volatility;
};

struct t_player_with_score {
  uint64_t uid;
  t_score current;   // Текущее состояние
  t_score next;     // Вычисленное следующее состояние
  double game_score;
  double smart_score;
};

// Вспомогательные функции для работы с t_score
t_score create_score(double rating = 1200.0, double volatility = 150.0) {
  return {rating, volatility};
}

bool operator==(const t_score& a, const t_score& b) {
  return a.rating == b.rating && a.volatility == b.volatility;
}

t_player_with_score create_new_player(uint64_t uid, double initial_rating = 1200.0) {
  return {uid, create_score(initial_rating, 150.0), create_score(), 0.0};
}

void major_update(t_player_with_score& player) {
  player.current.volatility = std::min(player.current.volatility * 1.5, 200.0);
}

struct t_player_rank {
  uint64_t uid;
  double game_score;
  int original_rank;
  double adjusted_rank;
  double*smart_score;
};

// Функция для вычисления мест на основе game_score
vector<t_player_rank> calculate_ranks(vector<t_player_with_score>& players) {
  vector<t_player_rank> ranks;
  ranks.reserve(players.size());
  
  for (auto& player : players) {
    ranks.push_back({player.uid, player.game_score, 0, 0.0, &player.smart_score});
  }
  
  std::sort(ranks.begin(), ranks.end(), 
       [](const t_player_rank& a, const t_player_rank& b) {
         return a.game_score > b.game_score;
       });
  
  size_t i = 0;
  while (i < ranks.size()) {
    double current_score = ranks[i].game_score;
    size_t start_index = i;
    size_t count_same = 0;
    
    while (i < ranks.size() && std::abs(ranks[i].game_score - current_score) < 1e-9) {
      count_same++;
      i++;
    }
    
    double average_rank = start_index + (count_same + 1) / 2.0;
    
    for (size_t j = start_index; j < start_index + count_same; j++) {
      ranks[j].original_rank = start_index + 1;
      ranks[j].adjusted_rank = average_rank;
    }
  }
  
  return ranks;
}

// Преобразование мест в умный game_score (от 0 до 1)
void convert_to_smart_score(vector<t_player_rank>& ranks) {
  if (ranks.empty()) return;
  
  size_t n = ranks.size();
  
  for (auto& rank : ranks) {
    auto&out=*rank.smart_score;
    out = 1.0 - (rank.adjusted_rank - 1.0) / (n - 1.0);
    //out = std::max(0.0, std::min(1.0, out));
  }
}

void update_smart_scores(vector<t_player_with_score>& players) {
  auto ranks = calculate_ranks(players);
  convert_to_smart_score(ranks);
}

void update_score_with_smart_ranking(vector<t_player_with_score>& players, double delta_time = 1.0) {
  constexpr double K = 32.0;
  constexpr double VOLATILITY_DECAY = 0.9;
  constexpr double MAX_VOLATILITY = 200.0;
  constexpr double MIN_VOLATILITY = 10.0;
  size_t n = players.size();
  if (n < 2) return;
  update_smart_scores(players);
  for (size_t i = 0; i < n; ++i) {
    double Ri = players[i].current.rating;
    double Vi = players[i].current.volatility;
    
    double expected_score = 0.0;
    for (size_t j = 0; j < n; ++j) {
      if (i == j) continue;
      
      double Rj = players[j].current.rating;
      double Vj = players[j].current.volatility;
      
      double combined_variance = sqrt(Vi * Vi + Vj * Vj);
      double expected = 1.0 / (1.0 + pow(10.0, (Rj - Ri) / (400.0 + combined_variance)));
      expected_score += expected;
    }
    expected_score /= (n - 1);
    
    double actual_score = players[i].smart_score;
    
    double dynamic_K = K * (Vi / MIN_VOLATILITY);
    dynamic_K = std::max(K * 0.5, std::min(K * 2.0, dynamic_K));
    
    double rating_change = dynamic_K * (actual_score - expected_score);
    double new_rating = Ri + rating_change;
    
    double new_volatility;
    if (abs(rating_change) > 50) {
      new_volatility = Vi * 1.2 + abs(rating_change) * 0.1;
    } else {
      new_volatility = Vi * VOLATILITY_DECAY;
    }
    new_volatility = std::max(MIN_VOLATILITY, std::min(MAX_VOLATILITY, new_volatility));
    
    players[i].next.rating = new_rating;
    players[i].next.volatility = new_volatility;
  }
  for (size_t i = 0; i < n; ++i) {
    players[i].current = players[i].next;
  }
}

void print_player_states(const vector<t_player_with_score>& players) {
  printf("Player States:\n");
  printf("UID\t\tCur_Rating\tCur_Vol\t\tNext_Rating\tNext_Vol\tGame_Score\n");
  for (const auto& player : players) {
    printf("%llu\t\t%.1f\t\t%.1f\t\t%.1f\t\t%.1f\t\t%.2f\n", 
        player.uid, 
        player.current.rating, player.current.volatility,
        player.next.rating, player.next.volatility,
        player.game_score);
  }
}

void prepare_new_round(vector<t_player_with_score>& players) {
  for (auto& player : players) {
    player.game_score = 0.0;
    player.next = create_score();
  }
}