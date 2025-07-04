#include "neonucleus.h"
#include "universe.h"
#include <string.h>

nn_universe *nn_newUniverse(nn_Alloc alloc) {
    nn_universe *u = nn_alloc(&alloc, sizeof(nn_universe));
    if(u == NULL) return u;
    u->alloc = alloc;
    // we leave udata uninitialized because it does not matter
    u->udataLen = 0;
    u->clockUserdata = NULL;
    u->currentClock = nn_realTimeClock;
    return u;
}

nn_Alloc *nn_getAllocator(nn_universe *universe) {
    return &universe->alloc;
}

void nn_unsafeDeleteUniverse(nn_universe *universe) {
    for(size_t i = 0; i < universe->udataLen; i++) {
        nn_deallocStr(&universe->alloc, universe->udata[i].name);
    }
    nn_dealloc(&universe->alloc, universe, sizeof(nn_universe));
}

void *nn_queryUserdata(nn_universe *universe, const char *name) {
    for(size_t i = 0; i < universe->udataLen; i++) {
        if(strcmp(universe->udata[i].name, name) == 0) {
            return universe->udata[i].userdata;
        }
    }
    return NULL;
}

void nn_storeUserdata(nn_universe *universe, const char *name, void *data) {
    if(universe->udataLen == NN_MAX_USERDATA) return; // prevent overflow

    size_t idx = universe->udataLen;
    char *allocName = nn_strdup(&universe->alloc, name);
    if(allocName == NULL) return;

    universe->udata[idx].name = allocName;
    universe->udata[idx].userdata = data;
    universe->udataLen++;
}

double nn_getTime(nn_universe *universe) {
    return universe->currentClock(universe->clockUserdata);
}

void nn_loadCoreComponentTables(nn_universe *universe) {
    nn_loadEepromTable(universe);
    nn_loadFilesystemTable(universe);
    nn_loadDriveTable(universe);
    nn_loadScreenTable(universe);
    nn_loadGraphicsCardTable(universe);
    nn_loadKeyboardTable(universe);
}
