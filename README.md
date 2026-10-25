## YaPB
[![Latest YaPB](https://img.shields.io/github/v/release/yapb/yapb)](https://github.com/yapb/yapb/releases/latest) [![Latest YaPB](https://github.com/yapb/yapb/workflows/build/badge.svg)](https://github.com/yapb/yapb/actions) [![YaPB License](https://img.shields.io/github/license/yapb/yapb)](https://github.com/yapb/yapb/blob/master/UNLICENSE) [![Downloads](https://img.shields.io/github/downloads/yapb/yapb/total)](https://github.com/yapb/yapb/releases/latest)

## AI caller framework: local 5v5 MVP

This fork extends YaPB for a local Counter-Strike 1.6 Dust2 match with stable identities, combat roles, team callers, editable strategies and recorded decisions. Originally built with Jev, it now uses **Perplexity Decider V1.2** (`perplexity/pplx-decider-v1.2`) through OpenRouter's Decisions API, polling **every five seconds per team**.

### Architecture and decision making

- **Native bots and local Node bridge:** movement, combat, buying and bomb interactions stay in the game; the bridge makes model requests outside the engine thread.
- **Silent orchestrator and separate CT/T callers:** a private world ledger tracks the match. Callers receive team observations, reports and public status; hidden enemy world positions stay out of model requests.
- **Plain-language briefings:** factual match reports describe the clock, score, economy, survivors, roles, weapons, named locations, sightings, sounds, kills/deaths and bomb information. Hypothetical examples explain tactical situations without presenting them as match events.
- **Bounded decisions:** readable options map to complete-team orders. Native validation checks request identity, roster revision, phase, timing, placements and expiry. One in-flight request per team, timeouts and local fallbacks keep play running through provider failures.
- **Immediate objective responses:** critical transitions can act before the next poll. Callers use native player actions and team chat, **never RCON**; administrative setup tools are separate.

### Players, roles and movement

- Five roster places per side, stable bot IDs across temporary player-slot changes, and human replacement/refill handling.
- An AWPer, lurker, entry and two supports on each team. Caller is an additional responsibility and passes to another living bot when necessary.
- Humans receive roles and objectives in team chat while retaining control of movement and purchases.
- Role-aware dispositions: entry is generally aggressive rather than forced to stand first; supports trade/cover; bomb carriers are cautious and stay behind the main group.
- Editable named positions and height-aware draft Dust2 callouts. The current registry has **85 graph-backed targets**, with room for temporary bomb objectives.
- Active holds, short forward/side peeks, collision/teammate/floor checks, advanced Long/Short holds, and alternating CT approach/site checks.
- Independent base combat tuning: the local Normal profile uses **180–280 ms reaction times**, reduced headshot preference and aim error. Strategic polling does not control individual reflexes.

### Terrorist coordination

- Four concrete main routes: **Long to A**, **Catwalk/Short to A**, **Upper Tunnels to B**, and **Mid Doors/CT Mid to B Doors**.
- Pistol rush options and three-stage non-rush attacks executing **30–60 seconds after freeze ends**. Bots stage at ready positions before execution.
- Run with guns drawn; sneak near quiet chokepoints, then run again once the approach is exposed.
- One eligible non-carrier can fake the other site without entering it and rejoin on execution.
- Confirmed target-site picks can accelerate execution. Other-site picks or heavy recent losses can permit a pivot without postponing the original deadline. Unknown kill locations do not imply a site opening.
- Recent own opening history recommends an underused route to reduce repetition; current evidence and bomb priorities take precedence.
- Support-first C4 carriage and safe pre-round handoff where possible. Carriers are excluded from fake/lurker assignments and regroup behind the largest teammate group.
- Dropped-bomb recovery takes priority: a collector retrieves C4 while the team covers. After pickup, cancel the old fake, clear obsolete tasks and reassess A/B, with a bounded fallback if the model cannot respond.
- Safe arrival in a legal plant zone can release the carrier from the planned wait and start planting. Postplant orders defend the bomb.

### Counter-Terrorist active defense

- Economy-aware openings: balanced Short/Long coverage, B-split defense, passive pistol coverage, close Short crossfire and a B eco stack.
- Defensive instructions and calls: hold sites, reinforce pressure, guard dropped C4, and retake/defuse after a plant. **CT man-advantage announcements are disabled.**
- Early reinforcement on fresh approach reports; stronger commitment for confirmed bomb attacks, three observed site intruders, or an evidence-backed overrun-site inference.
- Native-audible footsteps and gunfire reach the caller as approximate, short-lived evidence. Multiple sources can trigger cautious reinforcement before a visual entry or plant; hearing does not prove bomb location or an exact enemy count.
- Prior CT sightings identify repeated approaches. Eligible early four-player Long/Short/Mid/tunnels stacks retain an opposite-site defender. Bounded three/four-player crunches provide occasional defensive mix-ups and return to the original setup when quiet.
- Living players receive reinforcement jobs; survivors redistribute when dropped-bomb protection ends.
- Shared planted-bomb hearing/visual reports and site-elimination inference coordinate a retake. One designated defuser approaches the known bomb while teammates cover, with succession on death and respect for a human already defusing.
- Defuse repairs include a final approach and engine-confirmed latch protection: hold Use on every player-command tick, remain stationary, and prevent task/combat refreshes from restarting an active defuse. **Successful live completion after the latest repair is still unverified.**

### Economy and match rules

- Silent coordinated buys consider team money, role, assigned position, existing weapons and personal affordability.
- Save below **$3,500** average; half-buy at **$3,500–$4,499**; full-buy at **$4,500+**. Relevant last-half/match-point rounds override saves.
- Weapon-donation logic for wealthy bots with more than $4,500 left after their own buy, subject to equipment and nearby-recipient checks.
- **MR12**, halftime handling, first to 13, **$800 pistol starts**, and fresh 0–0 match/restart detection.
- **MR3 overtime at 12–12**, $10,000 each overtime half, repeated overtime when tied, **1:55 rounds** and a **0:40 bomb timer**.

### Strategy books and communication

- Per-map/per-side files: [`strategy/de_dust2/T/config.json`](strategy/de_dust2/T/config.json) and [`strategy/de_dust2/CT/config.json`](strategy/de_dust2/CT/config.json), using the general `strategy/<map>/<T|CT>/config.json` layout.
- **Twelve T entries and six CT entries**, with circumstances and pistol/buy/half-buy/save eligibility. Several T entries share routes and differ in economy guidance; these are not twelve distinct movement patterns.
- Books reload each decision without rebuilding. New physical routes still need authored positions and supported native behavior; descriptions alone do not implement choreography.
- Callers announce the matched opening strategy once per team per round, alongside roles/objectives and later execution/rotation calls.
- Both teams' bots report contacts, sounds, kills, bomb recovery and planting; a living caller relays teammate deaths. Cooldowns, speaker spacing, expiry and priority keep debugging chat bounded.

### Recording and local tools

- **SQLite:** private `bridge/logs/decisions.sqlite` retains structured state, exact model briefings/options, responses, latency, assignments, combat reports, telemetry and fallbacks.
- Native acknowledgements distinguish a reply from an accepted order; automatic/fallback orders retain their source. Progress-bar/defuse-task telemetry and latch transition logs support debugging.
- Read-only diagnostics on `127.0.0.1:28778`: `/health`, `/world`, `/callers/CT`, `/callers/T`, `/decisions?team=T&limit=50`, and `/decisions/<id>`. The world view is private and omniscient.
- Isolated Windows runtime, portable build tools and setup/build/start/stop scripts. The original Steam installation stays unchanged. Credentials, logs and database files remain outside Git.

### Setup, verification and limits

See [local setup](docs/jev-mvp.md), [decision eligibility and audit reference](docs/jev-decisions.md), and [strategy configuration](strategy/README.md). The current bridge requires **Node.js 24** for built-in SQLite, plus Steam CS 1.6/HLDS, Python and an OpenRouter key for the supplied setup. Connect to `127.0.0.1:27016`.

Verification includes **48 bridge tests**, native CT/economy/MR12/T policy checks, a Windows DLL build, graph checks and twelve real model-API scenarios. Live observations confirm five-second decisions, accepted orders, database recording, strategy announcements and both teams' sound/contact chat.

This is a **Dust2 MVP**. New-route movement, successful completion after the latest defuse-latch repair, full halftime side-switch acceptance and reliable weapon-donation transfers still need live playtesting. Callout boundaries are drafts. Coordinated flashes, grenade choreography, deliberate equipment-saving/recovery tactics and some described fallback movements remain unimplemented. Opponent/opening history resets with the bridge/new match; the database persists but does not yet learn a policy or automatically grade tactical quality. An accepted order does not prove movement success, and missing UDP acknowledgement is not proof of rejection.

## ☉ About
It's a computer controlled players (bots) for the Counter-Strike b6.5 - 1.6 and Counter-Strike: Condition Zero. Bots allows you to play that games without connecting any game server or even without internet.

## ☉ Documentation
* English: https://yapb.github.io/docs/en/
* Russian: https://yapb.github.io/docs/ru/

## ☉ Waypoints
All requests/bugs regarding bots navigation graph (waypoints) are located in this [repository](https://github.com/yapb/graph). if you have  waypoint request, please post an issue there.

## ☉ License
YaPB descends from POD-Bot by Count Floyd, whose 2003 source release grants "you're free to use it and abuse it like you want" - no license attached. We can't license other people's work under our own, so the code is released into the public domain under the [Unlicense](UNLICENSE).
