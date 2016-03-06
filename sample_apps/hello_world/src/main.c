/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Hello World Application
 *
 * Summary:
 *
 * Prints Hello World every 5 seconds on the serial console.
 * The serial console is set on UART-0.
 *
 * A serial terminal program like HyperTerminal, putty, or
 * minicom can be used to see the program output.
 */
#include <wmstdio.h>
#include <wm_os.h>
#include <wmsdk.h>
#include <wmstdio.h>
#include <wmlog.h>

#include "led_indicator.h"
#include "status_loop_thread.h"


#include "common.h"

#include "wlan_event.h"

static os_thread_t status_loop_thread;
static os_thread_stack_define(status_loop_stack, 8*1024);


#define CONFIG_DEBUG_BUILD

/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */
int main(void)
{
//	int count = 0;

	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);
        wmprintf("\r\n-------------------------------\r\n");
	wmprintf("Hello World application Started\r\n");
        wmprintf("Build Time: " __DATE__ " " __TIME__ "\r\n");
        wmprintf("-------------------------------\r\n");
        
        
//        status_loop_entrypoint(NULL);
        
        led_fast_blink(board_led_1());
//        led_fast_blink(board_led_2());
//        led_fast_blink(board_led_3()); // no such led?
//        led_fast_blink(board_led_4()); // no such led neither?
        
        
//        int ret  = wm_wlan_connect("Ronda","flamenco");
        int ret  = wm_wlan_connect("UPC0050584","CIQMGGJD");
        wmprintf("Return from wm_wlan_connect() is %d\r\n", ret);
        
        wmprintf("wm_wlan_connect() is non blocking\r\n");
        
        wmlog_e("hello", "this is wmlog and %d", 333);

        // let's start thread
        int tret = os_thread_create(&status_loop_thread, 
                        "statusLoopThread",
                        status_loop_entrypoint,
                        0,
                        &status_loop_stack,
                        OS_PRIO_3
                        );
        
        if ( tret != WM_SUCCESS ){
            wmprintf("Failed to start status_loop thread: %d", tret);
            return -1;
        }

	return 0;
}



