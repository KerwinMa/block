//
// Created by pojol on 2017/2/13.
//

#ifndef _GSF_EVENT_HEADER_
#define _GSF_EVENT_HEADER_

#pragma warning(disable:4819)

#include "module.h"
#include "event_handler.h"
#include "event_list.h"
#include "types.h"

#include <functional>
#include <tuple>
#include <list>
#include <vector>
#include <unordered_map>

#include "args.h"

#include "single.h"

namespace gsf
{
	typedef std::pair<uint32_t, uint32_t> EventPair;
	typedef std::function<void(const gsf::Args&)> CallbackFunc;
	typedef std::function<void(const gsf::Args&, CallbackFunc)> EventFunc;

	class Module;

	class IEvent
	{
	public:
		IEvent();
		virtual ~IEvent();

		// --local--
		/**!
			用于侦听模块之间的消息
		*/
		virtual void listen(Module *target, uint32_t event, EventFunc func);
		virtual void listen(uint32_t self, uint32_t event, EventFunc func);

		/**!
			用于将事件发往不同模块
		*/
		virtual void dispatch(uint32_t target, uint32_t event, const Args &args, CallbackFunc callback = nullptr);


		/**!
			移除module在event层上的绑定.
		*/
		virtual void bind_clear(uint32_t module_id);
	};

	class EventModule
			: public gsf::utils::Singleton<EventModule>
			, public Module
	{
		friend class IEvent;

	public:
		EventModule();

	protected:
		void execute();

		void bind_event(uint32_t type_id, uint32_t event, EventFunc func);

		void add_cmd(uint32_t type_id, uint32_t event, const gsf::Args &args, CallbackFunc callback = nullptr);
		///

		void rmv_event(uint32_t module_id);

    private:
		typedef std::unordered_map<uint32_t, EventFunc> InnerMap;
		typedef std::unordered_map<uint32_t, InnerMap> TypeMap;

		TypeMap type_map_;

	};
}

#endif