
#include "luat_base.h"

#include "luat_network_adapter.h"
#include "libemqtt.h"
#include "luat_rtos.h"
#include "luat_zbuff.h"
#define LUAT_LOG_TAG "mqtt"
#include "luat_log.h"

#define MQTT_RECV_BUF_LEN_MAX 4096
typedef struct
{
	mqtt_broker_handle_t *broker;
	network_ctrl_t *netc;
	luat_ip_addr_t *ip_addr;
	const char *ip;
	uint8_t mqtt_packet_buffer[MQTT_RECV_BUF_LEN_MAX];
	uint8_t mqtt_id;
	uint16_t remote_port;
	uint32_t keepalive;
	uint8_t adapter_index;
	uint8_t mqtt_state;
	void* timer;
}luat_mqtt_ctrl_t;

#define MAX_MQTT_COUNT 32
static int mqtt_cbs[MAX_MQTT_COUNT];
static uint8_t mqtt_id = 0;

#define LUAT_MQTT_CTRL_TYPE "MQTTCTRL*"

typedef struct
{
	uint16_t topic_len;
    uint16_t payload_len;
	uint8_t topic[255];
	uint8_t payload[1000];
}luat_mqtt_msg_t;

static int luat_socket_connect(luat_mqtt_ctrl_t *mqtt_ctrl, const char *hostname, uint16_t port, uint16_t keepalive);

static luat_mqtt_ctrl_t * get_mqtt_ctrl(lua_State *L){
	if (luaL_testudata(L, 1, LUAT_MQTT_CTRL_TYPE)){
		return ((luat_mqtt_ctrl_t *)luaL_checkudata(L, 1, LUAT_MQTT_CTRL_TYPE));
	}else{
		return ((luat_mqtt_ctrl_t *)lua_touserdata(L, 1));
	}
}

static int mqtt_close_socket(luat_mqtt_ctrl_t *mqtt_ctrl){
	if (mqtt_ctrl->netc){
		network_force_close_socket(mqtt_ctrl->netc);
		network_release_ctrl(mqtt_ctrl->netc);
    	mqtt_ctrl->netc = NULL;
	}
	if (mqtt_ctrl->broker){
		luat_heap_free(mqtt_ctrl->broker);
	}
	if (mqtt_ctrl->ip_addr){
		luat_heap_free(mqtt_ctrl->ip_addr);
	}
	if (mqtt_ctrl->ip){
		luat_heap_free(mqtt_ctrl->ip);
	}
	if (mqtt_ctrl){
		luat_heap_free(mqtt_ctrl);
	}
}

static int mqtt_read_packet(luat_mqtt_ctrl_t *mqtt_ctrl)
{
	memset(mqtt_ctrl->mqtt_packet_buffer, 0, MQTT_RECV_BUF_LEN_MAX);
	int total_len;
	int rx_len;
	int result = network_rx(mqtt_ctrl->netc, NULL, 0, 0, NULL, NULL, &total_len);
	result = network_rx(mqtt_ctrl->netc, mqtt_ctrl->mqtt_packet_buffer, total_len, 0, NULL, NULL, &rx_len);
	return rx_len;
}

static int32_t l_mqtt_callback(lua_State *L, void* ptr)
{
    rtos_msg_t* msg = (rtos_msg_t*)lua_topointer(L, -1);
    luat_mqtt_ctrl_t *mqtt_ctrl =(luat_mqtt_ctrl_t *)msg->ptr;
    switch (msg->arg1) {
		case MQTT_MSG_PUBLISH : {
			luat_mqtt_msg_t *mqtt_msg =(luat_mqtt_msg_t *)msg->arg2;
			if (mqtt_cbs[mqtt_ctrl->mqtt_id]) {
				luat_mqtt_msg_t *mqtt_msg =(luat_mqtt_msg_t *)msg->arg2;
				lua_geti(L, LUA_REGISTRYINDEX, mqtt_cbs[mqtt_ctrl->mqtt_id]);
				if (lua_isfunction(L, -1)) {
					lua_pushlightuserdata(L, mqtt_ctrl);
					lua_pushstring(L, "recv");
					lua_pushlstring(L, mqtt_msg->topic,mqtt_msg->topic_len);
					lua_pushlstring(L, mqtt_msg->payload,mqtt_msg->payload_len);
					lua_call(L, 4, 0);
				}
            }
			luat_heap_free(mqtt_msg);
            break;
        }
        case MQTT_MSG_CONNACK: {
			if (mqtt_cbs[mqtt_ctrl->mqtt_id]) {
				lua_geti(L, LUA_REGISTRYINDEX, mqtt_cbs[mqtt_ctrl->mqtt_id]);
				if (lua_isfunction(L, -1)) {
					lua_pushlightuserdata(L, mqtt_ctrl);
					lua_pushstring(L, "conack");
					lua_call(L, 2, 0);
				}
            }
            break;
        }
    }
    lua_pushinteger(L, 0);
    return 1;
}


static int mqtt_msg_cb(luat_mqtt_ctrl_t *mqtt_ctrl) {
	rtos_msg_t msg;
    msg.handler = l_mqtt_callback;
    uint8_t msg_tp = MQTTParseMessageType(mqtt_ctrl->mqtt_packet_buffer);
    switch (msg_tp) {
        case MQTT_MSG_PUBLISH : {
			luat_mqtt_msg_t *mqtt_msg = (luat_mqtt_msg_t *)luat_heap_malloc(sizeof(luat_mqtt_msg_t));
			mqtt_msg->topic_len = mqtt_parse_pub_topic(mqtt_ctrl->mqtt_packet_buffer, mqtt_msg->topic);
            mqtt_msg->payload_len = mqtt_parse_publish_msg(mqtt_ctrl->mqtt_packet_buffer, mqtt_msg->payload);
			msg.ptr = mqtt_ctrl;
			msg.arg1 = MQTT_MSG_PUBLISH;
			msg.arg2 = mqtt_msg;
			luat_msgbus_put(&msg, 0);
            break;
        }
        case MQTT_MSG_CONNACK: {
			if(mqtt_ctrl->mqtt_packet_buffer[3] != 0x00){
                mqtt_close_socket(mqtt_ctrl);
                return -2;
            }
			mqtt_ctrl->mqtt_state = 1;
			msg.ptr = mqtt_ctrl;
			msg.arg1 = MQTT_MSG_CONNACK;
			luat_msgbus_put(&msg, 0);
            break;
        }
        case MQTT_MSG_PINGRESP : {
			LLOGD("MQTT_MSG_PINGRESP");
            break;
        }
		case MQTT_MSG_SUBSCRIBE : {
			LLOGD("MQTT_MSG_SUBSCRIBE");
            break;
        }
        case MQTT_MSG_SUBACK : {
			LLOGD("MQTT_MSG_SUBACK");
            break;
        }
        case MQTT_MSG_UNSUBACK : {
			LLOGD("MQTT_MSG_UNSUBACK");
            break;
        }
        default : {
			LLOGD("mqtt_msg_cb no");
            break;
        }
    }
    return 0;
}

static int32_t luat_lib_mqtt_callback(void *data, void *param)
{
	OS_EVENT *event = (OS_EVENT *)data;
	luat_mqtt_ctrl_t *mqtt_ctrl =(luat_mqtt_ctrl_t *)param;
	int ret = 0;
	LLOGD("LINK %d ON_LINE %d EVENT %d TX_OK %d CLOSED %d",EV_NW_RESULT_LINK & 0x0fffffff,EV_NW_RESULT_CONNECT & 0x0fffffff,EV_NW_RESULT_EVENT & 0x0fffffff,EV_NW_RESULT_TX & 0x0fffffff,EV_NW_RESULT_CLOSE & 0x0fffffff);
	LLOGD("luat_lib_mqtt_callback %d %d",event->ID & 0x0fffffff,event->Param1);
	if (event->ID == EV_NW_RESULT_LINK)
	{
		int ret = luat_socket_connect(mqtt_ctrl, mqtt_ctrl->ip, mqtt_ctrl->remote_port, mqtt_ctrl->keepalive);
		if(ret){
			LLOGD("init_socket ret=%d\n", ret);
		}
	}else if(event->ID == EV_NW_RESULT_CONNECT){
		mqtt_connect(mqtt_ctrl->broker);
		luat_start_rtos_timer(mqtt_ctrl->timer, mqtt_ctrl->keepalive*1000, 1);
	}else if(event->ID == EV_NW_RESULT_EVENT){
		ret = mqtt_read_packet(mqtt_ctrl);
		if (ret > 0)
		{
			ret = mqtt_msg_cb(mqtt_ctrl);
		}
		
	}else if(event->ID == EV_NW_RESULT_TX){

	}else if(event->ID == EV_NW_RESULT_CLOSE){

	}
	network_wait_event(mqtt_ctrl->netc, NULL, 0, NULL);
    return 0;
}

static int mqtt_send_packet(void* socket_info, const void* buf, unsigned int count){
    luat_mqtt_ctrl_t * mqtt_ctrl = (luat_mqtt_ctrl_t *)socket_info;
	uint32_t tx_len;
	network_tx(mqtt_ctrl->netc, buf, count, 0, mqtt_ctrl->ip_addr->type?NULL:&(mqtt_ctrl->ip_addr), NULL, &tx_len, 0);
}

static int luat_socket_connect(luat_mqtt_ctrl_t *mqtt_ctrl, const char *hostname, uint16_t port, uint16_t keepalive){
	if(network_connect(mqtt_ctrl->netc, hostname, strlen(hostname), mqtt_ctrl->ip_addr->type?NULL:&(mqtt_ctrl->ip_addr), port, 0) < 0){
        network_close(mqtt_ctrl->netc, 0);
        return -1;
    }
    mqtt_set_alive(mqtt_ctrl->broker, keepalive);
    mqtt_ctrl->broker->socket_info = mqtt_ctrl;
    mqtt_ctrl->broker->send = mqtt_send_packet;
    return 0;
}

static void mqtt_timer_callback(void *data, void *param)
{
	luat_mqtt_ctrl_t * mqtt_ctrl = (luat_mqtt_ctrl_t *)param;
	mqtt_ping(mqtt_ctrl->broker);
}

static int l_mqtt_subscribe(lua_State *L) {
	size_t len;
	luat_mqtt_ctrl_t * mqtt_ctrl = (luat_mqtt_ctrl_t *)lua_touserdata(L, 1);
	const char * topic = luaL_checklstring(L, 2, &len);
	int subscribe_state = mqtt_subscribe(mqtt_ctrl->broker, topic, NULL);
	return 0;
}

static int l_mqtt_unsubscribe(lua_State *L) {
	size_t len;
	luat_mqtt_ctrl_t * mqtt_ctrl = (luat_mqtt_ctrl_t *)lua_touserdata(L, 1);
	const char * topic = luaL_checklstring(L, 2, &len);
	int subscribe_state = mqtt_unsubscribe(mqtt_ctrl->broker, topic, NULL);
	return 0;
}


static int l_mqtt_create(lua_State *L) {
	int adapter_index = luaL_optinteger(L, 1, network_get_last_register_adapter());
	if (adapter_index < 0 || adapter_index >= NW_ADAPTER_QTY){
		lua_pushnil(L);
		return 1;
	}
	luat_mqtt_ctrl_t *mqtt_ctrl = (luat_mqtt_ctrl_t *)lua_newuserdata(L, sizeof(luat_mqtt_ctrl_t));
	if (!mqtt_ctrl){
		lua_pushnil(L);
		return 1;
	}
	mqtt_ctrl->mqtt_id = mqtt_id++;
	mqtt_ctrl->mqtt_state = 0;
	mqtt_ctrl->adapter_index = adapter_index;
	mqtt_ctrl->netc = network_alloc_ctrl(adapter_index);
	if (!mqtt_ctrl->netc){
		LLOGD("create fail");
		lua_pushnil(L);
		return 1;
	}
	network_init_ctrl(mqtt_ctrl->netc, NULL, luat_lib_mqtt_callback, mqtt_ctrl);

	mqtt_ctrl->netc->is_debug = 1;
	mqtt_ctrl->keepalive = 240;

	network_set_base_mode(mqtt_ctrl->netc, 1, 10000, 0, 0, 0, 0);
	network_set_local_port(mqtt_ctrl->netc, 0);
	network_deinit_tls(mqtt_ctrl->netc);

	int packet_length;
	uint16_t msg_id, msg_id_rcv;
	mqtt_ctrl->broker = (mqtt_broker_handle_t *)luat_heap_malloc(sizeof(mqtt_broker_handle_t));
	
	const char *ip;
	size_t ip_len;

	mqtt_ctrl->ip_addr = (luat_ip_addr_t *)luat_heap_malloc(sizeof(luat_ip_addr_t));
	mqtt_ctrl->ip_addr->type = 0xff;
	if (lua_isinteger(L, 2)){
		mqtt_ctrl->ip_addr->type = IPADDR_TYPE_V4;
		mqtt_ctrl->ip_addr->u_addr.ip4.addr = lua_tointeger(L, 2);
		ip = NULL;
		ip_len = 0;
	}else{
		ip_len = 0;
		ip = luaL_checklstring(L, 2, &ip_len);
	}
	mqtt_ctrl->ip = luat_heap_malloc(ip_len + 1);
	memset(mqtt_ctrl->ip, 0, ip_len + 1);
	memcpy(mqtt_ctrl->ip, ip, ip_len);
	mqtt_ctrl->remote_port = luaL_checkinteger(L, 3);
	mqtt_ctrl->timer = luat_create_rtos_timer(mqtt_timer_callback, mqtt_ctrl, NULL);
	luaL_setmetatable(L, LUAT_MQTT_CTRL_TYPE);
	return 1;

}

static int l_mqtt_auth(lua_State *L) {
	luat_mqtt_ctrl_t * mqtt_ctrl = get_mqtt_ctrl(L);
	const char *client_id = luaL_checkstring(L, 2);
	const char *username = luaL_optstring(L, 3, "");
	const char *password = luaL_optstring(L, 4, "");
	mqtt_init(mqtt_ctrl->broker, client_id);
	mqtt_init_auth(mqtt_ctrl->broker, username, password);
	return 0;
}

static int l_mqtt_keepalive(lua_State *L) {
	luat_mqtt_ctrl_t * mqtt_ctrl = get_mqtt_ctrl(L);
	mqtt_ctrl->keepalive = luaL_optinteger(L, 2, 240);
	return 0;
}

static int l_mqtt_topics(lua_State *L) {
	luat_mqtt_ctrl_t * mqtt_ctrl = get_mqtt_ctrl(L);
	return 0;
}

static int l_mqtt_qos2auto(lua_State *L) {
	luat_mqtt_ctrl_t * mqtt_ctrl = get_mqtt_ctrl(L);
	return 0;
}

static int l_mqtt_on(lua_State *L) {
	luat_mqtt_ctrl_t * mqtt_ctrl = get_mqtt_ctrl(L);
	if (mqtt_cbs[mqtt_ctrl->mqtt_id] != 0) {
		luaL_unref(L, LUA_REGISTRYINDEX, mqtt_cbs[mqtt_ctrl->mqtt_id]);
		mqtt_cbs[mqtt_ctrl->mqtt_id] = 0;
	}
	if (lua_isfunction(L, 2)) {
		lua_pushvalue(L, 2);
		mqtt_cbs[mqtt_ctrl->mqtt_id] = luaL_ref(L, LUA_REGISTRYINDEX);
	}
	return 0;
}

static int l_mqtt_connect(lua_State *L) {
	luat_mqtt_ctrl_t * mqtt_ctrl = get_mqtt_ctrl(L);
	network_wait_link_up(mqtt_ctrl->netc, 0);
	return 0;
}

static int l_mqtt_publish(lua_State *L) {
	luat_mqtt_ctrl_t * mqtt_ctrl = get_mqtt_ctrl(L);
	const char * topic = luaL_checkstring(L, 2);
	const char * payload = luaL_checkstring(L, 3);
	uint8_t qos = luaL_optinteger(L, 4, 0);
	uint8_t retain = luaL_optinteger(L, 5, 0);
	mqtt_publish_with_qos(mqtt_ctrl->broker, topic, payload, retain, qos, NULL);
	return 0;
}

static int l_mqtt_ping(lua_State *L) {
	luat_mqtt_ctrl_t * mqtt_ctrl = get_mqtt_ctrl(L);
	mqtt_ping(mqtt_ctrl->broker);
	return 0;
}

static int l_mqtt_close(lua_State *L) {
	luat_mqtt_ctrl_t * mqtt_ctrl = get_mqtt_ctrl(L);
	mqtt_close_socket(mqtt_ctrl);
	return 0;
}

static int l_mqtt_ready(lua_State *L) {
	luat_mqtt_ctrl_t * mqtt_ctrl = get_mqtt_ctrl(L);
	lua_pushboolean(L, mqtt_ctrl->mqtt_state);
	return 1;
}

int _mqtt_struct_newindex(lua_State *L) {
    const char* key = luaL_checkstring(L, 2);
    if (!strcmp("auth", key)) {
        lua_pushcfunction(L, l_mqtt_auth);
        return 1;
    }
    else if (!strcmp("keepalive", key)) {
        lua_pushcfunction(L, l_mqtt_keepalive);
        return 1;
    }
    else if (!strcmp("topics", key)) {
        lua_pushcfunction(L, l_mqtt_topics);
        return 1;
    }
    else if (!strcmp("qos2auto", key)) {
        lua_pushcfunction(L, l_mqtt_qos2auto);
        return 1;
    }
	else if (!strcmp("on", key)) {
        lua_pushcfunction(L, l_mqtt_on);
        return 1;
    }
	else if (!strcmp("connect", key)) {
        lua_pushcfunction(L, l_mqtt_connect);
        return 1;
    }
	else if (!strcmp("publish", key)) {
        lua_pushcfunction(L, l_mqtt_publish);
        return 1;
    }
	else if (!strcmp("subscribe", key)) {
        lua_pushcfunction(L, l_mqtt_subscribe);
        return 1;
    }
	else if (!strcmp("unsubscribe", key)) {
        lua_pushcfunction(L, l_mqtt_unsubscribe);
        return 1;
    }
	else if (!strcmp("ping", key)) {
        lua_pushcfunction(L, l_mqtt_ping);
        return 1;
    }
	else if (!strcmp("close", key)) {
        lua_pushcfunction(L, l_mqtt_close);
        return 1;
    }
	else if (!strcmp("ready", key)) {
        lua_pushcfunction(L, l_mqtt_ready);
        return 1;
    }
    return 0;
}

void luat_mqtt_struct_init(lua_State *L) {
    luaL_newmetatable(L, LUAT_MQTT_CTRL_TYPE);
    lua_pushcfunction(L, _mqtt_struct_newindex);
    lua_setfield( L, -2, "__index" );
    lua_pop(L, 1);
}

#include "rotable2.h"
static const rotable_Reg_t reg_mqtt[] =
{
	{"create",			ROREG_FUNC(l_mqtt_create)},
	{"auth",			ROREG_FUNC(l_mqtt_auth)},
	{"keepalive",		ROREG_FUNC(l_mqtt_keepalive)},
	{"topics",			ROREG_FUNC(l_mqtt_topics)},
	{"qos2auto",		ROREG_FUNC(l_mqtt_qos2auto)},
	{"on",				ROREG_FUNC(l_mqtt_on)},
	{"connect",			ROREG_FUNC(l_mqtt_connect)},
	{"publish",			ROREG_FUNC(l_mqtt_publish)},
	{"subscribe",		ROREG_FUNC(l_mqtt_subscribe)},
	{"unsubscribe",		ROREG_FUNC(l_mqtt_unsubscribe)},
	{"ping",			ROREG_FUNC(l_mqtt_ping)},
	{"close",			ROREG_FUNC(l_mqtt_close)},
	{"ready",			ROREG_FUNC(l_mqtt_ready)},

	{ NULL,             ROREG_INT(0)}
};

LUAMOD_API int luaopen_mqtt( lua_State *L ) {
    luat_newlib2(L, reg_mqtt);
	luat_mqtt_struct_init(L);
    return 1;
}