# Jev MVP development

This is a fork of yapb/yapb. `origin` is aandrew/yapb; `upstream` is yapb/yapb.
See docs/jev-mvp.md for setup, protocol, acceptance, and current limits.

- Preserve the upstream bot behaviour when yb_systemone is disabled.
- The scenario is standard de_dust2 and five roster places on each side.
  Humans replace named bots; send humans team-chat advice, never input commands.
  Dead or disconnected identities stay in the roster; never renumber them.
- Use one team-level choice, not independent per-bot choices. CT round policy
  now replaces the original 3-A/2-B test: openings use the map strategy book: balanced buy defenses, B-split defense,
  passive pistol, close Short half-buy, or B eco stack. Clear attacks allow cautious reinforcement; confirmed
  bomb attacks commit all identities, and planted bombs trigger a full retake.
- Phase transitions must be supported by recent CT observations and public
  kill/death information. Keep hidden live enemy positions and bomb-carrier locations in the silent
  orchestrator only. Callers receive team reports and derived public/observed status. Validate each placement against its phase and target site.
- Author roster/roles in bridge/config/roster.json and exact named targets in
  bridge/config/de_dust2.positions.json. Runtime files are generated from these.
- Fine callout boundaries in bridge/config/de_dust2.areas.json are drafts, not
  approved standing positions. Keep unknown locations unmapped. Respect height
  ranges for overlapping floors and the source graph hash when using node IDs.
- Caller is an overlay on a combat role. Lock each T site and rush/30–60 second
  attack delay once per round, measured after freeze. A fresh kill and player
  advantage may offer an early execute at the next Jev poll. Two recent T
  deaths may offer one site switch; never postpone the original clock. Run with guns to ready
  points, sneak only near quiet chokepoints, and run once exposed. At most one
  non-C4 bot may fake outside the other site before rejoining on execute.
  Keep caller succession and its chat announcement when a caller dies.
  Avoid semicolons in chat (the fake-command parser splits at them).
  Clear in stages until the
  deadline, then execute. Entry is an aggressive disposition, not a mandatory
  lead position; never hold supports back to enforce entry-first movement.
- A planted bomb uses shared CT hearing/visual evidence or safe site-elimination
  inference. One defuser follows shared bomb coordinates, others cover. Prefer
  supports to carry C4, keep bot carriers behind teammates, recover a dropped
  bomb before T site plans, and let CTs guard a bomb they actually observed.
- CT may use evidence-backed early four-player choke stacks and single-contact
  preemptive reinforcement. Use CT-observed prior-round patterns only; current
  bomb/attack evidence overrides predictions. Do not count dead slots as reinforcements.
- CT caller may crunch Long/Short/Mid/B tunnels with 3-4 players early. Bound
  target nodes to the choke, save original assignments, and return when quiet.
  A confirmed pick on the selected site can trigger immediate execution.
  Other-site picks may offer an optional pivot; unknown kills imply no site
  opening. Only call man advantage when living T count exceeds CT count.
- Assigned spots are bounded active holds: short safe peeks and returns, not
  frozen actors. Check body clearance, teammates and floor/drop height. Preserve
  normal combat, bomb work and immediate new orders.
- Do not run destructive round-advancement or bot-kill tests on the live server
  while a human is playing.
- Coordinate silent buys after opening placement. Save below $3,500 average,
  half buy up to $4,499, full buy at $4,500+. Preserve existing primaries and
  personal affordability. MR12 last-half/match-point rounds override saves;
  use public TeamScore, never director round counters. Only donate after own
  buy with more than $4,500 left, a legal replacement and a nearby poor teammate.
- Keep five-second polling bounded to one immutable snapshot and one in-flight
  request per team. Failed updates retain prior orders; round/roster changes invalidate
  old replies. Repeated polls must not repeat round-start announcements.
- Caller actions must come from bot/player controls and team chat, never RCON.
  RCON tooling is only for local server setup/administration.
- Keep API requests outside the engine thread. Never send entity pointers. Omniscient world packets stay in the local
  orchestrator; never inject hidden enemy state into caller model requests. Validate round/request/nonce, roster membership, and expiry.
- bridge/.env and all runtime/log files are private. Never commit credentials or
  print environment values. Game RCON logs can contain a local control password.
- Run `node --test bridge/test.mjs` for bridge changes. Build native changes with
  tools/build-local.ps1. Use the actual local game for movement/chat acceptance;
  mocked decisions alone do not establish end-to-end success.
- Stop the local server and wait for it to exit before replacing its DLL.
- Maintain the existing C++ style and both CMake and Visual Studio source lists.
- Keep experimental server-side migration separate from the local MVP.
