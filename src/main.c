/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Sample echo app for CDC ACM class
 *
 * Sample app for USB CDC ACM class driver. The received data is echoed back
 * to the serial port.
 */


#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>

#include <zephyr/drivers/i2c.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cdc_acm_echo, LOG_LEVEL_INF);

#include "analog.h"
#include "pcf8523.h"

/* ADC RELATED */
#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA)
};


/* UART RING BUFFER DEFINES */
#define RING_BUF_SIZE 1024
uint8_t ring_buffer[RING_BUF_SIZE];
struct ring_buf usb_tx_ringbuf;
struct ring_buf usb_rx_ringbuf;
struct ring_buf uart1_tx_ringbuf;
struct ring_buf uart1_rx_ringbuf;


/* Get all the pins specs from the board .dts file */
const struct gpio_dt_spec LED1 = GPIO_DT_SPEC_GET(DT_NODELABEL(led1), gpios);
const struct gpio_dt_spec LED2 = GPIO_DT_SPEC_GET(DT_NODELABEL(led2), gpios);
// const struct gpio_dt_spec SCL_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b1_1_pin), gpios);
// const struct gpio_dt_spec SDA_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b2_1_pin), gpios);
const struct gpio_dt_spec COMM_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b3_1_pin), gpios);
const struct gpio_dt_spec STS_LED_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b4_1_pin), gpios);
const struct gpio_dt_spec SPARE_0_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b1_2_pin), gpios);
const struct gpio_dt_spec SPARE_1_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b2_2_pin), gpios);
const struct gpio_dt_spec SPARE_3_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b3_2_pin), gpios);
const struct gpio_dt_spec DIR_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b4_2_pin), gpios);
const struct gpio_dt_spec PLS_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b1_3_pin), gpios);
const struct gpio_dt_spec TAMPER_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b2_3_pin), gpios);
const struct gpio_dt_spec SPARE_2_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b3_3_pin), gpios);
const struct gpio_dt_spec SPARE_4_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b4_3_pin), gpios);
// const struct gpio_dt_spec TX_CON_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b1_4_pin), gpios);
// const struct gpio_dt_spec RX_CON_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b2_4_pin), gpios);
const struct gpio_dt_spec EXTRA_1_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b3_4_pin), gpios);
const struct gpio_dt_spec EXTRA_2_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(b4_4_pin), gpios);

const struct gpio_dt_spec AP22_EN_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(ap22_en_pin), gpios);
const struct gpio_dt_spec AP22_FLG_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(ap22_flg_pin), gpios);
const struct gpio_dt_spec SHUNT_BYPASS_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(shunt_bypass_pin), gpios);
const struct gpio_dt_spec SHUNT_EN_PIN = GPIO_DT_SPEC_GET(DT_NODELABEL(shunt_en_pin), gpios);
const struct gpio_dt_spec LS_OE = GPIO_DT_SPEC_GET(DT_NODELABEL(level_shift_oe), gpios);


const struct device *dev_UART1 = DEVICE_DT_GET(DT_NODELABEL(uart1));
const struct device *dev_GPIO0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
const struct device *dev_GPIO1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
const struct device *dev_USB   = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
const struct device *dev_I2C0 = DEVICE_DT_GET(DT_NODELABEL(i2c0));

/* timeout in milliseconds for led blinking */
#define LED_BLINK_TIME_OUT_MS	100	/* milliseconds */
#define LED_BLINK_TO			LED_BLINK_TIME_OUT_MS * (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC)/1000
/* timeout in milliseconds for adc sampling */
#define ADC_TIME_OUT_MS			1000	/* milliseconds */
#define ADC_TO					ADC_TIME_OUT_MS * (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC)/1000

enum states{
			st_output_voltage,
			st_read_rtc,
			st_test_rs232,
			st_results
};


void configure_and_set_io_pins();

static void usb_uart_interrupt_handler(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {

		/* Interrupt triggered by RX pin */
		if (uart_irq_rx_ready(dev)) {
			int recv_len, rb_len;
			uint8_t buffer[64];
			size_t len = MIN(ring_buf_space_get(&usb_rx_ringbuf),
					 sizeof(buffer));

			recv_len = uart_fifo_read(dev, buffer, len);
			if (recv_len < 0) {
				LOG_ERR("Failed to read UART FIFO");
				recv_len = 0;
			};

			rb_len = ring_buf_put(&usb_rx_ringbuf, buffer, recv_len);
			if (rb_len < recv_len) {
				LOG_ERR("Drop %u bytes", recv_len - rb_len);
			}
		}

		/* Interrupt triggered by TX pin */
		if (uart_irq_tx_ready(dev)) {
			uint8_t buffer[64];
			int rb_len, send_len;

			rb_len = ring_buf_get(&usb_tx_ringbuf, buffer, sizeof(buffer));
			if (!rb_len) {
				LOG_DBG("Ring buffer empty, disable TX IRQ");
				uart_irq_tx_disable(dev);
				continue;
			}

			send_len = uart_fifo_fill(dev, buffer, rb_len);
			if (send_len < rb_len) {
				LOG_ERR("Drop %d bytes", rb_len - send_len);
			}

			LOG_DBG("usb_tx_ringbuf -> tty fifo %d bytes", send_len);
		}
	}
}


static void uart1_interrupt_handler(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {

		/* Interrupt triggered by RX pin */
		if (uart_irq_rx_ready(dev)) {
			int recv_len, rb_len;
			uint8_t buffer[64];
			size_t len = MIN(ring_buf_space_get(&uart1_rx_ringbuf),
					 sizeof(buffer));

			recv_len = uart_fifo_read(dev, buffer, len);
			if (recv_len < 0) {
				LOG_ERR("Failed to read UART FIFO");
				recv_len = 0;
			};

			rb_len = ring_buf_put(&uart1_rx_ringbuf, buffer, recv_len);
			if (rb_len < recv_len) {
				LOG_ERR("Drop %u bytes", recv_len - rb_len);
			}
		}

		/* Interrupt triggered by TX pin */
		if (uart_irq_tx_ready(dev)) {
			uint8_t buffer[64];
			int rb_len, send_len;

			rb_len = ring_buf_get(&uart1_tx_ringbuf, buffer, sizeof(buffer));
			if (!rb_len) {
				LOG_DBG("Ring buffer empty, disable TX IRQ");
				uart_irq_tx_disable(dev);
				continue;
			}

			send_len = uart_fifo_fill(dev, buffer, rb_len);
			if (send_len < rb_len) {
				LOG_ERR("Drop %d bytes", rb_len - send_len);
			}

			LOG_DBG("usb_tx_ringbuf -> tty fifo %d bytes", send_len);
		}
	}
}

void setup_usb(const struct device *dev)
{
	int ret;
	uint32_t baudrate = 0U;
	
	
	ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &baudrate);
	if (ret) {
		LOG_WRN("Failed to get baudrate, ret code %d", ret);
	} else {
		LOG_INF("Baudrate detected: %d", baudrate);
	}
}


/* MAIN ENTRY POINT */
void main(void)
{
	//uint32_t baudrate, dtr = 0U;
	int ret;
	int32_t mv_Val=0;

	uint8_t str[100];
	int64_t ts=0;
	struct tm time = {0};
	char time_str[10];
	char date_str[10];
	int time_1 = 0;
	int time_2 = 0;
	uint32_t recv_len;
	uint8_t buffer[64];
	uint8_t *ptr;	

	/* Verify that devices are ready to use */
	if(!device_is_ready(dev_UART1)){ LOG_ERR("UART1 device not ready"); return;}
	if(!device_is_ready(dev_GPIO0)){ LOG_ERR("GPIO0 device not ready"); return;}
	if(!device_is_ready(dev_GPIO1)){ LOG_ERR("GPIO1 device not ready"); return;}
	if(!device_is_ready(dev_USB))  { LOG_ERR("CDC ACM device not ready"); return;}
	/* Verify USB is enabled */
	ret = usb_enable(NULL);
	if (ret != 0){ LOG_ERR("Failed to enable USB"); return; }

	/* Configure direction of IO pins and set the value according to application */
	configure_and_set_io_pins();
	
	/* Configure ADC channels individually prior to sampling. */
	for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		adc_init_chn(&adc_channels[i]);
	}

	/* Do USB-specific work not related to UART, waits for the host device to connect */
	setup_usb(dev_USB);

	// Configure/Initialize i2c controller and RTC module
	pcf8523_init(dev_I2C0);

	/* Initialize data ring buffers used by the uart module */
	ring_buf_init(&usb_rx_ringbuf, sizeof(ring_buffer), ring_buffer);
	ring_buf_init(&usb_tx_ringbuf, sizeof(ring_buffer), ring_buffer);
	ring_buf_init(&uart1_rx_ringbuf, sizeof(ring_buffer), ring_buffer);
	ring_buf_init(&uart1_tx_ringbuf, sizeof(ring_buffer), ring_buffer);

	/* Attach interrupt handler to USB UART port */
	uart_irq_callback_set(dev_USB, usb_uart_interrupt_handler);

	/* Enable usb uart rx interrupts */
	uart_irq_rx_enable(dev_USB);

	/* Attach interrupt handler to UART1 port */
	uart_irq_callback_set(dev_UART1, uart1_interrupt_handler);

	/* Enable uart1 rx interrupts */
	uart_irq_rx_enable(dev_UART1);

	// Wait for 2 seconds
	k_msleep(2000);

	int index=0;
	// Check switchover flag status
	if(pcf8523_switchover_occurred(dev_I2C0)){
		// Switch-over occured
		/* Send value through uart for debug purposes */
		sprintf(str, "\nSwitch-over flag set.\n");
		index =  strlen(str);

		pcf8523_get_time_tm(dev_I2C0, &time);
		time_1 = time.tm_sec;
		convert_time_ascii(&time, time_str, date_str);

		sprintf(&str[index], "\nStart time: %s", time_str);
		ring_buf_put(&usb_tx_ringbuf, str, strlen(str));
		uart_irq_tx_enable(dev_USB);

	}else{
		// No switch-over occured
		sprintf(str, "\nSwitch-over flag clear.\n");
		index =  strlen(str);

		// Set time and date to 1664506805: 03:00:05 30/09/2022
		ts = 1664506805;
		pcf8523_set_time(dev_I2C0, &ts);

		pcf8523_get_time_tm(dev_I2C0, &time);
		time_1 = time.tm_sec;
		convert_time_ascii(&time, time_str, date_str);

		sprintf(&str[index], "\nStart time: %s \n", time_str);
		ring_buf_put(&usb_tx_ringbuf, str, strlen(str));
		uart_irq_tx_enable(dev_USB);
	}

	// Wait for 2 seconds
	k_msleep(2000);

	pcf8523_get_time_tm(dev_I2C0, &time);
	time_2 = time.tm_sec;
	convert_time_ascii(&time, time_str, date_str);
	sprintf(str, "\nTime after 2 seconds: %s\n", time_str);
	ring_buf_put(&usb_tx_ringbuf, str, strlen(str));
	uart_irq_tx_enable(dev_USB);

	if(time_1 > 57)
		time_2 += 60;
	if((time_2-time_1) == 2){
		ring_buf_put(&usb_tx_ringbuf, "\nPASSED\n", 7);
		uart_irq_tx_enable(dev_USB);
	}else{
		ring_buf_put(&usb_tx_ringbuf, "\nFAILED\n", 7);
		uart_irq_tx_enable(dev_USB);
	}

	uint8_t adc_str[60];
	/* Read voltage value and make sure the voltage is 3.45V at the output of buck converter */
	while((mv_Val < LOWER_3v45) || (mv_Val > UPPER_3v45)){
		mv_Val = adc_read_ps_3v6(&adc_channels[ADC_3v45_CHN]);
		gpio_pin_toggle(LED1.port, LED1.pin);
		gpio_pin_toggle(LED2.port, LED2.pin);
		k_msleep(350);	
	}

	
	/* Send value through uart for debug purposes */
	sprintf(adc_str, " - Output Voltage: %d mV\n\n\n", mv_Val);
	ring_buf_put(&usb_tx_ringbuf, adc_str, strlen(adc_str));
	uart_irq_tx_enable(dev_USB);	
	gpio_pin_set_raw(LED1.port, LED1.pin, 0);
	gpio_pin_set_raw(LED2.port, LED2.pin, 0);


	/* Test RS-232 Comms */
	while(1){
		sprintf(&str, "UART1 Test:\n");
		ring_buf_put(&uart1_tx_ringbuf, str, strlen(str));
		uart_irq_tx_enable(dev_UART1);

		k_msleep(500);// Wait for 2 seconds

		recv_len = ring_buf_get(&uart1_rx_ringbuf, buffer, sizeof(buffer));
		if(recv_len) {
			ptr = strchr(buffer, '\n');
			if(ptr != NULL){
				// Line feed character received, process command
				/* Send value through uart for debug purposes */
				ring_buf_put(&usb_tx_ringbuf, buffer, 12);
				uart_irq_tx_enable(dev_USB);
			}
		}
		
		k_msleep(2000);// Wait for 2 seconds
	};
		

	uint32_t startTime = k_cycle_get_32();
	uint32_t startTime2 = k_cycle_get_32();

	/* Infinite Loop */
	while(1){


		/* Toggle leds every x time */
		if((k_cycle_get_32() - startTime) > LED_BLINK_TO){
			gpio_pin_toggle(LED1.port, LED1.pin);
			gpio_pin_toggle(LED2.port, LED2.pin);
			startTime = k_cycle_get_32();
		}
		
	}
}



void configure_and_set_io_pins(){
	/* Configure the direction of the board IO pins */
	gpio_pin_configure_dt(&LED1, GPIO_OUTPUT);
	gpio_pin_configure_dt(&LED2, GPIO_OUTPUT);
	//gpio_pin_configure_dt(&SCL_PIN, GPIO_OUTPUT);
	//gpio_pin_configure_dt(&SDA_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&COMM_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&STS_LED_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&SPARE_0_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&SPARE_1_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&SPARE_3_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&DIR_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&PLS_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&TAMPER_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&SPARE_2_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&SPARE_4_PIN, GPIO_OUTPUT);
	// gpio_pin_configure_dt(&TX_CON_PIN, GPIO_OUTPUT);	// Used for uart1 rx
	// gpio_pin_configure_dt(&RX_CON_PIN, GPIO_OUTPUT);	// Used for uart1 tx
	gpio_pin_configure_dt(&EXTRA_1_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&EXTRA_2_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&AP22_EN_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&AP22_FLG_PIN, GPIO_INPUT);
	gpio_pin_configure_dt(&SHUNT_BYPASS_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&SHUNT_EN_PIN, GPIO_OUTPUT);
	gpio_pin_configure_dt(&LS_OE, GPIO_OUTPUT);

	/* Set the default value of IO Pins */
	gpio_pin_set_raw(LED1.port, LED1.pin, 0);
	gpio_pin_set_raw(LED2.port, LED2.pin, 0);
	gpio_pin_set_raw(LS_OE.port, LS_OE.pin, 1); 					/* Enable level shifters */
	// gpio_pin_set_raw(SCL_PIN.port, SCL_PIN.pin, 0);
	// gpio_pin_set_raw(SDA_PIN.port, SDA_PIN.pin, 0);
	gpio_pin_set_raw(COMM_PIN.port, COMM_PIN.pin, 0);
	gpio_pin_set_raw(STS_LED_PIN.port, STS_LED_PIN.pin, 0);
	gpio_pin_set_raw(SPARE_0_PIN.port, SPARE_0_PIN.pin, 0);
	gpio_pin_set_raw(SPARE_1_PIN.port, SPARE_1_PIN.pin, 0);
	gpio_pin_set_raw(SPARE_3_PIN.port, SPARE_3_PIN.pin, 0);
	gpio_pin_set_raw(DIR_PIN.port, DIR_PIN.pin, 0);
	gpio_pin_set_raw(PLS_PIN.port, PLS_PIN.pin, 0);
	gpio_pin_set_raw(TAMPER_PIN.port, TAMPER_PIN.pin,  0);
	gpio_pin_set_raw(SPARE_2_PIN.port, SPARE_2_PIN.pin, 0);
	gpio_pin_set_raw(SPARE_4_PIN.port, SPARE_4_PIN.pin, 0);
	// gpio_pin_set_raw(TX_CON_PIN.port, TX_CON_PIN.pin, 0);	// Used for uart1 rx
	// gpio_pin_set_raw(RX_CON_PIN.port, RX_CON_PIN.pin, 0);	// Used for uart1 tx
	gpio_pin_set_raw(EXTRA_1_PIN.port, EXTRA_1_PIN.pin, 0);
	gpio_pin_set_raw(EXTRA_2_PIN.port, EXTRA_2_PIN.pin, 0);
	gpio_pin_set_raw(AP22_EN_PIN.port, AP22_EN_PIN.pin, 0);				/* Disable AP22 Power switch */
	gpio_pin_set_raw(SHUNT_BYPASS_PIN.port, SHUNT_BYPASS_PIN.pin, 0);	/* Disable Shunt bypass path */
	gpio_pin_set_raw(SHUNT_EN_PIN.port, SHUNT_EN_PIN.pin, 0);			/* Disable Shunt Resistor path */
}
