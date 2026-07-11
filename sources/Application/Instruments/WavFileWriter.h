#ifndef _WAV_FILE_WRITER_H_
#define _WAV_FILE_WRITER_H_

#include "System/FileSystem/FileSystem.h"
#include "Application/Utils/fixed.h"
#include <string>

class WavFileWriter {
public:
    WavFileWriter(const char *path);
    ~WavFileWriter();

    void AddBuffer(fixed *, int size); // size in stereo frames
    void Close();
    bool IsOpen() const;
    const std::string &GetPath() const;

private:
    void writeHeader();
    void writeU16(unsigned short value);
    void writeU32(unsigned int value);
    unsigned int nextDither();

    unsigned int sampleCount_;
    short *buffer_;
    int bufferSize_;
    I_File *file_;
    std::string path_;
    unsigned int ditherState_;
};
#endif
