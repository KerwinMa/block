﻿#include "wsConnector.h"

#include <core/application.h>



gsf::network::WebSocketConnectorModule::WebSocketConnectorModule(const std::string &name)
	: Module(name)
{

}

gsf::network::WebSocketConnectorModule::WebSocketConnectorModule()
	: Module("WebSocketAcceptorModule")
{

}

gsf::network::WebSocketConnectorModule::~WebSocketConnectorModule()
{

}

void gsf::network::WebSocketConnectorModule::before_init()
{
	using namespace std::placeholders;
	listen(eid::network::websocket_make_connector, std::bind(&WebSocketConnectorModule::eMakeConnector, this, _1, _2));
}

void gsf::network::WebSocketConnectorModule::init()
{
}

void gsf::network::WebSocketConnectorModule::execute()
{


}

void gsf::network::WebSocketConnectorModule::shut()
{

}

void gsf::network::WebSocketConnectorModule::after_shut()
{

}

void gsf::network::WebSocketConnectorModule::eMakeConnector(gsf::ModuleID target, gsf::ArgsPtr args)
{

}