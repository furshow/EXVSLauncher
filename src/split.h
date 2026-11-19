// Splitscreen: 2 to 4 copies of the game on this PC, one per player, tiled on one monitor.
// Included by main.cpp (after the controller code), not a separate unit.
//
// How it works:
//   - Each player gets a folder Splitscreen\P<n> with its own config.ini, card.ini and 29\testmode_pcb.json.
//     The game is started as `vsac29_Release.exe "<that folder>"` from the game folder: the loader (bngrw.dll)
//     then reads config.ini / card.ini from there (one command-line argument = config folder).
//   - Every copy is a separate cabinet, so each one needs its own IP address on the LAN (the game binds its
//     ports on the chosen address and finds the Live Monitor by broadcast) and its own cabinet number
//     (testmode networkId 0-3). The IPs are added to one network adapter for the session by an elevated helper
//     (this exe with --split-helper, one UAC prompt), and removed again when the last copy closes.
//   - The EXVS Overlay (1.1.0 or newer) reads the [split] section of each config.ini: each copy only sees its
//     own XInput controller, the keyboard only reaches the keyboard player, and the focus doesn't matter.
//   - The launcher places the windows (borderless) in a grid on the chosen monitor, over a black backdrop.

static const char *SPLIT_OVERLAY_MIN = "1.1.0";
static const wchar_t *SPLIT_STOP_EVENT = L"Local\\EXVSLauncher.SplitStop";
static const wchar_t *g_split_verb = L"runas";   // --split-test uses "open"

struct SplitPlayer { std::string input, chip, code, serial; int monitor = 0; };
static std::mutex g_split_mx;
static std::atomic<bool> g_split_running{false}, g_split_starting{false};
static std::vector<DWORD> g_split_pids;
static std::vector<std::string> g_split_ips;
static std::string g_split_error;
static std::vector<HWND> g_split_backdrops;   // one per monitor in use

static std::wstring split_dir(int n) { return g_dir + L"Splitscreen\\P" + std::to_wstring(n); }

static HANDLE split_stop_event() {
    static HANDLE ev = CreateEventW(NULL, TRUE, FALSE, SPLIT_STOP_EVENT);
    return ev;
}

// ---- settings (EXVSLauncher.ini [Splitscreen]) ---------------------------------------------------------
static std::string random_serial() {
    static const char *a = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    std::string s;
    unsigned v = GetTickCount() ^ (GetCurrentProcessId() << 16);
    for (int i = 0; i < 4; i++) {
        unsigned r = 0; BCryptGenRandom(NULL, (PUCHAR)&r, sizeof(r), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        s += a[(r ^ (v >> (i * 5))) % 57];
    }
    return s;
}

static json split_get_state(const json &cfg) {
    Ini me(L"EXVSLauncher.ini");
    const char *S = "Splitscreen";
    json players = json::array();
    std::string chip = cfg.value("chipId", ""), code = cfg.value("accessCode", ""), serial = cfg.value("serial", "");
    for (int n = 1; n <= 4; n++) {
        std::string k = "P" + std::to_string(n);
        std::string def_in = n == 1 ? "xinput:0" : "xinput:" + std::to_string(n - 1);
        std::string ser = me.get(S, (k + "Serial").c_str());
        if (ser.empty() && n > 1) { ser = random_serial(); me.set(S, (k + "Serial").c_str(), ser); }   // a cabinet serial of its own
        players.push_back({ {"input", me.get(S, (k + "Input").c_str(), def_in.c_str())},
                            {"chipId", me.get(S, (k + "Chip").c_str(), n == 1 ? chip.c_str() : (chip.empty() ? ("Player" + std::to_string(n)) : chip + "_P" + std::to_string(n)).c_str())},
                            {"accessCode", me.get(S, (k + "Code").c_str(), code.c_str())},
                            {"serial", n == 1 ? me.get(S, "P1Serial", serial.c_str()) : ser},
                            {"monitor", me.geti(S, (k + "Monitor").c_str(), me.geti(S, "Monitor", 0))} });   // old single setting = default
    }
    json monitors = json::array();
    EnumDisplayMonitors(NULL, NULL, [](HMONITOR m, HDC, LPRECT, LPARAM lp) -> BOOL {
        MONITORINFOEXW mi; mi.cbSize = sizeof(mi); GetMonitorInfoW(m, &mi);
        ((json *)lp)->push_back({ {"w", mi.rcMonitor.right - mi.rcMonitor.left}, {"h", mi.rcMonitor.bottom - mi.rcMonitor.top},
                                  {"primary", (mi.dwFlags & MONITORINFOF_PRIMARY) != 0} });
        return TRUE;
    }, (LPARAM)&monitors);
    return { {"players", std::max(2, std::min(4, me.geti(S, "Players", 2)))}, {"iface", me.get(S, "Interface")},
             {"resolution", me.geti(S, "Resolution", 3)}, {"monitor", me.geti(S, "Monitor", 0)}, {"startLM", me.getb(S, "StartLM", true)}, {"freePlay", me.getb(S, "FreePlay", true)},
             {"p", players}, {"monitors", monitors},
             {"pending", { {"ips", me.get(S, "PendingIps")}, {"iface", me.get(S, "PendingInterface")}, {"converted", me.getb(S, "PendingConverted", false)} }} };
}

static void split_save_state(const json &sp) {
    Ini me(L"EXVSLauncher.ini");
    const char *S = "Splitscreen";
    me.set(S, "Players", std::to_string(std::max(2, std::min(4, ji(sp, "players", 2)))));
    me.set(S, "Interface", js(sp, "iface"));
    me.set(S, "Resolution", std::to_string(ji(sp, "resolution", 3)));
    me.set(S, "Monitor", std::to_string(ji(sp, "monitor", 0)));
    me.set(S, "StartLM", tf(jb(sp, "startLM", true)));
    me.set(S, "FreePlay", tf(jb(sp, "freePlay", true)));
    if (!sp.contains("p") || !sp["p"].is_array()) return;
    for (int n = 1; n <= 4 && n <= (int)sp["p"].size(); n++) {
        const json &p = sp["p"][n - 1];
        std::string k = "P" + std::to_string(n);
        me.set(S, (k + "Input").c_str(), js(p, "input"));
        me.set(S, (k + "Chip").c_str(), js(p, "chipId"));
        me.set(S, (k + "Code").c_str(), js(p, "accessCode"));
        if (!js(p, "serial").empty()) me.set(S, (k + "Serial").c_str(), js(p, "serial"));
        me.set(S, (k + "Monitor").c_str(), std::to_string(std::max(0, ji(p, "monitor", 0))));
    }
}

// ---- controllers for the player list: XInput slots (with a live "pressed" flag) and DirectInput pads -----
static json split_pads() {
    json x = json::array();
    auto get = xinput_get();
    for (DWORD i = 0; i < 4; i++) {
        XState s = {};
        bool c = get && get(i, &s) == ERROR_SUCCESS;
        bool pressed = c && (s.pad.buttons || s.pad.lt > 60 || s.pad.rt > 60 || abs(s.pad.lx) > 16000 || abs(s.pad.ly) > 16000);
        x.push_back({ {"slot", (int)i}, {"connected", c}, {"pressed", pressed} });
    }
    json d = json::array();
    for (auto &dev : list_devices()) if (!pad_is_xinput(dev.name)) d.push_back(dev.name);
    return { {"xinput", x}, {"devices", d} };
}

// ---- network: the adapter's address and free addresses next to it -----------------------------------------
struct AdapterInfo { bool found = false; ULONG ip = 0; int prefix = 24; std::vector<ULONG> all; bool up = false; };
static std::string ip_str(ULONG ip);
// `extra` = addresses added by an earlier splitscreen session (not the adapter's own one)
static AdapterInfo adapter_info(const std::string &name, const std::vector<std::string> &extra = {}) {
    AdapterInfo r;
    ULONG sz = 65536; std::vector<char> buf(sz);
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL,
                             (IP_ADAPTER_ADDRESSES *)buf.data(), &sz) != NO_ERROR) return r;
    for (auto *a = (IP_ADAPTER_ADDRESSES *)buf.data(); a; a = a->Next) {
        if (narrow(a->FriendlyName) != name) continue;
        r.found = true; r.up = a->OperStatus == IfOperStatusUp;
        for (auto *u = a->FirstUnicastAddress; u; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
            ULONG ip = ((sockaddr_in *)u->Address.lpSockaddr)->sin_addr.s_addr;
            if ((ntohl(ip) >> 16) == 0xA9FE) continue;   // 169.254.x.x
            r.all.push_back(ip);
            if (r.ip || std::find(extra.begin(), extra.end(), ip_str(ip)) != extra.end()) continue;
            r.ip = ip; r.prefix = u->OnLinkPrefixLength;   // the adapter's own address
        }
        break;
    }
    return r;
}
static std::string ip_str(ULONG ip) { in_addr a; a.s_addr = ip; char b[32]; inet_ntop(AF_INET, &a, b, sizeof(b)); return b; }

// `count` addresses in the adapter's subnet that nobody answers ARP for, from the top of the range down.
// Addresses left on the adapter by an earlier session (`reuse`) are taken first.
static std::vector<std::string> pick_free_ips(const AdapterInfo &ad, int count, const std::vector<std::string> &reuse) {
    std::vector<std::string> out;
    for (auto &r : reuse) {
        for (ULONG a : ad.all) if (ip_str(a) == r && a != ad.ip && (int)out.size() < count) out.push_back(r);
    }
    if ((int)out.size() >= count) return out;
    int prefix = std::max(8, std::min(30, ad.prefix));
    ULONG host = ntohl(ad.ip), mask = prefix == 0 ? 0 : (0xFFFFFFFFu << (32 - prefix));
    ULONG net = host & mask, bcast = net | ~mask;
    std::vector<ULONG> cand;
    for (ULONG h = bcast - 5; h > net && cand.size() < 40; h--) {   // .250 and down on a /24
        if (h == host) continue;
        bool local = false;
        for (ULONG a : ad.all) if (ntohl(a) == h) local = true;
        if (!local) cand.push_back(h);
    }
    // ARP in parallel batches: a free address costs ~1-3 s of timeout
    for (size_t at = 0; at < cand.size() && (int)out.size() < count; at += 8) {
        size_t n = std::min<size_t>(8, cand.size() - at);
        std::vector<char> used(n, 0);
        std::vector<std::thread> ts;
        for (size_t i = 0; i < n; i++) ts.emplace_back([&, i] {
            ULONG mac[2]; ULONG len = 6;
            used[i] = SendARP(htonl(cand[at + i]), ad.ip, mac, &len) == NO_ERROR && len > 0;
        });
        for (auto &t : ts) t.join();
        for (size_t i = 0; i < n && (int)out.size() < count; i++) if (!used[i]) out.push_back(ip_str(htonl(cand[at + i])));
    }
    return out;
}

// ---- the elevated helper ------------------------------------------------------------------------------------
static std::string ps_quote(const std::string &s) {   // a PowerShell single-quoted string
    std::string o = "'";
    for (char c : s) { if (c == '\'') o += "''"; else o += c; }
    return o + "'";
}
// Runs a PowerShell script (passed with -EncodedCommand, so no quoting problems) and returns its exit code.
static DWORD run_ps(const std::string &script, std::string *output, DWORD timeout_ms) {
    std::wstring w = widen(script);
    std::string bytes((const char *)w.data(), w.size() * 2);
    std::wstring cmd = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " + widen(base64(bytes));
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return (DWORD)-1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = { sizeof(si) }; si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = wr; si.hStdError = wr;
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, &cmd[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, g_dir.c_str(), &si, &pi)) { CloseHandle(rd); CloseHandle(wr); return (DWORD)-1; }
    CloseHandle(wr); CloseHandle(pi.hThread);
    std::string out; char b[1024]; DWORD got;
    std::thread reader([&] { while (ReadFile(rd, b, sizeof(b), &got, NULL) && got) out.append(b, got); });
    if (WaitForSingleObject(pi.hProcess, timeout_ms) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
    reader.join(); CloseHandle(rd);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess);
    // PowerShell writes to a pipe in the console code page (e.g. 850 on a Spanish Windows), not UTF-8
    if (!out.empty() && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, out.data(), (int)out.size(), NULL, 0))
        out = narrow(widen(out, CP_OEMCP));
    if (output) *output = out;
    return code;
}

// Adds the addresses for this session only (ActiveStore: gone after a reboot as well).
// Windows turns DHCP off on an adapter as soon as an address is added to it by hand, and the DHCP address then
// goes away when the extra ones are removed (the adapter was left with only 169.254.x.x). So on a DHCP adapter
// the script first switches it to the same address, gateway and DNS set by hand, and prints CONVERTED before
// touching anything: the cleanup then always turns DHCP back on, even after a failure halfway.
static std::string net_add_script(const std::string &iface, const std::vector<std::string> &ips, int prefix) {
    std::string list;
    for (auto &ip : ips) list += (list.empty() ? "" : ",") + ps_quote(ip);
    return "$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'; $a=" + ps_quote(iface) + "; $p=" + std::to_string(prefix) + "; $ips=@(" + list + ");\n"
           "try {\n"
           "$if = Get-NetIPInterface -InterfaceAlias $a -AddressFamily IPv4\n"
           "if ($if.Dhcp -eq 'Enabled') {\n"
           "  $cur = Get-NetIPAddress -InterfaceAlias $a -AddressFamily IPv4 | Where-Object { $_.PrefixOrigin -eq 'Dhcp' } | Select-Object -First 1\n"
           "  if (-not $cur) { throw 'The adapter has no address from DHCP' }\n"
           "  $gw = (Get-NetRoute -InterfaceAlias $a -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue | Select-Object -First 1).NextHop\n"
           "  $dns = (Get-DnsClientServerAddress -InterfaceAlias $a -AddressFamily IPv4).ServerAddresses\n"
           "  Write-Output 'CONVERTED'\n"
           "  Set-NetIPInterface -InterfaceAlias $a -AddressFamily IPv4 -Dhcp Disabled\n"
           "  Start-Sleep -Milliseconds 500\n"
           "  if (-not (Get-NetIPAddress -InterfaceAlias $a -IPAddress $cur.IPAddress -ErrorAction SilentlyContinue)) {\n"
           "    New-NetIPAddress -InterfaceAlias $a -IPAddress $cur.IPAddress -PrefixLength $cur.PrefixLength -PolicyStore ActiveStore | Out-Null }\n"
           "  if ($gw -and -not (Get-NetRoute -InterfaceAlias $a -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue)) {\n"
           "    New-NetRoute -InterfaceAlias $a -DestinationPrefix '0.0.0.0/0' -NextHop $gw -PolicyStore ActiveStore | Out-Null }\n"
           "  if ($dns) { Set-DnsClientServerAddress -InterfaceAlias $a -ServerAddresses $dns }\n"
           "}\n"
           "foreach ($ip in $ips) {\n"
           "  if (Get-NetIPAddress -InterfaceAlias $a -IPAddress $ip -ErrorAction SilentlyContinue) { continue }\n"
           "  New-NetIPAddress -InterfaceAlias $a -IPAddress $ip -PrefixLength $p -SkipAsSource $true -PolicyStore ActiveStore | Out-Null\n"
           "}\n"
           "Write-Output 'ADDED'\n"
           "} catch { Write-Output ('ERROR: ' + $_.Exception.Message); exit 1 }";
}
static std::string net_remove_script(const std::string &iface, const std::vector<std::string> &ips, bool converted) {
    std::string list;
    for (auto &ip : ips) list += (list.empty() ? "" : ",") + ps_quote(ip);
    std::string s = "$ProgressPreference='SilentlyContinue'; $a=" + ps_quote(iface) + "; $ips=@(" + list + ");\n"
                    "foreach ($ip in $ips) { Remove-NetIPAddress -InterfaceAlias $a -IPAddress $ip -Confirm:$false -ErrorAction SilentlyContinue }\n";
    if (converted)   // back to DHCP, as it was before the session, and ask for an address right away
        s += "Remove-NetRoute -InterfaceAlias $a -DestinationPrefix '0.0.0.0/0' -Confirm:$false -ErrorAction SilentlyContinue\n"
             "Get-NetIPAddress -InterfaceAlias $a -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.PrefixOrigin -eq 'Manual' } | Remove-NetIPAddress -Confirm:$false -ErrorAction SilentlyContinue\n"
             "Set-NetIPInterface -InterfaceAlias $a -AddressFamily IPv4 -Dhcp Enabled\n"
             "Set-DnsClientServerAddress -InterfaceAlias $a -ResetServerAddresses\n"
             "& ipconfig.exe /renew $a | Out-Null\n";
    return s + "Write-Output 'REMOVED'";
}

static std::vector<std::string> split_csv(const std::string &s) {
    std::vector<std::string> v; std::stringstream ss(s); std::string x;
    while (std::getline(ss, x, ',')) { x = trim(x); if (!x.empty()) v.push_back(x); }
    return v;
}
static std::string join_csv(const std::vector<std::string> &v) { std::string o; for (auto &x : v) o += (o.empty() ? "" : ",") + x; return o; }
static bool ip_like(const std::string &s) { in_addr a; return inet_pton(AF_INET, s.c_str(), &a) == 1; }

// --split-helper <dir> <pipe> <launcher pid>: elevated. Request {"op":"session"|"cleanup", iface, ips, prefix, converted}.
// "session": add the addresses, answer "ready" (or "error: ..."), then keep them until the launcher says stop
// (event) or, if the launcher is closed, until no copy of the game runs any more; then remove them.
static int split_helper(const wchar_t *dir, const wchar_t *pipe_name, DWORD launcher_pid) {
    init_dir(dir);
    HANDLE p = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 50 && p == INVALID_HANDLE_VALUE; i++) {
        p = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (p == INVALID_HANDLE_VALUE) { if (GetLastError() == ERROR_PIPE_BUSY) WaitNamedPipeW(pipe_name, 2000); else Sleep(100); }
    }
    if (p == INVALID_HANDLE_VALUE) return 2;
    ULONG server = 0;
    if (!GetNamedPipeServerProcessId(p, &server) || server != launcher_pid) { CloseHandle(p); return 3; }
    auto say = [p](const std::string &s) { std::string l = s + "\n"; DWORD w; WriteFile(p, l.data(), (DWORD)l.size(), &w, NULL); };
    std::string req; char c; DWORD n;
    while (req.size() < 65536 && ReadFile(p, &c, 1, &n, NULL) && n && c != '\n') req += c;
    json j = json::parse(req, nullptr, false);
    std::string iface = j.is_object() ? j.value("iface", "") : "";
    std::vector<std::string> ips;
    if (j.is_object() && j.contains("ips") && j["ips"].is_array()) for (auto &x : j["ips"]) if (x.is_string() && ip_like(x.get<std::string>())) ips.push_back(x.get<std::string>());
    if (iface.empty() || ips.empty()) { say("error: bad request"); CloseHandle(p); return 4; }
    std::string op = j.value("op", "session");
    if (op == "cleanup") {
        std::string out; DWORD rc = run_ps(net_remove_script(iface, ips, j.value("converted", false)), &out, 60000);
        say(rc == 0 && out.find("REMOVED") != std::string::npos ? "removed" : "error: " + trim(out));
        CloseHandle(p); return 0;
    }
    int prefix = std::max(8, std::min(30, j.value("prefix", 24)));
    std::string out;
    DWORD rc = run_ps(net_add_script(iface, ips, prefix), &out, 90000);
    bool converted = out.find("CONVERTED") != std::string::npos;
    if (rc != 0 || out.find("ADDED") == std::string::npos) {
        run_ps(net_remove_script(iface, ips, converted), NULL, 60000);   // whatever was added before the failure
        std::string msg = trim(out);
        size_t e = msg.find("ERROR: "); if (e != std::string::npos) msg = trim(msg.substr(e + 7));
        if (msg.size() > 400) msg = msg.substr(0, 400);
        say("error: " + (msg.empty() ? "PowerShell exit code " + std::to_string(rc) : msg));
        CloseHandle(p); return 5;
    }
    say(converted ? "ready converted" : "ready");

    HANDLE stop = OpenEventW(SYNCHRONIZE, FALSE, SPLIT_STOP_EVENT);
    HANDLE launcher = OpenProcess(SYNCHRONIZE, FALSE, launcher_pid);
    bool orphan = false;
    for (;;) {
        std::vector<HANDLE> hs;
        if (stop) hs.push_back(stop);
        if (launcher && !orphan) hs.push_back(launcher);
        DWORD r = hs.empty() ? WAIT_TIMEOUT : WaitForMultipleObjects((DWORD)hs.size(), hs.data(), FALSE, 3000);
        if (r == WAIT_TIMEOUT) { if ((orphan || hs.empty()) && find_procs(CLIENT_EXE, true).empty()) break; continue; }
        if (r == WAIT_FAILED) { Sleep(3000); if (find_procs(CLIENT_EXE, true).empty()) break; continue; }
        if (hs[r - WAIT_OBJECT_0] == stop) break;
        orphan = true;   // the launcher was closed: keep the addresses while the game runs
    }
    run_ps(net_remove_script(iface, ips, converted), NULL, 60000);
    say("removed");
    if (stop) CloseHandle(stop);
    if (launcher) CloseHandle(launcher);
    CloseHandle(p);
    return 0;
}

// Starts the helper and sends it one request. On success the pipe stays open (session) and is returned.
static HANDLE split_helper_start(const json &request, std::string &reply, std::string &err) {
    std::wstring pipe_name = L"\\\\.\\pipe\\EXVSLauncher.split." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount());
    HANDLE pipe = CreateNamedPipeW(pipe_name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 4096, 4096, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) { err = "Could not create a pipe"; return NULL; }
    wchar_t self[MAX_PATH]; GetModuleFileNameW(NULL, self, MAX_PATH);
    std::wstring params = L"--split-helper \"" + g_dir.substr(0, g_dir.size() - 1) + L"\" " + pipe_name + L" " + std::to_wstring(GetCurrentProcessId());
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    SHELLEXECUTEINFOW se = { sizeof(se) };
    se.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    se.hwnd = g_hwnd; se.lpVerb = g_split_verb; se.lpFile = self; se.lpParameters = params.c_str(); se.nShow = SW_HIDE;
    BOOL launched = ShellExecuteExW(&se);
    DWORD e = GetLastError();
    CoUninitialize();
    if (!launched) {
        CloseHandle(pipe);
        err = e == ERROR_CANCELLED ? "Splitscreen needs administrator rights to add the players' network addresses: accept the Windows prompt"
                                   : "Could not start the network helper (error " + std::to_string(e) + ")";
        return NULL;
    }
    OVERLAPPED ov = {}; ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    bool connected = ConnectNamedPipe(pipe, &ov) != FALSE;
    e = GetLastError();
    if (!connected && e == ERROR_PIPE_CONNECTED) connected = true;
    else if (!connected && e == ERROR_IO_PENDING) {
        HANDLE hs[2] = { ov.hEvent, se.hProcess };
        DWORD r = WaitForMultipleObjects(se.hProcess ? 2 : 1, hs, FALSE, 20000);
        DWORD x;
        if (r != WAIT_OBJECT_0) CancelIoEx(pipe, &ov);
        connected = GetOverlappedResult(pipe, &ov, &x, TRUE) != FALSE && r == WAIT_OBJECT_0;
    }
    CloseHandle(ov.hEvent);
    if (se.hProcess) CloseHandle(se.hProcess);
    std::string req = request.dump() + "\n";
    DWORD w;
    if (!connected || !pipe_io(pipe, true, &req[0], (DWORD)req.size(), &w)) { CloseHandle(pipe); err = "The network helper did not start"; return NULL; }
    // first line = the answer (adding addresses takes a few seconds)
    reply.clear(); char ch; DWORD got;
    while (reply.size() < 4096 && pipe_io(pipe, false, &ch, 1, &got) && ch != '\n') reply += ch;
    reply = trim(reply);
    if (reply.empty()) { CloseHandle(pipe); err = "The network helper stopped without answering"; return NULL; }
    return pipe;
}

// ---- svd1 files (29\testmode_pcb.json): "svd1", save count, 64-bit body length, SHA-1 of (first 16 bytes + body), body
static bool sha1_of(const std::vector<unsigned char> &a, const std::vector<unsigned char> &b, unsigned char out[20]) {
    BCRYPT_ALG_HANDLE alg = NULL; BCRYPT_HASH_HANDLE h = NULL; bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, NULL, 0) == 0 && BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0) == 0) {
        BCryptHashData(h, (PUCHAR)a.data(), (ULONG)a.size(), 0);
        if (!b.empty()) BCryptHashData(h, (PUCHAR)b.data(), (ULONG)b.size(), 0);
        ok = BCryptFinishHash(h, out, 20, 0) == 0;
    }
    if (h) BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}
static bool read_file(const std::wstring &p, std::vector<unsigned char> &out) {
    FILE *f = _wfopen(p.c_str(), L"rb"); if (!f) return false;
    out.clear(); unsigned char b[65536]; size_t n;
    while ((n = fread(b, 1, sizeof(b), f)) > 0) out.insert(out.end(), b, b + n);
    fclose(f); return true;
}
static bool read_svd1(const std::wstring &p, json &j, std::vector<unsigned char> &f) {
    if (!read_file(p, f) || f.size() < 36 || memcmp(f.data(), "svd1", 4) != 0) return false;
    unsigned long long len; memcpy(&len, f.data() + 8, 8);
    if (len > f.size() - 36) return false;
    j = json::parse(std::string((const char *)f.data() + 36, (size_t)len), nullptr, false);
    return j.is_object() && j.contains("storage") && j["storage"].is_object();
}
// A player's testmode_pcb.json: starts from the game folder's file when the player has none yet (the test menu
// settings are kept), then sets the cabinet number (networkId), Free play (test menu › coin settings) and copies
// the debug menu options (debugParams) of the game folder's client, so what's turned on there applies to every
// player. False if there is no valid svd1 file.
static bool write_player_testmode(const std::wstring &src, const std::wstring &dst, int id, bool free_play) {
    json j, root; std::vector<unsigned char> f, rf;
    bool have_root = read_svd1(src, root, rf);
    if (!(exists(dst) && read_svd1(dst, j, f))) { if (!have_root) return false; j = root; f = rf; }
    json &st = j["storage"];
    st[".TestModeParam.game.networkId"] = { {"value", id}, {"type", 1} };
    const char *fp = ".TestModeParam.coin.freePlay";
    int root_fp = have_root && root["storage"].contains(fp) ? root["storage"][fp].value("value", 0) : 0;
    st[fp] = { {"value", free_play ? 1 : root_fp}, {"type", 1} };
    if (have_root)
        for (auto it = root["storage"].begin(); it != root["storage"].end(); ++it)
            if (it.key().rfind(".TestModeParam.debugParam.debugParams.", 0) == 0) st[it.key()] = it.value();
    std::string nb = j.dump(2);
    std::vector<unsigned char> head(f.begin(), f.begin() + 16), bv(nb.begin(), nb.end());
    unsigned long long nl = nb.size(); memcpy(head.data() + 8, &nl, 8);
    unsigned char dg[20]; if (!sha1_of(head, bv, dg)) return false;
    FILE *o = _wfopen(dst.c_str(), L"wb"); if (!o) return false;
    fwrite(head.data(), 1, 16, o); fwrite(dg, 1, 20, o); fwrite(bv.data(), 1, bv.size(), o); fclose(o);
    return true;
}

// ---- windows: a grid of 16:9 tiles on one monitor ------------------------------------------------------------
static RECT monitor_rect(int index) {
    struct Ctx { int want, at; RECT r; bool found; } ctx = { index, 0, {}, false };
    EnumDisplayMonitors(NULL, NULL, [](HMONITOR m, HDC, LPRECT, LPARAM lp) -> BOOL {
        Ctx *c = (Ctx *)lp;
        if (c->at++ == c->want) { MONITORINFO mi = { sizeof(mi) }; GetMonitorInfoW(m, &mi); c->r = mi.rcMonitor; c->found = true; return FALSE; }
        return TRUE;
    }, (LPARAM)&ctx);
    if (!ctx.found) { MONITORINFO mi = { sizeof(mi) }; GetMonitorInfoW(MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY), &mi); ctx.r = mi.rcMonitor; }
    return ctx.r;
}
// Player n (1-based) of the `count` players on one monitor: 1 = the whole monitor (16:9), 2 = side by side,
// 3-4 = 2x2 (the third player centred below when there are 3).
static RECT split_tile(const RECT &mon, int n, int count) {
    int mw = mon.right - mon.left, mh = mon.bottom - mon.top;
    int cols = count <= 1 ? 1 : 2, rows = count <= 2 ? 1 : 2;
    int cw = mw / cols, ch = mh / rows;
    int tw = cw, th = cw * 9 / 16;
    if (th > ch) { th = ch; tw = ch * 16 / 9; }
    int gx = mon.left + (mw - tw * cols) / 2, gy = mon.top + (mh - th * rows) / 2;
    int i = n - 1, cx = i % cols, cy = i / cols;
    int x = gx + cx * tw, y = gy + cy * th;
    if (count == 3 && n == 3) x = mon.left + (mw - tw) / 2;
    return { x, y, x + tw, y + th };
}
static void place_split_window(HWND h, const RECT &r) {
    LONG_PTR st = GetWindowLongPtrW(h, GWL_STYLE);
    st = (st & ~(WS_OVERLAPPEDWINDOW | WS_BORDER | WS_DLGFRAME)) | WS_POPUP;
    SetWindowLongPtrW(h, GWL_STYLE, st | WS_VISIBLE);
    SetWindowPos(h, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
}
static bool split_window_ok(HWND h, const RECT &r) {
    RECT wr, cr; GetWindowRect(h, &wr); GetClientRect(h, &cr);
    return EqualRect(&wr, &r) && cr.right == r.right - r.left && cr.bottom == r.bottom - r.top &&
           !(GetWindowLongPtrW(h, GWL_STYLE) & (WS_CAPTION | WS_THICKFRAME | WS_BORDER | WS_DLGFRAME));
}
static HWND main_window_of(DWORD pid) { FindWin f = { pid, NULL, 0 }; EnumWindows(find_win_cb, (LPARAM)&f); return f.best; }

// black window behind the game windows, so the gaps between tiles don't show the desktop
static void backdrop_show(const RECT &mon) {
    std::thread([mon] {
        WNDCLASSW wc = {}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"EXVSLauncherSplitBackdrop"; wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        HWND h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"EXVS Splitscreen", WS_POPUP,
                                 mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top, NULL, NULL, wc.hInstance, NULL);
        if (!h) return;
        { std::lock_guard<std::mutex> l(g_split_mx); g_split_backdrops.push_back(h); }
        ShowWindow(h, SW_SHOWNOACTIVATE);
        MSG m;
        while (GetMessageW(&m, NULL, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
    }).detach();
}
static void backdrop_hide() {
    std::vector<HWND> hs; { std::lock_guard<std::mutex> l(g_split_mx); hs.swap(g_split_backdrops); }
    for (HWND h : hs) { PostMessageW(h, WM_CLOSE, 0, 0); PostMessageW(h, WM_QUIT, 0, 0); }
}

// Where each player's window goes: players on the same monitor share it as a grid (in player order).
static std::vector<RECT> split_rects(const std::vector<SplitPlayer> &pl, std::vector<int> *monitors_used = NULL) {
    std::vector<RECT> out;
    for (size_t i = 0; i < pl.size(); i++) {
        int on = 0, k = 0;
        for (size_t j = 0; j < pl.size(); j++) if (pl[j].monitor == pl[i].monitor) { on++; if (j < i) k++; }
        out.push_back(split_tile(monitor_rect(pl[i].monitor), k + 1, on));
        if (monitors_used && std::find(monitors_used->begin(), monitors_used->end(), pl[i].monitor) == monitors_used->end()) monitors_used->push_back(pl[i].monitor);
    }
    return out;
}

// ---- start / stop --------------------------------------------------------------------------------------------
static json split_fail(const std::string &e) { log_line("splitscreen: %s", e.c_str()); return { {"ok", false}, {"error", e} }; }

// The players from the settings; "" or what is wrong.
static std::string split_players(const json &state, int count, std::vector<SplitPlayer> &pl) {
    const json &sp = state["split"];
    if (!sp.contains("p") || !sp["p"].is_array() || (int)sp["p"].size() < count) return "No player settings";
    int keyboards = 0; bool slot_used[4] = {};
    for (int n = 1; n <= count; n++) {
        const json &p = sp["p"].at(n - 1);
        SplitPlayer x = { js(p, "input"), trim(js(p, "chipId")), trim(js(p, "accessCode")), trim(js(p, "serial")), std::max(0, ji(p, "monitor", 0)) };
        std::string who = "Player " + std::to_string(n);
        if (x.input == "keyboard") keyboards++;
        else if (x.input.rfind("xinput:", 0) == 0) {
            int s = atoi(x.input.c_str() + 7);
            if (s < 0 || s > 3) return (who + ": choose a controller");
            if (slot_used[s]) return ("Two players have controller " + std::to_string(s + 1));
            slot_used[s] = true;
        } else if (x.input.rfind("di:", 0) != 0 || x.input.size() < 4) return (who + ": choose a controller or the keyboard");
        if (x.chip.empty()) return (who + ": enter a card (chip ID)");
        if (x.serial.empty()) x.serial = n == 1 ? js(state["cfg"], "serial") : random_serial();
        for (auto &o : pl) if (o.chip == x.chip) return ("Each player needs their own card (" + who + " has the same chip ID as another player)");
        pl.push_back(x);
    }
    if (keyboards > 1) return "Only one player can use the keyboard";
    return "";
}

// Each player's folder: config.ini (a copy of the game folder's, with this player's address, serial, controller and
// [split] section), card.ini, and 29\testmode_pcb.json with cabinet number = player - 1.
static void split_write_players(const json &state, const std::vector<SplitPlayer> &pl, const std::vector<std::string> &ips, int res) {
    int count = (int)pl.size();
    const json &pads = state["controller"];
    for (int n = 1; n <= count; n++) {
        const SplitPlayer &x = pl[n - 1];
        std::wstring d = split_dir(n);
        CreateDirectoryW((g_dir + L"Splitscreen").c_str(), NULL);
        CreateDirectoryW(d.c_str(), NULL);
        CreateDirectoryW((d + L"\\29").c_str(), NULL);
        CopyFileW(path_of(L"config.ini").c_str(), (d + L"\\config.ini").c_str(), FALSE);
        Ini c((L"Splitscreen\\P" + std::to_wstring(n) + L"\\config.ini").c_str());
        c.set("config", "mode", "Client");
        c.set("config", "serial", x.serial);
        c.set("config", "IpAddress", ips[n - 1]);
        c.del("config", "InterfaceName");
        c.set("config", "Windowed", "True");
        c.set("config", "BorderlessWindow", "False");
        c.set("config", "UseRealCardReader", "False");
        c.set("display", "Resolution", RES_NAMES[res]);
        c.set("keyboard", "Enabled", "True");   // the overlay lets only the keyboard player's copy see it
        int slot = -1;
        if (x.input.rfind("xinput:", 0) == 0) {
            slot = atoi(x.input.c_str() + 7);
            c.set("controller", "Enabled", "True");
            c.set("controller", "Mode", "xinput-native");
        } else if (x.input.rfind("di:", 0) == 0) {
            c.set("controller", "Enabled", "True");
            c.set("controller", "Name", x.input.substr(3));
            c.del("controller", "Mode");
            if (pads.is_array() && !pads.empty()) write_buttons(c, "controller", pads[0], true);   // Controller 1's mapping
        } else {
            c.set("controller", "Enabled", "False");
        }
        c.set("controller-1", "Enabled", "False");
        c.set("split", "Enabled", "1");
        c.set("split", "Player", std::to_string(n));
        c.set("split", "Pad", std::to_string(slot));
        c.set("split", "Keyboard", x.input == "keyboard" ? "1" : "0");
        Ini card((L"Splitscreen\\P" + std::to_wstring(n) + L"\\card.ini").c_str());
        card.set("card", "chipId", x.chip);
        card.set("card", "accessCode", x.code);
        if (!write_player_testmode(path_of(L"29\\testmode_pcb.json"), d + L"\\29\\testmode_pcb.json", n - 1, jb(state["split"], "freePlay", true)))
            log_line("splitscreen: P%d testmode_pcb.json not written (no valid 29\\testmode_pcb.json in the game folder yet)", n);
        for (const wchar_t *f : { L"charge_data.bin" })
            if (!exists(d + L"\\29\\" + f)) CopyFileW(path_of((std::wstring(L"29\\") + f).c_str()).c_str(), (d + L"\\29\\" + f).c_str(), TRUE);
    }
}

static void split_session_end(HANDLE helper_pipe) {
    SetEvent(split_stop_event());   // the helper removes the addresses
    if (helper_pipe) {
        std::string line; char ch; DWORD got;   // wait for "removed" (or the helper exiting)
        for (int i = 0; i < 4096 && pipe_io(helper_pipe, false, &ch, 1, &got); i++) { if (ch == '\n') { if (trim(line) == "removed") break; line.clear(); } else line += ch; }
        CloseHandle(helper_pipe);
    }
    backdrop_hide();
    Ini me(L"EXVSLauncher.ini");
    me.set("Splitscreen", "PendingIps", ""); me.set("Splitscreen", "PendingInterface", ""); me.set("Splitscreen", "PendingConverted", "False");
    { std::lock_guard<std::mutex> l(g_split_mx); g_split_pids.clear(); g_split_ips.clear(); }
    g_split_running = false;
    log_line("splitscreen: session ended, network addresses removed");
}

static json do_split(json state) {
    if (g_split_starting.exchange(true)) return split_fail("Splitscreen is already starting");
    struct Done { ~Done() { g_split_starting = false; set_busy(""); } } done;
    if (g_split_running) return split_fail("Splitscreen is already running");
    if (!state.contains("split") || !state["split"].is_object()) return split_fail("No splitscreen settings");
    const json &sp = state["split"];
    int count = std::max(2, std::min(4, ji(sp, "players", 2)));
    std::string iface = trim(js(sp, "iface"));
    int res = ji(sp, "resolution", 3); if (res < 0 || res > 6) res = 3;
    if (!exists(path_of(CLIENT_EXE))) return split_fail("vsac29_Release.exe not found next to the launcher");
    if (!find_procs(CLIENT_EXE, true).empty()) return split_fail("The game is already running: close it first");
    for (wchar_t ch : g_dir) if (ch >= 0x2E80) return split_fail("The game folder path has non-Latin characters; the game can't start from there");
    std::string ov = overlay_status();
    if (ov == "missing" || ov == "outdated") return split_fail(std::string("Splitscreen needs the EXVS Overlay ") + SPLIT_OVERLAY_MIN + " or newer (it gives each player their own controller): install or update it in Settings › Graphics");
    if (iface.empty()) return split_fail("Choose the network adapter for splitscreen");
    if (iface.find_first_of("'\"") != std::string::npos) return split_fail("The adapter name can't contain quotes");
    std::vector<SplitPlayer> pl;
    std::string perr = split_players(state, count, pl);
    if (!perr.empty()) return split_fail(perr);

    Ini me(L"EXVSLauncher.ini");
    std::vector<std::string> left = me.get("Splitscreen", "PendingInterface") == iface ? split_csv(me.get("Splitscreen", "PendingIps")) : std::vector<std::string>();
    AdapterInfo ad = adapter_info(iface, left);
    if (!ad.found || !ad.ip) return split_fail("The adapter '" + iface + "' has no IPv4 address");
    if (!ad.up) return split_fail("The adapter '" + iface + "' is not connected");

    write_game_files(state);   // the game folder's own files (Live Monitor, keys, buttons)
    log_line("splitscreen: %d players on '%s' (%s/%d)", count, iface.c_str(), ip_str(ad.ip).c_str(), ad.prefix);

    // network addresses: one per player
    set_busy("Looking for free network addresses…");
    std::vector<std::string> ips = pick_free_ips(ad, count, left);
    if ((int)ips.size() < count) return split_fail("Not enough free addresses on the network of '" + iface + "'");
    set_busy("Adding the players' network addresses (accept the Windows prompt)…");
    ResetEvent(split_stop_event());
    std::string reply, err;
    HANDLE helper = split_helper_start({ {"op", "session"}, {"iface", iface}, {"ips", ips}, {"prefix", ad.prefix} }, reply, err);
    if (!helper) return split_fail(err);
    if (reply.rfind("ready", 0) != 0) { CloseHandle(helper); return split_fail("Could not add the network addresses: " + reply); }
    bool converted = reply.find("converted") != std::string::npos;
    me.set("Splitscreen", "PendingIps", join_csv(ips)); me.set("Splitscreen", "PendingInterface", iface); me.set("Splitscreen", "PendingConverted", tf(converted));
    log_line("splitscreen: addresses %s added%s", join_csv(ips).c_str(), converted ? " (adapter switched from DHCP to the same fixed address)" : "");

    set_busy("Writing the players' settings…");
    split_write_players(state, pl, ips, res);
    // the Live Monitor on this PC listens on the same adapter
    if (exists(path_of(L"lmnfig.ini"))) { Ini lm(L"lmnfig.ini"); lm.set("config", "InterfaceName", iface); lm.del("config", "IpAddress"); }

    if (jb(state["app"], "autoServer", false) && !server_running()) {
        set_busy("Starting the card server…");
        std::string e = start_server();
        if (!e.empty()) log_line("splitscreen: card server: %s", e.c_str());
        else { for (int i = 0; i < 10 && !server_running(); i++) Sleep(200); Sleep(1000); }
    }
    if (jb(sp, "startLM", true) && !lm_running()) {
        set_busy("Starting the Live Monitor…");
        std::wstring lm = lm_path();
        if (!exists(lm)) { split_session_end(helper); return split_fail(narrow(file_name(lm)) + " (Live Monitor program) was not found"); }
        DWORD_PTR et = SHGetFileInfoW(lm.c_str(), 0, NULL, 0, SHGFI_EXETYPE);
        bool console = et && HIWORD(et) == 0;
        if (!start_process(lm, L"", g_dir.substr(0, g_dir.size() - 1), console ? CREATE_NEW_CONSOLE : 0)) { split_session_end(helper); return split_fail("Could not start the Live Monitor (see EXVSLauncher.log)"); }
        Sleep(2500);
    }

    std::vector<int> mons;
    std::vector<RECT> rects = split_rects(pl, &mons);
    for (int m : mons) backdrop_show(monitor_rect(m));
    std::vector<HANDLE> procs;
    std::vector<DWORD> pids;
    for (int n = 1; n <= count; n++) {
        set_busy("Starting player " + std::to_string(n) + "…");
        PROCESS_INFORMATION pi;
        if (!start_process(path_of(CLIENT_EXE), L"\"" + split_dir(n) + L"\"", g_dir.substr(0, g_dir.size() - 1), 0, &pi)) {
            for (HANDLE h : procs) { TerminateProcess(h, 0); CloseHandle(h); }
            split_session_end(helper);
            return split_fail("Could not start player " + std::to_string(n) + " (see EXVSLauncher.log)");
        }
        CloseHandle(pi.hThread);
        procs.push_back(pi.hProcess); pids.push_back(pi.dwProcessId);
        if (n < count) Sleep(1500);
    }
    { std::lock_guard<std::mutex> l(g_split_mx); g_split_pids = pids; g_split_ips = ips; g_split_error.clear(); }
    g_split_running = true;
    if (g_hwnd) ShowWindow(g_hwnd, SW_MINIMIZE);

    // place the windows (the game resizes its window while it starts: keep them in place for 2 minutes)
    std::thread([pids, rects] {
        std::vector<int> forced(pids.size(), 0);
        for (int t = 0; t < 240 && g_split_running; t++) {
            Sleep(500);
            for (size_t i = 0; i < pids.size() && i < rects.size(); i++) {
                HWND h = main_window_of(pids[i]);
                if (!h) continue;
                const RECT &r = rects[i];
                if (forced[i] < 2 || !split_window_ok(h, r)) { place_split_window(h, r); forced[i]++; }
            }
        }
    }).detach();
    // the session ends when every copy has closed (or "Stop" closed them)
    std::thread([procs, helper] {
        WaitForMultipleObjects((DWORD)procs.size(), procs.data(), TRUE, INFINITE);
        for (HANDLE h : procs) CloseHandle(h);
        split_session_end(helper);
    }).detach();
    return { {"ok", true}, {"ips", ips} };
}

static void split_stop() {
    std::vector<DWORD> pids; { std::lock_guard<std::mutex> l(g_split_mx); pids = g_split_pids; }
    for (DWORD pid : pids) { HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid); if (h) { TerminateProcess(h, 0); CloseHandle(h); } }
    log_line("splitscreen: closed from the launcher");
}

// Removes addresses an earlier session left behind (the launcher and the game were closed abnormally while
// the adapter had been switched to a fixed address, or the PC wasn't restarted since).
static json split_cleanup() {
    if (g_split_running) return split_fail("Splitscreen is running");
    Ini me(L"EXVSLauncher.ini");
    std::vector<std::string> ips = split_csv(me.get("Splitscreen", "PendingIps"));
    std::string iface = me.get("Splitscreen", "PendingInterface");
    if (ips.empty() || iface.empty()) return { {"ok", true} };
    std::string reply, err;
    HANDLE h = split_helper_start({ {"op", "cleanup"}, {"iface", iface}, {"ips", ips}, {"converted", me.getb("Splitscreen", "PendingConverted", false)} }, reply, err);
    if (!h) return split_fail(err);
    CloseHandle(h);
    if (reply != "removed") return split_fail("Could not clean up the network: " + reply);
    me.set("Splitscreen", "PendingIps", ""); me.set("Splitscreen", "PendingInterface", ""); me.set("Splitscreen", "PendingConverted", "False");
    return { {"ok", true} };
}

static json split_status() {
    std::lock_guard<std::mutex> l(g_split_mx);
    int alive = 0;
    for (DWORD pid : g_split_pids) { HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid); if (h) { if (WaitForSingleObject(h, 0) == WAIT_TIMEOUT) alive++; CloseHandle(h); } }
    return { {"running", (bool)g_split_running}, {"starting", (bool)g_split_starting}, {"players", alive}, {"ips", g_split_ips} };
}
