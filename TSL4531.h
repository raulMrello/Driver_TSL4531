/*
 * TSL4531.h
 *
 *  Created on: Feb 2018
 *      Author: raulMrello
 *
 *	TSL4531 es el driver de luminosidad control del chip TSL4531, que utiliza un canal I2C. 
 */
 
#ifndef TSL4531_H
#define TSL4531_H
 
#include "mbed.h"
#include "MQLib.h"
 

class TSL4531 {
public:

	/** Definición de los tiempos de integración */
	enum tsl4531_integration_time_t {
	    TSL4531_INTEGRATION_100MS = 0x02,
	    TSL4531_INTEGRATION_200MS = 0x01,
	    TSL4531_INTEGRATION_400MS = 0x00 // Default
	};


    /** Configure data pin (with other devices on I2C line)
      * @param data SDA and SCL pins
      */
    TSL4531(PinName p_sda, PinName p_scl, bool defdbg=false);
 

    /** Configure data pin (with other devices on I2C line)
      * @param I2C previous definition
      */
    TSL4531(I2C* p_i2c, bool defdbg=false);


    /** Set I2C clock frequency
      * @param freq.
      * @return none
      */
    void frequency(int hz){
        _i2c.frequency(hz);
    }

    /** Devuelve el estado de inicialización */
    bool ready() { return _ready; }

    /** Inicia un proceso de lectura periódica
     * 	@param cycle_ms ciclo de lectura en milis
     */
    void start(uint32_t cycle_ms){ _is_th_run = true; _cycle = cycle_ms; _th_read->start(callback(this, &TSL4531::readLux)); }

    /** Detiene un proceso de lectura
     */
    void stop(){ _is_th_run = false; }

    /** inicia una única lectura de lux */
    void readLux();

	/** Lee los lux
     *  @return Lux
     */
    uint16_t getLux() { return _lux; }


	/** Establece el tiempo de integración
     *  @param t tiempo de integración
     *  @return Código de error o 0.
     */
    int setIntegrationTime(tsl4531_integration_time_t t);


    /** Establece la inhibición del power save
      * @param endis Flag de activación
      * @return Código de error <0
      */
    int setPowerSaveSkip(bool endis);

 
protected:

    /** Piezas compatibles */
    enum tsl4531_part_id_t {
        TSL4531_PART_TSL45317 = 0x08,
        TSL4531_PART_TSL45313 = 0x09,
        TSL4531_PART_TSL45315 = 0x0A,
        TSL4531_PART_TSL45311 = 0x0B
    };

    /** Driver I2C asociado */
    I2C *_i2c_p;
    I2C& _i2c;

    /** Dirección I2C */
    uint8_t TSL4531_addr;

    /** Flags de estado y depuración */
    bool _defdbg;
    bool _ready;

    /** Parámetros de configuración e identificación del chip */
    bool _skip_powersave;
    tsl4531_part_id_t _part_id;
    tsl4531_integration_time_t _integration_time;
    uint16_t _lux;

    /** Thread asociado a las lecturas */
    Thread* _th_read;
    bool _is_th_run;
    uint32_t _cycle;

    /** Callback de publicación */
    MQ::PublishCallback _publicationCb;


    /** Inicializa el chip RTC
     *
     * @return Código de error
     */
    int8_t init(void);

    /** Operaciones i2c */
    bool _enable();
    bool _disable();
    bool _write(char reg, char cfg);

	/** Callback invocada al finalizar una publicación
     *  @param topic Identificador del topic
     *  @param result Resultado de la publicación
     */
    void pubCb(const char* topic, int32_t result){}

};
 
#endif      // TSL4531_H
