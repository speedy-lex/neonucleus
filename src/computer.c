#include "computer.h"
#include "component.h"
#include "universe.h"
#include "neonucleus.h"
#include <string.h>

nn_computer *nn_newComputer(nn_universe *universe, nn_address address, nn_architecture *arch, void *userdata, size_t memoryLimit, size_t componentLimit) {
    nn_computer *c = nn_alloc(&universe->alloc, sizeof(nn_computer));
    c->components = nn_alloc(&universe->alloc, sizeof(nn_component) * componentLimit);
    if(c->components == NULL) {
        nn_dealloc(&universe->alloc, c, sizeof(nn_computer));
        return NULL;
    }
    c->address = nn_strdup(&universe->alloc, address);
    if(c->address == NULL) {
        nn_dealloc(&universe->alloc, c->components, sizeof(nn_component) * componentLimit);
        nn_dealloc(&universe->alloc, c, sizeof(nn_computer));
        return NULL;
    }
    c->lock = nn_newGuard(&universe->alloc);
    if(c->lock == NULL) {
        nn_deallocStr(&universe->alloc, c->address);
        nn_dealloc(&universe->alloc, c->components, sizeof(nn_component) * componentLimit);
        nn_dealloc(&universe->alloc, c, sizeof(nn_computer));
        return NULL;
    }
    c->timeOffset = nn_getTime(universe);
    c->supportedArchCount = 0;
    c->argc = 0;
    c->retc = 0;
    c->err = NULL;
    c->allocatedError = false;
    c->state = NN_STATE_SETUP;
    c->componentLen = 0;
    c->componentCap = componentLimit;
    c->userCount = 0;
    c->maxEnergy = 5000;
    c->signalCount = 0;
    c->universe = universe;
    c->arch = arch;
    c->nextArch = arch;
    c->userdata = userdata;
    c->memoryTotal = memoryLimit;
    c->tmpAddress = NULL;
    c->temperature = 30;
    c->roomTemperature = 30;
    c->temperatureCoefficient = 1;
    c->callCost = 0;
    c->callBudget = 256;

    // Setup Architecture
    c->archState = c->arch->setup(c, c->arch->userdata);
    if(c->archState == NULL) {
        nn_deleteGuard(&universe->alloc, c->lock);
        nn_deallocStr(&universe->alloc, c->address);
        nn_dealloc(&universe->alloc, c->components, sizeof(nn_component) * componentLimit);
        nn_dealloc(&universe->alloc, c, sizeof(nn_computer));
        return NULL;
    }

    return c;
}

nn_universe *nn_getUniverse(nn_computer *computer) {
    return computer->universe;
}

void nn_setTmpAddress(nn_computer *computer, nn_address tmp) {
    nn_deallocStr(&computer->universe->alloc, computer->tmpAddress);
    computer->tmpAddress = nn_strdup(&computer->universe->alloc, tmp);
}

nn_address nn_getComputerAddress(nn_computer *computer) {
    return computer->address;
}

nn_address nn_getTmpAddress(nn_computer *computer) {
    return computer->tmpAddress;
}

int nn_tickComputer(nn_computer *computer) {
    computer->callCost = 0;
    computer->state = NN_STATE_RUNNING;
    nn_clearError(computer);
    computer->arch->tick(computer, computer->archState, computer->arch->userdata);
    return nn_getState(computer);
}

double nn_getUptime(nn_computer *computer) {
    return nn_getTime(computer->universe) - computer->timeOffset;
}

size_t nn_getComputerMemoryUsed(nn_computer *computer) {
    return computer->arch->getMemoryUsage(computer, computer->archState, computer->arch->userdata);
}

size_t nn_getComputerMemoryTotal(nn_computer *computer) {
    return computer->memoryTotal;
}

void *nn_getComputerUserData(nn_computer *computer) {
    return computer->userdata;
}

void nn_addSupportedArchitecture(nn_computer *computer, nn_architecture *arch) {
    if(computer->supportedArchCount == NN_MAX_ARCHITECTURES) return;
    computer->supportedArch[computer->supportedArchCount] = arch;
    computer->supportedArchCount++;
}

nn_architecture *nn_getSupportedArchitecture(nn_computer *computer, size_t idx) {
    if(idx >= computer->supportedArchCount) return NULL;
    return computer->supportedArch[idx];
}

nn_architecture *nn_getArchitecture(nn_computer *computer) {
    return computer->arch;
}

nn_architecture *nn_getNextArchitecture(nn_computer *computer) {
    return computer->nextArch;
}

void nn_setNextArchitecture(nn_computer *computer, nn_architecture *arch) {
    computer->nextArch = arch;
}

void nn_deleteComputer(nn_computer *computer) {
    nn_clearError(computer);
    nn_resetCall(computer);
    while(computer->signalCount > 0) {
        nn_popSignal(computer);
    }
    nn_Alloc *a = &computer->universe->alloc;
    for(size_t i = 0; i < computer->userCount; i++) {
        nn_deallocStr(a, computer->users[i]);
    }
    computer->arch->teardown(computer, computer->archState, computer->arch->userdata);
    nn_deleteGuard(a, computer->lock);
    nn_deallocStr(a, computer->address);
    nn_deallocStr(a, computer->tmpAddress);
    nn_dealloc(a, computer->components, sizeof(nn_component) * computer->componentCap);
    nn_dealloc(a, computer, sizeof(nn_computer));
}

const char *nn_pushSignal(nn_computer *computer, nn_value *values, size_t len) {
    if(len > NN_MAX_SIGNAL_VALS) return "too many values";
    if(len == 0) return "missing event";
    // no OOM for you hehe
    if(nn_measurePacketSize(values, len) > NN_MAX_SIGNAL_SIZE) {
        return "too big";
    }
    if(computer->signalCount == NN_MAX_SIGNALS) return "too many signals";
    computer->signals[computer->signalCount].len = len;
    for(size_t i = 0; i < len; i++) {
        computer->signals[computer->signalCount].values[i] = values[i];
    }
    computer->signalCount++;
    return NULL;
}

nn_value nn_fetchSignalValue(nn_computer *computer, size_t index) {
    if(computer->signalCount == 0) return nn_values_nil();
    nn_signal *p = computer->signals;
    if(index >= p->len) return nn_values_nil();
    return p->values[index];
}

size_t nn_signalSize(nn_computer *computer) {
    if(computer->signalCount == 0) return 0;
    return computer->signals[0].len;
}

void nn_popSignal(nn_computer *computer) {
    if(computer->signalCount == 0) return;
    nn_signal *p = computer->signals;
    for(size_t i = 0; i < p->len; i++) {
        nn_values_drop(p->values[i]);
    }
    for(size_t i = 1; i < computer->signalCount; i++) {
        computer->signals[i-1] = computer->signals[i];
    }
    computer->signalCount--;
}

const char *nn_addUser(nn_computer *computer, const char *name) {
    if(computer->userCount == NN_MAX_USERS) return "too many users";
    char *user = nn_strdup(&computer->universe->alloc, name);
    if(user == NULL) return "out of memory";
    computer->users[computer->userCount] = user;
    computer->userCount++;
    return NULL;
}

void nn_deleteUser(nn_computer *computer, const char *name) {
    size_t j = 0;
    for(size_t i = 0; i < computer->userCount; i++) {
        char *user = computer->users[i];
        if(strcmp(user, name) == 0) {
            nn_deallocStr(&computer->universe->alloc, user);
        } else {
            computer->users[j] = user;
            j++;
        }
    }
    computer->userCount = j;
}

const char *nn_indexUser(nn_computer *computer, size_t idx) {
    if(idx >= computer->userCount) return NULL;
    return computer->users[idx];
}

bool nn_isUser(nn_computer *computer, const char *name) {
    if(computer->userCount == 0) return true;
    for(size_t i = 0; i < computer->userCount; i++) {
        if(strcmp(computer->users[i], name) == 0) return true;
    }
    return false;
}

void nn_setCallBudget(nn_computer *computer, double callBudget) {
    computer->callBudget = callBudget;
}

double nn_getCallBudget(nn_computer *computer) {
    return computer->callBudget;
}

void nn_callCost(nn_computer *computer, double cost) {
    computer->callCost += cost;
    if(computer->callCost >= computer->callBudget) nn_triggerIndirect(computer);
}

double nn_getCallCost(nn_computer *computer) {
    return computer->callCost;
}

bool nn_isOverworked(nn_computer *computer) {
    return computer->state == NN_STATE_OVERWORKED;
}

void nn_triggerIndirect(nn_computer *computer) {
    computer->state = NN_STATE_OVERWORKED;
}

int nn_getState(nn_computer *computer) {
    return computer->state;
}

void nn_setState(nn_computer *computer, int state) {
    computer->state = state;
}

void nn_setEnergyInfo(nn_computer *computer, double energy, double capacity) {
    computer->energy = energy;
    computer->maxEnergy = capacity;
}

double nn_getEnergy(nn_computer *computer) {
    return computer->energy;
}

double nn_getMaxEnergy(nn_computer *computer) {
    return computer->maxEnergy;
}

void nn_removeEnergy(nn_computer *computer, double energy) {
    if(computer->energy < energy) {
        // blackout
        computer->energy = 0;
        computer->state = NN_STATE_BLACKOUT;
        return;
    }
    computer->energy -= energy;
}

void nn_addEnergy(nn_computer *computer, double amount) {
    if(computer->maxEnergy - computer->energy < amount) {
        computer->energy = computer->maxEnergy;
        return;
    }
    computer->energy += amount;
}

double nn_getTemperature(nn_computer *computer) {
    return computer->temperature;
}

double nn_getThermalCoefficient(nn_computer *computer) {
    return computer->temperatureCoefficient;
}

double nn_getRoomTemperature(nn_computer *computer) {
    return computer->roomTemperature;
}

void nn_setTemperature(nn_computer *computer, double temperature) {
    computer->temperature = temperature;
    if(computer->temperature < computer->roomTemperature) computer->temperature = computer->roomTemperature;
}

void nn_setTemperatureCoefficient(nn_computer *computer, double coefficient) {
    computer->temperatureCoefficient = coefficient;
}

void nn_setRoomTemperature(nn_computer *computer, double roomTemperature) {
    computer->roomTemperature = roomTemperature;
    if(computer->temperature < computer->roomTemperature) computer->temperature = computer->roomTemperature;
}

void nn_addHeat(nn_computer *computer, double heat) {
    computer->temperature += heat * computer->temperatureCoefficient;
    if(computer->temperature < computer->roomTemperature) computer->temperature = computer->roomTemperature;
}

void nn_removeHeat(nn_computer *computer, double heat) {
    computer->temperature -= heat;
    if(computer->temperature < computer->roomTemperature) computer->temperature = computer->roomTemperature;
}

bool nn_isOverheating(nn_computer *computer) {
    return computer->temperature > NN_OVERHEAT_MIN;
}

const char *nn_getError(nn_computer *computer) {
    return computer->err;
}

void nn_clearError(nn_computer *computer) {
    if(computer->allocatedError) {
        nn_deallocStr(&computer->universe->alloc, computer->err);
    }
    computer->err = NULL;
    computer->allocatedError = false;
}

void nn_setError(nn_computer *computer, const char *err) {
    nn_clearError(computer);
    char *copy = nn_strdup(&computer->universe->alloc, err);
    if(copy == NULL) {
        nn_setCError(computer, "out of memory");
        return;
    }
    computer->err = copy;
    computer->allocatedError = true;
}

void nn_setCError(nn_computer *computer, const char *err) {
    nn_clearError(computer);
    // we pinky promise this is safe
    computer->err = (char *)err;
    computer->allocatedError = false;
}

nn_component *nn_newComponent(nn_computer *computer, nn_address address, int slot, nn_componentTable *table, void *userdata) {
    nn_component *c = NULL;
    for(size_t i = 0; i < computer->componentLen; i++) {
        if(computer->components[i].address == NULL) {
            c = computer->components + i;
            break;
        }
    }
    if(c == NULL) {
        if(computer->componentLen == computer->componentCap) return NULL; // too many
        c = computer->components + computer->componentLen;
        computer->componentLen++;
    }

    c->address = nn_strdup(&computer->universe->alloc, address);
    if(c->address == NULL) return NULL;
    c->table = table;
    c->slot = slot;
    c->computer = computer;
    if(table->constructor == NULL) {
        c->statePtr = userdata;
    } else {
        c->statePtr = table->constructor(table->userdata, userdata);
    }
    return c;
}

void nn_removeComponent(nn_computer *computer, nn_address address) {
    for(size_t i = 0; i < computer->componentLen; i++) {
        if(strcmp(computer->components[i].address, address) == 0) {
            nn_destroyComponent(computer->components + i);
        }
    }
}

void nn_destroyComponent(nn_component *component) {
    nn_deallocStr(&component->computer->universe->alloc, component->address);
    if(component->table->destructor != NULL) {
        component->table->destructor(component->table->userdata, component, component->statePtr);
    }
    component->address = NULL; // marks component as freed
}

nn_component *nn_findComponent(nn_computer *computer, nn_address address) {
    for(size_t i = 0; i < computer->componentLen; i++) {
        if(computer->components[i].address == NULL) continue; // empty slot
        if(strcmp(computer->components[i].address, address) == 0) {
            return computer->components + i;
        }
    }
    return NULL;
}

nn_component *nn_iterComponent(nn_computer *computer, size_t *internalIndex) {
    for(size_t i = *internalIndex; i < computer->componentLen; i++) {
        if(computer->components[i].address == NULL) continue;
        *internalIndex = i+1;
        return computer->components + i;
    }
    return NULL;
}

void nn_resetCall(nn_computer *computer) {
    for(size_t i = 0; i < computer->argc; i++) {
        nn_values_drop(computer->args[i]);
    }
    
    for(size_t i = 0; i < computer->retc; i++) {
        nn_values_drop(computer->rets[i]);
    }

    computer->argc = 0;
    computer->retc = 0;
}

void nn_addArgument(nn_computer *computer, nn_value arg) {
    if(computer->argc == NN_MAX_ARGS) return;
    computer->args[computer->argc] = arg;
    computer->argc++;
}

void nn_return(nn_computer *computer, nn_value val) {
    if(computer->retc == NN_MAX_RETS) return;
    computer->rets[computer->retc] = val;
    computer->retc++;
}

nn_value nn_getArgument(nn_computer *computer, size_t idx) {
    if(idx >= computer->argc) return nn_values_nil();
    return computer->args[idx];
}

nn_value nn_getReturn(nn_computer *computer, size_t idx) {
    if(idx >= computer->retc) return nn_values_nil();
    return computer->rets[idx];
}

size_t nn_getArgumentCount(nn_computer *computer) {
    return computer->argc;
}

size_t nn_getReturnCount(nn_computer *computer) {
    return computer->retc;
}

char *nn_serializeProgram(nn_computer *computer, size_t *len) {
    return computer->arch->serialize(computer, computer->archState, computer->arch->userdata, len);
}

void nn_deserializeProgram(nn_computer *computer, const char *memory, size_t len) {
    computer->arch->deserialize(computer, memory, len, computer->archState, computer->arch->userdata);
}

void nn_lockComputer(nn_computer *computer) {
    nn_lock(computer->lock);
}

void nn_unlockComputer(nn_computer *computer) {
    nn_unlock(computer->lock);
}

void nn_return_nil(nn_computer *computer) {
    nn_return(computer, nn_values_nil());
}

void nn_return_integer(nn_computer *computer, intptr_t integer) {
    nn_return(computer, nn_values_integer(integer));
}

void nn_return_number(nn_computer *computer, double number) {
    nn_return(computer, nn_values_number(number));
}

void nn_return_boolean(nn_computer *computer, bool boolean) {
    nn_return(computer, nn_values_boolean(boolean));
}

void nn_return_cstring(nn_computer *computer, const char *cstr) {
    nn_return(computer, nn_values_cstring(cstr));
}

void nn_return_string(nn_computer *computer, const char *str, size_t len) {
    nn_value val = nn_values_string(&computer->universe->alloc, str, len);
    if(val.tag == NN_VALUE_NIL) {
        nn_setCError(computer, "out of memory");
    }
    nn_return(computer, val);
}

nn_value nn_return_array(nn_computer *computer, size_t len) {
    nn_value val = nn_values_array(&computer->universe->alloc, len);
    if(val.tag == NN_VALUE_NIL) {
        nn_setCError(computer, "out of memory");
    }
    nn_return(computer, val);
    return val;
}

nn_value nn_return_table(nn_computer *computer, size_t len) {
    nn_value val = nn_values_table(&computer->universe->alloc, len);
    if(val.tag == NN_VALUE_NIL) {
        nn_setCError(computer, "out of memory");
    }
    nn_return(computer, val);
    return val;
}
