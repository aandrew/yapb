// SPDX-License-Identifier: Unlicense
#pragma once
#include "ctstrategy.h"

namespace bot {
// Stable identities are independent of temporary player slots/userids.
class SystemOne {
  Team team_;
  struct Identity {
    char id[32] {}, bot_id[32] {}, name[32] {}, role[16] {};
    int slot {}, userid {}, generation {}, position { -1 };
    int difficulty { 2 }, personality {};
    bool announced {}, human {}, preferred_caller {};
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
  bool t_locked_ {};
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
  void Frame ();
  int Goal (int index) const;
  bool HoldPosition (Bot *bot) const;
  bool Cautious (int index) const;
  bool Planting (int index) const;
  bool PlantAllowed (int index) const;
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
