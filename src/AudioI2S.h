/*
 * Copyright (c) 2012 Arduino LLC. All right reserved.
 * Audio library for Arduino Due.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#ifndef AUDIO_H
#define AUDIO_H

#include "Arduino.h"
#include <SdFat.h>
#include "DAC.h"
#include "wav.h"

class AudioI2SClass {
public:
	AudioI2SClass() {};
	void begin(uint32_t bSize, bool debug=0);
	int prepare(File *toPlay);
	void play() ;
	void end();

	/* Call a function every n_samples */
	void set_callback(void (*func)(void), uint32_t n_samples);

private:
	File * __toPlay;

	riff_header *__RIFFHeader; // RIFF header
	wav_header *__WavHeader; // Wav header
	data_header *__DataHeader; // Wav header
	uint32_t __SamplesPending;

	bool __debug = false;
};

extern AudioI2SClass Audio;

#endif
