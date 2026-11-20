const readline = require('readline');
const mineflayer = require('mineflayer');
const { pathfinder, Movements, goals } = require('mineflayer-pathfinder');
const { createRunner } = require('./script-runner');

const bots = {};
const runners = {};
// per-bot behavior options (autoReconnect, autoRespawn, antiAfk, startDelay)
const botCfg = {};
// startDelay timers: 'quit' during the delay window has to cancel them
const pendingJoins = {};
// autoReconnect backoff timers and attempt counters
const reconnectTimers = {};
const reconnectAttempts = {};
// last script started per bot; restarted after an auto-reconnect
const lastScripts = {};
// deliberate disconnects are the only ones that suppress auto-reconnect
const deliberateQuit = {};
// anti-afk look-around intervals
const antiAfkTimers = {};
// 'kicked' is followed by 'end' for the same disconnect: handle it once
const disconnectHandled = {};

function send(event, data) {
  process.stdout.write(JSON.stringify({ event, ...data }) + '\n');
}

// clamp an integer option to [0, max]; garbage and undefined become 0
function clampSeconds(v, max) {
  const n = Math.floor(Number(v) || 0);
  return Math.max(0, Math.min(max, n));
}

function createBot(username, server, port = 25565, version, opts = {}) {
  if (bots[username] || pendingJoins[username] || reconnectTimers[username]) {
    send('error', { text: `Bot "${username}" already exists` });
    return;
  }
  const cfg = {
    autoReconnect: !!(opts && opts.autoReconnect),
    autoRespawn: !!(opts && opts.autoRespawn),
    antiAfkSeconds: clampSeconds(opts && opts.antiAfkSeconds, 3600),
    startDelaySeconds: clampSeconds(opts && opts.startDelaySeconds, 86400),
  };
  if (cfg.startDelaySeconds > 0) {
    send('log', { text: `${username} connecting to ${server}:${port} in ${cfg.startDelaySeconds}s...` });
    pendingJoins[username] = setTimeout(() => {
      delete pendingJoins[username];
      spawnBot(username, server, port, version, cfg);
    }, cfg.startDelaySeconds * 1000);
    return;
  }
  spawnBot(username, server, port, version, cfg);
}

function spawnBot(username, server, port, version, cfg) {
  send('log', { text: `Connecting ${username} to ${server}:${port}...` });
  const options = { host: server, port, username };
  if (version) options.version = version;
  const bot = mineflayer.createBot(options);
  bots[username] = bot;
  botCfg[username] = cfg;
  // keep connection info on the config so auto-reconnect can re-spawn identically
  cfg.server = server;
  cfg.port = port;
  cfg.version = version;
  deliberateQuit[username] = false;
  disconnectHandled[username] = false;
  bot.loadPlugin(pathfinder);

  bot.on('login', () => {
    delete reconnectAttempts[username];
    send('connected', { username, server });
    send('log', { text: `${username} joined ${server}` });
    // movements need only bot.version, known at login; spawn may come later
    bot.pathfinder.setMovements(new Movements(bot));
    startAntiAfk(username, cfg.antiAfkSeconds);
    // A script cannot survive a lost socket: the mineflayer bot object is a
    // single-connection thing, so a script that was running when the
    // connection dropped restarts cleanly from its first step once we are
    // back in (the runner itself reported 'disconnected' at that time).
    const script = lastScripts[username];
    if (script) {
      send('log', { text: `${username}: restarting script "${script.name || ''}" after reconnect` });
      runScript(username, script);
    }
  });

  bot.on('chat', (who, message) => {
    send('chat', { username, from: who, message });
  });

  bot.on('message', (jsonMsg) => {
    send('message', { username, text: jsonMsg.toString() });
  });

  bot.on('end', (reason) => {
    send('log', { text: `${username} disconnected: ${reason}` });
    botGone(username);
  });

  bot.on('error', (err) => {
    send('error', { text: `${username}: ${err.message}` });
  });

  bot.on('kicked', (reason) => {
    send('log', { text: `${username} was kicked: ${reason}` });
    botGone(username);
  });

  if (cfg.autoRespawn) {
    bot.on('death', () => {
      try {
        if (typeof bot.respawn === 'function') {
          bot.respawn();
        } else {
          // mineflayer builds without respawn(): vanilla "perform respawn" packet
          bot._client.write('client_command', { actionId: 0 });
        }
      } catch (e) {
        send('error', { text: `${username}: auto-respawn failed (${e.message})` });
        return;
      }
      send('log', { text: `${username} died; auto-respawning` });
    });
  }

  // Live position feed for the launcher map: at most one event per second.
  // entity/yaw/health guards cover older server versions where any of them
  // can be missing until spawn (or entirely).
  let lastPositionAt = 0;
  bot.on('move', () => {
    const now = Date.now();
    if (now - lastPositionAt < 1000) return;
    const ent = bot.entity;
    if (!ent || !ent.position) return;
    lastPositionAt = now;
    send('position', {
      username,
      x: ent.position.x,
      y: ent.position.y,
      z: ent.position.z,
      yaw: typeof ent.yaw === 'number' ? ent.yaw : null,
      health: typeof bot.health === 'number' ? bot.health : null,
    });
  });
}

// Shared 'end'/'kicked' cleanup. Schedules an auto-reconnect unless the
// disconnect was deliberate (quit/quit_all) or the bot opted out.
function botGone(username) {
  if (disconnectHandled[username]) return;
  disconnectHandled[username] = true;
  const cfg = botCfg[username] || {};
  delete bots[username];
  delete botCfg[username];
  stopAntiAfk(username);
  // runner sees 'end'/'kicked' too and finishes with 'disconnected' on its own
  if (cfg.autoReconnect && !deliberateQuit[username]) {
    scheduleReconnect(username, cfg);
  } else {
    delete lastScripts[username];
    delete deliberateQuit[username];
  }
}

// Backoff after an unexpected disconnect: 5s, 10s, 20s, 40s, then capped at
// 60s. Retries are unlimited; the attempt counter resets on a successful login.
function scheduleReconnect(username, cfg) {
  const state = reconnectAttempts[username] || (reconnectAttempts[username] = { count: 0 });
  const delays = [5, 10, 20, 40];
  const delay = state.count < delays.length ? delays[state.count] : 60;
  state.count++;
  send('log', { text: `${username}: reconnecting in ${delay}s (attempt ${state.count})` });
  reconnectTimers[username] = setTimeout(() => {
    delete reconnectTimers[username];
    spawnBot(username, cfg.server, cfg.port, cfg.version, cfg);
  }, delay * 1000);
}

// Small random look-around on a fixed interval. Never moves the bot, so the
// pathfinder state stays untouched.
function startAntiAfk(username, seconds) {
  if (!(seconds > 0)) return;
  antiAfkTimers[username] = setInterval(() => {
    const bot = bots[username];
    if (!bot || !bot.entity) return;  // not spawned yet or already gone
    try {
      const yaw = Math.random() * Math.PI * 2;
      const pitch = (Math.random() * 2 - 1) * (Math.PI / 4);
      bot.look(yaw, pitch);
      if (Math.random() < 0.25) bot.swingArm();
    } catch {
      // look/swing can throw before spawn completes; skip this tick
    }
  }, seconds * 1000);
}

function stopAntiAfk(username) {
  if (antiAfkTimers[username]) clearInterval(antiAfkTimers[username]);
  delete antiAfkTimers[username];
}

function validScript(script) {
  if (!script || typeof script !== 'object' || !Array.isArray(script.steps) || script.steps.length === 0) return false;
  const known = ['say', 'command', 'wait', 'wait_for_chat', 'wait_for_player', 'log', 'loop', 'set', 'inc', 'if'];
  const conditions = ['chat_contains', 'chat_equals', 'var_equals', 'var_not_equals', 'var_gt', 'var_gte', 'player_joined', 'health_below'];
  const vars = new Set();
  const varOk = (name) => {
    if (!/^[A-Za-z0-9_]{1,32}$/.test(name || '')) return false;
    vars.add(name);
    return vars.size <= 64;
  };
  const check = (steps, depth) => {
    if (depth > 5 || !Array.isArray(steps)) return false;
    return steps.every(s => {
      if (!s || !known.includes(s.type)) return false;
      if (s.type === 'loop') return check(s.steps, depth + 1);
      if (s.type === 'set' || s.type === 'inc') {
        if (!varOk(s.name)) return false;
        if (typeof s.value !== 'string' || s.value.length > 512) return false;
        if (s.type === 'inc') {
          if (!/^-?\d+$/.test(s.value)) return false;
          if (Math.abs(parseInt(s.value, 10)) > 1000000) return false;
        }
        return true;
      }
      if (s.type === 'if') {
        const cond = s.condition;
        if (!cond || typeof cond !== 'object' || !conditions.includes(cond.type)) return false;
        if (cond.timeout_seconds !== undefined &&
            !(Number.isInteger(cond.timeout_seconds) && cond.timeout_seconds >= 0 && cond.timeout_seconds <= 86400)) return false;
        if (['var_equals', 'var_not_equals', 'var_gt', 'var_gte'].includes(cond.type) && !varOk(cond.var)) return false;
        if (typeof cond.value === 'string' && cond.value.length > 512) return false;
        if (['var_gt', 'var_gte'].includes(cond.type) &&
            !(Number.isInteger(cond.number) && Math.abs(cond.number) <= 1000000)) return false;
        if (cond.type === 'health_below' &&
            !(Number.isInteger(cond.number) && cond.number >= 0 && cond.number <= 1000)) return false;
        if (!Array.isArray(s.steps)) return false;
        const elseSteps = s.else_steps === undefined ? [] : s.else_steps;
        return check(s.steps, depth + 1) && check(elseSteps, depth + 1);
      }
      return true;
    });
  };
  return check(script.steps, 0);
}

function stopRunner(username) {
  const runner = runners[username];
  if (!runner) return false;
  delete runners[username];
  try {
    runner.stop();
  } catch {}
  return true;
}

function runScript(username, script) {
  const bot = getBot(username);
  if (!bot) return;
  if (!validScript(script)) {
    send('error', { text: `${username}: invalid script shape` });
    return;
  }
  if (runners[username]) stopRunner(username);
  // remembered so auto-reconnect can restart it after the connection drops
  lastScripts[username] = script;
  const runner = createRunner(bot, script, {
    onStep: (index, stepType) => send('script_step', { username, index, step_type: stepType }),
    onFinished: (reason, error) => {
      delete runners[username];
      // keep the script for a reconnect restart only when the connection
      // killed it; any other end (completed/stopped/error/timeout) forgets it
      if (reason !== 'disconnected') delete lastScripts[username];
      const ev = { username, reason };
      if (error) ev.error = error;
      send('script_finished', ev);
    },
    onError: (stepIndex, message) => send('script_error', { username, step_index: stepIndex, message }),
  });
  runners[username] = runner;
  send('script_started', { username, name: script.name || '' });
  runner.start();
}

function stopScript(username) {
  if (!getBot(username)) return;
  // a deliberate stop must not restart on the next reconnect
  delete lastScripts[username];
  if (!stopRunner(username)) {
    send('error', { text: `${username}: no script running` });
  }
  // the runner's own finish emits script_finished/stopped
}

function reportScriptStatus(username) {
  if (!getBot(username)) return;
  const runner = runners[username];
  if (!runner) {
    send('log', { text: `${username}: no script running` });
    return;
  }
  const st = runner.status();
  send('log', { text: st ? `${username}: script at step ${st.index} (${st.step_type})` : `${username}: script finishing` });
}

function runCommand(username, cmd) {
  const bot = bots[username];
  if (!bot) {
    send('error', { text: `No bot "${username}"` });
    return;
  }
  // NOTE: a leading '/' makes the server run a command with the bot's
  // privileges. Chat-only vs /cmd is undecided; see follow-up.
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
  try {
    bot.pathfinder.setGoal(new goals.GoalFollow(target, 1), true);
  } catch (e) {
    send('error', { text: `${username}: pathfinder not ready (${e.message})` });
    return;
  }
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
  try {
    bot.pathfinder.setGoal(new goals.GoalBlock(x, y, z));
  } catch (e) {
    send('error', { text: `${username}: pathfinder not ready (${e.message})` });
    return;
  }
  send('log', { text: `${username} → going to ${x}, ${y}, ${z}` });
}

function goHome(username) {
  const bot = getBot(username);
  if (!bot) return;
  const s = bot.spawnPoint || { x: 0, y: 64, z: 0 };
  try {
    bot.pathfinder.setGoal(new goals.GoalBlock(s.x, s.y, s.z));
  } catch (e) {
    send('error', { text: `${username}: pathfinder not ready (${e.message})` });
    return;
  }
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
  const n = Math.min(item.count, Math.max(1, Math.floor(Number(count) || item.count)));
  bot.toss(item.type, null, n);
  send('log', { text: `${username} → dropped ${n} x ${item.name}` });
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

// strip control chars so chat text can never inject extra commands
function cleanChat(s) {
  return String(s).replace(/[\x00-\x1f\x7f]/g, '');
}

function whisper(username, player, message) {
  const bot = getBot(username);
  if (!bot) return;
  bot.chat(`/msg ${cleanChat(player)} ${cleanChat(message)}`);
  send('log', { text: `${username} → whispered ${player}` });
}

function respawnBot(username) {
  const bot = getBot(username);
  if (!bot) return;
  if (bot.health !== 0) {
    send('log', { text: `${username} is not dead, respawn skipped` });
    return;
  }
  // mineflayer has no respawn(); vanilla "perform respawn" packet instead
  bot._client.write('client_command', { actionId: 0 });
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
    // still waiting out a start delay? cancel it
    if (pendingJoins[username]) {
      clearTimeout(pendingJoins[username]);
      delete pendingJoins[username];
      send('log', { text: `${username} connect cancelled` });
      return;
    }
    // reconnect already scheduled? cancel that too
    if (reconnectTimers[username]) {
      clearTimeout(reconnectTimers[username]);
      delete reconnectTimers[username];
      delete reconnectAttempts[username];
      delete lastScripts[username];
      send('log', { text: `${username} reconnect cancelled` });
      return;
    }
    send('error', { text: `No bot "${username}"` });
    return;
  }
  deliberateQuit[username] = true;
  stopAntiAfk(username);
  delete lastScripts[username];
  bot.end();
  delete bots[username];
  send('log', { text: `${username} disconnected` });
}

function disconnectAllBots() {
  const names = Object.keys(bots);
  for (const name of names) {
    deliberateQuit[name] = true;
    stopAntiAfk(name);
    delete lastScripts[name];
    try {
      bots[name].end();
    } catch {
      // already ended or socket gone; still drop it below
    }
    delete bots[name];
    send('log', { text: `${name} disconnected` });
  }
  // pending start-delay connects and scheduled reconnects die with them
  for (const name of Object.keys(pendingJoins)) {
    clearTimeout(pendingJoins[name]);
    delete pendingJoins[name];
    send('log', { text: `${name} connect cancelled` });
  }
  for (const name of Object.keys(reconnectTimers)) {
    clearTimeout(reconnectTimers[name]);
    delete reconnectTimers[name];
    delete reconnectAttempts[name];
    send('log', { text: `${name} reconnect cancelled` });
  }
}

function listBots() {
  const names = Object.keys(bots);
  send('log', { text: names.length ? `Bots: ${names.join(', ')}` : 'No bots connected' });
}

const rl = readline.createInterface({ input: process.stdin, terminal: false });
rl.on('line', (line) => {
  let msg;
  try {
    msg = JSON.parse(line);
  } catch (e) {
    send('error', { text: `Invalid JSON: ${e.message}` });
    return;
  }
  try {
    switch (msg.cmd) {
      case 'join':
        createBot(msg.username, msg.server, msg.port || 25565, msg.version, {
          autoReconnect: msg.autoReconnect,
          autoRespawn: msg.autoRespawn,
          antiAfkSeconds: msg.antiAfkSeconds,
          startDelaySeconds: msg.startDelaySeconds,
        });
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
      case 'goto': {
        const x = Number(msg.x), y = Number(msg.y), z = Number(msg.z);
        if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(z)) {
          send('error', { text: 'goto: coordinates must be numbers' });
          break;
        }
        gotoPos(msg.username, x, y, z);
        break;
      }
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
      case 'run_script':
        runScript(msg.username, msg.script);
        break;
      case 'stop_script':
        stopScript(msg.username);
        break;
      case 'script_status':
        reportScriptStatus(msg.username);
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
    send('error', { text: `Command failed: ${e.message}` });
  }
});

// launcher closed stdin: drop every bot and quit, no zombies
process.stdin.on('end', () => {
  for (const name of Object.keys(runners)) stopRunner(name);
  disconnectAllBots();
  process.exit(0);
});
process.stdin.on('close', () => {
  for (const name of Object.keys(runners)) stopRunner(name);
  disconnectAllBots();
  process.exit(0);
});

// stdin does not close cleanly on every platform: watch the parent too
setInterval(() => {
  try {
    process.kill(process.ppid, 0);
  } catch {
    for (const name of Object.keys(runners)) stopRunner(name);
    disconnectAllBots();
    process.exit(0);
  }
}, 5000);

send('ready', {});
