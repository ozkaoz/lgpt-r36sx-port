#include "PersistencyService.h"
#include "Persistent.h"
#include "Externals/Compression/lz.h"
#include "System/Console/Trace.h"
#include "Foundation/Types/Types.h"
#include <string.h>

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
            SYS_FREE(compBuffer);
            return false;
        }

        int fullLength=0;
        memcpy(&fullLength,compBuffer,sizeof(fullLength));

        if (fullLength<=0 || fullLength>LGPT_PROJECT_XML_MAX_BYTES) {
            Trace::Error("Invalid compressed project XML size %d", fullLength);
            SYS_FREE(compBuffer);
            return false;
        }

        unsigned char *xmlSource=(unsigned char *)SYS_MALLOC(fullLength+1) ;
        if (!xmlSource) {
            Trace::Error("Could not allocate project XML buffer of %d bytes", fullLength+1);
            SYS_FREE(compBuffer);
            return false;
        }

        LZ_Uncompress(compBuffer+offset,xmlSource,(unsigned int)(fileLength-offset));
        xmlSource[fullLength]=0;

        parsed=(doc.Parse((char *)xmlSource)!=0 && !doc.Error());
        SYS_FREE(xmlSource);
    }

    SYS_FREE(compBuffer);

    if (!parsed) {
        Trace::Error("Project XML parse failed: %s", doc.ErrorDesc());
        return false;
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
