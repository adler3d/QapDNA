const net = require('net');
const fs = require('fs');
const path = require('path');
const os = require('os');
const { spawn } = require('child_process');

const SOCKET_PATH = '/tmp/nodejs-controller.sock';
const TMP_DIR = '/tmpfs'; // tmpfs монтируемой в docker
const AI_BIN_NAME = 'ai.bin';

const MSG_TYPE = {
  AI_BINARY: 0x01,
  AI_START: 0x02,
  AI_STOP: 0x03,
  AI_STDOUT: 0x04,
  AI_STDERR: 0x05,
  LOG: 0x06
};

function sliceChunk(chunk, needed) {
  return chunk.length > needed ? chunk.slice(0, needed) : chunk;
}

function protocolParse(buffer) {
  if (buffer.length < 5) return null;
  const length = buffer.readUInt32BE(0);
  if (buffer.length < length + 4) return null; // +4 байта на поле длины
  const type = buffer[4];
  const payload = buffer.slice(5, 4 + length); // payload с 5 до 4+length
  return {length, type, payload, totalSize: length + 4};
}

function protocolBuild(type, payloadBuffer) {
  const lengthBuf = Buffer.alloc(4);
  lengthBuf.writeUInt32BE(payloadBuffer.length + 1, 0); // +1 на type
  return Buffer.concat([lengthBuf, Buffer.from([type]), payloadBuffer]);
}

async function saveBinary(socket, length) {
  return new Promise((resolve, reject) => {
    const filePath = path.join(TMP_DIR, AI_BIN_NAME);
    const writeStream = fs.createWriteStream(filePath, {flags: 'w', mode: 0o755});
    let received = 0;

    function onData(chunk) {
      const toWrite = sliceChunk(chunk, length - received);
      writeStream.write(toWrite);
      received += toWrite.length;

      if (received >= length) {
        writeStream.end();
        socket.pause();
        socket.removeListener('data', onData);
        resolve(filePath);
      }
    }

    socket.on('data', onData);
    socket.once('error', reject);
  });
}

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
      aiProcess.stdin.write(data);
      return;
    }
    buffer = Buffer.concat([buffer, data]);
    for(;;) {
      const msg = protocolParse(buffer);
      if (!msg) break;
      buffer = buffer.slice(msg.totalSize);

      switch (msg.type) {
        case MSG_TYPE.AI_BINARY:
          // payload — бинарь ИИ
          console.log(`Saving binary of length ${msg.payload.length}`);
          socket.pause();
          const binaryPath = await saveBinaryToFile(msg.payload);
          socket.write(protocolBuild(MSG_TYPE.LOG, Buffer.from('Binary saved')));
          socket.resume();
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
            socket.write(protocolBuild(MSG_TYPE.AI_STDOUT, d));
          });

          aiProcess.stderr.on('data', d => {
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

async function saveBinaryToFile(buffer) {
  const filePath = path.join(TMP_DIR, AI_BIN_NAME);
  return new Promise((resolve, reject) => {
    fs.writeFile(filePath, buffer, {mode: 0o755}, err => {
      if (err) reject(err);
      else resolve(filePath);
    });
  });
}

module.exports = { MSG_TYPE, protocolBuild, protocolParse, handleConnection, saveBinaryToFile };

// Запуск сервера
if (require.main === module) {
  if (fs.existsSync(SOCKET_PATH)) fs.unlinkSync(SOCKET_PATH);

  const server = net.createServer(handleConnection);
  server.listen(SOCKET_PATH, () => {
    console.log(`Listening on Unix socket ${SOCKET_PATH}`);
  });
}