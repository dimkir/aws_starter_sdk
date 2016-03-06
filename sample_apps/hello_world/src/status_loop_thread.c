#include <wmstdio.h>
#include <wm_os.h>
#include <wmsdk.h>
#include <wmstdio.h>

#include "led_indicator.h"

#include "status_loop_thread.h"

#include "common.h"
#include "wmtime.h"

void status_loop_entrypoint(os_thread_arg_t data){
        int count = 0;
	while (1) {
		count++;
                if ( count % 2 == 1 ){
                    led_on(board_led_1());
                }
                else{
                    led_off(board_led_1());
                }
//		wmprintf("Hello World: iteration %d and device state: %d\r\n", count, DEVICE_STATE);
//                led_state()
		/* Sleep  5 seconds */
		os_thread_sleep(os_msec_to_ticks(2000));
	}    
}