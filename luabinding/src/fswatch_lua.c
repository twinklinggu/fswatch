#define LUA_LIB

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <libfswatch/c/libfswatch.h>
#include <pthread.h>
#include <unistd.h>
// #include "skynet_server.h"
// #include "skynet_mq.h"

struct fsw_session_context {
	FSW_HANDLE handle;	
};

/**
 * The following function implements the callback functionality for testing
 * eventnumber send from the libdawatch library. See FSW_CEVENT_CALLBACK for
 * details.
 *
 * @param events
 * @param event_num
 * @param data
 */
void my_callback(fsw_cevent const *const events,
                 const unsigned int event_num,
                 void *data)
{
  printf("my_callback: %d\n", event_num);
  for(int i = 0; i < event_num; i++) {
	fsw_cevent const cevent = events[i];	
	printf("file change, event:");
	for (int n = 0; n < cevent.flags_num; n++) {
		printf("%s,", fsw_get_event_flag_name(cevent.flags[n]));
	}
	printf(" path %s\n", cevent.path);
  }
}

void *start_monitor(void *param)
{
  FSW_HANDLE handle = (FSW_HANDLE) param;

int ret = fsw_start_monitor(handle);
  if (FSW_OK != ret)
  {
    fprintf(stderr, "Error creating thread111 0x%p, %d\n", handle, ret);
  }
  else
  {
    printf("Monitor stopped \n");
  }

  return NULL;
}

static int
ldestory_session(lua_State *L) {
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	FSW_HANDLE handle = ctx->handle;
	ctx->handle = NULL;
	if (FSW_OK != fsw_stop_monitor(handle))
	{
		fprintf(stderr, "Error stopping monitor \n");
		return 0;
	}
	fsw_destroy_session(handle);
	return 0;
}

static int
ladd_path(lua_State *L) {
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	FSW_HANDLE handle = ctx->handle;
	const char * path = luaL_checkstring(L, 2);
	if (FSW_OK != fsw_add_path(handle, path))
    {
      fsw_last_error();
	  luaL_error(L, "fsw_add_path failed\n");
    }
	return 0;
}

static int
lstart_monitor(lua_State *L) {
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	FSW_HANDLE handle = ctx->handle;

	if (FSW_OK != fsw_set_callback(handle, my_callback, ctx))
	{
	  fsw_last_error();
	  luaL_error(L, "fsw_set_callback failed\n");
	}
	fsw_set_recursive(handle, true);
	struct fsw_event_type_filter filter;
	filter.flag = PlatformSpecific; 
	fsw_add_event_type_filter(handle, filter);
	pthread_t start_thread;
	if (pthread_create(&start_thread, NULL, start_monitor, (void *)handle))
	{
	  luaL_error(L, "pthread_create failed\n");
	}
	return 0;
}

luaL_Reg lsession_meta[] = {
	{"__gc", ldestory_session},
	{"add_path", ladd_path},
	{"start_monitor", lstart_monitor},
	{NULL, NULL},
};

static int
lnew_session(lua_State* L) {
	struct fsw_session_context * ctx = lua_newuserdatauv(L, sizeof(*ctx), 0);
	FSW_HANDLE handle = fsw_init_session(inotify_monitor_type);
	ctx->handle = handle;
	fprintf(stderr, "creating session 0x%p\n", handle);
	lua_createtable(L, 0, sizeof(lsession_meta)/sizeof(lsession_meta[0]));
	luaL_setfuncs(L, lsession_meta, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);
	return 1;
}

static int
linit_library(lua_State* L) {
	if (FSW_OK != fsw_init_library())
	{
		fsw_last_error();
		luaL_error(L, "libfswatch cannot be initialised!\n");
	}
	return 0;
}

LUAMOD_API int
luaopen_fswatch(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "init_library", linit_library },
		{ "new_session", lnew_session },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}