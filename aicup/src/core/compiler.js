// ai-compiler.js
const fs = require('fs').promises;
const path = require('path');
const { exec } = require('child_process');
const util = require('util');
const execPromise = util.promisify(exec);

// Настройки
const TEMP_DIR = '/tmp/ai_compile';
const OUTPUT_BASE_DIR = '/compiled'; // должен существовать
const ALLOWED_FUNCTIONS = ['printf', 'malloc', 'free', 'sqrt']; // разрешённые C-функции
const FORBIDDEN_HEADERS = ['<unistd.h>', '<sys/socket.h>', '<pthread.h>'];

// Очистка временной директории при старте
async function init() {
  try {
    await fs.rm(TEMP_DIR, { recursive: true, force: true });
    await fs.mkdir(TEMP_DIR, { recursive: true });
    await fs.mkdir(OUTPUT_BASE_DIR, { recursive: true });
  } catch (e) {
    console.error('Init temp dir failed:', e.message);
  }
}

// Проверка на запрещённые элементы
function checkSecurity(source) {
  return [];/*const issues = [];
  
  // Запрещённые функции
  const forbiddenFuncs = ['system', 'exec', 'fork', 'socket', 'connect', 'open', 'fopen'];
  for (const func of forbiddenFuncs) {
    const regex = new RegExp(`\\b${func}\\s*\\(`);
    if (regex.test(source)) {
      issues.push(`forbidden_function: ${func}`);
    }
  }

  // Запрещённые заголовки
  for (const header of FORBIDDEN_HEADERS) {
    if (source.includes(header)) {
      issues.push(`forbidden_header: ${header}`);
    }
  }

  return issues;
  */
}

//const fetch = require('node-fetch');

async function uploadToCdn(path, dataBuffer, token) {
  const url = `http://t_cdn:12346/${path}`;
  const res = await fetch(url, {
    method: 'PUT',
    headers: {
      'Authorization': `Bearer ${token}`,
      'Content-Type': 'application/octet-stream',
      'Content-Length': dataBuffer.length
    },
    body: dataBuffer
  });
  if (!res.ok) {
    const text = await res.text();
    throw new Error(`CDN upload failed: ${res.status} ${text}`);
  }
  return true;
}


// Компиляция с песочницей
async function compileSource(data) {
  const { coder_id, elf_version, source_code, timeout_ms, memory_limit_mb } = data;

  const tempDir = path.join(TEMP_DIR, `${coder_id}_${elf_version}_${Date.now()}`);
  const sourcePath = path.join(tempDir, 'ai.cpp');
  const binaryPath = path.join(tempDir, 'ai.bin');//output_path; // прямой путь от сервера

  try {
    // 1. Создать временную папку
    await fs.mkdir(tempDir, { recursive: true });

    // 2. Проверить безопасность
    const securityIssues = checkSecurity(source_code);
    if (securityIssues.length > 0) {
      return {
        success: false,
        log: `Security violation: ${securityIssues.join(', ')}`,
        error_type: 'security',
        error_detail: securityIssues[0]
      };
    }

    // 3. Сохранить исходник
    await fs.writeFile(sourcePath, source_code);

    // 4. Выполнить компиляцию с ограничениями
    const command = `g++ -O2 -static -s -o "${binaryPath}" "${sourcePath}"`;

    // Обёртка с ulimit (ограничение памяти и времени)
    const memLimitKB = memory_limit_mb * 1024;
    const limitedCommand = `
      ulimit -t $(( ${timeout_ms} / 1000 + 1 )) &&
      ulimit -v ${memLimitKB} &&
      ${command}
    `;

    try {
      await execPromise(limitedCommand, { timeout: timeout_ms + 2000 });
      
      // Проверить, что файл создан и не пустой
      const stat = await fs.stat(binaryPath);
      if (stat.size === 0) {
        throw new Error('Binary is empty');
      }
      var compilationSuccess=true;
      if (compilationSuccess) {
        const binaryBuffer = await fs.readFile(binaryPath);
        await uploadToCdn(`binary/${coder_id}_${elf_version}.bin`, binaryBuffer, process.env.UPLOAD_TOKEN);
      }
      return {
        success: true,
        binary_path: binaryPath,
        log: 'Compilation successful.',
        error_type: null
      };

    } catch (err) {
      let msg = err.stderr || err.stdout || err.message;

      if (err.signal === 'SIGKILL' || msg.includes('Memory limit') || msg.includes('Virtual memory')) {
        return {
          success: false,
          log: 'Compilation failed: memory limit exceeded.',
          error_type: 'resource',
          error_detail: 'memory'
        };
      }
      if (msg.includes('timeout') || err.signal === 'SIGTERM') {
        return {
          success: false,
          log: 'Compilation failed: time limit exceeded.',
          error_type: 'resource',
          error_detail: 'timeout'
        };
      }

      return {
        success: false,
        log: `Compilation error: ${msg.slice(0,16384)}`,
        error_type: 'compile',
        error_detail: 'compiler_error'
      };
    }

  } catch (err) {
    return {
      success: false,
      log: `Internal error: ${err.message}`,
      error_type: 'internal',
      error_detail: 'fs_or_exec'
    };
  } finally {
    // Удалить временные файлы
    try {
      await fs.rm(tempDir, { recursive: true, force: true });
    } catch (e) {
      console.warn(`Failed to clean up ${tempDir}:`, e.message);
    }
  }
}

// HTTP API (пример на Express)
const express = require('express');
const app = express();
app.use(express.json({ limit: '10mb' }));

app.post('/compile', async (req, res) => {
  const job = req.body;

  // Валидация
  if (!job.coder_id || !job.elf_version || !job.source_code ) {
    return res.status(400).json({
      error: 'Missing required fields: coder_id, elf_version, source_code\nkeys:'+/*JSON.stringify*/(Object.keys(job))
    });
  }

  const result = await compileSource(job);
  res.json(result);
});

// Запуск
init().then(() => {
  const PORT = process.env.PORT || 3000;
  app.listen(PORT, () => {
    console.log(`AI Compiler Service running on port ${PORT}`);
  });
});

/*
input:
{
  "coder_id": "coder_123",
  "elf_version": "v1.0",
  "source_code": "string с кодом на C++",
  "timeout_ms": 10000,
  "memory_limit_mb": 512,
  "output_path": "/compiled/coder_123_v1.0.elf"
}

output:
{
  "coder_id": "coder_123",
  "elf_version": "v1.0",
  "success": true,
  "binary_path": "/compiled/coder_123_v1.0.elf",
  "log": "Compilation successful.",
  "error_type": null
}
// или
{
  "success": false,
  "log": "error: 'forbidden function: system()'",
  "error_type": "security",
  "error_detail": "forbidden_function"
}

*/