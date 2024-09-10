#include <highscore/libhighscore.h>
#include <stdlib.h>
#include <math.h>

#include <mupen64plus/m64p_plugin.h>
#include <mupen64plus/m64p_types.h>

#define INPUT_API_VERSION 0x20101

#define RD_GETSTATUS        0x00   // get status
#define RD_READKEYS         0x01   // read button values
#define RD_READPAK          0x02   // read from controllerpack
#define RD_WRITEPAK         0x03   // write to controllerpack
#define RD_RESETCONTROLLER  0xff   // reset controller
#define RD_READEEPROM       0x04   // read eeprom
#define RD_WRITEEPROM       0x05   // write eeprom

#define PAK_IO_RUMBLE       0xC000 // the address where rumble-commands are sent to

static HsCore *core;
// Lock input_mutex before accessing
static CONTROL_INFO control_info;
// Lock input_mutex before accessing
static BUTTONS button_state[4];
static GMutex input_mutex;

EXPORT m64p_error CALL
PluginStartup (m64p_dynlib_handle  CoreLibHandle,
               void               *Context,
               void (*DebugCallback)(void *, int, const char *))
{
  g_mutex_init (&input_mutex);

  return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL
PluginShutdown (void)
{
  g_mutex_clear (&input_mutex);

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
    *PluginType = M64PLUGIN_INPUT;

  if (PluginVersion != NULL)
    *PluginVersion = 0x00010000;

  if (APIVersion != NULL)
    *APIVersion = INPUT_API_VERSION;

  if (PluginNamePtr != NULL)
    *PluginNamePtr = "Highscore-Input";

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

static void
start_rumble_cb (gpointer data)
{
  guint player = (guint) GPOINTER_TO_INT (data);

  hs_core_rumble (core, player, 1, 1);
}

static void
stop_rumble_cb (gpointer data)
{
  guint player = (guint) GPOINTER_TO_INT (data);

  hs_core_rumble (core, player, 0, 0);
}

static unsigned char
data_crc (unsigned char *data, int length)
{
  unsigned char remainder = data[0];

  int byte = 1;
  unsigned char bit = 0;

  while (byte <= length) {
    int highBit = ((remainder & 0x80) != 0);
    remainder = remainder << 1;

    remainder += (byte < length && data[byte] & (0x80 >> bit )) ? 1 : 0;

    remainder ^= (highBit) ? 0x85 : 0;

    bit++;
    byte += bit/8;
    bit %= 8;
  }

  return remainder;
}

EXPORT void CALL
ControllerCommand (int Control, unsigned char *Command)
{
  unsigned char *data = &Command[5];

  if (Control == -1)
      return;

  switch (Command[2]) {
  case RD_GETSTATUS:
    break;
  case RD_READKEYS:
    break;
  case RD_READPAK:
    if (control_info.Controls[Control].Plugin == PLUGIN_RAW) {
      unsigned int dwAddress = (Command[3] << 8) + (Command[4] & 0xE0);

      if (dwAddress >= 0x8000 && dwAddress < 0x9000)
        memset (data, 0x80, 32);
      else
        memset (data, 0x00, 32);

      data[32] = data_crc (data, 32);
    }
    break;
  case RD_WRITEPAK:
    if (control_info.Controls[Control].Plugin == PLUGIN_RAW) {
      unsigned int dwAddress = (Command[3] << 8) + (Command[4] & 0xE0);
      data[32] = data_crc (data, 32);

      if (dwAddress == PAK_IO_RUMBLE) {
        if (*data)
          g_idle_add_once ((GSourceOnceFunc) start_rumble_cb, GINT_TO_POINTER (Control));
        else
          g_idle_add_once ((GSourceOnceFunc) stop_rumble_cb, GINT_TO_POINTER (Control));
      }
    }
    break;
  case RD_RESETCONTROLLER:
     break;
  case RD_READEEPROM:
    break;
  case RD_WRITEEPROM:
    break;
  }
}

EXPORT void CALL
GetKeys (int Control, BUTTONS *Keys)
{
  g_mutex_lock (&input_mutex);
  *Keys = button_state[Control];
  g_mutex_unlock (&input_mutex);
}

EXPORT void CALL
InitiateControllers (CONTROL_INFO ControlInfo)
{
  g_mutex_lock (&input_mutex);
  control_info = ControlInfo;
  g_mutex_unlock (&input_mutex);
}

EXPORT void CALL
ReadController (int Control, unsigned char *Command)
{
}

EXPORT void CALL
SDL_KeyDown (int keymod, int keysym)
{
}

EXPORT void CALL
SDL_KeyUp (int keymod, int keysym)
{
}

EXPORT void CALL
RenderCallback (void)
{
}

EXPORT void CALL
SendVRUWord (uint16_t length, uint16_t *word, uint8_t lang)
{
}

EXPORT void CALL
SetMicState (int state)
{
}

EXPORT void CALL
ReadVRUResults (uint16_t *error_flags, uint16_t *num_results, uint16_t *mic_level, uint16_t *voice_level, uint16_t *voice_length, uint16_t *matches)
{
}

EXPORT void CALL
ClearVRUWords(uint8_t length)
{
}

EXPORT void CALL
SetVRUWordMask (uint8_t length, uint8_t *mask)
{
}

EXPORT void CALL
hs_setup_input (HsCore *c)
{
  core = c;
}

const uint8_t PAD_BUTTON_OFFSETS[] = {
  0x03, // U_DPAD
  0x02, // D_DPAD
  0x01, // L_DPAD
  0x00, // R_DPAD
  0x07, // A_BUTTON
  0x06, // B_BUTTON
  0x0B, // U_CBUTTON
  0x0A, // D_CBUTTON
  0x09, // L_CBUTTON
  0x08, // R_CBUTTON
  0x0D, // L_TRIG
  0x0C, // R_TRIG
  0x05, // Z_TRIG
  0x04, // START_BUTTON
};

EXPORT void CALL
hs_poll_input (HsInputState *input_state)
{
  g_mutex_lock (&input_mutex);

  for (int player = 0; player < HS_NINTENDO_64_MAX_PLAYERS; player++) {
    uint32_t buttons = input_state->nintendo_64.pad_buttons[player];

    for (int btn = 0; btn < HS_NINTENDO_64_N_BUTTONS; btn++) {
      if (buttons & 1 << btn)
        button_state[player].Value |= 1 << PAD_BUTTON_OFFSETS[btn];
      else
        button_state[player].Value &= ~(1 << PAD_BUTTON_OFFSETS[btn]);
    }

    double x = input_state->nintendo_64.pad_control_stick_x[player];
    double y = input_state->nintendo_64.pad_control_stick_y[player];

    button_state[player].X_AXIS = (int8_t) round (x * 80);
    button_state[player].Y_AXIS = (int8_t) round (y * -80); // Y axis is inverted compared to the API
  }

  g_mutex_unlock (&input_mutex);
}

EXPORT void CALL
hs_set_controller (guint player, gboolean present, HsNintendo64Pak pak)
{
  g_mutex_lock (&input_mutex);

  control_info.Controls[player].Present = present ? 1 : 0;

  switch (pak) {
  case HS_NINTENDO_64_PAK_NONE:
    control_info.Controls[player].Plugin = PLUGIN_NONE;
    break;
  case HS_NINTENDO_64_PAK_MEMORY_PAK:
    control_info.Controls[player].Plugin = PLUGIN_MEMPAK;
    break;
  case HS_NINTENDO_64_PAK_RUMBLE_PAK:
    control_info.Controls[player].Plugin = PLUGIN_RAW;
    break;
  default:
    g_assert_not_reached ();
  }

  if (pak != HS_NINTENDO_64_PAK_RUMBLE_PAK)
    hs_core_rumble (core, player, 0, 0);

  g_mutex_unlock (&input_mutex);
}
