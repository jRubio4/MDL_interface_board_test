
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include "analog.h"

#define BUFFER_SIZE 1
static int16_t m_sample_buffer[BUFFER_SIZE];

int32_t adc_init_chn(const struct adc_dt_spec *adc_spec){

    int err;
    
    if (!device_is_ready(adc_spec->dev)) {
        printk("ADC controller device not ready\n");
        return 1;
    }

    err = adc_channel_setup_dt(adc_spec);
    if (err < 0) {
        printk("Could not setup channel: err (%d)\n", err);
        return 2;
    }

    return 0;
}

int32_t adc_read_chn_raw(const struct adc_dt_spec *adc_spec){
    int err;

	struct adc_sequence sequence = {
		.buffer = m_sample_buffer,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(m_sample_buffer),
	};

    (void)adc_sequence_init_dt(adc_spec, &sequence);

    err = adc_read(adc_spec->dev, &sequence);
    if (err < 0) {
        printk("Could not read (%d)\n", err);
        return -1;

    } else {

        return (int32_t)m_sample_buffer[0];
    }
}


int32_t adc_read_chn_mV(const struct adc_dt_spec *adc_spec){
    int32_t val_mv;
    int err;

	struct adc_sequence sequence = {
		.buffer = m_sample_buffer,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(m_sample_buffer),
	};

    (void)adc_sequence_init_dt(adc_spec, &sequence);

    err = adc_read(adc_spec->dev, &sequence);
    if (err < 0) {
        printk("Could not read (%d)\n", err);
        return -1;

    } else {

        /* conversion to mV */
        val_mv = m_sample_buffer[0];
        err = adc_raw_to_millivolts_dt(adc_spec, &val_mv);
        if (err < 0) {
            printk(" (value in mV not available)\n");
            return -1;
        } else {
            /* return mv value*/
            return val_mv;
        }
    }

    
}

int32_t adc_read_dut_1v8(const struct adc_dt_spec *adc_spec){

    int32_t val_mv;
    
    val_mv = adc_read_chn_mV(adc_spec);

    return val_mv*2;

}

int32_t adc_read_ps_3v6(const struct adc_dt_spec *adc_spec){
    int32_t val_mv;
    
    val_mv = adc_read_chn_mV(adc_spec);

    return val_mv*2;
}


int32_t adc_read_dut_current_uA(const struct adc_dt_spec *adc_spec){
    int32_t current_uA;
    int32_t offset = 95;

    current_uA = adc_read_chn_raw(adc_spec);
    current_uA = ((current_uA*879)/50/30) + offset;

    return current_uA;
}
