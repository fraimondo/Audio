/*
 * I2S DAC Library for Arduino DUE
 */

#include <DAC.h>

const PinDescription SSCTXPins[3] = {
	{ PIOA, PIO_PA16B_TD, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT, PIN_ATTR_DIGITAL,
	  NO_ADC, NO_ADC, NOT_ON_PWM, NOT_ON_TIMER }, // A0
	{ PIOA, PIO_PA15B_TF, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT, PIN_ATTR_DIGITAL,
	  NO_ADC, NO_ADC, NOT_ON_PWM, NOT_ON_TIMER }, // PIN 24
	{ PIOA, PIO_PA14B_TK, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT, PIN_ATTR_DIGITAL,
	  NO_ADC, NO_ADC, NOT_ON_PWM, NOT_ON_TIMER }, // PIN 23
}; 

DACClass::DACClass() {
}

void DACClass::setup(uint32_t srate, void(*tx_ready)(uint8_t)) {
	
	_tx_ready = tx_ready;
	pmc_enable_periph_clk(ID_SSC);
	ssc_reset(SSC);

	NVIC_DisableIRQ(SSC_IRQn);
	NVIC_ClearPendingIRQ(SSC_IRQn);
	NVIC_SetPriority(SSC_IRQn, 0);  // most arduino interrupts are set to priority 0.
	NVIC_EnableIRQ(SSC_IRQn);

	for (int i=0; i < 3; i++) {
		PIO_Configure(
			SSCTXPins[i].pPort,
			SSCTXPins[i].ulPinType,
			SSCTXPins[i].ulPin,
			SSCTXPins[i].ulPinConfiguration);
	}

	uint32_t ssc_bit_rate = DAC_BIT_LEN_PER_CHANNEL * srate * DAC_N_CHANNELS;

	ssc_set_clock_divider(SSC, ssc_bit_rate, F_CPU);
	ssc_i2s_set_transmitter(SSC, SSC_I2S_MASTER_OUT, 0, SSC_AUDIO_STERO, 16);
	_dataOutAddr = (uint32_t *)ssc_get_tx_access(SSC);
}

void DACClass::start() {
	ssc_enable_interrupt(SSC, SSC_IER_TXRDY);
	ssc_enable_tx(SSC);
	
	NVIC_DisableIRQ(SSC_IRQn);
	NVIC_ClearPendingIRQ(SSC_IRQn);
	NVIC_SetPriority(SSC_IRQn, DAC_SSC_IRQ_PRIO);
	NVIC_EnableIRQ(SSC_IRQn);
}

void DACClass::stop() {
	ssc_disable_tx(SSC);
	ssc_disable_interrupt(SSC, SSC_IER_TXRDY);
}

void DACClass::end() {
}

void DACClass::write(uint16_t value) {
	*(_dataOutAddr) = value;
}

void DACClass::onService(void){
	uint8_t channel;
	// read and save status -- some bits are cleared on a read 
	uint32_t status = ssc_get_status(SSC);

	if (ssc_is_tx_ready(SSC) == SSC_RC_YES) {
		// The TXSYN event is triggered based on what the start 
		// condition was set to during configuration.  This
		// is usually the left channel, except in the case of the 
		// mono right setup, in which case it's the right.  This
		// may need to change if support for other formats 
		// (e.g. TDM) is added. 
		if (status & SSC_IER_TXSYN) {
			channel = 1;
		} else {
			channel = 2;
		}
		_tx_ready(channel);
	}
}

void SSC_Handler(void) {
	DAC.onService();
}

DACClass DAC;