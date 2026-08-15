
#include "UnixFileSystem.h"
#include "System/Console/Trace.h"
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#ifdef _64BIT
#include <dirent.h>
#else
#include <sys/dir.h>
#endif
#include <sys/stat.h>
#ifdef __linux__
#include <sys/statvfs.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <string>

#include "Application/Utils/wildcard.h"

static std::string unixLowerName(const char *src) {
	std::string out = src ? src : "" ;
	for (size_t i=0;i<out.size();i++) {
		out[i]=(char)tolower((unsigned char)out[i]) ;
	}
	return out ;
}

static bool unixShouldSkipHiddenProjectEntry(const std::string &name) {
	return (!name.empty() && name[0]=='.' && name!="..") ;
}

static std::string unixJoinPath(const char *base,const std::string &leaf) {
	std::string fullpath = base ? base : "" ;
	if (!fullpath.empty() && fullpath[fullpath.size()-1]!='/') {
		fullpath += "/" ;
	}
	fullpath += leaf ;
	return fullpath ;
}

UnixDir::UnixDir(const char *path):I_Dir(path) {
} ;

void UnixDir::GetContent(char *mask) {

	Empty() ;

	DIR* directory; 
	struct dirent* entry; 

	directory = opendir (path_); 
	if (directory == NULL) {
		Trace::Error("Failed to open %s",path_) ;
		return ;
	}

	const char *pattern = mask ? mask : "*" ;
	while ((entry = readdir (directory)) != NULL) {
		std::string name = entry->d_name ? entry->d_name : "" ;
		std::string lowered = unixLowerName(entry->d_name) ;
		if (wildcardfit(pattern,lowered.c_str())) {
			std::string fullpath = unixJoinPath(path_,name) ;
			Path *path=new Path(fullpath.c_str()) ;
			Insert(path) ;
		}
	} ;   
	closedir (directory);
	
};

void UnixDir::GetProjectContent() {
	
	Empty() ;
	const char* mask = (const char *) "*";
	DIR* directory; 
	struct dirent* entry; 
	
	directory = opendir (path_); 
	if (directory == NULL) {
		Trace::Error("Failed to open %s",path_) ;
		return ;
	}
	
	while ((entry = readdir (directory)) != NULL) {
		std::string name = entry->d_name ? entry->d_name : "" ;
		std::string lowered = unixLowerName(entry->d_name) ;
		if (wildcardfit(mask,lowered.c_str())) {
			if(!unixShouldSkipHiddenProjectEntry(name)){
				std::string fullpath = unixJoinPath(path_,name) ;
				Path *path=new Path(fullpath.c_str()) ;
				Insert(path) ;
			}
		}
	} ;   
	closedir (directory);
} ;

UnixFile::UnixFile(FILE *file) {
	file_=file ;
} ;

int UnixFile::Read(void *ptr,int size, int nmemb) {
	return fread(ptr,size,nmemb,file_) ;
} ;

int UnixFile::Write(const void *ptr,int size, int nmemb) {
	return fwrite(ptr,size,nmemb,file_) ;
} ;

void UnixFile::Printf(const char *fmt, ...) {
     va_list args;
     va_start(args,fmt);

     vfprintf(file_,fmt,args ); 
     va_end(args);
} ;

void UnixFile::Seek(long offset,int whence) {
	fseek(file_,offset,whence) ;
} ;

long UnixFile::Tell() {
	return ftell(file_) ;
} ;
void UnixFile::Close() {
	fflush(file_) ;
#ifndef _64BIT
	fsync(fileno(file_)) ;
#endif
	fclose(file_) ;
} ;

UnixFileSystem::UnixFileSystem() {
}

I_File *UnixFileSystem::Open(const char *path,char *mode) {
	char *rmode ;
	switch(*mode) {
        case 'r':
            rmode=(char *)"rb" ;
            break ;
        case 'w':
            rmode=(char *)"wb" ;
            break ;
        default:
            Trace::Error("Invalid mode: %s",mode) ;
            return 0 ;
    }

	FILE *file=fopen(path,rmode) ;
	UnixFile *wFile=0 ;
	if (file) {
		wFile=new UnixFile(file) ;
	}
	return wFile ;
} ;

I_Dir *UnixFileSystem::Open(const char *path) {
    return new UnixDir(path) ;
} ;

FileType UnixFileSystem::GetFileType(const char* path) {

	struct stat attributes ;
	if (stat(path,&attributes)==0) {
		if (attributes.st_mode&S_IFDIR) return FT_DIR ;
		if (attributes.st_mode&S_IFREG) return FT_FILE ;
	}
	return FT_UNKNOWN ;

} ;

void UnixFileSystem::Delete(const char *path) {
	remove(path) ;
} ;

// MULTITRACK_EXPORT (bacon-1.5, item 8): free bytes on the filesystem that
// hosts `path` (statvfs), or -1 when unavailable.  Only used as a best-effort
// guard before explicit WAV export.
long long UnixFileSystem::GetFreeSpace(const char *path) {
#ifdef __linux__
	struct statvfs st ;
	if (statvfs(path ? path : ".", &st) != 0) return -1 ;
	if (st.f_bfree == 0 || st.f_frsize == 0) return -1 ;
	return (long long)st.f_bfree * (long long)st.f_frsize ;
#else
	return -1 ;
#endif
} ;

Result UnixFileSystem::MakeDir(const char *path) {
  
	int success = mkdir(path,S_IRWXU) ;
  if (success <0)
  {
    std::ostringstream oss;
    oss << "Could not create path " << path << " (errno:" << errno << ")";
    return Result(oss.str());
  }
  return Result::NoError;	
} ;	
