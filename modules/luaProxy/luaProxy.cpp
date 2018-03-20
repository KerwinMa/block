﻿
#include "luaProxy.h"

#include <core/application.h>

#include <algorithm>
#include <iostream>
#include <fmt/format.h>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif // WIN32

//int luaopen_protobuf_c(lua_State *L);

std::string Traceback(lua_State * _state)
{
	lua_Debug info;
	int level = 0;
	std::string outputTraceback;
	char buffer[4096];

	while (lua_getstack(_state, level, &info)) 
	{
		lua_getinfo(_state, "nSl", &info);

#if defined(_WIN32) && defined(_MSC_VER)
		sprintf_s(buffer, "  [%d] %s:%d -- %s [%s]\n",
			level, info.short_src, info.currentline,
			(info.name ? info.name : "<unknown>"), info.what);
#else
		sprintf(buffer, "  [%d] %s:%d -- %s [%s]\n",
			level, info.short_src, info.currentline,
			(info.name ? info.name : "<unknown>"), info.what);
#endif
		outputTraceback.append(buffer);
		++level;
	}
	return outputTraceback;
}
/*
std::string getPath()
{
	char _path[512];

#ifdef WIN32
	GetModuleFileName(NULL, _path, 512);
	for (int i = strlen(_path); i >= 0; i--)
	{
		if (_path[i] == '\\')
		{
			_path[i] = '\0';
			break;
		}
	}
#else
	int cnt = readlink("/proc/self/exe", _path, 512);
	if (cnt < 0 || cnt >= 512) {
		std::cout << "read path err" << std::endl;
		return "";
	}
	for (int i = cnt; i >= 0; --i)
	{
		if (_path[i] == '/') {
			_path[i + 1] = '\0';
			break;
		}
	}
#endif // WIN32

	return _path;
}
*/

void gsf::modules::LuaProxyModule::before_init()
{	
	logM_ = APP.getModule("LogModule");
	assert(logM_ != gsf::ModuleNil);
}

void gsf::modules::LuaProxyModule::init()
{
	listen(this, eid::lua_proxy::create
		, std::bind(&LuaProxyModule::eCreate, this
			, std::placeholders::_1, std::placeholders::_2));

	listen(this, eid::lua_proxy::destroy
		, std::bind(&LuaProxyModule::eDistory, this
			, std::placeholders::_1, std::placeholders::_2));

	listen(this, eid::lua_proxy::reload
		, std::bind(&LuaProxyModule::eReload, this
			, std::placeholders::_1, std::placeholders::_2));
}

void gsf::modules::LuaProxyModule::execute()
{
	//!
	for (auto itr = luaMap_.begin(); itr != luaMap_.end();)
	{
		LuaProxy *_lua = itr->second;

		try {
			sol::table _module = _lua->state_.get<sol::table>("module");

			if (_lua->app_state_ == LuaAppState::INIT) {
				_lua->call_list_[LuaAppState::INIT](_module);
				_lua->app_state_ = LuaAppState::EXECUTE;
			}
			else if (_lua->app_state_ == LuaAppState::EXECUTE) {
				_lua->call_list_[LuaAppState::EXECUTE](_module);
			}
			else if (_lua->app_state_ == LuaAppState::SHUT) {
				_lua->call_list_[LuaAppState::SHUT](_module);
				_lua->app_state_ = LuaAppState::AFTER_SHUT;
			}
			else if (_lua->app_state_ = LuaAppState::AFTER_SHUT) {
				_lua->call_list_[LuaAppState::AFTER_SHUT](_module);
			}
		}
		catch (sol::error e) {
			std::string _err = e.what() + '\n' + Traceback(_lua->state_);
			APP.ERR_LOG("LuaProxy", _err);

			if (_lua->app_state_ == LuaAppState::INIT) {
				destroy(_lua->lua_id_);
				break;
			}
			else {

			}
		}

		if (_lua->app_state_ == LuaAppState::AFTER_SHUT) {

			itr = luaMap_.erase(itr);
		}
		else {
			++itr;
		}
	}
}

void gsf::modules::LuaProxyModule::shut()
{
	for (auto itr : luaMap_)
	{
		uint32_t _module_id = std::get<0>(itr);
		destroy(_module_id);
	}

	luaMap_.clear();
}

void gsf::modules::LuaProxyModule::eCreate(gsf::ArgsPtr args, gsf::CallbackFunc callback /* = nullptr */)
{
	uint32_t _module_id = args->pop_i32();
	std::string _dir_name = APP.getScriptPath();
	std::string _file_name = args->pop_string();

	create(_module_id, _dir_name, _file_name);
}

void gsf::modules::LuaProxyModule::ldispatch(gsf::ModuleID lua_id, gsf::ModuleID target, gsf::EventID event, const std::string &buf, const sol::function &func)
{
	auto _smartPtr = gsf::ArgsPool::get_ref().get();
	auto _luaVirtual = findLua(lua_id);

	auto _callback = [func](gsf::ArgsPtr cptr) {
		try {
			auto _pos = cptr->get_size();
			auto _req = cptr->pop_block(0, _pos);
			func(_req, _pos);
		}
		catch (sol::error e) {
			APP.ERR_LOG("LuaProxy", "unknown err by ldispatch _callback");
		}
		catch (...)
		{
			APP.ERR_LOG("LuaProxy", "unknown err by ldispatch _callback");
		}
	};

	try {
		_smartPtr->push_block(buf.c_str(), buf.size());
		if (func.valid()) {
			dispatch(target, event, std::move(_smartPtr), _callback);
		}
		else {
			dispatch(target, event, std::move(_smartPtr));
		}

	}
	catch (sol::error e) {
		std::string _err = e.what() + '\n' + Traceback(findLua(lua_id)->state_);
		APP.ERR_LOG("LuaProxy", _err);
	}
	catch (...)
	{
		APP.ERR_LOG("LuaProxy", "unknown err by ldispatch", " {} {}", target, event);
	}
}

int gsf::modules::LuaProxyModule::llisten(uint32_t lua_id, uint32_t self, uint32_t event, const sol::function &func)
{
	try {
		listen(self, event, [=](gsf::ArgsPtr args, gsf::CallbackFunc callback)->void {
			try {
				std::string _req = "";
				std::string _res = "";
				if (args) {
					auto _pos = args->get_size();
					_req = args->pop_block(0, _pos);
					_res = func(_req, _pos);
				}
				else {
					_res = func(nullptr, 0);
				}

				if (_res != "") {
					//auto _smartPtr = gsf::ArgsPool::get_ref().get();
					//_smartPtr->push_block(_res.c_str(), _res.size());
					//return _smartPtr;
				}
				else {
					//return nullptr;
				}
			}
			catch (sol::error e) {
				std::string _err = e.what() + '\n' + Traceback(findLua(lua_id)->state_);
				APP.ERR_LOG("LuaProxy", _err);
			}
		});
	}
	catch (sol::error e) {
		std::string _err = e.what() + '\n' + Traceback(findLua(lua_id)->state_);
		APP.ERR_LOG("LuaProxy", _err);
	}
	catch (...)
	{
		APP.ERR_LOG("LuaProxy", "unknown err by llisten", " {} {}", self, event);
	}
	return 0;
}

void gsf::modules::LuaProxyModule::lrpc(uint32_t lua_id, uint32_t event, int32_t moduleid, const std::string &buf, const sol::function &func)
{
	auto _smartPtr = gsf::ArgsPool::get_ref().get();
	auto lua = findLua(lua_id);

	try {
		_smartPtr->push_block(buf.c_str(), buf.size());
		
		auto _callback = [=](const ArgsPtr &cbArgs, int32_t progress, bool bResult) {
			try {
				std::string _res = "";
				auto _pos = 0;
				if (nullptr != cbArgs) {
					_pos = cbArgs->get_size();
					_res = cbArgs->pop_block(0, _pos);
				}
				func(_res, _pos, progress, bResult);
			}
			catch (sol::error e) {
				std::string _err = e.what() + '\n' + Traceback(findLua(lua_id)->state_);
				APP.ERR_LOG("LuaProxy", _err);
			}
		};

		if (sol::type::lua_nil == func.get_type()) {
			rpc(event, moduleid, std::move(_smartPtr), nullptr);
		}
		else {
			rpc(event, moduleid, std::move(_smartPtr), _callback);
		}

	}
	catch (sol::error e)
	{
		std::string _err = e.what() + '\n' + Traceback(findLua(lua_id)->state_);
		APP.ERR_LOG("LuaProxy", _err);
	}
	catch (...)
	{
		APP.ERR_LOG("LuaProxy", "unknown err by llisten", " {} {}", moduleid, event);
	}
}

void gsf::modules::LuaProxyModule::create(uint32_t module_id, std::string dir_name, std::string file_name)
{
	LuaProxy *_lua = new LuaProxy(module_id);
	_lua->state_.open_libraries(
		sol::lib::os
		, sol::lib::base
		, sol::lib::package
		, sol::lib::math
		, sol::lib::io
		, sol::lib::table
		, sol::lib::string
		, sol::lib::debug);
	_lua->dir_name_ = dir_name;
	_lua->file_name_ = file_name;

	auto _path = dir_name + "/" + file_name;

	try
	{
		// set pbc to lua global
		//luaopen_protobuf_c(_lua->state_.lua_state());

		auto _ret = _lua->state_.do_file(_path.c_str());
		if (_ret) {
			std::string _err = Traceback(_lua->state_.lua_state()) + " build err " 
				+ _path + '\t' + lua_tostring(_lua->state_.lua_state(), _ret.stack_index()) + "\n";

			APP.ERR_LOG("LuaProxy", _err);
			return;
		}
	}
	catch (sol::error e)
	{
		std::string _err = e.what() + '\n';
		_err += Traceback(_lua->state_.lua_state());

		APP.ERR_LOG("LuaProxy", _err);
	}

	_lua->state_.new_usertype<gsf::Args>("Args"
		, sol::constructors<Args(), Args(const char *, int)>()
		, "push_ui16", &Args::push_ui16
		, "push_ui32", &Args::push_ui32
		, "push_i32", &Args::push_i32
		, "push_string", &Args::push_string
		, "push_bool", &Args::push_bool
		, "pop_string", &Args::pop_string
		, "pop_ui16", &Args::pop_ui16
		, "pop_ui32", &Args::pop_ui32
		, "pop_i32", &Args::pop_i32
		, "pop_i64", &Args::pop_i64
		, "pop_ui64", &Args::pop_ui64
		, "pop_bool", &Args::pop_bool
		, "pop_block", &Args::pop_block
		, "get_size", &Args::get_size
		, "get_tag", &Args::get_tag
		, "to_string", &Args::to_string);

	_lua->state_.new_usertype<LuaProxyModule>("LuaProxyModule"
		, "ldispatch", &LuaProxyModule::ldispatch
		, "llisten", &LuaProxyModule::llisten
		, "lrpc", &LuaProxyModule::lrpc);
	_lua->state_.set("event", this);
	_lua->state_.set("module_id", module_id);

	_lua->state_.new_usertype<gsf::Application>("Application"
		, "getModule", &Application::getModule
		, "getAppName", &Application::getAppName
		, "getMachine", &Application::getMachine
		, "getUUID", &Application::getUUID);
	_lua->state_.set("APP", gsf::Application::get_ptr());

	_lua->call_list_[LuaAppState::BEFORE_INIT] = [&](sol::table t) {
		t.get<std::function<void(std::string)>>("before_init")(dir_name + "/");
	};
	_lua->call_list_[LuaAppState::INIT] = [&](sol::table t) {
		t.get<std::function<void()>>("init")();
	};
	_lua->call_list_[LuaAppState::EXECUTE] = [&](sol::table t) {
		t.get<std::function<void()>>("execute")();
	};
	_lua->call_list_[LuaAppState::SHUT] = [&](sol::table t) {
		t.get<std::function<void()>>("shut")();
	};
	_lua->call_list_[LuaAppState::AFTER_SHUT] = [&](sol::table t) {
		t.get<std::function<void()>>("after_shut")();
	};

	try {
		sol::table _module = _lua->state_.get<sol::table>("module");
		_lua->call_list_[LuaAppState::BEFORE_INIT](_module);
		_lua->app_state_ = LuaAppState::INIT;
	}
	catch (sol::error e) {
		std::string _err = e.what() + '\n';
		_err += Traceback(_lua->state_.lua_state());
		APP.ERR_LOG("LuaProxy", _err);

		//! 清除此lua模块相关的事件
		wipeout(_lua->lua_id_);

		//! 销毁Lua虚拟机
		delete _lua;
		_lua = nullptr;
		return;
	}

	luaMap_.insert(std::make_pair(module_id, _lua));
}

void gsf::modules::LuaProxyModule::eDistory(gsf::ArgsPtr args, gsf::CallbackFunc callback /* = nullptr */)
{
	uint32_t _module_id = args->pop_i32();
	
	auto _itr = luaMap_.find(_module_id);
	if (_itr == luaMap_.end()) {
		
		APP.ERR_LOG("LuaProxy", "distory Did not find the module", " {}", _module_id);
		return;
	}

	LuaProxy *_lua = _itr->second;
	_lua->app_state_ = LuaAppState::AFTER_SHUT;
}

int gsf::modules::LuaProxyModule::destroy(uint32_t module_id)
{
	auto itr = luaMap_.find(module_id);
	if (itr == luaMap_.end()) {
		return -1;
	}

	LuaProxy *_lua = itr->second;
	if (nullptr == _lua) {
		assert(_lua);
		return -1;
	}

	wipeout(module_id);

	delete _lua;
	_lua = nullptr;
	luaMap_.erase(itr);

	return 0;
}

void gsf::modules::LuaProxyModule::eReload(gsf::ArgsPtr args, gsf::CallbackFunc callback /* = nullptr */)
{
	uint32_t _module_id = args->pop_i32();

	auto _itr = luaMap_.find(_module_id);
	if (_itr == luaMap_.end()) {
		APP.ERR_LOG("LuaProxy", "reload Did not find the module", " {}", _module_id);
		return;
	}

	std::string _dir_name = _itr->second->dir_name_;
	std::string _file_name = _itr->second->file_name_;

	if (destroy(_module_id) != 0) {
		APP.ERR_LOG("LuaProxy", "reload destory module fail!");
		return;
	}

	create(_module_id, _dir_name, _file_name);
}

gsf::modules::LuaProxy * gsf::modules::LuaProxyModule::findLua(uint32_t id)
{
	auto itr = luaMap_.find(id);
	if (itr == luaMap_.end()) {
		return nullptr;
	}
	else {
		return itr->second;
	}
}
