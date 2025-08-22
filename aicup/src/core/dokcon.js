const net = require('net');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const AI_BIN_NAME = 'ai.bin';
const TMP_DIR = '.'; // можно на /tmpfs
const SOCKET_PATH = '/tmp/dokcon.sock';

// === ВСТАВКА emitter_on_data_decoder и stream_write_encoder ===
var emitter_on_data_decoder = (emitter, cb) => {
  var rd = Buffer.from([]);
  emitter.on('data', data => {
    rd = Buffer.concat([rd, data]);
    var e = rd.indexOf("\0");
    if (e < 0) return;
    var en = e + 1;
    var zpos = rd.indexOf('\0', en);
    if (zpos < 0) return;
    var zn = zpos + 1;
    var blen = rd.slice(0, e);
    var len = blen.toString("binary") | 0;
    if (rd.length < zn + len) return;
    var bz = rd.slice(en, en + zpos - en);
    var z = bz.toString("binary");
    var bmsg = rd.slice(zn, zn + len);
    var msg = bmsg.toString("binary");
    rd = rd.slice(zn + len);
    cb(z, msg, bz, bmsg);
  });
};

var stream_write_encoder=(stream,z)=>data=>{
  var sep=Buffer.from([0]);
  stream.write(Buffer.concat([
    Buffer.from(!data?"0":(data.length+""),"binary"),sep,
    Buffer.from(z,"binary"),sep,
    Buffer.from(data?data:"","binary")
  ]));
};
/*
var stream_write_encoder = (stream, z) => data => {
  var sep = Buffer.from([0]);

  let payload;
  if (Buffer.isBuffer(data)) {
    payload = data;
  } else {
    payload = Buffer.from(data ? data.toString() : "", "utf8");
  }

  const lenBuf = Buffer.from(payload.length.toString(), "utf8");
  const zBuf = Buffer.from(z, "utf8");

  stream.write(Buffer.concat([
    lenBuf, sep,
    zBuf, sep,
    payload
  ]));
};*/
// === КОНЕЦ ВСТАВКИ ===

// --- Сервер (управляющая программа) ---

async function handleConnection(socket) {
  console.log('Client connected');
  let aiProcess = null;
  socket.setNoDelay(true);
  socket.on('close', () => {
    if (aiProcess) {
      console.log('Killing AI process due to socket close');
      aiProcess.kill();
      aiProcess = null;
    }
  });
  socket.on('error', err => {
    console.error('Socket error:', err);
    if (aiProcess) {
      aiProcess.kill();
      aiProcess = null;
    }
  });
  emitter_on_data_decoder(socket, async (z, msg, bz, bmsg) => {
    if (aiProcess) {
      // Пересылаем в stdin AI
      aiProcess.stdin.write(bmsg); // бинарно!
      return;
    }
    switch (z) {
      case 'ai_binary':
        console.log(`Saving binary of length ${bmsg.length}`);
        const filePath = path.join(TMP_DIR, AI_BIN_NAME);
        try {
          await fs.promises.writeFile(filePath, bmsg, { mode: 0o755 });
          stream_write_encoder(socket, 'log')('Binary saved');
          stream_write_encoder(socket, 'ai_binary_ack')(bmsg.length+"");
        } catch (err) {
          stream_write_encoder(socket, 'log')(`Write error: ${err.message}`);
        }
        break;

      case 'ai_start':
        if (aiProcess) {
          stream_write_encoder(socket, 'log')('AI already running');
          break;
        }
        const aiBin = path.join(TMP_DIR, AI_BIN_NAME);
        if (!fs.existsSync(aiBin)) {
          stream_write_encoder(socket, 'log')('Binary not found');
          break;
        }
        console.log('Starting AI process');
        aiProcess = spawn(aiBin,[],{stdio:['pipe','pipe','pipe'],windowsHide:true});

        // Перенаправляем stdout/stderr через стрим-протокол
        aiProcess.stdout.on('data', stream_write_encoder(socket, 'ai_stdout'));
        aiProcess.stderr.on('data', stream_write_encoder(socket, 'ai_stderr'));

        aiProcess.on('close', (code) => {
          stream_write_encoder(socket, 'log')(`AI exited with code ${code}`);
          aiProcess = null;
        });
        break;

      case 'ai_stop':
        if (aiProcess) {
          aiProcess.kill();
          stream_write_encoder(socket, 'log')('AI killed');
        } else {
          stream_write_encoder(socket, 'log')('No AI process running');
        }
        break;

      default:
        stream_write_encoder(socket, 'log')(`Unknown channel: ${z}`);
    }
  });
}

function startServer(useUnixSocket) {
  if (useUnixSocket) {
    if (fs.existsSync(SOCKET_PATH)) fs.unlinkSync(SOCKET_PATH);
    const server = net.createServer(handleConnection);
    server.listen(SOCKET_PATH, () => {
      console.log(`Server listening on Unix socket ${SOCKET_PATH}`);
    });
  } else {
    const PORT = 4000;
    const server = net.createServer(handleConnection);
    server.listen(PORT, () => {
      console.log(`Server listening on TCP port ${PORT}`);
    });
  }
}

// --- Клиент ---

let lastErrNum = -1;
const c_out = [];
const c_err_lines = [];
let err_buffer = ''; // для незавершённых строк

async function runClient(host, port, binaryFilePath) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    client.connect(port, host, async () => {
      console.log('Connected to server');

      // Читаем бинарник
      const binData = fs.readFileSync(binaryFilePath);

      // Отправляем бинарь
      stream_write_encoder(client, 'ai_binary')(binData);
      console.log('Sent binary');

      // Ждём подтверждение
      emitter_on_data_decoder(client, (z, msg) => {
        if (z === 'log' && msg.includes('Binary saved')) {
          // Запускаем ИИ
          stream_write_encoder(client, 'ai_start')('');
          console.log('Sent start command');
        }

        if (z === 'ai_stdout') {
          console.log('AI stdout:', msg);
          c_out.push(msg);
        }


        if (z === 'ai_stderr') {
          console.error('AI stderr:', JSON.stringify(msg)); // видим сырые фрагменты
          err_buffer += msg;

          // Разбиваем по \n (т.к. endl → \r\n, но \r не мешает)
          const lines = err_buffer.split('\n');
          err_buffer = lines.pop(); // последний — незавершённый

          for (const line of lines) {
            if (!line.trim()) continue;

            if (line.startsWith('ERR')) {
              const numStr = line.slice(3);
              const num = parseInt(numStr, 10);

              if (!isNaN(num)) {
                if (lastErrNum >= 0 && num !== lastErrNum + 1) {
                  console.log(`fail!!! Expected ERR${lastErrNum + 1}, got ERR${num} (line: ${line})`);
                }
                lastErrNum = num;
              } else {
                console.log(`fail!!! ERR followed by non-number: "${numStr}" (line: ${line})`);
              }
            }
            // Можно логировать полную строку
            console.log('Full line:', line);
          }
        }

        if (z === 'log') {
          console.log('Log:', msg);
          if (msg.includes('exited')) {
            client.destroy();
          }
        }
      });

      client.on('close', () => {
        console.log('Connection closed');
        resolve();
      });

      client.on('error', err => {
        reject(err);
      });
    });
  });
}

// --- Запуск ---

if (process.argv[2] === 'server_unix') {
  startServer(true);
} else if (process.argv[2] === 'server_tcp') {
  startServer(false);
} else if (process.argv[2] === 'client_tcp' && process.argv[3] && process.argv[4] && process.argv[5]) {
  const host = process.argv[3];
  const port = parseInt(process.argv[4]);
  const file = process.argv[5];
  runClient(host, port, file).catch(console.error);
} else {
  console.log('Usage: node dokcon.js [server_unix|server_tcp|client_tcp <host> <port> <file>]');
  //node dokcon.js client_tcp 127.0.0.1 4000 ai.exe
}