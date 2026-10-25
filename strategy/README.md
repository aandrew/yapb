# Map strategy books

Add `strategy/<map>/T/config.json` and `strategy/<map>/CT/config.json`.
The bridge reads the matching file for each decision, including the round-start call. No rebuild or service restart is needed for edits. Missing map files retain the built-in tactics. Invalid files cause a safe fallback to existing native orders; consult bridge fallback logs.

Each version-1 file contains a `strategies` array. Entries have a unique `id`, readable `name`, `rounds` (`pistol`, `buy`, `half_buy`, `save`, `other`, or `any`), `when` describing suitable circumstances, and `plan_prefixes` matching existing validated deployment choices. The bridge translates eligible entries and choices into plain-language guidance for the caller. Pistol-only entries are removed on known non-pistol rounds. Conditions guide Jev; they do not invent enemy intelligence or override bomb objectives.

The Dust2 starter books expose the existing A/B pistol rushes and staged attacks for T, and both balanced CT formations. This configuration selects supported tactics; adding new routes or actions still requires implementing and validating those actions in the framework. The engine retains the selected opening during the round; later objective and observation updates take priority.

The local silent orchestrator supplies `orchestrator_status` to both callers: team numbers, current objective, observed site strength and reports. It keeps hidden world positions separate from team decisions. Read-only diagnostics: localhost:28778/health, /world, /callers/T and /callers/CT. World diagnostics are private and omniscient.

Round categories use the native pistol flag first, then the coordinated economy mode: full/last-round maps to buy, half to half_buy, and save to save. Unknown economy exposes no economy-specific book entries; existing fallback tactics remain available. CT opening choices are restricted to applicable book entries when any match. Objective phases retain their native recovery/rotation choices.

The six Dust2 CT entries include actual validated B-split, passive pistol with a CT-spawn rotator, close Short half-buy and B eco-stack formations. The twelve T entries cover four concrete approaches: Long to A, Short to A, tunnels to B and Mid Doors to B. Economy-specific entries can share routes and supply selection guidance rather than distinct movement patterns. Flash timing, deliberate weapon-recovery/saving decisions and automatic Long-player fallback are not scripted by these descriptions.
