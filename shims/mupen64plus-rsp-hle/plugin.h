#include "osal/dynamiclib.h"

m64p_error CALL hlePluginStartup(void *Context, void (*DebugCallback)(void *, int, const char *));

m64p_error CALL hlePluginShutdown(void);

m64p_error CALL hlePluginGetVersion(m64p_plugin_type *PluginType, int *PluginVersion, int *APIVersion, const char **PluginNamePtr, int *Capabilities);

unsigned int CALL hleDoRspCycles(unsigned int Cycles);

void CALL hleInitiateRSP(RSP_INFO Rsp_Info, unsigned int* CycleCount);

void CALL hleRomClosed(void);
