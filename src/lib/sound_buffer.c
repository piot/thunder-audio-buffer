/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <imprint/allocator.h>
#include <thunder-audio-buffer/sound_buffer.h>

void thunderAudioBufferWrite(
    ThunderAudioBuffer* self, const ThunderSample* samples, size_t sample_count)
{
    CLOG_ASSERT(self->atomSize == sample_count, "Wrong store size:%zu", sample_count)

    if (self->writeIndex == self->readIndex) {
        // return;
    }

    size_t index = self->writeIndex;

    ThunderSampleOutputS16* buffer = self->buffers[index];

    tc_memcpy_octets(buffer, samples, sample_count * sizeof(ThunderSample));

    if (!self->readBuffer) {
        self->readBuffer = self->buffers[0];
        self->readBufferSamplesLeft = self->atomSize;
    }

    self->writeIndex++;
    self->writeIndex %= self->bufferCount;
}

// NOTE: IMPORTANT: You can not allocate mewmory or use mutex when reading!
void thunderAudioBufferRead(
    ThunderAudioBuffer* self, ThunderSampleOutputS16* output, size_t sample_count)
{
    size_t samples_to_read = sample_count;

    if (samples_to_read > self->readBufferSamplesLeft) {
        samples_to_read = self->readBufferSamplesLeft;
    }
    tc_memcpy_type(ThunderSampleOutputS16, output, self->readBuffer, samples_to_read);
    sample_count -= samples_to_read;
    self->readBufferSamplesLeft -= samples_to_read;

    if (self->readBufferSamplesLeft == 0) {
        if (self->writeIndex == self->readIndex) {
            ThunderSampleOutputS16* zero = output + samples_to_read;
            tc_mem_clear_type_n(zero, sample_count);
            return;
        }
        self->readIndex++;
        self->readIndex %= self->bufferCount;
        self->readBuffer = self->buffers[self->readIndex];
        self->readBufferSamplesLeft = self->atomSize;
    } else {
        self->readBuffer += samples_to_read;
    }

    if (sample_count > 0) {
        thunderAudioBufferRead(self, output + samples_to_read, sample_count);
    }
}

void thunderAudioBufferInit(
    ThunderAudioBuffer* self, struct ImprintAllocator* memory, size_t atom_count, size_t atom_size)
{
    self->bufferCount = atom_count;
    self->atomSize = atom_size;
    self->readIndex = (size_t)-1;
    self->writeIndex = 0;
    self->buffers = IMPRINT_CALLOC_TYPE_COUNT(memory, ThunderSampleOutputS16*, self->bufferCount);
    for (size_t i = 0; i < self->bufferCount; ++i) {
        self->buffers[i] = IMPRINT_CALLOC_TYPE_COUNT(memory, ThunderSampleOutputS16, atom_size);
    }
    self->readBuffer = 0;
    self->readBufferSamplesLeft = 0;
}

float thunderAudioBufferPercentageFull(ThunderAudioBuffer* self)
{
    size_t diff = thunderAudioBufferAtomsFull(self);
    return (float)diff / (float)self->bufferCount;
}

size_t thunderAudioBufferAtomsFull(ThunderAudioBuffer* self)
{
    size_t diff;

    if (self->writeIndex == self->readIndex) {
        return 0;
    }

    if (self->writeIndex > self->readIndex) {
        diff = self->writeIndex - self->readIndex;
    } else {
        diff = self->bufferCount - self->readIndex + self->writeIndex;
    }

    return diff;
}

void thunderAudioBufferFree(ThunderAudioBuffer* self)
{
    for (size_t i = 0; i < self->bufferCount; ++i) {
        tc_free(self->buffers[i]);
    }
    tc_free(self->buffers);
    self->bufferCount = 0;
}
