// Realistic infrastructure planner — FIXED VERSION
// English only, accounts for overhead, phased sandboxes, storage

const config = {
  machineTypes: [
    { cores: 4,  pricePerDay: 50 },/*
    { cores: 8,  pricePerDay: 90 },
    { cores: 16, pricePerDay: 170 },
    { cores: 32, pricePerDay: 330 },
    { cores: 64, pricePerDay: 650 }*/
  ],
  diskPricePerGBPerDay: 60 / 5 / 30, // ~0.4 RUB/GB/day

  phases: [
    { name: "S1", type: "sandbox", durationDays: 7, before: "R1", activePlayers: 30, gamesPerPlayerPerHour: 4, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 },
    { name: "R1", type: "round", durationHours: 24, participants: 300, gamesPerParticipant: 10, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 },
    { name: "S2", type: "sandbox", durationDays: 7, before: "R2", activePlayers: 20, gamesPerPlayerPerHour: 4, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 },
    { name: "R2", type: "round", durationHours: 24, participants: 100, gamesPerParticipant: 10, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 },
    { name: "SF", type: "sandbox", durationDays: 7, before: "F", activePlayers: 10, gamesPerPlayerPerHour: 4, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 },
    { name: "F",  type: "round", durationHours: 24, participants: 10,  gamesPerParticipant: 20, ticksPerGame: 20000, msPerTick: 20, playersPerGame: 4, stderrKB: 16, replayBytesPerTick: 100 }
  ]
};

// CPU seconds per game (AI + simulator + Docker overhead)
function cpuSecondsPerGame(phase) {
  const ai = phase.playersPerGame * phase.ticksPerGame * (phase.msPerTick / 1000);
  const sim = phase.playersPerGame * phase.ticksPerGame * (2 / 1000); // 2ms/tick/player
  const docker = phase.playersPerGame * (0.5 + 0.3); // 500ms start + 300ms stop
  return ai + sim + docker;
}

// Wall time per game (for concurrency estimation)
function wallSecondsPerGame(phase) {
  return phase.ticksPerGame * (phase.msPerTick / 1000);
}

// Storage per game (bytes)
function storagePerGame(phase) {
  const stderr = phase.playersPerGame * phase.stderrKB * 1024;
  const replay = phase.playersPerGame * phase.ticksPerGame * phase.replayBytesPerTick;
  return stderr + replay;
}

// Build explicit timeline
function buildTimeline(phases) {
  const rounds = phases.filter(p => p.type === "round");
  const sandboxes = phases.filter(p => p.type === "sandbox");

  let timeline = [];
  let currentTime = 0;

  for (const round of rounds) {
    // Find sandbox that precedes this round
    const sb = sandboxes.find(s => s.before === round.name);
    if (sb) {
      timeline.push({
        ...sb,
        start: currentTime,
        end: currentTime + sb.durationDays,
        isSandbox: true
      });
      currentTime += sb.durationDays;
    }

    const roundDurationDays = round.durationHours / 24;
    timeline.push({
      ...round,
      start: currentTime,
      end: currentTime + roundDurationDays,
      isSandbox: false
    });
    currentTime += roundDurationDays;
  }

  return timeline;
}

// Compute peak cores and total storage
function analyzeTimeline(timeline) {
  let peakCores = 0;
  let totalStorageBytes = 0;

  // Each segment is independent (no overlap)
  for (const seg of timeline) {
    let totalGames = 0;
    let concurrentGames = 0;

    if (seg.isSandbox) {
      const gamesPerPlayer = seg.activePlayers * seg.gamesPerPlayerPerHour * 24 * seg.durationDays;
      totalGames = gamesPerPlayer / seg.playersPerGame;
      // Peak concurrent games in sandbox
      const gamesPerSecond = (seg.activePlayers * seg.gamesPerPlayerPerHour) / 3600;
      concurrentGames = gamesPerSecond * wallSecondsPerGame(seg);
    } else {
      totalGames = (seg.participants * seg.gamesPerParticipant) / seg.playersPerGame;
      // In rounds, we assume we run as fast as possible > concurrency = totalGames / (total_time / game_time)
      const totalSeconds = seg.durationHours * 3600;
      concurrentGames = totalGames / (totalSeconds / wallSecondsPerGame(seg));
    }

    const cores = Math.ceil(concurrentGames * seg.playersPerGame); // each player = 1 core
    if (cores > peakCores) peakCores = cores;

    totalStorageBytes += totalGames * storagePerGame(seg);
  }

  return { peakCores, totalStorageBytes };
}

function chooseMachine(cores, types) {
  for (const m of types) if (m.cores >= cores) return m;
  return types[types.length - 1];
}

// === MAIN ===
const timeline = buildTimeline(config.phases);
const { peakCores, totalStorageBytes } = analyzeTimeline(timeline);
const totalStorageGB = totalStorageBytes / (1024 ** 3);
const totalDays = timeline.length > 0 ? timeline[timeline.length - 1].end : 0;

const machine = chooseMachine(peakCores, config.machineTypes);
const computeCost = machine.pricePerDay * totalDays;
const storageCost = totalStorageGB * config.diskPricePerGBPerDay * totalDays;
const totalCost = computeCost + storageCost;

// === OUTPUT ===
console.log("=== REALISTIC INFRASTRUCTURE PLAN ===\n");

console.log("Timeline:");
timeline.forEach((seg, i) => {
  console.log(`  ${i + 1}. ${seg.name} (${seg.isSandbox ? 'sandbox' : 'round'})`);
  console.log(`      Days: ${seg.start.toFixed(1)} – ${seg.end.toFixed(1)} (duration: ${(seg.end - seg.start).toFixed(1)} days)`);
  console.log(`      Active players: ${seg.isSandbox ? seg.activePlayers : seg.participants}`);
  console.log(`      Estimated games: ${Math.ceil((seg.isSandbox ? (seg.activePlayers * seg.gamesPerPlayerPerHour * 24 * seg.durationDays) : (seg.participants * seg.gamesPerParticipant)) / seg.playersPerGame)}`);
});

console.log(`\nPeak CPU cores required: ${peakCores}`);
console.log(`Total season duration: ${totalDays.toFixed(1)} days`);
console.log(`Machine selected: ${machine.cores} cores @ ${machine.pricePerDay} RUB/day`);
console.log(`Total compute cost: ${computeCost.toFixed(0)} RUB`);
console.log(`Total storage: ${totalStorageGB.toFixed(2)} GB`);
console.log(`Storage cost: ${storageCost.toFixed(0)} RUB`);
console.log(`\nTOTAL COST: ${totalCost.toFixed(0)} RUB`);