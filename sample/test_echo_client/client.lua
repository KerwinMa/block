module = {
    before_init = function() end,
	init = function(module_id) end,
	execute = function() end,
	shut = function() end,
}

local connect_id_ = 0
local log_id_ = 0
local module_id_ = 0
local session_id_ = 0

function msg_function(fd, msg_id, block)
    print_info(log_id_, "msg_function " .. tostring(msg_id) .. " " .. block)

    event:ldispatch_remote(connect_id_, session_id_, 1001, "hello")
end

function connect(ip, port)

    local function _new_connect(args, callback)
        session_id_ = args:pop_uint32(0)
        print_info(log_id_, "new connect fd : ", session_id_)

        event:ldispatch_remote(connect_id_, session_id_, 1001, "hello")
    end

    local function _dis_connect(args, callback)
        print_info(log_id_, "dis connect fd : ", args:pop_uint32(0)) 
    end

    listen(module_id_, eid.network.new_connect, _new_connect)
    listen(module_id_, eid.network.dis_connect, _dis_connect)

    dispatch(connect_id_, eid.network.make_connector, module_id_, ip, port)

end

module.before_init = function()
    print("before_init")

    local package_path = {}
    table.insert(package_path, "F:/github/gsf/common/?.lua")
    package.path = table.concat(package_path, ';')

    require "event"
    require "event_list"

    local function _get_log(args)
        log_id_ = args:pop_uint32(0)
        print_info(log_id_, "log id : " .. log_id_)
    end

    cb_dispatch(eid.app_id, eid.get_module, "LogModule", _get_log)
end

module.init = function(module_id)
    module_id_ = module_id
	print("init case_login module : " .. module_id)

    local function _create_connector(args)
        connect_id_ = args:pop_uint32(0)
        print_info(log_id_, "connect id : " .. connect_id_)
        connect("127.0.0.1", 8001)
    end

    cb_dispatch(eid.app_id, eid.create_dynamic_module, "ConnectorModule", _create_connector)

    dispatch(eid.app_id
        , eid.network.bind_remote_callback
        , module_id_
        , 1002
        , msg_function)

end


module.execute = function()
	
end


module.shut = function()
	print("shut")
end
