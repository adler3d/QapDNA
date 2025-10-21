const net = require('net');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const AI_BIN_NAME = './ai.bin';
const TMP_DIR = '.'///tmpfs'; // можно на /tmpfs
const SOCKET_PATH = process.env.SOCKET_PATH || '/tmp/dokcon.sock';

var log=function(...args){
  console.log.apply(console, [qap_time(), ...args]);
};

function qap_time() {
  const now = new Date();
  const utc = new Date(now.getTime() + now.getTimezoneOffset() * 60000);
  const moscowTime = new Date(utc.getTime() + 3 * 60 * 60000);
  const year = moscowTime.getFullYear();
  const month = String(moscowTime.getMonth() + 1).padStart(2, '0');
  const day = String(moscowTime.getDate()).padStart(2, '0');
  const hours = String(moscowTime.getHours()).padStart(2, '0');
  const minutes = String(moscowTime.getMinutes()).padStart(2, '0');
  const seconds = String(moscowTime.getSeconds()).padStart(2, '0');
  const milliseconds = String(moscowTime.getMilliseconds()).padStart(3, '0');
  return `${year}.${month}.${day} ${hours}:${minutes}:${seconds}.${milliseconds}`;
}
var func=x=>JSON.stringify(x.toString('binary'));
var emitter_on_data_decoder=(emitter,cb)=>{
  var rd=Buffer.from([]);
  emitter.on('data',data=>{
    //log('Received raw data, length=' + data.length+" value="+func(data.slice(0,128)));
    rd=Buffer.concat([rd,data]);
    for(;;){
      var e=rd.indexOf("\0");
      if(e<0)return log('e<0 e=',e);
      var en=e+1;
      var zpos=rd.indexOf('\0',en);
      if(zpos<0)return log('zpos<0 zpos=',e);
      var zn=zpos+1;
      var blen=rd.slice(0,e);
      var len=parseInt(blen.toString("binary"),10);
      if(rd.length<zn+len)return log('rd.length<zn+len ',{rdlen:rd.length,zn,len});
      var bz=rd.slice(en,en+zpos-en);var z=bz.toString("binary");
      var bmsg=rd.slice(zn,zn+len);var msg=bmsg.toString("binary");
      rd=rd.slice(zn+len);
      //log('rd=' + rd.length+" value="+func(rd.slice(0,128)));
      //log('z=' + z+" msg="+func(msg.slice(0,128)));
      cb(z,msg,bz,bmsg);
    }
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

async function handleConnection(socket) {
  log('Client connected');
  let aiProcess = null;
  socket.setNoDelay(true);
  stream_write_encoder(socket,'hi from dokcon.js')('2025.10.18 12:01:08.493');
  //log('after hi');
  socket.on('close', () => {
    if (aiProcess) {
      log('Killing AI process due to socket close');
      aiProcess.kill();
      aiProcess = null;
    }
  });
  //log('a bit later');
  socket.on('error', err => {
    log('Socket error:', err);
    if (aiProcess) {
      aiProcess.kill();
      aiProcess = null;
    }
  });
  
  //log('a bit later 2');
  emitter_on_data_decoder(socket, async (z, msg, bz, bmsg) => {
    switch (z) {
      case 'ai_stdin':
          if (aiProcess) {
            // Пересылаем в stdin AI
            aiProcess.stdin.write(bmsg); // бинарно!
            //stream_write_encoder(socket, 'log')('ai_stdin.length='+bmsg.length);
            return;
          }else stream_write_encoder(socket, 'log')('i got ai_stdin but aiProcess is not started!');
          break;

      case 'ai_binary':
        log(`Saving binary of length ${bmsg.length}`);
        const filePath = path.join(TMP_DIR, AI_BIN_NAME);
        try {
          log('otpravlenZ');
          fs.writeFileSync(filePath, bmsg, { mode: 0o755 });
          log('otpravlen0');
          //stream_write_encoder(socket, 'log')('Binary saved');
          //log('otpravlen1');
          stream_write_encoder(socket, 'ai_binary_ack')(bmsg.length+"");/*
          log('otpravlen2');
          stream_write_encoder(socket, 'log')('ai_binary_ack otpravlen 1');
          log('otpravlen3');
          stream_write_encoder(socket, 'log')('ai_binary_ack otpravlen 2');
          log('otpravlen4');
          stream_write_encoder(socket, 'log')('ai_binary_ack otpravlen 3');
          log('otpravlen5');
          stream_write_encoder(socket, 'log')('ai_binary_ack otpravlen 4');
          log('otpravlen6');
          stream_write_encoder(socket, 'log')('ai_binary_ack otpravlen 5');
          log('otpravlen7');*/
        } catch (err) {
          log('FAILED TO WRITE BINARY:', err);
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
        log('Starting AI process');
        log(aiBin);
        aiProcess = spawn("./"+aiBin,[],{stdio:['pipe','pipe','pipe'],windowsHide:true});

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
        log(`Unknown channel: ${z}`);
    }
  });
}

function startServer(useUnixSocket) {
  if (useUnixSocket) {
    if (fs.existsSync(SOCKET_PATH)) fs.unlinkSync(SOCKET_PATH);
    const server = net.createServer(handleConnection);
    server.listen(SOCKET_PATH, () => {
      log(`Server listening on Unix socket ${SOCKET_PATH}`);
    });
    fs.chmodSync(SOCKET_PATH, 0o777);
  } else {
    const PORT = 4000;
    const server = net.createServer(handleConnection);
    server.listen(PORT, () => {
      log(`Server listening on TCP port ${PORT}`);
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
          log('AI stderr:', JSON.stringify(msg)); // видим сырые фрагменты
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

async function runClientUnix(socketPath, binaryFilePath) {
  return new Promise((resolve, reject) => {
    const client = net.createConnection(socketPath);

    client.on('connect', async () => {
      console.log('Connected to Unix socket:', socketPath);

      // Читаем бинарник
      const binData = fs.readFileSync(binaryFilePath);

      // Отправляем бинарь
      stream_write_encoder(client, 'ai_binary')(binData);
      console.log('Sent binary');

      let binaryAckReceived = false;

      emitter_on_data_decoder(client, (z, msg, bz, bmsg) => {
        if (z === 'log') {
          console.log('Log:', msg);
        }

        if (z === 'ai_binary_ack' && !binaryAckReceived) {
          binaryAckReceived = true;
          console.log('Received ai_binary_ack, starting AI...');
          stream_write_encoder(client, 'ai_start')('hm...');
        }

        if (z === 'ai_stdout') {
          console.log('AI stdout:', msg);
        }

        if (z === 'ai_stderr') {
          log('AI stderr:', msg);
        }

        // Опционально: завершить, если AI вышел
        if (z === 'log' && msg.includes('exited')) {
          client.end();
        }
        log("msg from z='"+z+"'");
      });

      client.on('close', () => {
        console.log('Connection closed');
        resolve();
      });

      client.on('error', (err) => {
        log('Socket error:', err.message);
        reject(err);
      });
    });

    client.on('error', (err) => {
      log('Failed to connect to Unix socket:', err.message);
      reject(err);
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
  runClient(host, port, file).catch(log);
} else if (process.argv[2] === 'client_unix' && process.argv[3] && process.argv[4]) {
  const socketPath = process.argv[3];
  const binaryFilePath = process.argv[4];
  runClientUnix(socketPath, binaryFilePath).catch(log);
} else {
  console.log('Usage: node dokcon.js [server_unix|server_tcp|client_tcp <host> <port> <file>]');
  //node dokcon.js client_tcp 127.0.0.1 4000 ai.exe
}