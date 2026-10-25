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
This time and target stay locked for the round. About half of delayed rounds optionally assign one random
living bot without C4 to fake at the other site's Long Doors/Upper Tunnels
approach. It holds outside that site and rejoins the main attack on execute.
Rushes never split a fake. Humans are never automatically assigned the fake.
Before a delayed attack, bots
run with guns to three authored approach stages. Walking is limited to within
300 units of a quiet approach chokepoint. Own-team firing, damage or enemy
contact marks the approach exposed for the rest of the round, allowing running; supports have flank-facing
posts and lurkers use alternate routes. At the deadline all are ordered directly
to the chosen site. The C4 carrier may plant only after execution starts; after
planting, teammates hold site positions. Entry is an aggressive disposition
(YaPB Rusher), not a requirement to stay ahead of supports. No formation delay
forces teammates to wait behind the entry. Role preferences guide assignments;
combat and navigation continue normally.

Humans receive the same team calls and a once-per-round role announcement. A
living configured caller speaks. If killed, another living bot takes over,
announces the handoff and keeps the existing site and execute clock. An
all-human team receives server team-chat advice.

## Configuration and protocol

- `bridge/config/roster.json`: five profiles per side, combat role, personality,
  difficulty and one preferred caller. AWPer/lurker are cautious; entry is a
  rusher; supports are normal. Silent caller buying now uses the team-average economy policy below;
  AWP selection requires an affordable long-lane assignment.
- `bridge/config/de_dust2.positions.json`: 52 exact graph-backed standing targets,
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
node --test bridge/test.mjs bridge/t-approach.test.mjs
./tools/test-round-policy.ps1
node bridge/verify-local.mjs ../outputs/verification-v3.json
```

`yb jev_order <identity> <position>` remains subject to the same team policy.
Do not publish `.env`, runtime logs or raw RCON console output: the latter can
contain the local control password. The live verifier reports observed evidence
separately from untested scenarios. Automated bridge tests and native CT policy
assertions do not substitute for human movement/chat playtesting.

T chat uses short messages with explicit A/B and readable ready points. Internal
X/Y route codes are not displayed. Avoid semicolons in bot chat: the engine bot
command parser splits commands at semicolons, including inside quoted messages.

## Silent coordinated buying

Both teams wait for the caller's opening placement, then lock one native buy
plan before spending. This uses the current team-average cash: below $3,500
saves (including the $2,500 case), $3,500–$4,499 is a half buy, and $4,500+ is a
full buy. Save rounds spend at most $200 on ammunition; half buys cap each bot
at $2,500 for SMGs/pistols and equipment. Existing primary weapons are kept.
Full buys prefer AK/M4, with cheaper Galil/FAMAS/SMG/pistol alternatives when
an individual cannot afford the preferred gun. An AWPer on a long lane tries
AWP, then Scout. Equipment and ammunition reserves come before weapon choice;
actual server restrictions, prices and personal cash are checked again on buy.

Score comes from the public TeamScore messages, independently of internal
round IDs and practice restarts. Under the MR12 assumption, the upcoming 12th
and 24th regulation rounds, or a round with either side at 12 wins, override
saving. Unknown scores never invent a last-round override. The native match controller preserves team scores across a side switch after
twelve rounds, resets pistol cash to $800, and ends regulation at thirteen wins.
A 12–12 tie starts MR3 overtime with $10,000 each half; 3–3 blocks repeat until
a team wins four rounds in a block. Completed matches restart as new pistol
matches. Console restarts reset the match to 0–0, while internal halftime and
overtime restarts preserve the score. Physical side switching still needs
interactive match playtesting; the rule transitions have native tests.

After their own purchase, a bot with **more than $4,500 left** may donate its
primary to a nearby teammate without one and with less than $3,500 cash, then
rebuy the same legal, affordable weapon. Snipers donate sniper guns only to
AWPers. Transfers are limited to one per donor/receiver during freeze, with a
short pickup window after freeze. Bots are directed to collect their allocated
weapon; humans collect a dropped gun themselves. All allocations are silent.

Human cash is learned from the normal server Money message and is bound to
the player's current user ID. Unknown cash is excluded from the average and
reported as unknown, never treated as zero. Humans retain control of purchases.
`yb jev_status` displays the locked buy mode, average and score. Private logs
record allocations; the Jev snapshot includes the economy summary.

## Active holds and base combat

CT and T bots pause, make short shoulder peeks (24–48 units), and return inside
their assigned area while waiting for team instructions. Body traces include
other players; floor checks reject unsafe drops and slopes. New assignments
cancel local peeks. Enemy combat, planting/defusing and pickup tasks retain
priority. Both sides keep guns out at assigned spots. These are local bot
behaviors and do not create extra Jev calls.

`bridge/config/difficulty.cfg` overrides the Normal profile used by the named
rosters: reaction 0.18–0.28 seconds, head aim 20%, aim error 12/12/18, and reduced
wall-fire chances. This is base YaPB tuning, independent of Jev. Reaction delay
is randomized within that configured window; actual encounter-to-shot latency
also includes visibility and AI scheduling. Miss rates remain distance/weapon
and situation dependent rather than a fixed accuracy percentage.

Latest checks: 21 bridge tests and native CT/economy/MR12/T-adaptation tests pass.
Save/half/full purchases were exercised live. The controlled donation test did
not produce a verified transfer; drops need further mixed-money playtesting.
No automated bot-kill tests should run while a human uses the local match.

## Shared bomb objectives and CT crunches

CT plant knowledge is shared from one bot's audible/visible confirmation, or
from a teammate covering the full hearing area of a site and hearing no bomb.
Clearing A infers B and vice versa; contradictory clears stay unknown. Unknown
plants remain searchable. Once known, the caller sends the team to that site.
One living bot is assigned to defuse (travel time and kit considered), follows
the shared coordinates directly, and is replaced if killed. The others cover
instead of independently searching/defusing. Exact CT plant coordinates are
sent to Jev only after a real team observation, not just the global plant event.

A dropped C4 creates temporary graph-backed objective positions. T radar
knowledge permits a recovery plan: one preferred support collects the bomb,
the others cover. Recovery overrides the attack/fake/timer until the bomb is
picked up, then the retained site plan resumes. CT guards require a real team
sighting and retain only the last seen location; a confirmed empty location or
visible carrier ends the guard. A plant always outranks guarding dropped C4.

During freeze, bot C4 holders hand it to a nearby living support (including a
support human through normal server inventory handling). With no support
available a surviving teammate remains a fallback. Human inputs are untouched.
Bot carriers use a cautious disposition/safe paths and wait behind teammates,
with exceptions for planting at the site or being the last player alive.
Rushes route the stack through the same bounded approach gate. A credited T
kill on a CT starts execution immediately once C4 is available, without waiting
for the original clock or another model call. Opening prompts favor more rushes.

Standard defense remains the default. Crunch opportunities have a cooldown
(two rounds when CTs are behind, four when tied/ahead), and Jev can decline
them. After the standard opening is assigned, an eligible early poll can choose three or
four CTs to crunch Long, Short, Mid or B tunnels. Unallocated players retain
their assignments. Authored nodes bound the push at Long corner/blue box,
Short stairs, Mid doors, or the B tunnel exit/upper tunnel segment. The original
assignments are saved and restored once survivors reach the area and it goes
quiet. A quiet stalled push times out; ongoing contact prevents an idle return.
Plant and confirmed bomb objectives take priority. New graph-backed boundaries
still need human playtesting.
