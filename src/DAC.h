/*
 * I2S DAC Library for Arduino DUE
 */

#ifndef __DAC_HH__
#define __DAC_HH__

#include <Arduino.h>

#define DAC_N_CHANNELS 2
#define DAC_BIT_LEN_PER_CHANNEL 16
#define DAC_SSC_IRQ_PRIO 1

class DACClass {
public:
	DACClass();
	void setup(uint32_t srate, void(*tx_ready)(uint8_t), bool debug=false);
	void start();
	void stop();
	void end();

	void write(uint16_t value);

	void onService();

private:
	uint32_t *_dataOutAddr;
	void (*_tx_ready)(uint8_t channel);
};

extern DACClass DAC;

#endif /* __DAC_HH__ */
