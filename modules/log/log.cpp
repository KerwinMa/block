#include "log.h"

#include <string>
#include <sstream>

#define GOOGLE_GLOG_DLL_DECL
#include <glog/logging.h>

gsf::modules::LogModule::LogModule()
	: Module("LogModule")
{

}


void gsf::modules::LogModule::before_init()
{
	listen(this, eid::log::log_callback, [&](gsf::Args args, gsf::CallbackFunc callback) {
		auto _args = gsf::Args();
		_args.push_log_callback(std::bind(&LogModule::log_print, this
			, std::placeholders::_1
			, std::placeholders::_2
			, std::placeholders::_3));
		callback(_args);
	});
}

void gsf::modules::LogModule::init()
{
	listen(this, eid::log::init
		, std::bind(&LogModule::init_impl, this
		, std::placeholders::_1
		, std::placeholders::_2));

	listen(this, eid::log::info
		, std::bind(&LogModule::log_info, this
		, std::placeholders::_1
		, std::placeholders::_2));

	listen(this, eid::log::warning
		, std::bind(&LogModule::log_warning, this
		, std::placeholders::_1
		, std::placeholders::_2));

	listen(this, eid::log::error
		, std::bind(&LogModule::log_error, this
		, std::placeholders::_1
		, std::placeholders::_2));
}

void gsf::modules::LogModule::execute()
{
	while (!log_.empty())
	{
		auto itr = log_.begin();
		gsf::Args args = itr->second;
		
		std::ostringstream oss;

		for (int i = 0; i < args.get_count(); ++i)
		{
			uint32_t _type = args.get_typeid(i);
			switch (_type)
			{
			case 0:
				if (args.pop_bool(i)) {
					oss << "true";
				}
				else {
					oss << "false";
				}
				break;
			case 1:
				oss << args.pop_uint32(i);
				break;
			case 2:
				oss << args.pop_int32(i);
				break;
			case 3:
				oss << args.pop_uint64(i);
				break;
			case 4:
				oss << args.pop_int64(i);
				break;
			case 5:
				oss << args.pop_string(i);
				break;
			}
		}
		
		switch (itr->first)
		{
		case eid::log::info:
			LOG(INFO) << "[INFO] " << oss.str();
			break;
		case eid::log::warning:
			LOG(WARNING) << "[WARNING] " << oss.str();
			break;
		case eid::log::error:
			LOG(ERROR) << "[ERROR] " << oss.str();
			break;
		}

		log_.pop_front();
	}
}

void gsf::modules::LogModule::shut()
{
	google::ShutdownGoogleLogging();
}

void gsf::modules::LogModule::init_impl(gsf::Args args, gsf::CallbackFunc callback)
{
	std::string _path = args.pop_string(0);
	std::string _exe_name = args.pop_string(1);

	FLAGS_log_dir			= _path;	//设置输出路径
	FLAGS_alsologtostderr	= true;		//设置日志消息除了日志文件之外是否去标准输出
	FLAGS_colorlogtostderr	= true;		//设置记录到标准输出的颜色消息（如果终端支持）
	FLAGS_max_log_size		= 10;		//设置最大日志文件大小（以MB为单位）
	FLAGS_logbufsecs		= 0;		//立即写入到日志

	google::SetLogDestination(google::INFO, (_path + "/" + _exe_name + ".info_").c_str());
	google::SetLogDestination(google::WARNING, (_path + "/" + _exe_name + ".warning_").c_str());
	google::SetLogDestination(google::ERROR, (_path + "/" + _exe_name + ".error_").c_str());

	google::InitGoogleLogging(_exe_name.c_str());
}

void gsf::modules::LogModule::log_info(gsf::Args args, gsf::CallbackFunc callback)
{
	log_.push_back(std::make_pair(eid::log::info, args));
}

void gsf::modules::LogModule::log_warning(gsf::Args args, gsf::CallbackFunc callback)
{
	log_.push_back(std::make_pair(eid::log::warning, args));
}

void gsf::modules::LogModule::log_error(gsf::Args args, gsf::CallbackFunc callback)
{
	log_.push_back(std::make_pair(eid::log::error, args));
}

void gsf::modules::LogModule::log_print(uint32_t type, const char * title, gsf::Args args)
{
	log_.push_back(std::make_pair(type, args));
}

