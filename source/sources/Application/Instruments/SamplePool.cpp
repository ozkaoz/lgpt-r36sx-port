#include "SamplePool.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "System/Console/Trace.h"
#include "Application/Persistency/PersistencyService.h" 
#include "System/io/Status.h"
#include <string>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "SoundFontSample.h"
#include "SoundFontPreset.h"
#include "SoundFontManager.h"
#include "Application/Model/Config.h"

#define SAMPLE_LIB "root:samples" 


static bool IsTransientInternalSampleName(const char *name) {
    if (!name) return false;
    return strcasecmp(name, "__u2_listen_preview.wav") == 0 ||
           strcasecmp(name, "__u2_pitch_env_preview.wav") == 0;
}

SamplePool::SamplePool() {
	for (int i=0;i<MAX_PIG_SAMPLES;i++) {
		names_[i]=NULL ;
		wav_[i]=NULL ;
	} ;
	count_=0 ;
} ;

void SamplePool::ReleaseRetiredSources() {
    for (std::vector<SoundSource *>::iterator it=retiredWav_.begin();
         it!=retiredWav_.end(); ++it) {
        SoundSource *source=*it;
        SAFE_DELETE(source);
    }
    retiredWav_.clear();
}

SamplePool::~SamplePool() {
	for (int i=0;i<MAX_PIG_SAMPLES;i++) {
		SAFE_DELETE(wav_[i]) ;
		SAFE_FREE(names_[i]) ;
	} ;
    ReleaseRetiredSources();
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
    ReleaseRetiredSources();
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
        if (IsTransientInternalSampleName(path.GetName().c_str())) {
            Trace::Log("Load", "purging transient preview %s", path.GetCanonicalPath().c_str());
            FileSystem::GetInstance()->Delete(path.GetPath().c_str());
            continue;
        }
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
    if (!name) return -1;
    for (int i=0;i<count_;i++) {
        if (names_[i] && strcmp(names_[i],name)==0) return i;
    }
    return -1;
}

SoundSource *SamplePool::GetSource(int i) {
    if (i<0 || i>=count_) return 0;
    return wav_[i];
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
        if (!wave->GetBuffer(0,wave->GetSize(-1))) {
            Trace::Error("Failed to read sample data %s",wavPath.GetName().c_str());
            delete wave;
            return false;
        }

        const std::string name=wavPath.GetName();
        char *storedName=(char*)SYS_MALLOC(name.length()+1);
        if (!storedName) {
            Trace::Error("Failed to allocate sample name %s",name.c_str());
            delete wave;
            return false;
        }
        strcpy(storedName,name.c_str());

        wave->Close();
        wav_[count_]=wave;
        names_[count_]=storedName;
        count_++;
        return true;
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
    const std::string sourceName = path.GetName();
    for (int i = 0; i < count_; i++) {
        if (!names_[i] || IsTransientInternalSampleName(names_[i])) continue;

        /* Filename identity is exact and case-preserving. Reuse an existing
         * project sample only when basename (including case) and content match.
         * Case variants remain distinct whenever the underlying filesystem
         * can represent them safely. */
        if (strcmp(names_[i], sourceName.c_str()) != 0) continue;

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

    if (path.IsDirectory() || count_>=MAX_PIG_SAMPLES) return -1;

    int identical=FindIdenticalProjectSample(path);
    if (identical>=0) return identical;

    std::string importedName=MakeUniqueProjectSampleName(path.GetName());
    std::string dpath="samples:";
    dpath+=importedName;
    Path dstPath(dpath.c_str());

    I_File *fin=FileSystem::GetInstance()->Open(path.GetPath().c_str(),"r");
    if (!fin) {
        Trace::Error("Failed to open input file %s",
                     path.GetCanonicalPath().c_str());
        return -1;
    }

    fin->Seek(0,SEEK_END);
    long sourceSize=fin->Tell();
    fin->Seek(0,SEEK_SET);
    if (sourceSize<=0) {
        Trace::Error("Rejected empty import %s",path.GetCanonicalPath().c_str());
        fin->Close();
        delete fin;
        return -1;
    }

    /* U2.52.6: never fail import silently when the project samples folder
     * is missing (e.g. after an external delete of the project tree). The
     * project folder must not surface as a bare "Import failed" with no
     * log trail, so the samples folder is rebuilt on demand. */
    {
        std::string dstFull = dstPath.GetPath();
        std::string dstDir = dstFull;
        size_t lastSlash = dstDir.find_last_of('/');
        if (lastSlash != std::string::npos) {
            dstDir = dstDir.substr(0, lastSlash);
        }
        struct stat st;
        if (stat(dstDir.c_str(), &st) != 0) {
            errno = 0;
            /* mkdir -p semantics: create each missing component so an
             * entirely deleted project folder is rebuilt on import. */
            std::string walk;
            size_t pos = 0;
            while (pos <= dstDir.size()) {
                size_t next = dstDir.find('/', pos);
                if (next == std::string::npos) next = dstDir.size();
                std::string part = dstDir.substr(0, next);
                if (!part.empty()) {
                    if (stat(part.c_str(), &st) != 0) {
                        if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) {
                            Trace::Error("ImportSample: cannot recreate %s "
                                         "(errno=%d)", part.c_str(), errno);
                            break;
                        }
                    }
                }
                if (next >= dstDir.size()) break;
                pos = next + 1;
            }
            if (stat(dstDir.c_str(), &st) != 0) {
                Trace::Error("ImportSample: samples folder still missing: %s",
                             dstDir.c_str());
            } else {
                Trace::Log("ImportSample", "recreated missing samples "
                           "folder %s", dstDir.c_str());
            }
        }
    }

    I_File *fout=FileSystem::GetInstance()->Open(dstPath.GetPath().c_str(),"w");
    if (!fout) {
        Trace::Error("Failed to open project sample %s for writing "
                     "(errno=%d)", dstPath.GetCanonicalPath().c_str(), errno);
        fin->Close();
        delete fin;
        return -1;
    }

    char buffer[IMPORT_CHUNK_SIZE];
    long remaining=sourceSize;
    bool copied=true;
    while (remaining>0) {
        int requested=(remaining>IMPORT_CHUNK_SIZE)?
            IMPORT_CHUNK_SIZE:(int)remaining;
        int read=fin->Read(buffer,1,requested);
        if (read!=requested) {
            Trace::Error("Short sample import read: expected %d got %d",
                         requested,read);
            copied=false;
            break;
        }
        int written=fout->Write(buffer,1,read);
        if (written!=read) {
            Trace::Error("Short sample import write: expected %d got %d",
                         read,written);
            copied=false;
            break;
        }
        remaining-=read;
    }

    fin->Close();
    fout->Close();
    delete fin;
    delete fout;

    if (!copied || remaining!=0) {
        FileSystem::GetInstance()->Delete(dstPath.GetPath().c_str());
        return -1;
    }

    const int insertedIndex=count_;
    if (!loadSample(dstPath.GetPath().c_str())) {
        FileSystem::GetInstance()->Delete(dstPath.GetPath().c_str());
        return -1;
    }

    SetChanged();
    SamplePoolEvent ev;
    ev.index_=insertedIndex;
    ev.type_=SPET_INSERT;
    NotifyObservers(&ev);
    return insertedIndex;
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
    int insertedIndex=getIndexOf(name.c_str());
    if (imported && insertedIndex>=0)
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

    if (i<0 || i>=count_ || !names_[i]) return;

    /* U2.52.1: deletion is two-phase. The audio callback is not locked on the
     * TreeFrog port, so a voice can still hold a buffer pointer for a few
     * frames after Player::Stop(). Removing the entry from the logical pool is
     * immediate, but the SoundSource object is retired until project reset or
     * shutdown. This prevents intermittent use-after-free crashes when Song is
     * started immediately after deleting a sample. */
    std::string wavPath="samples:";
    wavPath+=names_[i];
    Path path(wavPath.c_str());
    SoundSource *retiredSource=wav_[i];
    char *retiredName=names_[i];

    FileSystem::GetInstance()->Delete(path.GetPath().c_str());

    for (int j=i;j<count_-1;j++) {
        wav_[j]=wav_[j+1];
        names_[j]=names_[j+1];
    }
    count_--;
    wav_[count_]=0;
    names_[count_]=0;

    if (retiredSource) retiredWav_.push_back(retiredSource);
    SAFE_FREE(retiredName);

    SetChanged();
    SamplePoolEvent ev;
    ev.index_=i;
    ev.type_=SPET_DELETE;
    NotifyObservers(&ev);
}

static bool RenamePathWithCaseSupport(const char *oldPath,const char *newPath) {
    if (!oldPath || !newPath || !oldPath[0] || !newPath[0]) return false;
    if (strcmp(oldPath,newPath)==0) return true;

    if (strcasecmp(oldPath,newPath)==0) {
        char temporary[1024];
        snprintf(temporary,sizeof(temporary),"%s.__u2521_case_%ld",oldPath,(long)getpid());
        unlink(temporary);
        if (rename(oldPath,temporary)!=0) return false;
        if (rename(temporary,newPath)!=0) {
            rename(temporary,oldPath);
            return false;
        }
        return true;
    }
    return rename(oldPath,newPath)==0;
}

bool SamplePool::RenameSample(int i,const char *newName) {
    if (i<0 || i>=count_ || !names_[i] || !newName || !newName[0]) return false;
    if (strcmp(names_[i],newName)==0) return true;

    for (int j=0;j<count_;++j) {
        if (j!=i && names_[j] && strcmp(names_[j],newName)==0) return false;
    }

    std::string oldLogical="samples:";
    oldLogical+=names_[i];
    std::string newLogical="samples:";
    newLogical+=newName;
    Path oldPath(oldLogical.c_str());
    Path newPath(newLogical.c_str());

    struct stat destinationInfo;
    if (strcasecmp(names_[i],newName)!=0 &&
        stat(newPath.GetPath().c_str(),&destinationInfo)==0) {
        return false;
    }

    std::string oldSidecar=oldPath.GetPath()+std::string(".u2chop");
    std::string newSidecar=newPath.GetPath()+std::string(".u2chop");
    struct stat sidecarInfo;
    const bool sidecarExists=stat(oldSidecar.c_str(),&sidecarInfo)==0;
    if (sidecarExists && strcasecmp(oldSidecar.c_str(),newSidecar.c_str())!=0 &&
        stat(newSidecar.c_str(),&sidecarInfo)==0) {
        return false;
    }

    if (!RenamePathWithCaseSupport(oldPath.GetPath().c_str(),newPath.GetPath().c_str())) {
        return false;
    }
    if (sidecarExists &&
        !RenamePathWithCaseSupport(oldSidecar.c_str(),newSidecar.c_str())) {
        RenamePathWithCaseSupport(newPath.GetPath().c_str(),oldPath.GetPath().c_str());
        return false;
    }

    char *storedName=(char*)SYS_MALLOC(strlen(newName)+1);
    if (!storedName) {
        if (sidecarExists) RenamePathWithCaseSupport(newSidecar.c_str(),oldSidecar.c_str());
        RenamePathWithCaseSupport(newPath.GetPath().c_str(),oldPath.GetPath().c_str());
        return false;
    }
    strcpy(storedName,newName);
    char *oldName=names_[i];
    names_[i]=storedName;
    SAFE_FREE(oldName);

    SetChanged();
    SamplePoolEvent ev;
    ev.index_=i;
    ev.type_=SPET_RENAME;
    NotifyObservers(&ev);
    return true;
}

void SamplePool::unload(int i) {

    if (i<0 || i>=count_) return;

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
