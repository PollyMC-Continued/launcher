// Script runner for launcher-driven bot scripts.
// Single-use state machine: walks steps, fires callbacks, cleans up.
// Created by bot-server/index.js, which maps callbacks onto wire events.
const RUNTIME_CAP_MS = 24 * 60 * 60 * 1000;
const MAX_LOOP_DEPTH = 5;
// script variables ('set'/'inc'): name shape and total distinct count caps
const MAX_VARIABLES = 64;
const VAR_NAME_RE = /^[A-Za-z0-9_]{1,32}$/;
// rolling chat history for 'if' conditions; small enough to never matter
const CHAT_BUFFER_MAX = 50;
// how often a timed 'if' condition is re-checked while waiting
const CONDITION_POLL_MS = 200;

function createRunner(bot, script, callbacks) {
  const state = {
    stack: [{ steps: script.steps, index: 0, times: 1, iter: 0 }],
    stopped: false,
    finished: false,
    startTime: Date.now(),
    cleanups: new Set(),
    vars: {},
    chat: [],
  };

  function onCleanup(fn) {
    state.cleanups.add(fn);
  }

  function runCleanups() {
    for (const fn of state.cleanups) {
      try {
        fn();
      } catch {}
    }
    state.cleanups.clear();
  }

  function finish(reason, error) {
    if (state.finished) return;
    state.finished = true;
    runCleanups();
    callbacks.onFinished(reason, error);
  }

  function fail(stepIndex, message) {
    callbacks.onError(stepIndex, message);
    finish('error', message);
  }

  function top() {
    return state.stack[state.stack.length - 1];
  }

  // Replace {varName} with the variable's current value. Unknown names stay
  // literal so typos never blank out chat text; replacement is a single pass
  // so values containing braces are not re-expanded.
  function substitute(text) {
    if (typeof text !== 'string' || text.indexOf('{') === -1) return text;
    return text.replace(/\{([A-Za-z0-9_]{1,32})\}/g, (whole, name) =>
      Object.prototype.hasOwnProperty.call(state.vars, name) ? String(state.vars[name]) : whole);
  }

  function varText(name) {
    const v = state.vars[name];
    return v === undefined ? '' : String(v);
  }

  // unset or non-numeric variables read as 0, matching 'inc' semantics
  function varNum(name) {
    const n = parseInt(state.vars[name], 10);
    return Number.isNaN(n) ? 0 : n;
  }

  // Evaluate an 'if' condition. Chat conditions only see messages that
  // arrived at or after sinceTs, the moment the if-step started.
  function evalCondition(cond, sinceTs) {
    const value = cond.value === undefined ? '' : String(cond.value);
    switch (cond.type) {
      case 'chat_contains':
        return state.chat.some(m => m.ts >= sinceTs && String(m.message).includes(value));
      case 'chat_equals':
        return state.chat.some(m => m.ts >= sinceTs && String(m.message) === value);
      case 'var_equals':
        return varText(cond.var) === value;
      case 'var_not_equals':
        return varText(cond.var) !== value;
      case 'var_gt':
        return varNum(cond.var) > cond.number;
      case 'var_gte':
        return varNum(cond.var) >= cond.number;
      case 'player_joined':
        return !!(bot.players && bot.players[value]);
      case 'health_below':
        return typeof bot.health === 'number' && bot.health < cond.number;
      default:
        return false;
    }
  }

  function advance() {
    while (!state.stopped && !state.finished) {
      // Pop all completed frames iteratively.
      while (state.stack.length > 0 && top().index >= top().steps.length) {
        if (state.stack.length === 1) { finish('completed'); return; }
        const child = state.stack.pop();
        child.iter++;
        if (child.times === -1 || child.iter < child.times) {
          child.index = 0;
          state.stack.push(child);
        }
      }
      if (state.stack.length === 0) { finish('completed'); return; }
      if (Date.now() - state.startTime > RUNTIME_CAP_MS) {
        finish('error', 'runtime cap reached');
        return;
      }
      const frame = top();
      const stepIndex = frame.index;
      const step = frame.steps[stepIndex];
      frame.index++;
      if (!runStep(step, stepIndex)) return;  // async, will call advance()
    }
  }

  function runStep(step, stepIndex) {
    if (state.stopped || state.finished) return false;
    if (state.stack.length > MAX_LOOP_DEPTH + 1) {
      fail(stepIndex, 'loop nesting too deep');
      return false;
    }
    try {
      callbacks.onStep(stepIndex, step.type);
      switch (step.type) {
        case 'say':
          bot.chat(substitute(step.text));
          return true;
        case 'command': {
          const text = substitute(step.text);
          bot.chat(text.startsWith('/') ? text : '/' + text);
          return true;
        }
        case 'log':
          return true;
        case 'set': {
          if (!VAR_NAME_RE.test(step.name || '')) {
            fail(stepIndex, `set: invalid variable name: ${step.name}`);
            return false;
          }
          if (!Object.prototype.hasOwnProperty.call(state.vars, step.name) &&
              Object.keys(state.vars).length >= MAX_VARIABLES) {
            fail(stepIndex, `set: too many variables (max ${MAX_VARIABLES})`);
            return false;
          }
          state.vars[step.name] = step.value === undefined ? '' : String(step.value);
          return true;
        }
        case 'inc': {
          if (!VAR_NAME_RE.test(step.name || '')) {
            fail(stepIndex, `inc: invalid variable name: ${step.name}`);
            return false;
          }
          if (!Object.prototype.hasOwnProperty.call(state.vars, step.name) &&
              Object.keys(state.vars).length >= MAX_VARIABLES) {
            fail(stepIndex, `inc: too many variables (max ${MAX_VARIABLES})`);
            return false;
          }
          const delta = parseInt(step.value, 10);
          if (Number.isNaN(delta)) {
            fail(stepIndex, `inc: value must be an integer, got: ${step.value}`);
            return false;
          }
          // a missing variable starts at 0; non-numeric reads as 0 too
          state.vars[step.name] = String(varNum(step.name) + delta);
          return true;
        }
        case 'wait': {
          const timer = setTimeout(() => {
            state.cleanups.delete(cleanup);
            advance();
          }, step.seconds * 1000);
          const cleanup = () => clearTimeout(timer);
          onCleanup(cleanup);
          return false;
        }
        case 'wait_for_chat': {
          let pattern = substitute(step.pattern);
          if (!step.case_sensitive) pattern = pattern.toLowerCase();
          const onChat = (who, message) => {
            const hay = String(message);
            if (step.case_sensitive ? hay.includes(pattern) : hay.toLowerCase().includes(pattern)) {
              done();
              advance();
            }
          };
          let timer = null;
          const done = () => {
            bot.removeListener('chat', onChat);
            if (timer) clearTimeout(timer);
            state.cleanups.delete(cleanup);
          };
          const cleanup = () => {
            bot.removeListener('chat', onChat);
            if (timer) clearTimeout(timer);
          };
          bot.on('chat', onChat);
          if (step.timeout_seconds > 0) {
            timer = setTimeout(() => {
              done();
              finish('timeout');
            }, step.timeout_seconds * 1000);
          }
          onCleanup(cleanup);
          return false;
        }
        case 'wait_for_player': {
          const player = substitute(step.player);
          if (bot.players[player]) {
            advance();
            break;
          }
          const onJoin = (joined) => {
            const name = typeof joined === 'string' ? joined : joined && joined.username;
            if (name === player) {
              done();
              advance();
            }
          };
          let timer = null;
          const done = () => {
            bot.removeListener('playerJoined', onJoin);
            if (timer) clearTimeout(timer);
            state.cleanups.delete(cleanup);
          };
          const cleanup = () => {
            bot.removeListener('playerJoined', onJoin);
            if (timer) clearTimeout(timer);
          };
          bot.on('playerJoined', onJoin);
          // timeout_seconds == 0 waits forever: only stop/disconnect ends it
          if (step.timeout_seconds > 0) {
            timer = setTimeout(() => {
              done();
              finish('timeout');
            }, step.timeout_seconds * 1000);
          }
          onCleanup(cleanup);
          return false;
        }
        case 'loop': {
          const frame = { steps: step.steps, index: 0, times: step.times, iter: 0 };
          state.stack.push(frame);
          return true;
        }
        case 'if': {
          // Copy the condition with its string operand substituted once:
          // variables cannot change while the branch decision is pending,
          // so per-poll re-substitution would give the same result.
          const cond = { ...(step.condition || {}) };
          if (typeof cond.value === 'string') cond.value = substitute(cond.value);
          const sinceTs = Date.now();
          const enterBranch = (steps) => {
            // an empty branch is a no-op frame: nothing to push
            if (Array.isArray(steps) && steps.length > 0)
              state.stack.push({ steps, index: 0, times: 1, iter: 0 });
          };
          if (evalCondition(cond, sinceTs)) {
            enterBranch(step.steps);
            return true;
          }
          const timeoutMs = Number(cond.timeout_seconds) > 0 ? cond.timeout_seconds * 1000 : 0;
          if (timeoutMs === 0) {
            // no wait window: a false condition takes the else branch right away
            enterBranch(step.else_steps);
            return true;
          }
          // Wait out the timeout re-checking the condition, so chat that
          // arrives during the window resolves the branch early instead of
          // at expiry. All condition types are re-checked on each poll.
          let poll = null;
          let timer = null;
          const done = () => {
            if (poll) clearInterval(poll);
            if (timer) clearTimeout(timer);
            state.cleanups.delete(cleanup);
          };
          const cleanup = () => {
            if (poll) clearInterval(poll);
            if (timer) clearTimeout(timer);
          };
          const settle = (steps) => {
            done();
            enterBranch(steps);
            advance();
          };
          poll = setInterval(() => {
            if (evalCondition(cond, sinceTs)) settle(step.steps);
          }, CONDITION_POLL_MS);
          // a timed-out condition is simply false: else branch, not an error
          timer = setTimeout(() => settle(step.else_steps), timeoutMs);
          onCleanup(cleanup);
          return false;
        }
        default:
          fail(stepIndex, `unknown step type: ${step.type}`);
          return false;
      }
    } catch (e) {
      fail(stepIndex, e && e.message ? e.message : String(e));
      return false;
    }
  }

  const capTimer = setTimeout(() => {
    finish('error', 'runtime cap reached');
  }, RUNTIME_CAP_MS);
  onCleanup(() => clearTimeout(capTimer));

  function onEnd() {
    if (!state.finished) finish('disconnected');
  }
  bot.once('end', onEnd);
  bot.once('kicked', onEnd);
  onCleanup(() => {
    bot.removeListener('end', onEnd);
    bot.removeListener('kicked', onEnd);
  });

  // Chat must stay observable for the whole run so 'if' conditions can look
  // back at it: keep a rolling buffer of recent messages with timestamps.
  const onChatRecord = (who, message) => {
    state.chat.push({ ts: Date.now(), from: who, message: String(message) });
    if (state.chat.length > CHAT_BUFFER_MAX) state.chat.shift();
  };
  bot.on('chat', onChatRecord);
  onCleanup(() => bot.removeListener('chat', onChatRecord));

  return {
    start() {
      advance();
    },
    stop() {
      if (state.finished) return;
      state.stopped = true;
      finish('stopped');
    },
    status() {
      if (state.finished || state.stack.length === 0) return null;
      const frame = top();
      // index points past the running step; report the running one
      const at = frame.index > 0 ? frame.index - 1 : 0;
      if (at >= frame.steps.length) return null;
      return { index: at, step_type: frame.steps[at].type };
    },
  };
}

module.exports = { createRunner, RUNTIME_CAP_MS, MAX_LOOP_DEPTH };
