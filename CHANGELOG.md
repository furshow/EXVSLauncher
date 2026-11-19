# Changelog

All notable changes to EXVS Launcher. The release workflow (.github/workflows/build-release.yml) uses the
section of the released version as the release notes.

## [Unreleased]
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
