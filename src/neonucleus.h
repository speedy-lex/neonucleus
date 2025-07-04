#ifndef NEONUCLEUS_H
#define NEONUCLEUS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

// Based off https://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
   //define something for Windows (32-bit and 64-bit, this part is common)
   #ifdef _WIN64
        #define NN_WINDOWS
   #else
        #error "Windows 32-bit is not supported"
   #endif
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
        #error "iPhone Emulators are not supported"
    #elif TARGET_OS_MACCATALYST
        // I guess?
        #define NN_MACOS
    #elif TARGET_OS_IPHONE
        #error "iPhone are not supported"
    #elif TARGET_OS_MAC
        #define NN_MACOS
    #else
        #error "Unknown Apple platform"
    #endif
#elif __ANDROID__
    #error "Android is not supported"
#elif __linux__
    #define NN_LINUX
#endif

#if __unix__ // all unices not caught above
    // Unix
    #define NN_UNIX
    #define NN_POSIX
#elif defined(_POSIX_VERSION)
    // POSIX
    #define NN_POSIX
#endif

// The entire C API, in one header

// Magic limits
// If your component needs more than these, redesign your API.

#define NN_MAX_ARGS 32
#define NN_MAX_RETS 32
#define NN_MAX_METHODS 32
#define NN_MAX_USERS 128
#define NN_MAX_ARCHITECTURES 16
#define NN_MAX_SIGNALS 128
#define NN_MAX_SIGNAL_VALS 32
#define NN_MAX_USERDATA 1024
#define NN_MAX_USER_SIZE 128
#define NN_MAX_SIGNAL_SIZE 8192
#define NN_MAX_OPEN_FILES 128
#define NN_MAX_SCREEN_KEYBOARDS 64

#define NN_OVERHEAT_MIN 100
#define NN_CALL_HEAT 0.05
#define NN_CALL_COST 1
#define NN_LABEL_SIZE 128

typedef struct nn_guard nn_guard;
typedef atomic_size_t nn_refc;
typedef struct nn_universe nn_universe;
typedef struct nn_computer nn_computer;
typedef struct nn_component nn_component;
typedef struct nn_componentTable nn_componentTable;
typedef struct nn_architecture {
    void *userdata;
    const char *archName;
    void *(*setup)(nn_computer *computer, void *userdata);
    void (*teardown)(nn_computer *computer, void *state, void *userdata);
    size_t (*getMemoryUsage)(nn_computer *computer, void *state, void *userdata);
    void (*tick)(nn_computer *computer, void *state, void *userdata);
    /* Pointer returned should be allocated with nn_malloc or nn_realloc, so it can be freed with nn_free */
    char *(*serialize)(nn_computer *computer, void *state, void *userdata, size_t *len);
    void (*deserialize)(nn_computer *computer, const char *data, size_t len, void *state, void *userdata);
} nn_architecture;
typedef char *nn_address;

// A non-zero malloc is a null ptr, with a 0 oldSize, but a non-0 newSize.
// A zero malloc is never called, the proc address itself is returned, which is ignored when freeing.
// A free is a non-null ptr, with a non-zero oldSize, but a newSize of 0.
// A realloc is a non-null ptr, with a non-zero oldSize, and a non-zero newSize.
typedef void *nn_AllocProc(void *userdata, void *ptr, size_t oldSize, size_t newSize, void *extra);

typedef struct nn_Alloc {
    void *userdata;
    nn_AllocProc *proc;
} nn_Alloc;

// TODO: write a bunch of utils so this *can* work on baremetal.

#ifndef NN_BAREMETAL
nn_Alloc nn_libcAllocator();
#endif

// Values for architectures

#define NN_VALUE_INT 0
#define NN_VALUE_NUMBER 1
#define NN_VALUE_BOOL 2
#define NN_VALUE_CSTR 3
#define NN_VALUE_STR 4
#define NN_VALUE_ARRAY 5
#define NN_VALUE_TABLE 6
#define NN_VALUE_NIL 7

typedef struct nn_string {
    char *data;
    size_t len;
    size_t refc;
    nn_Alloc alloc;
} nn_string;

typedef struct nn_array {
    struct nn_value *values;
    size_t len;
    size_t refc;
    nn_Alloc alloc;
} nn_array;

typedef struct nn_object {
    struct nn_pair *pairs;
    size_t len;
    size_t refc;
    nn_Alloc alloc;
} nn_table;

typedef struct nn_value {
    size_t tag;
    union {
        intptr_t integer;
        double number;
        bool boolean;
        const char *cstring;
        nn_string *string;
        nn_array *array;
        nn_table *table;
    };
} nn_value;

typedef struct nn_pair {
    nn_value key;
    nn_value val;
} nn_pair;

// we expose the allocator because of some utilities
void *nn_alloc(nn_Alloc *alloc, size_t size);
void *nn_resize(nn_Alloc *alloc, void *memory, size_t oldSize, size_t newSize);
void nn_dealloc(nn_Alloc *alloc, void *memory, size_t size);

// Utilities, both internal and external
char *nn_strdup(nn_Alloc *alloc, const char *s);
void *nn_memdup(nn_Alloc *alloc, const void *buf, size_t len);
void nn_deallocStr(nn_Alloc *alloc, char *s);

nn_guard *nn_newGuard(nn_Alloc *alloc);
void nn_lock(nn_guard *guard);
void nn_unlock(nn_guard *guard);
void nn_deleteGuard(nn_Alloc *alloc, nn_guard *guard);

void nn_addRef(nn_refc *refc, size_t count);
void nn_incRef(nn_refc *refc);
/* Returns true if the object should be freed */
bool nn_removeRef(nn_refc *refc, size_t count);
/* Returns true if the object should be freed */
bool nn_decRef(nn_refc *refc);

bool nn_unicode_validate(const char *s);
// returned string must be nn_deallocStr()'d
char *nn_unicode_char(nn_Alloc *alloc, unsigned int *codepoints, size_t codepointCount);
// returned array must be nn_dealloc()'d
unsigned int *nn_unicode_codepoints(nn_Alloc *alloc, const char *s, size_t *len);
size_t nn_unicode_len(const char *s);
unsigned int nn_unicode_codepointAt(const char *s, size_t byteOffset);
size_t nn_unicode_codepointSize(unsigned int codepoint);
const char *nn_unicode_codepointToChar(unsigned int codepoint, size_t *len);
size_t nn_unicode_charWidth(unsigned int codepoint);
size_t nn_unicode_wlen(const char *s);
unsigned int nn_unicode_upperCodepoint(unsigned int codepoint);
// returned string must be nn_deallocStr()'d
char *nn_unicode_upper(nn_Alloc *alloc, const char *s);
unsigned int nn_unicode_lowerCodepoint(unsigned int codepoint);
// returned string must be nn_deallocStr()'d
char *nn_unicode_lower(nn_Alloc *alloc, const char *s);

double nn_realTime();
double nn_realTimeClock(void *_);
/* Will busy-loop until the time passes. This is meant for computed latencies in components. */
void nn_busySleep(double t);
// calls nn_busySleep with a random latency
void nn_randomLatency(double min, double max);

typedef double nn_clock_t(void *_);

nn_universe *nn_newUniverse(nn_Alloc alloc);
nn_Alloc *nn_getAllocator(nn_universe *universe);
void nn_unsafeDeleteUniverse(nn_universe *universe);
void *nn_queryUserdata(nn_universe *universe, const char *name);
void nn_storeUserdata(nn_universe *universe, const char *name, void *data);
void nn_setClock(nn_universe *universe, nn_clock_t *clock, void *userdata);
double nn_getTime(nn_universe *universe);

nn_computer *nn_newComputer(nn_universe *universe, nn_address address, nn_architecture *arch, void *userdata, size_t memoryLimit, size_t componentLimit);
nn_universe *nn_getUniverse(nn_computer *computer);
int nn_tickComputer(nn_computer *computer);
double nn_getUptime(nn_computer *computer);
size_t nn_getComputerMemoryUsed(nn_computer *computer);
size_t nn_getComputerMemoryTotal(nn_computer *computer);
void *nn_getComputerUserData(nn_computer *computer);
void nn_addSupportedArchitecture(nn_computer *computer, nn_architecture *arch);
nn_architecture *nn_getSupportedArchitecture(nn_computer *computer, size_t idx);
nn_architecture *nn_getArchitecture(nn_computer *computer);
nn_architecture *nn_getNextArchitecture(nn_computer *computer);
void nn_setNextArchitecture(nn_computer *computer, nn_architecture *arch);
void nn_deleteComputer(nn_computer *computer);
const char *nn_pushSignal(nn_computer *computer, nn_value *values, size_t len);
nn_value nn_fetchSignalValue(nn_computer *computer, size_t index);
size_t nn_signalSize(nn_computer *computer);
void nn_popSignal(nn_computer *computer);
const char *nn_addUser(nn_computer *computer, const char *name);
void nn_deleteUser(nn_computer *computer, const char *name);
const char *nn_indexUser(nn_computer *computer, size_t idx);
bool nn_isUser(nn_computer *computer, const char *name);
void nn_setCallBudget(nn_computer *computer, double callBudget);
double nn_getCallBudget(nn_computer *computer);
void nn_callCost(nn_computer *computer, double cost);
double nn_getCallCost(nn_computer *computer);
bool nn_isOverworked(nn_computer *computer);
void nn_triggerIndirect(nn_computer *computer);

/* The memory returned can be freed with nn_free() */
char *nn_serializeProgram(nn_computer *computer, size_t *len);
void nn_deserializeProgram(nn_computer *computer, const char *memory, size_t len);

void nn_lockComputer(nn_computer *computer);
void nn_unlockComputer(nn_computer *computer);

/// This means the computer has not yet started.
#define NN_STATE_SETUP 0

/// This means the computer is running. There is no matching off-state, as the computer is
/// only off when it is deleted.
#define NN_STATE_RUNNING 1

/// This means a component's invocation could not be done due to a crucial resource being busy.
/// The sandbox should yield, then *invoke the component method again.*
#define NN_STATE_BUSY 2

/// This state occurs when a call to removeEnergy has consumed all the energy left.
/// The sandbox should yield, and the runner should shut down the computer.
/// No error is set, the sandbox can set it if it wanted to.
#define NN_STATE_BLACKOUT 3

/// This state only indicates that the runner should turn off the computer, but not due to a blackout.
/// The runner need not bring it back.
#define NN_STATE_CLOSING 4

/// This state indicates that the runner should turn off the computer, but not due to a blackout.
/// The runner should bring it back.
/// By "bring it back", we mean delete the computer, then recreate the entire state.
#define NN_STATE_REPEAT 5

/// This state indciates that the runner should turn off the computer, to switch architectures.
/// The architecture is returned by getNextArchitecture.
#define NN_STATE_SWITCH 6

/// The machine is overworked.
#define NN_STATE_OVERWORKED 7

int nn_getState(nn_computer *computer);
void nn_setState(nn_computer *computer, int state);

void nn_setEnergyInfo(nn_computer *computer, double energy, double capacity);
double nn_getEnergy(nn_computer *computer);
double nn_getMaxEnergy(nn_computer *computer);
void nn_removeEnergy(nn_computer *computer, double energy);
void nn_addEnergy(nn_computer *computer, double amount);

double nn_getTemperature(nn_computer *computer);
double nn_getThermalCoefficient(nn_computer *computer);
double nn_getRoomTemperature(nn_computer *computer);
void nn_setTemperature(nn_computer *computer, double temperature);
void nn_setTemperatureCoefficient(nn_computer *computer, double coefficient);
void nn_setRoomTemperature(nn_computer *computer, double roomTemperature);
void nn_addHeat(nn_computer *computer, double heat);
void nn_removeHeat(nn_computer *computer, double heat);
/* Checks against NN_OVERHEAT_MIN */
bool nn_isOverheating(nn_computer *computer);

// NULL if there is no error.
const char *nn_getError(nn_computer *computer);
void nn_clearError(nn_computer *computer);
void nn_setError(nn_computer *computer, const char *err);
// this version does NOT allocate a copy of err, thus err should come from the data
// segment or memory with the same lifetime as the computer. This may not be possible
// in garbage-collected languages using this API, and thus should be avoided.
// This can be used by low-level implementations of architectures such that any
// internal out-of-memory errors can be reported. The normal setError would report
// no error if allocating the copy failed, and would clear any previous error.
void nn_setCError(nn_computer *computer, const char *err);

// Component stuff

nn_component *nn_newComponent(nn_computer *computer, nn_address address, int slot, nn_componentTable *table, void *userdata);
void nn_setTmpAddress(nn_computer *computer, nn_address tmp);
nn_address nn_getComputerAddress(nn_computer *computer);
nn_address nn_getTmpAddress(nn_computer *computer);
void nn_removeComponent(nn_computer *computer, nn_address address);
void nn_destroyComponent(nn_component *component);
nn_computer *nn_getComputerOfComponent(nn_component *component);
nn_address nn_getComponentAddress(nn_component *component);
int nn_getComponentSlot(nn_component *component);
nn_componentTable *nn_getComponentTable(nn_component *component);
const char *nn_getComponentType(nn_componentTable *table);
void *nn_getComponentUserdata(nn_component *component);
nn_component *nn_findComponent(nn_computer *computer, nn_address address);
// the internal index is not the array index, but rather an index into
// an internal structure. YOU SHOULD NOT ADD OR REMOVE COMPONENTS WHILE ITERATING.
// the internalIndex SHOULD BE INITIALIZED TO 0.
// Returns NULL at the end
nn_component *nn_iterComponent(nn_computer *computer, size_t *internalIndex);

// Component VTable stuff

typedef void *nn_componentConstructor(void *tableUserdata, void *componentUserdata);
typedef void *nn_componentDestructor(void *tableUserdata, nn_component *component, void *componentUserdata);
typedef void nn_componentMethod(void *componentUserdata, void *methodUserdata, nn_component *component, nn_computer *computer);

nn_componentTable *nn_newComponentTable(nn_Alloc *alloc, const char *typeName, void *userdata, nn_componentConstructor *constructor, nn_componentDestructor *destructor);
void nn_destroyComponentTable(nn_componentTable *table);
void nn_defineMethod(nn_componentTable *table, const char *methodName, bool direct, nn_componentMethod *methodFunc, void *methodUserdata, const char *methodDoc);
const char *nn_getTableMethod(nn_componentTable *table, size_t idx, bool *outDirect);
const char *nn_methodDoc(nn_componentTable *table, const char *methodName);

// Component calling

/* Returns false if the method does not exist */
bool nn_invokeComponentMethod(nn_component *component, const char *name);
void nn_simulateBufferedIndirect(nn_component *component, double amount, double amountPerTick);
void nn_resetCall(nn_computer *computer);
void nn_addArgument(nn_computer *computer, nn_value arg);
void nn_return(nn_computer *computer, nn_value val);
nn_value nn_getArgument(nn_computer *computer, size_t idx);
nn_value nn_getReturn(nn_computer *computer, size_t idx);
size_t nn_getArgumentCount(nn_computer *computer);
size_t nn_getReturnCount(nn_computer *computer);

// Value stuff

nn_value nn_values_nil();
nn_value nn_values_integer(intptr_t integer);
nn_value nn_values_number(double num);
nn_value nn_values_boolean(bool boolean);
nn_value nn_values_cstring(const char *string);
nn_value nn_values_string(nn_Alloc *alloc, const char *string, size_t len);
nn_value nn_values_array(nn_Alloc *alloc, size_t len);
nn_value nn_values_table(nn_Alloc *alloc, size_t pairCount);

void nn_return_nil(nn_computer *computer);
void nn_return_integer(nn_computer *computer, intptr_t integer);
void nn_return_number(nn_computer *computer, double number);
void nn_return_boolean(nn_computer *computer, bool boolean);
void nn_return_cstring(nn_computer *computer, const char *cstr);
void nn_return_string(nn_computer *computer, const char *str, size_t len);
nn_value nn_return_array(nn_computer *computer, size_t len);
nn_value nn_return_table(nn_computer *computer, size_t len);

size_t nn_values_getType(nn_value val);
nn_value nn_values_retain(nn_value val);
void nn_values_drop(nn_value val);

void nn_values_set(nn_value arr, size_t idx, nn_value val);
nn_value nn_values_get(nn_value arr, size_t idx);

void nn_values_setPair(nn_value obj, size_t idx, nn_value key, nn_value val);
nn_pair nn_values_getPair(nn_value obj, size_t idx);

intptr_t nn_toInt(nn_value val);
double nn_toNumber(nn_value val);
bool nn_toBoolean(nn_value val);
const char *nn_toCString(nn_value val);
const char *nn_toString(nn_value val, size_t *len);

/*
 * Computes the "packet size" of the values, using the same algorithm as OC.
 * This is used by pushSignal to check the size
 */
size_t nn_measurePacketSize(nn_value *vals, size_t len);

// COMPONENTS

/* Loads the vtables for the default implementations of those components */
void nn_loadCoreComponentTables(nn_universe *universe);

// loading each component
void nn_loadEepromTable(nn_universe *universe);
void nn_loadFilesystemTable(nn_universe *universe);
void nn_loadDriveTable(nn_universe *universe);
void nn_loadScreenTable(nn_universe *universe);
void nn_loadGraphicsCardTable(nn_universe *universe);
void nn_loadKeyboardTable(nn_universe *universe);

nn_component *nn_mountKeyboard(nn_computer *computer, nn_address address, int slot);

// the helpers

// EEPROM
typedef struct nn_eepromControl {
    double readLatency;
    double writeLatency;

    double readEnergyCost;
    double writeEnergyCost;

    double writeHeatCost;

    double randomLatencyMin;
    double randomLatencyMax;

    // Call costs
    size_t readCost;
    size_t writeCost;
} nn_eepromControl;

typedef struct nn_eeprom {
    nn_refc refc;
    void *userdata;
    void (*deinit)(nn_component *component, void *userdata);

    nn_eepromControl (*control)(nn_component *component, void *userdata);

    // methods
    size_t (*getSize)(nn_component *component, void *userdata);
    size_t (*getDataSize)(nn_component *component, void *userdata);
    void (*getLabel)(nn_component *component, void *userdata, char *buf, size_t *buflen);
    size_t (*setLabel)(nn_component *component, void *userdata, const char *buf, size_t buflen);
    size_t (*get)(nn_component *component, void *userdata, char *buf);
    void (*set)(nn_component *component, void *userdata, const char *buf, size_t len);
    int (*getData)(nn_component *component, void *userdata, char *buf);
    void (*setData)(nn_component *component, void *userdata, const char *buf, size_t len);
    bool (*isReadonly)(nn_component *component, void *userdata);
    void (*makeReadonly)(nn_component *component, void *userdata);
} nn_eeprom;
nn_component *nn_addEeprom(nn_computer *computer, nn_address address, int slot, nn_eeprom *eeprom);

// FileSystem
typedef struct nn_filesystemControl {
    int pretendChunkSize;

    // speed
    // used to calculate the latency of seeking a file. It will treat the file as continuous within the storage medium, which is completely
    // unrealistic. Essentially, after a seek, it will check how much the file pointer was changed. If it went backwards, the drive spins
    // in the opposite direction. Drives work the same way.
    int pretendRPM;
    double readLatencyPerChunk;
    double writeLatencyPerChunk;

    // these control *random* latencies that each operation will do
    double randomLatencyMin;
    double randomLatencyMax;

    // thermals
    double motorHeat; // this times how many chunks have been seeked will be the heat addres, +/- the motor heat range.
    double motorHeatRange;
    double writeHeatPerChunk;
    
    // call budget
    size_t readCostPerChunk;
    size_t writeCostPerChunk;
    size_t seekCostPerChunk;

    // energy cost
    double readEnergyCost;
    double writeEnergyCost;
    double motorEnergyCost;
} nn_filesystemControl;

typedef struct nn_filesystem {
    nn_refc refc;
    void *userdata;
    void (*deinit)(nn_component *component, void *userdata);

    nn_filesystemControl (*control)(nn_component *component, void *userdata);
    void (*getLabel)(nn_component *component, void *userdata, char *buf, size_t *buflen);
    size_t (*setLabel)(nn_component *component, void *userdata, const char *buf, size_t buflen);

    size_t (*spaceUsed)(nn_component *component, void *userdata);
    size_t (*spaceTotal)(nn_component *component, void *userdata);
    bool (*isReadOnly)(nn_component *component, void *userdata);

    // general operations
    size_t (*size)(nn_component *component, void *userdata, const char *path);
    bool (*remove)(nn_component *component, void *userdata, const char *path);
    size_t (*lastModified)(nn_component *component, void *userdata, const char *path);
    size_t (*rename)(nn_component *component, void *userdata, const char *from, const char *to);
    bool (*exists)(nn_component *component, void *userdata, const char *path);

    // directory operations
    bool (*isDirectory)(nn_component *component, void *userdata, const char *path);
    bool (*makeDirectory)(nn_component *component, void *userdata, const char *path);
    // The returned array should be allocated with the supplied allocator.
    // The strings should be null terminated. Use nn_strdup for the allocation to guarantee nn_deallocStr deallocates it correctly.
    // For the array, the *exact* size of the allocation should be sizeof(char *) * (*len),
    // If it is not, the behavior is undefined.
    // We recommend first computing len then allocating, though if that is not doable or practical,
    // consider nn_resize()ing it to the correct size to guarantee a correct deallocation.
    char **(*list)(nn_Alloc *alloc, nn_component *component, void *userdata, const char *path, size_t *len);

    // file operations
    size_t (*open)(nn_component *component, void *userdata, const char *path, const char *mode);
    bool (*close)(nn_component *component, void *userdata, int fd);
    bool (*write)(nn_component *component, void *userdata, int fd, const char *buf, size_t len);
    size_t (*read)(nn_component *component, void *userdata, int fd, char *buf, size_t required);
    // moved is an out pointer that says how many bytes the pointer moved.
    size_t (*seek)(nn_component *component, void *userdata, int fd, const char *whence, int off, int *moved);
} nn_filesystem;

nn_filesystem *nn_volatileFileSystem(size_t capacity, nn_filesystemControl *control);
nn_component *nn_addFileSystem(nn_computer *computer, nn_address address, int slot, nn_filesystem *filesystem);

// Drive
typedef struct nn_driveControl {
    // Set it to 0 to disable seek latency.
    int rpm;

    double readLatencyPerSector;
    double writeLatencyPerSector;

    double randomLatencyMin;
    double randomLatencyMax;

    double motorHeat;
    double motorHeatRange;
    double writeHeatPerSector;
    
    // These are per sector
    double motorEnergyCost;
    double readEnergyCost;
    double writeEnergyCost;
    
    // call budget
    size_t readCostPerSector;
    size_t writeCostPerSector;
    size_t seekCostPerSector;
} nn_driveControl;

typedef struct nn_drive {
    nn_refc refc;
    void *userdata;
    void (*deinit)(nn_component *component, void *userdata);
    
    nn_driveControl (*control)(nn_component *component, void *userdata);
    void (*getLabel)(nn_component *component, void *userdata, char *buf, size_t *buflen);
    size_t (*setLabel)(nn_component *component, void *userdata, const char *buf, size_t buflen);

    size_t (*getPlatterCount)(nn_component *component, void *userdata);
    size_t (*getCapacity)(nn_component *component, void *userdata);
    size_t (*getSectorSize)(nn_component *component, void *userdata);

    // sectors start at 1 as per OC.
    void (*readSector)(nn_component *component, void *userdata, int sector, char *buf);
    void (*writeSector)(nn_component *component, void *userdata, int sector, const char *buf);

    // readByte and writeByte will internally use readSector and writeSector. This is to ensure they are handled *consistently.*
    // Also makes the interface less redundant
} nn_drive;

nn_drive *nn_volatileDrive(size_t capacity, size_t platterCount, nn_driveControl *control);
nn_component *nn_addDrive(nn_computer *computer, nn_address address, int slot, nn_drive *drive);

// Screens and GPUs
typedef struct nn_screen nn_screen;

typedef struct nn_scrchr_t {
    unsigned int codepoint;
    int fg;
    int bg;
    bool isFgPalette;
    bool isBgPalette;
} nn_scrchr_t;

nn_screen *nn_newScreen(nn_Alloc *alloc, int maxWidth, int maxHeight, int maxDepth, int editableColors, int paletteColors);
nn_componentTable *nn_getScreenTable(nn_universe *universe);

void nn_retainScreen(nn_screen *screen);
void nn_destroyScreen(nn_screen *screen);

void nn_lockScreen(nn_screen *screen);
void nn_unlockScreen(nn_screen *screen);

void nn_getResolution(nn_screen *screen, int *width, int *height);
void nn_maxResolution(nn_screen *screen, int *width, int *height);
void nn_setResolution(nn_screen *screen, int width, int height);

void nn_getViewport(nn_screen *screen, int *width, int *height);
void nn_setViewport(nn_screen *screen, int width, int height);

void nn_getAspectRatio(nn_screen *screen, int *width, int *height);
void nn_setAspectRatio(nn_screen *screen, int width, int height);

void nn_addKeyboard(nn_screen *screen, nn_address address);
void nn_removeKeyboard(nn_screen *screen, nn_address address);
nn_address nn_getKeyboard(nn_screen *screen, size_t idx);
size_t nn_getKeyboardCount(nn_screen *screen);

void nn_setEditableColors(nn_screen *screen, int count);
int nn_getEditableColors(nn_screen *screen);
void nn_setPaletteColor(nn_screen *screen, int idx, int color);
int nn_getPaletteColor(nn_screen *screen, int idx);
int nn_getPaletteCount(nn_screen *screen);

int nn_maxDepth(nn_screen *screen);
int nn_getDepth(nn_screen *screen);
void nn_setDepth(nn_screen *screen, int depth);

void nn_setPixel(nn_screen *screen, int x, int y, nn_scrchr_t pixel);
nn_scrchr_t nn_getPixel(nn_screen *screen, int x, int y);

bool nn_isDirty(nn_screen *screen);
void nn_setDirty(nn_screen *screen, bool dirty);
bool nn_isPrecise(nn_screen *screen);
void nn_setPrecise(nn_screen *screen, bool precise);
bool nn_isTouchModeInverted(nn_screen *screen);
void nn_setTouchModeInverted(nn_screen *screen, bool touchModeInverted);
bool nn_isOn(nn_screen *buffer);
void nn_setOn(nn_screen *buffer, bool on);

nn_component *nn_addScreen(nn_computer *computer, nn_address address, int slot, nn_screen *screen);

typedef struct nn_gpuControl {
    // resolution and colors
    int maxWidth;
    int maxHeight;
    int maxDepth;
    
    // VRAM Buffers
    int totalVRAM;

    // Energy costs
    double bindEnergy;
    double pixelChangeEnergy;
    double pixelResetEnergy;
    double colorChangeEnergy;
    double vramByteChangeEnergy;
    
    // Heat
    double bindHeat;
    double pixelChangeHeat;
    double pixelResetHeat;
    double colorChangeHeat;
    double vramByteChangeHeat;

    // Call budgets
    size_t bindCost;
    size_t pixelChangeCost;
    size_t pixelResetCost;
    size_t colorChangeCost;
    size_t vramByteChangeCost;

    // Latencies
    double bindLatency;
    double pixelChangeLatency;
    double pixelResetLatency;
    double colorChangeLatency;
    double vramByteChangeLatency;
} nn_gpuControl;

// the control is COPIED.
nn_component *nn_addGPU(nn_computer *computer, nn_address address, int slot, nn_gpuControl *control);

#ifdef __cplusplus
}
#endif

#endif
