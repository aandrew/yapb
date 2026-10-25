# Jev local MVP

This fork connects YaPB to TypeSafe Jev through OpenRouter. The first supported
scenario is the standard Steam `de_dust2` map with exactly five live CT **bots**.
Jev selects which three defend A; the other two defend B. All ten offered plans
have that exact split. Each bot sends one matching team-chat announcement during
freeze time. Normal aiming, shooting, and navigation remain in YaPB.

## Local Windows setup

Prerequisites: Steam Counter-Strike 1.6 (including `hlds.exe`), Git, Python with
pip, and Node 22 or later. An OpenRouter account with credits is needed for live
model calls. Neither a GPU nor a separate model installation is required.

Clone this fork recursively. Keep the source in a `yapb` directory with its
parent available for the `work` directory used by the local scripts.

```powershell
git clone --recursive https://github.com/aandrew/yapb.git
cd yapb
./tools/setup-local.ps1
# Edit bridge/.env and set OPENROUTER_API_KEY. Never commit this file.
./tools/build-local.ps1
./tools/start-local.ps1
```

The setup script pins workspace-local CMake, Ninja, and Zig. The build produces
a 32-bit Windows DLL without requiring a Windows SDK or Visual Studio repair.
TLS is disabled in this local DLL build because HTTPS is handled by Node; the
Dust2 graph is downloaded separately. This is a local build choice, not a global
YaPB build change.

The start script copies the installed game into `../work/runtime/Half-Life`,
installs this DLL and the configuration there, starts the bridge and a localhost
dedicated server, and adds five named CT bots plus one practice T bot. It
replaces development `server.cfg` and YaPB config entries in that copy. The
normal Steam installation is not edited. Use `-SteamHalfLife <path>` if Steam
is installed elsewhere. No public server port is opened.

From Counter-Strike 1.6, open the console and run:

```text
connect 127.0.0.1:27016
```

Join CT to see team-chat announcements; human CTs do not count toward the five
bot roster. Use `sv_restart 1` through the local control helper to repeat a round.
Do not launch a second server on top of the first. Stop and wait for the previous
process to exit before replacing its DLL or restarting. An overlapping early
restart encountered a GoldSrc filesystem assertion during development.

```powershell
node --env-file=bridge/.env bridge/control.mjs "sv_restart 1"
./tools/stop-local.ps1
```

## Integration

- `src/systemone.cpp`: round reset, roster snapshot, nonblocking loopback UDP,
  response validation, fallback, site waypoints, announcements, telemetry.
- `inc/systemone.h`: engine-owned round state; no model code on the engine thread.
- `bridge/decision.mjs`: typed request and response adapter for
  `https://openrouter.ai/api/alpha/decisions`, pinned to `typesafe/jev-1.14`.
- `bridge/server.mjs`: localhost UDP service on port 28777; API keys never enter
  the game process or state payload.
- `yb_systemone 1`: opt in; default is 0. Other maps and non-five-bot rosters
  retain ordinary YaPB behaviour.

After round start, wait 0.5 seconds for respawn and capture the live CT roster.
Send positions, health, map, sites, game time, and bomb state as JSON. Candidate
choices are the ten combinations of three IDs from five players. IDs in the
bridge protocol are **one-based player slots**, not YaPB's internal zero-based
indexes. No hidden enemy positions are exported.

The bridge returns `YJ1 request nonce a1 a2 a3`. YaPB accepts only three distinct,
ascending IDs from the captured roster, matching the current request and nonce,
before the two-second deadline. Replies are received from a connected localhost
UDP socket. The bridge deduplicates retries and permits only one API call at a
time. If the call fails or times out, the first three sorted CT IDs go A and the
remaining two go B. Late results cannot change that round's assignment.

The chosen destinations feed YaPB's existing normal-task goal logic. Conflicting
hunt, follow, camp, pause, and movement orders are suppressed while assigned.
Bots stop at their assigned waypoint and can react to immediate combat. They do not
teleport. This is an opening-deployment MVP; it does not promise five surviving
defenders or an unchanged physical count during combat, and does not implement
bomb-driven rotations, learning, arbitrary maps, or dynamic roster repair.

The A/B coordinates were checked against this installed Dust2 BSP's bomb-target
models. Five nearby candidate locations map to existing graph nodes. Other map
versions need calibration. `yb_debug_goal` can separately be used for a practice
opponent; CT deployment takes precedence over that debug setting.

## Verification and diagnostics

```powershell
node --test bridge/test.mjs
node --env-file=bridge/.env bridge/smoke.mjs
node bridge/verify-local.mjs
```

The bridge writes ignored `bridge/logs/decisions.jsonl`: request state, typed
decision, latency, provider usage, and position telemetry every two seconds.
`../work/logs` holds process output. The game writes `qconsole.log` and its normal
server logs inside the development runtime. GoldSrc logs RCON commands including
the local RCON password: keep raw runtime logs private. Verification exports only
deployment/chat evidence and never credentials.

Acceptance requires a real Jev decision, a matching 3-A/2-B assignment, exactly
five matching round-start announcements, and a live position snapshot with all
five CTs near their assigned sites. Test repeated rounds and repeat with the
bridge stopped: fallback must announce and preserve the same 3-A/2-B invariant.

References: [YaPB installation](https://yapb.github.io/docs/en/getting-started/installation/),
[YaPB builds](https://yapb.github.io/docs/en/getting-started/building/),
[Jev documentation](https://openrouter.ai/docs/guides/community/jev),
[Decisions tutorial](https://openrouter.ai/docs/guides/community/jev-tutorial).
