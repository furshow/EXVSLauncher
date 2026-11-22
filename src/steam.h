// Add to Steam (Settings > Launcher > Steam), included from main.cpp after cardserver.h.
//
// Adds EXVS Launcher to the Steam library as a non-Steam game, so Steam Input reaches it: the Steam Link
// on-screen controller, a Steam Deck, or any pad Steam supports shows up as a virtual Xbox (XInput) pad in the
// launcher and in the game copies it starts. Steam only does that for programs it starts itself, so the
// launcher must be opened from Steam ("Open through Steam").
//
// It writes Steam\userdata\<account>\config\shortcuts.vdf (binary KeyValues) for every account on this PC, the
// same way Supermedo's Bloodborne PC launcher does: appid = crc32("\"exe\"" + name) | 0x80000000, an entry with
// the same Exe is updated in place (keeping its appid, LastPlayTime and tags), the old file is kept as
// shortcuts.vdf.bak. Steam only reads that file when it starts and writes its own copy when it closes, so a
// running Steam is shut down first (steam.exe -shutdown) and started again afterwards.
#pragma once

static const char *STEAM_APP_NAME = "Gundam Extreme VS 2 Infinite Boost";   // the name shown in the Steam library

// ---- binary KeyValues --------------------------------------------------------------------------------------
// Types: 0 map, 1 string, 2 int32, 3 float32, 4 pointer, 5 wide string, 6 color, 7 uint64, 8 end, 10 int64.
// Values are kept as raw bytes so a file is written back byte for byte (except what we change).
struct SteamKv { uint8_t t = 0; std::string k, v; std::vector<SteamKv> kids; };

static bool steam_kv_parse(const std::string &d, size_t &p, std::vector<SteamKv> &out, int depth = 0) {
    if (depth > 32) return false;
    auto cstr = [&](std::string &s) { size_t e = d.find('\0', p); if (e == std::string::npos) return false; s = d.substr(p, e - p); p = e + 1; return true; };
    while (p < d.size()) {
        SteamKv kv; kv.t = (uint8_t)d[p++];
        if (kv.t == 8) return true;
        if (!cstr(kv.k)) return false;
        switch (kv.t) {
        case 0: if (!steam_kv_parse(d, p, kv.kids, depth + 1)) return false; break;
        case 1: if (!cstr(kv.v)) return false; break;
        case 2: case 3: case 4: case 6: if (p + 4 > d.size()) return false; kv.v = d.substr(p, 4); p += 4; break;
        case 7: case 10: if (p + 8 > d.size()) return false; kv.v = d.substr(p, 8); p += 8; break;
        case 5: { size_t e = p; while (e + 1 < d.size() && (d[e] || d[e + 1])) e += 2;
                  if (e + 1 >= d.size()) return false; kv.v = d.substr(p, e - p); p = e + 2; break; }
        default: return false;
        }
        out.push_back(std::move(kv));
    }
    return false;   // no end marker
}
static void steam_kv_dump(const std::vector<SteamKv> &list, std::string &o) {
    for (auto &kv : list) {
        o += (char)kv.t; o += kv.k; o += '\0';
        if (kv.t == 0) steam_kv_dump(kv.kids, o);
        else { o += kv.v; if (kv.t == 1) o += '\0'; else if (kv.t == 5) o.append(2, '\0'); }
    }
    o += (char)8;
}
static SteamKv *steam_kv_find(std::vector<SteamKv> &list, const char *key) {
    for (auto &kv : list) if (_stricmp(kv.k.c_str(), key) == 0) return &kv;
    return nullptr;
}
static SteamKv steam_kv_str(const char *k, const std::string &v) { SteamKv kv; kv.t = 1; kv.k = k; kv.v = v; return kv; }
static SteamKv steam_kv_int(const char *k, uint32_t v) { SteamKv kv; kv.t = 2; kv.k = k; kv.v.assign((const char *)&v, 4); return kv; }
static uint32_t steam_kv_u32(const SteamKv *kv) { uint32_t v = 0; if (kv && kv->t == 2 && kv->v.size() == 4) memcpy(&v, kv->v.data(), 4); return v; }

static uint32_t steam_crc32(const std::string &s) {
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char b : s) { c ^= b; for (int i = 0; i < 8; i++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1))); }
    return ~c;
}
static uint32_t steam_shortcut_id(const std::string &quoted_exe, const std::string &name) { return steam_crc32(quoted_exe + name) | 0x80000000u; }
static unsigned long long steam_game_id(uint32_t appid) { return ((unsigned long long)appid << 32) | 0x02000000ull; }   // steam://rungameid/

static std::string steam_lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }
static std::string steam_unquote(std::string s) { if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2); return s; }

// ---- Steam on this PC -------------------------------------------------------------------------------------
static std::wstring steam_reg(HKEY root, const wchar_t *key, const wchar_t *name) {
    wchar_t buf[MAX_PATH * 2]; DWORD sz = sizeof(buf);
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, NULL, buf, &sz) != ERROR_SUCCESS) return L"";
    std::wstring s = buf; for (auto &c : s) if (c == L'/') c = L'\\';
    while (!s.empty() && s.back() == L'\\') s.pop_back();
    return s;
}
static std::wstring g_steam_root_override;   // --steam-test: a fake Steam folder
static std::wstring steam_root() {
    if (!g_steam_root_override.empty()) return g_steam_root_override;
    std::wstring s = steam_reg(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (s.empty()) s = steam_reg(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
    if (s.empty()) s = steam_reg(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath");
    return !s.empty() && exists(s + L"\\steam.exe") ? s : (g_steam_root_override.empty() ? L"" : s);
}
// every account that has signed in on this PC: userdata\<numeric id>\config
static std::vector<std::wstring> steam_config_dirs(const std::wstring &root) {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((root + L"\\userdata\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        std::wstring n = fd.cFileName;
        if (n.empty() || n == L"0" || n.find_first_not_of(L"0123456789") != std::wstring::npos) continue;
        std::wstring cfg = root + L"\\userdata\\" + n + L"\\config";
        if (exists(cfg)) out.push_back(cfg);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}
static bool steam_running() {
    bool found = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = { sizeof(pe) };
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe)) found = _wcsicmp(pe.szExeFile, L"steam.exe") == 0;
    CloseHandle(snap);
    return found;
}
static bool steam_started_us() {   // Steam sets these for the programs it starts
    wchar_t b[64];
    return GetEnvironmentVariableW(L"SteamGameId", b, 64) || GetEnvironmentVariableW(L"SteamAppId", b, 64) || GetEnvironmentVariableW(L"SteamClientLaunch", b, 64);
}
static std::wstring steam_self_exe() { wchar_t p[MAX_PATH]; GetModuleFileNameW(NULL, p, MAX_PATH); return p; }

static bool steam_read_file(const std::wstring &path, std::string &d) {
    d.clear();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz; bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart < (64ll << 20);
    if (ok) { d.resize((size_t)sz.QuadPart); DWORD got = 0; ok = d.empty() || (ReadFile(h, &d[0], (DWORD)d.size(), &got, NULL) && got == d.size()); }
    CloseHandle(h);
    return ok;
}
static bool steam_write_file(const std::wstring &path, const std::string &d) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0; bool ok = d.empty() || (WriteFile(h, d.data(), (DWORD)d.size(), &put, NULL) && put == d.size());
    ok = FlushFileBuffers(h) && ok;
    CloseHandle(h);
    return ok;
}
// the entry for this exe in one shortcuts.vdf: its appid, or 0
static uint32_t steam_find_entry(const std::wstring &vdf, const std::string &exe_lower) {
    std::string d; if (!steam_read_file(vdf, d)) return 0;
    std::vector<SteamKv> top; size_t p = 0;
    if (!steam_kv_parse(d, p, top)) return 0;
    SteamKv *sc = steam_kv_find(top, "shortcuts"); if (!sc) return 0;
    for (auto &e : sc->kids) {
        SteamKv *x = steam_kv_find(e.kids, "Exe");
        if (x && x->t == 1 && steam_lower(steam_unquote(x->v)) == exe_lower) return steam_kv_u32(steam_kv_find(e.kids, "appid"));
    }
    return 0;
}

// Adds or updates the entry in one shortcuts.vdf. Returns "added" / "updated", or "" with err set.
static std::string steam_vdf_add(const std::wstring &vdf, const std::string &exe, const std::string &start_dir, uint32_t *appid_out, std::string &err) {
    std::string d, before;
    std::vector<SteamKv> top;
    if (steam_read_file(vdf, d) && !d.empty()) {
        size_t p = 0;
        if (!steam_kv_parse(d, p, top)) { err = "Steam's shortcuts.vdf has an unknown format; it was left untouched"; return ""; }
        before = d;
    }
    SteamKv *sc = steam_kv_find(top, "shortcuts");
    if (!sc) { SteamKv m; m.t = 0; m.k = "shortcuts"; top.push_back(m); sc = &top.back(); }

    std::string quoted = "\"" + exe + "\"", exe_lower = steam_lower(exe);
    SteamKv *old = nullptr;
    int next = 0;
    for (auto &e : sc->kids) {
        next = (std::max)(next, atoi(e.k.c_str()) + 1);
        SteamKv *x = steam_kv_find(e.kids, "Exe");
        if (!old && x && x->t == 1 && steam_lower(steam_unquote(x->v)) == exe_lower) old = &e;
    }
    uint32_t appid = old ? steam_kv_u32(steam_kv_find(old->kids, "appid")) : 0;
    if (!appid) appid = steam_shortcut_id(quoted, STEAM_APP_NAME);

    SteamKv e; e.t = 0; e.k = old ? old->k : std::to_string(next);
    e.kids = {
        steam_kv_int("appid", appid), steam_kv_str("AppName", STEAM_APP_NAME), steam_kv_str("Exe", quoted),
        steam_kv_str("StartDir", "\"" + start_dir + "\""), steam_kv_str("icon", exe), steam_kv_str("ShortcutPath", ""),
        steam_kv_str("LaunchOptions", ""), steam_kv_int("IsHidden", 0), steam_kv_int("AllowDesktopConfig", 1),
        steam_kv_int("AllowOverlay", 1), steam_kv_int("OpenVR", 0), steam_kv_int("Devkit", 0), steam_kv_str("DevkitGameID", ""),
        steam_kv_int("DevkitOverrideAppID", 0), steam_kv_int("LastPlayTime", 0), steam_kv_str("FlatpakAppID", "") };
    SteamKv tags; tags.t = 0; tags.k = "tags";
    if (old) {   // keep what Steam (or the user) set on the existing entry
        if (SteamKv *x = steam_kv_find(old->kids, "LastPlayTime")) *steam_kv_find(e.kids, "LastPlayTime") = *x;
        if (SteamKv *x = steam_kv_find(old->kids, "LaunchOptions")) *steam_kv_find(e.kids, "LaunchOptions") = *x;
        if (SteamKv *x = steam_kv_find(old->kids, "IsHidden")) *steam_kv_find(e.kids, "IsHidden") = *x;
        if (SteamKv *x = steam_kv_find(old->kids, "tags")) tags = *x;
    }
    e.kids.push_back(tags);
    if (old) *old = e; else sc->kids.push_back(e);

    std::string out; steam_kv_dump(top, out);
    if (appid_out) *appid_out = appid;
    if (out == before) return "updated";
    if (!before.empty()) {
        if (!steam_write_file(vdf + L".bak", before)) { err = "Could not back up shortcuts.vdf"; return ""; }
    }
    std::wstring tmp = vdf + L".tmp";
    if (!steam_write_file(tmp, out)) { DeleteFileW(tmp.c_str()); err = "Could not write Steam's shortcuts.vdf"; return ""; }
    if (!MoveFileExW(tmp.c_str(), vdf.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str()); err = "Could not write Steam's shortcuts.vdf"; return "";
    }
    return old ? "updated" : "added";
}

// ---- launcher API -----------------------------------------------------------------------------------------
static json steam_get_state() {
    std::wstring root = steam_root();
    json s = { {"installed", !root.empty()}, {"added", false}, {"running", false}, {"fromSteam", steam_started_us()}, {"accounts", 0} };
    if (root.empty()) return s;
    auto dirs = steam_config_dirs(root);
    s["accounts"] = (int)dirs.size();
    s["running"] = steam_running();
    std::string exe_lower = steam_lower(narrow(steam_self_exe()));
    for (auto &c : dirs) if (uint32_t id = steam_find_entry(c + L"\\shortcuts.vdf", exe_lower)) { s["added"] = true; s["appid"] = id; break; }
    return s;
}

static bool steam_wait_closed(DWORD ms) {
    for (DWORD t = GetTickCount(); steam_running(); Sleep(250)) if (GetTickCount() - t > ms) return false;
    Sleep(1000);   // let it finish writing its files
    return true;
}

static json steam_add() {
    std::wstring root = steam_root();
    if (root.empty()) return { {"ok", false}, {"error", "Steam isn't installed on this PC"} };
    auto dirs = steam_config_dirs(root);
    if (dirs.empty()) return { {"ok", false}, {"error", "Sign in to Steam once on this PC, then try again"} };

    bool was_running = g_steam_root_override.empty() && steam_running();
    if (was_running) {
        log_line("steam: closing Steam to add the shortcut");
        start_process(root + L"\\steam.exe", L"-shutdown", root);
        if (!steam_wait_closed(30000)) return { {"ok", false}, {"error", "Steam didn't close. Close Steam yourself and press Add to Steam again"} };
    }
    std::string exe = narrow(steam_self_exe()), start = narrow(g_dir.substr(0, g_dir.size() - 1));
    int added = 0, updated = 0; uint32_t appid = 0; std::string err;
    for (auto &c : dirs) {
        std::string e, r = steam_vdf_add(c + L"\\shortcuts.vdf", exe, start, &appid, e);
        if (r == "added") added++; else if (r == "updated") updated++; else err = e;
        log_line("steam: %s %s", narrow(c).c_str(), r.empty() ? e.c_str() : r.c_str());
    }
    if (was_running) start_process(root + L"\\steam.exe", L"", root);
    if (!added && !updated) return { {"ok", false}, {"error", err.empty() ? "Could not add EXVS Launcher to Steam" : err} };
    json s = steam_get_state();
    return { {"ok", true}, {"added", added}, {"updated", updated}, {"appid", appid}, {"restarted", was_running}, {"state", s} };
}

static json steam_open() {   // start the launcher again from Steam, so Steam Input reaches it
    json s = steam_get_state();
    if (!s.value("added", false)) return { {"ok", false}, {"error", "Add EXVS Launcher to Steam first"} };
    std::wstring url = L"steam://rungameid/" + std::to_wstring(steam_game_id(s["appid"].get<uint32_t>()));
    if ((INT_PTR)ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL) <= 32) return { {"ok", false}, {"error", "Could not open Steam"} };
    log_line("steam: reopening through %s", narrow(url).c_str());
    return { {"ok", true} };
}

// --steam-test <fake Steam folder> <out.json>: needs <folder>\userdata\<id>\config\; adds twice (added, then
// updated), checks the file reads back and that an untouched entry survives byte for byte. Starts nothing.
static int steam_selftest(const wchar_t *root, const wchar_t *out) {
    wchar_t self[MAX_PATH]; GetModuleFileNameW(NULL, self, MAX_PATH);
    std::wstring dir = self; dir.resize(dir.find_last_of(L'\\') + 1);
    init_dir(dir.c_str());
    g_steam_root_override = root;
    json r;
    r["before"] = steam_get_state();
    std::vector<std::string> originals;
    for (auto &c : steam_config_dirs(root)) { std::string d; steam_read_file(c + L"\\shortcuts.vdf", d); originals.push_back(d); }
    r["first"] = steam_add();
    r["second"] = steam_add();
    r["after"] = steam_get_state();
    json files = json::array();
    auto dirs = steam_config_dirs(root);
    for (size_t i = 0; i < dirs.size(); i++) {
        std::string d; steam_read_file(dirs[i] + L"\\shortcuts.vdf", d);
        std::vector<SteamKv> top; size_t p = 0; bool ok = steam_kv_parse(d, p, top) && p == d.size();
        std::string again; steam_kv_dump(top, again);
        json f = { {"dir", narrow(dirs[i])}, {"parses", ok}, {"roundTrip", again == d}, {"bytes", (long long)d.size()} };
        json names = json::array();
        if (SteamKv *sc = steam_kv_find(top, "shortcuts"))
            for (auto &e : sc->kids) { SteamKv *n = steam_kv_find(e.kids, "AppName"); if (!n) n = steam_kv_find(e.kids, "appname");
                                       names.push_back(e.k + ": " + (n ? n->v : "?")); }
        f["entries"] = names;
        if (i < originals.size() && !originals[i].empty()) {   // every entry that was there before is still identical
            std::vector<SteamKv> o; size_t q = 0; bool kept = steam_kv_parse(originals[i], q, o);
            SteamKv *os = kept ? steam_kv_find(o, "shortcuts") : nullptr, *ns = steam_kv_find(top, "shortcuts");
            std::string self_l = steam_lower(narrow(steam_self_exe()));
            if (os && ns) for (auto &e : os->kids) {
                SteamKv *x = steam_kv_find(e.kids, "Exe");
                if (x && steam_lower(steam_unquote(x->v)) == self_l) continue;
                std::string a, b; steam_kv_dump({ e }, a);
                SteamKv *m = nullptr; for (auto &n : ns->kids) if (n.k == e.k) m = &n;
                if (!m) { kept = false; break; }
                steam_kv_dump({ *m }, b); if (a != b) { kept = false; break; }
            }
            f["othersKept"] = kept;
        }
        files.push_back(f);
    }
    r["files"] = files;
    r["appidFormula"] = steam_shortcut_id("\"C:\\Games\\test.exe\"", "Test");
    std::string t = r.dump(2);
    if (FILE *f = _wfopen(out, L"wb")) { fwrite(t.data(), 1, t.size(), f); fclose(f); }
    return 0;
}
