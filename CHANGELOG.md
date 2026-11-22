# Changelog

All notable changes to EXVS Launcher. The release workflow (.github/workflows/build-release.yml) uses the
section of the released version as the release notes.

## [1.5.0] - 2026-11-22
- Bundles **EXVS Overlay 1.2.4** (unchanged).
### Steam
- **Add to Steam** (Settings > Launcher > Steam): adds the launcher to the Steam library as a non-Steam game, named
  "Gundam Extreme VS 2 Infinite Boost", for every Steam account on the PC (an existing entry is updated, never
  duplicated; the old `shortcuts.vdf` is kept as `shortcuts.vdf.bak`). If Steam is open, it closes and opens again, since Steam only reads its shortcuts at start.
- **Open through Steam**: starts the launcher again from Steam. Opened from Steam, Steam Input reaches the launcher
  and the game: the Steam Link on-screen controller, a Steam Deck or any pad Steam supports works as an Xbox
  controller, also in Split screen. Same shortcut format as Supermedo's Bloodborne PC launcher.
### Fixes
- **Play** brings the game window to the front once it opens. Started from Steam (Big Picture, Steam Link) it
  used to open behind the launcher or Steam.

## [1.4.1] - 2026-11-22
- Bundles **EXVS Overlay 1.2.4** (unchanged).
### Fixes
- **Settings toggles**: turning a toggle off (Quick skin change, Frame rate limit and the others) works on the first
  click again. It stayed ON on screen and wasn't saved until another setting changed.

## [1.4.0] - 2026-11-22
- Bundles **EXVS Overlay 1.2.4** (loads the replay patch).
### Replays
- **Replays on every machine** (Settings > Gameplay > Replays): the game keeps a match replay only on the machine it
  sends to the Live Monitor; with this on, every machine keeps its own copy in the game's replay list (and the Live
  Monitor adds loose replays to its list). Like the game, only matches with 4 human players are recorded. It uses
  **M1kou**'s "EXVS multi-function patch" (20261028), shipped unmodified with the author's permission: turning it on
  writes `Replay\` in the game folder (`EXVSReplay.dll` and its settings, with its input blocking and time rental
  free play bypass off), and EXVS Overlay 1.2.4 loads it. Thanks to M1kou.
### Credits
- Special thanks: SnowLauncher by Snow, aerial, M1kou, the ESVS2IB 4P launcher and the EXVS community.

## [1.3.3] - 2026-11-22
- Bundles **EXVS Overlay 1.2.3** (team cost per team in Custom Training).
### Custom Training
- **Team cost per team**: each team on the **Custom** page has a cost box (numbers only, 1500 to 99000; 6000 is the
  game's), for matches like 1v2 with more cost for you. Needs **Team cost** on and EXVS Overlay 1.2.3.

## [1.3.2] - 2026-11-22
- Bundles **EXVS Overlay 1.2.2** (Custom Training matches of 5 to 10 units).
### Custom Training
- **Up to 10 units**: six more CPUs, so 1v4, 3v3, 5v5, 1v9, 9v1 and everything between. **＋ Add CPU** shows while
  the match has fewer than 10 units; your team takes up to 9 (you and 8 CPUs) and the enemy team always keeps one.
  Needs EXVS Overlay 1.2.2 (it adds the extra units inside the game). The result screen shows 4 of the units.

## [1.3.1] - 2026-11-21
- Bundles **EXVS Overlay 1.2.1** (Custom Training layouts up to 4 units, result screen and rematch in every layout).
### Custom Training
- **Teams of your choice, up to 4 units**: 1v1, 1v2, 2v1, 1v3, 2v2 or 3v1. Each team has a **＋ Add CPU** button
  (while there's room), every CPU card a **✕** to take it out of the match, and any CPU card can be **dragged to
  the other team**. The enemy team always keeps at least one CPU. The mode shows between the teams.
- Team cost, the result screen and the rematch work in every layout. Needs EXVS Overlay 1.2.1.

## [1.3.0] - 2026-11-21
- Bundles **EXVS Overlay 1.2.0** (it runs Custom Training inside the game).
### Play button
- The Play button turns into **Close** only when what the selected mode starts is running: with the Client open,
  choosing **Live Monitor** shows Play again (to start only the Live Monitor). Close closes only that part
  (Client, Live Monitor, or both for Client + LM); Settings › Force close still closes everything.
### Card server chip
- The Play page chip now shows the card server of your settings, local or remote: **Card server online / offline**
  (or Local server on / off). The launcher checks it every 10 seconds (any HTTP answer = online). When it's
  online, clicking the chip opens its page in the browser (the Server-Infinite web panel for a local server).
### Custom Training
- New **Custom Training** page (**Custom** in the menu): VS Training as a 2v2. Pick your unit (or the one picked
  in game), a partner and two enemies from a picker with pictures, cost tabs and search (249 units, 13 bosses,
  Eldora Dotress), or **Random** for a new unit every match, or random by category: **Random 3000 / 2500 / 2000 /
  1500** and **Random boss**. CPU behavior (stand still, easy … extreme, walk, evade, jumps) and EX type per slot
  (or **Random**: a new F / S / E every match), random stage, and team cost (COST OVER ends the match).
  Changes apply to the next VS Training match, with the game open; turning it off brings the normal 1v1 back.
- The partner card has **Ally / Off / Enemy**: Off = **1v2** (you against the two enemies), Enemy = **1v3** (the
  card moves to the enemy team as Enemy 3). The card can also be dragged to the other team. Random stage works in all
  three; team cost works in 2v2 and (experimental) in 1v3 and 1v2.
- It runs on the bridge of aerial's free, non-commercial "2v2 Training Extension" 1.1 (QQ group 1132405231),
  embedded unmodified and written to `Training\` with its license the first time it's turned on, after accepting
  the license. The EXVS Overlay 1.2.0 or newer loads it inside the game; it accepts any 1.13.30 client (build
  check instead of exact exe hashes). Not an official aerial release: see `Training\README.txt`.

## [1.2.1] - 2026-11-19
### Fixed
- Splitscreen: with two DirectInput controllers of the same model (e.g. two DualSense), every copy read both.
  The launcher now lists each DirectInput controller on its own (numbered when several have the same name, and
  lit up when pressed) and gives each copy only its player's device (`Path` in its `config.ini` instead of the
  controller name). Players saved with a DirectInput controller in 1.2.0 keep it while only one connected
  controller has that name; otherwise the page asks to choose which one.
- Splitscreen: a copy could refuse to start with "Only one of InterfaceName and IpAddress can be specified in
  config.ini" (after playing normally first). The player's `config.ini` kept the game folder's `InterfaceName` when
  it appeared twice or the file started with a UTF-8 BOM; every `InterfaceName` / `IpAddress` line is now removed
  before the player's address is written.
- A fixed IP address for the Client or Live Monitor no longer leaves `InterfaceName` next to it in the config file.
### Changed
- Splitscreen: the Live Monitor the session started closes when the splitscreen ends (all copies closed or
  **Stop**). A Live Monitor that was already running before the session is left open.

## [1.2.0] - 2026-11-19
### Splitscreen
- New **Split** page: 2, 3 or 4 players on one PC. Each player gets a copy of the game and picks the **screen** it
  opens on. Players on the same screen share it (side by side for 2, 2×2 for 3-4); a player alone on a screen gets
  the whole screen. A black backdrop covers each screen in use, and the layout preview shows every screen.
- Each player picks a controller (Xbox / XInput 1-4, a DirectInput pad or the keyboard) and their own card. The
  player whose controller is pressed lights up on the page and in the layout preview.
- Every copy is a cabinet of its own: the launcher writes `Splitscreen\P1`…`P4` with its own `config.ini`,
  `card.ini`, serial and cabinet number (`29\testmode_pcb.json` networkId 0-3), and starts the game as
  `vsac29_Release.exe "<player folder>"`.
- Each copy needs its own address on the network: free addresses next to the chosen adapter's are added for
  the session only (one Windows permission prompt) and removed when the last copy closes. Addresses left by a
  session that ended abnormally can be removed with **Clean up**. On a DHCP adapter (Windows turns DHCP off as soon
  as an address is added by hand) the same address, gateway and DNS are kept during the session, and DHCP is
  turned back on afterwards.
- **Free play** (on by default): every copy starts in free play, and the debug menu options turned on in your
  client apply to every player.
- Optionally starts the Live Monitor (and the card server, if *Start card server with the game* is on).
- Needs EXVS Overlay 1.1.0 or newer (included): it gives each copy only its own controller.

### Programs
- Settings › Launcher › Programs: pick which **Live Monitor** the Play button starts — the game's own
  `vsac29_Release_LM.exe` or another one you add (for example LiteMonitor) — and switch between them with ◀ ▶.
- The same for the **card server** program. Auto finds `Server.exe` or `Server-Infinite\Server-Infinite.exe` in the
  game folder; Add… picks any other `.exe`.
- New option **Start card server with the game**: Play starts the card server first if it isn't running.

### Startup
- A loading splash (logo and a progress bar) shows while the launcher starts. The small white window that appeared
  before the launcher is gone: the main window only shows once the page is drawn.

## [1.1.0] - 2026-11-14
### Permissions
- The launcher no longer asks for administrator rights when it opens, and the game no longer runs as administrator.
  Only N2N needs them: Windows asks once when you connect, and a small helper runs `edge.exe`. Closing the launcher
  during a match still keeps N2N up until the game closes.

### Online
- The Online page has two tabs. **VPN / LAN** (the default) has a 3-step guide for playing over a VPN (Radmin VPN,
  ZeroTier, Hamachi, Tailscale...) or a LAN, and the network adapter and fixed IP for the Client and the Live Monitor
  (moved here from Settings › Advanced). **N2N** has the N2N connection, its log and the local card server.

### Controls
- Input tester in Settings › Controls: shows the game actions you press, for the keyboard and for each controller.
- Xbox controllers are also read through XInput when mapping buttons (DirectInput got no presses on some PCs).
- Fixed the bottom bar covering the Settings page when the right column is taller than the window.

### EXVS Overlay
- Bundles **EXVS Overlay 1.0.1** (DirectInput arcade sticks in the controller overlay and the menu, new arcade-stick look).

## [1.0.0] - 2026-11-14
First public release.

- A modern replacement for SnowLauncher: one exe in the game folder. It shares `SnowLauncher.ini`, so you can go
  back to SnowLauncher at any time.
- **Play:** Client, Live Monitor, or both, with quick window and resolution pickers and the status of the game,
  N2N and the local server.
- **Settings:** a game-menu style screen (Graphics, Gameplay, Controls, Account, Advanced, Launcher) with
  keyboard and controller navigation and auto-save.
  - All SnowLauncher resolutions and window sizes, with a recommended resolution for your monitor.
  - Keyboard for Client and Live Monitor, and controller mapping for both pads (XInput or DirectInput, with
    the game's own button layout or a custom one).
  - Card name, access code, card server (your own address, or Local with `Server.exe`), serial, region and
    opening skip.
- **Streamer mode:** hides the card server address and the access code.
- **Online:** N2N virtual LAN with a live log, and the local card server.
- **Mods:** opens the EXVSIB Mod Manager and can sync mods before playing.
- Bundles **EXVS Overlay 1.0.0** and installs, updates or removes it from Settings.
- Updates itself from the releases of this repo.
- English, Spanish, Simplified Chinese and Japanese.
