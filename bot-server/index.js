const readline = require('readline');
const mineflayer = require('mineflayer');
const { pathfinder, Movements, goals } = require('mineflayer-pathfinder');

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
  bot.loadPlugin(pathfinder);

  bot.on('login', () => {
    send('connected', { username, server });
    send('log', { text: `${username} joined ${server}` });
  });

  bot.once('spawn', () => {
    bot.pathfinder.setMovements(new Movements(bot));
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

function getBot(username) {
  const bot = bots[username];
  if (!bot) send('error', { text: `No bot "${username}"` });
  return bot;
}

function followPlayer(username, player) {
  const bot = getBot(username);
  if (!bot) return;
  const target = bot.players[player] && bot.players[player].entity;
  if (!target) {
    send('error', { text: `${username}: player "${player}" not found or not loaded` });
    return;
  }
  bot.pathfinder.setGoal(new goals.GoalFollow(target, 1), true);
  send('log', { text: `${username} → following ${player}` });
}

function stopBot(username) {
  const bot = getBot(username);
  if (!bot) return;
  bot.pathfinder.setGoal(null);
  send('log', { text: `${username} stopped` });
}

function gotoPos(username, x, y, z) {
  const bot = getBot(username);
  if (!bot) return;
  bot.pathfinder.setGoal(new goals.GoalBlock(x, y, z));
  send('log', { text: `${username} → going to ${x}, ${y}, ${z}` });
}

function goHome(username) {
  const bot = getBot(username);
  if (!bot) return;
  const s = bot.spawnPoint || { x: 0, y: 64, z: 0 };
  bot.pathfinder.setGoal(new goals.GoalBlock(s.x, s.y, s.z));
  send('log', { text: `${username} → going home` });
}

function reportPos(username) {
  const bot = getBot(username);
  if (!bot) return;
  const p = bot.entity.position;
  send('log', { text: `${username} position: ${Math.round(p.x)}, ${Math.round(p.y)}, ${Math.round(p.z)}` });
}

function reportHealth(username) {
  const bot = getBot(username);
  if (!bot) return;
  send('log', { text: `${username} health: ${Math.round(bot.health)}/20, food: ${Math.round(bot.food)}/20` });
}

function reportInventory(username) {
  const bot = getBot(username);
  if (!bot) return;
  const counts = {};
  for (const it of bot.inventory.items()) counts[it.name] = (counts[it.name] || 0) + it.count;
  const text = Object.entries(counts).map(([n, c]) => `${n} x${c}`).join(', ');
  send('log', { text: text ? `${username} inventory: ${text}` : `${username} inventory: empty` });
}

function findItem(bot, itemName) {
  return bot.inventory.items().find(i => i.name.includes(itemName.toLowerCase()));
}

function dropItem(username, itemName, count) {
  const bot = getBot(username);
  if (!bot) return;
  const item = findItem(bot, itemName);
  if (!item) {
    send('error', { text: `${username}: item "${itemName}" not found` });
    return;
  }
  bot.toss(item.type, null, count || item.count);
  send('log', { text: `${username} → dropped ${count || item.count} x ${item.name}` });
}

function equipItem(username, itemName) {
  const bot = getBot(username);
  if (!bot) return;
  const item = findItem(bot, itemName);
  if (!item) {
    send('error', { text: `${username}: item "${itemName}" not found` });
    return;
  }
  bot.equip(item, 'hand');
  send('log', { text: `${username} → equipped ${item.name}` });
}

function whisper(username, player, message) {
  const bot = getBot(username);
  if (!bot) return;
  bot.chat(`/msg ${player} ${message}`);
  send('log', { text: `${username} → whispered ${player}` });
}

function respawnBot(username) {
  const bot = getBot(username);
  if (!bot) return;
  bot.respawn();
  send('log', { text: `${username} respawning` });
}

function listPlayers(username) {
  const bot = getBot(username);
  if (!bot) return;
  const names = Object.values(bot.players).filter(p => p.username && p.username !== username).map(p => p.username);
  send('log', { text: names.length ? `${username} sees: ${names.join(', ')}` : `${username} sees no other players` });
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
      case 'follow':
        followPlayer(msg.username, msg.player);
        break;
      case 'stop':
        stopBot(msg.username);
        break;
      case 'goto':
        gotoPos(msg.username, Number(msg.x), Number(msg.y), Number(msg.z));
        break;
      case 'home':
        goHome(msg.username);
        break;
      case 'pos':
        reportPos(msg.username);
        break;
      case 'health':
        reportHealth(msg.username);
        break;
      case 'inventory':
        reportInventory(msg.username);
        break;
      case 'drop':
        dropItem(msg.username, msg.item, Number(msg.count) || 0);
        break;
      case 'equip':
        equipItem(msg.username, msg.item);
        break;
      case 'whisper':
        whisper(msg.username, msg.player, msg.message);
        break;
      case 'respawn':
        respawnBot(msg.username);
        break;
      case 'players':
        listPlayers(msg.username);
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
