﻿#include "connector.h"

#include "session_mgr.h"
#include "session.h"

#include <event2/buffer.h>
#include <event2/listener.h>


#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#endif // WIN32

gsf::network::ConnectorModule::ConnectorModule(const std::string &name)
	: Module(name)
{
}

gsf::network::ConnectorModule::ConnectorModule()
	: Module("ConnectorModule")
{

}

gsf::network::ConnectorModule::~ConnectorModule()
{

}

void gsf::network::ConnectorModule::before_init()
{
	new SessionMgr();

	event_base_ptr_ = event_base_new();

	listen(this, eid::network::make_connector
		, std::bind(&ConnectorModule::make_connector, this
			, std::placeholders::_1
			, std::placeholders::_2));

	listen_remote(this
		, std::bind(&ConnectorModule::send_msg, this
			, std::placeholders::_1
			, std::placeholders::_2
			, std::placeholders::_3));

	dispatch(eid::app_id, eid::get_module, gsf::Args(std::string("LogModule")), [=](gsf::Args args)
	{
		log_module_ = args.pop_uint32(0);
	});
}

void gsf::network::ConnectorModule::init()
{
	dispatch(get_module_id(), eid::network::connector_init, gsf::Args(""));
}

void gsf::network::ConnectorModule::execute()
{
	//! 先处理断连

	if (event_base_ptr_) {
		event_base_loop(event_base_ptr_, EVLOOP_ONCE | EVLOOP_NONBLOCK);
	}
}

void gsf::network::ConnectorModule::shut()
{
	bufferevent_free(buffer_event_ptr_);
	event_base_free(event_base_ptr_);

	bind_clear(get_module_id());
}

void gsf::network::ConnectorModule::after_shut()
{

}

void gsf::network::ConnectorModule::make_connector(gsf::Args args, gsf::CallbackFunc callback)
{
	uint32_t _module_id = args.pop_uint32(0);
	std::string _ip = args.pop_string(1);
	uint32_t _port = args.pop_uint32(2);

	module_id_ = _module_id;

	int32_t _ret = 0;
	int _fd = 0;

	do {
		buffer_event_ptr_ = bufferevent_socket_new(event_base_ptr_, -1, BEV_OPT_CLOSE_ON_FREE);
		if (!buffer_event_ptr_) {
			_ret = eid::network::err_socket_new;
			break;
		}

		struct sockaddr_in _sin;
		memset(&_sin, 0, sizeof(_sin));
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(_port);
		_sin.sin_addr.s_addr = inet_addr(_ip.c_str());

		if (bufferevent_socket_connect(buffer_event_ptr_, (sockaddr*)&_sin, sizeof(sockaddr_in)) < 0) {
			_ret = eid::network::err_socket_connect;
			break;
		}
		else {
			_fd = bufferevent_getfd(buffer_event_ptr_);
		}
	} while (0);


	if (_ret == 0) {
		session_ptr_ = std::make_shared<Session>(_fd, _module_id, std::bind(&ConnectorModule::need_close_session, this, std::placeholders::_1));
		bufferevent_setcb(buffer_event_ptr_, Session::read_cb, NULL, Session::err_cb, session_ptr_.get());
		bufferevent_enable(buffer_event_ptr_, EV_READ | EV_WRITE);

		session_ptr_->set_log_module(log_module_);

		gsf::Args res;
		res << uint32_t(_fd);
		dispatch(_module_id, eid::network::new_connect, res);
	}
}


void gsf::network::ConnectorModule::need_close_session(int fd)
{
	// 
	disconnect_vec_.push_back(fd);
}

void gsf::network::ConnectorModule::send_msg(std::vector<uint32_t> fd_vec, uint32_t msg_id, BlockPtr blockptr)
{
	session_ptr_->write(msg_id, blockptr);
}
