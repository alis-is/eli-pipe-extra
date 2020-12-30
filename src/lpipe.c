#include "lua.h"
#include "lauxlib.h"

#include "pipe.h"

static const struct luaL_Reg eliPipeExtra[] = {
    {"pipe", eli_pipe},
    {NULL, NULL},
};

int luaopen_eli_pipe_extra(lua_State *L)
{
    lua_newtable(L);
    luaL_setfuncs(L, eliPipeExtra, 0);
    return 1;
}
