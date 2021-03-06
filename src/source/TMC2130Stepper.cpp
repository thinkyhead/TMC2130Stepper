#include "TMC2130Stepper.h"
#include "TMC2130Stepper_MACROS.h"
#include <SPI.h>
#include "SW_SPI.h"

TMC2130Stepper::TMC2130Stepper(uint16_t pinCS) : _pinCS(pinCS) { _started = false; }

TMC2130Stepper::TMC2130Stepper(uint16_t pinEN, uint16_t pinDIR, uint16_t pinStep, uint16_t pinCS) {
	_started = false;
	_pinEN = pinEN;
	_pinSTEP = pinStep;
	_pinCS = pinCS;
	_pinDIR = pinDIR;
}

TMC2130Stepper::TMC2130Stepper(uint16_t pinEN, uint16_t pinDIR, uint16_t pinStep, uint16_t pinCS, uint16_t pinMOSI, uint16_t pinMISO, uint16_t pinSCK) {
	_started = false;
	_pinEN = pinEN;
	_pinSTEP = pinStep;
	_pinCS = pinCS;
	_pinDIR = pinDIR;
	uses_sw_spi = true;
	TMC_SW_SPI.setPins(pinMOSI, pinMISO, pinSCK);
}

void TMC2130Stepper::begin() {
#ifdef TMC2130DEBUG
	Serial.println("TMC2130 Stepper driver library");
	Serial.print("Enable pin: ");
	Serial.println(_pinEN);
	Serial.print("Direction pin: ");
	Serial.println(_pinDIR);
	Serial.print("Step pin: ");
	Serial.println(_pinSTEP);
	Serial.print("Chip select pin: ");
	Serial.println(_pinCS);
#endif
	delay(200);

	//set pins
	if (_pinEN != 0xFFFF) {
		pinMode(_pinEN, OUTPUT);
		digitalWrite(_pinEN, HIGH); //deactivate driver (LOW active)
	}
	if (_pinDIR != 0xFFFF) {
		pinMode(_pinDIR, OUTPUT);
		digitalWrite(_pinDIR, LOW); //LOW or HIGH
	}
	if (_pinSTEP != 0xFFFF) {
		pinMode(_pinSTEP, OUTPUT);
		digitalWrite(_pinSTEP, LOW);
	}

	pinMode(_pinCS, OUTPUT);
	digitalWrite(_pinCS, HIGH);

	if (uses_sw_spi) TMC_SW_SPI.init();

	GCONF(GCONF_sr);
	CHOPCONF(CHOPCONF_sr);
	COOLCONF(COOLCONF_sr);
	PWMCONF(PWMCONF_sr);
	IHOLD_IRUN(IHOLD_IRUN_sr);

	toff(8); //off_time(8);
	tbl(1); //blank_time(24);

	_started = true;
}

void TMC2130Stepper::send2130(uint8_t addressByte, uint32_t *config) {
	if (uses_sw_spi) {
		digitalWrite(_pinCS, LOW);

		status_response = TMC_SW_SPI.transfer(addressByte & 0xFF); // s =

		if (addressByte >> 7) { // Check if WRITE command
			//*config &= ~mask; // Clear bits being set
			//*config |= (value & mask); // Set new values
			TMC_SW_SPI.transfer((*config >> 24) & 0xFF);
			TMC_SW_SPI.transfer((*config >> 16) & 0xFF);
			TMC_SW_SPI.transfer((*config >>  8) & 0xFF);
			TMC_SW_SPI.transfer(*config & 0xFF);
		} else { // READ command
			TMC_SW_SPI.transfer16(0x0000); // Clear SPI
			TMC_SW_SPI.transfer16(0x0000);
			digitalWrite(_pinCS, HIGH);
			digitalWrite(_pinCS, LOW);

			TMC_SW_SPI.transfer(addressByte & 0xFF); // Send the address byte again
			*config = TMC_SW_SPI.transfer(0x00);
			*config <<= 8;
			*config|= TMC_SW_SPI.transfer(0x00);
			*config <<= 8;
			*config|= TMC_SW_SPI.transfer(0x00);
			*config <<= 8;
			*config|= TMC_SW_SPI.transfer(0x00);
		}

		digitalWrite(_pinCS, HIGH);
	} else {
		SPI.begin();
		SPI.beginTransaction(SPISettings(16000000/8, MSBFIRST, SPI_MODE3));
		digitalWrite(_pinCS, LOW);

		status_response = SPI.transfer(addressByte & 0xFF); // s =

		if (addressByte >> 7) { // Check if WRITE command
			//*config &= ~mask; // Clear bits being set
			//*config |= (value & mask); // Set new values
			SPI.transfer((*config >> 24) & 0xFF);
			SPI.transfer((*config >> 16) & 0xFF);
			SPI.transfer((*config >>  8) & 0xFF);
			SPI.transfer(*config & 0xFF);
		} else { // READ command
			SPI.transfer16(0x0000); // Clear SPI
			SPI.transfer16(0x0000);
			digitalWrite(_pinCS, HIGH);
			digitalWrite(_pinCS, LOW);

			SPI.transfer(addressByte & 0xFF); // Send the address byte again
			*config = SPI.transfer(0x00);
			*config <<= 8;
			*config|= SPI.transfer(0x00);
			*config <<= 8;
			*config|= SPI.transfer(0x00);
			*config <<= 8;
			*config|= SPI.transfer(0x00);
		}

		digitalWrite(_pinCS, HIGH);
		SPI.endTransaction();
	}
}

bool TMC2130Stepper::checkOT() {
	uint32_t response = DRV_STATUS();
	if (response & OTPW_bm) {
		flag_otpw = 1;
		return true; // bit 26 for overtemperature warning flag
	}
	return false;
}

bool TMC2130Stepper::getOTPW() { return flag_otpw; }

void TMC2130Stepper::clear_otpw() {	flag_otpw = 0; }

bool TMC2130Stepper::isEnabled() { return !digitalRead(_pinEN) && toff(); }

void TMC2130Stepper::push() {
	GCONF(GCONF_sr);
	IHOLD_IRUN(IHOLD_IRUN_sr);
	TPOWERDOWN(TPOWERDOWN_sr);
	TPWMTHRS(TPWMTHRS_sr);
	TCOOLTHRS(TCOOLTHRS_sr);
	THIGH(THIGH_sr);
	XDIRECT(XDIRECT_sr);
	VDCMIN(VDCMIN_sr);
	CHOPCONF(CHOPCONF_sr);
	COOLCONF(COOLCONF_sr);
	PWMCONF(PWMCONF_sr);
	ENCM_CTRL(ENCM_CTRL_sr);
}

uint8_t TMC2130Stepper::test_connection() {
	uint32_t drv_status = DRV_STATUS();
	switch (drv_status) {
	    case 0xFFFFFFFF: return 1;
	    case 0: return 2;
	    default: return 0;
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// R+C: GSTAT
void 	TMC2130Stepper::GSTAT(uint8_t input){
	GSTAT_sr = input;
	WRITE_REG(GSTAT);
}
uint8_t TMC2130Stepper::GSTAT()			 	{ READ_REG_R(GSTAT); 		}
bool 	TMC2130Stepper::reset()				{ GET_BYTE(GSTAT, RESET);	}
bool 	TMC2130Stepper::drv_err()			{ GET_BYTE(GSTAT, DRV_ERR);	}
bool 	TMC2130Stepper::uv_cp()				{ GET_BYTE(GSTAT, UV_CP);	}
///////////////////////////////////////////////////////////////////////////////////////
// R: IOIN
uint32_t 	TMC2130Stepper::IOIN() 			{ READ_REG_R(IOIN); 				}
bool 		TMC2130Stepper::step()			{ GET_BYTE_R(IOIN, STEP);			}
bool 		TMC2130Stepper::dir()			{ GET_BYTE_R(IOIN, DIR);			}
bool 		TMC2130Stepper::dcen_cfg4()		{ GET_BYTE_R(IOIN, DCEN_CFG4);		}
bool 		TMC2130Stepper::dcin_cfg5()		{ GET_BYTE_R(IOIN, DCIN_CFG5);		}
bool 		TMC2130Stepper::drv_enn_cfg6()	{ GET_BYTE_R(IOIN, DRV_ENN_CFG6);	}
bool 		TMC2130Stepper::dco()			{ GET_BYTE_R(IOIN, DCO);			}
uint8_t 	TMC2130Stepper::version() 		{ GET_BYTE_R(IOIN, VERSION);		}
///////////////////////////////////////////////////////////////////////////////////////
// W: TPOWERDOWN
uint32_t TMC2130Stepper::TPOWERDOWN() { return TPOWERDOWN_sr; }
void TMC2130Stepper::TPOWERDOWN(uint32_t input) {
	TPOWERDOWN_sr = input;
	WRITE_REG(TPOWERDOWN);
}
///////////////////////////////////////////////////////////////////////////////////////
// R: TSTEP
uint32_t TMC2130Stepper::TSTEP() { READ_REG_R(TSTEP); }
///////////////////////////////////////////////////////////////////////////////////////
// W: TPWMTHRS
uint32_t TMC2130Stepper::TPWMTHRS() { return TPWMTHRS_sr; }
void TMC2130Stepper::TPWMTHRS(uint32_t input) {
	TPWMTHRS_sr = input;
	WRITE_REG(TPWMTHRS);
}
///////////////////////////////////////////////////////////////////////////////////////
// W: TCOOLTHRS
uint32_t TMC2130Stepper::TCOOLTHRS() { return TCOOLTHRS_sr; }
void TMC2130Stepper::TCOOLTHRS(uint32_t input) {
	TCOOLTHRS_sr = input;
	WRITE_REG(TCOOLTHRS);
}
///////////////////////////////////////////////////////////////////////////////////////
// W: THIGH
uint32_t TMC2130Stepper::THIGH() { return THIGH_sr; }
void TMC2130Stepper::THIGH(uint32_t input) {
	THIGH_sr = input;
	WRITE_REG(THIGH);
}
///////////////////////////////////////////////////////////////////////////////////////
// RW: XDIRECT
uint32_t TMC2130Stepper::XDIRECT() { READ_REG(XDIRECT); }
void TMC2130Stepper::XDIRECT(uint32_t input) {
	XDIRECT_sr = input;
	WRITE_REG(XDIRECT);
}
void TMC2130Stepper::coil_A(int16_t B) 	{ MOD_REG(XDIRECT, COIL_A); 	}
void TMC2130Stepper::coil_B(int16_t B) 	{ MOD_REG(XDIRECT, COIL_B); 	}
int16_t TMC2130Stepper::coil_A() 		{ GET_BYTE_R(XDIRECT, COIL_A); 	}
int16_t TMC2130Stepper::coil_B() 		{ GET_BYTE_R(XDIRECT, COIL_A); 	}
///////////////////////////////////////////////////////////////////////////////////////
// W: VDCMIN
uint32_t TMC2130Stepper::VDCMIN() { return VDCMIN_sr; }
void TMC2130Stepper::VDCMIN(uint32_t input) {
	VDCMIN_sr = input;
	WRITE_REG(VDCMIN);
}
///////////////////////////////////////////////////////////////////////////////////////
// R: PWM_SCALE
uint8_t TMC2130Stepper::PWM_SCALE() { READ_REG_R(PWM_SCALE); }
///////////////////////////////////////////////////////////////////////////////////////
// W: ENCM_CTRL
uint8_t TMC2130Stepper::ENCM_CTRL() { return ENCM_CTRL_sr; }
void TMC2130Stepper::ENCM_CTRL(uint8_t input) {
	ENCM_CTRL_sr = input;
	WRITE_REG(ENCM_CTRL);
}
void TMC2130Stepper::inv(bool B)		{ MOD_REG(ENCM_CTRL, INV);		}
void TMC2130Stepper::maxspeed(bool B)	{ MOD_REG(ENCM_CTRL, MAXSPEED); }
bool TMC2130Stepper::inv() 				{ GET_BYTE(ENCM_CTRL, INV); 	}
bool TMC2130Stepper::maxspeed() 		{ GET_BYTE(ENCM_CTRL, MAXSPEED);}
///////////////////////////////////////////////////////////////////////////////////////
// R: LOST_STEPS
uint32_t TMC2130Stepper::LOST_STEPS() { READ_REG_R(LOST_STEPS); }


/**
 *	Helper functions
 */


/*	
	Requested current = mA = I_rms/1000
	Equation for current:
	I_rms = (CS+1)/32 * V_fs/(R_sense+0.02ohm) * 1/sqrt(2)
	Solve for CS ->
	CS = 32*sqrt(2)*I_rms*(R_sense+0.02)/V_fs - 1
	
	Example:
	vsense = 0b0 -> V_fs = 0.325V
	mA = 1640mA = I_rms/1000 = 1.64A
	R_sense = 0.10 Ohm
	->
	CS = 32*sqrt(2)*1.64*(0.10+0.02)/0.325 - 1 = 26.4
	CS = 26
*/	
void TMC2130Stepper::rms_current(uint16_t mA, float multiplier, float RS) {
	Rsense = RS;
	uint8_t CS = 32.0*1.41421*mA/1000.0*(Rsense+0.02)/0.325 - 1;
	// If Current Scale is too low, turn on high sensitivity R_sense and calculate again
	if (CS < 16) {
		vsense(true);
		CS = 32.0*1.41421*mA/1000.0*(Rsense+0.02)/0.180 - 1;
	} else if(vsense()) { // If CS >= 16, turn off high_sense_r if it's currently ON
		vsense(false);
	}
	irun(CS);
	ihold(CS*multiplier);
	val_mA = mA;
}

uint16_t TMC2130Stepper::rms_current() {
	return (float)(irun()+1)/32.0 * (vsense()?0.180:0.325)/(Rsense+0.02) / 1.41421 * 1000;
}

void TMC2130Stepper::setCurrent(uint16_t mA, float R, float multiplier) { rms_current(mA, multiplier, R); }
uint16_t TMC2130Stepper::getCurrent() {	return val_mA; }

void TMC2130Stepper::SilentStepStick2130(uint16_t current) { rms_current(current); }

void TMC2130Stepper::microsteps(uint16_t ms) {
	switch(ms) {
		case 256: mres(0); break;
		case 128: mres(1); break;
		case  64: mres(2); break;
		case  32: mres(3); break;
		case  16: mres(4); break;
		case   8: mres(5); break;
		case   4: mres(6); break;
		case   2: mres(7); break;
		case   0: mres(8); break;
		default: break;
	}
}

uint16_t TMC2130Stepper::microsteps() {
	switch(mres()) {
		case 0: return 256;
		case 1: return 128;
		case 2: return  64;
		case 3: return  32;
		case 4: return  16;
		case 5: return   8;
		case 6: return   4;
		case 7: return   2;
		case 8: return   0;
	}
	return 0;
}

void TMC2130Stepper::sg_current_decrease(uint8_t value) {
	switch(value) {
		case 32: sedn(0b00); break;
		case  8: sedn(0b01); break;
		case  2: sedn(0b10); break;
		case  1: sedn(0b11); break;
	}
}
uint8_t TMC2130Stepper::sg_current_decrease() {
	switch(sedn()) {
		case 0b00: return 32;
		case 0b01: return  8;
		case 0b10: return  2;
		case 0b11: return  1;
	}
	return 0;
}
