// Hybrid planner — FIXED: sandboxes have fixed duration, storage is cumulative
// All output in English
let ticks=7500; let TL=35*0+100; let stderrKB=64;
let config = {
  machineTypes: [
    { cores: 4,  pricePerDay: 50 },/*
    { cores: 8,  pricePerDay: 90 },
    { cores: 16, pricePerDay: 170 },
    { cores: 32, pricePerDay: 330 },
    { cores: 64, pricePerDay: 650 }*/
  ],
  diskPricePerGBPerDay: 60 / 5 / 30, // ~0.4 RUB/GB/day
  seasonName: "splinter_2025",
  startTime: "2025.11.03 00:00:00.000",
  phases1: [
    { name: "S1", type: "sandbox", durationHours: 24*7, coders:  75, gamesPerCoderPerHour: 1, ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 },
    { name: "R1", type: "round",   durationHours: 24,   coders:  50, gamesPerCoder: 2*24,     ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 },
    { name: "S2", type: "sandbox", durationHours: 24*7, coders:  50, gamesPerCoderPerHour: 1, ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 },
    { name: "R2", type: "round",   durationHours: 24,   coders:  25, gamesPerCoder: 2*48,     ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 },
    { name: "SF", type: "sandbox", durationHours: 24*7, coders:  25, gamesPerCoderPerHour: 1, ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 },
    { name: "F",  type: "round",   durationHours: 24,   coders:  10, gamesPerCoder: 250,      ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 }
  ],
  phases2: [
    { name: "S1", type: "sandbox", durationHours: 24*14,coders:  1000, gamesPerCoderPerHour: 1, ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 },
    { name: "R1", type: "round",   durationHours: 24,   coders:   900, gamesPerCoder: 2*24,     ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 },
    { name: "S2", type: "sandbox", durationHours: 24*7, coders:   500, gamesPerCoderPerHour: 1, ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 },
    { name: "R2", type: "round",   durationHours: 24,   coders:   360, gamesPerCoder: 2*48,     ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 },
    { name: "SF", type: "sandbox", durationHours: 24*7, coders:   250, gamesPerCoderPerHour: 1, ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 },
    { name: "F",  type: "round",   durationHours: 24,   coders:    60, gamesPerCoder: 250,      ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB, replayBytesPerTick: 100 }
  ],
  phases: [
    { name: "S1", type: "sandbox", durationHours: 24*3, coders:  1000, gamesPerCoderPerHour: 1, ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB: stderrKB, replayBytesPerTick: 100 },
    { name: "R1", type: "round",   durationHours: 24,   coders:   900, gamesPerCoder: 2*24,     ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB: stderrKB, replayBytesPerTick: 100 },
    { name: "S2", type: "sandbox", durationHours: 24*3, coders:   500, gamesPerCoderPerHour: 1, ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB: stderrKB, replayBytesPerTick: 100 },
    { name: "R2", type: "round",   durationHours: 24,   coders:   360, gamesPerCoder: 2*48,     ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB: stderrKB, replayBytesPerTick: 100 },
    { name: "SF", type: "sandbox", durationHours: 24*3, coders:   250, gamesPerCoderPerHour: 1, ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB: stderrKB, replayBytesPerTick: 100 },
    { name: "F",  type: "round",   durationHours: 24,   coders:    60, gamesPerCoder: 250,      ticksPerGame: ticks, msPerTick: TL, playersPerGame: 4, stderrKB: stderrKB, replayBytesPerTick: 100 }
  ]
};
config.phases1=[];
config.phases2=[];
let time_scale=1.0+0*3.0/24/60;
config.phases.map(e=>{e.world="t_splinter";e.durationHours*=time_scale;});
let get_phase=p=>config.phases.filter(e=>e.name===p)[0];
get_phase("R1").qualifyingFrom=[{"fromPhaseName":"S1","topN":900}];
get_phase("R2").qualifyingFrom=[{"fromPhaseName":"R1","topN":300},{"fromPhaseName":"S2","topN":60}];
get_phase("F").qualifyingFrom=[{"fromPhaseName":"R2","topN":50},{"fromPhaseName":"SF","topN":10}];
config.phases.map(e=>e.max_concurrent_waves=(e.type==="round"?3:1));
let sec_shift=(t,s)=>{t.setSeconds(t.getSeconds()+s);};
let t0=new Date();sec_shift(t0,+30);
let config2=buildAbsoluteConfig(t0.toJSON(),config.phases);
delete config.phases;
config={...config,...config2};
function buildAbsoluteConfig(seasonStart, phases) {
  let currentTime = new Date(seasonStart); // например, "2025-11-03T00:00:00.000Z"
  let endTime=currentTime;
  const result = {
    seasonName: "splinter_2025",
    startTime: formatDate(currentTime),
    phases: []
  };

  for (const p of phases) {
    const phaseStart = new Date(currentTime);
    sec_shift(currentTime,p.durationHours*3600);
    result.phases.push({
      ...p,
      startTime: formatDate(phaseStart),
      endTime: formatDate(new Date(currentTime)),
    });
  }

  return result;
}

function formatDate(d) {
  // Формат: "2025.11.03 00:00:00.000"
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, '0');
  const day = String(d.getDate()).padStart(2, '0');
  const h = String(d.getHours()).padStart(2, '0');
  const min = String(d.getMinutes()).padStart(2, '0');
  const sec = String(d.getSeconds()).padStart(2, '0');
  return `${y}.${m}.${day} ${h}:${min}:${sec}.000`;
}

console.log(JSON.stringify(config,0,2));

function cpuSecondsPerGame(phase) {
  const ai = phase.playersPerGame * phase.ticksPerGame * (phase.msPerTick / 1000);
  const sim = phase.playersPerGame * phase.ticksPerGame * (2 / 1000);
  const docker = phase.playersPerGame * (0.5 + 0.3);
  return ai + sim + docker;
}

function wallSecondsPerGame(phase) {
  return phase.ticksPerGame * (phase.msPerTick / 1000);
}

function storagePerGame(phase) {
  const stderr = phase.playersPerGame * phase.stderrKB * 1024;
  const replay = phase.playersPerGame * phase.ticksPerGame * phase.replayBytesPerTick;
  return stderr + replay;
}

function chooseMachine(requiredCores, types) {
  for (const m of types) if (m.cores >= requiredCores) return m;
  return types[types.length - 1];
}

function planHybrid(config) {
  const plan = { phases: [], totalComputeCost: 0, totalStorageGB: 0, totalDays: 0 };
  let currentTime = 0;
  let totalStorageBytes = 0;

  for (const phase of config.phases) {
    let requiredCores = 4; // default
    let totalGames = 0;

    if (phase.type === "sandbox") {
      // Fixed duration: 7 days
      // Compute peak concurrent games
      const gamesPerSecond = (phase.coders * phase.gamesPerCoderPerHour) / 3600;
      const concurrentGames = gamesPerSecond * wallSecondsPerGame(phase);
      requiredCores = /*Math.ceil*/(concurrentGames * phase.playersPerGame).toFixed(2); // each player = 1 core
      totalGames = (phase.coders * phase.gamesPerCoderPerHour * phase.durationHours) / phase.playersPerGame;
    } else {
      // Round: must finish in fixed time
      totalGames = (phase.coders * phase.gamesPerCoder) / phase.playersPerGame;
      const totalCpuSec = totalGames * cpuSecondsPerGame(phase);
      const availableSec = phase.durationHours * 3600;
      requiredCores = /*Math.ceil*/(totalCpuSec / availableSec).toFixed(2);
    }

    const machine = chooseMachine(requiredCores, config.machineTypes);
    const durationDays = phase.durationHours / 24;
    const computeCost = machine.pricePerDay * durationDays*Math.ceil(requiredCores/machine.cores);
    const storageBytes = totalGames * storagePerGame(phase);

    totalStorageBytes += storageBytes;
    plan.phases.push({
      name: phase.name,
      type: phase.type,
      durationDays,
      totalGames: Math.ceil(totalGames),
      requiredCores,
      machine,
      computeCost
    });

    plan.totalComputeCost += computeCost;
    currentTime += durationDays;
  }

  plan.totalDays = currentTime;
  plan.totalStorageGB = totalStorageBytes / (1024 ** 3);
  // Store all data for 30 days after season
  const storageCost = plan.totalStorageGB * config.diskPricePerGBPerDay * 30;
  plan.totalStorageCost = storageCost;
  plan.totalCost = plan.totalComputeCost + storageCost;

  return plan;
}

// === RUN ===
const plan = planHybrid(config);

console.log("=== CORRECTED HYBRID INFRASTRUCTURE PLAN ===\n");

plan.phases.forEach(p => {
  console.log(`Phase: ${p.name} (${p.type})`);
  console.log(`  Duration: ${p.durationDays} days (fixed)`);
  console.log(`  Games: ${p.totalGames}`);
  console.log(`  Required cores: ${p.requiredCores}`);
  console.log(`  Machine: ${p.machine.cores} cores @ ${p.machine.pricePerDay} RUB/day`);
  console.log(`  Compute cost: ${p.computeCost.toFixed(0)} RUB`);
  console.log();
});

console.log(`TOTAL SEASON DURATION: ${plan.totalDays} days`);
console.log(`TOTAL STORAGE: ${plan.totalStorageGB.toFixed(2)} GB (stored 30 days)`);
console.log(`COMPUTE COST: ${plan.totalComputeCost.toFixed(0)} RUB`);
console.log(`STORAGE COST: ${plan.totalStorageCost.toFixed(0)} RUB`);
console.log(`TOTAL COST: ${plan.totalCost.toFixed(0)} RUB`);