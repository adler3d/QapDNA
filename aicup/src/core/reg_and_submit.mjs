import fetch from 'node-fetch';
//let node_fetch=require('node-fetch');
//let fetch=node_fetch.fetch;
//const { default: fetch } = require('node-fetch')
// Настройки
const BASE_URL = 'http://127.0.0.1';'http://your-server.com'; // Замените на реальный URL
const REGISTRATION_ENDPOINT = `${BASE_URL}/coder/new`;
const SUBMIT_ENDPOINT = `${BASE_URL}/api/seasons/current/submit`;

/**
 * Регистрирует нового кодера.
 * @param {string} name
 * @param {string} email
 * @returns {Promise<Object>} - {id, token, time}
 */
async function registerCoder(name, email) {
    const payload = { name, email };
    const response = await fetch(REGISTRATION_ENDPOINT, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    });

    if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`Registration failed: ${response.status}, ${errorText}`);
    }
    return await response.json();
}

/**
 * Отправляет стратегию кодера.
 * @param {string} token - Токен кодера
 * @param {string} sourceCode - Исходный код стратегии
 * @returns {Promise<string>} - Ответ сервера
 */
async function submitStrategy(token, sourceCode) {
    const payload = { src: sourceCode };
    const response = await fetch(SUBMIT_ENDPOINT, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${token}` // Если сервер ожидает токен в заголовке
        },
        body: JSON.stringify(payload)
    });

    // Альтернатива: если токен передаётся в теле запроса
    // payload.token = token;

    if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`Submission failed: ${response.status}, ${errorText}`);
    }
    return await response.text();
}

/**
 * Регистрирует кодера и загружает его стратегию.
 * @param {string} name
 * @param {string} email
 * @param {string} sourceCode
 * @returns {Promise<void>}
 */
async function registerAndSubmit(name, email, sourceCode) {
    console.log(`Registering coder: ${name}...`);
    const registrationResult = await registerCoder(name, email);

    console.log(`Submitting strategy for coder ID ${registrationResult.id}...`);
    const submissionResult = await submitStrategy(registrationResult.token, sourceCode);

    console.log('Submission successful:', submissionResult);
}
/*
// Пример использования
const coderName = 'Alice';
const coderEmail = 'alice@example.com';
const myStrategySource = `// Example bot code
func main() {
    print("Hello, world!");
}
`;

registerAndSubmit(coderName, coderEmail, myStrategySource)
    .catch(err => {
        console.error('Process failed:', err);
    });
*/
// Основной цикл
let iteration = 1;
async function runLoop() {
    while (true) {
        const name = `Alice${iteration}`;
        const email = `alice+${iteration}@subaka.example.com`;
        const sourceCode = `//${iteration}`;

        try {
            await registerAndSubmit(name, email, sourceCode);
        } catch (error) {
            console.error(`[${iteration}] Error:`, error.message);
        }

        iteration++;
        await new Promise(resolve => setTimeout(resolve, 3000)); // Ждем 3 секунды
    }
}

runLoop();