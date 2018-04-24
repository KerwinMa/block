﻿#include "tcpAcceptor.h"

#include "sessionMgr.h"
#include "session.h"

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


#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>

#include <core/application.h>

gsf::network::TcpAcceptorModule::TcpAcceptorModule(const std::string &name)
	: Module(name)
{

}

gsf::network::TcpAcceptorModule::TcpAcceptorModule()
	: Module("AcceptorModule")
{

}

gsf::network::TcpAcceptorModule::~TcpAcceptorModule()
{

}

void gsf::network::TcpAcceptorModule::before_init()
{
	sessionMgr_ = new SessionMgr(this);

	eventBasePtr_ = event_base_new();

	using namespace std::placeholders;

	listen(eid::network::tcp_make_acceptor, std::bind(&TcpAcceptorModule::eMakeAcceptor, this, _1, _2));
	listen(eid::network::send, std::bind(&TcpAcceptorModule::eSendMsg, this, _1, _2));
	listen(eid::network::kick_connect, std::bind(&TcpAcceptorModule::eKick, this, _1, _2));
}

void gsf::network::TcpAcceptorModule::init()
{

}

void gsf::network::TcpAcceptorModule::execute()
{
	if (sessionMgr_) {
		sessionMgr_->exec();
	}

	if (eventBasePtr_) {
		event_base_loop(eventBasePtr_, EVLOOP_ONCE | EVLOOP_NONBLOCK);
	}
}

void gsf::network::TcpAcceptorModule::shut()
{
	evconnlistener_free(acceptListenerPtr_);
}

void gsf::network::TcpAcceptorModule::after_shut()
{
	if (sessionMgr_) {
		delete sessionMgr_;
		sessionMgr_ = nullptr;
	}
}

void gsf::network::TcpAcceptorModule::eMakeAcceptor(gsf::ModuleID target, gsf::ArgsPtr args)
{
	if (nullptr == acceptListenerPtr_) {
		std::string _ip = args->pop_string();
		uint32_t _port = args->pop_i32();

		module_id_ = target;	//! 绑定代理Module的id
		accept_bind(_ip, _port);
	}
	else {
		APP.INFO_LOG("Acceptor Module", "repeat make acceptor");
	}
}

/*
void gsf::network::AcceptorModule::bind_remote(const gsf::ArgsPtr &args, gsf::CallbackFunc callback)
{
	uint32_t _module_id = args->pop_i32();
	uint32_t _msg_id = args->pop_i32();

	auto _info_ptr = std::make_shared<RemoteInfo>(_module_id, _msg_id, callback);
	binder_->regist(_info_ptr);
}
*/
void gsf::network::TcpAcceptorModule::accept_bind(const std::string &ip, int port)
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	acceptListenerPtr_ = evconnlistener_new_bind(eventBasePtr_
		, accept_listen_cb
		, (void*)this
		, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE
		, -1
		, (sockaddr*)&sin
		, sizeof(sockaddr_in));

	if (nullptr == acceptListenerPtr_) {
		APP.ERR_LOG("Acceptor", "accept listen err!");
	}
	else {
		APP.INFO_LOG("Acceptor", "accept listen ready!");
	}
}

void gsf::network::TcpAcceptorModule::accept_listen_cb(::evconnlistener *listener, evutil_socket_t fd, sockaddr *sa, int socklen, void *arg)
{
	gsf::network::TcpAcceptorModule *network_ptr_ = (gsf::network::TcpAcceptorModule*)arg;
	::bufferevent *bev;
	int32_t _ret = 0;

	do
	{
		if (network_ptr_->sessionMgr_->find(fd)) {
			network_ptr_->sessionMgr_->addClose(fd);
			APP.ERR_LOG("acceptor", "repeat fd!");
			break;
		}

		// check max connect
		if (network_ptr_->sessionMgr_->curMaxConnect() >= NETWORK_CONNECT_MAX) {
			APP.ERR_LOG("acceptor", "max connect!");
			break;
		}

		bev = bufferevent_socket_new(network_ptr_->eventBasePtr_, fd, BEV_OPT_CLOSE_ON_FREE);
		if (!bev) {
			APP.ERR_LOG("acceptor", "new socket fail!");
			break;
		}

	} while (0);

	if (0 == _ret) {
		auto _session_ptr = network_ptr_->sessionMgr_->makeSession(fd, network_ptr_->module_id_, bev);
		bufferevent_setcb(bev, Session::readCB, NULL, Session::eventCB, _session_ptr.get());
		bufferevent_enable(bev, EV_READ | EV_WRITE);

		Session::eventCB(bev, BEV_EVENT_CONNECTED, (void*)_session_ptr.get());
	}
}

void gsf::network::TcpAcceptorModule::eSendMsg(gsf::ModuleID target, gsf::ArgsPtr args)
{
	assert(nullptr != acceptListenerPtr_);

	auto _fd = args->pop_fd();
	auto _msg = args->pop_msgid();
	std::string _str = "";

	//! 内部消息走的时Args流， 外部的是原始的二进制数据。 所以这里要分开处理下!
	if (_msg > eid::distributed::rpc_begin && _msg < eid::distributed::rpc_end) {
		//auto _headlen = sizeof(gsf::SessionID) + 1 + sizeof(gsf::MsgID) + 1;
		//_str = args->get_block(_headlen, args->get_size());
	}
	else {
		_str = args->pop_string();
	}

	auto _session_ptr = sessionMgr_->find(_fd);
	if (_session_ptr) {
		auto _block = std::make_shared<gsf::Block>(_fd, _msg, _str);
		_session_ptr->write(_msg, _block);
	}
}

void gsf::network::TcpAcceptorModule::eKick(gsf::ModuleID target, gsf::ArgsPtr args)
{
	auto _fd = args->pop_fd();
	sessionMgr_->addClose(_fd);
}
