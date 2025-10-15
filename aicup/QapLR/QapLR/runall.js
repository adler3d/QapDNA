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
        stdio: ['ignore', 'pipe', 'pipe'], // stdin=ignore, stdout/stderr=pipes
        windowsHide: true,
    });

    // Объединяем stderr в stdout
    child.stderr.on('data', (data) => {
        child.stdout.write(data);
    });

    // Читаем построчно
    const rl = readline.createInterface({
        input: child.stdout,
        crlfDelay: Infinity
    });

    rl.on('line', (line) => {
        const ts = formatTimestamp();
        console.log(`${ts} ${prefix}${line}`);
    });

    rl.on('close', () => {
        child.stdout.destroy();
    });

    return child;
}

// ----------------------------
// Запуск сервера
// ----------------------------
console.log("🚀 Starting QapLR.exe with live output...");

if(0){
  server = runWithLiveOutput(
      './QapLR.exe',
      ['t_splinter', '16', '10', '0', '-p', '31500'],
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
    for(let i=0;i<16;i++)ports.push(31500+i);
    
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