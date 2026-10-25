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

void CompetitiveMatch::NewMatch () {
  rules_.Reset (); pending_ = MatchAction::None; winner_seen_ = false; raw_ct_ = raw_t_ = 0;
  score_dirty_ = true;
  game.ServerCommand ("mp_startmoney 800");
  game.ServerCommand ("mp_roundtime 1.916667");
  game.ServerCommand ("mp_c4timer 40");
  game.Print ("[Match] New match 0-0. MR12, pistol cash 800, round 1:55, bomb 0:40. OT MR3 / 10000.");
}

void CompetitiveMatch::NewRound () {
  if (!cv_systemone) { return; }
  if (rules_.finished) {
    NewMatch (); internal_restart_ = true; game.ServerCommand ("sv_restart 1");
  }
  winner_seen_ = false;
  if (internal_restart_) { ignore_zero_until_ = game.Time () + 4.0f; internal_restart_ = false; }
  score_dirty_ = true;
}

void CompetitiveMatch::Win (Team team) {
  if (!cv_systemone || winner_seen_ || internal_restart_ || rules_.finished) { return; }
  winner_seen_ = true;
  pending_ = rules_.Win ((team == Team::CT) != rules_.swapped);
  score_dirty_ = true;
  game.Print ("[Match] Score A=%d B=%d swapped=%d overtime=%d action=%d", rules_.a, rules_.b, rules_.swapped ? 1 : 0, rules_.overtime ? 1 : 0, static_cast<int> (pending_));
}

void CompetitiveMatch::ObserveScore (ystl::StringRef team, int score) {
  if (!cv_systemone || publishing_) { return; }
  const int previous = raw_ct_ + raw_t_;
  if (team == "CT") { raw_ct_ = score; } else if (team == "TERRORIST") { raw_t_ = score; } else { return; }
  if (previous > 0 && raw_ct_ == 0 && raw_t_ == 0 && !internal_restart_ && game.Time () > ignore_zero_until_) { NewMatch (); }
  score_dirty_ = true;
}

void CompetitiveMatch::Frame () {
  if (!cv_systemone) { return; }
  if (pending_ == MatchAction::Finished) {
    pending_ = MatchAction::None;
    MessageWriter (MSG_ALL, msgs.Id (NetMsg::TextMsg)).WriteByte (4).WriteString ("Match finished. Next round starts a new $800 pistol match.");
  }
  if (pending_ != MatchAction::None && pending_ != MatchAction::Finished && !internal_restart_ && game_state.IsRoundOver ()) {
    const auto action = pending_; pending_ = MatchAction::None; internal_restart_ = true;
    ignore_zero_until_ = game.Time () + 8.0f;
    if (action == MatchAction::Halftime || action == MatchAction::OvertimeHalf) {
      edict_t *players[32] {}; int wanted[32] {}, count = 0;
      for (int slot = 1; slot <= game.MaxClients () && count < 32; ++slot) {
        auto *entity = game.EntityOfIndex (slot);
        if (!game.IsPlayerEntity (entity)) { continue; }
        const auto side = game.GetPlayerTeam (entity);
        if (side == Team::CT || side == Team::Terrorist) { players[count] = entity; wanted[count++] = side == Team::CT ? 1 : 2; }
      }
      for (int i = 0; i < count; ++i) {
        game.Command (players[i], "chooseteam"); game.Command (players[i], "menuselect %d", wanted[i]); game.Command (players[i], "menuselect 1");
      }
      game.Print ("[Match] Halftime switch requested for %d players.", count);
    }
    game.ServerCommand ("mp_startmoney %d", rules_.overtime ? 10000 : 800);
    game.ServerCommand ("sv_restart 1");
  }
  if (score_dirty_ && msgs.Id (NetMsg::TeamScore) > 0) {
    score_dirty_ = false; publishing_ = true;
    MessageWriter (MSG_ALL, msgs.Id (NetMsg::TeamScore)).WriteString ("CT").WriteShort (rules_.SideScore (true));
    MessageWriter (MSG_ALL, msgs.Id (NetMsg::TeamScore)).WriteString ("TERRORIST").WriteShort (rules_.SideScore (false));
    publishing_ = false;
  }
}

bool SystemOne::LoadConfig () {
  config_attempted_ = true;
  position_count_ = 0;
  ystl::MemFile file (ystl::strings.join_path (bstor.GetRunningPathVfs (), "conf", (team_ == Team::CT) != competitive_match.Rules ().swapped ? "systemone.cfg" : "systemone_t.cfg"));
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

void SystemOne::ObserveMoney (edict_t *entity, int amount) {
  if (game.IsNullEntity (entity) || !game.IsPlayerEntity (entity) || game.GetPlayerTeam (entity) != team_) { return; }
  const int slot = game.IndexOfEntity (entity);
  if (slot > 0 && slot <= 32) { wallet_[slot] = amount; wallet_userid_[slot] = engfuncs.pfnGetPlayerUserId (entity); }
}

void SystemOne::SetScore (ystl::StringRef team, int score) {
  if (score < 0 || score > 1000) { return; }
  if (team == "CT") { ct_score_ = cv_systemone ? competitive_match.Rules ().SideScore (true) : score; }
  else if (team == "TERRORIST") { t_score_ = cv_systemone ? competitive_match.Rules ().SideScore (false) : score; }
  else { return; }
  score_known_ = true;
}

int SystemOne::MoneyFor (int identity) const {
  if (auto *player = Player (identity); player != nullptr) { return player->money_amount_; }
  const auto &binding = identities_[identity];
  return binding.slot > 0 && binding.slot <= 32 && wallet_userid_[binding.slot] == binding.userid ? wallet_[binding.slot] : -1;
}

Weapon SystemOne::ChooseBuyWeapon (int identity, Bot *player) const {
  if (player->HasPrimaryWeapon () || economy_mode_ == EconomyMode::Save) { return Weapon::Invalid; }
  const int budget = EconomyWeaponBudget (economy_mode_, player->money_amount_, player->pev->armorvalue >= 65.0f, team_ == Team::CT);
  const bool half = economy_mode_ == EconomyMode::Half;
  const auto &binding = identities_[identity];
  const auto role = ystl::StringRef (binding.role);
  const bool long_lane = binding.position >= 0 && positions_[binding.position].look_at.distance_sq (positions_[binding.position].origin) >= ystl::sqrf (700.0f);
  Weapon candidates[10] {}; int count = 0;
  if (!half && role == "awper" && long_lane) { candidates[count++] = Weapon::AWP; candidates[count++] = Weapon::Scout; }
  if (!half) {
    candidates[count++] = team_ == Team::CT ? Weapon::M4A1 : Weapon::AK47;
    candidates[count++] = team_ == Team::CT ? Weapon::Famas : Weapon::Galil;
  }
  if (role == "lurker") { candidates[count++] = team_ == Team::CT ? Weapon::TMP : Weapon::MAC10; }
  candidates[count++] = Weapon::MP5;
  candidates[count++] = team_ == Team::CT ? Weapon::TMP : Weapon::MAC10;
  for (int i = 0; i < count; ++i) {
    const auto *weapon = conf.GetWeapon (candidates[i]);
    if (weapon != nullptr && weapon->price <= budget && player->IsWeaponEligibleForPurchase (weapon, team_)) { return candidates[i]; }
  }
  const auto *pistol = conf.GetWeapon (Weapon::Deagle);
  if (pistol != nullptr && pistol->price + 100 <= EconomySpendCap (economy_mode_, player->money_amount_) &&
      player->IsWeaponEligibleForPurchase (pistol, team_) && !has_flag (player->pev->weapons, ystl::bit (Weapon::Deagle))) { return Weapon::Deagle; }
  return Weapon::Invalid;
}

void SystemOne::PrepareEconomy () {
  if (economy_ready_ || !applied_ || game.Time () < round_time_ + 0.8f) { return; }
  int total = 0; economy_known_ = 0;
  for (int i = 0; i < 5; ++i) {
    const int money = MoneyFor (i);
    if (Entity (i) != nullptr && money >= 0) { total += money; ++economy_known_; }
  }
  economy_average_ = economy_known_ > 0 ? total / economy_known_ : 0;
  economy_mode_ = competitive_match.Rules ().MustBuy () ? EconomyMode::LastRound : ChooseEconomy (economy_average_, 0, 0, false);
  for (int i = 0; i < 5; ++i) {
    auto *player = Player (i);
    if (player == nullptr) { continue; }
    buy_cash_[i] = player->money_amount_;
    buy_cap_[i] = EconomySpendCap (economy_mode_, buy_cash_[i]);
    buy_weapon_[i] = ChooseBuyWeapon (i, player);
    game.Print ("[Jev] Buy team=%s caller=%d identity=%s mode=%s average=%d cash=%d cap=%d weapon=%d score=%d:%d",
      team_ == Team::CT ? "CT" : "T", Caller (), identities_[i].id, EconomyName (economy_mode_), economy_average_, buy_cash_[i], buy_cap_[i], static_cast<int> (buy_weapon_[i]), ct_score_, t_score_);
  }
  economy_ready_ = true;
}

bool SystemOne::HandleBuy (Bot *player) {
  if (!cv_systemone || player->team_ != team_) { return false; }
  if (config_attempted_ && !config_loaded_) { return false; }
  const int identity = IdentityFor (player->index_);
  if (config_loaded_ && identity < 0) { return false; }
  const auto wait = [&] () { player->next_buy_timer_.start (0.1f); player->PushMsgQueue (Msg::Buy); };
  if (identity < 0 || !applied_) { wait (); return true; }
  PrepareEconomy ();
  if (!economy_ready_) { wait (); return true; }
  const int remaining = ystl::min (player->money_amount_, ystl::max (0, buy_cap_[identity] - (buy_cash_[identity] - player->money_amount_)));
  const bool spending = economy_mode_ != EconomyMode::Save;
  switch (player->buy_state_) {
  case BuyState::PrimaryWeapon: {
    const auto *weapon = conf.GetWeapon (buy_weapon_[identity]);
    if (!player->HasPrimaryWeapon () && weapon != nullptr && weapon->price <= remaining && player->IsWeaponEligibleForPurchase (weapon, team_)) {
      player->IssueBuyCommand (weapon);
    }
    break;
  }
  case BuyState::ArmorVestHelm:
    if (spending && player->pev->armorvalue < 65.0f) {
      if (economy_mode_ != EconomyMode::Half && remaining >= 1100 && !player->IsWeaponRestricted (Weapon::ArmorHelm)) { player->IssueCommand ("buyequip;menuselect 2"); }
      else if (remaining >= 750 && !player->IsWeaponRestricted (Weapon::Armor)) { player->IssueCommand ("buyequip;menuselect 1"); }
    }
    break;
  case BuyState::Ammo:
    if (remaining >= 200) { if (player->HasPrimaryWeapon ()) { player->IssueCommand ("buyammo1"); } player->IssueCommand ("buyammo2"); }
    break;
  case BuyState::DefusalKit:
    if (spending && team_ == Team::CT && !player->has_defuser_ && remaining >= 200 && !player->IsWeaponRestricted (Weapon::Defuser)) { player->IssueCommand ("defuser"); }
    break;
  case BuyState::Grenades:
    if (spending && remaining >= 300 && !player->IsWeaponRestricted (Weapon::Flashbang)) { player->IssueCommand ("flash"); }
    break;
  default: break;
  }
  ++player->buy_state_;
  player->next_buy_timer_.start (0.1f);
  player->PushMsgQueue (Msg::Buy);
  return true;
}

void SystemOne::MaintainDrops () {
  if (!economy_ready_) { return; }
  for (int receiver = 0; receiver < 5; ++receiver) {
    auto *player = Player (receiver);
    auto *box = drop_box_[receiver];
    if (box == nullptr) { continue; }
    if (player == nullptr || player->HasPrimaryWeapon () || game.IsNullEntity (box) || box->serialnumber != drop_serial_[receiver] || game.Time () > game_state.GetRoundStartTime () + 6.0f) {
      if (player != nullptr && player->pickup_item_ == box) { player->pickup_item_ = nullptr; }
      if (player != nullptr && player->HasPrimaryWeapon ()) { game.Print ("[Jev] Drop received identity=%s", identities_[receiver].id); }
      drop_box_[receiver] = nullptr; continue;
    }
    player->pickup_item_ = box;
    player->pickup_type_ = Pickup::Weapon;
    player->StartTask (TaskId::PickupItem, 60.0f, kInvalidNodeIndex, 0.0f, true);
  }
  if (economy_mode_ == EconomyMode::Save || game.Time () >= game_state.GetRoundStartTime ()) { return; }
  for (int donor = 0; donor < 5; ++donor) {
    auto *player = Player (donor);
    if (donated_[donor] || player == nullptr || !player->buying_finished_ || !player->in_buy_zone_ || !player->HasPrimaryWeapon () || !game.IsAliveEntity (player->Ent ())) { continue; }
    Weapon primary = Weapon::Invalid;
    for (const auto &weapon : conf.GetWeapons ()) {
      if (has_flag (kPrimaryWeaponMask, ystl::bit (weapon.id)) && has_flag (player->pev->weapons, ystl::bit (weapon.id))) { primary = weapon.id; break; }
    }
    const auto *weapon = conf.GetWeapon (primary);
    if (weapon == nullptr || !player->IsWeaponEligibleForPurchase (weapon, team_)) { continue; }
    for (int receiver = 0; receiver < 5; ++receiver) {
      auto *entity = Entity (receiver);
      if ((primary == Weapon::AWP || primary == Weapon::Scout) && ystl::StringRef (identities_[receiver].role) != "awper") { continue; }
      if (receiver == donor || received_[receiver] || entity == nullptr || !game.IsAliveEntity (entity) ||
          !CanFundDrop (player->money_amount_, weapon->price, has_flag (entity->v.weapons, kPrimaryWeaponMask), MoneyFor (receiver)) ||
          entity->v.origin.distance_sq (player->pev->origin) > ystl::sqrf (400.0f)) { continue; }
      buy_weapon_[donor] = primary;
      buy_cap_[donor] = ystl::min (buy_cash_[donor], buy_cap_[donor] + weapon->price);
      player->DropWeaponForUser (entity, false);
      donated_[donor] = received_[receiver] = true;
      game.Print ("[Jev] Drop donor=%s receiver=%s weapon=%d surplus=%d", identities_[donor].id, identities_[receiver].id, static_cast<int> (primary), player->money_amount_);
      if (Player (receiver) != nullptr) {
        game.SearchEntities ("classname", "weaponbox", [&] (edict_t *box) {
          if (box->v.owner == player->Ent () && !game.IsEntityModelMatches (box, "backpack.mdl") && box->v.origin.distance_sq (player->pev->origin) < ystl::sqrf (128.0f)) {
            drop_box_[receiver] = box; drop_serial_[receiver] = box->serialnumber; return EntitySearchResult::Break;
          }
          return EntitySearchResult::Continue;
        });
      }
      break;
    }
  }
}

void SystemOne::NewRound () {
  ++round_;
  if (competitive_match.Rules ().a + competitive_match.Rules ().b == 0) { last_crunch_round_ = -100; }
  crunch_active_ = crunch_used_ = crunch_returning_ = false; crunch_choke_ = '-';
  drop_known_ = false; objective_origin_.clear (); objective_box_ = nullptr; objective_base_ = collector_ = -1;
  next_objective_scan_ = next_c4_handoff_ = 0.0f; t_pick_time_ = -100.0f;
  for (auto &passed : rush_passed_) { passed = false; }
  economy_ready_ = false;
  for (int i = 0; i < 5; ++i) { buy_weapon_[i] = Weapon::Invalid; buy_cash_[i] = buy_cap_[i] = 0; donated_[i] = received_[i] = false; drop_box_[i] = nullptr; drop_serial_[i] = 0; }
  nonce_ = ystl::rg (1, 0x3fffffff);
  pending_ = applied_ = config_loaded_ = config_attempted_ = false;
  pending_state_.clear ();
  round_time_ = game.Time ();
  next_poll_ = round_time_ + 0.5f;
  next_telemetry_ = next_chat_ = next_roster_maintain_ = 0.0f;
  t_retargeted_ = t_early_ = false;
  t_compromised_ = fake_chosen_ = false; fake_member_ = -1; last_caller_ = -2;
  t_locked_ = false; attack_delay_ = -1; t_stage_ = 0; t_target_ = '-'; attack_time_ = stage_ready_time_ = 0.0f;
  event_count_ = ct_deaths_ = t_deaths_ = event_revision_ = 0;
  bomb_seen_time_ = -100.0f;
  bomb_seen_site_ = '-';
  required_ = ordered_ = {};
  formation_ = 0;
  defuser_ = -1;
  planted_info_ = {}; shared_plant_origin_.clear (); plant_source_ = "unknown"; plant_reporter_ = -1;
  for (auto &sighting : sightings_) { sighting = {}; }
  for (auto &event : events_) { event = {}; }
  for (auto &identity : identities_) {
    identity.position = -1;
    identity.hold_position = -1;
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
    payload.appendf ("%s{\"id\":\"%s\",\"site\":\"%c\",\"zone\":\"%c\",\"stage\":%d,\"origin\":[%.1f,%.1f,%.1f],\"look_at\":[%.1f,%.1f,%.1f],\"radius\":%.1f,\"stance\":\"%s\",\"preferred_roles\":\"%s\",\"waypoint\":%d,\"crunch_group\":\"%c\"}",
      i == 0 ? "" : ",", p.id, p.site, p.zone, p.stage, p.origin.x, p.origin.y, p.origin.z, p.look_at.x, p.look_at.y, p.look_at.z,
      p.radius, p.crouch ? "crouch" : "stand", p.preferred_roles, p.waypoint, CrunchGroup (i));
  }
  payload.append ("],\"bots\":[");
  for (int i = 0; i < 5; ++i) {
    const auto &identity = identities_[i];
    auto *player = Player (i);
    auto *entity = Entity (i);
    const ystl::Vector origin = entity != nullptr ? entity->v.origin : ystl::Vector {};
    const bool alive = entity != nullptr && game.IsAliveEntity (entity);
    const bool assigned = identity.position >= 0 && identity.position < position_count_;
    const int actual_goal = player != nullptr ? Goal (player->index_) : (assigned ? positions_[identity.position].waypoint : -1);
    payload.appendf ("%s{\"id\":\"%s\",\"role\":\"%s\",\"slot\":%d,\"userid\":%d,\"generation\":%d,\"human\":%s,\"caller\":%s,\"connected\":%s,\"alive\":%s,\"health\":%.0f,\"money\":%d,\"weapon\":%d,\"origin\":[%.1f,%.1f,%.1f],\"position\":\"%s\",\"site\":\"%c\",\"goal\":%d,\"rotating\":%s}",
      i == 0 ? "" : ",", identity.id, identity.role, identity.slot, identity.userid, identity.generation,
      identity.human ? "true" : "false", Caller () == i ? "true" : "false", entity != nullptr ? "true" : "false", alive ? "true" : "false", entity != nullptr ? entity->v.health : 0.0f,
      MoneyFor (i), player != nullptr ? static_cast<int> (player->current_weapon_) : 0,
      origin.x, origin.y, origin.z, assigned ? positions_[identity.position].id : "pending",
      assigned ? positions_[identity.position].site : '-', actual_goal,
      alive && graph.Exists (actual_goal) && origin.distance_sq (graph[actual_goal].origin) > ystl::sqrf (96.0f) ? "true" : "false");
  }
  payload.appendf ("],\"economy\":{\"ready\":%s,\"mode\":\"%s\",\"average\":%d,\"known_members\":%d,\"ct_score\":%d,\"t_score\":%d,\"score_known\":%s,\"match_format\":\"MR12\",\"must_buy\":%s},\"observed_contacts\":[",
    economy_ready_ ? "true" : "false", EconomyName (economy_mode_), economy_average_, economy_known_, ct_score_, t_score_, score_known_ ? "true" : "false", competitive_match.Rules ().MustBuy () ? "true" : "false");
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
  payload.appendf ("],\"round_state\":{\"phase\":\"%s\",\"phase_code\":%d,\"target_site\":\"%c\",\"deployed\":%s,\"sightings_a\":%d,\"sightings_b\":%d,\"recent_ct_deaths_a\":%d,\"recent_ct_deaths_b\":%d,\"bomb_recent\":%s,\"bomb_site\":\"%c\",\"planted_site\":\"%c\",\"previous_target\":\"%c\",\"formation\":%d,\"ct_alive\":%d,\"t_alive\":%d,\"ct_deaths\":%d,\"t_deaths\":%d,\"event_revision\":%d,\"dropped_secured\":%s,\"dropped_site\":\"%c\",\"crunch_available\":%s,\"crunch_used\":%s,\"crunch_active\":%s,\"crunch_choke\":\"%c\",\"elapsed\":%.2f},\"bomb\":{\"planted\":%s,\"known\":%s,\"site\":\"%c\",\"age\":%.2f,\"source\":\"%s\"",
    CTPhaseName (required_.phase), static_cast<int> (required_.phase), required_.site, applied_ ? "true" : "false",
    evidence.sightings_a, evidence.sightings_b, evidence.deaths_a, evidence.deaths_b, evidence.bomb_recent ? "true" : "false",
    evidence.bomb_site, evidence.planted_site, evidence.previous_target, formation_, ct_alive, t_alive, ct_deaths_, t_deaths_, event_revision_, evidence.secured_drop ? "true" : "false", evidence.dropped_site, CrunchAvailable () ? "true" : "false", crunch_used_ ? "true" : "false", crunch_active_ ? "true" : "false", crunch_choke_, game.Time () - game_state.GetRoundStartTime (),
    evidence.planted ? "true" : "false", (evidence.planted && evidence.planted_site != '-') || evidence.bomb_recent ? "true" : "false",
    evidence.planted ? evidence.planted_site : evidence.bomb_site, evidence.planted ? 0.0f : game.Time () - bomb_seen_time_,
    evidence.planted ? (team_ == Team::CT ? plant_source_ : "own_team_plant") : (evidence.bomb_recent ? "last_visible_carrier" : "unknown"));
  if (evidence.planted && !(team_ == Team::CT ? shared_plant_origin_ : game_state.GetBombOrigin ()).empty ()) {
    const auto &origin = team_ == Team::CT ? shared_plant_origin_ : game_state.GetBombOrigin ();
    payload.appendf (",\"origin\":[%.1f,%.1f,%.1f]", origin.x, origin.y, origin.z);
  }
  else if (evidence.bomb_recent) {
    payload.appendf (",\"origin\":[%.1f,%.1f,%.1f]", bomb_seen_origin_.x, bomb_seen_origin_.y, bomb_seen_origin_.z);
  }
  payload.appendf ("},\"bomb_objective\":{\"dropped\":%s,\"collector_id\":\"%s\",\"pickup_position\":\"%s\",\"source\":\"%s\"",
    drop_known_ ? "true" : "false", collector_ >= 0 ? identities_[collector_].id : "", objective_base_ >= 0 ? positions_[objective_base_].id : "",
    team_ == Team::CT ? "last_team_sighting" : "team_bomb_radar");
  if (drop_known_) { payload.appendf (",\"origin\":[%.1f,%.1f,%.1f]", objective_origin_.x, objective_origin_.y, objective_origin_.z); }
  payload.appendf ("},\"t_plan\":{\"locked\":%s,\"target_site\":\"%c\",\"attack_delay\":%d,\"attack_at\":%.2f,\"elapsed\":%.2f,\"stage\":%d,\"fake_id\":\"%s\",\"compromised\":%s,\"retargeted\":%s,\"early_execute\":%s},\"round_events\":[",
    t_locked_ ? "true" : "false", t_target_, attack_delay_, attack_time_, game.Time () - game_state.GetRoundStartTime (), t_stage_,
    FakeMember () >= 0 ? identities_[FakeMember ()].id : "", t_compromised_ ? "true" : "false", t_retargeted_ ? "true" : "false", t_early_ ? "true" : "false");
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
  const bool pivot = team_ == Team::Terrorist && t_locked_ && site != t_target_;
  const bool early = team_ == Team::Terrorist && t_locked_ && required_.phase == CTPhase::TClear && phase == CTPhase::TExecute;
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
    if (phase == CTPhase::Crunch) {
      if (timing != 0 || (!crunch_active_ && !CrunchAvailable ())) { return false; }
      if (site != 'L' && site != 'S' && site != 'M' && site != 'B') { return false; }
      if (crunch_active_ && site != crunch_choke_) { return false; }
      if (!crunch_active_) { for (int i = 0; i < 5; ++i) { auto *entity = Entity (i); if (entity == nullptr || !game.IsAliveEntity (entity)) { return false; } } }
      int count = 0;
      for (int i = 0; i < 5; ++i) {
        if (CrunchGroup (targets[i]) == site) { ++count; }
        else if (targets[i] != (crunch_active_ ? crunch_home_[i] : identities_[i].position)) { return false; }
      }
      if ((count != 3 && count != 4) || formation != count) { return false; }
      if (!crunch_active_) {
        for (int i = 0; i < 5; ++i) { crunch_home_[i] = identities_[i].position; }
        last_crunch_round_ = competitive_match.Rules ().a + competitive_match.Rules ().b + 1;
        crunch_started_ = game.Time (); crunch_used_ = crunch_active_ = true; crunch_choke_ = site; crunch_count_ = count;
      }
      required_ = { CTPhase::Crunch, site };
    }
    else if (crunch_returning_) {
      for (int i = 0; i < 5; ++i) { if (targets[i] != crunch_home_[i]) { return false; } }
    }
    else if (phase != required_.phase || site != required_.site || timing != 0 || !ValidateCTPlacement (phase, site, formation, zones, holding)) { return false; }
    if (phase != CTPhase::Crunch) { crunch_active_ = false; }
  }
  else {
    if (site != 'A' && site != 'B') { return false; }
    if (!t_locked_) {
      if (phase != CTPhase::TCall || !(timing == 0 || (timing >= 45 && timing <= 80))) { return false; }
    }
    else if (timing != attack_delay_ || (phase != required_.phase && !(early && CanExecuteEarly ())) || (pivot && !CanRetarget ())) { return false; }
    const bool clearing = phase == CTPhase::TClear || (phase == CTPhase::TCall && timing > 0);
    const char zone = phase == CTPhase::TRecoverBomb ? 'R' : (clearing ? (site == 'A' ? 'X' : 'Y') : site);
    const int stage = phase == CTPhase::TCall ? 0 : (pivot ? 2 : t_stage_);
    if (formation != (clearing ? stage : 0)) { return false; }
    for (int i = 0; i < 5; ++i) {
      if (phase == CTPhase::TRecoverBomb && i == collector_ && targets[i] != objective_base_) { return false; }
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
  if (pivot) { for (auto &passed : rush_passed_) { passed = false; } t_target_ = site; t_stage_ = 2; t_retargeted_ = true; required_ = { phase, site }; }
  if (early) { attack_time_ = game.Time (); t_early_ = true; required_ = { phase, site }; }
  const bool opening = !applied_;
  pending_ = false;
  applied_ = true;
  const bool phase_changed = ordered_.phase != phase || ordered_.site != site;
  ordered_ = { phase, site };
  formation_ = formation;
  if (phase == CTPhase::Retake) { ChooseDefuser (site); }
  else { defuser_ = -1; }
  for (int i = 0; i < 5; ++i) {
    auto &identity = identities_[i];
    const bool changed = identity.position != targets[i];
    if (changed) { identity.hold_position = -1; }
    identity.position = targets[i];
    game.Print ("[Jev] Order round=%d request=%d source=%s phase=%s target=%c formation=%d identity=%s role=%s position=%s zone=%c changed=%d nonce=%d",
      round_, request_, source, CTPhaseName (phase), site, formation, identity.id, identity.role,
      positions_[targets[i]].id, positions_[targets[i]].zone, changed ? 1 : 0, nonce_);
    if (auto *player = Player (i); player != nullptr && (changed || (phase_changed && phase == CTPhase::Retake)) &&
        (opening || player->GetTaskId () == TaskId::Normal || (phase == CTPhase::Retake &&
          (player->GetTaskId () == TaskId::Hunt || player->GetTaskId () == TaskId::MoveTo || player->GetTaskId () == TaskId::Camp || player->GetTaskId () == TaskId::Pause)))) {
      player->ClearTasks ();
      player->ClearSearchNodes ();
      player->Task ()->data = positions_[targets[i]].waypoint;
    }
  }
  if (phase_changed && !opening) {
    if (phase == CTPhase::Crunch) { TeamChat (ystl::strings.format ("Caller: crunch %s with %d. Stop at the choke, fall back to setup once quiet.", CrunchName (), crunch_count_)); }
    else if (crunch_returning_) { /* The return call is sent by UpdateCrunch. */ }
    else if (phase == CTPhase::TRecoverBomb) { TeamChat ("Caller: bomb down. Recover the bomb first, everyone cover the recovery."); }
    else if (phase == CTPhase::GuardBomb) { TeamChat ("Caller: dropped bomb secured. Hold angles on the bomb, make them come to us."); }
    else if (phase == CTPhase::Retake) {
      TeamChat (ystl::strings.format ("Caller: bomb %c (%s). Everyone retake %c. %s defuses, others cover.", site, plant_source_, site,
        defuser_ >= 0 ? identities_[defuser_].id : "closest teammate"));
    }
    else if (pivot) { TeamChat (ystl::strings.format ("Caller: change of plan. Switch to %c. %s", site, phase == CTPhase::TExecute ? "Execute NOW." : "Get ready outside site, keep execute time.")); }
    else if (early || (t_early_ && phase == CTPhase::TExecute)) { TeamChat (ystl::strings.format ("Caller: player advantage. Execute %c NOW!", site)); }
    else if (phase == CTPhase::TExecute) { TeamChat (ystl::strings.format ("Caller: execute %c NOW. Everyone move in. Fake rejoins attack.", site)); }
    else if (phase != CTPhase::TClear) { TeamChat (ystl::strings.format ("Caller: %s, target %c.", CTPhaseName (phase), site)); }
  }
  if (opening) { next_chat_ = game.Time (); }
  next_telemetry_ = game.Time ();
  return true;
}

void SystemOne::ApplyFallback () {
  pending_ = false;
  if (required_.phase == CTPhase::GuardBomb || required_.phase == CTPhase::TRecoverBomb) {
    if (objective_base_ < 0 || (required_.phase == CTPhase::TRecoverBomb && collector_ < 0)) { return; }
    int targets[5] {}, next = 1;
    for (int i = 0; i < 5; ++i) { targets[i] = objective_base_ + (required_.phase == CTPhase::TRecoverBomb && i == collector_ ? 0 : (required_.phase == CTPhase::TRecoverBomb ? next++ : i)); }
    Apply (targets, "objective_fallback", required_.phase, required_.site, 0, team_ == Team::Terrorist ? attack_delay_ : 0);
    return;
  }
  if (team_ == Team::Terrorist) {
    const char site = t_locked_ ? t_target_ : (round_ % 2 == 0 ? 'A' : 'B');
    const int timing = t_locked_ ? attack_delay_ : 0;
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
  if (applied_ && ordered_.phase != CTPhase::GuardBomb && (required_.phase == CTPhase::Gather || required_.phase == CTPhase::LocateBomb ||
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
  if (team_ == Team::CT && required_.phase == CTPhase::LocateBomb && game_state.IsBombPlanted ()) { return -1; }
  if (ordered_.phase == CTPhase::Retake && identity == defuser_ && game_state.IsBombPlanted () && !shared_plant_origin_.empty ()) {
    return graph.GetNearest (shared_plant_origin_, 512.0f);
  }
  if (team_ == Team::Terrorist && ordered_.phase == CTPhase::TExecute && attack_delay_ == 0 && !rush_passed_[identity]) {
    const int gate = RushGate (); if (graph.Exists (gate)) { return gate; }
  }
  if (team_ == Team::Terrorist && ordered_.phase == CTPhase::TExecute) {
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
  if (Defusing (bot->index_) || (ordered_.phase == CTPhase::TRecoverBomb && IdentityFor (bot->index_) == collector_)) { return false; }
  if (team_ == Team::Terrorist && ordered_.phase >= CTPhase::TExecute && bot->has_c4_) { return false; }
  if (Goal (bot->index_) == -1) { return false; }
  const auto &identity = identities_[IdentityFor (bot->index_)];
  const auto &position = positions_[identity.position];
  const auto &anchor = graph[position.waypoint].origin;
  const float distance = bot->pev->origin.distance_sq (anchor);
  if (identity.hold_position != identity.position) {
    if (distance > ystl::sqrf (position.radius) || !util.IsVisible (anchor, bot->Ent ())) { return false; }
    identity.hold_position = identity.position;
    identity.hold_phase = 0;
    identity.hold_target = anchor;
    identity.hold_next = game.Time () + ystl::rg (0.7f, 1.8f);
  }
  // Keep local movement bounded to the assigned area; navigation recovers displacement.
  if (distance > ystl::sqrf (position.radius + 64.0f) || !bot->IsOnFloor () || bot->IsOnLadder ()) {
    identity.hold_position = -1;
    return false;
  }
  auto direction = (position.look_at - anchor).normalize2d ();
  const ystl::Vector side (-direction.y, direction.x, 0.0f);
  const auto clear_step = [&] (const ystl::Vector &target) {
    Trace::Result body {}, ground {};
    trace.Hull (bot->pev->origin, target, TraceIgnore::None, bot->IsDucking () ? head_hull : human_hull, bot->Ent (), &body);
    if (body.start_solid || body.fraction < 1.0f) { return false; }
    trace.Line (target + ystl::Vector (0.0f, 0.0f, 8.0f), target - ystl::Vector (0.0f, 0.0f, 64.0f), TraceIgnore::Monsters, bot->Ent (), &ground);
    const float standing_height = bot->IsDucking () ? 18.0f : 36.0f;
    return !ground.start_solid && ground.fraction < 1.0f && ground.plane_normal.z >= 0.7f &&
      ystl::abs (ground.end_pos.z + standing_height - bot->pev->origin.z) <= 18.0f;
  };
  if (game.Time () >= game_state.GetRoundStartTime ()) {
    if (identity.hold_phase == 0 && game.Time () >= identity.hold_next) {
      // Brief shoulder peeks, with pauses between them, instead of an endless strafe.
      const float width = ystl::rg (24.0f, 48.0f);
      const float sign = ystl::rg (0, 1) == 0 ? -1.0f : 1.0f;
      const auto candidate = anchor + side * width * sign;
      const auto other = anchor - side * width * sign;
      if (clear_step (candidate) || clear_step (other)) {
        identity.hold_target = clear_step (candidate) ? candidate : other;
        identity.hold_phase = 1;
        identity.hold_until = game.Time () + 1.1f;
      }
      else { identity.hold_next = game.Time () + ystl::rg (0.7f, 1.4f); }
    }
    if (identity.hold_phase == 1 && (game.Time () >= identity.hold_until || bot->pev->origin.distance_sq (identity.hold_target) < ystl::sqrf (8.0f))) {
      identity.hold_phase = 2;
      identity.hold_target = anchor;
    }
    if (identity.hold_phase == 2 && distance < ystl::sqrf (8.0f)) {
      identity.hold_phase = 0;
      identity.hold_next = game.Time () + ystl::rg (0.9f, 2.4f);
    }
  }
  bot->move_to_goal_ = bot->check_terrain_ = false;
  bot->move_speed_ = bot->strafe_speed_ = 0.0f;
  bot->look_at_safe_ = position.look_at;
  if (ordered_.phase == CTPhase::Crunch && CrunchGroup (identity.position) == crunch_choke_) {
    bot->look_at_safe_ = crunch_choke_ == 'L' ? ystl::Vector (570.0f,455.0f,48.0f) : (crunch_choke_ == 'S' ? ystl::Vector (-167.0f,965.0f,48.0f) :
      (crunch_choke_ == 'M' ? ystl::Vector (-500.0f,500.0f,48.0f) : ystl::Vector (-1700.0f,500.0f,48.0f)));
  }
  bot->aim_flags_ |= AimFlags::Camp;
  if (game.Time () >= game_state.GetRoundStartTime () && identity.hold_phase != 0 && clear_step (identity.hold_target)) {
    const float remaining = bot->pev->origin.distance2d (identity.hold_target);
    if (remaining > 6.0f) {
      bot->dest_origin_ = identity.hold_target;
      bot->move_speed_ = ystl::min (bot->GetShiftSpeed (), ystl::clamp (remaining * 5.0f, 35.0f, 110.0f));
    }
  }
  if (position.crouch) { bot->pev->button |= IN_DUCK; }
  return true;
}

void SystemOne::PrintStatus () const {
  ctrl.Msg ("Jev %s: round=%d request=%d revision=%d pending=%d poll=%.1fs", team_ == Team::CT ? "CT" : "T", round_, request_, roster_revision_, pending_ ? 1 : 0, cv_systemone_poll_interval.As<float> ());
  ctrl.Msg ("Economy: %s ready=%d average=%d known=%d score CT=%d T=%d MR12_last=%d", EconomyName (economy_mode_), economy_ready_ ? 1 : 0, economy_average_, economy_known_, ct_score_, t_score_, competitive_match.Rules ().MustBuy () ? 1 : 0);
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
  evidence.crunch_active = crunch_active_; evidence.crunch_choke = crunch_choke_;
  evidence.secured_drop = team_ == Team::CT && drop_known_ && objective_base_ >= 0;
  evidence.dropped_site = objective_base_ >= 0 ? positions_[objective_base_].site : 'M';
  evidence.planted = game_state.IsBombPlanted ();
  evidence.planted_site = evidence.planted ? (team_ == Team::CT ? planted_info_.Site () : SiteFor (game_state.GetBombOrigin (), true)) : '-';
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
    drop_known_ = false;
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
  if (team_ == Team::Terrorist && event.victim_team == 'C' && event.killer_team == 'T') { t_pick_time_ = game.Time (); }
  if (victim_team == Team::CT) { ++ct_deaths_; }
  else { ++t_deaths_; }
  if (victim_team == team_) { event.origin = victim->v.origin; event.known_origin = true; } // A public death notification does not reveal a T's location.
  for (auto &contact : sightings_) {
    if (contact.userid == event.victim) { contact.active = false; }
  }
  ++event_revision_;
}

bool SystemOne::CrunchAvailable () const {
  const auto &match = competitive_match.Rules ();
  if (team_ != Team::CT || crunch_used_ || required_.phase != CTPhase::Gather || game_state.IsBombPlanted () || drop_known_ ||
      game.Time () < game_state.GetRoundStartTime () || game.Time () - game_state.GetRoundStartTime () > 20.0f ||
      !CrunchMixupAllowed (match.a + match.b + 1, match.SideScore (true), match.SideScore (false), last_crunch_round_)) { return false; }
  for (int i = 0; i < 5; ++i) { auto *entity = Entity (i); if (entity == nullptr || !game.IsAliveEntity (entity)) { return false; } }
  return true;
}

char SystemOne::CrunchGroup (int position) const {
  if (position < 0 || position >= position_count_) { return '-'; }
  const auto id = ystl::StringRef (positions_[position].id);
  if (id == "long_anchor" || id == "long_crossfire" || id == "t_a_2_entry" || id == "t_a_2_awper") { return 'L'; }
  if (id == "short_anchor" || id == "short_crossfire" || id.starts_with ("crunch_short_")) { return 'S'; }
  if (id.starts_with ("crunch_mid_")) { return 'M'; }
  if (id == "b_anchor" || id == "t_b_2_entry" || id == "t_b_1_entry" || id.starts_with ("crunch_tunnels_")) { return 'B'; }
  return '-';
}

const char *SystemOne::CrunchName () const {
  return crunch_choke_ == 'L' ? "Long" : (crunch_choke_ == 'S' ? "Short" : (crunch_choke_ == 'M' ? "Mid" : "B tunnels"));
}

void SystemOne::UpdateCrunch () {
  if (team_ != Team::CT || !crunch_active_) { return; }
  if (game_state.IsBombPlanted () || drop_known_) { crunch_active_ = false; return; }
  int alive = 0, arrived = 0; bool contact = false;
  for (int i = 0; i < 5; ++i) {
    const int position = identities_[i].position;
    if (CrunchGroup (position) != crunch_choke_) { continue; }
    auto *entity = Entity (i);
    if (entity == nullptr || !game.IsAliveEntity (entity)) { continue; }
    ++alive;
    if (entity->v.origin.distance_sq (positions_[position].origin) < ystl::sqrf (160.0f)) { ++arrived; }
    for (const auto &sighting : sightings_) {
      if (sighting.active && game.Time () - sighting.time <= 4.0f && sighting.origin.distance_sq2d (positions_[position].origin) < ystl::sqrf (900.0f)) { contact = true; }
    }
  }
  const float elapsed = game.Time () - crunch_started_;
  if (alive >= 2 && (contact || (elapsed < 20.0f && (arrived != alive || elapsed < 6.0f)))) { return; }
  crunch_active_ = false; crunch_returning_ = true; required_ = { CTPhase::Gather, '-' };
  pending_ = false;
  TeamChat (ystl::strings.format ("Caller: %s %s. Fall back to your original positions.", CrunchName (), contact ? "crunch over" : "quiet"));
  Apply (crunch_home_, "crunch_return", CTPhase::Gather, '-', 0);
  crunch_returning_ = false; next_poll_ = game.Time ();
}

bool SystemOne::BuildObjectivePositions (const ystl::Vector &origin) {
  Position planned[5] {}; int used[5] {};
  for (int slot = 0; slot < 5; ++slot) {
    int best = -1; float score = kInfiniteDistance;
    for (const auto &node : graph) {
      if (has_flag (node.flags, NodeFlag::Ladder) || ystl::abs (node.origin.z - origin.z) > 96.0f) { continue; }
      const float distance = node.origin.distance_sq2d (origin);
      if (distance > ystl::sqrf (768.0f) || ((team_ == Team::CT || slot > 0) && distance < ystl::sqrf (128.0f))) { continue; }
      bool close = false;
      for (int prior = 0; prior < slot; ++prior) { close |= node.number == used[prior] || node.origin.distance_sq2d (graph[used[prior]].origin) < ystl::sqrf (64.0f); }
      if (!close && distance < score) { score = distance; best = node.number; }
    }
    if (best < 0) { return false; }
    used[slot] = best;
    auto &position = planned[slot];
    snprintf (position.id, sizeof (position.id), "%s_bomb_%d", team_ == Team::CT ? "guard" : "recover", slot);
    const char site = SiteFor (origin);
    position.site = site == 'A' || site == 'B' ? site : 'M';
    position.zone = team_ == Team::CT ? 'G' : 'R'; position.stage = -1; position.waypoint = best;
    position.origin = graph[best].origin; position.look_at = origin;
    if (team_ == Team::CT || slot > 0) { position.look_at = position.origin + (position.origin - origin).normalize2d () * 600.0f; }
    position.radius = 32.0f;
    snprintf (position.preferred_roles, sizeof (position.preferred_roles), "%s", team_ == Team::Terrorist && slot == 0 ? "support" : "support,entry,lurker,awper");
  }
  if (objective_base_ < 0) {
    if (position_count_ + 5 > 64) { return false; }
    objective_base_ = position_count_; position_count_ += 5;
  }
  for (int i = 0; i < 5; ++i) { positions_[objective_base_ + i] = planned[i]; }
  pending_ = false; ++roster_revision_; next_poll_ = game.Time ();
  return true;
}

int SystemOne::RushGate () const {
  const char *id = t_target_ == 'A' ? "t_a_2_entry" : "t_b_2_entry";
  for (int i = 0; i < position_count_; ++i) { if (ystl::StringRef (positions_[i].id) == id) { return positions_[i].waypoint; } }
  return -1;
}

bool SystemOne::CanRecover (int index) const {
  const int identity = IdentityFor (index);
  return !cv_systemone || team_ != Team::Terrorist || identity < 0 || ordered_.phase != CTPhase::TRecoverBomb || identity == collector_;
}

bool SystemOne::HoldCarrier (Bot *player) const {
  if (!cv_systemone || team_ != Team::Terrorist || !player->has_c4_ || !applied_ || game.Time () < game_state.GetRoundStartTime () ||
      player->in_bomb_zone_ || ordered_.phase == CTPhase::TPostPlant || ordered_.phase == CTPhase::TRecoverBomb) { return false; }
  ystl::Vector target {};
  for (int i = 0; i < position_count_; ++i) { if (positions_[i].zone == t_target_) { target = positions_[i].origin; break; } }
  const float own = player->pev->origin.distance2d (target);
  float teammate = kInfiniteDistance; int friends = 0;
  for (int i = 0; i < 5; ++i) {
    auto *entity = Entity (i);
    if (entity != nullptr && entity != player->Ent () && game.IsAliveEntity (entity)) { ++friends; teammate = ystl::min (teammate, entity->v.origin.distance2d (target)); }
  }
  if (friends == 0 || own >= teammate + 96.0f) { return false; }
  player->move_to_goal_ = player->check_terrain_ = false;
  player->move_speed_ = player->strafe_speed_ = 0.0f;
  player->look_at_safe_ = target; player->aim_flags_ |= AimFlags::Camp;
  return true;
}

void SystemOne::UpdateBombObjective () {
  if (game_state.IsBombPlanted ()) { drop_known_ = false; return; }
  if (team_ == Team::Terrorist) {
    const int gate = RushGate ();
    for (int i = 0; i < 5; ++i) {
      auto *player = Player (i); auto *entity = Entity (i);
      if (entity != nullptr && graph.Exists (gate) && entity->v.origin.distance_sq2d (graph[gate].origin) < ystl::sqrf (256.0f)) { rush_passed_[i] = true; }
      if (player == nullptr) { continue; }
      if (player->has_c4_) {
        player->personality_ = Personality::Careful;
        player->agression_level_ = ystl::min (player->agression_level_, 0.2f);
        player->fear_level_ = ystl::max (player->fear_level_, 0.8f);
        identities_[i].carrier_tuned = true;
        if (game.Time () < game_state.GetRoundStartTime () && game.Time () >= next_c4_handoff_ &&
            (player->pev->weapons & (1 << static_cast<int> (Weapon::C4))) != 0 && ystl::StringRef (identities_[i].role) != "support") {
          for (int support = 0; support < 5; ++support) {
            auto *recipient = Entity (support);
            if (recipient == nullptr || !game.IsAliveEntity (recipient) || ystl::StringRef (identities_[support].role) != "support" ||
                recipient->v.origin.distance_sq (player->pev->origin) > ystl::sqrf (1024.0f)) { continue; }
            next_c4_handoff_ = game.Time () + 1.0f;
            player->SelectWeaponById (Weapon::C4); player->DropCurrentWeapon ();
            game.SearchEntities ("classname", "weaponbox", [&] (edict_t *box) {
              if (game.IsEntityModelMatches (box, "backpack.mdl")) { box->v.flags |= FL_ONGROUND; MDLL_Touch (box, recipient); return EntitySearchResult::Break; }
              return EntitySearchResult::Continue;
            });
            if ((recipient->v.weapons & (1 << static_cast<int> (Weapon::C4))) != 0) {
              player->has_c4_ = false;
              if (auto *recipient_bot = bots[recipient]; recipient_bot != nullptr) { recipient_bot->has_c4_ = true; }
            }
            game.Print ("[Jev] C4 handoff from=%s support=%s", identities_[i].id, identities_[support].id); break;
          }
        }
      }
      else if (identities_[i].carrier_tuned) { player->personality_ = static_cast<Personality> (identities_[i].personality); identities_[i].carrier_tuned = false; }
    }
  }
  if (game.Time () < next_objective_scan_) { return; }
  next_objective_scan_ = game.Time () + 0.2f;
  edict_t *box = nullptr;
  game.SearchEntities ("classname", "weaponbox", [&] (edict_t *entity) {
    if (game.IsEntityModelMatches (entity, "backpack.mdl")) { box = entity; return EntitySearchResult::Break; }
    return EntitySearchResult::Continue;
  });
  if (team_ == Team::Terrorist) {
    for (const auto &client : clients) {
      if (!client.IsUsed () || !game.IsAliveEntity (client.ent) || game.GetPlayerTeam (client.ent) != Team::Terrorist) { continue; }
      auto *player = bots[client.ent];
      if ((client.ent->v.weapons & (1 << static_cast<int> (Weapon::C4))) != 0 || (player != nullptr && player->has_c4_)) {
        if (drop_known_) { TeamChat (ystl::strings.format ("Caller: bomb recovered. Return to attack %c.", t_target_)); }
        drop_known_ = false; objective_box_ = nullptr; return;
      }
    }
    if (box == nullptr) { return; }
  }
  else {
    bool observed = false;
    const auto target = drop_known_ ? objective_origin_ : (box != nullptr ? game.GetEntityOrigin (box) : objective_origin_);
    for (int i = 0; i < 5; ++i) {
      auto *player = Player (i);
      if (player == nullptr || !game.IsAliveEntity (player->Ent ())) { continue; }
      if (player->pev->origin.distance_sq (target) <= ystl::sqrf (1600.0f) && player->IsInViewCone (target) && util.IsVisible (target, player->Ent ())) { observed = true; break; }
    }
    if (!observed) { return; }
    if (box == nullptr || (drop_known_ && game.GetEntityOrigin (box).distance_sq (objective_origin_) > ystl::sqrf (64.0f))) { drop_known_ = false; return; }
  }
  const auto origin = game.GetEntityOrigin (box);
  if (!drop_known_ || objective_origin_.distance_sq (origin) > ystl::sqrf (24.0f)) {
    if (!BuildObjectivePositions (origin)) { return; }
    objective_origin_ = origin;
  }
  drop_known_ = true; objective_box_ = box; objective_serial_ = box->serialnumber;
  if (team_ == Team::Terrorist) {
    auto *current = collector_ >= 0 ? Entity (collector_) : nullptr;
    if (current == nullptr || !game.IsAliveEntity (current)) {
      collector_ = -1; float best = kInfiniteDistance;
      for (int i = 0; i < 5; ++i) {
        auto *entity = Entity (i);
        if (entity == nullptr || !game.IsAliveEntity (entity)) { continue; }
        const float score = entity->v.origin.distance2d (origin) + (ystl::StringRef (identities_[i].role) == "support" ? 0.0f : 4096.0f) + (identities_[i].human ? 8192.0f : 0.0f);
        if (score < best) { best = score; collector_ = i; }
      }
      pending_ = false; next_poll_ = game.Time ();
    }
    auto *collector = collector_ >= 0 ? Player (collector_) : nullptr;
    if (collector != nullptr && ordered_.phase == CTPhase::TRecoverBomb) {
      collector->pickup_item_ = box; collector->pickup_type_ = Pickup::DroppedC4;
      collector->StartTask (TaskId::PickupItem, 80.0f, kInvalidNodeIndex, 0.0f, true);
    }
  }
}

bool SystemOne::CanDefuse (int index) const {
  const int identity = IdentityFor (index);
  return !cv_systemone || team_ != Team::CT || identity < 0 || !applied_ || ordered_.phase != CTPhase::Retake || identity == defuser_;
}

bool SystemOne::Defusing (int index) const {
  return cv_systemone && team_ == Team::CT && applied_ && ordered_.phase == CTPhase::Retake &&
    game_state.IsBombPlanted () && !shared_plant_origin_.empty () && IdentityFor (index) == defuser_;
}

void SystemOne::ChooseDefuser (char site) {
  auto *current = defuser_ >= 0 ? Player (defuser_) : nullptr;
  if (current != nullptr && game.IsAliveEntity (current->Ent ())) { return; }
  ystl::Vector target = shared_plant_origin_;
  if (target.empty ()) {
    for (int i = 0; i < position_count_; ++i) { if (positions_[i].zone == site) { target = positions_[i].origin; break; } }
  }
  const int previous = defuser_; defuser_ = -1;
  float best = kInfiniteDistance;
  for (int i = 0; i < 5; ++i) {
    auto *player = Player (i);
    if (player == nullptr || !game.IsAliveEntity (player->Ent ())) { continue; }
    const float score = player->GetTaskId () == TaskId::DefuseBomb ? -1.0f : player->pev->origin.distance (target) / 240.0f + (player->has_defuser_ ? 5.0f : 10.0f);
    if (score < best) { best = score; defuser_ = i; }
  }
  if (previous >= 0 && defuser_ >= 0) {
    TeamChat (ystl::strings.format ("Caller: %s takes defuse on %c. Others cover.", identities_[defuser_].id, site));
  }
}

void SystemOne::UpdateRetakeKnowledge () {
  if (team_ != Team::CT || !game_state.IsBombPlanted ()) { return; }
  const auto &bomb = game_state.GetBombOrigin ();
  if (bomb.empty ()) { return; }
  const char previous = planted_info_.Site ();
  for (int i = 0; i < 5; ++i) {
    auto *player = Player (i);
    if (player == nullptr || !game.IsAliveEntity (player->Ent ())) { continue; }
    const bool heard = !player->IsBombAudible ().empty ();
    const bool seen = player->pev->origin.distance_sq (bomb) < ystl::sqrf (2048.0f) && player->IsInViewCone (bomb) && util.IsVisible (bomb, player->Ent ());
    if (heard || seen) {
      const char site = SiteFor (bomb, true);
      if (site != 'A' && site != 'B') { continue; }
      planted_info_.confirmed_site = site;
      shared_plant_origin_ = bomb;
      plant_source_ = heard ? "heard by teammate" : "seen by teammate";
      plant_reporter_ = i;
      break;
    }
    if (planted_info_.confirmed_site != '-' || game.Time () - game_state.GetTimeBombPlanted () < 1.0f) { continue; }
    for (const char site : { 'A', 'B' }) {
      bool covered = true, nearby = false; int goals = 0;
      for (const auto node : graph.GetPoints (PointType::Goal)) {
        if (SiteFor (graph[node].origin, true) != site) { continue; }
        ++goals;
        const float distance = player->pev->origin.distance_sq2d (graph[node].origin);
        covered &= distance < ystl::sqrf (kBombHearDistance - 128.0f);
        nearby |= distance < ystl::sqrf (350.0f);
      }
      if (goals > 0 && covered && nearby) {
        if (site == 'A') { planted_info_.cleared_a = true; } else { planted_info_.cleared_b = true; }
        plant_source_ = site == 'A' ? "A cleared by teammate" : "B cleared by teammate";
        plant_reporter_ = i;
      }
    }
  }
  if (planted_info_.Site () != '-') {
    for (auto &player : bots) {
      if (player.team_ != Team::CT) { continue; }
      player.bomb_search_overridden_ = true;
      if (!shared_plant_origin_.empty ()) { player.bomb_known_ = true; }
      if (ordered_.phase == CTPhase::Retake && !CanDefuse (player.index_) && player.GetTaskId () == TaskId::PickupItem && player.pickup_type_ == Pickup::PlantedC4) {
        player.pickup_item_ = nullptr; player.CompleteTask ();
      }
    }
    if (!shared_plant_origin_.empty ()) { bots.SetBombKnown (Team::CT); }
  }
  if (previous != planted_info_.Site ()) {
    game.Print ("[Jev] Shared plant site=%c source=%s reporter=%s", planted_info_.Site (), plant_source_, plant_reporter_ >= 0 ? identities_[plant_reporter_].id : "none");
  }
  if (ordered_.phase == CTPhase::Retake) { ChooseDefuser (planted_info_.Site ()); }
}

bool SystemOne::CanExecuteEarly () const {
  int ct = 0, ts = 0, deaths = 0;
  for (const auto &client : clients) {
    if (!client.IsUsed () || !game.IsAliveEntity (client.ent)) { continue; }
    if (game.GetPlayerTeam (client.ent) == Team::CT) { ++ct; }
    else if (game.GetPlayerTeam (client.ent) == Team::Terrorist) { ++ts; }
  }
  const int count = ystl::min (event_count_, 32);
  for (int i = 0; i < count; ++i) { const auto &event = events_[(event_count_ - count + i) % 32]; if (event.victim_team == 'C' && game.Time () - event.time <= 12.0f) { ++deaths; } }
  return TCanExecuteEarly (ct, ts, deaths, static_cast<int> (required_.phase), game.Time () >= game_state.GetRoundStartTime ());
}

bool SystemOne::CanRetarget () const {
  int alive = 0, deaths = 0;
  for (const auto &client : clients) { if (client.IsUsed () && game.IsAliveEntity (client.ent) && game.GetPlayerTeam (client.ent) == Team::Terrorist) { ++alive; } }
  const int count = ystl::min (event_count_, 32);
  for (int i = 0; i < count; ++i) { const auto &event = events_[(event_count_ - count + i) % 32]; if (event.victim_team == 'T' && game.Time () - event.time <= 12.0f) { ++deaths; } }
  return TCanRetarget (t_retargeted_, deaths, alive, static_cast<int> (required_.phase), game_state.IsBombPlanted ());
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
  if (team_ != Team::Terrorist || !applied_ || ordered_.phase != CTPhase::TExecute) { return false; }
  const int identity = IdentityFor (index);
  auto *player = identity >= 0 ? Player (identity) : nullptr;
  return player != nullptr && player->has_c4_;
}

bool SystemOne::PlantAllowed (int index) const {
  if (team_ != Team::Terrorist || IdentityFor (index) < 0) { return true; }
  return t_locked_ && !drop_known_ && game.Time () >= attack_time_;
}

CTDecision SystemOne::TerroristState () {
  if (!t_locked_) { return { CTPhase::TCall, '-' }; }
  if (drop_known_ && objective_base_ >= 0 && !game_state.IsBombPlanted ()) { return { CTPhase::TRecoverBomb, t_target_ }; }
  if (game_state.IsBombPlanted ()) {
    const char site = SiteFor (game_state.GetBombOrigin (), true);
    if (site == 'A' || site == 'B') { t_target_ = site; }
    return { CTPhase::TPostPlant, t_target_ };
  }
  if (game.Time () >= game_state.GetRoundStartTime () && game.Time () - t_pick_time_ <= 12.0f && attack_time_ > game.Time ()) {
    attack_time_ = game.Time (); t_early_ = true;
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
  if (applied_) { PrepareEconomy (); MaintainDrops (); }
  UpdateBombObjective ();
  UpdateRetakeKnowledge ();
  UpdateCrunch ();
  const int previous_stage = t_stage_;
  const auto decision = team_ == Team::CT ? EvaluateCTState (Evidence ()) : TerroristState ();
  if (decision.phase != required_.phase || decision.site != required_.site || previous_stage != t_stage_) {
    required_ = decision;
    pending_ = false;
    next_poll_ = game.Time ();
    game.Print ("[Jev] %s state phase=%s target=%c", team_ == Team::CT ? "CT" : "T", CTPhaseName (required_.phase), required_.site);
    if ((required_.phase == CTPhase::Commit || required_.phase == CTPhase::Retake || required_.phase == CTPhase::TExecute || required_.phase == CTPhase::TPostPlant || required_.phase == CTPhase::TRecoverBomb || required_.phase == CTPhase::GuardBomb ||
        ordered_.phase == CTPhase::TRecoverBomb || ordered_.phase == CTPhase::GuardBomb || previous_stage != t_stage_) &&
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
          if (phase >= 0 && phase <= 12 && Apply (targets, "jev", static_cast<CTPhase> (phase), site, formation, timing)) {
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
