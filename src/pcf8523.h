#ifndef PCF8523_H
#define PCF8523_H

#include <zephyr/device.h>
#include <time.h>

// I2C device slave address
#define PCF8523_SLAVE_ADD           0x68

// BCD related
#define PCF8523_BCD_UPPER_SHIFT     4
#define PCF8523_BCD_LOWER_MASK      0x0F
#define PCF8523_BCD_UPPER_MASK      0xF0
#define PCF8523_BCD_UPPER_MASK_SEC  0x70

// Control_1; Control and status Register Masks
#define PCF8523_CONTROL_1_ADD       0x00
#define PCF8523_CTRL1_CIE_MASK      BIT(0) // interrupt pulses are generated at every correction cycle
#define PCF8523_CTRL1_AIE_MASK      BIT(1) // alarm interrupt enabled
#define PCF8523_CTRL1_SIE_MASK      BIT(2) // second interrupt enabled
#define PCF8523_CTRL1_12_24_MASK    BIT(3) // 12 hour mode is selected
#define PCF8523_CTRL1_SR_MASK       BIT(4) // initiate software reset
#define PCF8523_CTRL1_STOP_MASK     BIT(5) //  RTC time circuits frozen;
#define PCF8523_CTRL1_T_MASK        BIT(6) // unused
#define PCF8523_CTRL1_CAP_SEL_MASK  BIT(7) //internal oscillator capacitor selection for quartz crystals with a corresponding load capacitance. Selects 12.5pF

// Control_2; Control and status Register Masks
#define PCF8523_CONTROL_2_ADD       0x01
#define PCF8523_CTRL2_CTBIE_MASK    BIT(0) // enable countdown timer B interrupt
#define PCF8523_CTRL2_CTAIE_MASK    BIT(1) // enable countdown timer A interrupt
#define PCF8523_CTRL2_WTAIE_MASK    BIT(2) // enable watchdog timer A interrupt i
#define PCF8523_CTRL2_AF_MASK       BIT(3) // flag set when alarm triggered; flag must be cleared to clear interrupt
#define PCF8523_CTRL2_SF_MASK       BIT(4) // flag set when second interrupt generated; flag must be cleared to clear interrupt
#define PCF8523_CTRL2_CTBF_MASK     BIT(5) // flag set when countdown timer B interrupt generated; flag must be cleared to clear interrupt
#define PCF8523_CTRL2_CTAF_MASK     BIT(6) // flag set when countdown timer A interrupt generated; flag must be cleared to clear interrupt
#define PCF8523_CTRL2_WTAF_MASK     BIT(7) // flag set when watchdog timer A interrupt generated; flag is read-only and cleared by reading register Control_2

// Control_3; Control and status Register Masks
#define PCF8523_CONTROL_3_ADD       0x02
#define PCF8523_CTRL3_BLIE_MASK     BIT(0) // enable interrupt generation when BLF gets set
#define PCF8523_CTRL3_BSIE_MASK     BIT(1) //enable interrupt generated when BSF gets set
#define PCF8523_CTRL3_BLF_MASK      BIT(2) //battery status low; flag is read-only
#define PCF8523_CTRL3_BSF_MASK      BIT(3) //flag set when battery switch-over occurs
/* Power Management configuration modes*/
#define PCF8523_CTRL3_SO_MODE_0_MASK    0x00  /* battery switch-over is enabled in standard mode; battery low detection function is enabled*/
#define PCF8523_CTRL3_SO_MODE_1_MASK    0x20  /* battery switch-over function is enabled in direct switching mode; battery low detection function is enabled*/
#define PCF8523_CTRL3_SO_MODE_2_MASK    0x40  /* battery switch-over function is disabled - only one power supply (VDD); battery low detection function is enabled*/
#define PCF8523_CTRL3_SO_MODE_3_MASK    0x40  /* battery switch-over function is disabled - only one power supply (VDD); battery low detection function is enabled*/
#define PCF8523_CTRL3_SO_MODE_4_MASK    0x80  /* battery switch-over function is enabled in standard mode;battery low detection function is disabled*/
#define PCF8523_CTRL3_SO_MODE_5_MASK    0xA0  /* battery switch-over function is enabled in direct switching mode; battery low detection function is disabled*/
#define PCF8523_CTRL3_SO_MODE_6_MASK    0xE0  /* battery switch-over function is disabled - only one power supply (VDD);battery low detection function is disabled*/
                                                

// Time and date related
#define PCF8523_SECONDS_ADD         0x03
#define PCF8523_SECONDS_OS          BIT(7)
#define PCF8523_SECONDS_MASK        0x7F

#define PCF8523_MINUTES_ADD         0x04
#define PCF8523_MINUTES_MASK        0x7F

#define PCF8523_HOURS_ADD           0x05
#define PCF8523_HOURS_AM_PM         BIT(5)
#define PCF8523_HOURS_MASK          0x3F

#define PCF8523_DAYS_ADD            0x06
#define PCF8523_DAYS_MASK           0x3F

#define PCF8523_WEEKDAYS_ADD        0x07
#define PCF8523_WEEKDAYS_MASK       0x07
#define PCF8523_WEEKDAYS_SUN        0x00
#define PCF8523_WEEKDAYS_MON        0x01
#define PCF8523_WEEKDAYS_TUE        0x02
#define PCF8523_WEEKDAYS_WED        0x03
#define PCF8523_WEEKDAYS_THR        0x04
#define PCF8523_WEEKDAYS_FRI        0x05
#define PCF8523_WEEKDAYS_SAT        0x06

#define PCF8523_MONTHS_ADD          0x08
#define PCF8523_MONTHS_MASK         0x1F
#define PCF8523_MONTHS_JAN          0x01
#define PCF8523_MONTHS_FEB          0x02
#define PCF8523_MONTHS_MAR          0x03
#define PCF8523_MONTHS_APR          0x04
#define PCF8523_MONTHS_MAY          0x05
#define PCF8523_MONTHS_JUN          0x06
#define PCF8523_MONTHS_JUL          0x07
#define PCF8523_MONTHS_AUG          0x08
#define PCF8523_MONTHS_SEP          0x09
#define PCF8523_MONTHS_OCT          0x10
#define PCF8523_MONTHS_NOV          0x11
#define PCF8523_MONTHS_DEC          0x12

#define PCF8523_YEARS_ADD           0x09


uint32_t pcf8523_init(const struct device *dev);
uint32_t pcf8523_switchover_occurred(const struct device *dev);
uint32_t pcf8523_set_time(const struct device *dev, int64_t *ts);
uint32_t pcf8523_get_time(const struct device *dev, int64_t *ts);
uint32_t pcf8523_get_time_tm(const struct device *dev, struct tm *time);
void convert_time_ascii(struct tm *time, char *tstr, char *dstr);


#endif /* DUT_SPI HEADER*/