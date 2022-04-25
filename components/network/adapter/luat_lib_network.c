/*
@module  network_adapter
@summary 网络接口适配
@version 1.0
@date    2022.04.11
*/
#include "luat_base.h"
#ifdef LUAT_USE_NETWORK
#include "luat_network_adapter.h"
#include "luat_rtos.h"
#include "luat_zbuff.h"
#define LUAT_LOG_TAG "network"
#include "luat_log.h"
//static const char NW_TYPE[] = "NWA*";
#define LUAT_NW_CTRL_TYPE "NWCTRL*"
typedef struct
{
	network_ctrl_t *netc;
	int cb_ref;	//回调函数
	char *task_name;
}luat_network_ctrl_t;


static int32_t l_network_callback(lua_State *L, void* ptr)
{
    rtos_msg_t* msg = (rtos_msg_t*)lua_topointer(L, -1);
    luat_network_ctrl_t *l_ctrl =(luat_network_ctrl_t *)msg->ptr;
    if (l_ctrl->netc)
    {
    	if (l_ctrl->cb_ref)
    	{
            lua_geti(L, LUA_REGISTRYINDEX, l_ctrl->cb_ref);
            if (lua_isfunction(L, -1)) {
            	lua_pushlightuserdata(L, l_ctrl);
            	lua_pushinteger(L, msg->arg1);
            	lua_pushinteger(L, msg->arg2);
                lua_call(L, 3, 0);
            }
    	}
    	else if (l_ctrl->task_name)
    	{
    	    lua_getglobal(L, "sys_send");
    	    if (lua_isfunction(L, -1)) {
    	        lua_pushstring(L, l_ctrl->task_name);
    	        lua_pushinteger(L, msg->arg1);
    	        lua_pushinteger(L, msg->arg2);
    	        lua_call(L, 3, 0);
    	    }
    	}
    	else
    	{
    	    lua_getglobal(L, "sys_pub");
    	    if (lua_isfunction(L, -1)) {
    	        lua_pushstring(L, LUAT_NW_CTRL_TYPE);
    	        lua_pushinteger(L, l_ctrl->netc->adapter_index);
    	        lua_pushinteger(L, l_ctrl->netc->socket_id);
    	        lua_pushinteger(L, msg->arg1);
    	        lua_pushinteger(L, msg->arg2);
    	        lua_call(L, 5, 0);
    	    }
    	}
    }
    lua_pushinteger(L, 0);
    return 1;
}

static int32_t luat_lib_network_callback(void *data, void *param)
{
	OS_EVENT *event = (OS_EVENT *)data;
	rtos_msg_t msg;
    msg.handler = l_network_callback;
    msg.ptr = param;
    msg.arg1 = event->ID & 0x0fffffff;
    msg.arg2 = event->Param1;
    luat_msgbus_put(&msg, 0);
    return 0;
}

// __gc
static int l_network_gc(lua_State *L)
{
	luat_network_ctrl_t *l_ctrl = ((luat_network_ctrl_t *)luaL_checkudata(L, 1, LUAT_NW_CTRL_TYPE));
    if (l_ctrl->netc)
    {
    	network_socket_force_close(l_ctrl->netc);
    	network_release_ctrl(l_ctrl->netc);
    	l_ctrl->netc = NULL;
    }
    if (l_ctrl->cb_ref)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, l_ctrl->cb_ref);
        l_ctrl->cb_ref = 0;
    }
    if (l_ctrl->task_name)
    {
    	luat_heap_free(l_ctrl->task_name);
    	l_ctrl->task_name = 0;
    }
    return 0;
}

/*
在某个适配的网卡上申请一个network_ctrl
@api    network.create(adapter, cb)
@int 适配器序号， 只能是network.ETH0，network.STA，network.AP，如果不填，会选择最后一个注册的适配器
@string or function string为消息通知的taskName，function则为回调函数，如果固件没有内置sys_wait，则必须是function
当通过回调函数回调消息时，输入给function一共3个参数：
param1为申请的network_ctrl
param2为具体的消息，只能是network.RESET, network.LINK, network.ON_LINE, network.TX_OK, network.RX_NEW, network.CLOSE等等
param3为消息对应的参数
@return 成功返回network_ctrl，失败返回nil
@usage
local netc = network.create(network.ETH0, socket_cb_fun)	--以太网网卡上申请一个network_ctrl,通过socket_cb_fun回调相关消息
local netc = network.create(network.ETH0, "IOT_TASK")	--以太网网卡上申请一个network_ctrl,通过sendMsg方式通知taskName为"IOT_TASK"回调相关消息

*/
static int l_network_create(lua_State *L)
{
	int adapter_index = luaL_optinteger(L, 1, network_get_last_register_adapter());
	if (adapter_index < 0 || adapter_index >= NW_ADAPTER_QTY)
	{
		lua_pushnil(L);
		return 1;
	}

	luat_network_ctrl_t *l_ctrl = (luat_network_ctrl_t *)lua_newuserdata(L, sizeof(luat_network_ctrl_t));
	if (!l_ctrl)
	{
		lua_pushnil(L);
		return 1;
	}
	l_ctrl->netc = network_alloc_ctrl(adapter_index);
	if (!l_ctrl->netc)
	{
		lua_pushnil(L);
		return 1;
	}
	network_init_ctrl(l_ctrl->netc, NULL, luat_lib_network_callback, l_ctrl);
	if (lua_isfunction(L, 2))
	{
        lua_pushvalue(L, 2);
        l_ctrl->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        l_ctrl->task_name = NULL;
	}
	else if (lua_isstring(L, 2))
	{
		l_ctrl->cb_ref = 0;
	    size_t len;
	    const char *buf;
        buf = lua_tolstring(L, 2, &len);//取出字符串数据
		l_ctrl->task_name = luat_heap_malloc(len + 1);
		memset(l_ctrl->task_name, 0, len + 1);
		memcpy(l_ctrl->task_name, buf, len);
	}
	luaL_setmetatable(L, LUAT_NW_CTRL_TYPE);
	return 1;
}

/*
作为客户端断开连接
@api network.debug(ctrl, onoff)
@user_data network.create得到的ctrl
@boolean true 打开debug开关
@return nil 无返回值
@usage network.debug(ctrl, true)
*/
static int l_network_set_debug(lua_State *L)
{
	luat_network_ctrl_t *l_ctrl = ((luat_network_ctrl_t *)luaL_checkudata(L, 1, LUAT_NW_CTRL_TYPE));
	if (lua_isboolean(L, 2))
	{
		l_ctrl->netc->is_debug = lua_toboolean(L, 2);
	}
	return 0;
}

/*
等待网卡linkup
@api network.linkup(ctrl)
@user_data network.create得到的ctrl
@return
boolean true有异常发生，false没有异常，如果有error则不需要看下一个返回值了
boolean true已经linkup，false没有linkup，之后需要接收network.LINK消息
@usage local error, result = network.linkup(ctrl)
*/
static int l_network_linkup(lua_State *L)
{
	luat_network_ctrl_t *l_ctrl = ((luat_network_ctrl_t *)luaL_checkudata(L, 1, LUAT_NW_CTRL_TYPE));
	int result = network_wait_link_up(l_ctrl->netc, 0);
	lua_pushboolean(L, result < 0);
	lua_pushboolean(L, result == 0);
	return 2;


}

/*
作为客户端连接服务器
@api network.connect(ctrl, ip, remote_port, is_udp, local_port, keep_idle, keep_interval, keep_cnt, server_cert, client_cert, client_key, client_password)
@user_data network.create得到的ctrl
@string or int ip或者域名，如果是IPV4，可以是大端格式的int值
@int 服务器端口号，小端格式
@int 本地端口号，小端格式，如果不写，则自动分配一个，如果用户填了端口号则需要小于60000, 默认不写
@boolean 是否是UDP，默认false
@int tcp keep live模式下的idle时间，如果留空则表示不启用，如果是不支持标准posix接口的网卡（比如W5500），则为心跳间隔
@int tcp keep live模式下的探测间隔时间
@int tcp keep live模式下的探测次数
@string TCP模式下的服务器ca证书数据，UDP模式下的PSK，不需要加密传输写nil，后续参数也全部nil
@string TCP模式下的客户端ca证书数据，UDP模式下的PSK-ID，TCP模式下如果不需要验证客户端证书时，忽略，一般不需要验证客户端证书
@string TCP模式下的客户端私钥加密数据
@string TCP模式下的客户端私钥口令数据
@return
boolean true有异常发生，false没有异常，如果有error则不需要看下一个返回值了，如果有异常，后续要close
boolean true已经connect，false没有connect，之后需要接收network.ON_LINE消息
@usage local error, result = network.connect(ctrl, "xxx.xxx.xxx.xxx", xxxx)
*/
static int l_network_connect(lua_State *L)
{
	luat_network_ctrl_t *l_ctrl = ((luat_network_ctrl_t *)luaL_checkudata(L, 1, LUAT_NW_CTRL_TYPE));
	luat_ip_addr_t ip_addr;
	uint8_t is_udp = 0;
	int param_pos = 2;
	uint32_t keep_idle, keep_interval, keep_cnt;
	const char *ip;
	const char *server_cert;
	const char *client_cert;
	const char *client_key;
	const char *client_password;
	size_t ip_len, server_cert_len, client_cert_len, client_key_len, client_password_len;
	ip_addr.is_ipv6 = 0xff;
	if (lua_isinteger(L, param_pos))
	{
		ip_addr.is_ipv6 = 0;
		ip_addr.ipv4 = lua_tointeger(L, param_pos);
		ip = NULL;
		ip_len = 0;
	}
	else
	{
		ip_len = 0;
	    ip = luaL_checklstring(L, param_pos, &ip_len);
	}
	uint16_t remote_port = luaL_checkinteger(L, ++param_pos);
	uint16_t local_port = luaL_optinteger(L, ++param_pos, 0);
	if (lua_isboolean(L, ++param_pos))
	{
		is_udp = lua_toboolean(L, param_pos);
	}
	keep_idle = luaL_optinteger(L, ++param_pos, 0);
	keep_interval = luaL_optinteger(L, ++param_pos, 0);
	keep_cnt = luaL_optinteger(L, ++param_pos, 0);
	if (lua_isstring(L, ++param_pos))
	{
		server_cert_len = 0;
		server_cert = luaL_checklstring(L, param_pos, &server_cert_len);
	}
	if (lua_isstring(L, ++param_pos))
	{
		client_cert_len = 0;
		client_cert = luaL_checklstring(L, param_pos, &client_cert_len);
	}

	if (lua_isstring(L, ++param_pos))
	{
		client_key_len = 0;
		client_key = luaL_checklstring(L, param_pos, &client_key_len);
	}
	if (lua_isstring(L, ++param_pos))
	{
		client_password_len = 0;
		client_password = luaL_checklstring(L, param_pos, &client_password_len);
	}
	network_set_base_mode(l_ctrl->netc, !is_udp, keep_idle, keep_idle, keep_interval, keep_cnt);
	network_set_local_port(l_ctrl->netc, local_port);
	int result = network_connect(l_ctrl->netc, ip, ip_len, ip_addr.is_ipv6?NULL:&ip_addr, remote_port, 0);
	lua_pushboolean(L, result < 0);
	lua_pushboolean(L, result == 0);
	return 2;
}

/*
作为客户端断开连接
@api network.close(ctrl)
@user_data network.create得到的ctrl
@return
boolean true有异常发生，false没有异常，如果有error则不需要看下一个返回值了
boolean true已经断开，false没有断开，之后需要接收network.CLOSED消息
@usage local error, result = network.close(ctrl)
*/
static int l_network_close(lua_State *L)
{
	luat_network_ctrl_t *l_ctrl = ((luat_network_ctrl_t *)luaL_checkudata(L, 1, LUAT_NW_CTRL_TYPE));
	int result = network_close(l_ctrl->netc, 0);
	lua_pushboolean(L, result < 0);
	lua_pushboolean(L, result == 0);
	return 2;
}

/*
发送数据给对端
@api network.tx(ctrl, data, ip, port, flag)
@user_data network.create得到的ctrl
@string or user_data zbuff  要发送的数据
@string or int 对端IP，如果是TCP应用则忽略，如果是UDP，如果留空则用connect时候的参数，如果是IPV4，可以是大端格式的int值
@int 对端端口号，小端格式，如果是TCP应用则忽略，如果是UDP，如果留空则用connect时候的参数
@int 发送参数，目前预留，不起作用
@return
boolean true有异常发生，false没有异常，如果有error则不需要看下一个返回值了，如果有异常，后续要close
boolean true缓冲区满了，false没有异常，如果true，则需要等待一段时间或者等到network.TX_OK消息后再尝试发送，同时忽略下一个返回值
boolean true已经收到应答，false没有收到应答，之后需要接收network.TX_OK消息， 也可以忽略继续发送，直到full==true
@usage local error, full, result = network.tx(ctrl, "123456", "xxx.xxx.xxx.xxx", xxxx)
*/
static int l_network_tx(lua_State *L)
{
	luat_network_ctrl_t *l_ctrl = ((luat_network_ctrl_t *)luaL_checkudata(L, 1, LUAT_NW_CTRL_TYPE));
	luat_ip_addr_t ip_addr;
	luat_zbuff_t *buff = NULL;
	const char *ip;
	const char *data;
	size_t ip_len, data_len;
	ip_addr.is_ipv6 = 0xff;
	if (lua_isstring(L, 2))
	{
		data_len = 0;
		data = luaL_checklstring(L, 2, &data_len);
	}
	else
	{
		buff = ((luat_zbuff_t *)luaL_checkudata(L, 2, LUAT_ZBUFF_TYPE));
		data = buff->addr;
		data_len = buff->used;
	}

	if (lua_isstring(L, 3))
	{
		ip_len = 0;
	    ip = luaL_checklstring(L, 3, &ip_len);

		if (network_string_is_ipv4(ip, ip_len))
		{
			ip_addr.is_ipv6 = 0;
			ip_addr.ipv4 = network_string_to_ipv4(ip, ip_len);
		}
		else
		{
			char *name = luat_heap_malloc(ip_len + 1);
			memcpy(name, ip, ip_len);
			name[ip_len] = 0;
			network_string_to_ipv6(name, &ip_addr);
			free(name);
		}
	}
	else if (lua_isinteger(L, 3))
	{
		ip_addr.is_ipv6 = 0;
		ip_addr.ipv4 = lua_tointeger(L, 3);
	}
	uint32_t tx_len;
	int result = network_tx(l_ctrl->netc, data, data_len, luaL_optinteger(L, 5, 0), &ip_addr, luaL_optinteger(L, 4, 0), &tx_len, 0);
	lua_pushboolean(L, result < 0);
	lua_pushboolean(L, tx_len != data_len);
	lua_pushboolean(L, result == 0);
	return 2;
}

/*
接收对端发出的数据，注意数据已经缓存在底层，使用本函数只是提取出来，UDP模式下一次只会取出一个数据包
@api network.rx(ctrl, buff, start, flag)
@user_data network.create得到的ctrl
@user_data zbuff 存放接收的数据，如果缓冲区不够大会自动扩容
@int 接收参数，目前预留，不起作用
@return
boolean true有异常发生，false没有异常，如果有异常，后续要close
int 本次接收到数据长度
string 对端IP，只有UDP模式下才有意义，TCP模式返回nil，注意返回的格式，如果是IPV4，1byte 0x00 + 4byte地址 如果是IPV6，1byte 0x01 + 16byte地址
int 对端port，只有UDP模式下才有意义，TCP模式返回0
@usage local error, data_len, ip, port = network.rx(ctrl, buff)
*/
static int l_network_rx(lua_State *L)
{
	luat_network_ctrl_t *l_ctrl = ((luat_network_ctrl_t *)luaL_checkudata(L, 1, LUAT_NW_CTRL_TYPE));
	luat_zbuff_t *buff = ((luat_zbuff_t *)luaL_checkudata(L, 2, LUAT_ZBUFF_TYPE));

	luat_ip_addr_t ip_addr;
	uint8_t ip[17];
	uint16_t port;
	uint8_t new_flag = 0;
	int rx_len;
	int total_len;
	int result = network_rx(l_ctrl->netc, NULL, 0, 0, NULL, NULL, &total_len);
	if (result < 0)
	{
		lua_pushboolean(L, 1);
		lua_pushinteger(L, 0);
		lua_pushnil(L);
		lua_pushnil(L);
	}
	else if (!total_len)
	{
		lua_pushboolean(L, 0);
		lua_pushinteger(L, 0);
		lua_pushnil(L);
		lua_pushnil(L);
	}
	else
	{
		if ((buff->len - buff->used) < total_len)
		{
			__zbuff_resize(buff, total_len + buff->used);
		}
		result = network_rx(l_ctrl->netc, buff->addr + buff->used, total_len, 0, &ip_addr, &port, &rx_len);
		if (result < 0)
		{
			lua_pushboolean(L, 1);
			lua_pushinteger(L, 0);
			lua_pushnil(L);
			lua_pushnil(L);
		}
		else if (!rx_len)
		{
			lua_pushboolean(L, 0);
			lua_pushinteger(L, 0);
			lua_pushnil(L);
			lua_pushnil(L);
		}
		else
		{
			buff->used += rx_len;
			lua_pushboolean(L, 0);
			lua_pushinteger(L, rx_len);
			if (l_ctrl->netc->is_tcp)
			{
				lua_pushnil(L);
				lua_pushnil(L);
			}
			else
			{
				if (ip_addr.is_ipv6)
				{
					ip[0] = 0;
					memcpy(ip + 1, &ip_addr.ipv4, 4);
					lua_pushlstring(L, ip, 5);
				}
				else
				{
					ip[0] = 1;
					memcpy(ip + 1, &ip_addr.ipv6_u8_addr, 16);
					lua_pushlstring(L, ip, 17);
				}
				lua_pushinteger(L, port);
			}
		}
	}
	return 4;
}

/*
等待新的socket消息，在连接成功和发送数据成功后，使用一次将network状态转换到接收新数据
@api network.wait(ctrl)
@user_data network.create得到的ctrl
@return
boolean true有异常发生，false没有异常，如果有异常，后续要close
boolean true有新的数据需要接收，false没有数据，之后需要接收network.EVENT消息
@usage local error, result = network.wait(ctrl)
*/
static int l_network_wait(lua_State *L)
{
	luat_network_ctrl_t *l_ctrl = ((luat_network_ctrl_t *)luaL_checkudata(L, 1, LUAT_NW_CTRL_TYPE));
	int result = network_wait_event(l_ctrl->netc, NULL, 0, NULL);
	lua_pushboolean(L, result < 0);
	lua_pushboolean(L, result == 0);
	return 2;
}


static int l_network_listen(lua_State *L)
{

}

static int l_network_accept(lua_State *L)
{

}

/*
主动释放掉network_ctrl
@api    network.release(ctrl)
@user_data	network.create得到的ctrl
@return 无
@usage network.release(ctrl)
*/
static int l_network_release(lua_State *L)
{
	return l_network_gc(L);
}

#include "rotable2.h"
static const rotable_Reg_t reg_network_adapter[] =
{
	{"create",			ROREG_FUNC(l_network_create)},
	{"debug",		ROREG_FUNC(l_network_set_debug)},
	{"linkup",			ROREG_FUNC(l_network_linkup)},
	{"connect",			ROREG_FUNC(l_network_connect)},
	{"close",			ROREG_FUNC(l_network_close)},
	{"tx",			ROREG_FUNC(l_network_tx)},
	{"rx",			ROREG_FUNC(l_network_rx)},
	{"wait",			ROREG_FUNC(l_network_wait)},
	{"listen",			ROREG_FUNC(l_network_listen)},
	{"accept",			ROREG_FUNC(l_network_accept)},
	{"release",			ROREG_FUNC(l_network_release)},
    { "ETH0",           ROREG_INT(NW_ADAPTER_INDEX_ETH0)},
	{ "STA",          	ROREG_INT(NW_ADAPTER_INDEX_STA)},
	{ "AP",     		ROREG_INT(NW_ADAPTER_INDEX_AP)},
    { "LINK",           ROREG_INT(EV_NW_RESULT_LINK & 0x0fffffff)},
	{ "ON_LINE",          	ROREG_INT(EV_NW_RESULT_CONNECT & 0x0fffffff)},
	{ "EVENT",          	ROREG_INT(EV_NW_RESULT_EVENT & 0x0fffffff)},
	{ "TX_OK",     		ROREG_INT(EV_NW_RESULT_TX & 0x0fffffff)},
	{ "CLOSED",     		ROREG_INT(EV_NW_RESULT_CLOSE & 0x0fffffff)},
	{ NULL,            ROREG_INT(0)}
};

LUAMOD_API int luaopen_network_adapter( lua_State *L ) {
    luat_newlib2(L, reg_network_adapter);
    luaL_newmetatable(L, LUAT_NW_CTRL_TYPE); /* create metatable for file handles */
    lua_pushcfunction(L, l_network_gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1); /* pop new metatable */
    return 1;
}
#endif