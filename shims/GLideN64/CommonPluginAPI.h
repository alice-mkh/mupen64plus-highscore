#include "api/m64p_plugin.h"

int CALL gliden64InitiateGFX (GFX_INFO Gfx_Info);

void CALL gliden64MoveScreen (int xpos, int ypos);

void CALL gliden64ProcessDList(void);

void CALL gliden64ProcessRDPList(void);

void CALL gliden64RomClosed (void);

void CALL gliden64ShowCFB (void);

void CALL gliden64UpdateScreen (void);

void CALL gliden64ViStatusChanged (void);

void CALL gliden64ViWidthChanged (void);

void CALL gliden64ChangeWindow(void);

void CALL gliden64FBWrite(unsigned int addr, unsigned int size);

void CALL gliden64FBRead(unsigned int addr);

void CALL gliden64FBGetFrameBufferInfo(void *pinfo);
