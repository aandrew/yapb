# Jev decision reference

This documents implemented behavior, not proposed strategy intent. The engine determines the current phase, the bridge builds valid complete-team orders, Jev chooses one, and the engine validates and applies it. Objective-critical changes also run locally without waiting for Jev. A logged provider decision proves a reply was sent; native `[Jev] Order ... source=jev` lines establish acceptance.

## T choice menu

| Choice | When available | Effect |
|---|---|---|
| `rush_A`, `rush_B` | Opening is unlocked and native rush gate permits it | Select site and immediate execute deadline. Allowed on pistol rounds and every fourth match round as a mix-up. |
| `clear_then_A_<n>s`, `clear_then_B_<n>s` | Opening is unlocked | Select site and one generated delay from 30 to 60 seconds after freeze. Both sites receive the same delay (`30 + nonce % 31`); Jev does not choose any arbitrary time. |
| `role_order` | Locked clearing, execution or postplant phase | Reassign positions using stronger role fit. |
| `closest_order` | Same locked phases | Reassign positions giving distance more weight; carrier/phase constraints remain mandatory. |
| `hold` | Existing assignments still validate in current phase | Retain the current assignments. |
| `site_pick_execute_now` | Clearing, round live, at least one T alive, enemy CT killed on target site within 12 seconds, no dropped/planted bomb | Execute current site now. Native code also triggers this automatically, so the option may disappear before the next poll. Strict numerical advantage is not required. |
| `other_site_pick_execute` | Clearing/execution, at least two Ts alive, confirmed CT pick on other site within 12 seconds, no previous pivot and no dropped/planted bomb | Pivot and execute the other site. |
| `casualties_switch_site` | At least two T deaths within 12 seconds, two Ts still alive, no prior pivot, no dropped/planted bomb; executing already or at least 8 seconds remain before execute | Switch to other site, preserving the original deadline. |
| `recover_bomb_first` | Dropped-bomb recovery phase | Whole-team recovery deployment, with designated collector pinned to pickup point and others covering. Only recovery choice is offered. |
| `recover_then_attack_A`, `recover_then_attack_B` | Pickup confirmed and reassessment pending | Choose a fresh site even after an earlier pivot. Stage at final ready positions if more than 8 seconds remain before execute; otherwise execute. Carrier stays with main support route. |

Native T behavior advances clearing stages after the entry (or replacement) is within 128 units of its stage goal for 2 seconds. The original execute deadline causes local execution. A planted bomb switches locally to postplant. Recovery cancels the old fake, clears obsolete pickup tasks, and limits reassessment to 2.5 seconds. The offline recovery fallback compares recent team sightings and confirmed site losses, treating unknown defenders as uncertain; ties keep the original target. It does not inspect hidden CT locations.

## CT choice menu

At opening, applicable strategy-book entries restrict offered choices when any match. Pistol status takes precedence; ready economy modes full/last_round map to buy, half to half_buy, save to save. If economy is not ready, no economy-specific category is inferred and base opening choices remain available.

| Choice | Situation | Placement |
|---|---|---|
| `b_mid_two_short_one_long` | Buy-book Short-pressure alternative; base fallback menu when no applicable opening book | B, Mid, Short, Short, Long |
| `b_mid_a_two_long` | Buy-book Long-control alternative; base fallback menu | B, Mid, A, Long, Long |
| `two_b_mid_a_long` | Buy category | B, B, Mid, A, Long |
| `two_b_two_a_ct` | Pistol | B, B, A Short-facing, A Long-facing, CT-spawn rotator |
| `b_mid_two_close_short_a` | Half-buy category | B, Mid, close Short, close Short, A |
| `four_b_one_a` | Save category | Four B, one A |
| `hold` | Current order validates, outside opening | Preserve current positions. |
| `cautious_A_0/2`, `cautious_B_0/2` | Clear attack evidence on a site | Three assigned to threatened site, one other site, one Mid. Suffix changes chosen site positions. |
| `committed_rotate_A/B_roles/nearest` | Confirmed bomb attack or at least three observed site intruders | All assignments go to selected site; role-fit or distance-weighted assignment. |
| `retake_A/B_roles/nearest` | Bomb planted and shared site known | All survivors retake selected site; native system designates defuser. |
| `guard_dropped_bomb` | CT has observed the dropped bomb | Guard positions surrounding bomb. |
| `crunch_<long/short/mid/b_tunnels>_<3/4>` | Early gather, five CTs alive, eligible mix-up round, no known dropped/planted bomb | Three/four nearest participants occupy bounded choke positions, others retain positions. |
| `hold_crunch` | Crunch already active | Retain crunch until native return/override. |

The standard opening formations are also available during gather and locate-bomb phases; the opening-book restriction applies specifically to phase 0. Economy tags are not a global ban on later placements.

## CT phase priorities (highest first)

1. Planted bomb: retake known site, otherwise locate bomb. Locate phase retains defense choices; native bomb search handles unknown-site navigation, rather than Jev inventing a location.
2. Observed dropped bomb: guard it.
3. Clear attack with bomb seen within 10 seconds: commit.
4. At least three observed Ts inside a site footprint: commit (more intruders wins; tie selects A).
5. Continue an active crunch unless overridden above.
6. Clear attack without bomb confirmation: cautious rotation.
7. Otherwise gather/standard defense.

Clear attack means at least two enemy sightings, or one sighting plus one CT death attributed to that site. Sighting/death evidence uses an 8-second window. When both sites qualify, more sightings wins; equal sightings retain the previous A/B target when available. Site footprints/callout boundaries are geometric approximations, not tactical certainty.

Crunch availability is restricted to the first 20 seconds after freeze. When CT is behind, eligibility uses every second match round with a two-round gap; otherwise every fourth with a four-round gap. Quiet return is local: after at least 6 seconds and arrival, or by 20 seconds unless recent nearby contact persists. Bomb priorities interrupt crunches. Native CT bomb pickup handling returns survivors to site coverage.

## What Jev weighs, rather than what code guarantees

The prompt asks Jev to prefer pistol rushes, delayed gun rounds, evidence-backed pivots, weaker observed sites, cautious bomb carriage and applicable book circumstances. These are model preferences without a documented numerical scoring function. Eligibility and placement validation are deterministic; choosing among eligible options is probabilistic. Unknown sites are explicitly not treated as clear.

The ten T book entries are not ten distinct native strategies. Six economy-specific descriptions share the two staged A/B routes. The announced/logged book name is matched from the selected route, preferring the applicable economy-specific entry; Jev does not separately return that strategy ID. Favorable Long spawns, coordinated flash timing, utility counts, automatic weapon-saving/weapon-recovery decisions and Long-player fallback choreography are not fully encoded by these config descriptions.

Buying is native coordinated policy, not a Jev deployment answer. It considers economy, role, position, existing equipment and individual affordability. Low-money/half/full and last-round overrides are handled in native code.

## How to assess a decision

For a test, record: team/round/request; phase; alive counts; target/deadline; recent confirmed contacts and kill/death sites; dropped/planted/picked-up bomb state; offered choices; provider selection; native accepted order; and the movement outcome.

Current private bridge logs retain request state, decision choice, matched book entry, optional confidence/probabilities, latency and fallbacks. The exact outgoing model request can be reconstructed from that state and the matching code/config revision; book edits are reloaded and not embedded in the logged request snapshot. Native order logs retain accepted assignments and source. Neither strategy chat nor a provider decision by itself proves movement succeeded.

Recommended next audit format (not yet implemented): a compact record for each decision with trigger, evidence ages, every offered option and eligibility reason, selected option, matched book entry, automatic-versus-model source, native acceptance/rejection, fallback reason and observed outcome. This would expose eligibility failures, service failures and bot navigation failures separately. A short stated rationale can be requested from a model, but would be an explanation, not access to internal reasoning or proof the evidence caused the choice.


## Natural-language caller input

The provider now receives `state.briefing`, a literal tactical match report, instead of the raw game snapshot. It describes objectives, clock, score/economy, alive counts, teammate roles/health/weapons and named locations, bomb possession/drop/plant, attack clock, recent sightings and quiet kill/death reports with ages, previous-round context and observed site uncertainty. Coordinates, waypoint IDs, nonce/revision fields and duplicate internal objects remain local. Each available choice has a plain-language action, applicable strategy guidance and living-player destinations. Explicit hypothetical examples distinguish a 4v4 site pick from an advantage, a cross-site pivot, bomb recovery, and CT site commitment.

Full structured state is still validated locally and used to build and validate the exact same complete-team orders. The bridge records `model_request` with the exact provider payload alongside its private structured request log, so reviewers can see what Jev actually read for each decision. This changes representation and prompting, not the menu or model. Natural-language superiority is a testable hypothesis, not an established result.


## Local decision database

The bridge stores private test data in `bridge/logs/decisions.sqlite` (SQLite WAL, no external service). `events` retains structured requests, exact model requests, decisions, fallbacks, quiet combat reports, telemetry and accepted native orders. `decisions` indexes one request by server session/map/team/round/request/nonce/roster revision with full state, exact provider briefing/options and response. `applied_orders` records native accepted placements, including automatic and fallback orders, with their source. This is separate from a provider selecting a response. No credentials or API headers are stored. Database and WAL files are ignored through the logs directory.

Read-only local endpoints: `http://127.0.0.1:28778/decisions?team=T&round=2&limit=50` (latest first, maximum 200), `/decisions/<id>` (full input, response and correlated accepted orders), `/health` (counts and database write health). Data persists across bridge restarts. Collection starts with this release; older JSONL logs are retained separately and are not automatically imported.

Request status `requested` means no reply/fallback recorded yet; `reply_sent` means the bridge sent an order, not that the engine accepted it; `accepted` requires a matching native acknowledgement; `fallback` means the provider/bridge request failed. Automatic/fallback accepted orders remain in their own table even without a Jev decision. Telemetry provides later game-state context, but no automatic tactical-quality label, match win attribution or physical movement-success claim is added. UDP acknowledgements can be lost: lack of acknowledgement is not proof of rejection. Database write failure is surfaced in health and stderr, while existing JSONL logging continues.

Example review queries:

```sql
SELECT team, choice, status, count(*) AS n, round(avg(latency_ms)) AS avg_ms
FROM decisions GROUP BY team, choice, status;
SELECT server_session, map, round, team, source, phase, target_site, assignments_json
FROM applied_orders ORDER BY id DESC LIMIT 50;
```


## CT active defense and Decider migration

The current decision model is `perplexity/pplx-decider-v1.2`, using the existing OpenRouter `/api/alpha/decisions` contract. Polling is every five seconds per team, with one in-flight request per team and the existing 1.8-second provider timeout / two-second native acceptance deadline. Earlier objective-driven updates remain enabled. The real API migration matrix is `node --env-file=bridge/.env bridge/verify-decision-model.mjs`; its private JSON report covers nine CT/T scenarios and validates the selected menu choice and resulting wire order. Unit tests also cover rate limits, provider failures, malformed answers and timeout behavior. Passing these checks establishes integration compatibility, not better tactical performance.

CT caller briefings now describe observed attack tendencies. The orchestrator keeps up to five previous observed rounds of CT sightings per session/map and resets on a detected new match. At least three unique enemies seen within the normal eight-second observation window establishes a grouped approach. A four-player Long/Short/Mid/tunnels stack is offered when that approach had an early group (first 35 seconds) in at least two previous rounds and at least half of the observed history. Stack choices are limited to opening/gather, five live teammates and the first 25 seconds. They hold the bounded choke, leave one opposite-site defender and yield to current bomb/attack priorities. Unlike a crunch, the stack does not automatically return after a short quiet interval; later caller orders can redistribute it. No hidden enemy positions contribute. History currently lives in bridge memory, so a bridge restart starts fresh; the underlying decision database persists.

`preempt_reinforce_A/B` is offered during gather on a single fresh site-approach sighting, retaining opposite-site and Mid cover rather than sending all five. Jev can retain the baseline defense if the evidence is weak; one contact does not force a full rotation. Cautious/preemptive plans prioritize living defenders for reinforcement slots rather than counting dead identities as arriving defenders.

An overrun site triggers local full commitment before waiting for Jev: at least two observed intruders, a CT death attributed to that site within eight seconds, and no living CT inside its defined site footprint. `site_lost_a/b` records this inference explicitly; it is not proof of permanent control. Three observed intruders still trigger full commitment independently, and planted bombs / known dropped bombs retain priority.

Validation for this release: 44 bridge tests including database persistence/correlation and CT history isolation; native CT/economy/MR12/T policy groups; native DLL build; nine live API scenarios. Live server cadence, database writes and native acceptances are checked separately after deployment. Tactical effectiveness requires actual playtesting.


Deployment observation: both CT and T issued five-second intervals (5.00–5.01 seconds in the initial sample), and native acknowledgements populated accepted decision records using `perplexity/pplx-decider-v1.2-20261016`. One T request exceeded the 1.8-second deadline; native fallback was recorded and subsequent T decisions were accepted. This is a demonstrated handled timeout, not a claim of zero provider failures. A sent reply without acknowledgement remains `reply_sent`; it is not labelled accepted or rejected.
