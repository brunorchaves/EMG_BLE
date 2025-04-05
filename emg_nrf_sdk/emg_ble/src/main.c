/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <stdio.h>
 #include <zephyr/kernel.h>
 #include <zephyr/drivers/gpio.h>
 #include <zephyr/drivers/i2c.h>
 
 /* The devicetree node identifier for the "led0" alias. */
 #define LED0_NODE DT_ALIAS(led0)
 
 static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
 
 /* Xiao nRF52840 pins - D7 is P1.12, D8 is P1.13 */
 #define XIAO_D7_PIN  12  // P1.12
 #define XIAO_D8_PIN  13  // P1.13
 static const struct device *gpio_dev_p1;  // GPIO Port 1
 
 int main(void)
 {
	 int ret;
	 bool led_state = true;
	 uint64_t last_toggle_time = k_uptime_get();
 
	 if (!gpio_is_ready_dt(&led)) {
		 printf("Main LED device not ready!\n");
		 return 0;
	 }
 
	 ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	 if (ret < 0) {
		 printf("Failed to configure main LED\n");
		 return 0;
	 }
 
	 /* Get the GPIO Port 1 device */
	 gpio_dev_p1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
	 if (!device_is_ready(gpio_dev_p1)) {
		 printf("GPIO Port 1 device not ready!\n");
		 return 0;
	 }
 
	 /* Configure D7 (P1.12) and D8 (P1.13) as outputs */
	 ret = gpio_pin_configure(gpio_dev_p1, XIAO_D7_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_HIGH);
	 if (ret < 0) {
		 printf("Failed to configure D7 (P1.12)\n");
	 }
 
	 ret = gpio_pin_configure(gpio_dev_p1, XIAO_D8_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_HIGH);
	 if (ret < 0) {
		 printf("Failed to configure D8 (P1.13)\n");
	 }
 
	 /* Set initial state */
	 gpio_pin_set(gpio_dev_p1, XIAO_D7_PIN, 0);
	 gpio_pin_set(gpio_dev_p1, XIAO_D8_PIN, 0);
 
	 while (1) {
		 uint64_t current_time = k_uptime_get();
 
		 /* Toggle LEDs every 1000ms (1 second) */
		 if (current_time - last_toggle_time >= 1000) {
			 ret = gpio_pin_toggle_dt(&led);
			 if (ret < 0) {
				 printf("Failed to toggle main LED\n");
			 }
 
			 ret = gpio_pin_toggle(gpio_dev_p1, XIAO_D7_PIN);
			 if (ret < 0) {
				 printf("Failed to toggle D7 (P1.12)\n");
			 }
 
			 ret = gpio_pin_toggle(gpio_dev_p1, XIAO_D8_PIN);
			 if (ret < 0) {
				 printf("Failed to toggle D8 (P1.13)\n");
			 }
 
			 led_state = !led_state;
			 printf("LED state: %s\n", led_state ? "ON" : "OFF");
			 last_toggle_time = current_time;
		 }
 
		 /* Keep CPU busy (100% usage) */
		 __asm__ volatile("nop");  // Prevent compiler optimization
	 }
	 return 0;
 }