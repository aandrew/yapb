# Install the AI caller MVP on Windows

This guide installs the local Counter-Strike 1.6 5v5 AI caller framework from this fork's `master` branch. **Only Dust2 (`de_dust2`) is currently supported by the AI extension.** Creating a strategy folder for another map is not enough: it also needs authored positions, routes, callouts and native validation.

## 1. Prerequisites

- Windows and PowerShell.
- Git with submodule support.
- Python with pip, available as `python`.
- **Node.js 24**, available as `node`. The bridge uses Node's built-in SQLite support.
- A Steam Counter-Strike 1.6 / HLDS installation. The source game directory must contain **`hlds.exe`** and a **`cstrike`** folder with the game assets.
- Internet access and **your own OpenRouter account and API key**, with access/credit for `perplexity/pplx-decider-v1.2`. Model requests use and are billed to your account.

Check the command-line tools:

```powershell
git --version
python --version
python -m pip --version
node --version
```

The supplied setup downloads portable CMake, Ninja and Zig. Visual Studio and a local AI GPU are not required.

## 2. Download the fork and build tools

From a folder where you want to keep the project:

```powershell
git clone --recursive --branch master https://github.com/aandrew/yapb.git
cd yapb
git submodule update --init --recursive
.\tools\setup-local.ps1
```

Run the remaining commands from this `yapb` repository directory. Setup stores portable tools and the Dust2 graph in a sibling `work` folder. It also creates `bridge\.env` from the blank example if the file is missing; it does not provide an API key.

## 3. Add your own OpenRouter key

Sign in to [OpenRouter's API key settings](https://openrouter.ai/settings/keys) and create your own key. Open the local environment file:

```powershell
notepad .\bridge\.env
```

Replace the empty value with your key and save the file:

```dotenv
OPENROUTER_API_KEY=your-own-openrouter-api-key
```

The value above is a placeholder. Replace it with the actual key from your account. Keep the filename exactly **`.env`**, not `.env.txt`. Leave any existing `RCON_PASSWORD` line intact; local runtime preparation generates that administrative password automatically.

**The key belongs in `bridge/.env`, not `bridge/.env.example`, the README, a strategy file or source code.** The real environment file and runtime logs/database are Git-ignored. Each user supplies their own credentials; none are included in the repository. Do not share the environment file or credentials in screenshots or logs.

You can check that the variable is loaded without displaying its value:

```powershell
node --env-file=bridge/.env -e "console.log({openrouterKeyConfigured: Boolean(process.env.OPENROUTER_API_KEY?.trim())})"
```

Then test authentication and a real model response:

```powershell
node --env-file=bridge/.env bridge/smoke.mjs
```

A successful check prints a `live-smoke` result and a selected plan. The presence check alone does not validate a key. The runtime model is configured in `bridge/decision.mjs`; users do not need to change it to install the current build.

## 4. Build and start the local server

```powershell
.\tools\build-local.ps1
.\tools\start-local.ps1
```

The default source installation is:

```text
C:\Program Files (x86)\Steam\steamapps\common\Half-Life
```

If your installation is elsewhere, supply the directory that contains both `hlds.exe` and the `cstrike` assets:

```powershell
.\tools\start-local.ps1 -SteamHalfLife 'D:\SteamLibrary\steamapps\common\Half-Life'
```

The first start copies those files into the isolated sibling `work\runtime\Half-Life` directory, installs the built DLL there, creates runtime configuration, starts the bridge/server and seeds the teams. It leaves the original Steam installation unchanged. Once the runtime exists, later starts reuse it; `-SteamHalfLife` supplies the source for the initial copy.

Wait for the `Ready: connect to 127.0.0.1:27016` message. The server starts Dust2 with five roster places per team. Decisions run through the local Node bridge every five seconds per team.

## 5. Join and check the callers

Start Counter-Strike 1.6 and enter this in its game console:

```text
connect 127.0.0.1:27016
```

Join CT or T. A human replaces a roster bot, and the team caller gives role/objective advice in team chat. Look for the opening strategy announcement and subsequent contact, sound, rotation and bomb-action reports.

Read-only diagnostics are available while the bridge is running:

- [Health and database counts](http://127.0.0.1:28778/health).
- [Recent decisions](http://127.0.0.1:28778/decisions?limit=50).
- [CT caller state](http://127.0.0.1:28778/callers/CT) and [T caller state](http://127.0.0.1:28778/callers/T).

Bot movement or the server's Ready message does not by itself prove the model is working: local fallback orders also keep bots playing. Check the smoke result and recorded decisions if callers appear unresponsive.

## 6. Stop, update or change your key

Stop before rebuilding or replacing the DLL:

```powershell
.\tools\stop-local.ps1
```

For a normal update with no conflicting local source changes:

```powershell
git pull --ff-only
git submodule update --init --recursive
.\tools\build-local.ps1
.\tools\start-local.ps1
```

To change the API key, stop the server/bridge, edit `bridge\.env`, then start again. A running bridge does not reload environment variables from disk automatically. Editable strategy books do reload on each decision; see the [strategy guide](../strategy/README.md).

## Troubleshooting and data locations

| Symptom | Check |
|---|---|
| `node:sqlite` is unavailable | Use Node.js 24. |
| Game/HLDS installation not found | Confirm the source directory contains `hlds.exe` and `cstrike`; use `-SteamHalfLife` on the initial start. |
| Build cannot find headers/dependencies | Initialise Git submodules and run the portable-tool setup. |
| Missing/invalid API key or provider access error | Edit `bridge/.env`, check account access/credit and run the smoke test. Restart after editing the key. |
| Server is already running / DLL cannot be replaced | Run `stop-local.ps1` before building or starting again. |
| Bridge/server fails to start | Inspect sibling `work/logs/bridge.stderr.log` and the HLDS logs. Check local ports 27016 (game), 28777 (bridge UDP) and 28778 (diagnostics). |
| Another map lacks caller behavior | The AI extension currently supports Dust2 only. |

Private decision data lives in `bridge/logs/decisions.sqlite` and `bridge/logs/decisions.jsonl`. The isolated game runtime and process logs live under the sibling `work` folder. Game logs may contain the local administrative password; redact credentials before sharing diagnostics. Callers themselves use player actions and team chat, not RCON.

See the [feature summary and current playtest limits](../README.md) and [decision reference](jev-decisions.md) before assessing strategy or defuse behavior.
