Replays on every machine (EXVS Launcher)
========================================

This folder was created by the EXVS Launcher (Settings > Gameplay > Replays). With it, every
machine in a match keeps the replay, not only the one the game picks.

EXVSReplay.dll is the unmodified "EXVS multi-function patch" (EXVS 多功能补丁, 20261028) by M1kou,
renamed from XINPUT9_1_0.dll. It is included with the author's permission. Credit: M1kou.
It uses MinHook (MinHook-LICENSE.txt).

How it works: the game records a match on one machine only (the "elected" one) and sends it to
the Live Monitor. The patch lets the other machines record it too and keep it on disk only
(29\replay_store, like the game). On the Live Monitor it adds loose .bin replays to
replay_list.json. It checks the game exe first and does nothing on an exe it doesn't know.
Like the game, it only records matches with 4 human players.

The EXVS Overlay (xinput9_1_0.dll, already loaded by the game) loads EXVSReplay.dll. Its input
blocking is off here ([InputFix] Enabled = 0): the overlay already does that. The free play bypass
for time rental is off too.

Files: xinputPatchConfig.ini = its settings (read when the game starts), xinputPatch.log = its log.
Turn Replays off in the launcher to stop loading it. The replays already saved stay in the game.
