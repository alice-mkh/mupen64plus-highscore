#include "api/m64p_plugin.h"

int CALL gliden64RomOpen(void);

m64p_error CALL gliden64PluginGetVersion(m64p_plugin_type * _PluginType, int * _PluginVersion, int * _APIVersion, const char ** _PluginNamePtr, int * _Capabilities);

m64p_error CALL gliden64PluginStartup(void *Context, void (*DebugCallback)(void *, int, const char *));

m64p_error CALL gliden64PluginShutdown(void);

void CALL gliden64ReadScreen2(void *dest, int *width, int *height, int front);

void CALL gliden64SetRenderingCallback(void (*callback)(int));

void CALL gliden64ResizeVideoOutput(int width, int height);
