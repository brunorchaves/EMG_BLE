/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <stdio.h>
 #include <zephyr/kernel.h>
 #include <zephyr/drivers/gpio.h>
 #include <zephyr/drivers/uart.h>
 #include <zephyr/sys/ring_buffer.h>
 
 /* 1000 msec = 1 sec */
 #define SLEEP_TIME_MS   1000
 
 /* The devicetree node identifier for the "led0" alias. */
 #define LED0_NODE DT_ALIAS(led0)
 
 /*
  * A build error on this line means your board is unsupported.
  * See the sample documentation for information on how to fix this.
  */
 static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
 
 /* Get UART device from device tree */
 static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
 
 int main(void)
 {
	 int ret;
	 bool led_state = true;
 
	 /* Check if LED is ready */
	 if (!gpio_is_ready_dt(&led)) {
		 printk("Error: LED device not ready\n");
		 return 0;
	 }
 
	 /* Configure LED */
	 ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	 if (ret < 0) {
		 printk("Error %d: failed to configure LED\n", ret);
		 return 0;
	 }
 
	 /* Check if UART device is ready */
	 if (!device_is_ready(uart_dev)) {
		 printk("Error: UART device not ready\n");
		 return 0;
	 }
 
	 /* Print initial message */
	 printk("Hello World!\n");
	 printf("System started. LED will toggle every %d ms.\n", SLEEP_TIME_MS);
 
	 while (1) {
		 /* Toggle LED */
		 ret = gpio_pin_toggle_dt(&led);
		 if (ret < 0) {
			 printk("Error %d: failed to toggle LED\n", ret);
			 return 0;
		 }
 
		 led_state = !led_state;
		 printf("LED state: %s\n", led_state ? "ON" : "OFF");
		 
		 /* Sleep for defined time */
		 k_msleep(SLEEP_TIME_MS);
	 }
	 return 0;
 }