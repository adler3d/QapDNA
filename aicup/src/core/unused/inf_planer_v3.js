// Hybrid infrastructure planner: use 4-core by default, scale up only when needed
// All output in English

const config = {
  // Available machine types (RUB per day)
  machineTypes: [
    { cores: 4,  pricePerDay: 50 },
    { cores: 8,  pricePerDay: 90 },
    { cores: 16, pricePerDay: 170 },
    { cores: 32, pricePerDay: 330 },
    { cores: 64, pricePerDay: 650 }
  ],
  diskPricePerGBPerDay: 60 / 5 / 30, // ~0.4 RUB/GB/day

  // Phases with hard deadlines (sandboxes: flexible; rounds: fixed duration)
  phases: [
    { name: "S1", type: "sandbox", maxDurationDays: 7, activePlayers: 30, gamesPerPlayerPerHour: 4, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 },
    { name: "R1", type: "round",   maxDurationHours: 24, participants: 300, gamesPerParticipant: 10, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 },
    { name: "S2", type: "sandbox", maxDurationDays: 7, activePlayers: 20, gamesPerPlayerPerHour: 4, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 },
    { name: "R2", type: "round",   maxDurationHours: 24, participants: 100, gamesPerParticipant: 10, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 },
    { name: "SF", type: "sandbox", maxDurationDays: 7, activePlayers: 10, gamesPerPlayerPerHour: 4, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 },
    { name: "F",  type: "round",   maxDurationHours: 24, participants: 10,  gamesPerParticipant: 20, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 }
  ]
};

// CPU seconds per game (AI + simulator + Docker startup/shutdown)
function cpuSecondsPerGame(phase) {
  const aiCpu = phase.playersPerGame * phase.ticksPerGame * (phase.msPerTick / 1000);
  const simCpu = phase.playersPerGame * phase.ticksPerGame * (2 / 1000); // 2ms/tick/player
  const dockerOverhead = phase.playersPerGame * (0.5 + 0.3); // 500ms start + 300ms stop
  return aiCpu + simCpu + dockerOverhead;
}

// Storage per game (bytes)
function storagePerGame(phase) {
  const stderr = phase.playersPerGame * phase.stderrKB * 1024;
  const replay = phase.playersPerGame * phase.ticksPerGame * phase.replayBytesPerTick;
  return stderr + replay;
}

// Choose smallest machine that can finish the phase on time
function selectMachineForPhase(phase, machineTypes) {
  const totalGames = phase.type === "sandbox"
    ? (phase.activePlayers * phase.gamesPerPlayerPerHour * 24 * phase.maxDurationDays) / phase.playersPerGame
    : (phase.participants * phase.gamesPerParticipant) / phase.playersPerGame;

  const totalCpuSeconds = totalGames * cpuSecondsPerGame(phase);
  const maxAllowedSeconds = phase.type === "sandbox"
    ? phase.maxDurationDays * 24 * 3600
    : phase.maxDurationHours * 3600;

  const minCoresNeeded = Math.ceil(totalCpuSeconds / maxAllowedSeconds);

  // Find smallest machine that meets core requirement
  for (const machine of machineTypes) {
    if (machine.cores >= minCoresNeeded) {
      return { machine, actualDurationDays: totalCpuSeconds / machine.cores / 86400 };
    }
  }

  // Fallback: largest machine
  const largest = machineTypes[machineTypes.length - 1];
  return { machine: largest, actualDurationDays: totalCpuSeconds / largest.cores / 86400 };
}

// Main planning function
function planHybridInfrastructure(config) {
  const plan = {
    phases: [],
    totalComputeCost: 0,
    totalStorageCost: 0,
    totalStorageGB: 0,
    totalDays: 0
  };

  let currentTime = 0;

  for (const phase of config.phases) {
    const totalGames = phase.type === "sandbox"
      ? Math.ceil((phase.activePlayers * phase.gamesPerPlayerPerHour * 24 * phase.maxDurationDays) / phase.playersPerGame)
      : Math.ceil((phase.participants * phase.gamesPerParticipant) / phase.playersPerGame);

    const { machine, actualDurationDays } = selectMachineForPhase(phase, config.machineTypes);
    const storageBytes = totalGames * storagePerGame(phase);
    const storageGB = storageBytes / (1024 ** 3);
    const storageCost = storageGB * config.diskPricePerGBPerDay * actualDurationDays;
    const computeCost = machine.pricePerDay * actualDurationDays;

    const phasePlan = {
      name: phase.name,
      type: phase.type,
      totalGames,
      startDay: currentTime,
      endDay: currentTime + actualDurationDays,
      durationDays: actualDurationDays,
      machine: machine,
      storageGB,
      computeCost,
      storageCost
    };

    plan.phases.push(phasePlan);
    plan.totalComputeCost += computeCost;
    plan.totalStorageCost += storageCost;
    plan.totalStorageGB += storageGB;
    currentTime += actualDurationDays;
  }

  plan.totalDays = currentTime;
  plan.totalCost = plan.totalComputeCost + plan.totalStorageCost;

  return plan;
}

// === RUN ===
const plan = planHybridInfrastructure(config);

console.log("=== HYBRID INFRASTRUCTURE PLAN ===\n");

plan.phases.forEach(p => {
  console.log(`Phase: ${p.name} (${p.type})`);
  console.log(`  Games: ${p.totalGames}`);
  console.log(`  Duration: ${p.durationDays.toFixed(2)} days`);
  console.log(`  Machine: ${p.machine.cores} cores @ ${p.machine.pricePerDay} RUB/day`);
  console.log(`  Compute cost: ${p.computeCost.toFixed(0)} RUB`);
  console.log(`  Storage: ${p.storageGB.toFixed(2)} GB (${p.storageCost.toFixed(0)} RUB)`);
  console.log();
});

console.log(`TOTAL SEASON DURATION: ${plan.totalDays.toFixed(1)} days`);
console.log(`TOTAL COMPUTE COST: ${plan.totalComputeCost.toFixed(0)} RUB`);
console.log(`TOTAL STORAGE COST: ${plan.totalStorageCost.toFixed(0)} RUB`);
console.log(`TOTAL COST: ${plan.totalCost.toFixed(0)} RUB`);