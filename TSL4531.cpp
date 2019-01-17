/*
 * TSL4531.cpp
 *
 *  Created on: Feb 2018
 *      Author: raulMrello
 *
 */
 
#include "TSL4531.h"
//------------------------------------------------------------------------------------
//-- PRIVATE TYPEDEFS ----------------------------------------------------------------
//------------------------------------------------------------------------------------

// RTC EPSON TSL4531
//  7bit address = 0b0110010(No other choice)
#define TSL4531ADDR  			(0x29 << 1)

// Registers
#define TSL4531_REG_COMMAND 	0x80
#define TSL4531_REG_CONTROL 	0x00
#define TSL4531_REG_CONFIG 		0x01
#define TSL4531_REG_DATA_LOW 	0x04
#define TSL4531_REG_DATA_HIGH 	0x05
#define TSL4531_REG_DEVICE_ID 	0x0A

// TSL4531 Misc Values
#define TSL4531_ON 				0x03
#define TSL4531_OFF 			0x00

struct Operation{
	char reg;
	char ldata;
	char hdata;
};


/** Macro para imprimir trazas de depuración, siempre que se haya configurado un objeto
 *	Logger válido (ej: _debug)
 */
static const char* _MODULE_ = "[TSL4531].......";
#define _EXPR_	(_defdbg && !IS_ISR())


//------------------------------------------------------------------------------------
//-- PUBLIC METHODS IMPLEMENTATION ---------------------------------------------------
//------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------
TSL4531::TSL4531 (PinName p_sda, PinName p_scl, bool defdbg) : _i2c_p(new I2C(p_sda, p_scl)), _i2c(*_i2c_p) {
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Iniciando driver TSL4531");
    TSL4531_addr = TSL4531ADDR;
    _integration_time = TSL4531_INTEGRATION_400MS;
    _skip_powersave = false;
    _defdbg = defdbg;
    _ready = (init() != -1)? true : false;
}


//------------------------------------------------------------------------------------
TSL4531::TSL4531 (I2C* p_i2c, bool defdbg) : _i2c_p(p_i2c), _i2c(*_i2c_p){
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Iniciando driver TSL4531");
    TSL4531_addr = TSL4531ADDR;
    _defdbg = defdbg;
    _ready = (init() != -1)? true : false;
}


//------------------------------------------------------------------------------------
int8_t TSL4531::init(){
    int8_t dt;
    Operation op;

    // habilita el chip
    if(!_enable()){
		return -1;
	}

    // chequea que está habilitado
    op = {(TSL4531_REG_COMMAND | TSL4531_REG_CONTROL), 0, 0};
    if(_i2c.write(TSL4531_addr, &op.reg, 1) != 0){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WRT registro TSL4531_REG_CONTROL");
		return -1;
	}
    if(_i2c.read(TSL4531_addr, &op.ldata, 1) != 0){
		DEBUG_TRACE_I(_EXPR_, _MODULE_, "rd_val_ERROR\r\n");
		return -1;
	}
    if (op.ldata != TSL4531_ON) {
    	DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_INIT tsl4531, no habilitado");
	}

    // lee el devID
    op = {(TSL4531_REG_COMMAND | TSL4531_REG_DEVICE_ID), 0, 0};
    if(_i2c.write(TSL4531_addr, &op.reg, 1) != 0){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WRT registro TSL4531_REG_CONTROL");
		return -1;
	}
	if(_i2c.read(TSL4531_addr, &op.ldata, 1) != 0){
		DEBUG_TRACE_I(_EXPR_, _MODULE_, "rd_val_ERROR\r\n");
		return -1;
	}
	uint8_t id = (op.ldata & 0xF0) >> 4;
    if (id == TSL4531_PART_TSL45317) {
		_part_id = TSL4531_PART_TSL45317;
	}
    else if (id == TSL4531_PART_TSL45313) {
		_part_id = TSL4531_PART_TSL45313;
	}
    else if (id == TSL4531_PART_TSL45315) {
		_part_id = TSL4531_PART_TSL45315;
	}
    else if (id == TSL4531_PART_TSL45311) {
		_part_id = TSL4531_PART_TSL45311;
	}
    else {
    	DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_ID pieza %d desconocida", id);
	}

    // deshabilita  el chip
    if(!_disable()){
		return -1;
	}

    // crea la callback de publicación
    _publicationCb = callback(this, &TSL4531::pubCb);

    // crea el hilo de lectura
    _th_read = new Thread(osPriorityNormal+2, OS_STACK_SIZE, NULL, "Lux");
    MBED_ASSERT(_th_read);
    _is_th_run = false;


	return 0;
}


//------------------------------------------------------------------------------------
int TSL4531::setIntegrationTime(TSL4531::tsl4531_integration_time_t t){
	int dt;
	uint8_t power_save_bit = _skip_powersave ? 0x08 : 0x00;
	uint8_t integration_time_bits = 0x03 & t;
	uint8_t new_config_reg = power_save_bit | integration_time_bits;

    // habilita el chip
    if(!_enable()){
		return -1;
	}

    // actualiza el tiempo de integración
	if(!_write((TSL4531_REG_COMMAND | TSL4531_REG_CONFIG), new_config_reg)){
		return -1;
	}

   // deshabilita  el chip
    if(!_disable()){
		return -1;
	}

	_integration_time = t;

    return 0;
}


//------------------------------------------------------------------------------------
int TSL4531::setPowerSaveSkip(bool endis){
	int dt;
	uint8_t power_save_bit = endis ? 0x08 : 0x00;
	uint8_t integration_time_bits = 0x03 & _integration_time;
	uint8_t new_config_reg = power_save_bit | integration_time_bits;

    // habilita el chip
    if(!_enable()){
		return -1;
	}

    // actualiza el power-save-skip
	if(!_write((TSL4531_REG_COMMAND | TSL4531_REG_CONFIG), new_config_reg)){
		return -1;
	}

   // deshabilita  el chip
    if(!_disable()){
		return -1;
	}

	_skip_powersave = endis;
    return 0;
}


//------------------------------------------------------------------------------------
void TSL4531::readLux(){
	int dt;
    bool success = true;
    uint16_t multiplier = 1;

    do{
    	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Iniciando lectura");

		// habilita el chip
		if(!_enable()){
			DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WRT Enable");
			continue;
		}

		switch (_integration_time){
			case TSL4531_INTEGRATION_100MS:
				multiplier = 4;
				wait_ms(100);
				break;
			case TSL4531_INTEGRATION_200MS:
				multiplier = 2;
				wait_ms(200);
				break;
			default:
			//case TSL4531_INTEGRATION_400MS:
				multiplier = 1;
				wait_ms(400);
				break;
		}

		uint16_t lux_data;
		// lee los lux
		Operation op = {(TSL4531_REG_COMMAND | TSL4531_REG_DATA_LOW), 0, 0};
		if(_i2c.write(TSL4531_addr, &op.reg, 1) != 0){
			DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WRT registro TSL4531_REG_DATA_LOW");
			continue;
		}
		if(_i2c.read(TSL4531_addr, &op.ldata, 2) != 0){
			DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_READ registro TSL4531_REG_DATA_LOW");
			continue;
		}

		// deshabilita  el chip
		if(!_disable()){
			DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WRT Disable");
			continue;
		}

		_lux = (multiplier * (((uint16_t)op.hdata << 8) + op.ldata));
		MQ::MQClient::publish("stat/value/lux", &_lux, sizeof(uint16_t), &_publicationCb);

		DEBUG_TRACE_I(_EXPR_, _MODULE_, "Lectura completada Lux = %d", _lux);

		wait_ms(_cycle);

    }while(_is_th_run);

}

//------------------------------------------------------------------------------------
//-- PROTECTED METHODS IMPLEMENTATION ------------------------------------------------
//------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------
bool TSL4531::_enable(){
    Operation op = {(TSL4531_REG_COMMAND | TSL4531_REG_CONTROL), TSL4531_ON, 0};
    if(_i2c.write(TSL4531_addr, &op.reg, 2) != 0){
    	DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WRT Enable");
		return false;
	}
    return true;
}


//------------------------------------------------------------------------------------
bool TSL4531::_disable(){
    Operation op = {(TSL4531_REG_COMMAND | TSL4531_REG_CONTROL), TSL4531_OFF, 0};
    if(_i2c.write(TSL4531_addr, &op.reg, 2) != 0){
    	DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WRT Disable");
		return false;
	}
    return true;
}


//------------------------------------------------------------------------------------
bool TSL4531::_write(char reg, char value){
    Operation op = {reg, value, 0};
    if(_i2c.write(TSL4531_addr, &op.reg, 2) != 0){
    	DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_WRT Config");
		return false;
	}
    return true;
}
