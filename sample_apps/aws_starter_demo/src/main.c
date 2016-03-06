/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * AWS Starter Demo Application
 *
 * Summary:
 *
 * Application demonstrates the bi-directional communication with the Thing
 * Shadow over MQTT. Using the web application, Configure the device to thing
 * shadow or any other shadow using its Thing name.
 *
 * Device publishes the changed state of the pushbuttons pb and pb_lambda on
 * Thing Shadow. Also it subscribes to the Thing Shadow delta and when state
 * change of LED is requested from AWS IOT web console, it toggles the LED
 * state.
 *
 * The serial console is set on UART-0.
 *
 * A serial terminal program like HyperTerminal, putty, or
 * minicom can be used to see the program output.
 */

#include <wm_os.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <wmsdk.h>

#include <led_indicator.h>
#include <board.h>
#include <push_button.h>

#include <aws_iot_mqtt_interface.h>
#include <aws_iot_shadow_interface.h>
#include <aws_utils.h>
/* configuration parameters */
#include <aws_iot_config.h>


#include "aws_starter_root_ca_cert.h"

enum state {
	AWS_CONNECTED = 1,
	AWS_RECONNECTED,
	AWS_DISCONNECTED
};

/*-----------------------Global declarations----------------------*/

/* These hold each pushbutton's count, updated in the callback ISR */
static volatile uint32_t pushbutton_a_count;
static volatile uint32_t pushbutton_a_count_prev = -1;
//static volatile uint32_t pushbutton_a_count_last_processed = -1; 
// _prev value is updated by the "procssing" thread.
// thus synchronization is done this way. It will miss an increment,
// if other thread didn't actually process it.



static volatile uint32_t pushbutton_b_count;
static volatile uint32_t pushbutton_b_count_prev = -1;


static volatile uint32_t led_1_state;
static volatile uint32_t led_1_state_prev = -1;

static output_gpio_cfg_t led_1;


static MQTTClient_t mqtt_client;



static enum state device_state;

/* Thread handle */
static os_thread_t aws_starter_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(aws_starter_stack, 8 * 1024);

/* Thread handle */
static os_thread_t aws_shadow_yield_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(aws_shadow_yield_stack, 2 * 1024);


/* aws iot url */
static char url[128];

#define MICRO_AP_SSID                "aws_starter"
#define MICRO_AP_PASSPHRASE          "marvellwm"
#define AMAZON_ACTION_BUF_SIZE  100
#define VAR_LED_1_PROPERTY      "led"
#define VAR_BUTTON_A_PROPERTY   "pb"
#define VAR_BUTTON_B_PROPERTY   "pb_lambda"
#define RESET_TO_FACTORY_TIMEOUT 5000
#define BUFSIZE                  128

/* callback function invoked on reset to factory */
static void device_reset_to_factory_cb()
{
	/* Clears device configuration settings from persistent memory
	 * and reboots the device.
	 */
	invoke_reset_to_factory();
}

/* board_button_2() is configured to perform reset to factory,
 * when pressed for more than 5 seconds.
 */
static void configure_reset_to_factory()
{
	input_gpio_cfg_t pushbutton_reset_to_factory = {
		.gpio = board_button_2(),
		.type = GPIO_ACTIVE_LOW
	};
	push_button_set_cb(pushbutton_reset_to_factory,
			   device_reset_to_factory_cb,
			   RESET_TO_FACTORY_TIMEOUT, 0, NULL);
}

/* callback function invoked when pushbutton_a is pressed */
static void pushbutton_a_cb()
{
        // this is how synchronization is done:
        // this thread only updates when their equal. _prev is rather (_last_processed)
	if (pushbutton_a_count_prev == pushbutton_a_count)
		pushbutton_a_count++;
}

/* callback function invoked when pushbutton_b is pressed */
static void pushbutton_b_cb()
{
	if (pushbutton_b_count_prev == pushbutton_b_count)
		pushbutton_b_count++;
}

/* Configure led and pushbuttons with callback functions */
static void configure_led_and_button()
{
	/* respective GPIO pins for pushbuttons and leds are defined in
	 * board file.
	 */
	input_gpio_cfg_t pushbutton_a = {
		.gpio = board_button_1(),
		.type = GPIO_ACTIVE_LOW
	};
	input_gpio_cfg_t pushbutton_b = {
		.gpio = board_button_2(),
		.type = GPIO_ACTIVE_LOW
	};

	led_1 = board_led_1();

	push_button_set_cb(pushbutton_a,
			   pushbutton_a_cb,
			   100, 0, NULL);
	push_button_set_cb(pushbutton_b,
			   pushbutton_b_cb,
			   100, 0, NULL);
}

static char client_cert_buffer[AWS_PUB_CERT_SIZE];
static char private_key_buffer[AWS_PRIV_KEY_SIZE];
#define THING_LEN 126
#define REGION_LEN 16
static char thing_name[THING_LEN];

/* populate aws shadow configuration details */
static int aws_starter_load_configuration(ShadowParameters_t *sp)
{
	int ret = WM_SUCCESS;
	char region[REGION_LEN];
	memset(region, 0, sizeof(region));

	/* read configured thing name from the persistent memory */
	ret = read_aws_thing(thing_name, THING_LEN);
	if (ret == WM_SUCCESS) {
		sp->pMyThingName = thing_name;
	} else {
		/* if not found in memory, take the default thing name */
		sp->pMyThingName = AWS_IOT_MY_THING_NAME;
	}
	sp->pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;

	/* read configured region name from the persistent memory */
	ret = read_aws_region(region, REGION_LEN);
	if (ret == WM_SUCCESS) {
		snprintf(url, sizeof(url), "data.iot.%s.amazonaws.com",
			 region);
	} else {
		snprintf(url, sizeof(url), "data.iot.%s.amazonaws.com",
			 AWS_IOT_MY_REGION_NAME);
	}
	sp->pHost = url;
	sp->port = AWS_IOT_MQTT_PORT;
	sp->pRootCA = rootCA;

	/* read configured certificate from the persistent memory */
	ret = read_aws_certificate(client_cert_buffer, AWS_PUB_CERT_SIZE);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to configure certificate. Returning!\r\n");
		return -WM_FAIL;
	}
	sp->pClientCRT = client_cert_buffer;

	/* read configured private key from the persistent memory */
	ret = read_aws_key(private_key_buffer, AWS_PRIV_KEY_SIZE);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to configure key. Returning!\r\n");
		return -WM_FAIL;
	}
	sp->pClientKey = private_key_buffer;

	return ret;
}

void shadow_update_status_cb(const char *pThingName, ShadowActions_t action,
			     Shadow_Ack_Status_t status,
			     const char *pReceivedJsonDocument,
			     void *pContextData) {

	if (status == SHADOW_ACK_TIMEOUT) {
		wmprintf("Shadow publish state change timeout occurred\r\n");
	} else if (status == SHADOW_ACK_REJECTED) {
		wmprintf("Shadow publish state change rejected\r\n");
	} else if (status == SHADOW_ACK_ACCEPTED) {
		wmprintf("Shadow publish state change accepted\r\n");
	}
}

/* shadow yield thread which periodically checks for data */
static void aws_shadow_yield(os_thread_arg_t data)
{
	while (1) {
		/* periodically check if any data is received on socket */
		aws_iot_shadow_yield(&mqtt_client, 500);
	}
}

/* create shadow yield thread */
static int aws_create_shadow_yield_thread()
{
	int ret;
	ret = os_thread_create(
		/* thread handle */
		&aws_shadow_yield_thread,
		/* thread name */
		"awsShadowYield",
		/* entry function */
		aws_shadow_yield,
		/* argument */
		0,
		/* stack */
		&aws_shadow_yield_stack,
		/* priority */
		OS_PRIO_3);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to create shadow yield thread: %d\r\n", ret);
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

/* This function will get invoked when led state change request is received */
void led_indicator_cb(const char *p_json_string,
		      uint32_t json_string_datalen,
		      jsonStruct_t *p_context) {
	int state;
	if (p_context != NULL) {
		state = *(int *)(p_context->pData);
		if (state) {
			led_on(led_1);
			led_1_state = 1;
		} else {
			led_off(led_1);
			led_1_state = 0;
		}
	}
}

/* Publish thing state to shadow */
int aws_publish_property_state(ShadowParameters_t *sp)
{
	char buf_out[BUFSIZE];
	char state[BUFSIZE];
	char *ptr = state;
	int ret = WM_SUCCESS;

	memset(state, 0, BUFSIZE);
	if (pushbutton_a_count_prev != pushbutton_a_count) {
		snprintf(buf_out, BUFSIZE, ",\"%s\":%d", VAR_BUTTON_A_PROPERTY,
			 pushbutton_a_count);
		strcat(state, buf_out);
		pushbutton_a_count_prev = pushbutton_a_count;
	}
	if (pushbutton_b_count_prev != pushbutton_b_count) {
		snprintf(buf_out, BUFSIZE, ",\"%s\":%d", VAR_BUTTON_B_PROPERTY,
			 pushbutton_b_count);
		strcat(state, buf_out);
		pushbutton_b_count_prev = pushbutton_b_count;
	}
	/* On receiving led state change notification from cloud, change
	 * the state of the led on the board in callback function and
	 * publish updated state on configured topic.
	 */
	if (led_1_state_prev != led_1_state) {
		snprintf(buf_out, BUFSIZE, ",\"%s\":%d", VAR_LED_1_PROPERTY,
			 led_1_state);
		strcat(state, buf_out);
		led_1_state_prev = led_1_state;
	}

	if (*ptr == ',')
		ptr++;
	if (strlen(state)) {
		snprintf(buf_out, BUFSIZE, "{\"state\": {\"reported\":{%s}}}",
			 ptr);
		wmprintf("Publishing '%s' to AWS\r\n", buf_out);

		/* publish incremented value on pushbutton press on
		 * configured thing */
		ret = aws_iot_shadow_update(&mqtt_client,
					    sp->pMyThingName,
					    buf_out,
					    shadow_update_status_cb,
					    NULL,
					    10, true);
	}
	return ret;
}

/* application thread */
static void aws_starter_demo(os_thread_arg_t data)
{
	int led_state = 0, ret;
	jsonStruct_t led_indicator;
	ShadowParameters_t sp;

	aws_iot_mqtt_init(&mqtt_client);

        // this just reads EPROM via wmsdk/../aws_utils
        // so if this fails this is fatal error
	ret = aws_starter_load_configuration(&sp);  
	if (ret != WM_SUCCESS) {
		wmprintf("aws shadow configuration failed : %d\r\n", ret);
		goto out;
	}

        // this as well initializes internal memory structures
        // only returns error if mqtt_client is NULL
	ret = aws_iot_shadow_init(&mqtt_client);
	if (ret != WM_SUCCESS) {
		wmprintf("aws shadow init failed : %d\r\n", ret);
		goto out;
	}

        // TODO: This is the offender - he actually performs 
        // connection and on flaky wireless connection this may result in 
        // failure...
	ret = aws_iot_shadow_connect(&mqtt_client, &sp);
	if (ret != WM_SUCCESS) {
		wmprintf("aws shadow connect failed : %d\r\n", ret);
		goto out;
	}

	/* indication that device is connected and cloud is started */
	led_on(board_led_2());
	wmprintf("Cloud Started\r\n");

	/* configures property of a thing */
	led_indicator.cb = led_indicator_cb;
	led_indicator.pData = &led_state;
	led_indicator.pKey = "led";
	led_indicator.type = SHADOW_JSON_INT8;

	/* subscribes to delta topic of the configured thing */
	ret = aws_iot_shadow_register_delta(&mqtt_client, &led_indicator);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to subscribe to shadow delta %d\r\n", ret);
		goto out;
	}

	/* creates a thread which will wait for incoming messages, ensuring the
	 * connection is kept alive with the AWS Service
	 */
	aws_create_shadow_yield_thread();

	while (1) {
		/* Implement application logic here */

		if (device_state == AWS_RECONNECTED) {
			ret = aws_iot_shadow_connect(&mqtt_client, &sp);
			if (ret != WM_SUCCESS) {
				wmprintf("aws shadow reconnect failed: "
					 "%d\r\n", ret);
				goto out;
			} else {
				device_state = AWS_CONNECTED;
				led_on(board_led_2());
			}
		}

		ret = aws_publish_property_state(&sp);
		if (ret != WM_SUCCESS)
			wmprintf("Sending property failed\r\n");

		os_thread_sleep(1000);
	}

	ret = aws_iot_shadow_disconnect(&mqtt_client);
	if (NONE_ERROR != ret) {
		wmprintf("aws iot shadow error %d\r\n", ret);
	}

out:
	os_thread_self_complete(NULL);
	return;
}

void wlan_event_normal_link_lost(void *data)
{
	/* led indication to indicate link loss */
        wmprintf("wlan_event_normal_link_lost()\r\n");
	aws_iot_shadow_disconnect(&mqtt_client);
	device_state = AWS_DISCONNECTED;
}

void wlan_event_normal_connect_failed(void *data)
{
	/* led indication to indicate connect failed */
        wmprintf("wlan_event_normal_connect_failed()\r\n");
	aws_iot_shadow_disconnect(&mqtt_client);
	device_state = AWS_DISCONNECTED;
}

/* This function gets invoked when station interface connects to home AP.
 * Network dependent services can be started here.
 * 
 * // this function will be called also on RECONNECTS!
 */
void wlan_event_normal_connected(void *data)
{
	int ret;
	/* Default time set to 1 October 2015 */
	time_t time = 1443657600;

	wmprintf("Connected successfully to the configured network\r\n");
        wmprintf("Will this be run on reconnect?\r\n");
        wmprintf("Also does this still run when there's no appropriate WLAN?\r\n");

	if (!device_state) { 
            // if first run (eg. device just woke up))
		/* set system time */
		wmtime_time_set_posix(time);

		/* create cloud thread */
		ret = os_thread_create(
			/* thread handle */
			&aws_starter_thread,
			/* thread name */
			"awsStarterDemo",
			/* entry function */
			aws_starter_demo,
			/* argument */
			0,
			/* stack */
			&aws_starter_stack,
			/* priority */
			OS_PRIO_3);
		if (ret != WM_SUCCESS) {
			wmprintf("Failed to start cloud_thread: %d\r\n", ret);
			return;
		}
	}

	if (!device_state)
		device_state = AWS_CONNECTED;
	else if (device_state == AWS_DISCONNECTED) // precondition can be that device was reconnected.
		device_state = AWS_RECONNECTED;
}

int main()
{
	/* initialize the standard input output facility over uart */
	if (wmstdio_init(UART0_ID, 0) != WM_SUCCESS) {
		return -WM_FAIL;
	}

	/* initialize gpio driver */
	if (gpio_drv_init() != WM_SUCCESS) {
		wmprintf("gpio_drv_init failed\r\n");
		return -WM_FAIL;
	}

	wmprintf("Build Time: " __DATE__ " " __TIME__ "\r\n");
	wmprintf("\r\n#### AWS STARTER DEMO ####\r\n\r\n");

	/* configure pushbutton on device to perform reset to factory */
	configure_reset_to_factory();
	/* configure led and pushbutton to communicate with cloud */
	configure_led_and_button();

	/* This api adds aws iot configuration support in web application.
	 * Configuration details are then stored in persistent memory.
	 */
	enable_aws_config_support();

	/* This api starts micro-AP if device is not configured, else connects
	 * to configured network stored in persistent memory. Function
	 * wlan_event_normal_connected() is invoked on successful connection.
	 */
        wmprintf("Starting microAP with network %s and password [%s]", MICRO_AP_SSID, MICRO_AP_PASSPHRASE);
	wm_wlan_start(MICRO_AP_SSID, MICRO_AP_PASSPHRASE);
        
        // !!!the new thread would be started from the "wlan_event_connection_success()"
        wmprintf("wlan_start() is non blocking");
	return 0;
}
