/*
 * Copyright (c) 2015 by
 Arturo Guadalupi <a.guadalupi@arduino.cc>
 Angelo Scialabba <a.scialabba@arduino.cc>
 Claudio Indellicati <c.indellicati@arduino.cc> <bitron.it@gmail.com>

 * Audio library for Arduino Zero.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include "AudioI2S.h"

#include <SdFat.h>
// #include <SD.h>
// #include <SPI.h>

#define NEUTRAL_VALUE 0 // = (2^16 / 2)
#define IDLE_VALUE 0

/* Global variables */
volatile bool __StartFlag; // Is started?
volatile uint32_t __SampleIndex; // current played sample
uint32_t __HeadIndex; // current start of buffer
volatile uint32_t __StopAtIndex; // Sample where to stop writing buffer into DAC

int16_t *__WavSamples; // buffer
uint32_t __bufferSize;

uint32_t __samples_callback;
uint32_t __samples_next_callback;
void (*__callback)(void);

void AudioI2SClass::begin(uint32_t bSize, bool debug) {
	__StartFlag = false;
	__bufferSize = bSize;
	__SampleIndex = 0;			//in order to start from the beginning
	__StopAtIndex = -1;
	__SamplesPending = 0;
	__debug = debug;
	if (__debug) {
		Serial.print("Allocated buffer size: ");
		Serial.println(__bufferSize);
	}

	/*Allocate the buffer where the samples are stored*/
	__WavSamples = (int16_t *) malloc(bSize * sizeof(int16_t) * 2);
	__RIFFHeader = (riff_header *) malloc(RIFF_HEADER_SIZE);
	__WavHeader = (wav_header *) malloc(WAV_HEADER_SIZE);
	__DataHeader = (data_header *) malloc(DATA_HEADER_SIZE);

	/*Set callback to none */
	__samples_callback = 0;
	__samples_next_callback = 0;
	__callback = NULL;

}

void AudioI2SClass::end() {
	DAC.end();
	free(__WavSamples);
	free(__RIFFHeader);
	free(__WavHeader);
	free(__DataHeader);
}

#ifdef __cplusplus
extern "C" {
#endif

void I2S_DAC_Handler (uint8_t channel) {
	// Write next sample
	if ((__SampleIndex != __StopAtIndex) && __StartFlag != false) {
		if (channel == 1) {
			DAC.write((__WavSamples[(__SampleIndex << 1)] + NEUTRAL_VALUE));
		} else {
			DAC.write((__WavSamples[(__SampleIndex << 1) + 1] + NEUTRAL_VALUE));
			__SampleIndex++;
		}
	} else {
		DAC.write(IDLE_VALUE);
	}

	if (__samples_callback != 0 && __StartFlag != false &&
			(__SampleIndex != __StopAtIndex)) {
		if (__samples_next_callback == 0 ) {
			__callback();
			__samples_next_callback = __samples_callback;
		}
		__samples_next_callback--;
	}
	// If it was the last sample in the buffer, start again
	if (__SampleIndex == __bufferSize - 1) {
		__SampleIndex = 0;
	}
}

#ifdef __cplusplus
}
#endif


int AudioI2SClass::prepare(File * toPlay){
	__StartFlag = false; // to stop writing from the buffer
	__SampleIndex = 0;	//in order to start from the beginning
	__StopAtIndex = -1;
	__SamplesPending = 0;
	__toPlay = toPlay;


	// Read header + first buffer
	int to_read = 0;
	int available = __toPlay->available();

	if (__debug) {
		Serial.print("Available: ");
		Serial.println(available);
	}

	if (available > 0) {
		if (__debug) {
			Serial.println("Reading RIFF header");
		}
		// READ RIFF Header
		__toPlay->read(__RIFFHeader, RIFF_HEADER_SIZE);

		if (__debug) {
			Serial.println("Reading WAV header");
		}
		// Read WAV Header
		__toPlay->read(__WavHeader, WAV_HEADER_SIZE);

		DAC.setup(__WavHeader->sample_rate, I2S_DAC_Handler);

		if (__debug) {
			Serial.println("Reading Data header");
		}
		__toPlay->seek(RIFF_HEADER_SIZE + __WavHeader->data_header_length + 8);

		__toPlay->read(__DataHeader, DATA_HEADER_SIZE);
		__SamplesPending = __DataHeader->data_length / sizeof(int16_t) / 2;

		if (__debug) {
			Serial.print("Samples to play: ");
			Serial.println(__SamplesPending);
		}

		to_read = min(__bufferSize, __SamplesPending);

		if (__debug) {
			Serial.print("Reading samples N=");
			Serial.println(to_read);
		}
		__toPlay->read(__WavSamples, to_read  * sizeof(int16_t) * 2);
		__HeadIndex = 0;
		__SamplesPending -= to_read;
		if (__debug) {
			Serial.println("Starting DAC");
		}
		DAC.start(); //start the interruptions, it will do nothing until __StartFlag is true
		return 0;
	} else {
		return -1; // File is empty
	}
}

void AudioI2SClass::play() {
	int to_read = 0;
	int available = 0;
	uint32_t current__SampleIndex;
	__StartFlag = true;
	while ((available = __toPlay->available()) && __SamplesPending > 0) {
		available = available / sizeof(int16_t) / 2; // Convert from bytes to samples
		current__SampleIndex = __SampleIndex;

		if (current__SampleIndex > __HeadIndex) {
			to_read = min(current__SampleIndex - __HeadIndex, __SamplesPending);
			if (to_read > 0) { // First time this will be 0
				if (to_read > available) {
					// If we have less bytes to read, read the missing and stop
					to_read = available;
				}
				__toPlay->read(&__WavSamples[__HeadIndex << 1], to_read * sizeof(int16_t)  * 2);
				__SamplesPending -= to_read;
				__StopAtIndex = to_read + __HeadIndex-1;
				__HeadIndex = current__SampleIndex;
			}
		} else if (current__SampleIndex < __HeadIndex) {
			to_read = min(__bufferSize-1 - __HeadIndex, __SamplesPending);
			if (to_read > 0) {
				if (to_read > available) {
					// If we have less bytes to read, read the missing and stop
					to_read = available;
					__StopAtIndex = to_read + __HeadIndex - 1;
				} else {
					// If not, we have less bytes available
					available -= to_read;
				}
				__toPlay->read(&__WavSamples[__HeadIndex << 1], to_read  * sizeof(int16_t) * 2);
				__SamplesPending -= to_read;
				if (available > 0) {
					to_read = min(current__SampleIndex, __SamplesPending);
					if (to_read > 0) {
						if (to_read > available) {
							// If we have less bytes to read, read the missing and stop
							to_read = available;
						}
						__toPlay->read(__WavSamples, to_read  * sizeof(int16_t) * 2);
						__SamplesPending -= to_read;
						__StopAtIndex = to_read - 1;
					}
				}
				__HeadIndex = current__SampleIndex;
			}
		}
	}
	if (__debug) {
		Serial.println("Waiting for last samples");
	}
	while (__SampleIndex != __StopAtIndex) {
		delay(1);
	}

	if (__debug) {
		Serial.println("done!");
	}
}


void AudioI2SClass::set_callback(void (*func)(void), uint32_t n_samples) {
	__callback = func;
	__samples_callback = n_samples;
	__samples_next_callback = 0;
}

AudioI2SClass Audio;

