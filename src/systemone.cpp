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

namespace {
bool ValidToken (const char *value) {
  if (*value == '\0') {
    return false;
  }
  for (const char *ch = value; *ch != '\0'; ++ch) {
    if (!((*ch >= 'a' && *ch <= 'z') || (*ch >= '0' && *ch <= '9') || *ch == '_')) {
      return false;
    }
  }
  return true;
}
bool ValidRole (ystl::StringRef role) {
  return role == "awper" || role == "lurker" || role == "entry" || role == "support";
}
}

bool SystemOne::LoadConfig () {
  config_attempted_ = true;
  position_count_ = 0;
  ystl::MemFile file (ystl::strings.join_path (bstor.GetRunningPathVfs (), "conf", "systemone.cfg"));
  ystl::String line {};
  int identity_count = 0;
  bool correct_map = false;
  while (file.get_line (line)) {
    line.trim ();
    if (line.empty () || line.starts_with ("#")) {
      continue;
    }
    char map[32] {};
    if (line.starts_with ("map ")) {
      if (sscanf (line.chars (), "map %31s", map) != 1 || !ValidToken (map) || ystl::StringRef (map) != game.GetMapName ()) {
        return false;
      }
      correct_map = true;
    }
    else if (line.starts_with ("bot ")) {
      if (identity_count >= 5) {
        return false;
      }
      auto &identity = identities_[identity_count];
      char id[32] {}, name[32] {}, role[16] {};
      if (sscanf (line.chars (), "bot %31s %31s %15s", id, name, role) != 3 || !ValidToken (id) || !ValidRole (role)) {
        return false;
      }
      for (int i = 0; i < identity_count; ++i) {
        if (ystl::StringRef (identities_[i].id) == id || ystl::StringRef (identities_[i].name) == name) {
          return false;
        }
      }
      // A changed identity definition invalidates its previous entity binding.
      if (ystl::StringRef (identity.id) != id || ystl::StringRef (identity.name) != name) {
        identity.slot = identity.userid = identity.generation = 0;
        ++roster_revision_;
      }
      memcpy (identity.id, id, sizeof (id));
      memcpy (identity.name, name, sizeof (name));
      memcpy (identity.role, role, sizeof (role));
      ++identity_count;
    }
    else if (line.starts_with ("position ")) {
      if (position_count_ >= 32) {
        return false;
      }
      auto &position = positions_[position_count_];
      char stance[8] {};
      if (sscanf (line.chars (), "position %31s %c %f %f %f %f %f %f %f %7s %63s",
            position.id, &position.site, &position.origin.x, &position.origin.y, &position.origin.z,
            &position.look_at.x, &position.look_at.y, &position.look_at.z, &position.radius, stance, position.preferred_roles) != 11 ||
          !ValidToken (position.id) || (position.site != 'A' && position.site != 'B') ||
          !(position.radius >= 8.0f && position.radius <= 96.0f) ||
          !(ystl::abs (position.origin.x) < 100000.0f && ystl::abs (position.origin.y) < 100000.0f && ystl::abs (position.origin.z) < 100000.0f) ||
          !(ystl::abs (position.look_at.x) < 100000.0f && ystl::abs (position.look_at.y) < 100000.0f && ystl::abs (position.look_at.z) < 100000.0f) ||
          (ystl::StringRef (stance) != "stand" && ystl::StringRef (stance) != "crouch")) {
        return false;
      }
      position.crouch = ystl::StringRef (stance) == "crouch";
      for (const auto &role : ystl::String (position.preferred_roles).split (",")) {
        if (!ValidRole (role)) {
          return false;
        }
      }
      position.waypoint = graph.GetNearest (position.origin, 256.0f);
      if (!graph.Exists (position.waypoint)) {
        return false;
      }
      for (int i = 0; i < position_count_; ++i) {
        if (ystl::StringRef (positions_[i].id) == position.id || positions_[i].waypoint == position.waypoint) {
          return false;
        }
      }
      ++position_count_;
    }
    else {
      return false;
    }
  }
  int a = 0, b = 0;
  for (int i = 0; i < position_count_; ++i) {
    positions_[i].site == 'A' ? ++a : ++b;
  }
  config_loaded_ = correct_map && identity_count == 5 && a >= 3 && b >= 2;
  if (config_loaded_) {
    game.Print ("[Jev] Loaded %d named positions and five stable role identities.", position_count_);
  }
  return config_loaded_;
}

void SystemOne::NewRound () {
  ++round_;
  nonce_ = ystl::rg (1, 0x3fffffff);
  pending_ = applied_ = config_loaded_ = config_attempted_ = false;
  pending_state_.clear ();
  round_time_ = game.Time ();
  next_poll_ = round_time_ + 0.5f;
  next_telemetry_ = next_chat_ = 0.0f;
  for (auto &identity : identities_) {
    identity.position = -1;
    identity.announced = false;
  }
}

Bot *SystemOne::Player (int identity) const {
  const auto &binding = identities_[identity];
  for (auto &bot : bots) {
    if (bot.Entindex () == binding.slot && game.GetPlayerTeam (bot.Ent ()) == Team::CT &&
        bot.Ent ()->serialnumber == binding.generation && engfuncs.pfnGetPlayerUserId (bot.Ent ()) == binding.userid &&
        bot.pev->netname.str () == ystl::StringRef (binding.name)) {
      return &bot;
    }
  }
  return nullptr;
}

int SystemOne::IdentityFor (int index) const {
  for (int i = 0; i < 5; ++i) {
    if (identities_[i].slot == index + 1 && Player (i) != nullptr) {
      return i;
    }
  }
  return -1;
}

bool SystemOne::SyncRoster () {
  bool changed = false;
  for (int i = 0; i < 5; ++i) {
    auto &identity = identities_[i];
    int slot = 0, userid = 0, generation = 0;
    for (auto &bot : bots) {
      if (game.GetPlayerTeam (bot.Ent ()) == Team::CT && bot.pev->netname.str () == ystl::StringRef (identity.name)) {
        slot = bot.Entindex ();
        userid = engfuncs.pfnGetPlayerUserId (bot.Ent ());
        generation = bot.Ent ()->serialnumber;
        break;
      }
    }
    if (slot != identity.slot || userid != identity.userid || generation != identity.generation) {
      identity.slot = slot;
      identity.userid = userid;
      identity.generation = generation;
      changed = true;
      game.Print ("[Jev] Binding %s role=%s slot=%d userid=%d generation=%d", identity.id, identity.role, slot, userid, generation);
    }
  }
  if (changed) {
    ++roster_revision_;
    pending_ = false; // Discard in-flight orders for the previous entity bindings.
    pending_state_.clear ();
    next_poll_ = game.Time () + 0.5f;
  }
  return changed;
}

ystl::String SystemOne::BuildState (bool telemetry) {
  ystl::String payload {};
  payload.appendf ("{\"version\":2,\"event\":\"%s\",\"map\":\"%s\",\"round\":%d,\"request\":%d,\"nonce\":%d,\"roster_revision\":%d,\"game_time\":%.2f,\"time_left\":%.2f,\"poll_interval\":%.1f,\"bomb_planted\":%s,\"positions\":[",
    telemetry ? "telemetry" : (pending_opening_ ? "round_start" : "poll"), game.GetMapName (), round_, request_, nonce_, roster_revision_,
    game.Time (), game_state.GetRoundTimeLeft (), cv_systemone_poll_interval.As<float> (), game_state.IsBombPlanted () ? "true" : "false");
  for (int i = 0; i < position_count_; ++i) {
    const auto &p = positions_[i];
    payload.appendf ("%s{\"id\":\"%s\",\"site\":\"%c\",\"origin\":[%.1f,%.1f,%.1f],\"look_at\":[%.1f,%.1f,%.1f],\"radius\":%.1f,\"stance\":\"%s\",\"preferred_roles\":\"%s\",\"waypoint\":%d}",
      i == 0 ? "" : ",", p.id, p.site, p.origin.x, p.origin.y, p.origin.z, p.look_at.x, p.look_at.y, p.look_at.z,
      p.radius, p.crouch ? "crouch" : "stand", p.preferred_roles, p.waypoint);
  }
  payload.append ("],\"bots\":[");
  for (int i = 0; i < 5; ++i) {
    const auto &identity = identities_[i];
    auto *player = Player (i);
    const ystl::Vector origin = player != nullptr ? player->pev->origin : ystl::Vector {};
    const bool alive = player != nullptr && game.IsAliveEntity (player->Ent ());
    const bool assigned = identity.position >= 0 && identity.position < position_count_;
    payload.appendf ("%s{\"id\":\"%s\",\"role\":\"%s\",\"slot\":%d,\"userid\":%d,\"generation\":%d,\"connected\":%s,\"alive\":%s,\"health\":%.0f,\"money\":%d,\"weapon\":%d,\"origin\":[%.1f,%.1f,%.1f],\"position\":\"%s\",\"site\":\"%c\",\"goal\":%d}",
      i == 0 ? "" : ",", identity.id, identity.role, identity.slot, identity.userid, identity.generation,
      player != nullptr ? "true" : "false", alive ? "true" : "false", player != nullptr ? player->pev->health : 0.0f,
      player != nullptr ? player->money_amount_ : 0, player != nullptr ? static_cast<int> (player->current_weapon_) : 0,
      origin.x, origin.y, origin.z, assigned ? positions_[identity.position].id : "pending",
      assigned ? positions_[identity.position].site : '-', assigned ? positions_[identity.position].waypoint : -1);
  }
  payload.append ("],\"observed_contacts\":[");
  edict_t *enemy = nullptr;
  ystl::Vector contact {};
  if (bots.GetTeamContact (Team::CT, enemy, contact, 8.0f)) {
    // Only the team's recent observation is shared, never the entity's live coordinates.
    payload.appendf ("{\"origin\":[%.1f,%.1f,%.1f],\"source\":\"recent_team_observation\"}", contact.x, contact.y, contact.z);
  }
  payload.append ("]}");
  return payload;
}

void SystemOne::Send (ystl::StringRef payload) {
  if (!Open ()) {
    return;
  }
#if defined(YSTL_WINDOWS)
  ::send (static_cast<NativeSocket> (socket_), payload.chars (), static_cast<int> (payload.size ()), 0);
#else
  ::send (static_cast<NativeSocket> (socket_), payload.chars (), payload.size (), 0);
#endif
}

void SystemOne::BeginRequest () {
  ++request_;
  nonce_ = ystl::rg (1, 0x3fffffff);
  pending_opening_ = !applied_;
  pending_ = true;
  deadline_ = game.Time () + 2.0f;
  next_send_ = game.Time ();
  next_poll_ = game.Time () + cv_systemone_poll_interval.As<float> ();
  pending_state_ = BuildState (false); // Retries reuse the same immutable snapshot.
  game.Print ("[Jev] Request round=%d request=%d nonce=%d revision=%d event=%s", round_, request_, nonce_, roster_revision_, pending_opening_ ? "round_start" : "poll");
}

bool SystemOne::Apply (const int *targets, const char *source) {
  int a = 0, b = 0;
  for (int i = 0; i < 5; ++i) {
    if (targets[i] < 0 || targets[i] >= position_count_ || !graph.Exists (positions_[targets[i]].waypoint)) {
      return false;
    }
    positions_[targets[i]].site == 'A' ? ++a : ++b;
    for (int j = 0; j < i; ++j) {
      if (targets[i] == targets[j]) {
        return false;
      }
    }
  }
  if (a != 3 || b != 2) {
    return false;
  }
  const bool opening = !applied_;
  pending_ = false;
  applied_ = true;
  for (int i = 0; i < 5; ++i) {
    auto &identity = identities_[i];
    const bool changed = identity.position != targets[i];
    identity.position = targets[i];
    game.Print ("[Jev] Order round=%d request=%d source=%s identity=%s role=%s position=%s site=%c changed=%d",
      round_, request_, source, identity.id, identity.role, positions_[targets[i]].id, positions_[targets[i]].site, changed ? 1 : 0);
    if (auto *player = Player (i); player != nullptr && changed) {
      // Do not erase a current combat reaction for a mid-round placement update.
      if (opening || player->GetTaskId () == TaskId::Normal) {
        player->ClearTasks ();
        player->ClearSearchNodes ();
        player->Task ()->data = positions_[targets[i]].waypoint;
      }
    }
  }
  if (opening) {
    next_chat_ = game.Time ();
  }
  next_telemetry_ = game.Time ();
  return true;
}

void SystemOne::ApplyFallback () {
  pending_ = false;
  if (applied_) {
    game.Print ("[Jev] Request %d timed out; retaining last valid named orders.", request_);
    return;
  }
  int targets[5] {}, a = 0, b = 0;
  for (int i = 0; i < 5; ++i) {
    targets[i] = -1;
    for (int p = 0; p < position_count_; ++p) {
      if ((i < 3 ? positions_[p].site == 'A' : positions_[p].site == 'B')) {
        const int ordinal = i < 3 ? a : b;
        int seen = 0;
        for (int q = 0; q < p; ++q) {
          if (positions_[q].site == positions_[p].site) {
            ++seen;
          }
        }
        if (seen == ordinal) {
          targets[i] = p;
          break;
        }
      }
    }
    i < 3 ? ++a : ++b;
  }
  Apply (targets, "fallback");
}

int SystemOne::Goal (int index) const {
  if (!cv_systemone || !config_loaded_ || !applied_) {
    return -1;
  }
  const int identity = IdentityFor (index);
  if (identity == -1 || identities_[identity].position < 0) {
    return -1;
  }
  return positions_[identities_[identity].position].waypoint;
}

bool SystemOne::HoldPosition (Bot *bot) const {
  if (Goal (bot->index_) == -1) {
    return false;
  }
  const auto &position = positions_[identities_[IdentityFor (bot->index_)].position];
  const auto &destination = graph[position.waypoint].origin;
  if (bot->pev->origin.distance_sq (destination) > ystl::sqrf (position.radius) || !util.IsVisible (destination, bot->Ent ())) {
    return false;
  }
  bot->move_to_goal_ = bot->check_terrain_ = false;
  bot->move_speed_ = bot->strafe_speed_ = 0.0f;
  bot->look_at_safe_ = position.look_at;
  bot->aim_flags_ |= AimFlags::Camp;
  if (position.crouch) {
    bot->pev->button |= IN_DUCK;
  }
  return true;
}

void SystemOne::PrintStatus () const {
  ctrl.Msg ("Jev: round=%d request=%d revision=%d pending=%d poll=%.1fs", round_, request_, roster_revision_, pending_ ? 1 : 0, cv_systemone_poll_interval.As<float> ());
  for (const auto &identity : identities_) {
    ctrl.Msg ("%s name=%s role=%s slot=%d userid=%d position=%s", identity.id, identity.name, identity.role,
      identity.slot, identity.userid, identity.position >= 0 ? positions_[identity.position].id : "pending");
  }
}

bool SystemOne::AssignNamed (ystl::StringRef identity, ystl::StringRef position) {
  if (!cv_systemone || !applied_ || !config_loaded_) {
    return false;
  }
  int member = -1, target = -1;
  int targets[5] {};
  for (int i = 0; i < 5; ++i) {
    targets[i] = identities_[i].position;
    if (ystl::StringRef (identities_[i].id) == identity) {
      member = i;
    }
  }
  for (int i = 0; i < position_count_; ++i) {
    if (ystl::StringRef (positions_[i].id) == position) {
      target = i;
    }
  }
  if (member == -1 || target == -1) {
    return false;
  }
  // Exchange occupied spots to preserve the team split and unique reservations.
  for (int i = 0; i < 5; ++i) {
    if (i != member && targets[i] == target) {
      targets[i] = targets[member];
    }
  }
  targets[member] = target;
  if (!Apply (targets, "operator")) {
    return false;
  }
  next_poll_ = game.Time () + cv_systemone_poll_interval.As<float> ();
  return true;
}

void SystemOne::Frame () {
  if (!cv_systemone || graph.Length () == 0 || game_state.IsRoundOver ()) {
    return;
  }
  if (!config_attempted_ && !LoadConfig ()) {
    game.Print ("[Jev] Invalid/missing named-position configuration for this map; normal bot behaviour retained.");
  }
  if (!config_loaded_) {
    return;
  }
  SyncRoster ();
  bool any_connected = false;
  bool opening_ready = true;
  for (int i = 0; i < 5; ++i) {
    auto *player = Player (i);
    any_connected |= player != nullptr;
    opening_ready &= player != nullptr && game.IsAliveEntity (player->Ent ());
  }
  if (!pending_ && game.Time () >= next_poll_ && game.Time () >= round_time_ + 0.5f &&
      any_connected && (applied_ || opening_ready)) {
    BeginRequest ();
  }
  if (pending_) {
    if (game.Time () >= next_send_) {
      Send (pending_state_);
      next_send_ = game.Time () + 0.5f;
    }
    if (socket_ != kNoSocket) {
      char response[256] {};
      for (int packet = 0; packet < 4; ++packet) {
        const auto length = ::recv (static_cast<NativeSocket> (socket_), response, sizeof (response) - 1, 0);
        if (length <= 0) {
          break;
        }
        response[length] = '\0';
        int round = 0, request = 0, nonce = 0, revision = 0, targets[5] {};
        char extra = 0;
        if (sscanf (response, "YJ2 %d %d %d %d %d %d %d %d %d %c", &round, &request, &nonce, &revision,
              &targets[0], &targets[1], &targets[2], &targets[3], &targets[4], &extra) == 9 &&
            round == round_ && request == request_ && nonce == nonce_ && revision == roster_revision_ && game.Time () < deadline_) {
          if (Apply (targets, "jev")) {
            break;
          }
        }
      }
    }
    if (pending_ && game.Time () >= deadline_) {
      ApplyFallback ();
    }
  }
  if (applied_ && game.Time () >= next_chat_) {
    for (int i = 0; i < 5; ++i) {
      auto &identity = identities_[i];
      if (!identity.announced && Player (i) != nullptr) {
        const auto &position = positions_[identity.position];
        const auto message = ystl::strings.format ("%s: %s, going %c - %s.", identity.id, identity.role, position.site, position.id);
        Player (i)->SendToChat (message, true);
        game.Print ("[Jev] Chat round=%d identity=%s role=%s site=%c position=%s", round_, identity.id, identity.role, position.site, position.id);
        identity.announced = true;
        next_chat_ = game.Time () + 0.25f;
        break;
      }
    }
  }
  if (applied_ && game.Time () >= next_telemetry_) {
    Send (BuildState (true));
    next_telemetry_ = game.Time () + 2.0f;
  }
}
}
