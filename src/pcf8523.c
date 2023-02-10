#include "pcf8523.h"


#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/timeutil.h>
#include <string.h>

/*!
* @brief Initialize/Configure i2C controller and RTC module
*
* @param dev Pointer to the device structure for an I2C controller
* driver configured in master mode.
*
* @return 0 if succesful
* @return 1 if i2c controller is not ready
*
*/
uint32_t pcf8523_init(const struct device *dev){
	uint8_t i2c_buff[7] = {0};

	// Configure I2C peripheral
	i2c_configure(dev, I2C_SPEED_SET(I2C_SPEED_STANDARD));

	if(!device_is_ready(dev)){ 
		printk("Coud not get i2c device binding\n");
		return 1;
	}

	// Set pointer to the Control_3 register and write the switchover functionality. leave the rest to default values
	i2c_buff[0] = PCF8523_CONTROL_3_ADD; // Control_3 Register (Address 02h)
	if(pcf8523_switchover_occurred(dev))
		// if switchover ocurred do not clear the flag yet
		i2c_buff[1] = PCF8523_CTRL3_SO_MODE_4_MASK|PCF8523_CTRL3_BSF_MASK; // battery switch-over function is enabled in standard mode;battery low detection function is disabled*/
	else
		i2c_buff[1] = PCF8523_CTRL3_SO_MODE_4_MASK; // battery switch-over function is enabled in standard mode;battery low detection function is disabled*/
	i2c_write(dev, i2c_buff, 2, PCF8523_SLAVE_ADD);
	return 0;
}

/*!
* @brief Checks if the switch-over flag is set and returns its state
*
* @param dev Pointer to the device structure for an I2C controller
* driver configured in master mode.
*
* @return 0 if no switch-over event occured
* @return 1 if a sitch-over event occurred
*
*/
uint32_t pcf8523_switchover_occurred(const struct device *dev){
	
	uint8_t i2c_buff[7] = {0};
	uint8_t switch_over = 0;

	i2c_buff[0] = PCF8523_CONTROL_3_ADD; // Register Control_3 (Address 02h)
	i2c_write_read(dev, PCF8523_SLAVE_ADD, i2c_buff, 1, &i2c_buff[1], 1);

	switch_over = i2c_buff[1] & PCF8523_CTRL3_BSF_MASK; // Get BSF flag state

	if(switch_over)
		return true;// Switch-over event occurred
	else
		return false;// No switch-over event occurred
}

/*
 * Function from https://stackoverflow.com/questions/19377396/c-get-day-of-year-from-date
 */
static int yisleap(int year)
{
	return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/*
 * Function from https://stackoverflow.com/questions/19377396/c-get-day-of-year-from-date
 */
static int get_yday(int mon, int day, int year)
{
	static const int days[2][12] = {
		{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
		{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}};
	int leap = yisleap(year);

	/* Minus 1 since gmtime starts from 0 */
	return days[leap][mon] + day - 1;
}

/*!
* @brief Writes date and time to RTC.
*
* @param dev Pointer to the device structure for an I2C controller
* driver configured in master mode.
* @param ts Pointer to the time in seconds since January 01 1970 (UTC)
*
* @return 0 if successful
* @return 1 if not successful
*
*/
uint32_t pcf8523_set_time(const struct device *dev, int64_t *ts){

	uint8_t i2c_buff[7] = {0};

	/* Convert time to struct tm */
	struct tm *tm_p= gmtime(ts);
	struct tm time = {0};

	if (tm_p != NULL){
		memcpy(&time, tm_p, sizeof(struct tm));
	}
	else{	// Unable to convert to broken down UTC
		return 1;
	}

	// Set pointer to the seconds register and write the time and date
	i2c_buff[0] = PCF8523_SECONDS_MASK & (((time.tm_sec/10) << PCF8523_BCD_UPPER_SHIFT) + (time.tm_sec%10));	// Number of seconds in BCD
	i2c_buff[1] = PCF8523_MINUTES_MASK & (((time.tm_min/10) << PCF8523_BCD_UPPER_SHIFT) + (time.tm_min%10));	// Number of minutes in BCD
	i2c_buff[2] = PCF8523_HOURS_MASK & (((time.tm_hour/10) << PCF8523_BCD_UPPER_SHIFT) + (time.tm_hour%10));	// Number of hours in BCD - 24hr format
	i2c_buff[3] = PCF8523_DAYS_MASK & (((time.tm_mday/10) << PCF8523_BCD_UPPER_SHIFT) + (time.tm_mday%10));		// Day of the month
	i2c_buff[4] = PCF8523_WEEKDAYS_MASK & time.tm_wday;															// Day of the week Since Sunday
	i2c_buff[5] = PCF8523_MONTHS_MASK & (((time.tm_mon/10) << PCF8523_BCD_UPPER_SHIFT) + (time.tm_mon%10));		// Day of the month
	uint8_t year = time.tm_year%100;
	i2c_buff[6] = ((year / 10) << PCF8523_BCD_UPPER_SHIFT) + (year % 10); // year

	// Send data through I2C
	i2c_burst_write(dev, PCF8523_SLAVE_ADD, PCF8523_SECONDS_ADD, i2c_buff, sizeof(i2c_buff));

	return 0;
}

/*!
* @brief Reads date and time from RTC in Unix Epoch (seconds since January 01 1970 (UTC)).
*
* @param dev Pointer to the device structure for an I2C controller
* driver configured in master mode.
* @param ts Pointer where the amounts of seconds since January 01 1970 (UTC) will be stored
*
* @return 0 if successful
* @return 1 if not successful
*
*/
uint32_t pcf8523_get_time(const struct device *dev, int64_t *ts){

	uint8_t i2c_buff[7] = {0};
	struct tm time = {0};

	// Read data from I2C
	i2c_burst_read(dev, PCF8523_SLAVE_ADD, PCF8523_SECONDS_ADD, i2c_buff, sizeof(i2c_buff));

	
	time.tm_sec = (i2c_buff[0] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[0] & PCF8523_BCD_UPPER_MASK_SEC) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get seconds */
	time.tm_min = (i2c_buff[1] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[1] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get minutes */
	time.tm_hour = (i2c_buff[2] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[2] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get hours */
	time.tm_mday = (i2c_buff[3] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[3] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get days */
	time.tm_wday = (i2c_buff[4] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[4] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get weekdays */
	time.tm_mon = (i2c_buff[5] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[5] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get month */
	time.tm_year = (i2c_buff[6] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[6] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10) + 100;/* Get year with offset of 100 since we're in 2000+ */
	time.tm_yday = get_yday(time.tm_mon, time.tm_mday, time.tm_year + 1900);/* Get day number in year */
	time.tm_isdst = 0;/* DST not used  */

	*ts = timeutil_timegm64(&time);

	return 0;
}

/*!
* @brief Reads date and time from RTC in time struct tm.
*
* @param dev Pointer to the device structure for an I2C controller
* driver configured in master mode.
* @param time Pointer to the tm struct
*
* @return 0 if successful
* @return 1 if not successful
*
*/
uint32_t pcf8523_get_time_tm(const struct device *dev, struct tm *time){

	uint8_t i2c_buff[7] = {0};

	// Read data from I2C
	i2c_burst_read(dev, PCF8523_SLAVE_ADD, PCF8523_SECONDS_ADD, i2c_buff, sizeof(i2c_buff));

	
	time->tm_sec = (i2c_buff[0] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[0] & PCF8523_BCD_UPPER_MASK_SEC) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get seconds */
	time->tm_min = (i2c_buff[1] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[1] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get minutes */
	time->tm_hour = (i2c_buff[2] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[2] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get hours */
	time->tm_mday = (i2c_buff[3] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[3] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get days */
	time->tm_wday = (i2c_buff[4] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[4] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get weekdays */
	time->tm_mon = (i2c_buff[5] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[5] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10);/* Get month */
	time->tm_year = (i2c_buff[6] & PCF8523_BCD_LOWER_MASK) + (((i2c_buff[6] & PCF8523_BCD_UPPER_MASK) >> PCF8523_BCD_UPPER_SHIFT) * 10) + 100;/* Get year with offset of 100 since we're in 2000+ */
	time->tm_yday = get_yday(time->tm_mon, time->tm_mday, time->tm_year + 1900);/* Get day number in year */
	time->tm_isdst = 0;/* DST not used  */

	return 0;
}

void convert_time_ascii(struct tm *time, char *tstr, char *dstr){

	int qty=0;
	
	qty += u8_to_dec(tstr, 3, (uint8_t)time->tm_hour);
	if(time->tm_hour < 10){
		tstr[qty] = tstr[qty-1];
		tstr[qty-1] = '0';
		tstr[qty+1] = ':';
		qty += 2;
	}else{
		tstr[qty] = ':';
		qty++;
	}
	qty += u8_to_dec(&tstr[qty], 3, (uint8_t)time->tm_min);
	if(time->tm_min < 10){
		tstr[qty] = tstr[qty-1];
		tstr[qty-1] = '0';
		tstr[qty+1] = ':';
		qty += 2;
	}else{
		tstr[qty] = ':';
		qty++;
	}
	qty += u8_to_dec(&tstr[qty], 3, (uint8_t)time->tm_sec);
	if(time->tm_sec < 10){
		tstr[qty] = tstr[qty-1];
		tstr[qty-1] = '0';
	}
	qty++;
	tstr[qty] = 0;

	// Convert date to str
	qty=0;
	qty += u8_to_dec(dstr, 3, (uint8_t)time->tm_mday);
	if(time->tm_mday < 10){
		dstr[qty] = dstr[qty-1];
		dstr[qty-1] = '0';
		dstr[qty+1] = '/';
		qty += 2;
	}else{
		dstr[qty] = '/';
		qty++;
	}
	qty += u8_to_dec(&dstr[qty], 3, (uint8_t)time->tm_mon + 1);
	if((time->tm_mon+1) < 10){
		dstr[qty] = dstr[qty-1];
		dstr[qty-1] = '0';
		dstr[qty+1] = '/';
		qty += 2;
	}else{
		dstr[qty] = '/';
		qty++;
	}
	qty += u8_to_dec(&dstr[qty], 3, (uint8_t)time->tm_year - 100);
	if((time->tm_year-100) < 10){
		dstr[qty] = dstr[qty-1];
		dstr[qty-1] = '0';
	}
	qty++;
	dstr[qty] = 0;

	return;
}