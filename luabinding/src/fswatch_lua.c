#define LUA_LIB

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <libfswatch/c/libfswatch.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "skynet_server.h"
#include "skynet_mq.h"
#include "skynet.h"

#define MAX_SESSION_PATH 64
#define MAX_SESSION_FLAGS 64
struct fsw_session_context {
  	bool allow_overflow;
  	bool recursive;
  	bool directory_only;
  	bool follow_symlinks;
	char * paths[MAX_SESSION_PATH];
	int path_n;
	int opaque;
  	double latency;
	int flags[MAX_SESSION_FLAGS];
	int flag_n;
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
void event_callback(fsw_cevent const *const events,
                 const unsigned int event_num,
                 void *data)
{
  for(int i = 0; i < event_num; i++) {
	fsw_cevent const * event = &events[i];	
	char * path = event->path;
	size_t path_len = strlen(path);
	int *opaque = (int*) data;
	for (int n = 0; n < event->flags_num; n++) {
		char * event_flag_name = fsw_get_event_flag_name(event->flags[n]);
		size_t sz = path_len + strlen(event_flag_name) + 2;
		char * data = (char *) skynet_malloc(sz);
		memcpy(data, path, path_len);
		data[path_len] = ',';
		memcpy(data + path_len + 1, event_flag_name, strlen(event_flag_name));
		data[sz -1] = '\0';

		struct skynet_message message;
		message.source = 0;
		message.session = 0;
		message.data = data;
		message.sz = sz | ((size_t)PTYPE_TEXT << MESSAGE_TYPE_SHIFT);
		if (skynet_context_push((uint32_t)*opaque, &message))
		{
			skynet_free(data);
		}
	}
  }
}

void *start_monitor(void *param)
{
	struct fsw_session_context *ctx = (struct fsw_session_context *) param;
	FSW_HANDLE handle = fsw_init_session(system_default_monitor_type);
	fsw_set_recursive(handle, ctx->recursive);
	fsw_set_latency(handle, ctx->latency);
	fsw_set_directory_only(handle, ctx->directory_only);
	fsw_set_follow_symlinks(handle, ctx->follow_symlinks);
	for (int i = 0; i < ctx->path_n; i++){
		fsw_add_path(handle, ctx->paths[i]);
	}
	for (int i = 0; i < ctx->flag_n; i++){
		struct fsw_event_type_filter filter;
		filter.flag = ctx->flags[i];
		fsw_add_event_type_filter(handle, filter);
	}
	int opaque = ctx->opaque;
	fsw_set_callback(handle, event_callback, &opaque);

	printf("fswatch monitor started \n");
	if (FSW_OK != fsw_start_monitor(handle))
	{
		fprintf(stderr, "Error creating thread\n");
	}
	else
	{
		printf("Monitor stopped \n");
	}
	fsw_destroy_session(handle);
	return NULL;
}

static int
ladd_path(lua_State *L) {
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	if (ctx->path_n < MAX_SESSION_PATH) {
		const char *path = luaL_checkstring(L, 2);
		int sz = strlen(path);
		char *tmp = skynet_malloc(sz +1);
		memcpy(tmp, path, sz);
		tmp[sz] = '\0';
		ctx->paths[ctx->path_n++] = tmp;
	} else {
		luaL_error(L, "session paths limit\n");
	}
	return 0;
}

static int
lstart_monitor(lua_State *L) {
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	int sz = sizeof(struct fsw_session_context);
	struct fsw_session_context * tmp =(struct fsw_session_context *) malloc(sz);
	memcpy(tmp, ctx, sz);	
	pthread_t pid;
	if (pthread_create(&pid, NULL, start_monitor, tmp))
	{
	  luaL_error(L, "pthread_create failed\n");
	}
	return 0;
}

static int
lset_recursive(lua_State *L){
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	ctx->recursive =lua_toboolean(L, 2); 
	return 0;
}

static int
lset_directory_only(lua_State *L){
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	ctx->directory_only = lua_toboolean(L, 2);
	return 0;
}

static int
lset_latency(lua_State *L){
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	double latency = luaL_checknumber(L, 2);
	ctx->latency = latency;
	return 0;
}

static int
lset_follow_symlinks(lua_State *L){
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	ctx->follow_symlinks = lua_toboolean(L, 2);
	return 0;
}

static int
lset_allow_overflow(lua_State *L){
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	ctx->allow_overflow = lua_toboolean(L, 2);
	return 0;
}

static int
ladd_event_type_filter(lua_State *L){
	struct fsw_session_context * ctx = lua_touserdata(L, 1);
	enum fsw_event_flag flag = lua_tointeger(L, 2);
	if (ctx->flag_n < MAX_SESSION_FLAGS)
	{
	  ctx->flags[ctx->flag_n++] = flag;
	}
	else
	{
	  luaL_error(L, "session flags limit");
	}
	return 0;
}


luaL_Reg fsw_session_meta[] = {
	{"add_path", ladd_path},
	{"start_monitor", lstart_monitor},
	{"set_recursive", lset_recursive},
	{"set_directory_only", lset_directory_only},
	{"set_latency", lset_latency},
	{"set_follow_symlinks", lset_follow_symlinks},
	{"set_allow_overflow", lset_allow_overflow},
	{"add_event_type_filter", ladd_event_type_filter},
	{NULL, NULL},
};

static int
lnew_session(lua_State* L) {
	struct fsw_session_context * ctx = lua_newuserdatauv(L, sizeof(*ctx), 0);
	memset(ctx, 0,sizeof(*ctx));
	ctx->opaque = lua_tointeger(L, 1);
	lua_createtable(L, 0, sizeof(fsw_session_meta)/sizeof(fsw_session_meta[0]));
	luaL_setfuncs(L, fsw_session_meta, 0);
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
	bool verbose = lua_toboolean(L, 1);
	return 0;
}

static int
lset_verbose(lua_State* L) {
	bool verbose = lua_toboolean(L, 1);
	fsw_set_verbose(verbose);
	return 0;
}

LUAMOD_API int
luaopen_fswatch(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{"init_library", linit_library},
		{"new_session", lnew_session},
		{"set_verbose", lset_verbose},
		{NULL, NULL},
	};
	luaL_newlib(L, l);
	return 1;
}