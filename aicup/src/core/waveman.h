struct WaveManager {
  struct WaveStats {
    double startTimeMS;
    double endTimeMS;
    double durationMS;
  };
  QapClock roundClock;
  double roundDurationMS=5000.0;
  std::vector<WaveStats> waveHistory; // stores max 3 waves
  int currentWaveNumber = 0;
  double last3SumMS = 0.0;  // sum of durations of last 3 waves
  int last3Count = 0;    // count of waves in window (0..3)
  double predictNextWaveDurationMS() const {
    return !last3Count?1000:last3SumMS / last3Count;
  }
  bool canStartNextWave() {
    double elapsedMS = roundClock.MS();
    double remainingTimeMS = roundDurationMS - elapsedMS;
    double predictedDurationMS = predictNextWaveDurationMS();
    return remainingTimeMS >= predictedDurationMS;
  }
  void addWaveAndUpdateWindow(const WaveStats& wave) {
    int limit=30;
    if (last3Count == limit) {
      last3SumMS -= waveHistory[0].durationMS; // remove oldest
    }
    waveHistory.push_back(wave);
    if (waveHistory.size() > limit) {
      waveHistory.erase(waveHistory.begin());
    }
    last3SumMS += wave.durationMS;
    last3Count = std::min(last3Count + 1, limit);
  }
  void startRound(double ms) {
    roundDurationMS=ms;
    roundClock.Start();
    currentWaveNumber = 0;
    waveHistory.clear();
    last3SumMS = 0.0;
    last3Count = 0;
    LOG("Round started, duration: " + std::to_string(roundDurationMS / 1000.0) + " sec");
  }
  bool tryStartNextWave() {
    if (!canStartNextWave()) {
      LOG("Not enough time for new wave. Round ended.");
      return false;
    }
 
    LOG("Preparing to start wave #" + std::to_string(currentWaveNumber+1));
    //markWaveStart();
    return true;
  }
  double currentWaveStartTime = 0.0;
 
  void markWaveStart() {
    currentWaveNumber++;
    currentWaveStartTime = roundClock.MS();
    LOG("Wave #" + std::to_string(currentWaveNumber) + " started");
  }
 
  void markWaveEnd() {
    WaveStats wave;
    wave.startTimeMS = currentWaveStartTime;
    wave.endTimeMS = roundClock.MS();
    wave.durationMS = wave.endTimeMS - wave.startTimeMS;
 
    addWaveAndUpdateWindow(wave);
 
    LOG("Wave #" + std::to_string(currentWaveNumber) +
      " completed in " + std::to_string(wave.durationMS) + " ms");
  }
};