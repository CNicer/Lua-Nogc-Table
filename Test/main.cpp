#include <iostream>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "xnogc.h"

#include <stdint.h>
#include <chrono>

uint64_t get_microseconds() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

int lua_getmicroseconds(lua_State* L)
{
    lua_pushnumber(L, get_microseconds());
    return 1;
}

int main(int argc, char** argv)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // open no gc libs
    luaL_opengclibs(L);

    lua_pushglobaltable(L);
    const luaL_Reg timefuncs[] = {
        {"get_microseconds", lua_getmicroseconds},
        {NULL, NULL}
    };
    luaL_setfuncs(L, timefuncs, 0);

    if(luaL_dofile(L, "test.lua") != LUA_OK)
    {
        std::cerr << "call test.lua error=" << lua_tostring(L, -1) << std::endl;
    }

    std::cout << "finish" << std::endl;
}