// Infrastructure planning script for AI competition seasons
// Input: season configuration with phases
// Output: rental plan (machine type, duration, cost)

const config = {
  // Global machine pricing (RUB per day)
  machineTypes: [
    { cores: 4,  pricePerDay: 50 },
    { cores: 32,  pricePerDay: 300 }
    /*,
    { cores: 8,  pricePerDay: 90 },
    { cores: 16, pricePerDay: 170 },
    { cores: 32, pricePerDay: 330 },
    { cores: 64, pricePerDay: 650 }*/
  ],

  // Phases: sandboxes and rounds
  phases: [
    // Sandboxes (continuous load)
    {
      name: "S1",
      type: "sandbox",
      durationDays: 14,
      activePlayers: 100,
      gamesPerPlayerPerHour: 4,
      ticksPerGame: 20000,
      msPerTick: 20,
      playersPerGame: 4
    },
    {
      name: "S2",
      type: "sandbox",
      durationDays: 14,
      activePlayers: 100,
      gamesPerPlayerPerHour: 4,
      ticksPerGame: 20000,
      msPerTick: 20,
      playersPerGame: 4
    },
    {
      name: "SF",
      type: "sandbox",
      durationDays: 14,
      activePlayers: 100,
      gamesPerPlayerPerHour: 4,
      ticksPerGame: 20000,
      msPerTick: 20,
      playersPerGame: 4
    },

    // Rounds (burst load)
    {
      name: "R1",
      type: "round",
      durationHours: 48,
      participants: 75,
      gamesPerParticipant: 125,
      ticksPerGame: 20000,
      msPerTick: 20,
      playersPerGame: 4
    },
    {
      name: "R2",
      type: "round",
      durationHours: 48,
      participants: 50,
      gamesPerParticipant: 250,
      ticksPerGame: 20000,
      msPerTick: 20,
      playersPerGame: 4
    },
    {
      name: "F",
      type: "round",
      durationHours: 48,
      participants: 10,
      gamesPerParticipant: 500,
      ticksPerGame: 20000,
      msPerTick: 20,
      playersPerGame: 4
    }
  ]
};

// Compute wall-clock seconds per game for a phase
function wallSecondsPerGame(phase) {
  return phase.ticksPerGame * (phase.msPerTick / 1000);
}

// Compute total CPU seconds per game for a phase
function cpuSecondsPerGame(phase) {
  return phase.playersPerGame * phase.ticksPerGame * (phase.msPerTick / 1000);
}

// Estimate peak concurrent games in a sandbox
function peakConcurrentGames(sandbox) {
  const gamesPerSecond = (sandbox.activePlayers * sandbox.gamesPerPlayerPerHour) / 3600;
  const gameDuration = wallSecondsPerGame(sandbox);
  return Math.ceil(gamesPerSecond * gameDuration);
}

// Choose smallest machine that fits required cores
function chooseMachine(requiredCores, machineTypes) {
  for (const machine of machineTypes) {
    if (machine.cores >= requiredCores) {
      return machine;
    }
  }
  return machineTypes[machineTypes.length - 1]; // fallback: largest available
}

// Main planning function
function planInfrastructure(config) {
  const plan = {
    phases: [],
    totalCostRUB: 0
  };

  // === 1. Handle all sandboxes (they run in parallel) ===
  let maxSandboxEndDay = 0;
  let totalPeakConcurrentGames = 0;

  const sandboxPhases = config.phases.filter(p => p.type === "sandbox");
  for (const sb of sandboxPhases) {
    const concurrent = peakConcurrentGames(sb);
    totalPeakConcurrentGames += concurrent;
    maxSandboxEndDay = Math.max(maxSandboxEndDay, sb.durationDays);
  }

  if (sandboxPhases.length > 0) {
    const coresNeeded = totalPeakConcurrentGames * 4; // 4 players = 4 cores per game
    const machine = chooseMachine(coresNeeded, config.machineTypes);
    const cost = machine.pricePerDay * maxSandboxEndDay;

    plan.phases.push({
      name: "Sandboxes: " + sandboxPhases.map(p => p.name).join(", "),
      type: "continuous",
      startDay: 0,
      endDay: maxSandboxEndDay,
      coresRequired: coresNeeded,
      machine: machine,
      costRUB: cost
    });

    plan.totalCostRUB += cost;
  }
  /*
твой скрипт не учитывает что кроме ИИ есть ещё куча всего:
  сам симулятор игрового мира жрёт процессорное время пока симулирует мир и в это время всё простаивает, что-то около 2мс на игрока каждый тик.
  неэфективность скрипта который сидит в докер контейнере и пересылает сообщения между хостом и ИИ:
    мы тратим время на запуск докера и ИИ, что-то около 500мс на ИИ.
    мы тратим время на остановку докера и ИИ, тоже что-то около 300мс, но там неизвестно что за нагрузка на проц, в это время и что мы столько ждём.
  нам надо хранить реплеи игр: 4+4*8*3 байт/тик/игрок, но зависит от фазы.
  нам надо хранить stderr(сейчас 16кб/игра/игрок, но это зависит от фазы) ИИ.
  диск стоит 60р/месяц за 5ГБ.

критика скрипта:
  твой скрипт счетает что все песочницы будут работать одновременно 14 дней, а на деле каждая будет активна только 14-7 дней перед раундом/финалом, а всё остальное время заморожена.
  сейчас ты просто передаёшь желаемое кол-во ядер в chooseMachine и там выбираешь по этому параметру такой тариф в котором не меньше ядер и всё, это что вся логика скрипта? так не пойдёт.
  скрипт - это рекомендательная система, она должна говорить:
    надо столько-то ядерных машин на столько то времени.
    иначе если будет столько-то 
    
*/
  // === 2. Handle rounds (sequential bursts) ===
  let currentDay = maxSandboxEndDay;

  const roundPhases = config.phases.filter(p => p.type === "round");
  for (const round of roundPhases) {
    const totalGames = Math.ceil((round.participants * round.gamesPerParticipant) / round.playersPerGame);
    const totalCpuSeconds = totalGames * cpuSecondsPerGame(round);
    const availableSeconds = round.durationHours * 3600;

    const requiredCores = Math.ceil(totalCpuSeconds / availableSeconds);
    const machine = chooseMachine(requiredCores, config.machineTypes);
    const durationDays = round.durationHours / 24;
    const cost = machine.pricePerDay * durationDays;

    plan.phases.push({
      name: round.name,
      type: "burst",
      startDay: currentDay,
      endDay: currentDay + durationDays,
      coresRequired: requiredCores,
      machine: machine,
      costRUB: cost,
      details: {
        totalGames: totalGames,
        totalCpuHours: (totalCpuSeconds / 3600).toFixed(1)
      }
    });

    plan.totalCostRUB += cost;
    currentDay += durationDays;
  }

  return plan;
}

// === Run and output ===
const plan = planInfrastructure(config);

console.log("=== INFRASTRUCTURE RENTAL PLAN ===\n");

plan.phases.forEach(phase => {
  console.log(`Phase: ${phase.name}`);
  console.log(`  Type: ${phase.type}`);
  console.log(`  Time: day ${phase.startDay.toFixed(1)} to ${phase.endDay.toFixed(1)}`);
  console.log(`  Required cores: ${phase.coresRequired}`);
  console.log(`  Machine: ${phase.machine.cores} cores @ ${phase.machine.pricePerDay} RUB/day`);
  console.log(`  Cost: ${Math.round(phase.costRUB)} RUB`);
  if (phase.details) {
    console.log(`  Details: ${phase.details.totalGames} games, ${phase.details.totalCpuHours} CPU-hours`);
  }
  console.log();
});

console.log(`TOTAL COST: ${Math.round(plan.totalCostRUB)} RUB`);