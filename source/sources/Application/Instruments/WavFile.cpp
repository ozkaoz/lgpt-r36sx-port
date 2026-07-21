
#include "WavFile.h"
#include "System/Console/Trace.h"
#include "Foundation/Types/Types.h"
#include "Services/Time/TimeService.h"
#include "Application/Model/Config.h"
#include <stdlib.h>

int WavFile::bufferChunkSize_=-1 ;
bool WavFile::initChunkSize_=true ;

short Swap16 (short from)
{
#ifdef __ppc__
	short result;
	((char*)&result)[0] = ((char*)&from)[1];
	((char*)&result)[1] = ((char*)&from)[0];
	return  result;
#else
	return from;
#endif	
}

int Swap32 (int from)
{
#ifdef __ppc__
	int result;
	((char*)&result)[0] = ((char*)&from)[3];
	((char*)&result)[1] = ((char*)&from)[2];
	((char*)&result)[2] = ((char*)&from)[1];
	((char*)&result)[3] = ((char*)&from)[0];			 
	return  result;
#else
	return from;
#endif 	
}


WavFile::WavFile(I_File *file) {
	if (initChunkSize_) {
		const char *size=Config::GetInstance()->GetValue("SAMPLELOADCHUNKSIZE") ;
		if (size) {
			bufferChunkSize_=atoi(size) ;
		}
		initChunkSize_=false;
	}
	samples_=0 ;
	size_=0 ;
	readBuffer_=0 ;
	readBufferSize_=0 ;
	sampleBufferSize_=0 ;
	file_=file ;
} ;

WavFile::~WavFile() {
	if (file_) {
		file_->Close() ;
		delete file_ ;
	}
	SAFE_FREE(samples_) ;
	SAFE_FREE(readBuffer_) ;
} ;

WavFile *WavFile::Open(const char *path) {

    // open file

	FileSystem *fs=FileSystem::GetInstance() ;
	I_File *file=fs->Open(path,"r") ;
	
	if (!file) return 0 ;

	file->Seek(0,SEEK_END) ;
	long fileSize=file->Tell() ;
	file->Seek(0,SEEK_SET) ;
	if (fileSize<44) {
		Trace::Error("WAV file too small: %ld bytes",fileSize) ;
		file->Close() ;
		delete file ;
		return 0 ;
	}

	WavFile *wav=new WavFile(file) ;

        
        // Get data
        
/*        file->Seek(0,SEEK_SET) ;
        file->Read(fileBuffer,filesize,1) ;
        uchar *ptr=fileBuffer ;*/
        
//Trace::Dump("Loading sample from %s",path) ;

	long position=0 ;

	// Read 'RIFF'

	unsigned int chunk ;

	position+=wav->readBlock(position,4) ;
	memcpy(&chunk,wav->readBuffer_,4) ;
	chunk = Swap32(chunk);
		
	if (chunk!=0x46464952) {
		Trace::Error("Bad RIFF format %x",chunk) ;
		delete(wav) ;
		return 0 ;
	}


	// Read size

	unsigned int size ;
	position+=wav->readBlock(position,4) ;
	memcpy(&size,wav->readBuffer_,4) ;
	size = Swap32(size);

	// Read WAVE

	position+=wav->readBlock(position,4) ;
	memcpy(&chunk,wav->readBuffer_,4) ;
	chunk = Swap32(chunk);

	if (chunk!=0x45564157) {
		Trace::Error("Bad WAV format") ;
		delete wav ;
		return 0 ;
	}

    // Read fmt or JUNK

    position += wav->readBlock(position, 4);
    memcpy(&chunk,wav->readBuffer_,4) ;
	chunk = Swap32(chunk);

        // Read (possible) JUNK

    if (chunk == 0x4b4e554a) {
        if (position+4>fileSize) {
            Trace::Error("Truncated WAV JUNK header") ;
            delete wav ;
            return 0 ;
        }
        position+=wav->readBlock(position,4) ;
        memcpy(&size, wav->readBuffer_,4) ;
        size = Swap32(size) ;
        Trace::Debug("WavFile::Open(): skipping JUNK with size=%d", size);
        unsigned long paddedJunk=(unsigned long)size+((unsigned long)size&1UL) ;
        if (fileSize-position<4 ||
            paddedJunk>(unsigned long)(fileSize-position-4)) {
            Trace::Error("Invalid WAV JUNK size: %u",size) ;
            delete wav ;
            return 0 ;
        }
        position+=(long)paddedJunk;
        position += wav->readBlock(position, 4);
        memcpy(&chunk,wav->readBuffer_,4) ;
		chunk = Swap32(chunk);
    }

    // Read fmt

    if (chunk!=0x20746D66) {
		Trace::Error("Bad WAV/fmt format") ;
		delete wav ;
		return 0 ;
	}

	// Read subchunk size

	position+=wav->readBlock(position,4) ;
	memcpy(&size,wav->readBuffer_,4) ;
	size = Swap32(size);

	unsigned long paddedFmt=(unsigned long)size+((unsigned long)size&1UL) ;
	if (size<16 || fileSize-position<0 ||
	    paddedFmt>(unsigned long)(fileSize-position)) {
		Trace::Error("Bad fmt size format: %u",size) ;
		delete wav ;
		return 0 ;
	}
	int offset=size-16 ;

	// Read compression

	unsigned short comp ;
	position+=wav->readBlock(position,2) ;
	memcpy(&comp,wav->readBuffer_,2) ;
	comp = Swap16(comp);

	if (comp!=1) {
		Trace::Error("Unsupported compression") ;
		delete wav ;
		return 0 ;
	}

	// Read NumChannels (mono/Stereo)

	unsigned short nChannels ;
	position+=wav->readBlock(position,2) ;
	memcpy(&nChannels,wav->readBuffer_,2) ;
	nChannels = Swap16(nChannels);
	if ((nChannels!=1)&&(nChannels!=2)) {
		Trace::Error("Only mono/stereo WAV supported: channels=%d",nChannels) ;
		delete wav ;
		return 0 ;
	}

	// Read Sample rate 

	unsigned int sampleRate ;

	position+=wav->readBlock(position,4) ;
	memcpy(&sampleRate,wav->readBuffer_,4) ;
	sampleRate = Swap32(sampleRate);
	if (sampleRate==0) {
		Trace::Error("Invalid WAV sample rate") ;
		delete wav ;
		return 0 ;
	}

	// Skip byteRate & blockalign

	position+=6 ;

	short bitPerSample ;
	position+=wav->readBlock(position,2) ;
	memcpy(&bitPerSample,wav->readBuffer_,2) ;
	bitPerSample = Swap16(bitPerSample);
		
	if ((bitPerSample!=16)&&(bitPerSample!=8)) {
		Trace::Error("Only 8/16 bit supported") ;
		delete wav ;
		return 0 ;
	} ;
	bitPerSample/=8 ;
	wav->bytePerSample_=bitPerSample ;

	// some bad files have bigger chunks

	if (offset) {
		position+=offset ;
	}
	if (size&1U) position++ ; // RIFF chunks are word-aligned.

	// read data subchunk header
	//Trace::Dump("data subch") ;

	if (position+4>fileSize) {
		Trace::Error("Missing WAV data chunk") ;
		delete wav ;
		return 0 ;
	}
	position+=wav->readBlock(position,4) ;
	memcpy(&chunk,wav->readBuffer_,4) ;
	chunk = Swap32(chunk);
	

	while (chunk!=0x61746164) {
		if (position+4>fileSize) {
			Trace::Error("Truncated WAV chunk header") ;
			delete wav ;
			return 0 ;
		}
		position+=wav->readBlock(position,4) ;
		memcpy(&size,wav->readBuffer_,4) ;
		size = Swap32(size);

		unsigned long paddedSize=(unsigned long)size+((unsigned long)size&1UL) ;
		if (fileSize-position<4 ||
		    paddedSize>(unsigned long)(fileSize-position-4)) {
			Trace::Error("Invalid WAV chunk size: %u",size) ;
			delete wav ;
			return 0 ;
		}
		position+=(long)paddedSize ;
		position+=wav->readBlock(position,4) ;
		memcpy(&chunk,wav->readBuffer_,4) ;
		chunk = Swap32(chunk);
	}

        wav->sampleRate_=sampleRate ;
       	wav->channelCount_=nChannels ;

	// Read data size in byte

	if (position+4>fileSize) {
		Trace::Error("Truncated WAV data header") ;
		delete wav ;
		return 0 ;
	}
	position+=wav->readBlock(position,4) ;
	memcpy(&size,wav->readBuffer_,4) ;
	size = Swap32(size);

	unsigned int frameBytes=(unsigned int)nChannels*(unsigned int)bitPerSample ;
	if (frameBytes==0 || size==0 || (size%frameBytes)!=0 ||
	    fileSize-position<0 ||
	    (unsigned long)size>(unsigned long)(fileSize-position)) {
		Trace::Error("Bad WAV data: bytes=%u channels=%d bytesPerSample=%d",size,nChannels,bitPerSample) ;
		delete wav ;
		return 0 ;
	}
	wav->size_=size/frameBytes ; // Size in sample frames
	if (wav->size_<=0) {
		Trace::Error("Bad WAV sample count: data bytes=%u",size) ;
		delete wav ;
		return 0 ;
	}

	wav->dataPosition_=position ;

	return wav ;
} ; 

void *WavFile::GetSampleBuffer(int note) {
	return samples_ ;
} ;

int WavFile::GetSize(int note) {
	return size_ ;
} ;

int WavFile::GetChannelCount(int note) {
    return channelCount_ ;
} ;

int WavFile::GetSampleRate(int note) {
    return sampleRate_ ;
} ;

long WavFile::readBlock(long start,long size) {
    if (!file_ || start<0 || size<=0) return 0;

    if (size>readBufferSize_) {
        SAFE_FREE(readBuffer_);
        readBuffer_=SYS_MALLOC(size);
        if (!readBuffer_) {
            readBufferSize_=0;
            Trace::Error("Failed to allocate WAV read buffer of size %ld",size);
            return 0;
        }
        readBufferSize_=(int)size;
    }

    file_->Seek(start,SEEK_SET);
    int bytesRead=file_->Read(readBuffer_,1,(int)size);
    if (bytesRead!=(int)size) {
        Trace::Error("Short WAV read: expected %ld got %d",size,bytesRead);
        return bytesRead>0?bytesRead:0;
    }
    return bytesRead;
}


bool WavFile::GetBuffer(long start,long size) {

    if (start<0 || size<=0 || start>=size_ || size>(long)size_-start) {
        Trace::Error("Invalid WAV buffer range start=%ld size=%ld total=%d",
                     start,size,size_);
        return false;
    }
    if (channelCount_<=0 || channelCount_>2 ||
        (bytePerSample_!=1 && bytePerSample_!=2)) {
        Trace::Error("Invalid WAV format channels=%d bytesPerSample=%d",
                     channelCount_,bytePerSample_);
        return false;
    }

    const long sampleWords=size*(long)channelCount_;
    const long sampleBufferBytes=sampleWords*(long)sizeof(short);
    const long fileBufferBytes=sampleWords*(long)bytePerSample_;
    if (sampleBufferBytes<=0 || sampleBufferBytes>0x7fffffffL ||
        fileBufferBytes<=0 || fileBufferBytes>0x7fffffffL) {
        Trace::Error("WAV buffer size overflow");
        return false;
    }

    if (sampleBufferBytes>sampleBufferSize_) {
        SAFE_FREE(samples_);
        samples_=(short *)SYS_MALLOC((int)sampleBufferBytes);
        if (!samples_) {
            sampleBufferSize_=0;
            Trace::Error("Failed to allocate WAV sample buffer of %ld bytes",
                         sampleBufferBytes);
            return false;
        }
        sampleBufferSize_=(int)sampleBufferBytes;
    }

    const long bufferStartBase=dataPosition_+
        start*(long)channelCount_*(long)bytePerSample_;
    long remaining=fileBufferBytes;
    long sourceOffset=0;
    int readSize=(bufferChunkSize_>0)?
        bufferChunkSize_:(remaining>4096?4096:(int)remaining);

    unsigned char *raw=(unsigned char *)samples_;
    while (remaining>0) {
        int chunk=(remaining>readSize)?readSize:(int)remaining;
        long got=readBlock(bufferStartBase+sourceOffset,chunk);
        if (got!=chunk || !readBuffer_) {
            Trace::Error("Failed to read complete WAV sample data");
            return false;
        }
        memcpy(raw+sourceOffset,readBuffer_,chunk);
        remaining-=chunk;
        sourceOffset+=chunk;
        if (bufferChunkSize_>0) TimeService::GetInstance()->Sleep(1);
    }

    if (bytePerSample_==1) {
        // Expand backwards because source bytes and destination shorts share
        // the same allocation. Include every channel, not only every frame.
        unsigned char *src=(unsigned char *)samples_;
        short *dst=samples_;
        for (long i=sampleWords-1;i>=0;i--) {
            dst[i]=(short)(((int)src[i]-128)*256);
        }
    } else {
        short *dst=samples_;
        for (long i=0;i<sampleWords;i++) dst[i]=Swap16(dst[i]);
    }

    return true;
}

void WavFile::Close() {
	file_->Close() ;
	SAFE_DELETE(file_) ;
	SAFE_FREE(readBuffer_) ;
	readBufferSize_=0 ;
} ;

int WavFile::GetRootNote(int note) {
	return 60 ;
} 


static void lgptWavWriteU16(I_File *file, unsigned short value) {
    unsigned short v = Swap16(value);
    file->Write(&v, 1, 2);
}

static void lgptWavWriteU32(I_File *file, unsigned int value) {
    unsigned int v = Swap32((int)value);
    file->Write(&v, 1, 4);
}

bool WavFile::ReplaceBuffer(short *samples, int frameCount, int channelCount, int sampleRate) {
    if (!samples || frameCount <= 0 || channelCount <= 0 || sampleRate <= 0) return false;
    int bytes = frameCount * channelCount * 2;
    short *next = (short *)SYS_MALLOC(bytes);
    if (!next) return false;
    memcpy(next, samples, bytes);
    SAFE_FREE(samples_);
    samples_ = next;
    sampleBufferSize_ = bytes;
    size_ = frameCount;
    channelCount_ = channelCount;
    sampleRate_ = sampleRate;
    bytePerSample_ = 2;
    dataPosition_ = 44;
    return true;
}

bool WavFile::SaveBufferToPath(const char *path) {
    if (!path || !samples_ || size_ <= 0 || channelCount_ <= 0 || sampleRate_ <= 0) return false;

    Path outPath(path);
    std::string resolvedPath = outPath.GetPath();
    if (resolvedPath.empty()) return false;

    /* U2.19: destructive sample edits pass logical paths such as samples:break.wav.
       The loaded WAV was opened through Path::GetPath(), so writes must do the same.
       Also close the read handle before opening the same file for writing. */
    if (file_) {
        file_->Close();
        delete file_;
        file_ = 0;
    }

    FileSystem *fs = FileSystem::GetInstance();
    if (!fs) return false;
    I_File *file = fs->Open(resolvedPath.c_str(), "wb");
    if (!file) file = fs->Open(resolvedPath.c_str(), "w");
    if (!file) return false;

    unsigned int dataBytes = (unsigned int)(size_ * channelCount_ * 2);
    unsigned int riffBytes = 36 + dataBytes;
    unsigned int byteRate = (unsigned int)(sampleRate_ * channelCount_ * 2);
    unsigned short blockAlign = (unsigned short)(channelCount_ * 2);

    unsigned int chunk = Swap32(0x46464952); /* RIFF */
    file->Write(&chunk, 1, 4);
    lgptWavWriteU32(file, riffBytes);
    chunk = Swap32(0x45564157); /* WAVE */
    file->Write(&chunk, 1, 4);
    chunk = Swap32(0x20746D66); /* fmt  */
    file->Write(&chunk, 1, 4);
    lgptWavWriteU32(file, 16);
    lgptWavWriteU16(file, 1);
    lgptWavWriteU16(file, (unsigned short)channelCount_);
    lgptWavWriteU32(file, (unsigned int)sampleRate_);
    lgptWavWriteU32(file, byteRate);
    lgptWavWriteU16(file, blockAlign);
    lgptWavWriteU16(file, 16);
    chunk = Swap32(0x61746164); /* data */
    file->Write(&chunk, 1, 4);
    lgptWavWriteU32(file, dataBytes);
    file->Write(samples_, 2, size_ * channelCount_);
    file->Close();
    delete file;
    return true;
}

