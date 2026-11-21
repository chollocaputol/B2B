#pragma once
// Multihop priority gate. Pure decision only: no radio authorization.
// A received SOS stays active until cancellation, but must not starve
// ordinary position forwarding after all urgent relay work has drained.
namespace MultihopPriority {
inline bool mayDrainRoutine(bool localEmergency, bool localSOSRetry,
                           bool localCancelRetry, bool relayedControlQueued,
                           bool macCadActive, bool rxIrqPending) {
  return !localEmergency && !localSOSRetry && !localCancelRetry &&
         !relayedControlQueued && !macCadActive && !rxIrqPending;
}
}
