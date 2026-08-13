/*
 * Copyright (C) 2023-2026 Alice Mikhaylenko <alicem@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <highscore/libhighscore.h>

#include <mupen64plus/m64p_plugin.h>
#include <mupen64plus/m64p_types.h>

#define AUDIO_API_VERSION 0x20000

typedef void (*HsSampleRateChangedCallback) (HsCore *core, double sample_rate);

static HsCore *core;
static AUDIO_INFO audio_info;
static HsSampleRateChangedCallback sample_rate_callback;

EXPORT m64p_error CALL
PluginStartup (m64p_dynlib_handle  CoreLibHandle,
               void               *Context,
               void (*DebugCallback)(void *, int, const char *))
{
  return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL
PluginShutdown (void)
{
  return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL
PluginGetVersion (m64p_plugin_type  *PluginType,
                  int               *PluginVersion,
                  int               *APIVersion,
                  const char       **PluginNamePtr,
                  int               *Capabilities)
{
  if (PluginType != NULL)
    *PluginType = M64PLUGIN_AUDIO;

  if (PluginVersion != NULL)
    *PluginVersion = 0x00010000;

  if (APIVersion != NULL)
    *APIVersion = AUDIO_API_VERSION;

  if (PluginNamePtr != NULL)
    *PluginNamePtr = "Highscore-Audio";

  if (Capabilities != NULL)
    *Capabilities = 0;

  return M64ERR_SUCCESS;
}

EXPORT int CALL
RomOpen (void)
{
  return 1;
}

EXPORT void CALL
RomClosed (void)
{
  return;
}

EXPORT void CALL
AiDacrateChanged (int SystemType)
{
  guint clock_rate;
  switch (SystemType) {
  case SYSTEM_PAL:
    clock_rate = 49656530;
    break;
  case SYSTEM_MPAL:
    clock_rate = 48628316;
    break;
  case SYSTEM_NTSC:
  default:
    clock_rate = 48681812;
    break;
  }

  sample_rate_callback (core, clock_rate / (*audio_info.AI_DACRATE_REG + 1));
}

EXPORT void CALL
AiLenChanged (void)
{
  int len_reg = *audio_info.AI_LEN_REG;
  uint8_t *samples = (uint8_t*) (audio_info.RDRAM + (*audio_info.AI_DRAM_ADDR_REG & 0xFFFFFF));

  // Swap left and right channel
  for (uint32_t i = 0; i < (uint32_t) len_reg; i += 4) {
    samples[i] ^= samples[i + 2];
    samples[i + 2] ^= samples[i];
    samples[i] ^= samples[i + 2];
    samples[i + 1] ^= samples[i + 3];
    samples[i + 3] ^= samples[i + 1];
    samples[i + 1] ^= samples[i + 3];
  }

  hs_core_play_samples (HS_CORE (core), (int16_t *) samples, len_reg / sizeof (int16_t));
}

EXPORT int CALL
InitiateAudio (AUDIO_INFO Audio_Info)
{
  audio_info = Audio_Info;

  return 1;
}

EXPORT void CALL
ProcessAList (void)
{
  return;
}

EXPORT void CALL
SetSpeedFactor (int percent)
{
  return;
}

EXPORT void CALL
VolumeUp (void)
{
  return;
}

EXPORT void CALL
VolumeDown (void)
{
  return;
}

EXPORT int CALL
VolumeGetLevel (void)
{
  return 0;
}

EXPORT void CALL
VolumeSetLevel (int level)
{
  return;
}

EXPORT void CALL
VolumeMute (void)
{
  return;
}

EXPORT const char * CALL
VolumeGetString (void)
{
  return "disabled";
}

EXPORT void CALL
hs_setup_audio (HsCore *c, HsSampleRateChangedCallback sample_rate_cb)
{
  core = c;
  sample_rate_callback = sample_rate_cb;
}
