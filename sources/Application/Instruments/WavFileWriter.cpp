#include "WavFileWriter.h"
#include "System/Console/Trace.h"
#include "System/System/typedefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LGPT_WAV_RENDER_SAMPLE_RATE 44100
#define LGPT_WAV_RENDER_CHANNELS 2
#define LGPT_WAV_RENDER_BITS 16

static void wavDebugLine(const char *msg, const char *path) {
    I_File *fp = FileSystem::GetInstance()->Open("/mnt/sdcard/lgpt/wav_export_debug.log", "a");
    if (!fp) return;
    fp->Printf("WavFileWriter: %s %s\n", msg ? msg : "", path ? path : "");
    fp->Close();
    SAFE_DELETE(fp);
}

WavFileWriter::WavFileWriter(const char *path)
    : sampleCount_(0),
      buffer_(0),
      bufferSize_(0),
      file_(0),
      path_(path ? path : ""),
      ditherState_(0x12345678u) {
    Path filePath(path_.c_str());
    std::string resolved = filePath.GetPath();
    file_ = FileSystem::GetInstance()->Open(resolved.c_str(), "wb");
    if (file_) {
        writeHeader();
        Trace::Log("WavFileWriter", "opened %s", resolved.c_str());
        wavDebugLine("opened", resolved.c_str());
    } else {
        Trace::Log("WavFileWriter", "failed to open %s", resolved.c_str());
        wavDebugLine("failed_to_open", resolved.c_str());
    }
}

WavFileWriter::~WavFileWriter() { Close(); }

bool WavFileWriter::IsOpen() const { return file_ != 0; }

const std::string &WavFileWriter::GetPath() const { return path_; }

unsigned int WavFileWriter::nextDither() {
    ditherState_ = ditherState_ * 1664525u + 1013904223u;
    return ditherState_;
}

void WavFileWriter::writeU16(unsigned short value) {
    unsigned short v = Swap16(value);
    file_->Write(&v, 1, 2);
}

void WavFileWriter::writeU32(unsigned int value) {
    unsigned int v = Swap32((int)value);
    file_->Write(&v, 1, 4);
}

void WavFileWriter::writeHeader() {
    unsigned int chunk;

    chunk = Swap32(0x46464952); // RIFF
    file_->Write(&chunk, 1, 4);
    writeU32(0); // patched in Close()

    chunk = Swap32(0x45564157); // WAVE
    file_->Write(&chunk, 1, 4);

    chunk = Swap32(0x20746D66); // fmt 
    file_->Write(&chunk, 1, 4);
    writeU32(16);
    writeU16(1); // PCM
    writeU16(LGPT_WAV_RENDER_CHANNELS);
    writeU32(LGPT_WAV_RENDER_SAMPLE_RATE);
    writeU32(LGPT_WAV_RENDER_SAMPLE_RATE * LGPT_WAV_RENDER_CHANNELS * (LGPT_WAV_RENDER_BITS / 8));
    writeU16(LGPT_WAV_RENDER_CHANNELS * (LGPT_WAV_RENDER_BITS / 8));
    writeU16(LGPT_WAV_RENDER_BITS);

    chunk = Swap32(0x61746164); // data
    file_->Write(&chunk, 1, 4);
    writeU32(0); // patched in Close()
}

void WavFileWriter::AddBuffer(fixed *bufferIn, int size) {
    if (!file_ || !bufferIn || size <= 0) return;

    if (size > bufferSize_) {
        SAFE_FREE(buffer_);
        buffer_ = (short *)malloc(size * LGPT_WAV_RENDER_CHANNELS * sizeof(short));
        bufferSize_ = size;
    }

    if (!buffer_) return;

    short *dst = buffer_;
    fixed *src = bufferIn;

    const float exportGain = 0.97723722f; /* -0.2 dB final safety margin. */

    for (int i = 0; i < size * LGPT_WAV_RENDER_CHANNELS; i++) {
        float v = fp2fl(*src++) * exportGain;

        /* Very small deterministic TPDF dither before 16-bit quantization. */
        float d1 = (float)(nextDither() & 0xFFFF) / 65535.0f;
        float d2 = (float)(nextDither() & 0xFFFF) / 65535.0f;
        v += (d1 - d2) * 0.25f;

        if (v > 32767.0f) {
            v = 32767.0f;
        } else if (v < -32768.0f) {
            v = -32768.0f;
        }

        int out = (v >= 0.0f) ? (int)(v + 0.5f) : (int)(v - 0.5f);
        if (out > 32767) out = 32767;
        if (out < -32768) out = -32768;
        *dst++ = Swap16((short)out);
    }

    int written = file_->Write(buffer_, sizeof(short), size * LGPT_WAV_RENDER_CHANNELS);
    if (written != size * LGPT_WAV_RENDER_CHANNELS) {
        Trace::Log("WavFileWriter", "short write path=%s written=%d expected=%d", path_.c_str(), written, size * LGPT_WAV_RENDER_CHANNELS);
        wavDebugLine("short_write", path_.c_str());
    }
    sampleCount_ += (unsigned int)size;
}

void WavFileWriter::Close() {
    if (!file_) return;

    unsigned int dataBytes = sampleCount_ * LGPT_WAV_RENDER_CHANNELS * (LGPT_WAV_RENDER_BITS / 8);
    unsigned int riffBytes = 36 + dataBytes;

    file_->Seek(4, SEEK_SET);
    writeU32(riffBytes);

    file_->Seek(40, SEEK_SET);
    writeU32(dataBytes);

    file_->Seek(0, SEEK_END);
    file_->Close();
    SAFE_DELETE(file_);
    SAFE_FREE(buffer_);
    bufferSize_ = 0;
    sampleCount_ = 0;

    Trace::Log("WavFileWriter", "closed %s", path_.c_str());
    wavDebugLine("closed", path_.c_str());
}
