# Jev local framework

This fork connects YaPB to TypeSafe Jev through OpenRouter. The local scenario is
standard Steam `de_dust2`, five persistent CT identities, three A assignments and
two B assignments. The framework now supports named standing positions, fixed
roles, callout-area annotations and fresh placement decisions every ten seconds.

## Setup and running

Prerequisites: Steam Counter-Strike 1.6 (including `hlds.exe`), Git, Python with
pip, Node 22+, and an OpenRouter key with credit for live calls. The portable
toolchain needs no GPU, Windows SDK, or Visual Studio repair.

```powershell
git clone --recursive --branch jev-mvp https://github.com/aandrew/yapb.git
cd yapb
./tools/setup-local.ps1
# Set OPENROUTER_API_KEY in bridge/.env; never commit this file.
./tools/build-local.ps1
./tools/start-local.ps1
```

Keep the source in a `yapb` directory whose parent is writable for `work/`.
Setup pins workspace-local CMake, Ninja and Zig. The local DLL is 32-bit and
disables in-DLL TLS; the Node companion handles HTTPS and setup downloads the
Dust2 graph separately. This does not change the default upstream build flags.

The start script creates `../work/runtime/Half-Life` from the installed game,
installs the development DLL there, generates configuration, starts a localhost
server on port 27016 and a bridge on port 28777, and seeds the configured roster
plus one practice T. The normal Steam installation is unchanged. Use
`-SteamHalfLife <path>` when necessary. The bridge and server bind to localhost.

From the CS 1.6 console:

```text
connect 127.0.0.1:27016
```

Join CT to see the round-start team chat. Human players are not managed roster
members. Stop the game process and wait for it to exit before replacing its DLL.
Do not overlap server instances: an early overlapping restart encountered a
GoldSrc filesystem assertion, which the start/stop scripts now avoid.

```powershell
./tools/stop-local.ps1
./tools/build-local.ps1
./tools/start-local.ps1
```

## Stable identities and roles

`bridge/config/roster.json` is the source of truth. `seed.mjs` reads it to create
the bots, and `prepare-local.mjs` compiles it into the engine configuration.

| ID | Bot name | Role | Seed personality |
| --- | --- | --- | --- |
| ct_01 | CT_Jev_1 | awper | careful |
| ct_02 | CT_Jev_2 | lurker | careful |
| ct_03 | CT_Jev_3 | entry | rusher |
| ct_04 | CT_Jev_4 | support | normal |
| ct_05 | CT_Jev_5 | support | normal |

All five currently use difficulty 2. The role persists through round resets and
entity replacement and informs placement preferences. Personality biases use
existing YaPB behaviour. Dedicated weapon purchasing and per-role aim/reaction
overrides are not implemented by this change; existing money-aware buying still
applies, so an AWPer identity does not guarantee an equipped AWP.

Stable IDs are independent of slots. The engine binds each configured bot name
to its current CT bot entity, one-based player slot, engine userid and entity
generation. Every lookup revalidates that binding. Disconnection does not delete
or renumber the identity. Rejoining with its configured name rebinds the same ID
and role; pending responses for old bindings are invalidated by roster revision.
Only actual bot entities are eligible; a human with a matching name cannot bind.

Opening deployment waits until all five configured identities are alive. Once
deployed, polls continue with dead/disconnected identities in the state, marked
as unable to move. The three-A/two-B invariant is over identities, not a promise
that five defenders remain alive or physically stationary during combat.

## Named positions and callout areas

These are deliberately different data layers:

- `bridge/config/de_dust2.positions.json`: curated standing positions that can
  become movement orders. Each has a stable ID, A/B site, destination, viewing
  target, arrival radius, stance and preferred roles.
- `bridge/config/de_dust2.areas.json`: broader callout regions, aliases,
  height ranges and waypoint groups. Fine boundaries remain drafts and do not
  automatically become movement targets.

The five starter targets are `a_anchor`, `a_crossfire`, `a_close_cover`,
`b_anchor`, and `b_crossfire`. These are technical names for existing calibrated
spots; they do not claim to be a complete set of community callouts. Edit the
position registry and rerun prepare/start to add validated spots without editing
C++ source. Native loading checks map name, tokens, roles, finite coordinates,
radius, stance, nearby graph nodes, and distinct node reservations.

Bots use the existing graph to reach a named spot, then use its arrival radius,
viewing target and stance while running the normal task. Immediate combat
reactions can interrupt; redundant polls do not clear paths or restart tasks.
Conflicting legacy hunt/follow/camp/movement orders remain suppressed while a
managed placement is active.

### Mapping the supplied callout image

The reference uses the installed native 1024x768 Dust2 overview at pixel offset
(0,150) within a 1024x1024 canvas. With overview zoom 1.50 and origin (-223,1097),
world coordinates project to reference-image pixels as:

```text
pixel_x = 717.6875 - 0.1875 * world_y
pixel_y = 492.1875 - 0.1875 * world_x
```

The catalogue maps 57 names/areas to the actual 1,238-node YaPB graph. Draft
boundaries cover 1,014 nodes; 224 are intentionally left unmapped. Every named
area has graph members and a representative node. A/B zone boundaries were
checked against the BSP bomb-target models; smaller callouts require in-game
review. CT Spawn and Short use separate height ranges to avoid treating floors
at the same x/y as one area. More specific overlapping regions take precedence.

Examples include Goose, Default, Long, Pit, Short A, Catwalk, Mid Doors, CT Mid,
Window, B Doors, Upper/Lower Tunnels and the image's local aliases. Source
attribution and the graph checksum are stored in the catalogue. Node IDs must
be regenerated when the graph changes; prepare warns on a checksum mismatch.
Spatial draft labels alone do not establish visibility, reachability or cover.

```powershell
node bridge/areas-cli.mjs
node bridge/areas-cli.mjs "B Window"
# Rebuild region membership from a matching graph. --render also needs Pillow.
python tools/map-callouts.py --graph ../work/assets/de_dust2.graph
```

The bridge annotates each connected bot, candidate position and observed contact
with an area ID, label and verification status before calling Jev. Unknown
positions remain `unmapped`. The full catalogue is not copied into every model
request, and its draft representative nodes are not automatically activated.

## Polling and decisions

`yb_systemone` opts in (default 0). `yb_systemone_poll_interval` defaults to 10
seconds and is configurable from 3 to 120. The local seed explicitly sets 10.
All actual API work stays in `bridge/server.mjs`, outside the engine thread.

After round start and a short respawn settling period, the engine snapshots the
configured roster. Thereafter it creates a fresh snapshot every ten game-time
seconds during active rounds. A roster rebind schedules an early replacement
request; round resets invalidate the previous round's pending work.

Snapshots contain stable IDs, roles, validated entity bindings, alive/connected
flags, health, money, current weapon ID, locations, current named orders,
available positions, round time, bomb state, and the team's recent observed
contact. No hidden live enemy coordinates or entity pointers are exported.

The bridge offers ten complete three-A/two-B combinations. Within each candidate,
unique positions are selected using role fit, current position and travel cost.
When valid existing orders exist, an additional `hold` option preserves them.
Jev chooses one typed option. This supplies a working mid-round update mechanism
without introducing free-form tactics, arbitrary coordinates, or new playbooks.

There is one in-flight request and a two-second deadline. Retries reuse the
same immutable snapshot and nonce; the bridge deduplicates them. The wire reply:

```text
YJ2 round request nonce roster_revision target0 target1 target2 target3 target4
```

Targets are indices into that snapshot's position registry, in roster manifest
order. The engine validates round, request, nonce, roster revision, deadline,
indices, uniqueness, valid graph nodes and the 3/2 split before atomically applying
orders. Slots never substitute for stable IDs in the model's choices.

Opening failure uses a deterministic three-A/two-B fallback. A failed mid-round
request retains the last valid orders and tries again at the next interval.
Late/stale/invalid replies cannot overwrite them. A mid-round change does not
erase an active combat task; normal movement adopts the new goal afterward.

Round-start chat includes identity, role, site and named position, once per
identity per round. Polls do not repeat those announcements.

## Direct control and diagnostics

Run through the game/server console or the local RCON helper:

```text
yb jev_status
yb jev_order ct_01 b_anchor
yb_systemone_poll_interval 10
sv_restart 1
```

`jev_order` resolves the stable ID to its current binding. An occupied target
exchanges the two identities' positions, preserving uniqueness and the site
split. An unoccupied target is accepted only if the resulting split remains
valid. Direct orders cancel pending model replies and schedule the next poll
ten seconds later; they are not permanent locks against future model updates.

```powershell
node --env-file=bridge/.env bridge/control.mjs "yb jev_status"
node --test bridge/test.mjs
node --env-file=bridge/.env bridge/smoke.mjs
node bridge/verify-local.mjs
```

Ignored `bridge/logs/decisions.jsonl` records versioned request state, decisions,
latency, usage and two-second telemetry. Native logs identify requests, bindings,
applied orders and announcements. GoldSrc logs RCON commands including the local
RCON password: keep raw runtime logs private. Never commit `.env` or runtime data.

Local acceptance checks a real model decision and poll applied in the engine,
ten-second cadence, five matching role/position announcements, arrival near
assigned named spots, stable identity after rebinding, and rejection of invalid,
duplicate, stale-revision and late responses. Bridge tests cover schema, role
fit, unique placements, disconnected identities, wire binding and area floors.

References: [YaPB installation](https://yapb.github.io/docs/en/getting-started/installation/),
[YaPB builds](https://yapb.github.io/docs/en/getting-started/building/),
[Jev](https://openrouter.ai/docs/guides/community/jev).
The exact supplied callout image URL is preserved in `de_dust2.areas.json`.
