# Report bot

Vercel function behind the launcher's **Help > Report a Bug** dialog. Each report is posted to a **private Discord
channel** (an embed with the report, environment info, screenshots and the launcher log as a file), so users don't
need an account anywhere and nothing is public.

The webhook URL lives only in a Vercel environment variable. Never put it in the launcher itself: anyone could extract
it from the binary and spam your channel.

## Set up Discord

1. Create a channel for reports (text channel, or a **forum** channel to get one thread per report). Keep it private.
2. Channel settings > Integrations > Webhooks > New Webhook > **Copy Webhook URL**.
3. Optional: copy the id of a role you want pinged on every report (Developer Mode > right-click role > Copy Role ID).

## Deploy

1. Copy `api/report.js` into the Vercel project that serves pollymc.vercel.app (keep the `api/` folder).
   The launcher posts to `https://pollymc.vercel.app/api/report` (CMake option `Launcher_BUG_REPORT_API_URL`).
2. In Vercel > Settings > Environment Variables add:

   | Name | Value |
   | --- | --- |
   | `DISCORD_WEBHOOK_URL` | the webhook URL (required) |
   | `DISCORD_FORUM` | `1` if the webhook belongs to a forum channel, otherwise leave unset |
   | `DISCORD_PING_ROLE_ID` | role id to ping on each report (optional) |
   | `REPORT_EMBED_URL` | where the embed title links to (optional, defaults to the launcher repo) |

3. Redeploy. The old `GITHUB_*` variables are no longer used and can be deleted. Test with:

```sh
curl -X POST https://pollymc.vercel.app/api/report \
  -H 'Content-Type: application/json' \
  -d '{"type":"bug","name":"test","description":"Testing the report bot, please ignore.","launcherVersion":"9.4.1"}'
```

A `201 {"ok":true,"id":"..."}` answer means the message is in Discord.

## Notes

- Report types: `bug`, `crash`, `feature`, `other` (colour-coded). Launchers that don't send a type are filed as `bug`.
- Nothing a user types can ping anyone: the webhook is called with `allowed_mentions` empty. Only `DISCORD_PING_ROLE_ID` is allowed through.
- Screenshots must be real JPEGs under 1.5 MB, max 3, shown as a gallery inside the embed. The log is attached as `launcher-log.txt`
  (the launcher already hides personal paths). No third-party log host is needed any more.
- Vercel rejects request bodies above about 4.5 MB, so the launcher caps screenshots at 800 KB each and refuses to send reports over 4.3 MB.
- Discord rate limits are handled with one automatic retry; if it is still busy the user gets a "try again later" message.
- The built-in rate limit (5 reports/hour/IP) is best-effort on serverless. Add a Vercel WAF rate-limit rule on `/api/report` for real protection.
- If the webhook ever leaks, delete it in Discord and create a new one; only the Vercel variable needs updating, no launcher release.
