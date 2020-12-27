#include "lua.h"
#include "lauxlib.h"
#include "lutil.h"
#include "luaconf.h"
#include "pipe.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
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

int pipe_is_nonblocking(lua_State *L)
{
    ELI_PIPE_END *_pipe = ((ELI_PIPE_END *)luaL_checkudata(L, 1, PIPE_METATABLE));
#ifdef _WIN32
    HANDLE h = _pipe->h;
    if (h == INVALID_HANDLE_VALUE)
    {
        errno = EBADF;
        return push_error(L, "Failed nonblocking check");
    }
    if (GetFileType(h) == FILE_TYPE_PIPE)
    {
        DWORD state;
        if (!GetNamedPipeHandleState(h, &state, NULL, NULL, NULL, NULL, 0))
            return push_error(L, "Failed nonblocking check");

        lua_pushboolean(L, (state & PIPE_NOWAIT) != 0);
        _pipe->nonblocking = (state & PIPE_NOWAIT) != 0;
    }
    else
    {
        lua_pushboolean(L, 0);
    }
#else
    int fd = _pipe->fd;
    if (fd < 0)
    {
        return push_error(L, "Failed nonblocking check");
    }
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        return push_error(L, "Failed to get file flags");
    }
    lua_pushboolean(L, (flags & O_NONBLOCK) != 0);
    _pipe->nonblocking = (flags & O_NONBLOCK) != 0;
#endif
    return 1;
}

int pipe_set_nonblocking(lua_State *L)
{
    ELI_PIPE_END *_pipe = ((ELI_PIPE_END *)luaL_checkudata(L, 1, PIPE_METATABLE));
    int nonblocking = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : 1;
#ifdef _WIN32
    HANDLE h = _pipe->h;
    if (h == INVALID_HANDLE_VALUE)
    {
        errno = EBADF;
        return push_error(L, "Failed set nonblocking");
    }
    if (GetFileType(h) == FILE_TYPE_PIPE)
    {
        DWORD state;
        if (GetNamedPipeHandleState(h, &state, NULL, NULL, NULL, NULL, 0))
        {
            if (((state & PIPE_NOWAIT) != 0) == nonblocking)
            {
                lua_pushboolean(L, 1);
                _pipe->nonblocking = nonblocking;
                return 1;
            }

            if (nonblocking)
                state &= ~PIPE_NOWAIT;
            else
                state |= PIPE_NOWAIT;
            if (SetNamedPipeHandleState(h, &state, NULL, NULL))
            {
                lua_pushboolean(L, 1);
                _pipe->nonblocking = nonblocking;
                return 1;
            }
            errno = EINVAL;
            return push_error(L, "Failed set nonblocking");
        }
    }
    errno = ENOTSUP;
    return push_error(L, "Failed set nonblocking");
#else
    int fd = _pipe->fd;
    if (fd < 0)
    {
        return push_error(L, "Failed set nonblocking");
    }
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        return push_error(L, "Failed to get file flags");
    }
    if (((flags & O_NONBLOCK) != 0) == nonblocking)
    {
        lua_pushboolean(L, 1);
        _pipe->nonblocking = nonblocking;
        return 1;
    }
    if (nonblocking)
    {
        flags |= O_NONBLOCK;
    }
    else
    {
        flags &= ~O_NONBLOCK;
    }
    int res = fcntl(fd, F_SETFL, flags);
    if (res == 0)
    {
        _pipe->nonblocking = nonblocking;
    }
    return push_result(L, res, NULL);
#endif
}

static int pipe_close(lua_State *L)
{
    ELI_PIPE_END *p = ((ELI_PIPE_END *)luaL_checkudata(L, 1, PIPE_METATABLE));
    int res;
    if (p->closed != 1)
    {
#ifdef _WIN32
        res = CloseHandle(p->h);
        p->closed = 1 & (res != 0);
    
        return luaL_fileresult(L, res, NULL);
#else
        res = close(p->fd);
        p->closed = 1 & (res == 0);
        return luaL_fileresult(L, (res == 0), NULL);
#endif
    }
}

int new_pipe(PIPE_DESCRIPTORS * descriptors) {
#ifdef _WIN32
    if (!CreatePipe(ph + 0, ph + 1, 0, 0))
        return -1;
    SetHandleInformation(descriptors->ph[0], HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(descriptors->ph[1], HANDLE_FLAG_INHERIT, 0);
#else
    int fd[2];
    if (-1 == pipe(fd))
        return -1;
    closeonexec(descriptors->fd[0]);
    closeonexec(descriptors->fd[1]);
#endif
}

#ifdef _WIN32
static ELI_PIPE_END *new_eli_pipe(lua_State *L, HANDLE h, const char *mode)
#else
static ELI_PIPE_END *new_eli_pipe(lua_State *L, int fd, const char *mode)
#endif
{
    ELI_PIPE_END *p = (ELI_PIPE_END *)lua_newuserdata(L, sizeof(ELI_PIPE_END));
    luaL_getmetatable(L, PIPE_METATABLE);
    lua_setmetatable(L, -2);
#ifdef _WIN32
    p->h = h;
#else
    p->fd = fd;
#endif
    p->nonblocking = 0;
    p->closed = 0;
    p->mode = mode;
    return p;
}

/* -- in out/nil error */
int eli_pipe(lua_State *L)
{
    PIPE_DESCRIPTORS descriptors;
#ifdef _WIN32
    if (new_pipe(&descriptors) == -1)
        return push_error(L, NULL);
    new_eli_pipe(L, ph[0], "r");
    new_eli_pipe(L, ph[1], "w");
#else
    if (new_pipe(&descriptors) == -1)
        return push_error(L, NULL);
    new_eli_pipe(L, descriptors.fd[0], "r");
    new_eli_pipe(L, descriptors.fd[1], "w");
#endif
    return 2;
}

static int pipe_write(lua_State *L)
{
    ELI_PIPE_END *_pipe = ((ELI_PIPE_END *)luaL_checkudata(L, 1, PIPE_METATABLE));
    int arg = 2;

    int nargs = lua_gettop(L) - 1;
    size_t status = 1;
    for (; status && nargs--; arg++)
    {
        size_t msgsize;
        const char *msg = luaL_checklstring(L, arg, &msgsize);
#ifdef _WIN32
        DWORD dwBytesWritten = 0;
        DWORD bErrorFlag = WriteFile(_pipe->h, msg, msgsize, &dwBytesWritten, NULL);
        if (bErrorFlag == FALSE)
        {
            DWORD err = GetLastError();
            if (!_pipe->nonblocking || err != ERROR_IO_PENDING)
            { // nonblocking so np
                LPSTR messageBuffer = NULL;
                size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                             NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
                lua_pushnil(L);
                lua_pushstring(L, messageBuffer);
                lua_pushinteger(L, err);
                return 3;
            }
        }
        status = status && (dwBytesWritten == msgsize);
#else
        status = status && (write(_pipe->fd, msg, msgsize) == msgsize);
#endif
    }
    lua_pushboolean(L, status);
    return 1;
}

#ifdef _WIN32
static DWORD read(HANDLE h, char *buffer, DWORD count)
{
    size_t res;
    DWORD lpNumberOfBytesRead = 0;
    DWORD bErrorFlag = ReadFile(h, buffer, LUAL_BUFFERSIZE, &lpNumberOfBytesRead, NULL);
    return bErrorFlag == FALSE ? -1 : lpNumberOfBytesRead;
}
#endif

static int push_read_result(lua_State *L, int res, int nonblocking)
{
    if (res == -1)
    {
        char *errmsg;
        size_t _errno;

#ifdef _WIN32
        if (!nonblocking || (_errno = GetLastError()) != ERROR_NO_DATA)
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK || !nonblocking)
#endif
        {
#ifdef _WIN32
            if (!nonblocking || _errno != ERROR_NO_DATA)
            { // nonblocking so np
                LPSTR messageBuffer = NULL;
                size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                             NULL, _errno, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
                errmsg = messageBuffer;
            }
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK || !nonblocking)
            {
                errmsg = strerror(errno);
                _errno = errno;
            }
#endif
            if (lua_rawlen(L, -1) == 0)
            {
                lua_pushnil(L);
            }
            lua_pushstring(L, errmsg);
            lua_pushinteger(L, _errno);
            return 3;
        }
    }
    return 1;
}

static int read_all(lua_State *L, ELI_PIPE_END *_pipe)
{
    size_t res;
    luaL_Buffer b;
    luaL_buffinit(L, &b);
#ifdef _WIN32
    HANDLE fd = _pipe->h;
#else
    int fd = _pipe->fd;
#endif

    do
    { /* read file in chunks of LUAL_BUFFERSIZE bytes */
        char *p = luaL_prepbuffer(&b);
        res = read(fd, p, LUAL_BUFFERSIZE);
        if (res != -1)
            luaL_addlstring(&b, p, res);
    } while (res == LUAL_BUFFERSIZE);

    luaL_pushresult(&b); /* close buffer */
    return push_read_result(L, res, _pipe->nonblocking);
}

static int read_line(lua_State *L, ELI_PIPE_END *_pipe, int chop)
{
    luaL_Buffer b;
#ifdef _WIN32
    HANDLE fd = _pipe->h;
#else
    int fd = _pipe->fd;
#endif
    char c = '\0';
    luaL_buffinit(L, &b);
    size_t res = 1;

    while (res == 1 && c != EOF && c != '\n')
    {
        char *buff = luaL_prepbuffer(&b);
        int i = 0;
        while (i < LUAL_BUFFERSIZE && (res = read(fd, &c, sizeof(char))) == 1 && c != EOF && c != '\n')
        {
            buff[i++] = c;
        }
        if (res != -1)
            luaL_addsize(&b, i);
    }
    if (!chop && c == '\n')
        luaL_addchar(&b, c);
    luaL_pushresult(&b);

    return push_read_result(L, res, _pipe->nonblocking);
}

static int pipe_read(lua_State *L)
{
    ELI_PIPE_END *_pipe = ((ELI_PIPE_END *)luaL_checkudata(L, 1, PIPE_METATABLE));

    if (lua_type(L, 2) == LUA_TNUMBER)
    {
        size_t l = (size_t)luaL_checkinteger(L, 2);
        char *buffer = malloc(sizeof(char) * l);

#ifdef _WIN32
        HANDLE fd = _pipe->h;
#else
        int fd = _pipe->fd;
#endif
        size_t res = read(fd, buffer, l);
        if (res != -1)
        {
            lua_pushlstring(L, buffer, res);
            free(buffer);
            return 1;
        }
        free(buffer);
        return luaL_fileresult(L, res, NULL);
    }
    else
    {
        const char *p = luaL_checkstring(L, 2);
        size_t success;
        if (*p == '*')
            p++; /* skip optional '*' (for compatibility) */
        switch (*p)
        {
        case 'l': /* line */
            return read_line(L, _pipe, 1);
        case 'L': /* line with end-of-line */
            return read_line(L, _pipe, 0);
        case 'a':
            return read_all(L, _pipe); /* read all data available */
        default:
            return luaL_argerror(L, 2, "invalid format");
        }
    }
}

static int io_fclose(lua_State *L)
{
    luaL_Stream *p = ((luaL_Stream *)luaL_checkudata(L, 1, LUA_FILEHANDLE));
    int res = fclose(p->f);
    return luaL_fileresult(L, (res == 0), NULL);
}

static int as_filestream(lua_State *L)
{
    ELI_PIPE_END *_pipe = ((ELI_PIPE_END *)luaL_checkudata(L, 1, PIPE_METATABLE));
    luaL_Stream *p = (luaL_Stream *)lua_newuserdata(L, sizeof(luaL_Stream));
    luaL_setmetatable(L, LUA_FILEHANDLE);
    p->f = fdopen(dup(_pipe->fd), _pipe->mode);
    p->closef = &io_fclose;
    return 1;
}

int pipe_create_meta(lua_State *L)
{
    luaL_newmetatable(L, PIPE_METATABLE);

    /* Method table */
    lua_newtable(L);
    lua_pushcfunction(L, pipe_read);
    lua_setfield(L, -2, "read");
    lua_pushcfunction(L, pipe_write);
    lua_setfield(L, -2, "write");
    lua_pushcfunction(L, pipe_close);
    lua_setfield(L, -2, "close");
    lua_pushcfunction(L, pipe_set_nonblocking);
    lua_setfield(L, -2, "set_nonblocking");
    lua_pushcfunction(L, pipe_is_nonblocking);
    lua_setfield(L, -2, "is_nonblocking");
    lua_pushcfunction(L, as_filestream);
    lua_setfield(L, -2, "as_filestream");

    lua_pushstring(L, PIPE_METATABLE);
    lua_setfield(L, -2, "__type");

    /* Metamethods */
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, pipe_close);
    lua_setfield(L, -2, "__gc");
    return 1;
}
