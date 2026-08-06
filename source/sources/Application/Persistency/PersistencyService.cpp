#include "PersistencyService.h"
#include "Persistent.h"
#include "Externals/Compression/lz.h"
#include "System/Console/Trace.h"
#include "Foundation/Types/Types.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define LGPT_PROJECT_FILE_MAX_BYTES (64 * 1024 * 1024)
#define LGPT_PROJECT_XML_MAX_BYTES  (128 * 1024 * 1024)

PersistencyService::PersistencyService():Service(MAKE_FOURCC('S','V','P','S')) {
} ;

void PersistencyService::Save(const char *name) {

    Path filename(name);

    TiXmlDocument doc(filename.GetPath());
    TiXmlElement first("LITTLEGPTRACKER") ;
    TiXmlNode *node=doc.InsertEndChild(first) ;

    // Loop on all registered service accumulating XML flow.
    IteratorPtr<SubService> it(GetIterator()) ;
    for (it->Begin();!it->IsDone();it->Next()) {
        Persistent *currentItem=(Persistent *)&it->CurrentItem() ;
        currentItem->Save(node) ;
    } ;

    if (!doc.SaveFile()) {
        Trace::Error("Failed to save project file %s", filename.GetPath().c_str());
    }

    /* TREEFROG_SAVE_SYNC_V1 (Bacon 1.1.1): the R36S is normally power-cycled
     * without an orderly unmount.  libc fclose() only flushes the user-space
     * buffer; without an explicit sync() the project bytes can still sit in
     * the kernel page cache when the power is cut, and the next boot loads
     * the old (or missing) lgptsav.dat.  Force data + FAT metadata to the
     * card on every project save so a hard power-off cannot lose it. */
    sync();
};

bool PersistencyService::Load() {

    Path filename("project:lgptsav.dat") ;
    PersistencyDocument doc(filename.GetPath());

    FileSystem *fs=FileSystem::GetInstance() ;
    I_File *file=fs->Open(filename.GetPath().c_str(),"r") ;
    if (!file) return false ;

    file->Seek(0,SEEK_END) ;
    long fileLength=file->Tell() ;
    file->Seek(0,SEEK_SET) ;

    if (fileLength<=0 || fileLength>LGPT_PROJECT_FILE_MAX_BYTES) {
        Trace::Error("Invalid project file size %ld", fileLength);
        file->Close();
        delete file;
        return false;
    }

    unsigned char *compBuffer=(unsigned char *)SYS_MALLOC((int)fileLength+1) ;
    if (!compBuffer) {
        Trace::Error("Could not allocate project buffer of %ld bytes", fileLength+1);
        file->Close();
        delete file;
        return false;
    }

    int bytesRead=file->Read(compBuffer,1,(int)fileLength) ;
    file->Close();
    delete file ;

    if (bytesRead!=(int)fileLength) {
        Trace::Error("Short project read: expected %ld got %d", fileLength, bytesRead);
        SYS_FREE(compBuffer);
        return false;
    }
    compBuffer[fileLength]=0;

    bool parsed=(doc.Parse((char *)compBuffer)!=0 && !doc.Error());

    if (!parsed) {
        // Backward-compatible compressed format:
        // first 4 bytes = uncompressed XML byte length.
        doc.Clear();

        const int offset=(int)sizeof(int);
        if (fileLength<=offset) {
            Trace::Error("Project is neither valid XML nor a compressed project");
        } else {
            int fullLength=0;
            memcpy(&fullLength,compBuffer,sizeof(fullLength));

            if (fullLength<=0 || fullLength>LGPT_PROJECT_XML_MAX_BYTES) {
                Trace::Error("Invalid compressed project XML size %d", fullLength);
            } else {
                unsigned char *xmlSource=(unsigned char *)SYS_MALLOC(fullLength+1) ;
                if (xmlSource) {
                    /* Bacon 1.1.1 V18: bounded decompression.  Corrupt save
                     * files must never overflow the output buffer; a
                     * hostile/truncated stream returns -1 here instead of
                     * crashing the port on LoadProject. */
                    int outLen=LZ_Uncompress_Safe(compBuffer+offset,xmlSource,
                                                  (unsigned int)(fileLength-offset),
                                                  (unsigned int)fullLength);
                    if (outLen<0) {
                        Trace::Error("Corrupt compressed project (bounded decode failed)");
                    } else {
                        xmlSource[fullLength]=0;
                        parsed=(doc.Parse((char *)xmlSource)!=0 && !doc.Error());
                    }
                    SYS_FREE(xmlSource);
                }
            }
        }
    }

    SYS_FREE(compBuffer);

    if (!parsed) {
        Trace::Error("Project XML parse failed: %s", doc.ErrorDesc());
        /* Bacon 1.1.1 V18: when the main lgptsav.dat is unusable, try the
         * save-as scratch file that LGPT writes before saving a project
         * copy (project:lgptsav_tmp.dat).  It is plain XML when present. */
        Path fallback("project:lgptsav_tmp.dat") ;
        FileSystem *fsFallback=FileSystem::GetInstance() ;
        I_File *fileFallback=fsFallback->Open(fallback.GetPath().c_str(),"r") ;
        if (fileFallback) {
            fileFallback->Seek(0,SEEK_END) ;
            long fbLen=fileFallback->Tell() ;
            fileFallback->Seek(0,SEEK_SET) ;
            if (fbLen>0 && fbLen<=LGPT_PROJECT_FILE_MAX_BYTES) {
                unsigned char *fbBuffer=(unsigned char *)SYS_MALLOC((int)fbLen+1) ;
                if (fbBuffer) {
                    int fbRead=fileFallback->Read(fbBuffer,1,(int)fbLen) ;
                    fileFallback->Close();
                    delete fileFallback;
                    if (fbRead==(int)fbLen) {
                        fbBuffer[fbLen]=0;
                        doc.Clear();
                        parsed=(doc.Parse((char *)fbBuffer)!=0 && !doc.Error());
                        if (parsed) {
                            Trace::Log("Persistency","Loaded project from lgptsav_tmp.dat fallback");
                        }
                    }
                    SYS_FREE(fbBuffer);
                } else {
                    fileFallback->Close();
                    delete fileFallback;
                }
            } else {
                fileFallback->Close();
                delete fileFallback;
            }
        }
        if (!parsed) {
            /* TREEFROG_SAVE_SYNC_V1 (Bacon 1.1.1): never leave the broken
             * main save in place for the autosave to overwrite silently.  A
             * torn FAT write (hard power-off) can produce a file that parses
             * neither as XML nor as compressed data; keep it aside so the
             * data is not lost before the user decides what to do. */
            Path mainPath("project:lgptsav.dat");
            Path backupPath("project:lgptsav.dat.corrupto.bak");
            if (rename(mainPath.GetPath().c_str(), backupPath.GetPath().c_str()) == 0) {
                Trace::Log("Persistency","Moved unreadable lgptsav.dat to .corrupto.bak");
                sync();
            } else {
                Trace::Log("Persistency","Could not move unreadable lgptsav.dat aside");
            }
            return false;
        }
    }

    TiXmlNode* node=doc.FirstChild("LITTLEGPTRACKER");
    if (!node) {
        Trace::Error("Could not find LITTLEGPTRACKER master node");
        return false;
    }

    TiXmlElement* root=node->ToElement();
    if (!root) {
        Trace::Error("Invalid LITTLEGPTRACKER master node");
        return false;
    }

    TiXmlElement* element=root->FirstChildElement();
    while (element) {
        IteratorPtr<SubService> it(GetIterator()) ;
        for (it->Begin();!it->IsDone();it->Next()) {
            Persistent *currentItem=(Persistent *)&it->CurrentItem() ;
            if (currentItem->Restore(element)) break;
        }
        element=element->NextSiblingElement();
    }
    return true ;
} ;
