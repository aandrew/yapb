# Jev local 5v5 framework

The local Dust2 match has five roster places on each team. Stable bot IDs, combat
roles, named positions and caller decisions are shared by native YaPB and a Node
bridge. OpenRouter Jev (`typesafe/jev-1.14`) selects bounded team plans through
`https://openrouter.ai/api/alpha/decisions` every ten seconds. Urgent objective
transitions apply locally immediately, then ask Jev to refine placement.

## Run locally

Prerequisites: Steam Counter-Strike 1.6 with HLDS, Git, Python/pip, Node 22+ and an
OpenRouter key. No GPU or Visual Studio installation is needed.

```powershell
git clone --recursive --branch jev-mvp https://github.com/aandrew/yapb.git
cd yapb
./tools/setup-local.ps1
# Put OPENROUTER_API_KEY in the ignored bridge/.env file.
./tools/build-local.ps1
./tools/start-local.ps1
```

Setup uses portable CMake/Ninja/Zig. The development game copy and logs live in
`../work/`; the Steam installation stays unchanged. Stop with
`./tools/stop-local.ps1` before replacing the DLL. In the game console use:

```text
connect 127.0.0.1:27016
```

Join either side. A human replaces a roster bot so that each side stays at five.
The caller gives the human a combat role and named objective in team chat. Human
movement and purchases remain under human control. The first human fills a
support place. Multiple humans occupy additional roster roles. A disconnected
human's place is refilled by its original named bot. Bot IDs never depend on
transient player slots or user IDs; replies validate both the identity revision
and current engine binding.

## Round behavior

CT openings are B/Mid/Short/Short/Long or B/Mid/A/Long/Long. CTs gather actual team
sightings, public kill/death events and observed bomb information. A clear attack
means two distinct enemies seen within eight seconds, or one enemy sighting plus
a recent friendly death on that approach. Cautious reinforcement assigns three
to the threatened site, one opposite and one mid. A matching bomb-carrier sighting
within ten seconds commits everyone. A planted bomb triggers a full retake; one
living bot is released to normal defuse behavior. Unknown plant location retains
coverage while gathering information instead of inventing a target.

T has a caller overlay in addition to its combat role. Jev chooses A/B and either
a rush or one offered random delay from 45 to 80 seconds **after freeze ends**.
This time and target stay locked for the round. Before a delayed attack, bots
walk and clear up to three authored approach stages; supports have flank-facing
posts and lurkers use alternate routes. At the deadline all are ordered directly
to the chosen site. The C4 carrier may plant only after execution starts; after
planting, teammates hold site positions. Entry is an aggressive disposition
(YaPB Rusher), not a requirement to stay ahead of supports. No formation delay
forces teammates to wait behind the entry. Role preferences guide assignments;
combat and navigation continue normally.

Humans receive the same team calls and a once-per-round role announcement. A
living configured caller speaks, with another living bot as fallback. An
all-human team receives server team-chat advice.

## Configuration and protocol

- `bridge/config/roster.json`: five profiles per side, combat role, personality,
  difficulty and one preferred caller. AWPer/lurker are cautious; entry is a
  rusher; supports are normal. Weapon buying still uses YaPB's economy logic;
  this release does not guarantee an AWP purchase.
- `bridge/config/de_dust2.positions.json`: 45 exact graph-backed standing targets,
  sight directions, radius, role preferences, zone and clearing stage.
- `bridge/config/de_dust2.areas.json`: 57 draft callout areas and waypoint groups.
  A/B bomb zones are BSP-verified; fine boundaries and routes need human review.
- `bridge/prepare-local.mjs`: generates separate native CT/T configuration.

Version 3 snapshots contain only a team's roster, actual teammate state,
observed enemy contacts, public deaths, round phase, known objective and plan
clock. CT does not receive hidden live T coordinates or carrier locations.
Snapshots use immutable retries, separate team sockets and one in-flight call
per team. UDP is localhost only. Wire replies contain round, request, nonce,
roster revision, phase, target, formation, timing and five registry indices.
Native validation rejects stale, colliding or phase-incompatible targets.
Ordinary failures retain orders; objective-critical transitions use legal local
fallbacks. Turning `yb_systemone` off restores upstream behavior.

```powershell
node --env-file=bridge/.env bridge/control.mjs "yb jev_status"
node --test bridge/test.mjs
./tools/test-round-policy.ps1
node bridge/verify-local.mjs ../outputs/verification-v3.json
```

`yb jev_order <identity> <position>` remains subject to the same team policy.
Do not publish `.env`, runtime logs or raw RCON console output: the latter can
contain the local control password. The live verifier reports observed evidence
separately from untested scenarios. Automated bridge tests and native CT policy
assertions do not substitute for human movement/chat playtesting.
