# Changelog

All notable changes to EXVS Launcher. The release workflow (.github/workflows/build-release.yml) uses the
section of the released version as the release notes.

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
