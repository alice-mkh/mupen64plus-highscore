#define M64P_CORE_PROTOTYPES 1

#include "mupen64plus-highscore.h"

#include "api/m64p_common.h"
#include "api/m64p_config.h"
#include "api/m64p_frontend.h"
#include "api/m64p_plugin.h"
#include "api/m64p_types.h"
// Needed for ROM_PARAMS.systemtype and ROM_PARAMS.headername
#include "main/rom.h"
// Needed for manually attaching plugins
#include "plugin/plugin.h"

#include <math.h>
#include <stdio.h>

#include "shims/GLideN64/CommonPluginAPI.h"
#include "shims/GLideN64/MupenPlusPluginAPI.h"
#include "shims/mupen64plus-rsp-hle/plugin.h"

#define SAMPLE_RATE 33600

static Mupen64PlusCore *core;

struct _Mupen64PlusCore
{
  HsCore parent_instance;

  HsGLContext *context;
  GThread *emulation_thread;

  AUDIO_INFO audio_info;
  double sample_rate;
  // Lock audio_mutex before accessing
  double new_sample_rate;
  GMutex audio_mutex;

  // Lock controls_mutex before accessing
  CONTROL_INFO control_info;
  // Lock controls_mutex before accessing
  BUTTONS button_state[4];
  GMutex input_mutex;

  m64p_rom_settings rom_settings;

  // Access only with g_atomic_int_*()
  gboolean paused;

  // Lock savestate_mutex before accessing
  HsStateCallback savestate_callback;
  // Lock savestate_mutex before accessing
  gboolean savestate_load;
  // Lock savestate_mutex before accessing
  int savestate_result;
  GMutex savestate_mutex;
};

static void mupen64plus_nintendo_64_core_init (HsNintendo64CoreInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (Mupen64PlusCore, mupen64plus_core, HS_TYPE_CORE,
                               G_IMPLEMENT_INTERFACE (HS_TYPE_NINTENDO_64_CORE, mupen64plus_nintendo_64_core_init))

static void
debug_callback (gpointer context, int level, const char *message)
{
  // Since we're not using the regular plugin loading mechanism, the core will think plugins
  // the plugins aren't attached and will warn about that. Silence those warnings.
  if (g_str_equal (message, "No video plugin attached.  There will be no video output.") ||
      g_str_equal (message, "No RSP plugin attached.  The video output will be corrupted.") ||
      g_str_equal (message, "No audio plugin attached.  There will be no sound output.") ||
      g_str_equal (message, "No input plugin attached.  You won't be able to control the game.")) {
    return;
  }

  HsLogLevel hs_level;

  switch (level) {
  case M64MSG_ERROR:
    hs_level = HS_LOG_CRITICAL;
    break;
  case M64MSG_WARNING:
    hs_level = HS_LOG_WARNING;
    break;
  case M64MSG_INFO:
    hs_level = HS_LOG_INFO;
    break;
  case M64MSG_STATUS:
  case M64MSG_VERBOSE:
  default:
    hs_level = HS_LOG_DEBUG;
    break;
  }

  hs_core_log (HS_CORE (core), hs_level, message);
}

static void
finish_savestate_cb (Mupen64PlusCore *self)
{
  g_mutex_lock (&self->savestate_mutex);

  int result = self->savestate_result;

  if (result) {
    self->savestate_callback (HS_CORE (self), NULL);
  } else {
    GError *error;

    g_set_error (&error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL,
                 "Failed to %s state", self->savestate_load ? "load" : "save");

    self->savestate_callback (HS_CORE (self), &error);
  }

  self->savestate_callback = NULL;
  self->savestate_load = FALSE;
  self->savestate_result = -1;

  g_mutex_unlock (&self->savestate_mutex);

  g_object_unref (self);
}

static void
state_callback (gpointer context, m64p_core_param param_type, int new_value)
{
  if (param_type == M64CORE_STATE_LOADCOMPLETE || param_type == M64CORE_STATE_SAVECOMPLETE) {
    g_mutex_lock (&core->savestate_mutex);
    core->savestate_result = new_value;
    g_idle_add_once ((GSourceOnceFunc) finish_savestate_cb, core);
    g_mutex_unlock (&core->savestate_mutex);
  }
}

m64p_error
video_init (void)
{
  hs_gl_context_realize (core->context);

  return M64ERR_SUCCESS;
}

m64p_error
video_quit (void)
{
  hs_gl_context_unrealize (core->context);

  return M64ERR_SUCCESS;
}

m64p_error
video_list_modes (m64p_2d_size *sizes, int *n_sizes)
{
  *n_sizes = 0;
  return M64ERR_SUCCESS;
}

m64p_error
video_list_rates (m64p_2d_size size, int *rates, int *n_rates)
{
  return M64ERR_UNSUPPORTED;
}

m64p_error
video_set_mode (int width, int height, int bpp, int mode, int flags)
{
  hs_gl_context_set_size (core->context, width, height);

  return M64ERR_SUCCESS;
}

m64p_error
video_set_mode_with_rate (int width, int height, int refresh_rate, int bpp, int mode, int flags)
{
  return M64ERR_UNSUPPORTED;
}

m64p_function
video_gl_get_proc (const char *name)
{
  return hs_gl_context_get_proc_address (core->context, name);
}

m64p_error
video_gl_set_attr (m64p_GLattr attr, int value)
{
  return M64ERR_SUCCESS;
}

m64p_error
video_gl_get_attr (m64p_GLattr attr, int *value)
{
  // Just hardcode GLideN64's values. If we switch to another plugin
  // in future, we'll just update these, but it's much simpler than
  // making frontend handle all of these, and as a separate calls too,
  // with no notification when the plugin is done setting them.
  int values[] = {
    1,  // M64P_GL_DOUBLEBUFFER
    32, // M64P_GL_BUFFER_SIZE
    16, // M64P_GL_DEPTH_SIZE
    8,  // M64P_GL_RED_SIZE
    8,  // M64P_GL_GREEN_SIZE
    8,  // M64P_GL_BLUE_SIZE
    8,  // M64P_GL_ALPHA_SIZE
    0,  // M64P_GL_SWAP_CONTROL
    0,  // M64P_GL_MULTISAMPLEBUFFERS
    0,  // M64P_GL_MULTISAMPLESAMPLES
    3,  // M64P_GL_CONTEXT_MAJOR_VERSION
    3,  // M64P_GL_CONTEXT_MINOR_VERSION
    M64P_GL_CONTEXT_PROFILE_CORE,
  };

  *value = values[attr - 1];

  return M64ERR_SUCCESS;
}

m64p_error
video_gl_swap_buf (void)
{
  // Occasionally we get black screen when pausing, we don't want that
  if (!g_atomic_int_get (&core->paused))
    hs_gl_context_swap_buffers (core->context);

  return M64ERR_SUCCESS;
}

m64p_error
video_set_caption (const char *caption)
{
  return M64ERR_UNSUPPORTED;
}

m64p_error
video_toggle_fs (void)
{
  return M64ERR_UNSUPPORTED;
}

m64p_error
video_resize_window (int width, int height)
{
  return M64ERR_UNSUPPORTED;
}

uint32_t
video_gl_get_default_framebuffer (void)
{
  return 0;
}

m64p_error
video_init_with_render_mode (m64p_render_mode mode)
{
  if (mode == M64P_RENDER_OPENGL)
    return video_init ();

  return M64ERR_UNSUPPORTED;
}

m64p_error
video_vk_get_surface (void **surface, void *instance)
{
  return M64ERR_UNSUPPORTED;
}

m64p_error
video_vk_get_instance_extensions (const char **extensions[], uint32_t *n_extensions)
{
  return M64ERR_UNSUPPORTED;
}

static void
audio_ai_dacrate_changed (int system_type)
{
  guint clock_rate;
  switch(system_type) {
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

  g_mutex_lock (&core->audio_mutex);
  core->new_sample_rate = clock_rate / (*core->audio_info.AI_DACRATE_REG + 1);
  g_mutex_unlock (&core->audio_mutex);
}

static void
audio_ai_len_changed (void)
{
  int len_reg = *core->audio_info.AI_LEN_REG;
  uint8_t *samples = (uint8_t*) (core->audio_info.RDRAM + (*core->audio_info.AI_DRAM_ADDR_REG & 0xFFFFFF));

  // Swap left and right channel
  for (uint32_t i = 0; i < len_reg; i += 4) {
    samples[i] ^= samples[i + 2];
    samples[i + 2] ^= samples[i];
    samples[i] ^= samples[i + 2];
    samples[i + 1] ^= samples[i + 3];
    samples[i + 3] ^= samples[i + 1];
    samples[i + 1] ^= samples[i + 3];
  }

  hs_core_play_samples (HS_CORE (core), (int16_t *) samples, len_reg / sizeof (int16_t));
}

static int
audio_initiate_audio (AUDIO_INFO info)
{
  core->audio_info = info;

  return 1;
}

void
input_initiate_controllers (CONTROL_INFO info)
{
  g_mutex_lock (&core->input_mutex);
  core->control_info = info;
  core->control_info.Controls[0].Present = 1;
  core->control_info.Controls[0].Plugin = PLUGIN_MEMPAK;
  g_mutex_unlock (&core->input_mutex);
}

void
input_get_keys (int control, BUTTONS *keys)
{
  g_mutex_lock (&core->input_mutex);
  *keys = core->button_state[control];
  g_mutex_unlock (&core->input_mutex);
}

static gpointer
run_emulation_thread (Mupen64PlusCore *self)
{
  CoreDoCommand (M64CMD_EXECUTE, 0, NULL);

  return NULL;
}

#define EEPROM_SIZE 0x800
#define MEMPAKS_SIZE 0x8000 * 4
#define SRAM_SIZE 0x8000
#define FLASHRAM_SIZE 0x20000

static int
string_replace_chars (char* str, const char* chars, const char r)
{
  int i, y;
  int str_size, chars_size;
  int replacements = 0;

  str_size   = strlen (str);
  chars_size = strlen (chars);

  for (i = 0; i < str_size; i++) {
    for (y = 0; y < chars_size; y++) {
      if (str[i] == chars[y]) {
        str[i] = r;
        replacements++;
        break;
      }
    }
  }

  return replacements;
}

static char *
get_save_filename (Mupen64PlusCore *self)
{
  char *filename = g_new0 (char, 256);

  m64p_handle config;
  ConfigOpenSection ("Core", &config);
  int format = ConfigGetParamInt (config, "SaveFilenameFormat");

  if (format == 0) {
    snprintf (filename, 256, "%s", ROM_PARAMS.headername);
  } else /* if (format == 1) */ {
    if (strstr (ROM_SETTINGS.goodname, "(unknown rom)") == NULL) {
      snprintf (filename, 256, "%.32s-%.8s", self->rom_settings.goodname, self->rom_settings.MD5);
    } else if (ROM_HEADER.Name[0] != 0) {
      snprintf (filename, 256, "%s-%.8s", ROM_PARAMS.headername, self->rom_settings.MD5);
    } else {
      snprintf (filename, 256, "unknown-%.8s", self->rom_settings.MD5);
    }
  }

  /* sanitize filename */
  string_replace_chars (filename, ":<>\"/\\|?*", '_');

  return filename;
}

static gboolean
try_migrate_libretro_save (Mupen64PlusCore  *self,
                           const char       *save_path,
                           const char       *cache_path,
                           GError          **error)
{
  g_autoptr (GFile) save_file = g_file_new_for_path (save_path);

  if (!g_file_query_exists (save_file, NULL)) {
    // No save file, nothing to do here
    return TRUE;
  }

  g_autoptr (GFileInfo) save_info =
    g_file_query_info (save_file,
                       G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                       G_FILE_ATTRIBUTE_STANDARD_SIZE,
                       G_FILE_QUERY_INFO_NONE, NULL, error);

  if (save_info == NULL)
    return FALSE;

  if (g_file_info_get_file_type (save_info) == G_FILE_TYPE_DIRECTORY) {
    // This is a new save file, all good
    return TRUE;
  }

  if (g_file_info_get_size (save_info) != EEPROM_SIZE + MEMPAKS_SIZE + SRAM_SIZE + FLASHRAM_SIZE) {
    // This is not a libretro save
    return TRUE;
  }

  // Make a temporary dir
  g_autofree char *tmp_path = g_build_filename (cache_path, "libretro-save-XXXXXX", NULL);
  tmp_path = g_mkdtemp (tmp_path);
  g_autoptr (GFile) tmp_file = g_file_new_for_path (tmp_path);

  // Backup the old save
  g_autoptr (GFile) tmp_backup_file = g_file_get_child (tmp_file, "libretro-backup");
  if (!g_file_copy (save_file, tmp_backup_file, G_FILE_COPY_BACKUP, NULL, NULL, NULL, error))
    return FALSE;

  // Clean up the old save file and create a directory there instead. Copy the backup there
  if (!g_file_delete (save_file, NULL, error))
    return FALSE;

  if (!g_file_make_directory_with_parents (save_file, NULL, error))
    return FALSE;

  g_autoptr (GFile) backup_file = g_file_get_child (save_file, "libretro-backup");
  if (!g_file_move (tmp_backup_file, backup_file, G_FILE_COPY_BACKUP, NULL, NULL, NULL, error))
    return FALSE;

  if (!g_file_delete (tmp_file, NULL, error))
    return FALSE;

  // Load the old file
  g_autofree char *contents = NULL;
  gsize contents_length;
  if (!g_file_load_contents (backup_file, NULL, &contents, &contents_length, NULL, error))
    return FALSE;

  g_assert (contents_length == EEPROM_SIZE + MEMPAKS_SIZE + SRAM_SIZE + FLASHRAM_SIZE);

  // EEPROM
  {
    g_autofree char *basename = get_save_filename (self);
    g_autofree char *new_name = g_strconcat (basename, ".eep", NULL);
    g_autoptr (GFile) new_file = g_file_get_child (save_file, new_name);

    if (!g_file_replace_contents (new_file, contents, EEPROM_SIZE, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, error))
      return FALSE;
  }

  // Memory Paks
  {
    g_autofree char *basename = get_save_filename (self);
    g_autofree char *new_name = g_strconcat (basename, ".mpk", NULL);
    g_autoptr (GFile) new_file = g_file_get_child (save_file, new_name);

    if (!g_file_replace_contents (new_file, contents + EEPROM_SIZE, MEMPAKS_SIZE, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, error))
      return FALSE;
  }

  // SRAM
  {
    g_autofree char *basename = get_save_filename (self);
    g_autofree char *new_name = g_strconcat (basename, ".sra", NULL);
    g_autoptr (GFile) new_file = g_file_get_child (save_file, new_name);

    if (!g_file_replace_contents (new_file, contents + EEPROM_SIZE + MEMPAKS_SIZE, SRAM_SIZE, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, error))
      return FALSE;
  }

  // Flash RAM
  {
    g_autofree char *basename = get_save_filename (self);
    g_autofree char *new_name = g_strconcat (basename, ".fla", NULL);
    g_autoptr (GFile) new_file = g_file_get_child (save_file, new_name);

    if (!g_file_replace_contents (new_file, contents + EEPROM_SIZE + MEMPAKS_SIZE + SRAM_SIZE, FLASHRAM_SIZE, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, error))
      return FALSE;
  }

  // All done
  g_autofree char *backup_path = g_file_get_path (backup_file);
  g_autofree char *message = g_strdup_printf ("Libretro save file migrated successfully. A backup has been made in %s", backup_path);
  hs_core_log (HS_CORE (self), HS_LOG_MESSAGE, message);

  return TRUE;
}

static gboolean
mupen64plus_core_start (HsCore      *core,
                        const char  *rom_path,
                        const char  *save_path,
                        GError     **error)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);
  char *data;
  gsize length;

  if (!g_file_get_contents (rom_path, &data, &length, error))
    return FALSE;

  int api_version;
  if (PluginGetVersion (NULL, NULL, &api_version, NULL, NULL) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to fetch to core API version");

    return FALSE;
  }

  g_autofree char *cache_path = hs_core_get_cache_path (core);

  if (CoreStartup (api_version, /* ConfigPath */ save_path, /* DataPath */ CORE_DIR,
                   (gpointer) self, debug_callback, (gpointer) self, state_callback) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to start the core");

    return FALSE;
  }

  m64p_handle config;
  ConfigOpenSection ("Core", &config);
  ConfigSetParameter (config, "SaveSRAMPath", M64TYPE_STRING, save_path);
  ConfigSaveSection ("Core");

  ConfigSaveSection ("CoreEvents");

  if (ConfigOverrideUserPaths (/* DataPath */ save_path, /* CachePath */ cache_path) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to override user paths");

    return FALSE;
  }

  if (CoreDoCommand (M64CMD_ROM_OPEN, (int) length, (gpointer) data) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_COULDNT_LOAD_ROM, "Failed to load ROM");

    return FALSE;
  }

  if (CoreDoCommand (M64CMD_ROM_GET_SETTINGS, sizeof(m64p_rom_settings), &self->rom_settings) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to retrieve ROM settings");

    return FALSE;
  }

  if (!try_migrate_libretro_save (self, save_path, cache_path, error))
    return FALSE;

  // Set up video
  self->context = hs_core_create_gl_context (core,
                                             HS_GL_PROFILE_CORE,
                                             3, 3,
                                             HS_GL_FLAGS_DEPTH);

  m64p_video_extension_functions override = {
    17, // The number of functions below
    video_init,
    video_quit,
    video_list_modes,
    video_list_rates,
    video_set_mode,
    video_set_mode_with_rate,
    video_gl_get_proc,
    video_gl_set_attr,
    video_gl_get_attr,
    video_gl_swap_buf,
    video_set_caption,
    video_toggle_fs,
    video_resize_window,
    video_gl_get_default_framebuffer,
    video_init_with_render_mode,
    video_vk_get_surface,
    video_vk_get_instance_extensions,
  };

  if (CoreOverrideVidExt (&override) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to set up video rendering");

    return FALSE;
  }

  gfx.getVersion = gliden64PluginGetVersion;
  gfx.changeWindow = gliden64ChangeWindow;
  gfx.initiateGFX = gliden64InitiateGFX;
  gfx.moveScreen = gliden64MoveScreen;
  gfx.processDList = gliden64ProcessDList;
  gfx.processRDPList = gliden64ProcessRDPList;
  gfx.romClosed = gliden64RomClosed;
  gfx.romOpen = gliden64RomOpen;
  gfx.showCFB = gliden64ShowCFB;
  gfx.updateScreen = gliden64UpdateScreen;
  gfx.viStatusChanged = gliden64ViStatusChanged;
  gfx.viWidthChanged = gliden64ViWidthChanged;
  gfx.readScreen = gliden64ReadScreen2;
  gfx.setRenderingCallback = gliden64SetRenderingCallback;
  gfx.resizeVideoOutput = gliden64ResizeVideoOutput;
  gfx.fBRead = gliden64FBRead;
  gfx.fBWrite = gliden64FBWrite;
  gfx.fBGetFrameBufferInfo = gliden64FBGetFrameBufferInfo;
  if (plugin_start (M64PLUGIN_GFX) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to start video plugin");

    return FALSE;
  }
  gliden64PluginStartup ((gpointer) self, debug_callback);

  // Change default resolution to match N64
  ConfigOpenSection ("Video-General", &config);
  int value = 320;
  ConfigSetParameter (config, "ScreenWidth", M64TYPE_INT, &value);
  value = 240;
  ConfigSetParameter (config, "ScreenHeight", M64TYPE_INT, &value);
  ConfigSaveSection ("Video-General");

  // Set up audio
  audio.aiDacrateChanged = audio_ai_dacrate_changed;
  audio.aiLenChanged = audio_ai_len_changed;
  audio.initiateAudio = audio_initiate_audio;
  if (plugin_start (M64PLUGIN_AUDIO) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to start audio plugin");

    return FALSE;
  }

  // Set up input
  input.getKeys = input_get_keys;
  input.initiateControllers = input_initiate_controllers;
  if (plugin_start (M64PLUGIN_INPUT) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to start input plugin");

    return FALSE;
  }

  // Set up RSP
  rsp.getVersion = hlePluginGetVersion;
  rsp.doRspCycles = hleDoRspCycles;
  rsp.initiateRSP = hleInitiateRSP;
  rsp.romClosed = hleRomClosed;
  if (plugin_start (M64PLUGIN_RSP) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to start RSP plugin");

    return FALSE;
  }
  hlePluginStartup ((gpointer) self, debug_callback);

  self->emulation_thread = g_thread_new ("Mupen64Plus emulation thread",
                                         (GThreadFunc) run_emulation_thread, self);

  return TRUE;
}

static void
mupen64plus_core_run_frame (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  g_mutex_lock (&self->audio_mutex);

  if (self->new_sample_rate > 0) {
    self->sample_rate = self->new_sample_rate;
    self->new_sample_rate = -1;
  }

  g_mutex_unlock (&self->audio_mutex);

  if (g_atomic_int_get (&self->paused))
    CoreDoCommand (M64CMD_ADVANCE_FRAME, 0, NULL);
}

static void
mupen64plus_core_reset (HsCore *core)
{
  CoreDoCommand (M64CMD_RESET, 1, NULL);
}

static void
mupen64plus_core_stop (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  if (CoreDoCommand (M64CMD_STOP, 0, NULL) != M64ERR_SUCCESS)
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to stop core");

  g_clear_pointer (&self->emulation_thread, g_thread_join);
  g_clear_object (&self->context);

  if (CoreDoCommand (M64CMD_ROM_CLOSE, 0, NULL) != M64ERR_SUCCESS)
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to close ROM");

  if (gliden64PluginShutdown () != M64ERR_SUCCESS)
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to shut down GFX plugin");

  if (hlePluginShutdown () != M64ERR_SUCCESS)
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to shut down RSP plugin");

  if (CoreShutdown () != M64ERR_SUCCESS)
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to shut down the core");
}

static void
mupen64plus_core_pause (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  g_atomic_int_set (&self->paused, TRUE);

  CoreDoCommand (M64CMD_PAUSE, 0, NULL);
}

static void
mupen64plus_core_resume (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  CoreDoCommand (M64CMD_RESUME, 0, NULL);

  g_atomic_int_set (&self->paused, FALSE);
}

static void
mupen64plus_core_load_state (HsCore          *core,
                             const char      *path,
                             HsStateCallback  callback)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  g_mutex_lock (&self->savestate_mutex);
  self->savestate_callback = callback;
  self->savestate_load = TRUE;
  g_mutex_unlock (&self->savestate_mutex);

  g_object_ref (self);

  if (CoreDoCommand (M64CMD_STATE_LOAD, 1, (gpointer) path) != M64ERR_SUCCESS) {
    GError *error;

    g_set_error (&error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to initiate loading state");
    callback (core, &error);

    g_mutex_lock (&self->savestate_mutex);
    self->savestate_callback = NULL;
    self->savestate_load = FALSE;
    g_mutex_unlock (&self->savestate_mutex);

    g_object_unref (self);

    return;
  }
}

static void
mupen64plus_core_save_state (HsCore          *core,
                             const char      *path,
                             HsStateCallback  callback)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  g_mutex_lock (&self->savestate_mutex);
  self->savestate_callback = callback;
  self->savestate_load = FALSE;
  g_mutex_unlock (&self->savestate_mutex);

  g_object_ref (self);

  if (CoreDoCommand (M64CMD_STATE_SAVE, 1, (gpointer) path) != M64ERR_SUCCESS) {
    GError *error;

    g_set_error (&error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to initiate saving state");
    callback (core, &error);

    g_mutex_lock (&self->savestate_mutex);
    self->savestate_callback = NULL;
    g_mutex_unlock (&self->savestate_mutex);

    g_object_unref (self);

    return;
  }

}

static double
mupen64plus_core_get_frame_rate (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  if (ROM_PARAMS.systemtype == SYSTEM_PAL)
    return 50;

  return 60;
}

static double
mupen64plus_core_get_aspect_ratio (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  if (ROM_PARAMS.systemtype == SYSTEM_NTSC)
    return 4.0 / 3.0 * 120.0 / 119.0;

  return 4.0 / 3.0;
}

static double
mupen64plus_core_get_sample_rate (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  return self->sample_rate;
}

static void
mupen64plus_core_finalize (GObject *object)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (object);

  g_mutex_clear (&self->audio_mutex);
  g_mutex_clear (&self->input_mutex);
  g_mutex_clear (&self->savestate_mutex);

  core = NULL;

  G_OBJECT_CLASS (mupen64plus_core_parent_class)->finalize (object);
}

static void
mupen64plus_core_class_init (Mupen64PlusCoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HsCoreClass *core_class = HS_CORE_CLASS (klass);

  object_class->finalize = mupen64plus_core_finalize;

  core_class->start = mupen64plus_core_start;
  core_class->run_frame = mupen64plus_core_run_frame;
  core_class->reset = mupen64plus_core_reset;
  core_class->stop = mupen64plus_core_stop;
  core_class->pause = mupen64plus_core_pause;
  core_class->resume = mupen64plus_core_resume;

  core_class->load_state = mupen64plus_core_load_state;
  core_class->save_state = mupen64plus_core_save_state;

  core_class->get_frame_rate = mupen64plus_core_get_frame_rate;
  core_class->get_aspect_ratio = mupen64plus_core_get_aspect_ratio;

  core_class->get_sample_rate = mupen64plus_core_get_sample_rate;
}

static void
mupen64plus_core_init (Mupen64PlusCore *self)
{
  g_assert (!core);

  core = self;

  self->sample_rate = SAMPLE_RATE;
  self->new_sample_rate = -1;
  self->savestate_result = -1;

  g_mutex_init (&self->audio_mutex);
  g_mutex_init (&self->input_mutex);
  g_mutex_init (&self->savestate_mutex);
}

const uint8_t button_offsets[] = {
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

static void
mupen64plus_nintendo_64_core_button_pressed (HsNintendo64Core *core, guint port, HsNintendo64Button button)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  g_mutex_lock (&self->input_mutex);
  self->button_state[port].Value |= (1 << button_offsets[button]);
  g_mutex_unlock (&self->input_mutex);
}

static void
mupen64plus_nintendo_64_core_button_released (HsNintendo64Core *core, guint port, HsNintendo64Button button)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  g_mutex_lock (&self->input_mutex);
  self->button_state[port].Value &= ~(1 << button_offsets[button]);
  g_mutex_unlock (&self->input_mutex);
}

static void
mupen64plus_nintendo_64_core_control_stick_moved (HsNintendo64Core *core, guint port, double x, double y)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  g_mutex_lock (&self->input_mutex);
  self->button_state[port].X_AXIS = (int8_t) round (x * 80);
  self->button_state[port].Y_AXIS = (int8_t) round (y * 80);
  g_mutex_unlock (&self->input_mutex);
}

static void
mupen64plus_nintendo_64_core_init (HsNintendo64CoreInterface *iface)
{
  iface->button_pressed = mupen64plus_nintendo_64_core_button_pressed;
  iface->button_released = mupen64plus_nintendo_64_core_button_released;
  iface->control_stick_moved = mupen64plus_nintendo_64_core_control_stick_moved;
}

GType
hs_get_core_type (void)
{
  return MUPEN64PLUS_TYPE_CORE;
}
