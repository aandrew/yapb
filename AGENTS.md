# Jev MVP development

This is a fork of yapb/yapb. `origin` is aandrew/yapb; `upstream` is yapb/yapb.
See docs/jev-mvp.md for setup, protocol, acceptance, and current limits.

- Preserve the upstream bot behaviour when yb_systemone is disabled.
- The first scenario is standard de_dust2 and exactly five live CT bots.
- Every offered and applied deployment must contain exactly three A and two B
  assignments. Use one team-level choice, not independent per-bot choices.
- Keep API requests outside the engine thread. Never send entity pointers or
  hidden enemy state. Validate round/request/nonce, roster membership, and expiry.
- bridge/.env and all runtime/log files are private. Never commit credentials or
  print environment values. Game RCON logs can contain a local control password.
- Run `node --test bridge/test.mjs` for bridge changes. Build native changes with
  tools/build-local.ps1. Use the actual local game for movement/chat acceptance;
  mocked decisions alone do not establish end-to-end success.
- Stop the local server and wait for it to exit before replacing its DLL.
- Maintain the existing C++ style and both CMake and Visual Studio source lists.
- Keep experimental server-side migration separate from the local MVP.
