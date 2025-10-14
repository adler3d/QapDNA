#!/usr/bin/env node
//прост тестил как работает сокет адаптер, вроде работает, больше этот файл не нужен.
const net = require('net');

function printHelp() {
    console.log(`
Usage: node tcp_logger.js [host] <port>

  host   - IP to bind to (default: 127.0.0.1)
  port   - TCP port to listen on (required)

Examples:
  node tcp_logger.js 12345
  node tcp_logger.js 0.0.0.0 12345
  node tcp_logger.js ::1 12345   # IPv6 localhost
`);
}

function main() {
    const args = process.argv.slice(2);
    let host, port;

    if (args.length === 1) {
        host = '127.0.0.1';
        port = parseInt(args[0], 10);
    } else if (args.length === 2) {
        host = args[0];
        port = parseInt(args[1], 10);
    } else {
        printHelp();
        process.exit(1);
    }

    if (isNaN(port) || port <= 0 || port > 65535) {
        console.error('❌ Invalid port:', args[args.length - 1]);
        process.exit(1);
    }

    const server = net.createServer((socket) => {
        const remote = `${socket.remoteAddress}:${socket.remotePort}`;
        console.log(`📥 New connection from ${remote}`);

        socket.on('data', (data) => {
            // Попытка декодировать как UTF-8
            let text;
            try {
                text = data.toString('utf8');
                // Проверим, нет ли "битых" символов (часто при бинарных данных)
                /*if (text.includes('')) {
                    throw new Error('Invalid UTF-8');
                }*/
            } catch (e) {
                // Если не UTF-8 — выводим hex
                console.log(`[BINARY from ${remote}] ${data.toString('hex')}`);
                return;
            }

            // Печатаем текст построчно, чтобы не нарушать логи
            const lines = text.split(/\r?\n/);
            lines.forEach(line => {
                if (line !== '') {
                    console.log(`[${remote}] ${line}`);
                }
            });
        });

        socket.on('end', () => {
            console.log(`🔌 Connection from ${remote} closed`);
        });

        socket.on('error', (err) => {
            console.error(`⚠️ Socket error (${remote}):`, err.message);
        });
    });

    server.on('error', (err) => {
        console.error('❌ Server error:', err.message);
        process.exit(1);
    });

    server.listen(port, host, () => {
        const addr = server.address();
        console.log(`📡 Listening on ${addr.family === 'IPv6' ? `[${addr.address}]` : addr.address}:${addr.port}`);
        console.log('Press Ctrl+C to exit\n');
    });

    // Корректное завершение
    process.on('SIGINT', () => {
        console.log('\n🛑 Shutting down...');
        process.exit(0);
        server.close(() => {
            console.log('✅ Server closed');
            process.exit(0);
        });
    });
}

main();