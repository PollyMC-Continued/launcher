const readline = require('readline');
const mineflayer = require('mineflayer');

const bots = {};

function send(event, data) {
  process.stdout.write(JSON.stringify({ event, ...data }) + '\n');
}

function createBot(username, server, port = 25565, version) {
  if (bots[username]) {
    send('error', { text: `Bot "${username}" already exists` });
    return;
  }
  send('log', { text: `Connecting ${username} to ${server}:${port}...` });
  const options = { host: server, port, username };
  if (version) options.version = version;
  const bot = mineflayer.createBot(options);
  bots[username] = bot;

  bot.on('login', () => {
    send('connected', { username, server });
    send('log', { text: `${username} joined ${server}` });
  });

  bot.on('chat', (who, message) => {
    send('chat', { username, from: who, message });
  });

  bot.on('message', (jsonMsg) => {
    send('message', { username, text: jsonMsg.toString() });
  });

  bot.on('end', (reason) => {
    send('log', { text: `${username} disconnected: ${reason}` });
    delete bots[username];
  });

  bot.on('error', (err) => {
    send('error', { text: `${username}: ${err.message}` });
  });

  bot.on('kicked', (reason) => {
    send('log', { text: `${username} was kicked: ${reason}` });
    delete bots[username];
  });
}

function runCommand(username, cmd) {
  const bot = bots[username];
  if (!bot) {
    send('error', { text: `No bot "${username}"` });
    return;
  }
  bot.chat(cmd);
  send('log', { text: `${username} → /${cmd}` });
}

function disconnectBot(username) {
  const bot = bots[username];
  if (!bot) {
    send('error', { text: `No bot "${username}"` });
    return;
  }
  bot.end();
  delete bots[username];
  send('log', { text: `${username} disconnected` });
}

function disconnectAllBots() {
  const names = Object.keys(bots);
  for (const name of names) {
    bots[name].end();
    delete bots[name];
    send('log', { text: `${name} disconnected` });
  }
}

function listBots() {
  const names = Object.keys(bots);
  send('log', { text: names.length ? `Bots: ${names.join(', ')}` : 'No bots connected' });
}

const rl = readline.createInterface({ input: process.stdin, terminal: false });
rl.on('line', (line) => {
  try {
    const msg = JSON.parse(line);
    switch (msg.cmd) {
      case 'join':
        createBot(msg.username, msg.server, msg.port || 25565, msg.version);
        break;
      case 'say':
        runCommand(msg.username, msg.message);
        break;
      case 'quit':
        disconnectBot(msg.username);
        break;
      case 'quit_all':
        disconnectAllBots();
        break;
      case 'list':
        listBots();
        break;
      default:
        send('error', { text: `Unknown command: ${msg.cmd}` });
    }
  } catch (e) {
    send('error', { text: `Invalid JSON: ${e.message}` });
  }
});

send('ready', {});
