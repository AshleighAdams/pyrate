/*
    This file is part of pyrate.

    pyrate is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    pyrate is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pyrate.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <lua.hpp>
#include <thread>
#include <mutex>
#include <chrono>

using namespace std;
using namespace chrono;

// mimics the behavoir of `lua_raw(s|g)et(luaState*, LUA_GLOBALSINDEX)` for >= Lua 5.2
void lua_setglobal(lua_State* state);
void lua_getglobal(lua_State* state);

/**
 * Thread controls
 */
struct lua_thread_control {
	/**
	 * Remote state
	 */
	lua_State* thread_state;

	/**
	 * Lock to protect the remote stack
	 */
	mutex m;

	/**
	 * Thread interacting with the remote stack
	 */
	thread t;

	/**
	 * Initialize the thread controls for a remote stack
	 */
	lua_thread_control(lua_State* t):
		thread_state(t)
	{}
};

/**
 * Lua: thread.create()
 * 	Create a new thread/channel object.
 */
static int lua_thread_create(lua_State* state){
	lua_State* thread_state = lua_newthread(state);
	lua_pop(state, 1);
	lua_pushlightuserdata(state, new lua_thread_control(thread_state));

	lua_newtable(state);
	lua_pushstring(state, "__index");
	lua_pushstring(state, "thread");
	lua_getglobal(state);
	lua_rawset(state, -3);

	lua_setmetatable(state, -2);

	return 1;
}

/**
 * Lua: thread.join(my_thread [, num_elems])
 * 		my_thread:join([num_elems])
 *   Wait for the thread to finish processing.
 *   If num_elems is omitted, all available results are returned,
 *   otherwise return the first num_elems results.
 */
static int lua_thread_join(lua_State* state){
	if(lua_gettop(state) > 0 && lua_type(state, 1) == LUA_TLIGHTUSERDATA){
		lua_thread_control* usrtc = (lua_thread_control*) lua_touserdata(state, 1);

		if(usrtc != NULL){
			usrtc->m.lock();
			int ret = lua_gettop(usrtc->thread_state);

			if(lua_gettop(state) > 1 && lua_type(state, 2) == LUA_TNUMBER){
				ret = min<int>(lua_tointeger(state, 2), ret);

				for(int i = 0; i < ret; i++){
					lua_pushvalue(usrtc->thread_state, 1);
					lua_remove(usrtc->thread_state, 1);
				}
			}

			lua_xmove(usrtc->thread_state, state, ret);

			usrtc->m.unlock();
			return ret;
		}else{
			lua_pushstring(state, "Thread is uninitialized");
			lua_error(state);
			return 0;
		}
	}else{
		lua_pushstring(state, "Usage: thread.join(userdata)");
		lua_error(state);
		return 0;
	}
}

/**
 * Thread worker
 */
static void lua_thread_call(lua_thread_control* usrtc, int params){
	lua_call(usrtc->thread_state, params, LUA_MULTRET);

	usrtc->m.unlock();
}

/**
 * Lua: thread.run(my_thread, function [, parameters ... ])
 *  	my_thread:run(function [, parameters ... ])
 *   Pass a function to the thread, that will be processed.
 *   The results of function are available via my_thread:join([return_values]).
 */
static int lua_thread_run(lua_State* state){
	int npar = lua_gettop(state);

	if(npar >= 2 && lua_type(state, 1) == LUA_TLIGHTUSERDATA && lua_type(state, 2) == LUA_TFUNCTION){
		lua_thread_control* usrtc = (lua_thread_control*) lua_touserdata(state, 1);

		if(usrtc != NULL){
			usrtc->m.lock();

			if(usrtc->t.joinable()){ // Not sure why, but this has to be here. Trust me.
				usrtc->t.join();
			}

			for(int idx = 2; idx <= npar; idx++){
				lua_pushvalue(state, idx);
			}
			lua_xmove(state, usrtc->thread_state, npar - 1);

			usrtc->t = thread(lua_thread_call, usrtc, npar - 2);
		}else{
			lua_pushstring(state, "Thread is uninitialized");
			lua_error(state);
		}
	}else{
		lua_pushstring(state, "Usage: thread.run(userdata, function)");
		lua_error(state);
	}

	return 0;
}

/**
 * Lua: thread.sleep(ms)
 * 	Sleep for ms milliseconds.
 */
static int lua_thread_sleep(lua_State* state){
	if(lua_gettop(state) == 1 && lua_type(state, 1) == LUA_TNUMBER){
		this_thread::sleep_for(milliseconds(lua_tointeger(state, 1)));
	}else{
		lua_pushstring(state, "Usage: thread.sleep(milliseconds)");
		lua_error(state);
	}

	return 0;
}

/**
 * Used when loaded via 'require'.
 */
extern "C" int luaopen_pyrate(lua_State* state){
	lua_pushstring(state, "__threads");
	lua_newtable(state);
	lua_setglobal(state);
	

	lua_pushstring(state, "thread");
	lua_newtable(state);

	lua_pushstring(state, "create");
	lua_pushcfunction(state, &lua_thread_create);
	lua_rawset(state, -3);

	lua_pushstring(state, "join");
	lua_pushcfunction(state, &lua_thread_join);
	lua_rawset(state, -3);

	lua_pushstring(state, "run");
	lua_pushcfunction(state, &lua_thread_run);
	lua_rawset(state, -3);

	lua_pushstring(state, "sleep");
	lua_pushcfunction(state, &lua_thread_sleep);
	lua_rawset(state, -3);

	lua_setglobal(state);

	return 0;
}

#if (LUA_VERSION_NUM <= 501)
void lua_setglobal(lua_State* state){
	lua_rawset(state, LUA_GLOBALSINDEX);
}
void lua_getglobal(lua_State* state){
	lua_rawget(state, LUA_GLOBALSINDEX);
}
#else // Lua 5.2 and above
void lua_setglobal(lua_State* state){
	// stack: name, value
	lua_rawgeti(state, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	// stack: name, value, _G
	lua_pushvalue(state, -3 /* name */);
	// stack: name, value, _G, name
	lua_pushvalue(state, -3 /* value */);
	// stack: name, value, _G, name, value
	lua_rawset(state, -3 /* _G */);
	// stack: name, value, _G
	lua_pop(state, 3);
	// stack: (empty)
}
void lua_getglobal(lua_State* state){
	lua_rawgeti(state, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	// stack: name, _G
	lua_pushvalue(state, -2 /* name */);
	// stack: name, _G, name
	lua_rawget(state, -2 /* _G */);
	// stack: name, _G, value
	lua_replace(state, -3 /* value */);
	// stack: value, _G
	lua_pop(state, 1);
	// stack: value
}
#endif
