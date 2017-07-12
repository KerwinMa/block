#ifndef _COORDINATE_HEADER_
#define _COORDINATE_HEADER_

#include <core/module.h>
#include <core/event.h>

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace gsf
{
	namespace modules
	{
		struct NodeInfo
		{
			std::vector<std::pair<gsf::ModuleID, std::string>> modules;

			std::string ip_ = "";
			uint32_t port_ = 0;

			std::string type_ = "";

			uint32_t weight_ = 0;
		};
		typedef std::shared_ptr<NodeInfo> NodePtr;


		/**
		coodinator Э�������ͬҵ��app֮��ĸ���
		*/
		class Coodinator
			: public gsf::Module
			, public gsf::IEvent
		{
		public:

			Coodinator();

			void before_init() override;


		private:
			void event_adjust_module_weight(const gsf::ArgsPtr &args, gsf::CallbackFunc callback);

			void event_get_light_module(const gsf::ArgsPtr &args, gsf::CallbackFunc callback);

			void event_regist(const gsf::ArgsPtr &args, gsf::CallbackFunc callback);
			void event_unregist(const gsf::ArgsPtr &args, gsf::CallbackFunc callback);

		private:

			std::map<gsf::SessionID, NodePtr> node_id_map_;
			std::multimap<std::string, NodePtr> node_name_map_;
		};
	}
}


#endif // !_COORDINATE_HEADER_
