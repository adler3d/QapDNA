const net = require('net');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const AI_BIN_NAME = 'ai.bin';
const TMP_DIR = '.';//'/tmpfs'; // или любой доступный tmpfs, например /tmp
const SOCKET_PATH = '/tmp/nodejs-controller.sock';

const MSG_TYPE = {
  AI_BINARY: 0x01,
  AI_START: 0x02,
  AI_STOP: 0x03,
  AI_STDOUT: 0x04,
  AI_STDERR: 0x05,
  LOG: 0x06
};

// --- Протокол и функции для него ---

function sliceChunk(chunk, needed) {
  return chunk.length > needed ? chunk.slice(0, needed) : chunk;
}

function protocolParse(buffer) {
  if (buffer.length < 5) return null;
  const length = buffer.readUInt32BE(0);
  if (buffer.length < length + 4) return null;
  const type = buffer[4];
  const payload = buffer.slice(5, 4 + length);
  return {length, type, payload, totalSize: length + 4};
}

function protocolBuild(type, payloadBuffer) {
  const lengthBuf = Buffer.alloc(4);
  lengthBuf.writeUInt32BE(payloadBuffer.length + 1, 0);
  return Buffer.concat([lengthBuf, Buffer.from([type]), payloadBuffer]);
}

async function saveBinaryToFile(buffer) {
  const filePath = path.join(TMP_DIR, AI_BIN_NAME);
  return new Promise((resolve, reject) => {
    fs.writeFile(filePath, buffer, {mode: 0o755}, err => {
      if (err) reject(err);
      else resolve(filePath);
    });
  });
}

// --- Сервер (управляющая программа) ---

async function handleConnection(socket) {
  console.log('Client connected');
  let buffer = Buffer.alloc(0);
  let aiProcess = null;

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

  socket.on('data', async data => {
    if (aiProcess) {
      // Во время работы AI просто пересылаем данные в stdin AI
      aiProcess.stdin.write(data);
      return;
    }

    buffer = Buffer.concat([buffer, data]);
    for(;;){
      const msg = protocolParse(buffer);
      if (!msg) break;
      buffer = buffer.slice(msg.totalSize);

      switch (msg.type) {
        case MSG_TYPE.AI_BINARY:
          console.log(`Saving binary of length ${msg.payload.length}`);
          await saveBinaryToFile(msg.payload);
          socket.write(protocolBuild(MSG_TYPE.LOG, Buffer.from('Binary saved')));
          break;

        case MSG_TYPE.AI_START:
          if (aiProcess) {
            socket.write(protocolBuild(MSG_TYPE.LOG, Buffer.from('AI already running')));
            break;
          }
          const aiBin = path.join(TMP_DIR, AI_BIN_NAME);
          if (!fs.existsSync(aiBin)) {
            socket.write(protocolBuild(MSG_TYPE.LOG, Buffer.from('Binary not found')));
            break;
          }
          console.log('Starting AI process');
          aiProcess = spawn(aiBin, [], {stdio: ['pipe', 'pipe', 'pipe']});
          aiProcess.stdout.on('data', d => {
            console.log({out:1,n:d.length,d,s:d+""});
            socket.write(protocolBuild(MSG_TYPE.AI_STDOUT, d));
          });
          aiProcess.stderr.on('data', d => {
            console.log({err:1,n:d.length,d,s:d+""});
            socket.write(protocolBuild(MSG_TYPE.AI_STDERR, d));
          });
          aiProcess.on('close', code => {
            socket.write(protocolBuild(MSG_TYPE.LOG, Buffer.from(`AI exited with code ${code}`)));
            aiProcess = null;
          });
          break;

        case MSG_TYPE.AI_STOP:
          if (aiProcess) {
            aiProcess.kill();
            socket.write(protocolBuild(MSG_TYPE.LOG, Buffer.from('AI killed')));
          } else {
            socket.write(protocolBuild(MSG_TYPE.LOG, Buffer.from('No AI process running')));
          }
          break;

        default:
          socket.write(protocolBuild(MSG_TYPE.LOG, Buffer.from('Unknown message type')));
      }
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
    // TCP сервер для тестов под Windows
    const PORT = 4000;
    const server = net.createServer(handleConnection);
    server.listen(PORT, () => {
      console.log(`Server listening on TCP port ${PORT}`);
    });
  }
}

// --- Клиент (имитация t_node) ---
var c_out=[];var c_err=[];
async function runClient(host, port, binaryFilePath) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    client.connect(port, host, async () => {
      console.log('Connected to server');
      // Читаем бинарь
      const binData = fs.readFileSync(binaryFilePath);

      // Отправляем бинарь
      const binMsg = protocolBuild(MSG_TYPE.AI_BINARY, binData);
      client.write(binMsg);
      console.log('Sent binary');

      // Ждем лог-сообщения от сервера о сохранении
      client.once('data', () => {
        // После этого запускаем ИИ
        const startMsg = protocolBuild(MSG_TYPE.AI_START, Buffer.alloc(0));
        client.write(startMsg);
        console.log('Sent start command');
      });

      client.on('data', data => {
        const msg = protocolParse(data);
        if (!msg) return;
        switch(msg.type){
          case MSG_TYPE.AI_STDOUT:
            console.log('AI stdout:', msg.payload.toString());c_out.push(msg.payload+"");
            break;
          case MSG_TYPE.AI_STDERR:
            console.error('AI stderr:', msg.payload.toString(),msg.payload);c_err.push(msg.payload+"");
            var arr=c_err.join("").split("\r").join("").split("\n").join("").split("ERR");
            var ok=true;var fail_id=0;
            for(var i=1;i<arr.length-10;i++){
              if(arr[i]!=(""+(i-1))){ok=false;fail_id=i;break;}
            }
            if(!ok)console.log("FAIL!!! "+fail_id);
            break;
          case MSG_TYPE.LOG:
            console.log('Log:', msg.payload.toString());
            break;
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

// --- Запуск обоих режимов по аргументу ---

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
  console.log('Usage: node script.js [server_unix|server_tcp|client_tcp <host> <port> <file>]');
}
