// SPDX-License-Identifier: Unlicense
#pragma once
#include "ctstrategy.h"
#include "economy_policy.h"
#include "match_policy.h"
#include "tstrategy.h"

namespace bot {
class CompetitiveMatch final : public ystl::Singleton<CompetitiveMatch> {
  MatchRules rules_ {};
  MatchAction pending_ { MatchAction::None };
  bool winner_seen_ {}, internal_restart_ {}, publishing_ {}, score_dirty_ {};
  int raw_ct_ {}, raw_t_ {};
  float ignore_zero_until_ {};
public:
  const MatchRules &Rules () const { return rules_; }
  void NewMatch ();
  void NewRound ();
  void Win (Team team);
  void ObserveScore (ystl::StringRef team, int score);
  void Frame ();
  bool InternalRestart () const { return internal_restart_; }
  bool Publishing () const { return publishing_; }
};
YSTL_EXPOSE_GLOBAL_SINGLETON (CompetitiveMatch, competitive_match);

// Stable identities are independent of temporary player slots/userids.
class SystemOne {
  Team team_;
  bool economy_ready_ {}, score_known_ {};
  int ct_score_ {}, t_score_ {}, economy_average_ {}, economy_known_ {};
  EconomyMode economy_mode_ { EconomyMode::Save };
  Weapon buy_weapon_[5] {};
  int buy_cash_[5] {}, buy_cap_[5] {}, wallet_[33] {}, wallet_userid_[33] {};
  bool donated_[5] {}, received_[5] {};
  edict_t *drop_box_[5] {};
  int drop_serial_[5] {};
  void PrepareEconomy ();
  void MaintainDrops ();
  int MoneyFor (int identity) const;
  Weapon ChooseBuyWeapon (int identity, Bot *player) const;
  struct Identity {
    char id[32] {}, bot_id[32] {}, name[32] {}, role[16] {};
    int slot {}, userid {}, generation {}, position { -1 };
    int difficulty { 2 }, personality {};
    bool announced {}, human {}, preferred_caller {}, carrier_tuned {};
    mutable int hold_position { -1 }, hold_phase {};
    mutable float hold_next {}, hold_until {};
    mutable ystl::Vector hold_target {};
  };
  struct Position {
    char id[32] {}, preferred_roles[64] {};
    char site {}, zone {};
    ystl::Vector origin {}, look_at {};
    float radius { 40.0f };
    int waypoint { -1 }, stage { -1 };
    bool crouch {};
  };
  Identity identities_[5] {};
  Position positions_[64] {};
  int position_count_ {}, round_ {}, request_ {}, nonce_ {}, roster_revision_ {};
  bool config_loaded_ {}, config_attempted_ {}, pending_ {}, applied_ {};
  bool pending_opening_ {};
  float deadline_ {}, next_send_ {}, next_poll_ {}, next_telemetry_ {}, next_chat_ {}, round_time_ {};
  uintptr_t socket_ { static_cast<uintptr_t> (-1) };
  ystl::String pending_state_ {};
  struct Sighting {
    int userid {};
    float time {};
    ystl::Vector origin {};
    char site { '-' };
    bool active {};
  };
  struct RoundEvent {
    int id {}, killer {}, victim {};
    char killer_team { '-' }, victim_team { '-' };
    float time {};
    ystl::Vector origin {};
    bool known_origin {};
  };
  Sighting sightings_[8] {};
  RoundEvent events_[32] {};
  int event_count_ {}, ct_deaths_ {}, t_deaths_ {}, event_revision_ {};
  float bomb_seen_time_ { -100.0f };
  ystl::Vector bomb_seen_origin_ {};
  char bomb_seen_site_ { '-' };
  CTDecision required_ {}, ordered_ {};
  int formation_ {}, defuser_ { -1 };
  CTBombKnowledge planted_info_ {};
  ystl::Vector shared_plant_origin_ {};
  const char *plant_source_ { "unknown" };
  int plant_reporter_ { -1 };
  void UpdateRetakeKnowledge ();
  void ChooseDefuser (char site);
  bool t_locked_ {}, t_compromised_ {}, fake_chosen_ {}, t_retargeted_ {}, t_early_ {};
  bool crunch_active_ {}, crunch_used_ {}, crunch_returning_ {};
  char crunch_choke_ { '-' };
  int crunch_home_[5] {}, crunch_count_ {};
  float crunch_started_ {};
  int last_crunch_round_ { -100 };
  bool CrunchAvailable () const;
  char CrunchGroup (int position) const;
  const char *CrunchName () const;
  void UpdateCrunch ();
  bool drop_known_ {};
  ystl::Vector objective_origin_ {};
  edict_t *objective_box_ {};
  int objective_serial_ {}, objective_base_ { -1 }, collector_ { -1 };
  float next_objective_scan_ {}, next_c4_handoff_ {}, t_pick_time_ { -100.0f };
  bool rush_passed_[5] {};
  void UpdateBombObjective ();
  bool BuildObjectivePositions (const ystl::Vector &origin);
  int RushGate () const;
  bool CanExecuteEarly () const;
  bool CanRetarget () const;
  int fake_member_ { -1 }, last_caller_ { -2 };
  int FakeMember () const;
  int FakePosition (char site) const;
  const char *PositionLabel (int position) const;
  int attack_delay_ { -1 }, t_stage_ {};
  float attack_time_ {}, stage_ready_time_ {}, next_roster_maintain_ {};
  char t_target_ { '-' };
  edict_t *Entity (int identity) const;
  void MaintainRoster ();
  int Caller () const;
  void TeamChat (ystl::StringRef message);
  CTDecision TerroristState ();
  CTEvidence Evidence () const;
  char SiteFor (const ystl::Vector &origin, bool planted = false) const;
  bool Open ();
  bool LoadConfig ();
  bool SyncRoster ();
  Bot *Player (int identity) const;
  int IdentityFor (int index) const;
  ystl::String BuildState (bool telemetry);
  void Send (ystl::StringRef payload);
  void BeginRequest ();
  bool Apply (const int *targets, const char *source, CTPhase phase, char site, int formation, int timing = 0);
  void ApplyFallback ();
public:
  explicit SystemOne (Team team) : team_ (team) {}
  ~SystemOne ();
  void NewRound ();
  bool HandleBuy (Bot *player);
  void ObserveMoney (edict_t *entity, int amount);
  void SetScore (ystl::StringRef team, int score);
  void Frame ();
  int Goal (int index) const;
  bool HoldPosition (Bot *bot) const;
  bool Cautious (int index) const;
  bool GunReady (int index) const;
  bool Planting (int index) const;
  bool Defusing (int index) const;
  bool CanDefuse (int index) const;
  bool PlantAllowed (int index) const;
  bool HoldCarrier (Bot *player) const;
  bool CanRecover (int index) const;
  void ObserveEnemy (Bot *observer, edict_t *enemy, const ystl::Vector &origin);
  void RecordDeath (edict_t *killer, edict_t *victim);
  void PrintStatus () const;
  bool AssignNamed (ystl::StringRef identity, ystl::StringRef position);
};
class CTDirector final : public SystemOne, public ystl::Singleton<CTDirector> {
public: CTDirector () : SystemOne (Team::CT) {}
};
class TDirector final : public SystemOne, public ystl::Singleton<TDirector> {
public: TDirector () : SystemOne (Team::Terrorist) {}
};
YSTL_EXPOSE_GLOBAL_SINGLETON (CTDirector, system_one);
YSTL_EXPOSE_GLOBAL_SINGLETON (TDirector, t_system_one);
}
