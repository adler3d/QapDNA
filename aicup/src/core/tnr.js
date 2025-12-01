#!/usr/bin/env node

const net = require('net');
const { spawn } = require('child_process');
const fs = require('fs');

// Конфигурация через переменные окружения
const CONFIG = {
  TNR_MAIN_PORT: parseInt(process.env.TNR_MAIN_PORT || '11224'),
  TNR_MAIN_HOST: process.env.TNR_MAIN_HOST || 'localhost',
  UPLOAD_TOKEN: process.env.UPLOAD_TOKEN || '',
  TNR_PING_MONITOR_INTERVAL: parseInt(process.env.TNR_PING_MONITOR_INTERVAL || '5000'),
  TNR_INACTIVE_THRESHOLD: parseInt(process.env.TNR_INACTIVE_THRESHOLD || '30000'),
  TNR_TIMEOUT_THRESHOLD: parseInt(process.env.TNR_TIMEOUT_THRESHOLD || '60000'),
  TNR_RESTART_DELAY: parseInt(process.env.TNR_RESTART_DELAY || '1000'),
  TNR_MAX_RESTARTS: parseInt(process.env.TNR_MAX_RESTARTS || '30000'),
  TNR_LOG_LEVEL: process.env.TNR_LOG_LEVEL || 'DEBUG',
  TNR_CORE_PATH: process.env.TNR_CORE_PATH || './core.elf'
};

// Глобальное состояние
let state = {
  tnrToken: null,
  tnodeProcess: null,
  tmainSocket: null,
  restartsCount: 0,
  lastRestartTime: 0,
  isConnected: false,
  lastStatusInfo: null,
  lastNodeState: 'unknown',
  socket:null
};

// Вспомогательные функции логирования
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

function log(level, ...args) {
  const levels = { DEBUG: 0, INFO: 1, WARN: 2, ERROR: 3 };
  if (levels[level] >= levels[CONFIG.TNR_LOG_LEVEL]) {
    console.log(`[${qap_time()}] [${level}]`, ...args);
  }
}

function info(...args) { log('INFO', ...args); }
function warn(...args) { log('WARN', ...args); }
function error(...args) { log('ERROR', ...args); }
function debug(...args) { log('DEBUG', ...args); }

// ZChan протокол (на основе dokcon.js)
function emitter_on_data_decoder(emitter, cb) {
  let rd = Buffer.from([]);
  emitter.on('data', data => {
    debug('Received raw data, length=', data.length);
    rd = Buffer.concat([rd, data]);
    for (;;) {
      const e = rd.indexOf("\0");
      if (e < 0) return debug('e<0 e=', e);
      const en = e + 1;
      const zpos = rd.indexOf('\0', en);
      if (zpos < 0) return debug('zpos<0 zpos=', e);
      const zn = zpos + 1;
      const blen = rd.slice(0, e);
      const len = parseInt(blen.toString("binary"), 10);
      if (rd.length < zn + len) return debug('rd.length<zn+len ', { rdlen: rd.length, zn, len });
      const bz = rd.slice(en, en + zpos - en);
      const z = bz.toString("binary");
      const bmsg = rd.slice(zn, zn + len);
      const msg = bmsg.toString("binary");
      rd = rd.slice(zn + len);
      debug('z=', z, 'msg=', msg);
      cb(z, msg, bz, bmsg);
    }
  });
}

function stream_write_encoder(stream, z) {
  return (data) => {
    const sep = Buffer.from([0]);
    stream.write(Buffer.concat([
      Buffer.from(!data ? "0" : (data.length + ""), "binary"), sep,
      Buffer.from(z, "binary"), sep,
      Buffer.from(data ? data : "", "binary")
    ]));
  };
}

// Подключение к t_main
function connectToMain() {
  return new Promise((resolve, reject) => {
    info('Connecting to t_main at', CONFIG.TNR_MAIN_HOST + ':' + CONFIG.TNR_MAIN_PORT);
    
    const socket = new net.Socket();
    state.tmainSocket = socket;
    
    socket.setNoDelay(true);
    //socket.deaded=false;
    
    socket.on('connect', () => {
      info('Connected to t_main');
      state.isConnected = true;
      //state.socket=socket;
      
      // Настройка декодера для входящих сообщений
      emitter_on_data_decoder(socket, handleMainMessage);
      
      // Регистрация TNR
      registerTNR();
      resolve();
    });
    
    socket.on('error', (err) => {
      error('Socket error:', err.message);
      state.isConnected = false;
      reject(err);
    });
    
    socket.on('close', () => {
      info('Disconnected from t_main');
      state.isConnected = false;
      // Попытка переподключения
      //if(('deaded' in socket)&&!socket.deaded)
      setTimeout(() => {
        info('Attempting to reconnect to t_main...');
        connectToMain().catch(err => {
          error('Reconnection failed:', err.message);
        });
      }, 2500);
    });
    
    socket.connect(CONFIG.TNR_MAIN_PORT, CONFIG.TNR_MAIN_HOST);
  });
}

// Регистрация TNR в t_main
function registerTNR() {
  if (!CONFIG.UPLOAD_TOKEN) {
    error('UPLOAD_TOKEN not configured');
    return;
  }
  
  let registerData = JSON.stringify({
    upload_token: CONFIG.UPLOAD_TOKEN,
    tnr_version: '1.0',
    timestamp: qap_time(),
    token:state.tnrToken?state.tnrToken:""
  });
  
  const send_register = stream_write_encoder(state.tmainSocket, 'register_tnr');
  send_register(registerData);
  info('Sent TNR registration request');
}

// Обработка сообщений от t_main
function handleMainMessage(z, msg, bz, bmsg) {
  debug('Received message:', z, msg);
  
  switch (z) {
    case 'tnr_token':
      handleTNRAuth(msg);
      break;
    case 'tnr_reg_ack':
      info('Received tnr_reg_ack: '+msg);
      break;
    case 't_node_active':
      handleNodeStatus('active', msg);
      break;
    case 't_node_inactive':
      handleNodeStatus('inactive', msg);
      break;
    case 't_node_timeout':
      handleNodeStatus('timeout', msg);
      break;
    case 't_node_recovered':
      handleNodeStatus('recovered', msg);
      break;
    case 'game_info':
      handleGameInfo(msg);
      break;
    default:
      debug('Unknown message type:', z);
  }
}

// Обработка авторизации TNR
function handleTNRAuth(msg) {
  try {
    const authData = JSON.parse(msg);
    state.tnrToken = authData.token;
    info('Received TNR token:', state.tnrToken);
    
    // Запуск t_node после получения токена
    startTNode();
  } catch (e) {
    error('Failed to parse TNR auth response:', e.message);
  }
}

// Обработка статуса t_node
function handleNodeStatus(status, msg) {
  info('Node status changed to:', status);
  
  try {
    const statusData = JSON.parse(msg);
    state.lastStatusInfo = statusData;
    
    if (status === 'timeout' && state.tnrToken) {
      // Критическая ситуация - перезапускаем t_node
      handleNodeTimeout(statusData);
    } else if (status === 'recovered') {
      // t_node восстановился - сбрасываем счетчик перезапусков
      state.restartsCount = 0;
      info('t_node recovered, restart counter reset');
    }
  } catch (e) {
    error('Failed to parse node status:', e.message);
  }
}

// Обработка информации об активных играх
function handleGameInfo(msg) {
  try {
    const gameData = JSON.parse(msg);
    state.lastStatusInfo = gameData;
    
    debug('Active games:', gameData.active_games?.length || 0);
  } catch (e) {
    error('Failed to parse game info:', e.message);
  }
}

// Обработка таймаута t_node
function handleNodeTimeout(statusData) {
  //const now = Date.now();
  
  // Проверяем лимит перезапусков
  if (state.restartsCount >= CONFIG.TNR_MAX_RESTARTS) {
    error('Maximum restart attempts reached, giving up');
    return;
  }
  /*
  // Проверяем интервал между перезапусками
  if (now - state.lastRestartTime < CONFIG.TNR_RESTART_DELAY) {
    warn('Restart delay not satisfied, skipping restart');
    return;
  }
  */
  info('Initiating t_node restart due to timeout');
  restartTNode(statusData);
}

// Запуск t_node
function startTNode() {
  if (!state.tnrToken) {
    error('Cannot start t_node without TNR token');
    return;
  }
  
  info('Starting t_node with token:', state.tnrToken);
  
  // Завершаем старый процесс если есть
  if (state.tnodeProcess) {
    info('Terminating existing t_node process');
    state.tnodeProcess.kill();
  }
  
  // Запускаем новый процесс
  const args = ['t_node', state.tnrToken];
  info('Executing:', CONFIG.TNR_CORE_PATH, args.join(' '));
  
  state.tnodeProcess = spawn(CONFIG.TNR_CORE_PATH, args, {
    stdio: ['pipe', 'pipe', 'pipe']
  });
  
  // Обработка вывода t_node
  state.tnodeProcess.stdout.on('data', (data) => {
    debug('t_node stdout:', data.toString().trim());
  });
  
  state.tnodeProcess.stderr.on('data', (data) => {
    warn('t_node stderr:', data.toString().trim());
  });
  
  state.tnodeProcess.on('close', (code) => {
    info('t_node process exited with code:', code);
    if (code !== 0) {
      warn('t_node exited with non-zero code, may need restart');
    }
    state.tnodeProcess=null;
    setTimeout(() => {
      restartTNode("restart after close");
    }, 5000);
  });
  
  state.tnodeProcess.on('error', (err) => {
    error('Failed to start t_node:', err.message);
  });
  
  info('t_node started successfully');
}

// Перезапуск t_node
function restartTNode(reason) {
  const now = Date.now();
  
  info(`Restarting t_node (attempt ${state.restartsCount + 1}/${CONFIG.TNR_MAX_RESTARTS})`);
  info('Restart reason:', reason);
  
  // Уведомляем t_main о перезапуске
  if (state.tmainSocket && state.isConnected) {
    const restartInfo = JSON.stringify({
      reason: reason,
      restart_count: state.restartsCount + 1,
      timestamp: qap_time()
    });
    
    const send_restart = stream_write_encoder(state.tmainSocket, 'restarting');
    send_restart(restartInfo);
  }
  
  // Запускаем t_node заново
  startTNode();
  
  // Обновляем счетчики
  state.restartsCount++;
  state.lastRestartTime = now;
}

// Graceful shutdown
function gracefulShutdown() {
  info('TNR shutting down gracefully...');
  
  // Уведомляем t_main о завершении
  if (state.tmainSocket && state.isConnected) {
    const shutdownInfo = JSON.stringify({
      restart_count: state.restartsCount,
      last_restart: qap_time(),
      timestamp: qap_time()
    });
    
    const send_shutdown = stream_write_encoder(state.tmainSocket, 'tnr_shutdown');
    send_shutdown(shutdownInfo);
  }
  
  // Завершаем t_node процесс
  if (state.tnodeProcess) {
    info('Terminating t_node process...');
    state.tnodeProcess.kill();
  }
  
  // Закрываем соединения
  if (state.tmainSocket) {
    state.tmainSocket.destroy();
  }
  
  process.exit(0);
}

// Регистрация обработчиков сигналов
process.on('SIGINT', gracefulShutdown);
process.on('SIGTERM', gracefulShutdown);

process.on('uncaughtException', (err) => {
  error('Uncaught exception:', err.message);
  gracefulShutdown();
});

process.on('unhandledRejection', (reason, promise) => {
  error('Unhandled rejection:', reason);
  gracefulShutdown();
});

// Основная функция
async function main() {
  try {
    info('TNR (t_node restart) system starting...');
    info('Configuration:', JSON.stringify(CONFIG, null, 2));
    
    if (!CONFIG.UPLOAD_TOKEN) {
      error('UPLOAD_TOKEN environment variable is required');
      process.exit(1);
    }
    
    await connectToMain();
    
    // Основной цикл мониторинга
    setInterval(() => {
      if (state.isConnected) {
        // Отправляем пинг в t_main
        const send_ping = stream_write_encoder(state.tmainSocket, 'ping:tnr');
        send_ping(qap_time());
      }
    }, CONFIG.TNR_PING_MONITOR_INTERVAL);
    
    info('TNR system is running and monitoring t_node...');
    
  } catch (err) {
    error('Failed to start TNR system:', err.message);
    process.exit(1);
  }
}

// Запуск
main();