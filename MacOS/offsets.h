#pragma once
#include <stdint.h>

// ============================================================================
// macOS Roblox Offsets — version-73d88b9483b340ae
// These are absolute VAs — rebase() strips the image base to get RVA
// At runtime: real_addr = dylib_base + RVA
// ============================================================================

#define DEFAULT_BASE 0x100000000ULL
#define rebase(addr) ((uintptr_t)((addr) - DEFAULT_BASE))

namespace offsets {
inline constexpr uintptr_t setfenv = rebase(0x104155336);
inline constexpr uintptr_t spawn = rebase(0x10177E5D6);
inline constexpr uintptr_t error = rebase(0x10459352E);
inline constexpr uintptr_t print = rebase(0x1001D4110);
inline constexpr uintptr_t fireproximityprompt = rebase(0x101F85348);
inline constexpr uintptr_t teleport = rebase(0x102430FB0);
inline constexpr uintptr_t lua_gettop = rebase(0x1041532E7);
inline constexpr uintptr_t lua_getfield = rebase(0x10415480D);
inline constexpr uintptr_t lua_pushnil = rebase(0x10415436B);
inline constexpr uintptr_t luavm_load = rebase(0x1017DD0DA);
inline constexpr uintptr_t lua_settop = rebase(0x1041532F9);
inline constexpr uintptr_t lua_tostring = rebase(0x104153D34);
inline constexpr uintptr_t lua_newthread = rebase(0x10415323C);
inline constexpr uintptr_t lual_register = rebase(0x104156A01);
inline constexpr uintptr_t lua_call = rebase(0x1041553D2);
inline constexpr uintptr_t luau_load = rebase(0x10417AEEA);
inline constexpr uintptr_t lua_rawgeti = rebase(0x104154A22);
inline constexpr uintptr_t lua_next = rebase(0x104155676);
inline constexpr uintptr_t lua_setfield = rebase(0x104154EAA);
inline constexpr uintptr_t lua_pushvalue = rebase(0x104153670);
inline constexpr uintptr_t lua_pushlstring = rebase(0x10415441D);
inline constexpr uintptr_t lua_pushstring = rebase(0x10415448D);
inline constexpr uintptr_t lua_pushcclosure = rebase(0x104156796);
inline constexpr uintptr_t lua_pushnumber = rebase(0x104154384);
inline constexpr uintptr_t lua_tonumber = rebase(0x104153AC5);
inline constexpr uintptr_t lua_xmove = rebase(0x1041531B1);
inline constexpr uintptr_t lua_insert = rebase(0x1041534D9);
inline constexpr uintptr_t lua_newtable = rebase(0x104154B67);
inline constexpr uintptr_t lua_setmetatable = rebase(0x104155263);
inline constexpr uintptr_t lua_settable = rebase(0x104154E35);
inline constexpr uintptr_t lua_yield = rebase(0x10415F771);
inline constexpr uintptr_t lua_type = rebase(0x1041536F5);
inline constexpr uintptr_t lua_objlen = rebase(0x104154081);
inline constexpr uintptr_t EnableLoadModule = rebase(0x10177F0D0);
inline constexpr uintptr_t FireTouchInterest = rebase(0x10264222E);
inline constexpr uintptr_t FireProximityPrompt = rebase(0x101F85348);
inline constexpr uintptr_t lua_createtable = rebase(0x104154B67);
inline constexpr uintptr_t lua_remove = rebase(0x1017AF5D7);
inline constexpr uintptr_t lua_getrawfield = rebase(0x1041548D1);
inline constexpr uintptr_t lua_getfenv = rebase(0x104154D95);
inline constexpr uintptr_t luaL_error = rebase(0x104156252);
inline constexpr uintptr_t lua_setfenv = rebase(0x104155336);
inline constexpr uintptr_t FireClickDetector = rebase(0x101987EBB);
inline constexpr uintptr_t lua_tolstring = rebase(0x104153D34);
inline constexpr uintptr_t GetGlobalState = rebase(0x101827B0C);
} // namespace offsets