#ifndef ANALOG_H
#define	ANALOG_H


#include <zephyr/drivers/adc.h>

/* MAXIMUM ACCEPTABLE SLEEP CURRENT FOR DUT */
#define DUT_MAX_SLEEP_CURRENT_UA    190

/* Upper and lower threshold for buck output voltage */
#define UPPER_3v45   3500
#define LOWER_3v45   3400

/* Upper and lower threshold for dut output voltage */
#define UPPER_1v8   1900
#define LOWER_1v8   1700

/* ADC channel array indexing - Check devicetree to get the order */
#define ADC_CURRENT_CHN     0
#define ADC_3v6_CHN         1
#define ADC_3v45_CHN         2

int32_t adc_init_chn(const struct adc_dt_spec *adc_spec);
int32_t adc_read_chn_raw(const struct adc_dt_spec *adc_spec);
int32_t adc_read_chn_mV(const struct adc_dt_spec *adc_spec);
int32_t adc_read_dut_1v8(const struct adc_dt_spec *adc_spec);
int32_t adc_read_ps_3v6(const struct adc_dt_spec *adc_spec);
int32_t adc_read_dut_current_uA(const struct adc_dt_spec *adc_spec);

#endif /* DUT_UART HEADER*/