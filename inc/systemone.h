// SPDX-License-Identifier: Unlicense
#pragma once

namespace bot {
// The engine thread owns all state. Only nonblocking loopback UDP crosses to the bridge.
class SystemOne final : public ystl::Singleton<SystemOne> {
  int request_ {};
  int nonce_ {};
  int roster_[5] {};
  int goals_[5] { -1, -1, -1, -1, -1 };
  bool site_a_[5] {};
  bool announced_[5] {};
  bool pending_ {};
  bool applied_ {};
  bool skipped_ {};
  float deadline_ {};
  float next_send_ {};
  float next_telemetry_ {};
  float next_chat_ {};
  float round_time_ {};
  uintptr_t socket_ { static_cast<uintptr_t> (-1) };
  bool Open ();
  void SendState (bool telemetry);
  void Apply (const int *a, const char *source);
  bool RosterMatches () const;
public:
  ~SystemOne ();
  void NewRound ();
  void Frame ();
  int Goal (int index) const;
};
YSTL_EXPOSE_GLOBAL_SINGLETON (SystemOne, system_one);
}
