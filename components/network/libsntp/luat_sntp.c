
#include "luat_base.h"

#include "luat_network_adapter.h"
#include "luat_rtos.h"
#include "luat_msgbus.h"
#include "luat_fs.h"
#include "luat_malloc.h"
#include "luat_rtc.h"

#include "luat_sntp.h"

#define LUAT_LOG_TAG "sntp"
#include "luat_log.h"

static const char* sntp_server[] = {
    "ntp1.aliyun.com",
    "ntp2.aliyun.com",
    "ntp3.aliyun.com"
};
static sntp_server_num = 0;

static const uint8_t sntp_packet[48]={0x1b};

static int l_sntp_event_handle(lua_State* L, void* ptr) {
    if (lua_getglobal(L, "sys_pub") != LUA_TFUNCTION) {
        return 0;
    };
    // LLOGD("TIME_SYNC %d", status);
    lua_pushstring(L, "NTP_UPDATE");
    lua_call(L, 1, 0);
    return 0;
}


int luat_sntp_connect(network_ctrl_t *sntp_netc){
    int ret;
    luat_ip_addr_t ip_addr;
#ifdef LUAT_USE_LWIP
	ip_addr.type = 0xff;
#else
	ip_addr.is_ipv6 = 0xff;
#endif
    if (sntp_server_num >= sizeof(sntp_server))
        return -1;
#ifdef LUAT_USE_LWIP
	ret = network_connect(sntp_netc, sntp_server[sntp_server_num], strlen(sntp_server[sntp_server_num]), (0xff == ip_addr.type)?NULL:&(ip_addr), 123, 1000);
#else
	ret = network_connect(sntp_netc, sntp_server[sntp_server_num], strlen(sntp_server[sntp_server_num]), (0xff == ip_addr.is_ipv6)?NULL:&(ip_addr), 123, 1000);
#endif
    sntp_server_num++;
	LLOGD("network_connect ret %d", ret);
	if (ret < 0) {
        network_close(sntp_netc, 0);
        return -1;
    }
    return 0;
}

int luat_sntp_close_socket(network_ctrl_t *sntp_netc){
    if (sntp_netc){
		network_force_close_socket(sntp_netc);
	}
	if (sntp_server_num > 0 && sntp_server_num < sizeof(sntp_server)){
		luat_sntp_connect(sntp_netc);
	}else{
        network_release_ctrl(sntp_netc);
        sntp_server_num = 0;
    }
    return 0;
}

int32_t luat_sntp_callback(void *data, void *param) {
	OS_EVENT *event = (OS_EVENT *)data;
	network_ctrl_t *sntp_netc =(network_ctrl_t *)param;
	int ret = 0;
    uint32_t tx_len = 0;

	// LLOGD("LINK %d ON_LINE %d EVENT %d TX_OK %d CLOSED %d",EV_NW_RESULT_LINK & 0x0fffffff,EV_NW_RESULT_CONNECT & 0x0fffffff,EV_NW_RESULT_EVENT & 0x0fffffff,EV_NW_RESULT_TX & 0x0fffffff,EV_NW_RESULT_CLOSE & 0x0fffffff);
	// LLOGD("network sntp cb %8X %s %8X",event->ID & 0x0ffffffff, event2str(event->ID & 0x0ffffffff) ,event->Param1);
	if (event->ID == EV_NW_RESULT_LINK){
		return 0; // 这里应该直接返回, 不能往下调用network_wait_event
	}else if(event->ID == EV_NW_RESULT_CONNECT){
        network_tx(sntp_netc, sntp_packet, sizeof(sntp_packet), 0, NULL, 0, &tx_len, 0);
        // LLOGD("luat_sntp_callback tx_len:%d",tx_len);
	}else if(event->ID == EV_NW_RESULT_EVENT){
		uint32_t total_len = 0;
		uint32_t rx_len = 0;
		int result = network_rx(sntp_netc, NULL, 0, 0, NULL, NULL, &total_len);
		// LLOGD("result:%d total_len:%d",result,total_len);
		if (0 == result){
			if (total_len>0){
				char* resp_buff = luat_heap_malloc(total_len + 1);
				resp_buff[total_len] = 0x00;
next:
				result = network_rx(sntp_netc, resp_buff, total_len, 0, NULL, NULL, &rx_len);
				// LLOGD("result:%d rx_len:%d",result,rx_len);
				// LLOGD("resp_buff:%.*s len:%d",total_len,resp_buff,total_len);
				if (result)
					goto next;
				if (rx_len == 0||result!=0) {
                    luat_heap_free(resp_buff);
					luat_sntp_close_socket(sntp_netc);
					return -1;
				}
                const uint8_t *p = (const uint8_t *)resp_buff+40;
                uint32_t time =  (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
                LLOGD("time:%d",time - 2208988800);
				luat_heap_free(resp_buff);
#ifdef __LUATOS__
                rtos_msg_t msg;
                msg.handler = l_sntp_event_handle;
                int re = luat_msgbus_put(&msg, 0);
#endif
                luat_rtc_set_tamp32(time);
                sntp_server_num = 0;
                luat_sntp_close_socket(sntp_netc);
			}
		}else{
			luat_sntp_close_socket(sntp_netc);
			return -1;
		}
	}else if(event->ID == EV_NW_RESULT_TX){

	}else if(event->ID == EV_NW_RESULT_CLOSE){

	}
	if (event->Param1){
		LLOGW("sntp_callback param1 %d, closing socket", event->Param1);
		luat_sntp_close_socket(sntp_netc);
	}
	ret = network_wait_event(sntp_netc, NULL, 0, NULL);
	if (ret < 0){
		LLOGW("network_wait_event ret %d, closing socket", ret);
		luat_sntp_close_socket(sntp_netc);
		return -1;
	}
    return 0;
}

int ntp_get(void){
	int adapter_index = network_get_last_register_adapter();
	if (adapter_index < 0 || adapter_index >= NW_ADAPTER_QTY){
		return -1;
	}
	network_ctrl_t *sntp_netc = network_alloc_ctrl(adapter_index);
	if (!sntp_netc){
		LLOGW("network_alloc_ctrl fail");
		return -1;
	}
	network_init_ctrl(sntp_netc, NULL, luat_sntp_callback, sntp_netc);
	network_set_base_mode(sntp_netc, 0, 10000, 0, 0, 0, 0);
	network_set_local_port(sntp_netc, 0);
	network_deinit_tls(sntp_netc);
    return luat_sntp_connect(sntp_netc);
}


