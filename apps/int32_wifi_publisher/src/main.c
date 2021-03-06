#include <zephyr.h>
#include <device.h>
#include <stdio.h>
#include <stdint.h>

#include <net/net_if.h>
#include <net/wifi_mgmt.h>
#include <net/net_event.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/int32.h>

#include <rclc/rclc.h>
#include <rclc/executor.h>

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printk("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printk("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

// Wireless management
static struct net_mgmt_event_callback wifi_shell_mgmt_cb;
static bool connected = 0;

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{	
	if(NET_EVENT_IPV4_ADDR_ADD == mgmt_event){
			printf("DHCP Connected\n");
			connected = 1;
	}
}

// micro-ROS
rcl_publisher_t publisher;
std_msgs__msg__Int32 msg;

void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
  UNUSED(last_call_time);
  if (timer != NULL) {
    RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
	msg.data++;
  }
}

void main(void)
{	
	// ------ Wifi Configuration ------
	net_mgmt_init_event_callback(&wifi_shell_mgmt_cb,
					wifi_mgmt_event_handler,
					NET_EVENT_IPV4_ADDR_ADD);

	net_mgmt_add_event_callback(&wifi_shell_mgmt_cb);

	struct net_if *iface = net_if_get_default();
	static struct wifi_connect_req_params cnx_params;


	cnx_params.ssid = "WIFI_SSID_HERE";
	cnx_params.ssid_length = strlen(cnx_params.ssid);
	cnx_params.channel = 0;
	cnx_params.psk = "WIFI_PSK_HERE";
	cnx_params.psk_length = strlen(cnx_params.psk);
	cnx_params.security = WIFI_SECURITY_TYPE_PSK;

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(struct wifi_connect_req_params))) {
		printf("Connection request failed\n");
	} else {
		printf("Connection requested\n");
	}

	while (!connected)
	{
		printf("Waiting for connection\n");
		usleep(10000);
	}
	printf("Connection OK\n");
	
	// ------ micro-ROS ------
	rcl_allocator_t allocator = rcl_get_default_allocator();
	rclc_support_t support;

	// create init_options
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

	// create node
	rcl_node_t node = rcl_get_zero_initialized_node();
	RCCHECK(rclc_node_init_default(&node, "zephyr_int32_publisher", "", &support));

	// create publisher
	RCCHECK(rclc_publisher_init_default(
		&publisher,
		&node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
		"zephyr_int32_publisher"));

	// create timer,
	rcl_timer_t timer = rcl_get_zero_initialized_timer();
	const unsigned int timer_timeout = 1000;
	RCCHECK(rclc_timer_init_default(
		&timer,
		&support,
		RCL_MS_TO_NS(timer_timeout),
		timer_callback));

	// create executor
	rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
	RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));

	unsigned int rcl_wait_timeout = 1000;   // in ms
	RCCHECK(rclc_executor_set_timeout(&executor, RCL_MS_TO_NS(rcl_wait_timeout)));
	RCCHECK(rclc_executor_add_timer(&executor, &timer));

	msg.data = 0;
	
	while(1){
    	rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
		usleep(10000);
	}

	RCCHECK(rcl_publisher_fini(&publisher, &node))
	RCCHECK(rcl_node_fini(&node))
}



