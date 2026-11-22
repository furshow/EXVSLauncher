# Third-party notices

EXVS Launcher includes the following third-party code. Each keeps its own license.

| Component | Location | License |
|---|---|---|
| [webview](https://github.com/webview/webview) 0.12 | `third_party/webview/` | MIT, see `third_party/webview/LICENSE` |
| Microsoft WebView2 SDK headers | `third_party/webview2/` | BSD-3-Clause, see `third_party/webview2/LICENSE.txt` |
| [JSON for Modern C++](https://github.com/nlohmann/json) 3.11.3 (Niels Lohmann) | `third_party/json/json.hpp` | MIT (SPDX header in the file) |
| "EXVS multi-function patch" 20261028 by **M1kou** (replays on every machine), unmodified, renamed `EXVSReplay.dll`; uses MinHook | `third_party/replay/` | Shipped with the author's permission; credit M1kou. MinHook: BSD-2-Clause, see `third_party/replay/MinHook-LICENSE.txt` |

The release exe also embeds the EXVS Overlay DLL (MIT, with Dear ImGui and AMD FidelityFX FSR 1, both MIT).
See the EXVSOverlay repository for its notices.

This project is a fan-made tool. It is not affiliated with, or endorsed by, Bandai Namco or Sunrise.
It contains no game files.
