#include "lua.h"
#include "lauxlib.h"
#include "lutil.h"
#include "luaconf.h"
#include "pipe.h"
#include "stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define close _close
#else
#include <unistd.h>
#endif

#include <errno.h>

#ifndef _WIN32
static int closeonexec(int d)
{
    int fl = fcntl(d, F_GETFD);
    if (fl != -1)
        fl = fcntl(d, F_SETFD, fl | FD_CLOEXEC);
    return fl;
}
#endif

int new_pipe(PIPE_DESCRIPTORS * descriptors) {
    int fd[2];
#ifdef _WIN32
    HANDLE ph[2];
    int res = 0;
    if (!CreatePipe(ph, ph + 1, 0, 0))
        return -1;
    res = SetHandleInformation(ph[0], HANDLE_FLAG_INHERIT, 0);
    if (!res) {
        return -1;
    }
    res = SetHandleInformation(ph[1], HANDLE_FLAG_INHERIT, 0);
    if (!res) {
        return -1;
    }

    fd[0] = _open_osfhandle((intptr_t)ph[0], _O_RDONLY);
    if (fd[0] == -1) {
        return -1;
    }
    fd[1] = _open_osfhandle((intptr_t)ph[1], _O_WRONLY);
    if (fd[1] == -1) {
        close(fd[0]);
        return -1;
    }
#else
    if (-1 == pipe(fd))
        return -1;
    closeonexec(fd[0]);
    closeonexec(fd[1]);
#endif
    descriptors->fd[0] = fd[0];
    descriptors->fd[1] = fd[1];
    return 0;
}


static void new_eli_stream(lua_State *L, int fd, ELI_STREAM_KIND kind)
{
    ELI_STREAM *p = (ELI_STREAM *)lua_newuserdata(L, sizeof(ELI_STREAM));
    switch (kind) {
        case ELI_STREAM_R_KIND: 
            luaL_getmetatable(L, ELI_STREAM_R_METATABLE);
            break;
        case ELI_STREAM_W_KIND: 
            luaL_getmetatable(L, ELI_STREAM_W_METATABLE);
            break;
        case ELI_STREAM_RW_KIND:
            luaL_getmetatable(L, ELI_STREAM_RW_METATABLE);
            break;
        default:
            luaL_error(L, "[new_eli_stream] Invalid ELI_STREAM_KIND!");
    }
    lua_setmetatable(L, -2);
    p->fd = fd;
    p->nonblocking = 0;
    p->closed = 0;
}

/* -- in out/nil error */
int eli_pipe(lua_State *L)
{
    PIPE_DESCRIPTORS descriptors;
    if (new_pipe(&descriptors) == -1)
        return push_error(L, NULL);
    new_eli_stream(L, descriptors.fd[0], ELI_STREAM_R_KIND);
    new_eli_stream(L, descriptors.fd[1], ELI_STREAM_W_KIND);
    return 2;
}
