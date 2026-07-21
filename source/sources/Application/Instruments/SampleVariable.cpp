#include "SampleVariable.h"
#include "SamplePool.h"

SampleVariable::SampleVariable(const char *name,FourCC id):WatchedVariable(name,id,0,0,-1) {
	SamplePool *pool=SamplePool::GetInstance() ;
	list_.char_=pool->GetNameList() ;
	listSize_=pool->GetNameListSize() ;
	pool->AddObserver(*this) ;
} ;

SampleVariable::~SampleVariable() {
	SamplePool *pool=SamplePool::GetInstance() ;
	pool->RemoveObserver(*this) ;
} ;

void SampleVariable::Update(Observable &o,I_ObservableData *d) {
    SamplePoolEvent *e=(SamplePoolEvent *)d;
    SamplePool *pool=(SamplePool*)&o;
    list_.char_=pool->GetNameList();
    listSize_=pool->GetNameListSize();

    if (e->type_==SPET_DELETE) {
        if (value_.index_==e->index_) {
            /* U2.52.1 safety net: callers should unassign before deletion, but
             * a stale instrument must never retain the deleted index. SetInt
             * notifies SampleInstrument so its cached SoundSource is cleared. */
            SetInt(-1);
        } else if (value_.index_>e->index_) {
            /* The same SoundSource object moved one slot down. Keep the cached
             * pointer and adjust only the logical index. */
            value_.index_--;
        }
    }
} ;
