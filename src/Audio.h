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

class AudioClass {
public:
	AudioClass(DACClass &_dac) : dac(&_dac) { };
	void begin(uint32_t _bSize, int debug=0);
	int prepare(File *toPlay);
	void play();
	void end();

	// virtual size_t write(uint8_t c)                         { /* not implemented */ };
	// virtual size_t write(const uint8_t *data, size_t size)  { return write(reinterpret_cast<const uint32_t*>(data), size/4) * 4; };
	// virtual size_t write(const uint16_t *data, size_t size) { return write(reinterpret_cast<const uint32_t*>(data), size/2) * 2; };
	// virtual size_t write(const int16_t *data, size_t size)  { return write(reinterpret_cast<const uint32_t*>(data), size/2) * 2; };
	// virtual size_t write(const uint32_t *data, size_t size);

	// void debug() {
//		Serial1.print(running-buffer, DEC);
//		Serial1.print(" ");
//		Serial1.print(current-buffer, DEC);
//		Serial1.print(" ");
//		Serial1.print(next-buffer, DEC);
//		Serial1.print(" ");
//		Serial1.println(last-buffer, DEC);
	// }

private:
	void enqueue();
	static void onTransmitEnd(void *me);

	// Buffers definition
	uint32_t __bufferSize;
	int32_t *__WavSamples; // buffer
	int16_t *__WavMonoSamples; // buffer
	riff_header *__RIFFHeader; // RIFF header
	wav_header *__WavHeader; // Wav header
	data_header *__DataHeader; // Wav header


	File * __toPlay;

	uint32_t __halfSample;

	uint32_t volatile __nextBufferSample; // The sample being played next
	uint32_t volatile __SamplesPending;

	DACClass *dac;
	int __debug = 0;

	void fix_samples(int st, int size);
};

extern AudioClass Audio;

#endif
