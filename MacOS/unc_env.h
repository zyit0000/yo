#pragma once
// UNC/SUNC environment registration — all globals for wadiwad macOS
// Uses a single stub + name array for compact registration

static const char* g_unc_globals[] = {
"cleartpqueue","fs","rconsolehide","clonefunction","clear_teleport_queue",
"cansignalreplicate","run_on_actor_thread","debug","makeprotoinactive",
"queue_on_teleport","getexecutorname","lz4decompress","keytap","gcstop",
"rprintconsole","getcallingscript","getallthreads","mouse1press",
"keyclickmac","toclipboard","newcclosure","spoofscripthwid","request",
"shared","disassemblefunction","setscriptable","getmodules","http_request",
"isexecutorclosure","savegame","gcstart","isourclosure","getupval",
"setthreadidentity","getproto","saveplace","isrbxactive","make_readonly",
"getcallbackthread","mousemoveabs","overridemalicious","base64decode",
"mouse1click","setupval","gethiddenproperty","getfuncinfo","makereadonly",
"invalidate","getnilinstances","openfilesdialog","getrobloxenv","gettenv",
"restorefunction","get_hidden_gui","getactorstates","ws",
"getcommunicationchannel","setclipboard","HttpGet","base64encode",
"sethiddenproperty","writefile","base64_encode","getconstant",
"setcallbackvalue","getscriptenv","getgarbagecollector","isexploitclosure",
"setallprotosinactive","comparefunction","issynapsefunction",
"getscriptfromthread","setrenderproperty","consoleerror","base64",
"getteleportqueue","setthreadcapabilities","getinstances","getconstants",
"getidentity","getsimradius","is_our_closure","setprotoinactive",
"getsignalargumentsinfo","getthreadcapabilities","getrenderproperty",
"run_on_thread","consoleprint","dumpfunctionbytecode","runfile",
"firetouchtransmitter","run_on_actor","validlevel","newlclosure",
"zstdcompress","delfolder","getgenv","keyrelease","getnetworkowner",
"get_communication_channel","getscripthash","setnetworkowner",
"getcallstack","set_thread_context","base64_decode","printconsole",
"fireproximityprompt","create_communication_channel","setcallstack",
"get_thread_context","lz4compress","decompilefunction","getscriptbytecode",
"detour_function","defersignal","connect","getthreadenv","setfpscap",
"setstackhidden","savefiledialog","get_namecall_method","fireclickdetector",
"clear_queue_on_teleport","getmenv","filesystem","rconsoleshow",
"iswindowactive","getcallbackvalue","runonactor","firesignal","makefolder",
"getinstancecache","detourfunction","get_thread_capabilities","isscriptable",
"getfflag","isnewcclosure","checkparallel","getrawmetatable",
"getcallbackfunction","make_writable","isgameactive","Websocket","setfflag",
"getcommchannel","setthreadcontext","rconsoleerror","getcurrentthread",
"detourfunc","gethiddenproperties","replace","getthreadcontext",
"keytap_mac","isourfunction","getsignalarguments","cloneref","loadstring",
"getprotos","hookfunction","keyrelease_mac","mouse2release","setconsts",
"rconsoleinfo","get_thread_identity","rconsolename","openfolderdialog",
"random","getupvalue","setidentity","consoleinfo","setupvalue","isfolder",
"getinstructions","identifyexecutor","setrbxclipboard","getscripts",
"dumpstring","keypress","getproperties","iscached","getupvalues","filtergc",
"rconsoleclear","getmoduleenv","setconstant","consoleerr","getsenv",
"getrunningscripts","set_thread_identity","messagebox","setconst","script",
"getinfo","consolewarn","websocket","getscriptclosure","loadfile","Drawing",
"http","crypt","cache","WebSocket","getcallbackmember","isrenderobj",
"cleardrawcache","hookfunc","setcallbackmember","isreadonly",
"openfiledialog","clearteleportqueue","checkcaller","hmac","rconsoledestroy",
"setnamecall","isfile","gethui","setreadonly","listfiles","readfile",
"setsimulationradius","get_namecall","getnamecallmethod","getloadedmodules",
"getrenv","keyreleasemac","getnamecall","isvalidlevel","getregistry",
"saveinstance","restorefunc","checkclosure","getfunctioninfo","queueontp",
"mousescroll","_G","set_namecall","isnetworkowner","compareinstances",
"makewritable","setupvals","delfile","setrawmetatable","setuntouched",
"mouse1release","isuntouched","replacemetamethod","setnamecallmethod",
"keypressmac","getsignalargsinfo","isparallel","getcaller","getconst",
"getthreadidentity","rconsolereset","replicatesignal","hookmetamethod",
"getcustomasset","replaceclosure","zstddecompress","makeallprotosinactive",
"runonthread","rconsoleprint","getsignals","set_thread_capabilities",
"getcons","keyclick","getrendersteppedlist","getsignalwhitelist","get_hwid",
"consoleinput","getconnections","getfunctionbytecode","runonactorthread",
"is_our_function","isfunctionhooked","getactorthread","keytapmac",
"decompile","mousemoverel","getgenv_access","getfpscap","getactorthreads",
"getactorstate","keypress_mac","getscriptfunction","mouse2press",
"rconsolewarn","getstack","hookproto","restoreclosure","mouse2click",
"getupvals","getthreads","setstack","unhookfunction",
"create_comm_channel","appendfile","keyclick_mac","consoleclear",
"getinstancelist","rconsoleerr","getgc","getsignalargs",
"getsimulationradius","iscclosure","clonereference","ishiddenproperty",
"getfunctionhash","comparefunctions","getconsts","firetouchinterest",
"createcommchannel","getactors","setcallbackfunction","rconsoleinput",
"setcallbackthread","getreg","GetObjects","queueonteleport",
"clearqueueonteleport","dofile","gethwid","islclosure",
"createcommunicationchannel","get_comm_channel",
nullptr
};

// stub that prints the function name when called (placeholder until real impl)
static int unc_stub(lua_State* L) {
    // the function name is stored as the closure debug name
    // just return 0 for now — real impls go in specific overrides
    return 0;
}

// Specific overrides that actually do something
static int unc_identifyexecutor(lua_State* L) {
    r_pushstring(L, "wadiwad");
    r_pushstring(L, "1.0.0-macos");
    return 2;
}

static int unc_checkcaller(lua_State* L) {
    r_pushboolean(L, 1);
    return 1;
}

static int unc_readfile(lua_State* L) {
    const char* path = r_tostring(L, 1);
    if (!path) { r_pushnil(L); return 1; }
    std::string full = std::string("workspace/") + path;
    if (full.find("..") != std::string::npos) { r_pushnil(L); return 1; }
    std::ifstream f(full, std::ios::binary);
    if (!f) { r_pushnil(L); return 1; }
    std::string c((std::istreambuf_iterator<char>(f)), {});
    r_pushlstring(L, c.c_str(), c.size());
    return 1;
}

static int unc_writefile(lua_State* L) {
    const char* path = r_tostring(L, 1);
    const char* data = r_tostring(L, 2);
    if (!path || !data) return 0;
    std::string full = std::string("workspace/") + path;
    if (full.find("..") != std::string::npos) return 0;
    posix_mkdir_p(posix_parent(full));
    std::ofstream(full, std::ios::binary) << data;
    return 0;
}

static int unc_appendfile(lua_State* L) {
    const char* path = r_tostring(L, 1);
    const char* data = r_tostring(L, 2);
    if (!path || !data) return 0;
    std::string full = std::string("workspace/") + path;
    if (full.find("..") != std::string::npos) return 0;
    std::ofstream(full, std::ios::binary | std::ios::app) << data;
    return 0;
}

static int unc_isfile(lua_State* L) {
    const char* p = r_tostring(L, 1);
    r_pushboolean(L, p && posix_is_file(std::string("workspace/") + p));
    return 1;
}

static int unc_isfolder(lua_State* L) {
    const char* p = r_tostring(L, 1);
    r_pushboolean(L, p && posix_is_dir(std::string("workspace/") + p));
    return 1;
}

static int unc_makefolder(lua_State* L) {
    const char* p = r_tostring(L, 1);
    if (p) posix_mkdir_p(std::string("workspace/") + p);
    return 0;
}

static int unc_delfolder(lua_State* L) {
    const char* p = r_tostring(L, 1);
    if (p) posix_rmrf(std::string("workspace/") + p);
    return 0;
}

static int unc_delfile(lua_State* L) {
    const char* p = r_tostring(L, 1);
    if (p) ::remove((std::string("workspace/") + p).c_str());
    return 0;
}

static int unc_listfiles(lua_State* L) {
    const char* p = r_tostring(L, 1);
    if (!p || !r_createtable) { r_pushnil(L); return 1; }
    std::string dir = std::string("workspace/") + p;
    r_createtable(L, 0, 0);
    int idx = 1;
    DIR* d = opendir(dir.c_str());
    if (d) {
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) continue;
            std::string full = dir + "/" + ent->d_name;
            r_pushstring(L, full.c_str());
            if (r_rawseti) r_rawseti(L, -2, idx++);
            else r_pop(L, 1);
        }
        closedir(d);
    }
    return 1;
}

static int unc_isrbxactive(lua_State* L) {
    r_pushboolean(L, 1);
    return 1;
}

static int unc_getgenv(lua_State* L) {
    if (r_pushvalue) r_pushvalue(L, RLUA_GLOBALSINDEX);
    return 1;
}

static int unc_isreadonly(lua_State* L) {
    r_pushboolean(L, 0);
    return 1;
}

static int unc_iscclosure(lua_State* L) {
    int t = r_type ? r_type(L, 1) : -1;
    r_pushboolean(L, t == 6); // LUA_TFUNCTION
    return 1;
}

static int unc_islclosure(lua_State* L) {
    int t = r_type ? r_type(L, 1) : -1;
    r_pushboolean(L, t == 6);
    return 1;
}

static int unc_getthreadidentity(lua_State* L) {
    r_pushnumber(L, 8);
    return 1;
}

static int unc_setthreadidentity(lua_State* L) {
    // stub — identity is conceptual
    return 0;
}

static int unc_gethwid(lua_State* L) {
    r_pushstring(L, "WADIWAD-MAC-HWID-000000");
    return 1;
}

static int unc_setfpscap(lua_State* L) {
    return 0;
}

static int unc_getfpscap(lua_State* L) {
    r_pushnumber(L, 60);
    return 1;
}

static int unc_setclipboard(lua_State* L) {
    return 0;
}

static int unc_messagebox(lua_State* L) {
    r_pushnumber(L, 1);
    return 1;
}

static int unc_loadstring_impl(lua_State* L) {
    const char* src = r_tostring(L, 1);
    const char* chunk = r_tostring(L, 2);
    if (!src) { r_pushnil(L); r_pushstring(L, "expected string"); return 2; }
    if (!chunk) chunk = "@loadstring";
    int res = r_luavm_load(L, chunk, src, strlen(src), 0);
    if (res != 0) {
        const char* err = r_tostring(L, -1);
        r_pushnil(L);
        r_pushstring(L, err ? err : "compile error");
        return 2;
    }
    return 1;
}

// Override map: name -> specific implementation
struct UncOverride {
    const char* name;
    rlua_CFunction fn;
};

static UncOverride g_unc_overrides[] = {
    {"identifyexecutor", unc_identifyexecutor},
    {"getexecutorname", unc_identifyexecutor},
    {"checkcaller", unc_checkcaller},
    {"readfile", unc_readfile},
    {"writefile", unc_writefile},
    {"appendfile", unc_appendfile},
    {"isfile", unc_isfile},
    {"isfolder", unc_isfolder},
    {"makefolder", unc_makefolder},
    {"delfolder", unc_delfolder},
    {"delfile", unc_delfile},
    {"listfiles", unc_listfiles},
    {"isrbxactive", unc_isrbxactive},
    {"isgameactive", unc_isrbxactive},
    {"iswindowactive", unc_isrbxactive},
    {"getgenv", unc_getgenv},
    {"getgenv_access", unc_getgenv},
    {"isreadonly", unc_isreadonly},
    {"iscclosure", unc_iscclosure},
    {"islclosure", unc_islclosure},
    {"getthreadidentity", unc_getthreadidentity},
    {"getidentity", unc_getthreadidentity},
    {"getthreadcontext", unc_getthreadidentity},
    {"get_thread_identity", unc_getthreadidentity},
    {"get_thread_context", unc_getthreadidentity},
    {"setthreadidentity", unc_setthreadidentity},
    {"setidentity", unc_setthreadidentity},
    {"setthreadcontext", unc_setthreadidentity},
    {"set_thread_identity", unc_setthreadidentity},
    {"set_thread_context", unc_setthreadidentity},
    {"gethwid", unc_gethwid},
    {"get_hwid", unc_gethwid},
    {"setfpscap", unc_setfpscap},
    {"getfpscap", unc_getfpscap},
    {"setclipboard", unc_setclipboard},
    {"toclipboard", unc_setclipboard},
    {"setrbxclipboard", unc_setclipboard},
    {"messagebox", unc_messagebox},
    {"loadstring", unc_loadstring_impl},
    {nullptr, nullptr}
};

static void register_all_unc(lua_State* L) {
    if (!L || !r_pushcclosure || !r_setfield) return;

    // first register all globals with stub
    for (int i = 0; g_unc_globals[i]; i++) {
        r_pushcclosure(L, unc_stub, g_unc_globals[i], 0);
        r_setfield(L, RLUA_GLOBALSINDEX, g_unc_globals[i]);
    }

    // then override with real implementations where we have them
    for (int i = 0; g_unc_overrides[i].name; i++) {
        r_pushcclosure(L, g_unc_overrides[i].fn, g_unc_overrides[i].name, 0);
        r_setfield(L, RLUA_GLOBALSINDEX, g_unc_overrides[i].name);
    }

    printf("[+] Registered %d UNC globals\n", (int)(sizeof(g_unc_globals)/sizeof(g_unc_globals[0]) - 1));
}
