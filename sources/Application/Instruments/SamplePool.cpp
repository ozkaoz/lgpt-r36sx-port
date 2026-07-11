#include "SamplePool.h"
#include <string.h>
#include <stdlib.h>
#include "System/Console/Trace.h"
#include "Application/Persistency/PersistencyService.h" 
#include "System/io/Status.h"
#include <string>
#include <time.h>
#include "SoundFontSample.h"
#include "SoundFontPreset.h"
#include "SoundFontManager.h"
#include "Application/Model/Config.h"

#define SAMPLE_LIB "root:samplelib" 

SamplePool::SamplePool() {
	for (int i=0;i<MAX_PIG_SAMPLES;i++) {
		names_[i]=NULL ;
		wav_[i]=NULL ;
	} ;
	count_=0 ;
} ;

SamplePool::~SamplePool() {
	for (int i=0;i<MAX_PIG_SAMPLES;i++) {
		SAFE_DELETE(wav_[i]) ;
		SAFE_FREE(names_[i]) ;
	} ;
} ;

const char *SamplePool::GetSampleLib() {
	Config *config=Config::GetInstance() ;
	const char *lib=config->GetValue("SAMPLELIB") ;
	return lib?lib:SAMPLE_LIB ;
} 

void SamplePool::Reset() {
	count_=0 ;
	for (int i=0;i<MAX_PIG_SAMPLES;i++) {
		SAFE_DELETE(wav_[i]) ;
		SAFE_FREE(names_[i]) ;
	} ;
	SoundFontManager::GetInstance()->Reset() ;
} ;

void SamplePool::Load() {

	Path sampleDir("samples:");

	I_Dir *dir=FileSystem::GetInstance()->Open(sampleDir.GetPath().c_str()) ;
	if (!dir) {
		return ;
	}

	// First, find all wav files

	dir->GetContent("*.wav") ;
	IteratorPtr<Path> it(dir->GetIterator()) ;
	count_=0 ;

	for(it->Begin();!it->IsDone();it->Next()) {
		Path &path=it->CurrentItem() ;
        Trace::Log("Load", "%s", path.GetCanonicalPath().c_str());
        loadSample(path.GetPath().c_str()) ;
		if (count_==MAX_PIG_SAMPLES) {
		   Trace::Error("Warning maximum sample count reached") ;
		   break ;
		} ;

	} ;

	// now, let's look at soundfonts

	dir->GetContent("*.sf2") ;
	IteratorPtr<Path> it2(dir->GetIterator()) ;

	for(it2->Begin();!it2->IsDone();it2->Next()) {
		Path &path=it2->CurrentItem() ;
		loadSoundFont(path.GetPath().c_str()) ;
	} ;

	delete dir ;

	// now sort the samples
    Sort();
} ;

void SamplePool::Sort() {
    int rest=count_;
	while(rest>0) {
        int index = 0;
        for (int i=1;i<rest;i++) {
			if (strcmp(names_[i],names_[index])>0) {
                index = i;
            }
        }
        SoundSource *tWav = wav_[index];
		char *tName = names_[index];
		wav_[index] = wav_[rest-1];
		names_[index] = names_[rest-1];
		wav_[rest-1] = tWav;
        names_[rest - 1] = tName;
        rest--;
	}
}

int SamplePool::getIndexOf(const char *name) {
    for (int i=0;i<count_;i++) {
		if (strcmp(names_[i], name)==0) {
			return i;
		}
	}
	return -1;
}

SoundSource *SamplePool::GetSource(int i) {
	return wav_[i] ;
} ;

char **SamplePool::GetNameList() {
	return names_ ;
} ;

int SamplePool::GetNameListSize() {
	return count_ ;
} ;

bool SamplePool::loadSample(const char *path) {

	if (count_==MAX_PIG_SAMPLES) return false ;

	Path sPath(path) ;
    Status::Set("Loading %s",sPath.GetName().c_str()) ;
    Trace::Log("loadSample", "%s", path);

    Path wavPath(path);
    WavFile *wave=WavFile::Open(path) ;
	if (wave) {
        if (wave->GetSize(-1) <= 0) {
            Trace::Error("Rejected zero-length sample %s", wavPath.GetName().c_str());
            delete wave;
            return false;
        }
		wav_[count_]=wave ;
		const std::string name=wavPath.GetName() ;
		names_[count_]=(char*)SYS_MALLOC(name.length()+1) ;
		strcpy(names_[count_],name.c_str()) ;
		count_++ ;
		wave->GetBuffer(0,wave->GetSize(-1)) ;
		wave->Close() ;
		return true ;
	} else {
		Trace::Error("Failed to load samples %s",wavPath.GetName().c_str()) ;
		return false ;
 	}
}

#define IMPORT_CHUNK_SIZE 1000

// TREEFROG_U2_35_SAMPLE_MANAGER_IMPORT_FORCE_DELETE
// TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
// Return a project-local name that does not overwrite an existing imported WAV.
// Example: kick.wav, kick_01.wav, kick_02.wav ...
static std::string MakeUniqueProjectSampleName(const std::string &name) {
    std::string base = name;
    std::string ext = "";
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        base = name.substr(0, dot);
        ext = name.substr(dot);
    }

    char candidate[256];
    snprintf(candidate, sizeof(candidate), "%s", name.c_str());
    for (int attempt = 0; attempt < 100; attempt++) {
        if (attempt > 0) {
            snprintf(candidate, sizeof(candidate), "%s_%02d%s", base.c_str(), attempt, ext.c_str());
        }
        std::string logical = "samples:";
        logical += candidate;
        Path dstPath(logical.c_str());
        Path checkPath(dstPath.GetPath());
        if (!checkPath.Exists()) return std::string(candidate);
    }

    snprintf(candidate, sizeof(candidate), "%s_%ld%s", base.c_str(), (long)time(0), ext.c_str());
    return std::string(candidate);
}


// TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
// Exact-content compare used by ImportSample(): importing the same WAV twice must
// reuse the existing project sample instead of creating kick_01.wav, kick_02.wav.
static bool FilesAreIdenticalForImport(const char *a, const char *b) {
    if (!a || !b) return false;
    I_File *fa = FileSystem::GetInstance()->Open(a, "r");
    if (!fa) return false;
    I_File *fb = FileSystem::GetInstance()->Open(b, "r");
    if (!fb) {
        fa->Close();
        delete fa;
        return false;
    }

    fa->Seek(0, SEEK_END);
    long sa = fa->Tell();
    fa->Seek(0, SEEK_SET);
    fb->Seek(0, SEEK_END);
    long sb = fb->Tell();
    fb->Seek(0, SEEK_SET);
    if (sa != sb || sa < 0) {
        fa->Close(); fb->Close();
        delete fa; delete fb;
        return false;
    }

    char ba[IMPORT_CHUNK_SIZE];
    char bb[IMPORT_CHUNK_SIZE];
    long remaining = sa;
    bool same = true;
    while (remaining > 0) {
        int count = (remaining > IMPORT_CHUNK_SIZE) ? IMPORT_CHUNK_SIZE : (int)remaining;
        int ra = fa->Read(ba, 1, count);
        int rb = fb->Read(bb, 1, count);
        if (ra != count || rb != count || memcmp(ba, bb, count) != 0) {
            same = false;
            break;
        }
        remaining -= count;
    }

    fa->Close(); fb->Close();
    delete fa; delete fb;
    return same;
}

int SamplePool::FindIdenticalProjectSample(Path &path) {
    if (path.IsDirectory()) return -1;
    for (int i = 0; i < count_; i++) {
        if (!names_[i]) continue;
        std::string logical = "samples:";
        logical += names_[i];
        Path existing(logical.c_str());
        if (FilesAreIdenticalForImport(path.GetPath().c_str(), existing.GetPath().c_str())) {
            return i;
        }
    }
    return -1;
}

int SamplePool::ImportSample(Path &path) {

    // TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
    // If this exact WAV content is already present in the project, reuse it.
    // This saves RAM and project storage even when the source filename differs.
    int identical = FindIdenticalProjectSample(path);
    if (identical >= 0) return identical;

	if (count_==MAX_PIG_SAMPLES) return -1 ;

	// construct target path without overwriting a different existing project sample

	std::string importedName = MakeUniqueProjectSampleName(path.GetName()) ;
	std::string dpath="samples:" ;
	dpath+=importedName ;
	Path dstPath(dpath.c_str()) ;

    // Opens files

	I_File *fin=FileSystem::GetInstance()->Open(path.GetPath().c_str(),"r") ;
    if (!fin) {
        Trace::Error("Failed to open input file %s",
                     path.GetCanonicalPath().c_str());
        return -1;
    };
    fin->Seek(0,SEEK_END) ;
	long size=fin->Tell() ;
	fin->Seek(0,SEEK_SET) ;

	I_File *fout=FileSystem::GetInstance()->Open(dstPath.GetPath().c_str(),"w") ;
	if (!fout) {
		fin->Close() ;
		delete (fin) ;
		return -1 ;
	} ;

	// copy file to current project

	char buffer[IMPORT_CHUNK_SIZE] ;
	while (size>0) {
		int count=(size>IMPORT_CHUNK_SIZE)?IMPORT_CHUNK_SIZE:size ;
		fin->Read(buffer,1,count) ;
		fout->Write(buffer,1,count) ;
		size-=count ;
	} ;

	fin->Close() ;
	fout->Close() ;
	delete(fin) ;
	delete(fout) ;

	// now load the sample

	bool status=loadSample(dstPath.GetPath().c_str()) ;

	SetChanged() ;
	SamplePoolEvent ev ;
	ev.index_=count_-1 ;
	ev.type_=SPET_INSERT ;
	NotifyObservers(&ev) ;
	return status?(count_-1):-1 ;
};

bool SamplePool::IsImported(std::string name) {
    std::string dpath="samples:";
    dpath += name;
    Path dstPath(dpath.c_str());
    Path checkPath(dstPath.GetPath());
    return checkPath.Exists();
}

/*
    Unsorted reassign for now
    Returns the index of the sample in the pool
    count_-1 position if new
    previous position if already imported
*/
int SamplePool::Reassign(std::string name, bool imported) {
    if (count_ == MAX_PIG_SAMPLES)
        return -1;
    int insertedIndex = getIndexOf(name.c_str());
    if (imported)
        unload(insertedIndex);

    std::string aliasPath = "samples:";
    aliasPath += name;
    Path dstPath(aliasPath.c_str());

    if (loadSample(dstPath.GetCanonicalPath().c_str())) {
        SetChanged();
        SamplePoolEvent ev;
        ev.index_ = getIndexOf(name.c_str());;
        ev.type_=SPET_INSERT;
        NotifyObservers(&ev);
        return ev.index_;
    }
    return -1;
}

void SamplePool::PurgeSample(int i) {

	// construct the path of the sample to delete

	std::string wavPath="samples:" ;
	wavPath+=names_[i] ;
	Path path(wavPath.c_str()) ;
	//delete wav
	SAFE_DELETE(wav_[i]) ;
	// delete name entry
	SAFE_DELETE(names_[i]) ;

	// delete file
	FileSystem::GetInstance()->Delete(path.GetPath().c_str()) ;

	// shift all entries from deleted to end
	for (int j=i;j<count_-1;j++) {
		wav_[j]=wav_[j+1] ;
		names_[j]=names_[j+1] ;
	} ;
	// decrease sample count
	count_-- ;
	wav_[count_]=0 ;
	names_[count_]=0 ;

	// now notify observers
	SetChanged() ;
	SamplePoolEvent ev ;
	ev.index_=i ;
	ev.type_=SPET_DELETE ;
	NotifyObservers(&ev) ;
} ;

void SamplePool::unload(int i) {

    // construct the path of the sample to delete

	std::string wavPath="samples:" ;
	wavPath+=names_[i] ;
	Path path(wavPath.c_str()) ;

	// shift all entries from deleted to end
	for (int j=i;j<count_-1;j++) {
		wav_[j]=wav_[j+1] ;
		names_[j]=names_[j+1] ;
	} ;
	// decrease sample count
	count_-- ;
	wav_[count_]=0 ;
	names_[count_]=0 ;

	// now notify observers
	SetChanged() ;
	SamplePoolEvent ev ;
	ev.index_=i ;
	ev.type_=SPET_DELETE ;
	NotifyObservers(&ev) ;
}

bool SamplePool::loadSoundFont(const char *path) {

	sfBankID  id=SoundFontManager::GetInstance()->LoadBank(path) ;
	if (id==-1) {
		return false ;
	} 

	// Grab the sample offset

	long offset=sfGetSMPLOffset(id) ;

	// Add all presets of the sf

	WORD presetCount=0 ;
	SFPRESETHDRPTR pHeaders=sfGetPresetHdrs(id,&presetCount); 

	for (int i=0;i<presetCount;i++) {
		if (count_<MAX_PIG_SAMPLES) {
			sfPresetHdr current=pHeaders[i] ;
			wav_[count_]=new SoundFontPreset(id,i) ;
			const char *name=pHeaders[i].achPresetName ;
			names_[count_]=(char*)SYS_MALLOC(strlen(name)+1) ;
			strcpy(names_[count_],name) ;
			count_++ ;
		}
	}
/*
	// Get Sample information

	WORD headerCount=0 ;
	SFSAMPLEHDRPTR  &headers=sfGetSampHdrs(id,&headerCount ); 

	// Loop on every sample, add them

	for (int i=0;i<headerCount;i++) {
		if (count_<MAX_PIG_SAMPLES) {
			sfSampleHdr &current=headers[i] ;
			wav_[count_]=new SoundFontSample(current) ;
			const char *name=headers[i].achSampleName ;
			names_[count_]=(char*)SYS_MALLOC(strlen(name)+1) ;
			strcpy(names_[count_],name) ;
			count_++ ;
		}
	}
*/	return true ;
} ;
