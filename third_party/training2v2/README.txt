Custom Training (EXVS Launcher)
===============================

This folder was created by the EXVS Launcher (Custom Training page). It lets VS Training run as a
custom match (2v2, 1v2 or 1v3): your unit, a partner and two enemies, each with its own unit, CPU
behavior and EX type.

training_bridge.dll is the unmodified bridge of the "2v2 Training Extension" 1.1 (LicenseEdition)
by aerial. Official channel, license checks and reports of anyone charging for it: QQ group 1132405231.
It is free for non-commercial use under aerial's license (LICENSE-TrainingExtension.txt, Chinese).
Do not sell it or charge for it. If someone asked you to pay for it, report it in the QQ group above.
The bridge uses MinHook (MinHook-LICENSE.txt).

This is NOT an official aerial release. What is different from aerial's package:
- aerial's TrainingApi web panel and its DLL injection are not used. The EXVS Overlay
  (xinput9_1_0.dll, already loaded by the game) loads training_bridge.dll itself, after checking that
  the game is the 1.13.30 build (PE build stamp 0x69E2081B) instead of two exact exe hashes, so a
  game with mods works too.
- isohook_2v2.dll is not used. Only its two 2v2 code patches (player count and 2v2 gate) are applied,
  by the overlay; the rest of that DLL is window / input handling for multiple game copies.
- Unit mastery saving is not included.
The source of these changes is public: the EXVS Overlay and EXVS Launcher repositories on GitHub (furshow).

Files: training.ini = settings written by the launcher, status.ini = state reported by the game.
Delete this folder (with the game closed) to remove Custom Training completely.
