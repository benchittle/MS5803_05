/*	
 *  This library has been modified to work with the Arduino ESP framework. It 
 *  has also been modified to work in RTC memory of the ESP32-E for deep sleep
 *  applications.
 * 
 *  Copyright Ben Chittle, 2022
 * 
 * MS5803_05
 * 	An Arduino library for the Measurement Specialties MS5803 family
 * 	of pressure sensors. This library uses I2C to communicate with the
 * 	MS5803 using the Wire library from Arduino.
 *	
 *	This library only works with the MS5803-05BA model sensor. It DOES NOT
 *	work with the other pressure-range models such as the MS5803-14BA or
 *	MS5803-01BA. Those models will return incorrect pressure and temperature 
 *	readings if used with this library. See http://github.com/millerlp for
 *	libraries for the other models. 
 *	 
 * 	No warranty is given or implied. You are responsible for verifying that 
 *	the outputs are correct for your sensor. There are likely bugs in
 *	this code that could result in incorrect pressure readings, particularly
 *	due to variable overflows within some pressure ranges. 
 * 	DO NOT use this code in a situation that could result in harm to you or 
 * 	others because of incorrect pressure readings.
 * 	 
 * 	
 * 	Licensed under the GPL v3 license. 
 * 	Please see accompanying LICENSE.md file for details on reuse and 
 * 	redistribution.
 * 	
 *  Copyright Ben Chittle, 2022
 * 	Copyright Luke Miller, April 1 2014
 */


#ifndef __MS_5803__
#define __MS_5803__

#include <Arduino.h>

class MS_5803 {
public:
	// Constructor for the class. Supply the pressure range for the sensor
	// you're using. Available models include 1, 2, 5, 14, 30 bar. 
	// Your sensor part number should have the pressure range in the number,
	// for example MS5803-14BA or MS5803-01BA would be 14 and 1 bar units.
	// The 2nd argument is the desired oversampling resolution, which has 
	// values of 256, 512, 1024, 2048, 4096
    MS_5803(uint16_t Resolution = 512);
    // Initialize the sensor 
    boolean initializeMS_5803(boolean Verbose = true);
    // Reset the sensor
    void resetSensor();
    // Read the sensor
    void readSensor();
    //*********************************************************************
    // Additional methods to extract temperature, pressure (mbar), and the 
    // varD1,varD2 values after readSensor() has been called
    
    // Return temperature in degrees Celsius.
    float temperature() const       {return tempC;}  
    // Return pressure in mbar.
    float pressure() const          {return mbar;}
//    // Return temperature in degress Fahrenheit.
//    float temperatureF() const		{return tempF;}
//    // Return pressure in psi (absolute)
//    float psia() const				{return psiAbs;}
//    // Return pressure in psi (gauge)
//    float psig() const				{return psiGauge;}
//    // Return pressure in inHg
//    float inHg() const				{return inHgPress;}
//    // Return pressure in mmHg
//    float mmHg() const				{return mmHgPress;}
    // Return the varD1 and varD2 values, mostly for troubleshooting
    uint32_t D1val() const 	{return varD1;}
    uint32_t D2val() const		{return varD2;}
    
    
private:
    
    float mbar; // Store pressure in mbar. 
    float tempC; // Store temperature in degrees Celsius
//    float tempF; // Store temperature in degrees Fahrenheit
//    float psiAbs; // Store pressure in pounds per square inch, absolute
//    float psiGauge; // Store gauge pressure in pounds per square inch (psi)
//    float inHgPress;	// Store pressure in inches of mercury
//    float mmHgPress;	// Store pressure in mm of mercury
    uint32_t varD1;	// Store varD1 value
    uint32_t varD2;	// Store varD2 value
    int32_t mbarInt; // pressure in mbar, initially as a signed long integer
    // Check data integrity with CRC4
    uint8_t MS_5803_CRC(uint16_t n_prom[]); 
    // Handles commands to the sensor.
    uint32_t MS_5803_ADC(char commandADC);
    // Oversampling resolution
    uint16_t _Resolution;

    // Create array to hold the 8 sensor calibration coefficients
    uint16_t sensorCoeffs[8]; // unsigned 16-bit integer (0-65535)
    // These three variables are used for the conversion steps
    // They should be signed 32-bit integer initially 
    // i.e. signed long from -2147483648 to 2147483647
    int32_t	dT;
    int32_t TEMP;
    // These values need to be signed 64 bit integers 
    // (long long = int64_t)
    int64_t	Offset;
    int64_t	Sensitivity;
    int64_t	varT2;
    int64_t	OFF2;
    int64_t	Sens2;
    // bytes to hold the results from I2C communications with the sensor
    byte HighByte;
    byte MidByte;
    byte LowByte;
};

#endif 