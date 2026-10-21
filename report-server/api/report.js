// PollyMC-Continued bug report relay (Vercel serverless function).
// Receives a report from the launcher's "Report a Bug" dialog and posts it to a private Discord channel
// through a webhook. The webhook URL stays in an environment variable and never ships inside the launcher.
//
// Env vars:
//   DISCORD_WEBHOOK_URL   required. Channel > Edit > Integrations > Webhooks > Copy Webhook URL
//   DISCORD_FORUM         "1" if the webhook belongs to a *forum* channel: every report becomes its own thread
//   DISCORD_PING_ROLE_ID  optional role id to ping on every report (the only mention the bot is allowed to make)
//   REPORT_EMBED_URL      optional link the embed title points at (default: the launcher repo)

const WEBHOOK_URL = process.env.DISCORD_WEBHOOK_URL || '';
const FORUM = process.env.DISCORD_FORUM === '1';
const PING_ROLE = /^\d{5,25}$/.test(process.env.DISCORD_PING_ROLE_ID || '') ? process.env.DISCORD_PING_ROLE_ID : '';
const EMBED_URL = process.env.REPORT_EMBED_URL || 'https://github.com/PollyMC-Continued/launcher';

const MAX_DESCRIPTION = 4000;
const MIN_DESCRIPTION = 10;
const MAX_NAME = 40;
const MAX_CONTACT = 80;
const MAX_SHOTS = 3;
const MAX_SHOT_BYTES = 1.5 * 1024 * 1024;
const MAX_LOG_CHARS = 200000;

const TYPES = {
  bug: { label: 'Bug', emoji: '\u{1F41E}', color: 0xe74c3c },
  crash: { label: 'Crash', emoji: '\u{1F4A5}', color: 0x992d22 },
  feature: { label: 'Suggestion', emoji: '\u{1F4A1}', color: 0x3498db },
  other: { label: 'Other', emoji: '\u{1F4AC}', color: 0x95a5a6 },
};

// Best-effort rate limit. Serverless instances don't share memory, so this only slows down
// casual spam; for real protection add Vercel's WAF rate limiting on /api/report.
const PER_IP_LIMIT = 5;
const WINDOW_MS = 60 * 60 * 1000;
const hits = new Map();

function rateLimited(ip) {
  const now = Date.now();
  const recent = (hits.get(ip) || []).filter((t) => now - t < WINDOW_MS);
  if (recent.length >= PER_IP_LIMIT) {
    hits.set(ip, recent);
    return true;
  }
  recent.push(now);
  hits.set(ip, recent);
  if (hits.size > 5000) hits.clear();
  return false;
}

// One-line fields: no newlines, no characters that would turn into markdown links.
function oneLine(text, max) {
  return String(text || '')
    .replace(/[\r\n\t]+/g, ' ')
    .replace(/[`<>[\]]/g, '')
    .trim()
    .slice(0, max);
}
function str(value, max) {
  return typeof value === 'string' ? value.slice(0, max) : '';
}
function clip(text, max) {
  return text.length > max ? `${text.slice(0, max - 1)}\u2026` : text;
}

function webhookEndpoint() {
  const url = new URL(WEBHOOK_URL);
  url.searchParams.set('wait', 'true'); // makes Discord return the created message, so we know it really worked
  return url.toString();
}

async function postToDiscord(payload, files) {
  const build = () => {
    const form = new FormData();
    form.append('payload_json', JSON.stringify(payload));
    files.forEach((f, i) => form.append(`files[${i}]`, new Blob([f.data], { type: f.type }), f.name));
    return form;
  };

  for (let attempt = 0; attempt < 2; attempt++) {
    const res = await fetch(webhookEndpoint(), { method: 'POST', body: build() });
    if (res.status === 429 && attempt === 0) {
      // Discord rate limit: wait the advertised time (capped) and try once more.
      const data = await res.json().catch(() => ({}));
      await new Promise((r) => setTimeout(r, Math.min(Math.ceil((data.retry_after || 1) * 1000), 3000)));
      continue;
    }
    const data = await res.json().catch(() => ({}));
    if (!res.ok) {
      const err = new Error(`Discord webhook -> ${res.status} ${data.message || ''}`);
      err.status = res.status;
      throw err;
    }
    return data;
  }
  const err = new Error('Discord webhook -> rate limited');
  err.status = 429;
  throw err;
}

module.exports = async (req, res) => {
  if (req.method !== 'POST') {
    res.setHeader('Allow', 'POST');
    return res.status(405).json({ error: 'POST only' });
  }
  if (!/^https:\/\/(?:[a-z]+\.)?discord(?:app)?\.com\/api\/webhooks\/\d+\/[\w-]+$/i.test(WEBHOOK_URL)) {
    console.error('DISCORD_WEBHOOK_URL is missing or malformed');
    return res.status(500).json({ error: 'Report server is not configured.' });
  }

  const ip = String(req.headers['x-forwarded-for'] || req.socket?.remoteAddress || 'unknown').split(',')[0].trim();
  if (rateLimited(ip)) return res.status(429).json({ error: 'Too many reports, please try again later.' });

  let b = req.body;
  if (typeof b === 'string') {
    try {
      b = JSON.parse(b);
    } catch {
      b = null;
    }
  }
  if (!b || typeof b !== 'object') return res.status(400).json({ error: 'Invalid request.' });

  const description = str(b.description, MAX_DESCRIPTION).trim();
  if (description.length < MIN_DESCRIPTION) return res.status(400).json({ error: 'Please describe the problem a bit more.' });

  const type = TYPES[b.type] ? b.type : 'bug'; // older launchers don't send a type
  const meta = TYPES[type];
  const name = oneLine(b.name, MAX_NAME) || 'Anonymous';
  const contact = oneLine(b.contact, MAX_CONTACT);
  const version = oneLine(b.launcherVersion, 40) || 'unknown';
  const platform = oneLine(b.platform, 40) || 'unknown';
  const system = oneLine(b.system, 80) || 'unknown';
  const arch = oneLine(b.arch, 20) || 'unknown';
  const qt = oneLine(b.qt, 20) || 'unknown';

  // Attachments. Screenshots must decode to a real JPEG (the launcher always sends JPEG) and stay small.
  const files = [];
  for (const raw of Array.isArray(b.screenshots) ? b.screenshots.slice(0, MAX_SHOTS) : []) {
    if (typeof raw !== 'string' || raw.length > MAX_SHOT_BYTES * 1.4) continue;
    const buf = Buffer.from(raw, 'base64');
    if (buf.length > MAX_SHOT_BYTES || buf.length < 4 || buf[0] !== 0xff || buf[1] !== 0xd8 || buf[2] !== 0xff) continue;
    files.push({ name: `screenshot-${files.length + 1}.jpg`, type: 'image/jpeg', data: buf });
  }
  const shotCount = files.length;
  const log = str(b.log, MAX_LOG_CHARS);
  if (log) files.push({ name: 'launcher-log.txt', type: 'text/plain', data: Buffer.from(log, 'utf8') });

  const firstLine = oneLine(description.split(/\r?\n/)[0], 80);
  const fields = [
    { name: 'Type', value: meta.label, inline: true },
    { name: 'Reported by', value: name, inline: true },
    ...(contact ? [{ name: 'Contact', value: contact, inline: true }] : []),
    { name: 'Launcher', value: `${version} (${platform})`, inline: true },
    { name: 'System', value: system, inline: true },
    { name: 'CPU / Qt', value: `${arch} / Qt ${qt}`, inline: true },
  ];

  // Embeds that share the same url are shown by Discord as one gallery, so every screenshot lands inside the report.
  const embeds = [
    {
      title: clip(`${meta.emoji} ${firstLine}`, 250),
      url: EMBED_URL,
      description: description,
      color: meta.color,
      fields,
      footer: { text: log ? 'Launcher log attached' : 'No launcher log attached' },
      timestamp: new Date().toISOString(),
      ...(shotCount ? { image: { url: 'attachment://screenshot-1.jpg' } } : {}),
    },
  ];
  for (let i = 2; i <= shotCount; i++) embeds.push({ url: EMBED_URL, image: { url: `attachment://screenshot-${i}.jpg` } });

  const payload = {
    username: 'PollyMC Reports',
    embeds,
    // Nothing the user typed may ping anybody. Only the configured role (if any) is allowed through.
    allowed_mentions: PING_ROLE ? { parse: [], roles: [PING_ROLE] } : { parse: [] },
  };
  if (PING_ROLE) payload.content = `<@&${PING_ROLE}> new ${meta.label.toLowerCase()} report`;
  if (FORUM) payload.thread_name = clip(`${meta.label}: ${firstLine}`, 100);

  try {
    const message = await postToDiscord(payload, files);
    return res.status(201).json({ ok: true, id: message.id });
  } catch (err) {
    console.error(err.message);
    const busy = err.status === 429;
    return res.status(busy ? 503 : 502).json({ error: 'Could not send the report right now. Please try again later.' });
  }
};
