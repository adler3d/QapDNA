struct WaveManagerV1 {
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
 
    //LOG("Preparing to start wave #" + std::to_string(currentWaveNumber+1));
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
struct WaveManager {
  struct WaveStats {
    #define DEF_PRO_COPYABLE()
    #define DEF_PRO_CLASSNAME()WaveStats
    #define DEF_PRO_VARIABLE(ADD)\
    ADD(double,startTimeMS,{})\
    ADD(double,endTimeMS,{})\
    ADD(double,durationMS,{})\
    ADD(int,waveNumber,{})\
    //===
    #include "defprovar.inl"
    //===
  };
  #define DEF_PRO_COPYABLE()
  #define DEF_PRO_CLASSNAME()WaveManager
  #define DEF_PRO_VARIABLE(ADD)\
  ADD(double,roundDurationMS,5000.0)\
  ADD(deque<WaveStats>,waveHistory,{})\
  ADD(int,maxHistorySize,30)\
  ADD(vector<WaveStats>,activeWaves,{})\
  ADD(int,nextWaveNumber,0)\
  ADD(double,lastNSumMS,0.0)\
  ADD(int,lastNCount,0)\
  ADD(int,lastNLimit,3)\
  ADD(double,saveAtMS,0.0)\
  //===
  #include "defprovar.inl"
  //===
  QapClock roundClock;

  void restoreAfterLoad() {
    roundClock.Start();
    for(auto& wave : activeWaves) {
      if(wave.startTimeMS > 0) {
        wave.startTimeMS = (wave.startTimeMS > saveAtMS) ? (wave.startTimeMS - saveAtMS) : 0.0;
      }
    }
    LOG("WaveManager restored: timer reset and wave start times adjusted by saveAtMS=" + std::to_string(saveAtMS) + " ms");
  }

  double predictNextWaveDurationMS() const {
    return (lastNCount == 0) ? 1000 : lastNSumMS / lastNCount;
  }

  double totalActiveWavesDuration() const {
    double sum = 0.0;
    for (const auto& w : activeWaves) sum += w.durationMS;
    return sum;
  }

  bool canStartNewWave() {
    double elapsedMS = roundClock.MS();
    double remainingTimeMS = roundDurationMS - elapsedMS;
    double predictedDurationMS = predictNextWaveDurationMS();
    // Проверяем что остаётся времени на новую волну с учётом текущих активных волн
    return (remainingTimeMS - totalActiveWavesDuration()) >= predictedDurationMS;
  }

  // Запуск новой волны: фиксируем старт, возвращаем номер волны
  int startNewWave() {
    if (!canStartNewWave()) {
      LOG("Not enough time for new wave. Round ended.");
      return -1;
    }
    WaveStats wave;
    wave.startTimeMS = roundClock.MS();
    wave.endTimeMS = 0.0;
    wave.durationMS = 0.0;
    wave.waveNumber = nextWaveNumber++;
    activeWaves.push_back(wave);
    LOG("Wave #" + std::to_string(wave.waveNumber) + " started");
    return wave.waveNumber;
  }

  // Завершение волны по номеру, обновление истории и статистики
  void endWave(int waveNumber) {
    double now = roundClock.MS();
    auto it = std::find_if(activeWaves.begin(), activeWaves.end(),
                           [waveNumber](const WaveStats& w) { return w.waveNumber == waveNumber; });
    if (it == activeWaves.end()) {
      LOG("Trying to end unknown wave: " + std::to_string(waveNumber));
      return;
    }
    it->endTimeMS = now;
    it->durationMS = it->endTimeMS - it->startTimeMS;

    // Обновляем статистику последних N волн
    waveHistory.push_back(*it);
    if (waveHistory.size() > maxHistorySize) {
      // Обновляем сумму для среднего
      lastNSumMS -= waveHistory.front().durationMS;
      waveHistory.pop_front();
    }
    lastNSumMS += it->durationMS;
    lastNCount = std::min(lastNCount + 1, lastNLimit);

    LOG("Wave #" + std::to_string(it->waveNumber) + " completed in " + std::to_string(it->durationMS) + " ms");

    activeWaves.erase(it);
  }

  void startRound(double ms) {
    roundDurationMS = ms;
    roundClock.Start();
    nextWaveNumber = 0;
    waveHistory.clear();
    lastNSumMS = 0.0;
    lastNCount = 0;
    activeWaves.clear();
    LOG("Round started, duration: " + std::to_string(roundDurationMS / 1000.0) + " sec");
  }
};
