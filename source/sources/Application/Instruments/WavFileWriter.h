#ifndef _WAV_FILE_WRITER_H_
#define _WAV_FILE_WRITER_H_

#include "System/FileSystem/FileSystem.h"
#include "Application/Utils/fixed.h"

class WavFileWriter {
public:
	WavFileWriter(const char *path) ;
	~WavFileWriter() ;
	void AddBuffer(fixed *,int size) ; // size in samples
	void Close() ;
	// MULTITRACK_EXPORT (bacon-1.5, item 8): true when the underlying file
	// was opened successfully (the caller can abort export on failure).
	bool IsOpen() { return file_ != 0 ; } ;
private:
	int sampleCount_ ;
	short *buffer_ ;
	int bufferSize_ ;
	I_File *file_ ;
} ;
#endif
