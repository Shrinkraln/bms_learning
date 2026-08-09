# Task 3-8 Report — CommunicationMonitor (Tasks 3-8 combined)

Date: 2026-08-09
Branch: worktree-device-status (device-status worktree)

## Status: COMPLETE

`CommunicationMonitor` class fully implemented (header + implementation),
wired, built, and behavior-verified with a signal-level self-test.

## Files created

- `model/CommunicationMonitor.h` — exact copy of plan Task 3 (10 Q_PROPERTYs,
  8 private slots, all members default-initialized false/0, no state querying
  from CanWorker)
- `model/CommunicationMonitor.cpp` — implementation per plan Tasks 4-8

## Commits (in order)

| Commit | Message | Files |
|--------|---------|-------|
| 986556f | feat(monitor): add CommunicationMonitor header | CommunicationMonitor.h |
| ca16976 | feat(can): add deviceConnectedChanged signal for hardware state | CanWorker.h/.cpp |
| ffeb755 | fix(model): sync afe_online into m_snap in applySnapshot | BmsDataModel.cpp |
| a451cb2 | feat(monitor): implement CommunicationMonitor — frame counting, timeout, AFE tracking, health summary | CommunicationMonitor.cpp |
| fcfd224 | build: add CommunicationMonitor to CMakeLists.txt | CMakeLists.txt |

Note: ca16976 and ffeb755 are REQUIRED prerequisites discovered during
implementation — the documented interface contract could not compile/work
without them (see Concerns).

## Test summary — PASS (all checks)

A throwaway self-test (`tests/monitor_selftest.cpp`, deleted after run) drove
real CanWorker/BmsDataModel signals through the wired monitor and verified
30+ assertions. Built as a temporary CMake target, ran, then removed.

Results: ALL TESTS PASSED (exit 0). Coverage:

- Initial state: all properties false/0, healthSummary 0
- hardwareConnected via deviceConnectedChanged(true); health 1
- canBusActive via connectionStatusChanged(true); health 2
- totalFrameCount=50 after 10 batches x 5 frames; fps=50.0 after 1s window;
  count not reset; fps drops to 0.0 after silence
- timeoutEventCount=3 after 3x "CAN frame timeout (>500ms)"; non-timeout
  error not counted; lastError relayed
- AFE tristate: first snapshot online (haveAfeInfo set) -> offline
  transition -> afeOfflineSeconds=3 after ~3.4s -> recovery resets to 0
- reconnectCount relayed
- health 3/3 all-green; device loss cascades: hardware/CAN/AFE all false,
  healthSummary back to 0, reconnectCount unchanged, offline seconds reset
- re-plug: hardware true, bus stays inactive, health 1

Full app build: `cmake --build build --target BmsHostApp` — success, zero
errors/warnings ("ninja: no work to do" on final state).

## Deviations / corrections vs plan (all reported)

1. **CanWorker was missing `deviceConnectedChanged`** — the plan (Task 3
   header, Task 8 setCanWorker) and spec data flow both reference it, but
   Task 1 only added `reconnectCountChanged`. Added the signal:
   `emit deviceConnectedChanged(true)` in `start()` after connect,
   `emit deviceConnectedChanged(false)` in `handleDeviceLost()`. Committed
   separately (ca16976). Without this the monitor's hardware layer and
   cascade gray-out could not work.

2. **`BmsDataModel::batteryStatusTextChanged` does not exist** — the real
   signal is `statusTextChanged(const QString&)`. Used the real name in
   `setBmsModel()`.

3. **Plan Task 6 bug: healthSummary stale after cascade** — plan calls
   `updateHealthSummary()` BEFORE the cascade resets canBusActive/afeOnline,
   so after device loss healthSummary would show 2 instead of 0. Fixed by
   re-computing after the cascade (one extra `updateHealthSummary()` call).
   Verified by self-test ("health=0 after cascade").

4. **`afeOnline()` getter was non-functional** — `BmsSnapshot::afe_online`
   defaults to `true` and `applySnapshot()` never wrote `m_snap.afe_online`,
   so the Task 2 getter would always return true (AFE could never report
   offline). Fixed in `applySnapshot()`: sync `m_snap.afe_online` from the
   decoded snapshot before the connStatus branch. Committed (ffeb755).
   Without this fix the AFE offline/offline-duration features are dead.

5. **CMakeLists.txt updated (Task 10 content) ahead of schedule** — required
   for build verification. Committed with the plan's Task 10 message
   (fcfd224). Task 10 is effectively done; the later agent should skip or
   verify.

## Concerns

- `connection_alive` in BmsSnapshot has the same never-synced issue as
  afe_online had, but nothing reads it today — left untouched.
- Monitor's first-AFE-snapshot detection relies on `statusTextChanged`,
  which BmsDataModel only emits on connStatus transitions; if the very first
  snapshot has afe_online=0 (battery not attached at startup) no signal
  fires and AFE stays "unknown" until a later transition. Pre-existing
  design constraint of the plan, not a regression.
- Test harness was removed; no permanent test target exists (project has no
  Qt Test setup). The self-test can be resurrected from the report if a
  regression suite is desired later.
- Working tree is clean except untracked `.superpowers/` (report file).
