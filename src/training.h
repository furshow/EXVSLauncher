// Custom Training: custom VS Training matches (up to 4 units: 1v1, 1v2, 2v1, 1v3, 2v2, 3v1; CPU behavior and EX per slot).
// Included by main.cpp, not a separate unit.
//
// The match logic is training_bridge.dll from aerial's "2v2 Training Extension" 1.1 (free, non-commercial
// license; QQ group 1132405231). It is embedded unmodified (third_party/training2v2) and written to
// <game>\Training\ with its license files. The EXVS Overlay (TRAINING_OVERLAY_MIN or newer) loads it inside
// the game after checking the build (1.13.30), applies the 2v2 code patches, and copies the settings below
// into the bridge whenever Training\training.ini changes; it reports back in Training\status.ini.
//
// Training\training.ini [Training]:
//   Enabled, Seq (bumped on every Apply), Unit1..4 (unit id, -1 random, Unit1 0 = the unit picked in game,
//   -1500/-2000/-2500/-3000 random unit of that cost, -9 random boss: SlotPool1..4 = the ids to draw from),
//   Cpu2..4 (CPU behavior: 0 stop, 1 easy, 2 normal, 3 hard, 4 extreme, 5 walk, 6 evade, 7/8/9 low/mid/high
//   jump; the bridge calls it "level". Default partner normal, enemies hard), Ex1..4 (0 F, 1 S, 2 E, 3 random
//   each match; Ex1 -1 = the one picked in game), RandomStage, ConsumeCost (losing a unit costs team cost, COST
//   OVER ends the match), Pool (random units), Team2..4 (0 your team, 1 enemy team) and On2..4 (1 in the match,
//   0 out) for the CPU slots: at least one enemy, 4 units at most; Partner (slot 2 only: 1 ally, 0 out, 2 enemy)
//   for overlays older than 1.2.1.

static const char *TRAINING_OVERLAY_MIN = "1.2.1";
static const char *TRAINING_LICENSE_ID = "aerial-noncommercial-1.0";

static std::wstring training_dir() { return g_dir + L"Training\\"; }
static std::wstring training_ini() { return training_dir() + L"training.ini"; }

static bool training_write_resource(const wchar_t *res, const std::wstring &dst) {
    HRSRC r = FindResourceW(NULL, res, MAKEINTRESOURCEW(10) /*RT_RCDATA*/);
    if (!r) return false;
    DWORD size = SizeofResource(NULL, r);
    const char *data = (const char *)LockResource(LoadResource(NULL, r));
    // same bytes already there (the DLL may be loaded by a running game): nothing to do
    HANDLE f = CreateFileW(dst.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fs; GetFileSizeEx(f, &fs);
        bool same = false;
        if (fs.QuadPart == size) {
            std::vector<char> buf(size); DWORD rd = 0;
            same = ReadFile(f, buf.data(), size, &rd, NULL) && rd == size && memcmp(buf.data(), data, size) == 0;
        }
        CloseHandle(f);
        if (same) return true;
    }
    f = CreateFileW(dst.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0; WriteFile(f, data, size, &w, NULL); CloseHandle(f);
    return w == size;
}

static std::string training_install() {   // "" = ok
    CreateDirectoryW(training_dir().c_str(), NULL);
    struct { const wchar_t *res, *file; } files[] = {
        { L"TRAINING_BRIDGE", L"training_bridge.dll" }, { L"TRAINING_ENTRY", L"training-entry.ini" },
        { L"TRAINING_LICENSE", L"LICENSE-TrainingExtension.txt" }, { L"TRAINING_MINHOOK", L"MinHook-LICENSE.txt" },
        { L"TRAINING_README", L"README.txt" } };
    for (auto &x : files)
        if (!training_write_resource(x.res, training_dir() + x.file)) {
            if (!FindResourceW(NULL, x.res, MAKEINTRESOURCEW(10))) return "This launcher build doesn't include the Custom Training files";
            return "Could not write Training\\" + narrow(x.file);
        }
    return "";
}

static int training_get(const char *key, int def) { return GetPrivateProfileIntW(L"Training", widen(key).c_str(), def, training_ini().c_str()); }
static void training_set(const char *key, const std::string &v) {
    WritePrivateProfileStringW(L"Training", widen(key).c_str(), widen(v).c_str(), training_ini().c_str());
}

static json training_get_state() {
    Ini me(L"EXVSLauncher.ini");
    json slots = json::array();
    // teams: Team<n> / On<n> for the CPU slots 2-4; older settings only have Partner (1 ally, 0 off, 2 enemy)
    int partner = std::max(0, std::min(2, training_get("Partner", 1)));
    bool has_teams = training_get("Team2", -1) != -1;
    for (int i = 1; i <= 4; i++) {
        std::string n = std::to_string(i);
        int team = i == 1 ? 0 : has_teams ? (training_get(("Team" + n).c_str(), i == 2 ? 0 : 1) ? 1 : 0) : (i == 2 ? (partner == 2 ? 1 : 0) : 1);
        bool on = i == 1 || (has_teams ? training_get(("On" + n).c_str(), 1) != 0 : (i != 2 || partner != 0));
        slots.push_back({ {"unit", training_get(("Unit" + n).c_str(), i == 1 ? 0 : -1)},
                          {"level", training_get(("Cpu" + n).c_str(), i == 2 ? 2 : 3)},
                          {"ex", training_get(("Ex" + n).c_str(), i == 1 ? -1 : 0)},
                          {"team", team}, {"on", on} });
    }
    return { {"licenseAccepted", me.get("Training", "LicenseAccepted") == TRAINING_LICENSE_ID},
             {"installed", exists(training_dir() + L"training_bridge.dll")},
             {"enabled", training_get("Enabled", 0) != 0}, {"slots", slots},
             {"randomStage", training_get("RandomStage", 0) != 0}, {"consumeCost", training_get("ConsumeCost", 1) != 0},
             {"overlayMin", TRAINING_OVERLAY_MIN} };
}

// {enabled, slots:[{unit,level,ex}x4], randomStage, consumeCost, pool:[ids]}
static json training_apply(const json &t) {
    if (Ini(L"EXVSLauncher.ini").get("Training", "LicenseAccepted") != TRAINING_LICENSE_ID)
        return { {"ok", false}, {"error", "Accept the Custom Training license first"} };
    bool on = jb(t, "enabled", false);
    if (on) {
        std::string err = training_install();
        if (!err.empty()) return { {"ok", false}, {"error", err} };
    } else if (!exists(training_dir())) {
        return { {"ok", true} };   // never turned on: nothing to write
    }
    if (!t.contains("slots") || !t["slots"].is_array() || t["slots"].size() != 4) return { {"ok", false}, {"error", "Bad training setup"} };
    // teams of the CPU slots: at least one enemy, at most 2 CPUs on your team (4 units in all)
    int team[4] = { 0, 0, 1, 1 }; bool slot_on[4] = { true, true, true, true };
    int enemies = 0, allies = 0;
    for (int i = 1; i < 4; i++) {
        const json &s = t["slots"][i];
        team[i] = ji(s, "team", i == 1 ? 0 : 1) ? 1 : 0;
        slot_on[i] = jb(s, "on", true);
        if (slot_on[i]) (team[i] ? enemies : allies)++;
    }
    if (!enemies || allies > 2) return { {"ok", false}, {"error", "Bad training setup"} };
    for (int i = 1; i < 4; i++) {
        training_set(("Team" + std::to_string(i + 1)).c_str(), team[i] ? "1" : "0");
        training_set(("On" + std::to_string(i + 1)).c_str(), slot_on[i] ? "1" : "0");
    }
    for (int i = 0; i < 4; i++) {
        const json &s = t["slots"][i];
        std::string n = std::to_string(i + 1);
        int unit = ji(s, "unit", -1), level = ji(s, "level", i == 1 ? 2 : 3), ex = ji(s, "ex", 0);
        bool category = unit == -9 || unit == -1500 || unit == -2000 || unit == -2500 || unit == -3000;
        if ((unit < -1 && !category) || (i && unit == 0)) unit = -1;
        std::string sp;   // category: the units to draw from (sent by the page)
        if (category && s.contains("pool") && s["pool"].is_array())
            for (auto &id : s["pool"]) if (id.is_number_integer()) { if (!sp.empty()) sp += ","; sp += std::to_string(id.get<int>()); }
        if (category && sp.empty()) unit = -1;
        training_set(("SlotPool" + n).c_str(), sp);
        level = std::max(0, std::min(9, level));
        ex = std::max(i ? 0 : -1, std::min(3, ex));
        training_set(("Unit" + n).c_str(), std::to_string(unit));
        if (i) training_set(("Cpu" + n).c_str(), std::to_string(level));
        training_set(("Ex" + n).c_str(), std::to_string(ex));
    }
    std::string pool;
    if (t.contains("pool") && t["pool"].is_array())
        for (auto &id : t["pool"]) if (id.is_number_integer()) { if (!pool.empty()) pool += ","; pool += std::to_string(id.get<int>()); }
    training_set("Pool", pool);
    training_set("RandomStage", jb(t, "randomStage", false) ? "1" : "0");
    training_set("ConsumeCost", jb(t, "consumeCost", true) ? "1" : "0");
    // Partner for overlays older than 1.2.1 (slot 2 only): 1 ally, 0 off, 2 enemy
    training_set("Partner", !slot_on[1] ? "0" : team[1] ? "2" : "1");
    training_set("Enabled", on ? "1" : "0");
    training_set("Seq", std::to_string(training_get("Seq", 0) + 1));   // last: the overlay reloads on any change
    log_line("custom training %s", on ? "applied" : "turned off");
    return { {"ok", true} };
}

static json training_status();

// --training-test <game dir> <out.json>: the page's steps without a window (refused before the license,
// accept, apply, turn off) and what ends up on disk.
static int training_selftest(const wchar_t *dir, const wchar_t *out) {
    init_dir(dir);
    json r;
    json setup = { {"enabled", true}, {"randomStage", true}, {"consumeCost", false}, {"pool", {1001001, 2001001}},
                   {"slots", { { {"unit", 0}, {"level", 3}, {"ex", -1} }, { {"unit", 1001001}, {"level", 7}, {"ex", 0} },
                               { {"unit", -2500}, {"level", 12}, {"ex", 1}, {"pool", {2001001, 3001001}} }, { {"unit", 601001001}, {"level", 9}, {"ex", 2} } }} };
    Ini(L"EXVSLauncher.ini").del("Training", "LicenseAccepted");
    r["beforeLicense"] = training_apply(setup);
    Ini(L"EXVSLauncher.ini").set("Training", "LicenseAccepted", TRAINING_LICENSE_ID);
    r["apply"] = training_apply(setup);
    r["state"] = training_get_state();
    json t31 = setup; t31["slots"][2]["team"] = 0;   // 3v1: enemy 1 on your team
    r["apply3v1"] = training_apply(t31);
    r["state3v1"] = training_get_state()["slots"];
    json bad = setup; for (int i = 1; i < 4; i++) bad["slots"][i]["team"] = 0;   // nobody to fight: refused
    r["noEnemy"] = training_apply(bad);
    std::string text;
    if (FILE *f = _wfopen(training_ini().c_str(), L"rb")) { char buf[4096]; size_t n; while ((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n); fclose(f); }
    r["ini"] = text;
    json files = json::object();
    for (const wchar_t *f : { L"training_bridge.dll", L"training-entry.ini", L"LICENSE-TrainingExtension.txt", L"MinHook-LICENSE.txt", L"README.txt" }) {
        WIN32_FILE_ATTRIBUTE_DATA fa = {};
        files[narrow(f)] = GetFileAttributesExW((training_dir() + f).c_str(), GetFileExInfoStandard, &fa) ? (long long)fa.nFileSizeLow : -1;
    }
    r["files"] = files;
    json off = setup; off["enabled"] = false;
    r["off"] = training_apply(off);
    r["enabledAfterOff"] = training_get("Enabled", -1);
    r["seq"] = training_get("Seq", -1);
    r["status"] = training_status();
    if (FILE *f = _wfopen(out, L"wb")) { std::string d = r.dump(2); fwrite(d.data(), 1, d.size(), f); fclose(f); }
    return 0;
}

// What the overlay in the running game reported: {state: off|queued|applied|error|nogame|waiting, message}
static json training_status() {
    auto pids = find_procs(CLIENT_EXE, true);
    if (pids.empty()) return { {"state", "nogame"}, {"message", ""} };
    std::wstring st = training_dir() + L"status.ini";
    wchar_t buf[512];
    DWORD pid = GetPrivateProfileIntW(L"Status", L"Pid", 0, st.c_str());
    if (std::find(pids.begin(), pids.end(), pid) == pids.end()) return { {"state", "waiting"}, {"message", ""} };
    GetPrivateProfileStringW(L"Status", L"State", L"", buf, 512, st.c_str()); std::string state = narrow(buf);
    GetPrivateProfileStringW(L"Status", L"Message", L"", buf, 512, st.c_str());
    return { {"state", state}, {"message", narrow(buf)} };
}
