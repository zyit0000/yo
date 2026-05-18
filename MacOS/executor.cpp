// macOS executor dylib — injected into RobloxPlayer
// TCP server on 127.0.0.1:5555, start.sh connects and sends scripts
// Offsets: subtract 0x100000000 (base), then add ASLR slide

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <ftw.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>

#include "offsets.h"

// POSIX filesystem helpers (10.13 compatible, no std::filesystem)
static bool posix_is_file(const std::string& p) {
    struct stat st; return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
static bool posix_is_dir(const std::string& p) {
    struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
static void posix_mkdir_p(const std::string& p) {
    std::string tmp;
    for (char c : p) {
        tmp += c;
        if (c == '/') mkdir(tmp.c_str(), 0755);
    }
    mkdir(tmp.c_str(), 0755);
}
static std::string posix_parent(const std::string& p) {
    auto pos = p.rfind('/');
    return pos != std::string::npos ? p.substr(0, pos) : ".";
}
static int _nftw_rm(const char* path, const struct stat*, int, struct FTW*) {
    return remove(path);
}
static void posix_rmrf(const std::string& p) {
    nftw(p.c_str(), _nftw_rm, 64, FTW_DEPTH | FTW_PHYS);
}

// ============================================================================
// Minimal Luau types
// ============================================================================
struct lua_State;
typedef int (*rlua_CFunction)(lua_State* L);

typedef int         (*fn_lua_gettop)(lua_State*);
typedef void        (*fn_lua_settop)(lua_State*, int);
typedef int         (*fn_lua_type)(lua_State*, int);
typedef const char* (*fn_lua_tolstring)(lua_State*, int, size_t*);
typedef double      (*fn_lua_tonumber)(lua_State*, int);
typedef void        (*fn_lua_pushnil)(lua_State*);
typedef void        (*fn_lua_pushnumber)(lua_State*, double);
typedef void        (*fn_lua_pushboolean)(lua_State*, int);
typedef void        (*fn_lua_pushlstring)(lua_State*, const char*, size_t);
typedef void        (*fn_lua_pushstring)(lua_State*, const char*);
typedef void        (*fn_lua_pushcclosure)(lua_State*, rlua_CFunction, const char*, int);
typedef void        (*fn_lua_pushvalue)(lua_State*, int);
typedef void        (*fn_lua_getfield)(lua_State*, int, const char*);
typedef void        (*fn_lua_setfield)(lua_State*, int, const char*);
typedef void        (*fn_lua_createtable)(lua_State*, int, int);
typedef void        (*fn_lua_rawseti)(lua_State*, int, int);
typedef void        (*fn_lua_call)(lua_State*, int, int);
typedef int         (*fn_luavm_load)(lua_State*, const char*, const char*, size_t, int);
typedef void        (*fn_luaL_error)(lua_State*, const char*, ...);

#define RLUA_GLOBALSINDEX (-10002)

// ============================================================================
// Runtime state
// ============================================================================
static uintptr_t g_base = 0;       // image base (with ASLR slide)
static intptr_t  g_slide = 0;      // ASLR slide value
static lua_State* g_rL = nullptr;
static lua_State* g_eL = nullptr;

// resolved function pointers
static fn_lua_gettop      r_gettop = nullptr;
static fn_lua_settop      r_settop = nullptr;
static fn_lua_type        r_type = nullptr;
static fn_lua_tolstring   r_tolstring = nullptr;
static fn_lua_tonumber    r_tonumber = nullptr;
static fn_lua_pushnil     r_pushnil = nullptr;
static fn_lua_pushnumber  r_pushnumber = nullptr;
static fn_lua_pushlstring r_pushlstring = nullptr;
static fn_lua_pushstring  r_pushstring = nullptr;
static fn_lua_pushcclosure r_pushcclosure = nullptr;
static fn_lua_pushvalue   r_pushvalue = nullptr;
static fn_lua_getfield    r_getfield = nullptr;
static fn_lua_setfield    r_setfield = nullptr;
static fn_lua_createtable r_createtable = nullptr;
static fn_lua_rawseti     r_rawseti = nullptr;
static fn_lua_call        r_call = nullptr;
static fn_luavm_load      r_luavm_load = nullptr;

// pushboolean shim — no offset available, emulate via pushnumber
static void r_pushboolean(lua_State* L, int b) {
    if (r_pushnumber) r_pushnumber(L, b ? 1.0 : 0.0);
}

// helpers
static const char* r_tostring(lua_State* L, int idx) {
    return r_tolstring ? r_tolstring(L, idx, nullptr) : nullptr;
}
static void r_newtable(lua_State* L) {
    if (r_createtable) r_createtable(L, 0, 0);
}
static void r_pop(lua_State* L, int n = 1) {
    if (r_settop) r_settop(L, -(n) - 1);
}
static void r_register_global(lua_State* L, const char* name, rlua_CFunction fn) {
    if (r_pushcclosure && r_setfield) {
        r_pushcclosure(L, fn, name, 0);
        r_setfield(L, RLUA_GLOBALSINDEX, name);
    }
}

// ============================================================================
// Include UNC environment (all 350+ globals)
// ============================================================================
#include "unc_env.h"

// ============================================================================
// Get Mach-O image base + ASLR slide
// ============================================================================
static bool get_image_info(uintptr_t* out_base, intptr_t* out_slide) {
    uint32_t count = _dyld_image_count();
    for (uint32_t i = 0; i < count; i++) {
        const char* name = _dyld_get_image_name(i);
        if (name && strstr(name, "RobloxPlayer")) {
            *out_base = (uintptr_t)_dyld_get_image_header(i);
            *out_slide = _dyld_get_image_vmaddr_slide(i);
            return true;
        }
    }
    // fallback: main executable
    *out_base = (uintptr_t)_dyld_get_image_header(0);
    *out_slide = _dyld_get_image_vmaddr_slide(0);
    return *out_base != 0;
}

// ============================================================================
// Resolve offsets — offset is already rebased (- 0x100000000), add slide
// ============================================================================
#define RESOLVE(ptr, type, off) ptr = reinterpret_cast<type>(g_slide + offsets::off)

static bool resolve_offsets() {
    if (!get_image_info(&g_base, &g_slide)) {
        printf("[-] Failed to get image base\n");
        return false;
    }
    printf("[+] Image base: 0x%lx | ASLR slide: 0x%lx\n",
           (unsigned long)g_base, (unsigned long)g_slide);

    RESOLVE(r_gettop,      fn_lua_gettop,      lua_gettop);
    RESOLVE(r_settop,      fn_lua_settop,      lua_settop);
    RESOLVE(r_type,        fn_lua_type,        lua_type);
    RESOLVE(r_tolstring,   fn_lua_tolstring,   lua_tolstring);
    RESOLVE(r_tonumber,    fn_lua_tonumber,    lua_tonumber);
    RESOLVE(r_pushnil,     fn_lua_pushnil,     lua_pushnil);
    RESOLVE(r_pushnumber,  fn_lua_pushnumber,  lua_pushnumber);
    RESOLVE(r_pushlstring, fn_lua_pushlstring, lua_pushlstring);
    RESOLVE(r_pushstring,  fn_lua_pushstring,  lua_pushstring);
    RESOLVE(r_pushcclosure,fn_lua_pushcclosure,lua_pushcclosure);
    RESOLVE(r_pushvalue,   fn_lua_pushvalue,   lua_pushvalue);
    RESOLVE(r_getfield,    fn_lua_getfield,    lua_getfield);
    RESOLVE(r_setfield,    fn_lua_setfield,    lua_setfield);
    RESOLVE(r_createtable, fn_lua_createtable, lua_createtable);
    RESOLVE(r_call,        fn_lua_call,        lua_call);
    RESOLVE(r_luavm_load,  fn_luavm_load,      luavm_load);

    // lua_pushboolean / lua_rawseti not in offsets.h
    // pushboolean shim: push 0.0 or 1.0 via pushnumber
    // rawseti stays null — listfiles will skip indexing

    printf("[+] Offsets resolved (slide-adjusted)\n");
    return true;
}

// ============================================================================
// TCP server on 127.0.0.1:5555
// ============================================================================
#define SERVER_PORT 5555

static void tcp_server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); return;
    }
    listen(server_fd, 5);
    printf("[+] TCP server listening on 127.0.0.1:%d\n", SERVER_PORT);

    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;

        // read entire script from client
        std::string script;
        char buf[4096];
        ssize_t n;
        while ((n = read(client_fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            script += buf;
        }
        close(client_fd);

        if (!script.empty() && g_eL && r_luavm_load) {
            printf("[*] Executing script (%zu bytes)\n", script.size());
            int r = r_luavm_load(g_eL, "@wadiwad", script.c_str(), script.size(), 0);
            if (r == 0 && r_call) {
                r_call(g_eL, 0, 0);
                printf("[+] Script executed OK\n");
            } else {
                const char* err = r_tostring(g_eL, -1);
                printf("[-] Script error: %s\n", err ? err : "unknown");
                r_pop(g_eL, 1);
            }
        }
    }
}

// ============================================================================
// Constructor — runs when dylib is loaded
// ============================================================================
__attribute__((constructor))
static void dylib_entry() {
    printf("[*] wadiwad dylib loaded (macOS)\n");

    if (!resolve_offsets()) {
        printf("[-] Failed to resolve offsets, aborting\n");
        return;
    }

    // lua_State acquisition — walk TaskScheduler from GetGlobalState
    // This runs in a separate thread to wait for the game to initialize
    std::thread([]() {
        // give the game time to start up
        sleep(5);

        // attempt to get global state via offset
        if (offsets::GetGlobalState != 0) {
            typedef lua_State* (*fn_GetGlobalState)();
            auto getGlobalState = reinterpret_cast<fn_GetGlobalState>(g_slide + offsets::GetGlobalState);
            g_rL = getGlobalState();
            if (g_rL) {
                printf("[+] Got lua_State: %p\n", (void*)g_rL);
                // create executor thread
                typedef lua_State* (*fn_lua_newthread)(lua_State*);
                auto r_newthread = reinterpret_cast<fn_lua_newthread>(g_slide + offsets::lua_newthread);
                if (r_newthread) {
                    g_eL = r_newthread(g_rL);
                    printf("[+] Executor thread: %p\n", (void*)g_eL);
                } else {
                    g_eL = g_rL;
                }
            } else {
                printf("[-] GetGlobalState returned null\n");
            }
        }

        if (g_eL) {
            register_all_unc(g_eL);
            printf("[+] Environment ready — send scripts to 127.0.0.1:%d\n", SERVER_PORT);
        } else {
            printf("[!] No lua_State — server will still listen but can't execute\n");
        }

        // start TCP server on this thread
        tcp_server();
    }).detach();
}
