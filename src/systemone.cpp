// SPDX-License-Identifier: Unlicense
#include <yapb.h>

namespace bot {
namespace {
constexpr uintptr_t kNoSocket = static_cast<uintptr_t> (-1);
#if defined(YSTL_WINDOWS)
using NativeSocket = SOCKET;
#else
using NativeSocket = int;
#endif
}

SystemOne::~SystemOne () {
  if (socket_ != kNoSocket) {
#if defined(YSTL_WINDOWS)
    closesocket (static_cast<NativeSocket> (socket_));
    WSACleanup ();
#else
    close (static_cast<NativeSocket> (socket_));
#endif
  }
}

bool SystemOne::Open () {
  if (socket_ != kNoSocket) {
    return true;
  }
#if defined(YSTL_WINDOWS)
  WSADATA data {};
  if (WSAStartup (MAKEWORD (2, 2), &data) != 0) {
    return false;
  }
#endif
  const auto handle = ::socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (handle == static_cast<NativeSocket> (-1)) {
#if defined(YSTL_WINDOWS)
    WSACleanup ();
#endif
    return false;
  }
  const auto close_failed = [&] () {
#if defined(YSTL_WINDOWS)
    closesocket (handle);
    WSACleanup ();
#else
    close (handle);
#endif
    return false;
  };
#if defined(YSTL_WINDOWS)
  u_long nonblocking = 1;
  if (ioctlsocket (handle, static_cast<long> (FIONBIO), &nonblocking) != 0) {
    return close_failed ();
  }
#else
  if (fcntl (handle, F_SETFL, O_NONBLOCK) == -1) {
    return close_failed ();
  }
#endif
  sockaddr_in bridge {};
  bridge.sin_family = AF_INET;
  bridge.sin_port = htons (28777);
  bridge.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  if (::connect (handle, reinterpret_cast<sockaddr *> (&bridge), sizeof (bridge)) != 0) {
    return close_failed ();
  }
  socket_ = static_cast<uintptr_t> (handle);
  return true;
}

void SystemOne::NewRound () {
  ++request_;
  nonce_ = ystl::rg (1, 0x3fffffff);
  pending_ = false;
  applied_ = false;
  skipped_ = false;
  round_time_ = game.Time ();
  deadline_ = 0.0f;
  next_send_ = 0.0f;
  for (int i = 0; i < 5; ++i) {
    roster_[i] = 0;
    goals_[i] = -1;
    site_a_[i] = false;
    announced_[i] = false;
  }
}

bool SystemOne::RosterMatches () const {
  int count = 0;
  for (auto &bot : bots) {
    if (game.GetPlayerTeam (bot.Ent ()) != Team::CT || !game.IsAliveEntity (bot.Ent ())) {
      continue;
    }
    ++count;
    bool found = false;
    for (const auto id : roster_) {
      found |= id == bot.index_ + 1;
    }
    if (!found) {
      return false;
    }
  }
  return count == 5;
}

void SystemOne::SendState (bool telemetry) {
  if (!Open ()) {
    return;
  }
  ystl::String payload {};
  payload.appendf ("{\"version\":1,\"event\":\"%s\",\"map\":\"de_dust2\",\"request\":%d,\"nonce\":%d,\"game_time\":%.2f,\"bomb_planted\":%s,\"sites\":{\"A\":[1152,2464,144],\"B\":[-1536,2688,48]},\"bots\":[",
    telemetry ? "telemetry" : "round_start", request_, nonce_, game.Time (), game_state.IsBombPlanted () ? "true" : "false");
  for (int i = 0; i < 5; ++i) {
    Bot *player = nullptr;
    for (auto &bot : bots) {
      if (bot.index_ + 1 == roster_[i]) {
        player = &bot;
        break;
      }
    }
    if (player == nullptr) {
      return;
    }
    const auto &p = player->pev->origin;
    payload.appendf ("%s{\"id\":%d,\"health\":%.0f,\"origin\":[%.1f,%.1f,%.1f],\"site\":\"%s\",\"goal\":%d}",
      i == 0 ? "" : ",", roster_[i], player->pev->health, p.x, p.y, p.z,
      applied_ ? (site_a_[i] ? "A" : "B") : "pending", goals_[i]);
  }
  payload.append ("]}");
#if defined(YSTL_WINDOWS)
  ::send (static_cast<NativeSocket> (socket_), payload.chars (), static_cast<int> (payload.size ()), 0);
#else
  ::send (static_cast<NativeSocket> (socket_), payload.chars (), payload.size (), 0);
#endif
}

void SystemOne::Apply (const int *a, const char *source) {
  // A model can only select a valid combination of three members of this roster.
  for (int i = 0; i < 3; ++i) {
    if (i > 0 && a[i] <= a[i - 1]) {
      return;
    }
    bool found = false;
    for (const auto id : roster_) {
      found |= id == a[i];
    }
    if (!found) {
      return;
    }
  }
  const ystl::Vector positions[5] = {
    { 1152.0f, 2464.0f, 144.0f }, { 1280.0f, 2368.0f, 144.0f }, { 1024.0f, 2368.0f, 144.0f },
    { -1536.0f, 2688.0f, 48.0f }, { -1408.0f, 2592.0f, 48.0f }
  };
  int a_slot = 0, b_slot = 3;
  for (int i = 0; i < 5; ++i) {
    site_a_[i] = roster_[i] == a[0] || roster_[i] == a[1] || roster_[i] == a[2];
    const int slot = site_a_[i] ? a_slot++ : b_slot++;
    goals_[i] = graph.GetNearest (positions[slot], 512.0f);
    if (!graph.Exists (goals_[i])) {
      game.Print ("[Jev] Cannot assign site: missing Dust2 graph nodes.");
      pending_ = false;
      skipped_ = true;
      return;
    }
  }
  pending_ = false;
  applied_ = true;
  next_chat_ = game.Time ();
  next_telemetry_ = game.Time ();
  game.Print ("[Jev] Round %d deployment source=%s: A=%d,%d,%d B=remaining two", request_, source, a[0], a[1], a[2]);
  for (auto &bot : bots) {
    if (Goal (bot.index_) != -1) {
      bot.ClearTasks ();
      bot.ClearSearchNodes ();
      bot.Task ()->data = Goal (bot.index_);
    }
  }
}

int SystemOne::Goal (int index) const {
  if (!cv_systemone || !applied_ || ystl::StringRef (game.GetMapName ()) != "de_dust2") {
    return -1;
  }
  for (int i = 0; i < 5; ++i) {
    if (roster_[i] == index + 1) {
      return goals_[i];
    }
  }
  return -1;
}

void SystemOne::Frame () {
  if (!cv_systemone || ystl::StringRef (game.GetMapName ()) != "de_dust2" || graph.Length () == 0 || game_state.IsRoundOver ()) {
    return;
  }
  // RoundStart can precede respawn. Wait briefly before capturing the live roster.
  if (!pending_ && !applied_ && !skipped_ && game.Time () >= round_time_ + 0.5f) {
    int count = 0;
    for (auto &bot : bots) {
      if (game.GetPlayerTeam (bot.Ent ()) == Team::CT && game.IsAliveEntity (bot.Ent ())) {
        if (count < 5) {
          roster_[count] = bot.index_ + 1;
        }
        ++count;
      }
    }
    if (count != 5) {
      return; // This MVP deliberately requires exactly five live CT bots.
    }
    for (int i = 0; i < 5; ++i) {
      for (int j = i + 1; j < 5; ++j) {
        if (roster_[j] < roster_[i]) {
          const int tmp = roster_[i]; roster_[i] = roster_[j]; roster_[j] = tmp;
        }
      }
    }
    pending_ = true;
    deadline_ = game.Time () + 2.0f;
    next_send_ = game.Time ();
  }
  if (pending_) {
    if (!RosterMatches ()) {
      NewRound ();
      return;
    }
    if (game.Time () >= next_send_) {
      SendState (false);
      next_send_ = game.Time () + 0.5f;
    }
    if (socket_ != kNoSocket) {
      char response[128] {};
      // Limit work per frame even when the bridge has queued several datagrams.
      for (int packet = 0; packet < 4; ++packet) {
        const auto length = ::recv (static_cast<NativeSocket> (socket_), response, sizeof (response) - 1, 0);
        if (length <= 0) {
          break;
        }
        response[length] = '\0';
        int req = 0, nonce = 0, a[3] {};
        char extra = 0;
        if (sscanf (response, "YJ1 %d %d %d %d %d %c", &req, &nonce, &a[0], &a[1], &a[2], &extra) == 5 &&
            req == request_ && nonce == nonce_ && game.Time () < deadline_) {
          Apply (a, "jev");
          if (applied_) {
            break;
          }
        }
      }
    }
    if (pending_ && game.Time () >= deadline_) {
      Apply (roster_, "fallback");
    }
  }
  if (applied_ && game.Time () >= next_chat_) {
    for (int i = 0; i < 5; ++i) {
      if (!announced_[i]) {
        for (auto &bot : bots) {
          if (bot.index_ + 1 == roster_[i]) {
            bot.SendToChat (site_a_[i] ? "Going A - defending A site." : "Going B - defending B site.", true);
            game.Print ("[Jev] CT %d says: Going %s", roster_[i], site_a_[i] ? "A" : "B");
          }
        }
        announced_[i] = true;
        next_chat_ = game.Time () + 0.25f;
        break;
      }
    }
  }
  if (applied_ && game.Time () >= next_telemetry_) {
    SendState (true);
    next_telemetry_ = game.Time () + 2.0f;
  }
}
}
