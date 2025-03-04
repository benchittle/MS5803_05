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
 * 	Copyright Luke Miller, April 1 2014
 */

#include "MS5803_05.h"
#include <Wire.h>

// For I2C, set the CSB Pin (pin 3) high for address 0x76, and pull low
// for address 0x77. If you use 0x77, change the value on the line below:


// Some constants used in calculations below
#define POW_2_33 8589934592ULL;

//-------------------------------------------------
// Constructor
MS_5803::MS_5803( uint16_t Resolution) {
	// The argument is the oversampling resolution, which may have values
	// of 256, 512, 1024, 2048, or 4096.
	_Resolution = Resolution;

    dT = 0;
    TEMP = 0;
    Offset = 0;
    Sensitivity = 0;
    varT2 = 0;
    OFF2 = 0;
    Sens2 = 0;
}

//-------------------------------------------------
boolean MS_5803::initializeMS_5803(boolean Verbose) {
    Wire.begin();
    // Reset the sensor during startup
    resetSensor(); 
    
    if (Verbose) {
    	// Display the oversampling resolution or an error message
    	if (_Resolution == 256 || _Resolution == 512 || _Resolution == 1024 || _Resolution == 2048 || _Resolution == 4096){
        	Serial.print("Oversampling setting: ");
        	Serial.println(_Resolution);    		
    	} else {
			Serial.println("*******************************************");
			Serial.println("Error: specify a valid oversampling value");
			Serial.println("Choices are 256, 512, 1024, 2048, or 4096");
			Serial.println("*******************************************");
    	}

    }
	// Read sensor coefficients
    for (int i = 0; i < 8; i++ ){
    	// The PROM starts at address 0xA0
    	Wire.beginTransmission(MS5803_I2C_ADDRESS);
    	Wire.write(0xA0 + (i * 2));
    	Wire.endTransmission();
    	Wire.requestFrom(MS5803_I2C_ADDRESS, 2);
    	while(Wire.available()) {
    		HighByte = Wire.read();
    		LowByte = Wire.read();
    	}
    	sensorCoeffs[i] = (((uint16_t)HighByte << 8) + LowByte);
    	if (Verbose){
			// Print out coefficients 
			Serial.print("C");
			Serial.print(i);
			Serial.print(" = ");
			Serial.println(sensorCoeffs[i]);
			delay(10);
    	}
    }
    // The last 4 bits of the 7th coefficient form a CRC error checking code.
    uint8_t p_crc = sensorCoeffs[7];
    // Use a function to calculate the CRC value
    uint8_t n_crc = MS_5803_CRC(sensorCoeffs); 
    
    if (Verbose) {
		Serial.print("p_crc: ");
		Serial.println(p_crc);
		Serial.print("n_crc: ");
		Serial.println(n_crc);
    }
    // If the CRC value doesn't match the sensor's CRC value, then the 
    // connection can't be trusted. Check your wiring. 
    if (p_crc != n_crc) {
        return false;
    }
    // Otherwise, return true when everything checks out OK. 
    return true;
}

//------------------------------------------------------------------
void MS_5803::readSensor() {
	// Choose from CMD_ADC_256, 512, 1024, 2048, 4096 for mbar resolutions
	// of 1, 0.6, 0.4, 0.3, 0.2 respectively. Higher resolutions take longer
	// to read.
	if (_Resolution == 256){
		varD1 = MS_5803_ADC(CMD_ADC_D1 + CMD_ADC_256); // read raw pressure
		varD2 = MS_5803_ADC(CMD_ADC_D2 + CMD_ADC_256); // read raw temperature	
	} else if (_Resolution == 512) {
		varD1 = MS_5803_ADC(CMD_ADC_D1 + CMD_ADC_512); // read raw pressure
		varD2 = MS_5803_ADC(CMD_ADC_D2 + CMD_ADC_512); // read raw temperature		
	} else if (_Resolution == 1024) {
		varD1 = MS_5803_ADC(CMD_ADC_D1 + CMD_ADC_1024); // read raw pressure
		varD2 = MS_5803_ADC(CMD_ADC_D2 + CMD_ADC_1024); // read raw temperature
	} else if (_Resolution == 2048) {
		varD1 = MS_5803_ADC(CMD_ADC_D1 + CMD_ADC_2048); // read raw pressure
		varD2 = MS_5803_ADC(CMD_ADC_D2 + CMD_ADC_2048); // read raw temperature
	} else if (_Resolution == 4096) {
		varD1 = MS_5803_ADC(CMD_ADC_D1 + CMD_ADC_4096); // read raw pressure
		varD2 = MS_5803_ADC(CMD_ADC_D2 + CMD_ADC_4096); // read raw temperature
	}
    convertRaw(varD1, varD2);
}

void MS_5803::convertRaw(uint32_t d1Val, uint32_t d2Val) {
    // Calculate 1st order temperature, dT is a long integer
	// varD2 is originally cast as an uint32_t, but can fit in a int32_t, so we'll
	// cast both parts of the equation below as signed values so that we can
	// get a negative answer if needed
    dT = (int32_t)d2Val - ( (int32_t)sensorCoeffs[5] * 256 );
    // Use integer division to calculate TEMP. It is necessary to cast
    // one of the operands as a signed 64-bit integer (int64_t) so there's no 
    // rollover issues in the numerator.
    TEMP = 2000 + ((int64_t)dT * sensorCoeffs[6]) / 8388608LL;
    // Recast TEMP as a signed 32-bit integer
    TEMP = (int32_t)TEMP;
 
    
    // All operations from here down are done as integer math until we make
    // the final calculation of pressure in mbar. 
    
    
    // Do 2nd order temperature compensation (see pg 9 of MS5803 data sheet)
    // I have tried to insert the fixed values wherever possible 
    // (i.e. 2^31 is hard coded as 2147483648).
    if (TEMP < 2000) {
		// For 5 bar model
		// If temperature is below 20.0C
		varT2 = 3 * ((int64_t)dT * dT)  / POW_2_33 ; // 2^33 = 8589934592
		varT2 = (int32_t)varT2; // recast as signed 32bit integer
		OFF2 = 3 * ((TEMP-2000) * (TEMP-2000)) / 8 ;
		Sens2 = 7 * ((TEMP-2000) * (TEMP-2000)) / 8 ;
    } else { // if TEMP is > 2000 (20.0C)
		// For 5 bar sensor
		varT2 = 0;
		OFF2 = 0;
		Sens2 = 0;
    }
    // Additional compensation for very low temperatures (< -15C)
//    if (TEMP < -1500) {
//		// For 5 bar sensor
//		// Leave OFF2 alone in this case
//		Sens2 = Sens2 + 3 * ((TEMP+1500)*(TEMP+1500));
//    }
    
    // Calculate initial Offset and Sensitivity
    // Notice lots of casts to uint32_t and int64_t to ensure that the 
    // multiplication operations don't overflow the original 16 bit and 32 bit
    // integers

	// For 5 bar sensor
	Offset = (int64_t)sensorCoeffs[2] * 262144 + (sensorCoeffs[4] * (int64_t)dT) / 32;
	Sensitivity = (int64_t)sensorCoeffs[1] * 131072 + (sensorCoeffs[3] * (int64_t)dT) / 128;

    
    // Adjust TEMP, Offset, Sensitivity values based on the 2nd order 
    // temperature correction above.
    TEMP = TEMP - varT2; // both should be int32_t
    Offset = Offset - OFF2; // both should be int64_t
    Sensitivity = Sensitivity - Sens2; // both should be int64_t
    // Final compensated pressure calculation. We first calculate the pressure
    // as a signed 32-bit integer (mbarInt), then convert that value to a
    // float (mbar). 

	// For 5 bar sensor
	mbarInt = ((d1Val * Sensitivity) / 2097152 - Offset) / 32768;
    mbar = (float)mbarInt / 100;
    
    // Calculate the human-readable temperature in Celsius
    tempC = (float)TEMP / 100;
    
    // Start other temperature conversions by converting mbar to psi absolute
//    psiAbs = mbar * 0.0145038;
//    // Convert psi absolute to inches of mercury
//    inHgPress = psiAbs * 2.03625;
//    // Convert psi absolute to gauge pressure
//    psiGauge = psiAbs - 14.7;
//    // Convert mbar to mm Hg
//    mmHgPress = mbar * 0.7500617;
//    // Convert temperature to Fahrenheit
//    tempF = (tempC * 1.8) + 32;
}

//------------------------------------------------------------------
// Function to check the CRC value provided by the sensor against the 
// calculated CRC value from the rest of the coefficients. 
// Based on code from Measurement Specialties application note AN520
// http://www.meas-spec.com/downloads/C-Code_Example_for_MS56xx,_MS57xx_%28except_analog_sensor%29_and_MS58xx_Series_Pressure_Sensors.pdf
uint8_t MS_5803::MS_5803_CRC(uint16_t n_prom[]) {
    int16_t cnt;				// simple counter
    uint16_t n_rem;		// crc reminder
    uint16_t crc_read;	// original value of the CRC
    uint8_t  n_bit;
    n_rem = 0x00;
    crc_read = sensorCoeffs[7];		// save read CRC
    sensorCoeffs[7] = (0xFF00 & (sensorCoeffs[7])); // CRC byte replaced with 0
    for (cnt = 0; cnt < 16; cnt++)
    { // choose LSB or MSB
        if (cnt%2 == 1) {
        	n_rem ^= (uint16_t)((sensorCoeffs[cnt>>1]) & 0x00FF);
        }
        else {
        	n_rem ^= (uint16_t)(sensorCoeffs[cnt>>1] >> 8);
        }
        for (n_bit = 8; n_bit > 0; n_bit--)
        {
            if (n_rem & (0x8000))
            {
                n_rem = (n_rem << 1) ^ 0x3000;
            }
            else {
                n_rem = (n_rem << 1);
            }
        }
    }
    n_rem = (0x000F & (n_rem >> 12));// // final 4-bit reminder is CRC code
    sensorCoeffs[7] = crc_read; // restore the crc_read to its original place
    // Return n_rem so it can be compared to the sensor's CRC value
    return (n_rem ^ 0x00); 
}

//-----------------------------------------------------------------
// Send commands and read the temperature and pressure from the sensor
uint32_t MS_5803::MS_5803_ADC(char commandADC) {
	// varD1 and varD2 will come back as 24-bit values, and so they must be stored in 
	// a long integer on 8-bit Arduinos.
    int32_t result = 0;
    // Send the command to do the ADC conversion on the chip
	Wire.beginTransmission(MS5803_I2C_ADDRESS);
    Wire.write(CMD_ADC_CONV + commandADC);
    Wire.endTransmission();
    // Wait a specified period of time for the ADC conversion to happen
    // See table on page 1 of the MS5803 data sheet showing response times of
    // 0.5, 1.1, 2.1, 4.1, 8.22 ms for each accuracy level. 
    switch (commandADC & 0x0F) 
    {
        case CMD_ADC_256 :
            delay(1); // 1 ms
            break;
        case CMD_ADC_512 :
            delay(3); // 3 ms
            break;
        case CMD_ADC_1024:
            delay(4);
            break;
        case CMD_ADC_2048:
            delay(6);
            break;
        case CMD_ADC_4096:
            delay(10);
            break;
    }
    // Now send the read command to the MS5803 
    Wire.beginTransmission(MS5803_I2C_ADDRESS);
    Wire.write((byte)CMD_ADC_READ);
    Wire.endTransmission();
    // Then request the results. This should be a 24-bit result (3 bytes)
    Wire.requestFrom(MS5803_I2C_ADDRESS, 3);
    while(Wire.available()) {
    	HighByte = Wire.read();
    	MidByte = Wire.read();
    	LowByte = Wire.read();
    }
    // Combine the bytes into one integer
    result = ((uint32_t)HighByte << 16) + ((uint32_t)MidByte << 8) + (uint32_t)LowByte;
    return result;
}

//----------------------------------------------------------------
// Sends a power on reset command to the sensor.
void MS_5803::resetSensor() {
    	Wire.beginTransmission(MS5803_I2C_ADDRESS);
        Wire.write(CMD_RESET);
        Wire.endTransmission();
    	delay(5);
}
