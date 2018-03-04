#include "event.h"
#include "application.h"
#include <algorithm>


void gsf::EventModule::execute()
{
	while (!event_queue_.empty())
	{
		auto _info = event_queue_.front();

		auto _reg = type_map_.find(_info.target_);
		if (_reg != type_map_.end()){
			auto _regList = _reg->second;
			auto _findItr = std::find_if(_regList.begin(), _regList.end(), [&](MIList::value_type it) {
				return (it.event_id_ == _info.event_);
			});

			if (_findItr != _regList.end()){
#ifdef WATCH_PERF
				_findItr->calls_++;
#endif
                _findItr->event_func_()
			}
			else {
				APP.WARN_LOG("EventCenter", "Did not find the event from module", " {} {}", _info.event_, _info.target_);
			}
		}
		else {
			APP.WARN_LOG("EventCenter", "Did not find the module", " {}", _info.target_);
		}

		event_queue_.pop_front();
	}
}

gsf::EventModule::EventModule()
	: Module("EventModule")
{

}


void gsf::EventModule::bind_event(uint32_t module_id, uint32_t event, DispatchFunc func)
{
	auto regf = [&](MIList &itr) {

		ModuleIterfaceObj _obj;
		_obj.event_id_ = event;
		_obj.event_func_ = func;

		itr.push_back(_obj);
	};

	auto typeItr = type_map_.find(module_id);
	if (typeItr != type_map_.end()) {

		auto listItr = typeItr->second;

		auto fItr = std::find_if(listItr.begin(), listItr.end(), [&](MIList::value_type it) {
			return (it.event_id_ == event);
		});

		if (fItr != listItr.end()) {
			printf("repeated event!\n");
			return;
		}

		regf(typeItr->second);
	}
	else {
		MIList _list;
		regf(_list);

		type_map_.insert(std::make_pair(module_id, _list));
	}
}

void gsf::EventModule::dispatch(uint32_t module_id, uint32_t event, const ArgsPtr &args)
{
	EventInfo _einfo;
	_einfo.event_ = event;
	_einfo.target_ = module_id;
	_einfo.ptr_ = std::move(args);
	event_queue_.push_back(_einfo);
}

void gsf::EventModule::boardcast(uint32_t event, const ArgsPtr &args)
{
	for (auto &it : type_map_)
	{
		dispatch(it.first, event, args);
	}
}

void gsf::EventModule::bind_rpc(RpcFunc rpc_callback)
{
	rpc_ = rpc_callback;
}

void gsf::EventModule::dispatch_rpc(uint32_t event, int32_t moduleid, const ArgsPtr &args, RpcCallback callback /* = nullptr */)
{
	assert(rpc_);
	rpc_(event, moduleid, args, callback);
}

void gsf::EventModule::rmv_event(ModuleID module_id)
{
	auto tItr = type_map_.find(module_id);

	if (tItr != type_map_.end()) {
		tItr->second.clear();
	}
}

void gsf::EventModule::rmv_event(ModuleID module_id, EventID event_id)
{
	auto tItr = type_map_.find(module_id);

	if (tItr != type_map_.end()) {
	
		auto listItr = tItr->second;
		auto fItr = std::find_if(listItr.begin(), listItr.end(), [&](MIList::value_type it) {
			return (it.event_id_ == event_id);
		});

		if (fItr != listItr.end()) {
			listItr.erase(fItr);
		}
	}
}

gsf::IEvent::IEvent()
{
}

gsf::IEvent::~IEvent()
{

}

void gsf::IEvent::rpc_listen(RpcFunc rpc_callback)
{
	EventModule::get_ref().bind_rpc(rpc_callback);
}

void gsf::IEvent::listen(Module *target, uint32_t event, DispatchFunc func)
{
	EventModule::get_ref().bind_event(target->get_module_id(), event, func);
}

void gsf::IEvent::listen(ModuleID self, uint32_t event, DispatchFunc func)
{
	EventModule::get_ref().bind_event(self, event, func);
}

gsf::ArgsPtr gsf::IEvent::dispatch(uint32_t target, uint32_t event, const gsf::ArgsPtr &args)
{
	return EventModule::get_ref().dispatch(target, event, args);
}

void gsf::IEvent::boardcast(uint32_t event, const gsf::ArgsPtr &args)
{
	EventModule::get_ref().boardcast(event, args);
}


void gsf::IEvent::rpc(uint32_t event, int32_t moduleid, const ArgsPtr &args, RpcCallback callback /* = nullptr */)
{
	EventModule::get_ref().dispatch_rpc(event, moduleid, args, callback);
}

void gsf::IEvent::wipeout(ModuleID self)
{
	EventModule::get_ref().rmv_event(self);
}

void gsf::IEvent::wipeout(Module *self, EventID event_id)
{
	EventModule::get_ref().rmv_event(self->get_module_id(), event_id);
}

void gsf::IEvent::wipeout(Module *self)
{
	EventModule::get_ref().rmv_event(self->get_module_id());
}

void gsf::IEvent::wipeout(ModuleID self, EventID event_id)
{
	EventModule::get_ref().rmv_event(self, event_id);
}

