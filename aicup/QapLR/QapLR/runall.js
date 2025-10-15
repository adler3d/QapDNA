#!/usr/bin/env node

const { spawn } = require('child_process');
const readline = require('readline');

// Время старта скрипта
const startTime = process.hrtime.bigint();

// Форматирование времени: прошло с начала в миллисекундах, с двумя знаками после запятой
function formatTimestamp() {
    const now = process.hrtime.bigint();
    const diffNs = now - startTime;
    const diffMs = Number(diffNs) / 1_000_000; // наносекунды → миллисекунды
    return diffMs.toFixed(2).padStart(8, ' '); // например: "  123.45"
}

// Функция для запуска процесса с логированием
function runWithLiveOutput(cmd, args, prefix) {
    const child = spawn(cmd, args, {
        stdio: ['ignore', 'pipe', 'pipe'],
        windowsHide: true,
    });

    // Обрабатываем stdout
    readline.createInterface({ input: child.stdout, crlfDelay: Infinity })
        .on('line', (line) => {
            const ts = formatTimestamp();
            console.log(`${ts} ${prefix}${line}`);
        });

    // Обрабатываем stderr (например, с пометкой [ERR])
    readline.createInterface({ input: child.stderr, crlfDelay: Infinity })
        .on('line', (line) => {
            const ts = formatTimestamp();
            console.log(`${ts} ${prefix}[STDERR] ${line}`);
        });

    return child;
}

// ----------------------------
// Запуск сервера
// ----------------------------
console.log("🚀 Starting QapLR.exe with live output...");
let N=12; let port=31500;
if(1){
  server = runWithLiveOutput(
      './QapLR.exe',
      ['t_splinter', N, '11', '0', '-p', port,'-g'],
      '[QapLR] '
  );
}else server={on:()=>0};

// Ждём, пока сервер стартует
setTimeout(() => {
    // ----------------------------
    // Запуск клиентов
    // ----------------------------
    const clients = [];
    const ports = [];//[31500, 31501, 31502, 31503];
    for(let i=0;i<N-1;i++)ports.push(port+i);
    const client = runWithLiveOutput(
      './socket_adapter.exe',
      ['-e', `127.0.0.1:${port+N-1}`, './adlerN0907.exe'],
      `[AdlerN0907] `
    );
    ports.forEach((port, i) => {
        console.log(`🔌 Starting client ${i + 1} for port ${port}...`);
        const client = runWithLiveOutput(
            './socket_adapter.exe',
            ['-e', `127.0.0.1:${port}`, './gpt5.exe'],
            `[Client-${i}] `
        );
        clients.push(client);
    });

    console.log("✅ All processes started.");

    // ----------------------------
    // Ожидание завершения сервера
    // ----------------------------
    server.on('close', (code) => {
        console.log(`\n✅ QapLR exited with code ${code}`);
        // Опционально: завершить клиентов
        clients.forEach(c => {
            if (!c.killed) c.kill();
        });
        process.exit(code === 0 ? 0 : 1);
    });

    // Обработка Ctrl+C
    process.on('SIGINT', () => {
        console.log('\n🛑 Interrupted. Terminating...');
        server.kill();
        clients.forEach(c => {
            if (!c.killed) c.kill();
        });
        process.exit(130);
    });

}, 100); // 100 мс задержка (как time.sleep(0.1))