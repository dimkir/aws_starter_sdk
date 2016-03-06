#include <wmstdio.h>
#include <wm_os.h>
#include <wmsdk.h>
#include <wmstdio.h>

#include "wlan_event.h"
#include "common.h"
#include "led_indicator.h"

void wlan_event_normal_connect_failed(void* data){
    wmprintf("*** wlan_event_normal_connect_failed() called\r\n");
    led_fast_blink(board_led_2());
    DEVICE_STATE = DISCONNETED;
}

void wlan_event_normal_connected(void* data){
    wmprintf("*** wlan_event_normal_connected\r\n");
    led_on(board_led_2());
    DEVICE_STATE = CONNECTED;
    
}

void wlan_event_normal_link_lost(void* data){
    wmprintf("*** wlan_event_normal_link_lost()\r\n");
    led_fast_blink(board_led_2());
    DEVICE_STATE = DISCONNETED;
}


void wlan_event_connect_failed(){
    wmprintf("--- wlan_event_connect_failed()\r\n");
    led_fast_blink(board_led_2());
    DEVICE_STATE = DISCONNETED;
    
}