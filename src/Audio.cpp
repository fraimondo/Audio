/*
 * Copyright (c) 2012 Arduino LLC. All right reserved.
 * Audio library for Arduino Due.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include "Audio.h"

#define NEUTRAL_SOUND 0x8000 // = 0x10000 / 2


void AudioClass::begin(uint32_t bSize, int debug) {
	// Allocate a buffer to keep msPreBuffer milliseconds of audio
	__debug = debug;

	__bufferSize = bSize;
	if (__bufferSize  < 1024)
		__bufferSize = 1024;

	if (__debug) {
		Serial.print("Allocated buffer size: ");
		Serial.println(__bufferSize);
	}

	/*Allocate the buffer where the samples are stored*/
	__WavSamples = (int32_t *) malloc(__bufferSize * sizeof(int32_t));
	__RIFFHeader = (riff_header *) malloc(RIFF_HEADER_SIZE);
	__WavHeader = (wav_header *) malloc(WAV_HEADER_SIZE);
	__DataHeader = (data_header *) malloc(DATA_HEADER_SIZE);
	__WavMonoSamples = (int16_t*) __WavSamples;



	// Buffering starts from the beginning
	__SamplesPending = 0;
	__halfSample = __bufferSize / 2;
	__nextBufferSample = 0;

}

void AudioClass::end() {
	dac->end();
	free(__WavSamples);
	free(__RIFFHeader);
	free(__WavHeader);
	free(__DataHeader);
}

void AudioClass::fix_samples(int st, int size) {
	int tag, i, value;
	// for (i = 0; i < size * 2; i++) {
	// 	tag = i % 2 == 0 ? 0 : 0x1000;
	// 	// tag = 0;
	// 	__WavMonoSamples[st * 2 + i] = ((__WavMonoSamples[st * 2 + i] + NEUTRAL_SOUND) >> 4) | tag;
	// }
	tag = 0x1000;
	for (i = 0; i < size * 2; i += 2) {
		// value = (int(sin(2 * 3.14159 * 440 * (i / 2) / 44100) * NEUTRAL_SOUND) + NEUTRAL_SOUND) >> 4;
		value = (__WavMonoSamples[st * 2 + i] + NEUTRAL_SOUND) >> 4;
		__WavMonoSamples[st * 2 + i] = value | tag;
		value = (__WavMonoSamples[st * 2 + i + 1] + NEUTRAL_SOUND) >> 4;
		__WavMonoSamples[st * 2 + i + 1] = value;
	}
}


int AudioClass::prepare(File *toPlay){
	int to_read = 0;
	__SamplesPending = 0;
	__nextBufferSample = 0;
	__toPlay = toPlay;

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

		if (__debug) {
			Serial.println("Reading Data header");
		}

        __toPlay->seek(RIFF_HEADER_SIZE + __WavHeader->data_header_length + 8);
        __toPlay->read(__DataHeader, DATA_HEADER_SIZE);
        __SamplesPending = __DataHeader->data_length / sizeof(int32_t);

		if (__debug) {
			Serial.print("Samples to play: ");
			Serial.println(__SamplesPending);
		}

		to_read = min(__bufferSize, __SamplesPending);

		if (__debug) {
			Serial.print("Reading samples N=");
			Serial.println(to_read);
		}

        __toPlay->read(__WavSamples, to_read  * sizeof(int32_t));
		fix_samples(0, to_read);
        __SamplesPending -= to_read;

		if (__debug) {
			Serial.println("Starting DAC");
		}
		// Start DAC
		dac->begin(VARIANT_MCK / (__WavHeader->sample_rate * 2));
		// dac->begin(VARIANT_MCK / 2);
		dac->setOnTransmitEnd_CB(onTransmitEnd, this);
        return 0;
    } else {
        return -1; // File is empty
    }


}


void AudioClass::play() {
	int to_read = 0;
	uint32_t where_to = 0;
	int available = 0;

	enqueue(); // Queue half buffer

	// Wait till it plays half the buffer
	if (__debug) {
		Serial.print("Waiting (1)... ");
	}
	while (__nextBufferSample == 0);
	if (__debug) {
		Serial.println("done!");
	}

	while ((available = __toPlay->available()) && __SamplesPending > 0) {
		enqueue(); // Queue next half buffers

		// Now start filling the half of the buffer that is not being played
		if (__nextBufferSample == __halfSample) {
			// If we are going to play starting from the middle, read to 0
			where_to = 0;
		} else {
			// If next sample is at 0, read to the second half
			where_to = __halfSample;
		}
		to_read = min(__bufferSize / 2, __SamplesPending);
		if (__debug) {
			Serial.print("Reading ");
			Serial.print(to_read);
			Serial.print(" to ");
			Serial.println(where_to);
		}
		__toPlay->read(&__WavSamples[where_to], to_read * sizeof(int32_t));
		fix_samples(where_to, to_read);
		__SamplesPending -= to_read;
		if (__debug) {
			Serial.print("Samples left to play: ");
			Serial.println(__SamplesPending);
		}
		// Wait till the next sample is the one that we just wrote
		if (__debug) {
			Serial.print("Waiting (2)... ");
		}
		while (__nextBufferSample != where_to);
		if (__debug) {
			Serial.println("done!");
		}

	}

	// Wait till it reaches the next available buffer
	if (__debug) {
		Serial.print("Waiting (3)... ");
	}
	while (__nextBufferSample != where_to);
	dac->end();
	if (__debug) {
		Serial.println("done!");
	}
	__toPlay->close();

}

// size_t AudioClass::write(const uint32_t *data, size_t size) {
// 	const uint32_t TAG = 0x10000000;
// 	int i;
// 	for (i = 0; i < size; i++) {
// 		*next = data[i] | TAG;
// 		next++;
//
// 		if (next == half || next == last) {
//
// 			while (next == running)
// 				;
// 		}
// 	}
//
// 	return i;
// }

void AudioClass::enqueue() {
	if (!dac->canQueue()) {
		// DMA queue full
		return;
	}
	uint32_t rest = __bufferSize / 2;
	if (rest > __SamplesPending) {
		rest = __SamplesPending;
	}

	if (__nextBufferSample == 0) {
		// Enqueue the first half
		if (__debug) {
			Serial.print("Queue first half N=");
			Serial.println(rest);
		}
		dac->queueBuffer((uint32_t *)__WavSamples, rest);
	} else {
		// Enqueue the second half
		if (__debug) {
			Serial.print("Queue second half N=");
			Serial.println(rest);
		}
		dac->queueBuffer((uint32_t *)&__WavSamples[__halfSample], rest);
	}
}

void AudioClass::onTransmitEnd(void *_me) {
	AudioClass *me = reinterpret_cast<AudioClass *> (_me);
	if (me->__nextBufferSample == 0)
		me->__nextBufferSample = me->__halfSample;
	else
		me->__nextBufferSample = 0;
}

AudioClass Audio(DAC);
