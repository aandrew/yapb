// SPDX-License-Identifier: Unlicense
#pragma once

namespace bot {
// Stable identities are independent of temporary player slots/userids.
class SystemOne final : public ystl::Singleton<SystemOne> {
  struct Identity {
    char id[32] {}, name[32] {}, role[16] {};
    int slot {}, userid {}, generation {}, position { -1 };
    bool announced {};
  };
  struct Position {
    char id[32] {}, preferred_roles[64] {};
    char site {};
    ystl::Vector origin {}, look_at {};
    float radius { 40.0f };
    int waypoint { -1 };
    bool crouch {};
  };
  Identity identities_[5] {};
  Position positions_[32] {};
  int position_count_ {}, round_ {}, request_ {}, nonce_ {}, roster_revision_ {};
  bool config_loaded_ {}, config_attempted_ {}, pending_ {}, applied_ {};
  bool pending_opening_ {};
  float deadline_ {}, next_send_ {}, next_poll_ {}, next_telemetry_ {}, next_chat_ {}, round_time_ {};
  uintptr_t socket_ { static_cast<uintptr_t> (-1) };
  ystl::String pending_state_ {};
  bool Open ();
  bool LoadConfig ();
  bool SyncRoster ();
  Bot *Player (int identity) const;
  int IdentityFor (int index) const;
  ystl::String BuildState (bool telemetry);
  void Send (ystl::StringRef payload);
  void BeginRequest ();
  bool Apply (const int *targets, const char *source);
  void ApplyFallback ();
public:
  ~SystemOne ();
  void NewRound ();
  void Frame ();
  int Goal (int index) const;
  bool HoldPosition (Bot *bot) const;
  void PrintStatus () const;
  bool AssignNamed (ystl::StringRef identity, ystl::StringRef position);
};
YSTL_EXPOSE_GLOBAL_SINGLETON (SystemOne, system_one);
}
