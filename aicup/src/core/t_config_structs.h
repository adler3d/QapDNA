struct TQualRuleConfig {
  string fromPhaseName;
  uint64_t topN = 0;
};

struct TPhaseConfig {
  string phaseName;
  string type; // "sandbox" or "round"
  string world;
  uint64_t durationDays = 7;
  uint64_t durationHours = 24;
  uint64_t activePlayers = 0;
  uint64_t participants = 0;
  uint64_t gamesPerParticipant = 0;
  uint64_t ticksPerGame = 7500;
  uint64_t msPerTick = 35;
  uint64_t playersPerGame = 4;
  uint64_t stderrKb = 16;
  uint64_t replayBytesPerTick = 100;
  double gamesPerPlayerPerHour = 1.0;

  vector<TQualRuleConfig> qualifyingFrom;
  string startTime;
};

struct TSeasonConfig {
  string seasonName;
  string startTime;
  vector<TPhaseConfig> phases;
};

void from_json(const json& j, TQualRuleConfig& r) {
  j.at("fromPhaseName").get_to(r.fromPhaseName);
  j.at("topN").get_to(r.topN);
}

void from_json(const json& j, TPhaseConfig& p) {
  j.at("phaseName").get_to(p.phaseName);
  j.at("type").get_to(p.type);
  j.at("world").get_to(p.world);

  if (j.contains("durationDays")) j.at("durationDays").get_to(p.durationDays);
  if (j.contains("durationHours")) j.at("durationHours").get_to(p.durationHours);
  if (j.contains("activePlayers")) j.at("activePlayers").get_to(p.activePlayers);
  if (j.contains("participants")) j.at("participants").get_to(p.participants);
  if (j.contains("gamesPerParticipant")) j.at("gamesPerParticipant").get_to(p.gamesPerParticipant);
  if (j.contains("ticksPerGame")) j.at("ticksPerGame").get_to(p.ticksPerGame);
  if (j.contains("msPerTick")) j.at("msPerTick").get_to(p.msPerTick);
  if (j.contains("playersPerGame")) j.at("playersPerGame").get_to(p.playersPerGame);
  if (j.contains("stderrKb")) j.at("stderrKb").get_to(p.stderrKb);
  if (j.contains("replayBytesPerTick")) j.at("replayBytesPerTick").get_to(p.replayBytesPerTick);
  if (j.contains("gamesPerPlayerPerHour")) j.at("gamesPerPlayerPerHour").get_to(p.gamesPerPlayerPerHour);

  if (j.contains("qualifyingFrom")) {
    j.at("qualifyingFrom").get_to(p.qualifyingFrom);
  }
  j.at("startTime").get_to(p.startTime);
}

void from_json(const json& j, TSeasonConfig& cfg) {
  j.at("seasonName").get_to(cfg.seasonName);
  j.at("startTime").get_to(cfg.startTime);
  j.at("phases").get_to(cfg.phases);
}