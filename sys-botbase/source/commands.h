#include <switch.h>
#include <time.h>
#define TOUCHPOLLMIN 15000000L // touch screen polling rate seems to be 15ms (no idea how to change)

extern Handle debughandle;
extern bool bControllerIsInitialised;
extern HiddbgHdlsHandle controllerHandle;
extern HiddbgHdlsDeviceInfo controllerDevice;
extern HiddbgHdlsState controllerState;
extern u64 buttonClickSleepTime;
extern u32 fingerDiameter;

typedef struct {
    u64 main_nso_base;
    u64 heap_base;
    u64 titleID;
    u8 buildID[0x20];
} MetaData;

void attach();
void detach();
u64 getMainNsoBase(u64 pid);
u64 getHeapBase(Handle handle);
u64 getTitleId(u64 pid);
void getBuildID(MetaData* meta, u64 pid);
MetaData getMetaData(void);

void poke(u64 offset, u64 size, u8* val);
void peek(u64 offset, u64 size);
void click(HidNpadButton btn);
void press(HidNpadButton btn);
void release(HidNpadButton btn);
void setStickState(int side, int dxVal, int dyVal);
void dateSkip(int resetTimeAfterSkips, int resetNTP);
void resetTime();
void resetTimeNTP();
void touch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold);
