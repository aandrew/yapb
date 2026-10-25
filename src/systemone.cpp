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
  ystl::MemFile file (ystl::strings.join_path (bstor.GetRunningPathVfs (), "conf", team_ == Team::CT ? "systemone.cfg" : "systemone_t.cfg"));
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
      int difficulty = 2, personality = 0, caller = 0;
      if (sscanf (line.chars (), "bot %31s %31s %15s %d %d %d", id, name, role, &difficulty, &personality, &caller) != 6 || !ValidToken (id) || !ValidRole (role) || difficulty < 0 || difficulty > 4 || personality < 0 || personality > 2) {
        return false;
      }
      for (int i = 0; i < identity_count; ++i) {
        if (ystl::StringRef (identities_[i].id) == id || ystl::StringRef (identities_[i].name) == name) {
          return false;
        }
      }
      // A changed identity definition invalidates its previous entity binding.
      if (ystl::StringRef (identity.bot_id) != id || ystl::StringRef (identity.name) != name) {
        identity.slot = identity.userid = identity.generation = 0;
        ++roster_revision_;
      }
      memcpy (identity.bot_id, id, sizeof (id));
      if (!identity.human) { memcpy (identity.id, id, sizeof (id)); }
      identity.difficulty = difficulty; identity.personality = personality; identity.preferred_caller = caller != 0;
      memcpy (identity.name, name, sizeof (name));
      memcpy (identity.role, role, sizeof (role));
      ++identity_count;
    }
    else if (line.starts_with ("position ")) {
      if (position_count_ >= 64) {
        return false;
      }
      auto &position = positions_[position_count_];
      char stance[8] {};
      if (sscanf (line.chars (), "position %31s %c %c %d %f %f %f %f %f %f %f %7s %63s",
            position.id, &position.site, &position.zone, &position.stage, &position.origin.x, &position.origin.y, &position.origin.z,
            &position.look_at.x, &position.look_at.y, &position.look_at.z, &position.radius, stance, position.preferred_roles) != 13 ||
          !ValidToken (position.id) || (position.site != 'A' && position.site != 'B' && position.site != 'M') ||
          (position.zone != 'A' && position.zone != 'B' && position.zone != 'M' && position.zone != 'S' && position.zone != 'L' && position.zone != 'X' && position.zone != 'Y') ||
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
        if (ystl::StringRef (positions_[i].id) == position.id) {
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
    if (positions_[i].zone == 'A') { ++a; }
    if (positions_[i].zone == 'B') { ++b; }
  }
  config_loaded_ = correct_map && identity_count == 5 && a >= 5 && b >= 5;
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
  next_telemetry_ = next_chat_ = next_roster_maintain_ = 0.0f;
  t_compromised_ = fake_chosen_ = false; fake_member_ = -1; last_caller_ = -2;
  t_locked_ = false; attack_delay_ = -1; t_stage_ = 0; t_target_ = '-'; attack_time_ = stage_ready_time_ = 0.0f;
  event_count_ = ct_deaths_ = t_deaths_ = event_revision_ = 0;
  bomb_seen_time_ = -100.0f;
  bomb_seen_site_ = '-';
  required_ = ordered_ = {};
  formation_ = 0;
  defuser_ = -1;
  for (auto &sighting : sightings_) { sighting = {}; }
  for (auto &event : events_) { event = {}; }
  for (auto &identity : identities_) {
    identity.position = -1;
    identity.announced = false;
  }
}

edict_t *SystemOne::Entity (int identity) const {
  const auto &binding = identities_[identity];
  if (binding.slot <= 0) { return nullptr; }
  auto *entity = game.EntityOfIndex (binding.slot);
  if (game.IsNullEntity (entity) || game.GetPlayerTeam (entity) != team_ || entity->serialnumber != binding.generation ||
      engfuncs.pfnGetPlayerUserId (entity) != binding.userid) { return nullptr; }
  auto *bot = bots[entity];
  if (binding.human) { return bot == nullptr && !(entity->v.flags & FL_FAKECLIENT) ? entity : nullptr; }
  return bot != nullptr && !bot->is_stale_ && entity->v.netname.str () == ystl::StringRef (binding.name) ? entity : nullptr;
}

Bot *SystemOne::Player (int identity) const {
  auto *entity = Entity (identity);
  return entity != nullptr && !identities_[identity].human ? bots[entity] : nullptr;
}

int SystemOne::IdentityFor (int index) const {
  for (int i = 0; i < 5; ++i) {
    if (identities_[i].slot == index + 1 && Entity (i) != nullptr) { return i; }
  }
  return -1;
}

bool SystemOne::SyncRoster () {
  edict_t *humans[5] {}; int human_count = 0;
  for (int slot = 1; slot <= game.MaxClients () && human_count < 5; ++slot) {
    auto *entity = game.EntityOfIndex (slot);
    if (!game.IsNullEntity (entity) && (entity->v.flags & FL_CLIENT) && !(entity->v.flags & FL_FAKECLIENT) &&
        bots[entity] == nullptr && game.GetPlayerTeam (entity) == team_) { humans[human_count++] = entity; }
  }
  bool changed = false;
  for (int i = 0; i < 5; ++i) {
    auto &identity = identities_[i];
    edict_t *entity = nullptr;
    const bool human = i >= 5 - human_count;
    if (human) { entity = humans[4 - i]; }
    else {
      for (auto &bot : bots) {
        if (!bot.is_stale_ && game.GetPlayerTeam (bot.Ent ()) == team_ && bot.pev->netname.str () == ystl::StringRef (identity.name)) {
          entity = bot.Ent (); break;
        }
      }
    }
    const int slot = entity != nullptr ? game.IndexOfEntity (entity) : 0;
    const int userid = entity != nullptr ? engfuncs.pfnGetPlayerUserId (entity) : 0;
    const int generation = entity != nullptr ? entity->serialnumber : 0;
    if (slot != identity.slot || userid != identity.userid || generation != identity.generation || human != identity.human) {
      identity.slot = slot; identity.userid = userid; identity.generation = generation; identity.human = human;
      identity.announced = false;
      if (human) { snprintf (identity.id, sizeof (identity.id), "%s_human_%d", team_ == Team::CT ? "ct" : "t", userid); }
      else { memcpy (identity.id, identity.bot_id, sizeof (identity.id)); }
      changed = true;
      game.Print ("[Jev] Binding %s role=%s human=%d slot=%d userid=%d generation=%d", identity.id, identity.role, human ? 1 : 0, slot, userid, generation);
    }
  }
  if (changed) { ++roster_revision_; pending_ = false; pending_state_.clear (); next_poll_ = game.Time () + 0.5f; }
  return changed;
}

void SystemOne::MaintainRoster () {
  if (game.Time () < next_roster_maintain_) { return; }
  next_roster_maintain_ = game.Time () + 1.0f;
  for (int i = 0; i < 5; ++i) {
    auto &identity = identities_[i]; bool found = false;
    for (auto &bot : bots) {
      if (bot.pev->netname.str () != ystl::StringRef (identity.name)) { continue; }
      found = true;
      if (identity.human && !bot.is_stale_) { bot.Kick (true); }
      break;
    }
    if (!identity.human && !found && !bots.HasPendingBot (identity.name)) {
      bots.Addbot (identity.name, static_cast<Difficulty> (identity.difficulty), static_cast<Personality> (identity.personality), team_, Skin::Random, false);
    }
  }
}

int SystemOne::Caller () const {
  int fallback = -1;
  for (int i = 0; i < 5; ++i) {
    auto *player = Player (i);
    if (player == nullptr || !game.IsAliveEntity (player->Ent ())) { continue; }
    if (identities_[i].preferred_caller) { return i; }
    if (fallback == -1) { fallback = i; }
  }
  return fallback;
}

int SystemOne::FakeMember () const {
  if (fake_member_ < 0) { return -1; }
  auto *player = Player (fake_member_);
  return player != nullptr && game.IsAliveEntity (player->Ent ()) && !player->has_c4_ ? fake_member_ : -1;
}

int SystemOne::FakePosition (char site) const {
  const char *name = site == 'A' ? "t_b_1_entry" : "t_a_1_entry";
  for (int i = 0; i < position_count_; ++i) { if (ystl::StringRef (positions_[i].id) == name) { return i; } }
  return -1;
}

const char *SystemOne::PositionLabel (int position) const {
  if (position < 0 || position >= position_count_) { return "team ready point"; }
  const auto &p = positions_[position];
  const auto id = ystl::StringRef (p.id);
  if (id.starts_with ("t_a_")) {
    if (id.ends_with ("lurker")) { return p.stage == 0 ? "Top Mid" : (p.stage == 1 ? "Catwalk" : "A Short"); }
    if (p.stage == 0) { return "Outside Long"; }
    if (p.stage == 1) { return id.ends_with ("entry") ? "Long Doors" : "Outside Long"; }
    return id.ends_with ("entry") ? "Long Corner" : "Long Blue Box";
  }
  if (id.starts_with ("t_b_")) {
    if (id.ends_with ("lurker")) { return p.stage == 0 ? "Top Mid" : (p.stage == 1 ? "Lower Tunnels" : "B Doors"); }
    return p.stage == 0 ? "Outside Tunnels" : (p.stage == 1 ? "Upper Tunnels" : "B Tunnel Exit");
  }
  return p.site == 'A' ? "A Site" : (p.site == 'B' ? "B Site" : "Mid");
}

void SystemOne::TeamChat (ystl::StringRef message) {
  const int caller = Caller ();
  if (caller != -1) { Player (caller)->SendToChat (message, true); return; }
  // An all-human team receives a server advisory; no human commands are issued.
  for (const auto &client : clients) {
    if (client.IsUsed () && game.GetPlayerTeam (client.ent) == team_) {
      MessageWriter (MSG_ONE, msgs.Id (NetMsg::SayText), nullptr, client.ent).WriteByte (0).WriteString (message.chars ());
    }
  }
}

ystl::String SystemOne::BuildState (bool telemetry) {
  ystl::String payload {};
  payload.appendf ("{\"version\":3,\"team\":\"%s\",\"event\":\"%s\",\"map\":\"%s\",\"round\":%d,\"request\":%d,\"nonce\":%d,\"roster_revision\":%d,\"game_time\":%.2f,\"time_left\":%.2f,\"poll_interval\":%.1f,\"bomb_planted\":%s,\"positions\":[",
    team_ == Team::CT ? "CT" : "T", telemetry ? "telemetry" : (pending_opening_ ? "round_start" : "poll"), game.GetMapName (), round_, request_, nonce_, roster_revision_,
    game.Time (), game_state.GetRoundTimeLeft (), cv_systemone_poll_interval.As<float> (), game_state.IsBombPlanted () ? "true" : "false");
  for (int i = 0; i < position_count_; ++i) {
    const auto &p = positions_[i];
    payload.appendf ("%s{\"id\":\"%s\",\"site\":\"%c\",\"zone\":\"%c\",\"stage\":%d,\"origin\":[%.1f,%.1f,%.1f],\"look_at\":[%.1f,%.1f,%.1f],\"radius\":%.1f,\"stance\":\"%s\",\"preferred_roles\":\"%s\",\"waypoint\":%d}",
      i == 0 ? "" : ",", p.id, p.site, p.zone, p.stage, p.origin.x, p.origin.y, p.origin.z, p.look_at.x, p.look_at.y, p.look_at.z,
      p.radius, p.crouch ? "crouch" : "stand", p.preferred_roles, p.waypoint);
  }
  payload.append ("],\"bots\":[");
  for (int i = 0; i < 5; ++i) {
    const auto &identity = identities_[i];
    auto *player = Player (i);
    auto *entity = Entity (i);
    const ystl::Vector origin = entity != nullptr ? entity->v.origin : ystl::Vector {};
    const bool alive = entity != nullptr && game.IsAliveEntity (entity);
    const bool assigned = identity.position >= 0 && identity.position < position_count_;
    payload.appendf ("%s{\"id\":\"%s\",\"role\":\"%s\",\"slot\":%d,\"userid\":%d,\"generation\":%d,\"human\":%s,\"caller\":%s,\"connected\":%s,\"alive\":%s,\"health\":%.0f,\"money\":%d,\"weapon\":%d,\"origin\":[%.1f,%.1f,%.1f],\"position\":\"%s\",\"site\":\"%c\",\"goal\":%d,\"rotating\":%s}",
      i == 0 ? "" : ",", identity.id, identity.role, identity.slot, identity.userid, identity.generation,
      identity.human ? "true" : "false", Caller () == i ? "true" : "false", entity != nullptr ? "true" : "false", alive ? "true" : "false", entity != nullptr ? entity->v.health : 0.0f,
      player != nullptr ? player->money_amount_ : 0, player != nullptr ? static_cast<int> (player->current_weapon_) : 0,
      origin.x, origin.y, origin.z, assigned ? positions_[identity.position].id : "pending",
      assigned ? positions_[identity.position].site : '-', assigned ? positions_[identity.position].waypoint : -1,
      alive && assigned && origin.distance_sq (graph[positions_[identity.position].waypoint].origin) > ystl::sqrf (96.0f) ? "true" : "false");
  }
  payload.append ("],\"observed_contacts\":[");
  bool first = true;
  for (const auto &contact : sightings_) {
    if (!contact.active || game.Time () - contact.time > 8.0f) { continue; }
    payload.appendf ("%s{\"enemy_id\":%d,\"time\":%.2f,\"age\":%.2f,\"site\":\"%c\",\"origin\":[%.1f,%.1f,%.1f],\"source\":\"visible_team_sighting\"}",
      first ? "" : ",", contact.userid, contact.time, game.Time () - contact.time, contact.site,
      contact.origin.x, contact.origin.y, contact.origin.z);
    first = false;
  }
  const auto evidence = Evidence ();
  int ct_alive = 0, t_alive = 0;
  for (int slot = 1; slot <= game.MaxClients (); ++slot) {
    auto *entity = game.EntityOfIndex (slot);
    if (game.IsNullEntity (entity) || !game.IsAliveEntity (entity)) { continue; }
    const auto team = game.GetPlayerTeam (entity);
    if (team == Team::CT) { ++ct_alive; }
    if (team == Team::Terrorist) { ++t_alive; }
  }
  payload.appendf ("],\"round_state\":{\"phase\":\"%s\",\"phase_code\":%d,\"target_site\":\"%c\",\"deployed\":%s,\"sightings_a\":%d,\"sightings_b\":%d,\"recent_ct_deaths_a\":%d,\"recent_ct_deaths_b\":%d,\"bomb_recent\":%s,\"bomb_site\":\"%c\",\"planted_site\":\"%c\",\"previous_target\":\"%c\",\"formation\":%d,\"ct_alive\":%d,\"t_alive\":%d,\"ct_deaths\":%d,\"t_deaths\":%d,\"event_revision\":%d},\"bomb\":{\"planted\":%s,\"known\":%s,\"site\":\"%c\",\"age\":%.2f,\"source\":\"%s\"",
    CTPhaseName (required_.phase), static_cast<int> (required_.phase), required_.site, applied_ ? "true" : "false",
    evidence.sightings_a, evidence.sightings_b, evidence.deaths_a, evidence.deaths_b, evidence.bomb_recent ? "true" : "false",
    evidence.bomb_site, evidence.planted_site, evidence.previous_target, formation_, ct_alive, t_alive, ct_deaths_, t_deaths_, event_revision_,
    evidence.planted ? "true" : "false", evidence.planted || evidence.bomb_recent ? "true" : "false",
    evidence.planted ? evidence.planted_site : evidence.bomb_site, evidence.planted ? 0.0f : game.Time () - bomb_seen_time_,
    evidence.planted ? "planted_objective" : (evidence.bomb_recent ? "last_visible_carrier" : "unknown"));
  if (evidence.planted && !game_state.GetBombOrigin ().empty ()) {
    const auto &origin = game_state.GetBombOrigin ();
    payload.appendf (",\"origin\":[%.1f,%.1f,%.1f]", origin.x, origin.y, origin.z);
  }
  else if (evidence.bomb_recent) {
    payload.appendf (",\"origin\":[%.1f,%.1f,%.1f]", bomb_seen_origin_.x, bomb_seen_origin_.y, bomb_seen_origin_.z);
  }
  payload.appendf ("},\"t_plan\":{\"locked\":%s,\"target_site\":\"%c\",\"attack_delay\":%d,\"attack_at\":%.2f,\"elapsed\":%.2f,\"stage\":%d,\"fake_id\":\"%s\",\"compromised\":%s},\"round_events\":[",
    t_locked_ ? "true" : "false", t_target_, attack_delay_, attack_time_, game.Time () - game_state.GetRoundStartTime (), t_stage_,
    FakeMember () >= 0 ? identities_[FakeMember ()].id : "", t_compromised_ ? "true" : "false");
  first = true;
  const int count = ystl::min (event_count_, 32);
  for (int i = 0; i < count; ++i) {
    const auto &event = events_[(event_count_ - count + i) % 32];
    payload.appendf ("%s{\"id\":%d,\"type\":\"death\",\"time\":%.2f,\"killer\":%d,\"victim\":%d,\"killer_team\":\"%c\",\"victim_team\":\"%c\"",
      first ? "" : ",", event.id, event.time, event.killer, event.victim, event.killer_team, event.victim_team);
    if (event.known_origin) {
      payload.appendf (",\"origin\":[%.1f,%.1f,%.1f]", event.origin.x, event.origin.y, event.origin.z);
    }
    payload.append ("}");
    first = false;
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
  game.Print ("[Jev] Request round=%d request=%d nonce=%d revision=%d event=%s team=%s", round_, request_, nonce_, roster_revision_, pending_opening_ ? "round_start" : "poll", team_ == Team::CT ? "CT" : "T");
}

bool SystemOne::Apply (const int *targets, const char *source, CTPhase phase, char site, int formation, int timing) {
  char zones[5] {};
  bool holding = applied_;
  for (int i = 0; i < 5; ++i) {
    if (targets[i] < 0 || targets[i] >= position_count_ || !graph.Exists (positions_[targets[i]].waypoint)) { return false; }
    zones[i] = positions_[targets[i]].zone;
    holding &= identities_[i].position == targets[i];
    for (int j = 0; j < i; ++j) {
      if (targets[i] == targets[j] || positions_[targets[i]].waypoint == positions_[targets[j]].waypoint) { return false; }
    }
  }
  if (team_ == Team::CT) {
    if (phase != required_.phase || site != required_.site || timing != 0 || !ValidateCTPlacement (phase, site, formation, zones, holding)) { return false; }
  }
  else {
    if (site != 'A' && site != 'B') { return false; }
    if (!t_locked_) {
      if (phase != CTPhase::TCall || !(timing == 0 || (timing >= 45 && timing <= 80))) { return false; }
    }
    else if (phase != required_.phase || site != t_target_ || timing != attack_delay_) { return false; }
    const bool clearing = phase == CTPhase::TClear || (phase == CTPhase::TCall && timing > 0);
    const char zone = clearing ? (site == 'A' ? 'X' : 'Y') : site;
    const int stage = phase == CTPhase::TCall ? 0 : t_stage_;
    if (formation != (clearing ? stage : 0)) { return false; }
    for (int i = 0; i < 5; ++i) {
      if (clearing && i == FakeMember ()) {
        if (targets[i] != FakePosition (site)) { return false; }
      }
      else if (zones[i] != zone || (clearing && positions_[targets[i]].stage != stage)) { return false; }
    }
    if (!t_locked_) {
      t_locked_ = true; t_target_ = site; attack_delay_ = timing;
      attack_time_ = game_state.GetRoundStartTime () + static_cast<float> (timing);
      const int caller = Caller ();
      TeamChat (timing == 0 ? ystl::strings.format ("Caller %s: attack %c. Rush now, guns out!", caller >= 0 ? identities_[caller].id : "server", site) :
        ystl::strings.format ("Caller %s: attack %c. Ready outside site. Execute %02d:%02d after freeze. Guns out.", caller >= 0 ? identities_[caller].id : "server", site, timing / 60, timing % 60));
      game.Print ("[Jev] T call target=%c delay=%d attack_at=%.2f", site, timing, attack_time_);
    }
  }
  const bool opening = !applied_;
  pending_ = false;
  applied_ = true;
  const bool phase_changed = ordered_.phase != phase || ordered_.site != site;
  ordered_ = { phase, site };
  formation_ = formation;
  defuser_ = -1;
  if (phase == CTPhase::Retake) {
    float best = kInfiniteDistance;
    for (int i = 0; i < 5; ++i) {
      auto *player = Player (i);
      if (player == nullptr || !game.IsAliveEntity (player->Ent ())) { continue; }
      const float score = player->pev->origin.distance_sq (game_state.GetBombOrigin ());
      if (score < best) { best = score; defuser_ = i; }
    }
  }
  for (int i = 0; i < 5; ++i) {
    auto &identity = identities_[i];
    const bool changed = identity.position != targets[i];
    identity.position = targets[i];
    game.Print ("[Jev] Order round=%d request=%d source=%s phase=%s target=%c formation=%d identity=%s role=%s position=%s zone=%c changed=%d nonce=%d",
      round_, request_, source, CTPhaseName (phase), site, formation, identity.id, identity.role,
      positions_[targets[i]].id, positions_[targets[i]].zone, changed ? 1 : 0, nonce_);
    if (auto *player = Player (i); player != nullptr && changed && (opening || player->GetTaskId () == TaskId::Normal)) {
      player->ClearTasks ();
      player->ClearSearchNodes ();
      player->Task ()->data = positions_[targets[i]].waypoint;
    }
  }
  if (phase_changed && !opening) {
    if (phase == CTPhase::TExecute) { TeamChat (ystl::strings.format ("Caller: execute %c NOW. Everyone move in. Fake rejoins attack.", site)); }
    else if (phase != CTPhase::TClear) { TeamChat (ystl::strings.format ("Caller: %s, target %c.", CTPhaseName (phase), site)); }
  }
  if (opening) { next_chat_ = game.Time (); }
  next_telemetry_ = game.Time ();
  return true;
}

void SystemOne::ApplyFallback () {
  pending_ = false;
  if (team_ == Team::Terrorist) {
    const char site = t_locked_ ? t_target_ : (round_ % 2 == 0 ? 'A' : 'B');
    const int timing = t_locked_ ? attack_delay_ : 60;
    if (applied_ && ordered_.phase == required_.phase && ordered_.site == required_.site &&
        (required_.phase != CTPhase::TClear || formation_ == t_stage_)) { return; }
    const bool clearing = required_.phase == CTPhase::TClear || (!t_locked_ && timing > 0);
    const char zone = clearing ? (site == 'A' ? 'X' : 'Y') : site;
    int targets[5] {}, count = 0;
    for (int i = 0; i < position_count_ && count < 5; ++i) {
      if (positions_[i].zone == zone && (!clearing || positions_[i].stage == t_stage_)) { targets[count++] = i; }
    }
    if (count == 5 && clearing) {
      int ordered[5] {};
      for (int member = 0; member < 5; ++member) {
        ordered[member] = -1;
        for (int candidate = 0; candidate < count; ++candidate) {
          if (ystl::StringRef (positions_[targets[candidate]].preferred_roles) != identities_[member].role) { continue; }
          bool used = false;
          for (int prior = 0; prior < member; ++prior) { used |= ordered[prior] == targets[candidate]; }
          if (!used) { ordered[member] = targets[candidate]; break; }
        }
      }
      if (FakeMember () >= 0) { ordered[FakeMember ()] = FakePosition (site); }
      Apply (ordered, "policy_fallback", required_.phase, site, t_stage_, timing);
    }
    else if (count == 5) { Apply (targets, "policy_fallback", required_.phase, site, 0, timing); }
    return;
  }
  if (applied_ && (required_.phase == CTPhase::Gather || required_.phase == CTPhase::LocateBomb ||
      (ordered_.phase == required_.phase && ordered_.site == required_.site))) {
    game.Print ("[Jev] Request %d timed out; retaining last valid named orders.", request_);
    return;
  }
  char wanted[5] { 'B', 'M', 'S', 'S', 'L' };
  int formation = 1;
  if (required_.phase == CTPhase::Cautious) {
    wanted[0] = wanted[1] = wanted[2] = required_.site;
    wanted[3] = required_.site == 'A' ? 'B' : 'A';
    wanted[4] = 'M'; formation = 0;
  }
  else if (required_.phase == CTPhase::Commit || required_.phase == CTPhase::Retake) {
    for (auto &zone : wanted) { zone = required_.site; }
    formation = 0;
  }
  int targets[5] {};
  for (int i = 0; i < 5; ++i) {
    targets[i] = -1;
    for (int candidate = 0; candidate < position_count_; ++candidate) {
      if (positions_[candidate].zone != wanted[i]) { continue; }
      bool used = false;
      for (int j = 0; j < i; ++j) { used |= targets[j] == candidate; }
      if (!used) { targets[i] = candidate; break; }
    }
  }
  Apply (targets, "policy_fallback", required_.phase, required_.site, formation);
}

int SystemOne::Goal (int index) const {
  if (!cv_systemone || !config_loaded_ || !applied_) {
    return -1;
  }
  const int identity = IdentityFor (index);
  if (identity == -1 || identities_[identity].position < 0) {
    return -1;
  }
  if (ordered_.phase == CTPhase::Retake && identity == defuser_ && game_state.IsBombPlanted ()) {
    return -1; // Let the closest defender execute YaPB's normal bomb/defuse tasks.
  }
  if (team_ == Team::Terrorist && ordered_.phase >= CTPhase::TExecute) {
    auto *player = Player (identity);
    if (player != nullptr && player->has_c4_) {
      int best = -1; float distance = kInfiniteDistance;
      for (const auto node : graph.GetPoints (PointType::Goal)) {
        if (SiteFor (graph[node].origin, true) != t_target_) { continue; }
        const float score = player->pev->origin.distance_sq (graph[node].origin);
        if (score < distance) { distance = score; best = node; }
      }
      if (best >= 0) { return best; }
    }
  }
  return positions_[identities_[identity].position].waypoint;
}

bool SystemOne::HoldPosition (Bot *bot) const {
  if (team_ == Team::Terrorist && ordered_.phase >= CTPhase::TExecute && bot->has_c4_) { return false; }
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
  ctrl.Msg ("Jev %s: round=%d request=%d revision=%d pending=%d poll=%.1fs", team_ == Team::CT ? "CT" : "T", round_, request_, roster_revision_, pending_ ? 1 : 0, cv_systemone_poll_interval.As<float> ());
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
  if (!Apply (targets, "operator", required_.phase, required_.site, formation_, team_ == Team::Terrorist ? attack_delay_ : 0)) {
    return false;
  }
  next_poll_ = game.Time () + cv_systemone_poll_interval.As<float> ();
  return true;
}

char SystemOne::SiteFor (const ystl::Vector &origin, bool planted) const {
  if (origin.empty ()) { return '-'; }
  float best = kInfiniteDistance;
  char site = '-';
  for (int i = 0; i < position_count_; ++i) {
    const auto &position = positions_[i];
    if (planted && position.zone != 'A' && position.zone != 'B') { continue; }
    const float distance = origin.distance_sq (position.origin);
    if (distance < best) { best = distance; site = position.site; }
  }
  return (site == 'A' || site == 'B') && (planted || best < ystl::sqrf (800.0f)) ? site : '-';
}

CTEvidence SystemOne::Evidence () const {
  CTEvidence evidence {};
  evidence.deployed = applied_;
  evidence.planted = game_state.IsBombPlanted ();
  evidence.planted_site = evidence.planted ? SiteFor (game_state.GetBombOrigin (), true) : '-';
  evidence.bomb_recent = game.Time () - bomb_seen_time_ <= 10.0f;
  evidence.bomb_site = evidence.bomb_recent ? bomb_seen_site_ : '-';
  evidence.previous_target = ordered_.site;
  for (const auto &contact : sightings_) {
    if (!contact.active || game.Time () - contact.time > 8.0f) { continue; }
    if (contact.site == 'A') { ++evidence.sightings_a; }
    if (contact.site == 'B') { ++evidence.sightings_b; }
  }
  const int count = ystl::min (event_count_, 32);
  for (int i = 0; i < count; ++i) {
    const auto &event = events_[(event_count_ - count + i) % 32];
    if (!event.known_origin || event.victim_team != 'C' || game.Time () - event.time > 8.0f) { continue; }
    const char site = SiteFor (event.origin);
    if (site == 'A') { ++evidence.deaths_a; }
    if (site == 'B') { ++evidence.deaths_b; }
  }
  return evidence;
}

void SystemOne::ObserveEnemy (Bot *observer, edict_t *enemy, const ystl::Vector &origin) {
  if (!cv_systemone || !config_loaded_ || observer->team_ != team_ || !game.IsPlayerEntity (enemy) ||
      game.GetPlayerTeam (enemy) == team_ || !util.IsVisible (origin, observer->Ent ())) { return; }
  const int userid = engfuncs.pfnGetPlayerUserId (enemy);
  Sighting *slot = &sightings_[0];
  for (auto &contact : sightings_) {
    if (contact.active && contact.userid == userid) { slot = &contact; break; }
    if (!contact.active || contact.time < slot->time) { slot = &contact; }
  }
  slot->userid = userid; slot->time = game.Time (); slot->origin = origin;
  slot->site = SiteFor (origin); slot->active = true;
  if (team_ == Team::CT && (enemy->v.weapons & (1 << static_cast<int> (Weapon::C4))) != 0) {
    bomb_seen_time_ = game.Time (); bomb_seen_origin_ = origin; bomb_seen_site_ = slot->site;
  }
}

void SystemOne::RecordDeath (edict_t *killer, edict_t *victim) {
  if (!cv_systemone || !game.IsPlayerEntity (victim)) { return; }
  const auto victim_team = game.GetPlayerTeam (victim);
  if (victim_team != Team::CT && victim_team != Team::Terrorist) { return; }
  auto &event = events_[event_count_ % 32]; event = {};
  event.id = ++event_count_; event.time = game.Time ();
  event.victim = engfuncs.pfnGetPlayerUserId (victim);
  event.victim_team = victim_team == Team::CT ? 'C' : 'T';
  if (game.IsPlayerEntity (killer)) {
    event.killer = engfuncs.pfnGetPlayerUserId (killer);
    const auto team = game.GetPlayerTeam (killer);
    event.killer_team = team == Team::CT ? 'C' : (team == Team::Terrorist ? 'T' : '-');
  }
  if (victim_team == Team::CT) { ++ct_deaths_; }
  else { ++t_deaths_; }
  if (victim_team == team_) { event.origin = victim->v.origin; event.known_origin = true; } // A public death notification does not reveal a T's location.
  for (auto &contact : sightings_) {
    if (contact.userid == event.victim) { contact.active = false; }
  }
  ++event_revision_;
}

bool SystemOne::GunReady (int index) const {
  return team_ == Team::Terrorist && Goal (index) != -1;
}

bool SystemOne::Cautious (int index) const {
  if (Goal (index) == -1) { return false; }
  if (team_ == Team::CT) { return ordered_.phase == CTPhase::Cautious; }
  if (ordered_.phase != CTPhase::TClear || t_compromised_ || IdentityFor (index) == FakeMember ()) { return false; }
  auto *entity = Entity (IdentityFor (index));
  if (entity == nullptr) { return false; }
  for (int i = 0; i < position_count_; ++i) {
    const auto &position = positions_[i];
    if (position.site == t_target_ && position.stage >= 1 && ystl::StringRef (position.id).ends_with ("entry") &&
        entity->v.origin.distance_sq (position.origin) <= ystl::sqrf (300.0f)) { return true; }
  }
  return false;
}

bool SystemOne::Planting (int index) const {
  if (team_ != Team::Terrorist || !applied_ || ordered_.phase < CTPhase::TExecute) { return false; }
  const int identity = IdentityFor (index);
  auto *player = identity >= 0 ? Player (identity) : nullptr;
  return player != nullptr && player->has_c4_;
}

bool SystemOne::PlantAllowed (int index) const {
  if (team_ != Team::Terrorist || IdentityFor (index) < 0) { return true; }
  return t_locked_ && game.Time () >= attack_time_;
}

CTDecision SystemOne::TerroristState () {
  if (!t_locked_) { return { CTPhase::TCall, '-' }; }
  if (game_state.IsBombPlanted ()) {
    const char site = SiteFor (game_state.GetBombOrigin (), true);
    if (site == 'A' || site == 'B') { t_target_ = site; }
    return { CTPhase::TPostPlant, t_target_ };
  }
  if (game.Time () >= attack_time_) { return { CTPhase::TExecute, t_target_ }; }
  if (t_stage_ < 2 && game.Time () >= game_state.GetRoundStartTime ()) {
    int entry = -1;
    for (int i = 0; i < 5; ++i) {
      auto *entity = Entity (i);
      if (i != FakeMember () && entity != nullptr && game.IsAliveEntity (entity) && ystl::StringRef (identities_[i].role) == "entry") { entry = i; break; }
    }
    if (entry == -1) {
      for (int i = 0; i < 5; ++i) { auto *entity = Entity (i); if (i != FakeMember () && entity != nullptr && game.IsAliveEntity (entity)) { entry = i; break; } }
    }
    if (entry >= 0 && identities_[entry].position >= 0) {
      const auto &position = positions_[identities_[entry].position];
      if (position.stage == t_stage_ && Entity (entry)->v.origin.distance_sq (graph[position.waypoint].origin) < ystl::sqrf (128.0f)) {
        if (stage_ready_time_ <= 0.0f) { stage_ready_time_ = game.Time (); }
        if (game.Time () >= stage_ready_time_ + 2.0f) { ++t_stage_; stage_ready_time_ = 0.0f; }
      }
      else { stage_ready_time_ = 0.0f; }
    }
  }
  return { CTPhase::TClear, t_target_ };
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
  MaintainRoster ();
  const int caller = Caller ();
  if (caller != last_caller_) {
    if (applied_ && last_caller_ >= 0 && caller >= 0) {
      TeamChat (team_ == Team::Terrorist ? ystl::strings.format ("%s taking over caller. Attack %c. Execute time unchanged.", identities_[caller].id, t_target_) :
        ystl::strings.format ("%s taking over caller. Keep current defense orders.", identities_[caller].id));
      game.Print ("[Jev] Caller team=%s identity=%s previous=%d", team_ == Team::CT ? "CT" : "T", caller >= 0 ? identities_[caller].id : "none", last_caller_);
      pending_ = false; next_poll_ = game.Time ();
    }
    last_caller_ = caller;
  }
  if (team_ == Team::Terrorist) {
    if (!fake_chosen_) {
      bool ready = true;
      for (int i = 0; i < 5; ++i) { ready &= Entity (i) != nullptr; }
      if (ready) {
        fake_chosen_ = true;
        if (ystl::rg (0, 1) != 0) {
          int candidates[5] {}, count = 0;
          for (int i = 0; i < 5; ++i) { auto *player = Player (i); if (player != nullptr && game.IsAliveEntity (player->Ent ()) && !player->has_c4_) { candidates[count++] = i; } }
          if (count > 0) { fake_member_ = candidates[ystl::rg (0, count - 1)]; }
        }
      }
    }
    if (!t_compromised_ && game.Time () >= game_state.GetRoundStartTime ()) {
      for (int i = 0; i < 5; ++i) {
        auto *entity = Entity (i); auto *player = Player (i);
        if (entity != nullptr && game.IsAliveEntity (entity) && (entity->v.health < 100.0f ||
            (entity->v.button & IN_ATTACK) != 0 || (player != nullptr && has_flag (player->states_, Sense::SeeingEnemy)))) {
          t_compromised_ = true;
          game.Print ("[Jev] T approach exposed; run to ready/execute positions."); break;
        }
      }
    }
  }
  const int previous_stage = t_stage_;
  const auto decision = team_ == Team::CT ? EvaluateCTState (Evidence ()) : TerroristState ();
  if (decision.phase != required_.phase || decision.site != required_.site || previous_stage != t_stage_) {
    required_ = decision;
    pending_ = false;
    next_poll_ = game.Time ();
    game.Print ("[Jev] %s state phase=%s target=%c", team_ == Team::CT ? "CT" : "T", CTPhaseName (required_.phase), required_.site);
    if ((required_.phase == CTPhase::Commit || required_.phase == CTPhase::Retake || required_.phase == CTPhase::TExecute || required_.phase == CTPhase::TPostPlant || previous_stage != t_stage_) &&
        (ordered_.phase != required_.phase || ordered_.site != required_.site || previous_stage != t_stage_)) {
      ApplyFallback (); // Objective-critical orders do not wait for a cloud call.
    }
  }
  bool any_connected = false;
  bool opening_ready = true;
  for (int i = 0; i < 5; ++i) {
    auto *entity = Entity (i);
    any_connected |= entity != nullptr;
    opening_ready &= entity != nullptr && game.IsAliveEntity (entity);
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
        int round = 0, request = 0, nonce = 0, revision = 0, phase = 0, formation = 0, timing = 0, targets[5] {};
        char site = '-';
        char extra = 0;
        if (sscanf (response, "YJ3 %d %d %d %d %d %c %d %d %d %d %d %d %d %c", &round, &request, &nonce, &revision, &phase, &site, &formation, &timing,
              &targets[0], &targets[1], &targets[2], &targets[3], &targets[4], &extra) == 13 &&
            round == round_ && request == request_ && nonce == nonce_ && revision == roster_revision_ && game.Time () < deadline_) {
          if (phase >= 0 && phase <= 9 && Apply (targets, "jev", static_cast<CTPhase> (phase), site, formation, timing)) {
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
      if (!identity.announced && Entity (i) != nullptr) {
        const auto &position = positions_[identity.position];
        if (team_ == Team::Terrorist) {
          int ready = identity.position;
          if (i != FakeMember ()) {
            for (int p = 0; p < position_count_; ++p) {
              if (positions_[p].site == t_target_ && positions_[p].stage == 2 &&
                  ystl::StringRef (positions_[p].preferred_roles) == identity.role) { ready = p; break; }
            }
          }
          const ystl::String message = i == FakeMember () && attack_delay_ > 0 ?
            ystl::strings.format ("%s %s: attack %c. Fake %c at %s. Stay OUTSIDE site and rejoin on execute.", identity.id, identity.role, t_target_, t_target_ == 'A' ? 'B' : 'A', PositionLabel (identity.position)) :
            ystl::strings.format ("%s %s: attack %c. %s %s.", identity.id, identity.role, t_target_, attack_delay_ == 0 ? "rush" : "ready at", attack_delay_ == 0 ? PositionLabel (identity.position) : PositionLabel (ready));
          if (identity.human) { TeamChat (ystl::strings.format ("Human role - %s", message.chars ())); }
          else { Player (i)->SendToChat (message, true); }
        }
        else if (identity.human) {
          TeamChat (ystl::strings.format ("Caller: human %s, your role is %s; objective %s.", identity.id, identity.role, position.id));
        }
        else { Player (i)->SendToChat (ystl::strings.format ("%s: %s, going %c - %s.", identity.id, identity.role, position.site, position.id), true); }
        game.Print ("[Jev] Chat round=%d identity=%s role=%s site=%c position=%s", round_, identity.id, identity.role, position.zone, position.id);
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
