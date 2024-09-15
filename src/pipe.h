#include "lua.h"

typedef struct PIPE_DESCRIPTORS {
#ifdef _WIN32
	HANDLE fd[2];
#else
	int fd[2];
#endif
} PIPE_DESCRIPTORS;

int eli_pipe(lua_State *L);
int new_pipe(PIPE_DESCRIPTORS *descriptors);