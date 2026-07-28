/*
 * Copyright (C) 2023-2025 Alice Mikhaylenko <alicem@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mupen64plus-highscore.h"

#include <mupen64plus/m64p_common.h>
#include <mupen64plus/m64p_config.h>
#include <mupen64plus/m64p_debugger.h>
#include <mupen64plus/m64p_frontend.h>
#include <mupen64plus/m64p_plugin.h>
#include <mupen64plus/m64p_types.h>

#include <ctype.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdbool.h>

#define PLUGIN_VIDEO_GLIDEN64 "mupen64plus-video-GLideN64.so"
#define PLUGIN_VIDEO_PARALLEL "mupen64plus-video-parallel.so"
#define PLUGIN_RSP_HLE        "mupen64plus-rsp-hle.so"
#define PLUGIN_RSP_PARALLEL   "mupen64plus-rsp-parallel.so"

#define SAMPLE_RATE 33600
#define OVERSCAN_V 8
#define OVERSCAN_H 8

#define VI_STATUS_REG 0
#define VI_CURRENT_LINE_REG 4
#define VI_SERRATE_FLAG (1 << 6)

#define N_GL_ATTRS M64P_GL_CONTEXT_PROFILE_MASK

static ptr_CoreStartup             CoreStartup;
static ptr_CoreShutdown            CoreShutdown;
static ptr_CoreDoCommand           CoreDoCommand;
static ptr_CoreAttachPlugin        CoreAttachPlugin;
static ptr_CoreDetachPlugin        CoreDetachPlugin;
static ptr_CoreOverrideVidExt      CoreOverrideVidExt;
static ptr_ConfigOpenSection       ConfigOpenSection;
static ptr_ConfigSaveSection       ConfigSaveSection;
static ptr_ConfigDeleteSection     ConfigDeleteSection;
static ptr_ConfigGetParamInt       ConfigGetParamInt;
static ptr_ConfigGetParameterType  ConfigGetParameterType;
static ptr_ConfigSetParameter      ConfigSetParameter;
static ptr_ConfigOverrideUserPaths ConfigOverrideUserPaths;
static ptr_DebugMemGetPointer      DebugMemGetPointer;
static ptr_PluginGetVersion        PluginGetVersion;

typedef void (*HsSampleRateChangedCallback) (HsCore *core, double sample_rate);
typedef void (*hs_setup_audio_t) (HsCore *core, HsSampleRateChangedCallback sample_rate_cb);
typedef void (*hs_setup_input_t) (HsCore *core);
typedef void (*hs_poll_input_t) (HsInputState *input_state);
typedef void (*hs_set_controller_t) (guint player, gboolean present, HsNintendo64Pak pak);

static hs_poll_input_t hs_poll_input;
static hs_set_controller_t hs_set_controller;

static Mupen64PlusCore *core;

struct _Mupen64PlusCore
{
  HsCore parent_instance;

  // Lock video_mutex before accessing
  HsGLContext *context;
  GThread *emulation_thread;

  AUDIO_INFO audio_info;
  double sample_rate;
  // Lock audio_mutex before accessing
  double new_sample_rate;
  GMutex audio_mutex;

  m64p_dynlib_handle *core_handle;
  m64p_dynlib_handle *gfx_plugin;
  m64p_dynlib_handle *audio_plugin;
  m64p_dynlib_handle *input_plugin;
  m64p_dynlib_handle *rsp_plugin;

  m64p_rom_header rom_header;
  m64p_rom_settings rom_settings;

  // Access only with g_atomic_int_*()
  gboolean paused;

  GMutex savestate_mutex;

  // Lock savestate_mutex before accessing
  HsStateCallback savestate_callback;
  gboolean savestate_load;
  int savestate_result;
  gboolean should_pause_again;
  gboolean savestate_in_progress;

  // Lock video_mutex before accessing
  int gl_attrs[N_GL_ATTRS];
  gboolean realized;
  gboolean attrs_changed;
  int width;
  int height;
  gboolean pending_resize;
  int pending_width;
  int pending_height;
  GMutex *video_mutex;

  guint32 *vi_regs;
  float next_colorburst_offset;

  HsNintendo64EmulationMode mode;

  GCond frame_cond;
  GMutex frame_mutex;
};

static void mupen64plus_nintendo_64_core_init (HsNintendo64CoreInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (Mupen64PlusCore, mupen64plus_core, HS_TYPE_CORE,
                               G_IMPLEMENT_INTERFACE (HS_TYPE_NINTENDO_64_CORE, mupen64plus_nintendo_64_core_init))

static m64p_system_type
rom_country_code_to_system_type (uint16_t country_code)
{
  switch (country_code) {
  // PAL codes
  case 0x44:
  case 0x46:
  case 0x49:
  case 0x50:
  case 0x53:
  case 0x55:
  case 0x58:
  case 0x59:
    return SYSTEM_PAL;

  // NTSC codes
  case 0x37:
  case 0x41:
  case 0x45:
  case 0x4a:
  default: // Fallback for unknown codes
    return SYSTEM_NTSC;
  }
}

static void
debug_callback (gpointer context, int level, const char *message)
{
  // These are similarly harmless and not something we care about.
  if (g_str_equal (message, "No version number in 'Core' config section. Setting defaults.") ||
      g_str_equal (message, "No version number in 'CoreEvents' config section. Setting defaults.") ||
      g_str_equal (message, "No version number in 'Rsp-HLE' config section. Setting defaults.")) {
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

  hs_core_log_literal (HS_CORE (core), hs_level, message);
}

static void
finish_savestate_cb (Mupen64PlusCore *self)
{
  g_mutex_lock (&self->savestate_mutex);

  int result = self->savestate_result;

  if (result) {
    if (self->savestate_load)
      self->next_colorburst_offset = hs_core_get_colorburst_offset (HS_CORE (self));

    self->savestate_callback (HS_CORE (self), NULL);
  } else {
    GError *error = NULL;

    g_set_error (&error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL,
                 "Failed to %s state", self->savestate_load ? "load" : "save");

    self->savestate_callback (HS_CORE (self), &error);
  }

  self->savestate_callback = NULL;
  self->savestate_load = FALSE;
  self->savestate_result = -1;
  self->savestate_in_progress = FALSE;

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

    if (param_type == M64CORE_STATE_SAVECOMPLETE && core->should_pause_again) {
      CoreDoCommand (M64CMD_PAUSE, 0, NULL);
      core->should_pause_again = FALSE;
    }

    g_mutex_unlock (&core->savestate_mutex);
  }
}

static void
sample_rate_cb (HsCore *core, double sample_rate)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  g_mutex_lock (&self->audio_mutex);
  self->new_sample_rate = sample_rate;
  g_mutex_unlock (&self->audio_mutex);
}

m64p_error
video_init (void)
{
  int i;

  g_mutex_lock (&core->video_mutex);

  for (i = 0; i < N_GL_ATTRS; i++)
    core->gl_attrs[i] = 0;

  g_mutex_unlock (&core->video_mutex);

  return M64ERR_SUCCESS;
}

m64p_error
video_quit (void)
{
  g_mutex_lock (&core->video_mutex);
  hs_gl_context_unrealize (core->context);
  core->realized = FALSE;
  g_mutex_unlock (&core->video_mutex);

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
  g_mutex_lock (&core->video_mutex);

  if (core->attrs_changed) {
    g_clear_object (&core->context);
    core->attrs_changed = FALSE;
  }

  if (!core->context) {
    int major = core->gl_attrs[M64P_GL_CONTEXT_MAJOR_VERSION - 1];
    int minor = core->gl_attrs[M64P_GL_CONTEXT_MINOR_VERSION - 1];
    gboolean is_gles = core->gl_attrs[M64P_GL_CONTEXT_PROFILE_MASK - 1] == M64P_GL_CONTEXT_PROFILE_ES;

    HsGLFlags flags = HS_GL_FLAGS_FLIPPED;

    if (core->gl_attrs[M64P_GL_DEPTH_SIZE - 1] > 0)
      flags |= HS_GL_FLAGS_DEPTH;

    core->context = hs_core_create_gl_context (HS_CORE (core),
                                               is_gles ? HS_GL_API_GLES : HS_GL_API_GL,
                                               major, minor, flags);

    core->realized = FALSE;
  }

  if (!core->realized) {
    g_autoptr (GError) error = NULL;

    if (!hs_gl_context_realize (core->context, &error)) {
      hs_core_log (core, HS_LOG_CRITICAL, "Failed to realize GL context: %s", error->message);

      g_mutex_unlock (&core->video_mutex);

      return M64ERR_SYSTEM_FAIL;
    }

    core->realized = TRUE;
  }

  hs_gl_context_set_size (core->context, width, height);
  core->width = width;
  core->height = height;
  g_mutex_unlock (&core->video_mutex);

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
  gpointer ret;

  g_mutex_lock (&core->video_mutex);
  ret = hs_gl_context_get_proc_address (core->context, name);
  g_mutex_unlock (&core->video_mutex);

  return ret;
}

m64p_error
video_gl_set_attr (m64p_GLattr attr, int value)
{
  g_assert (attr <= N_GL_ATTRS);

  g_mutex_lock (&core->video_mutex);

  if (core->gl_attrs[attr - 1] != value) {
    core->gl_attrs[attr - 1] = value;
    core->attrs_changed = TRUE;
  }

  g_mutex_unlock (&core->video_mutex);

  return M64ERR_SUCCESS;
}

m64p_error
video_gl_get_attr (m64p_GLattr attr, int *value)
{
  g_assert (attr <= N_GL_ATTRS);

  g_mutex_lock (&core->video_mutex);
  *value = core->gl_attrs[attr - 1];
  g_mutex_unlock (&core->video_mutex);

  return M64ERR_SUCCESS;
}

m64p_error
video_gl_swap_buf (void)
{
  g_mutex_lock (&core->video_mutex);

  if (core->pending_resize) {
    hs_gl_context_set_size (core->context, core->pending_width, core->pending_height);

    core->width = core->pending_width;
    core->height = core->pending_height;
    core->pending_resize = FALSE;
  }

  hs_gl_context_swap_buffers (core->context);

  g_mutex_unlock (&core->video_mutex);

  g_cond_signal (&core->frame_cond);

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
  g_mutex_lock (&core->video_mutex);
  hs_gl_context_set_size (core->context, width, height);
  core->width = width;
  core->height = height;
  g_mutex_unlock (&core->video_mutex);

  return M64ERR_SUCCESS;
}

uint32_t
video_gl_get_default_framebuffer (void)
{
  uint32_t ret;

  g_mutex_lock (&core->video_mutex);
  ret = hs_gl_context_get_default_framebuffer (core->context);
  g_mutex_unlock (&core->video_mutex);

  return ret;
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
string_trim (char *str)
{
  char *start = str, *end = str + strlen(str);

  while (start < end && isspace((unsigned char)(*start)))
    start++;

  while (end > start && isspace((unsigned char)(*(end-1))))
    end--;

  memmove(str, start, end - start);
  str[end - start] = '\0';

  return str;
}

static char *
get_save_filename (Mupen64PlusCore *self)
{
  char *filename = g_new0 (char, 256);

  m64p_handle config;
  ConfigOpenSection ("Core", &config);
  int format = ConfigGetParamInt (config, "SaveFilenameFormat");

  char header_name[21];
  memcpy (header_name, self->rom_header.Name, 20);
  header_name[20] = '\0';
  string_trim (header_name); /* Remove trailing whitespace from ROM name. */

  if (format == 0) {
    snprintf (filename, 256, "%s", header_name);
  } else /* if (format == 1) */ {
    if (strstr (self->rom_settings.goodname, "(unknown rom)") == NULL) {
      snprintf (filename, 256, "%.32s-%.8s", self->rom_settings.goodname, self->rom_settings.MD5);
    } else if (self->rom_header.Name[0] != 0) {
      snprintf (filename, 256, "%s-%.8s", header_name, self->rom_settings.MD5);
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

  g_autoptr (GFile) cache_dir = g_file_new_for_path (cache_path);
  if (!g_file_query_exists (cache_dir, NULL) &&
      !g_file_make_directory_with_parents (cache_dir, NULL, error)) {
    return FALSE;
  }

  // Make a temporary dir
  g_autofree char *tmp_path = g_build_filename (cache_path, "libretro-save-XXXXXX", NULL);
  tmp_path = g_mkdtemp (tmp_path);
  g_autoptr (GFile) tmp_file = g_file_new_for_path (tmp_path);

  // Back up the old save
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
  const char *backup_path = g_file_peek_path (backup_file);
  hs_core_log (HS_CORE (self), HS_LOG_MESSAGE, "Libretro save file migrated successfully. A backup has been made in %s", backup_path);

  return TRUE;
}

static m64p_dynlib_handle
attach_plugin (Mupen64PlusCore   *self,
               m64p_plugin_type   type,
               const char        *dir,
               const char        *name,
               GError           **error)
{
  g_autofree char *path = g_build_filename (dir, name, NULL);
  m64p_dynlib_handle handle;
  const char *type_name;

  switch (type) {
  case M64PLUGIN_GFX:
    type_name = "GFX";
    break;
  case M64PLUGIN_AUDIO:
    type_name = "audio";
    break;
  case M64PLUGIN_INPUT:
    type_name = "input";
    break;
  case M64PLUGIN_RSP:
    type_name = "RSP";
    break;
  default:
    g_assert_not_reached ();
  }

  hs_core_log (HS_CORE (self), HS_LOG_INFO, "Loading %s plugin from %s", type_name, path);

  handle = dlopen (path, RTLD_NOW);
  if (!handle) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Missing %s plugin", type_name);

    return NULL;
  }

  ptr_PluginStartup startup = dlsym (handle, "PluginStartup");
  if (!startup) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Missing PluginStartup in %s plugin", type_name);

    return NULL;
  }

  if (startup (self->core_handle, (gpointer) self, debug_callback) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to start up %s plugin", type_name);

    return NULL;
  }

  if (CoreAttachPlugin (type, handle)) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to attach %s plugin", type_name);

    return NULL;
  }

  return handle;
}

static gboolean
has_setting (m64p_handle section, const char *name)
{
  m64p_type *type;

  return ConfigGetParameterType (section, name, &type) == M64ERR_SUCCESS;
}

static gboolean
mupen64plus_core_load_rom (HsCore      *core,
                           const char **rom_paths,
                           int          n_rom_paths,
                           const char  *save_path,
                           GError     **error)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);
  char *data;
  gsize length;

  g_assert (n_rom_paths == 1);

  if (!g_file_get_contents (rom_paths[0], &data, &length, error))
    return FALSE;

  g_autofree char *core_path = g_build_filename (LIB_DIR, "/libmupen64plus.so.2", NULL);
  hs_core_log (HS_CORE (self), HS_LOG_INFO, "Loading mupen64plus-core from %s", core_path);

  self->core_handle = dlopen (core_path, RTLD_NOW);
  if (!self->core_handle) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Missing Mupen64Plus-Core");

    return FALSE;
  }

  CoreStartup             = dlsym (self->core_handle, "CoreStartup");
  CoreShutdown            = dlsym (self->core_handle, "CoreShutdown");
  CoreDoCommand           = dlsym (self->core_handle, "CoreDoCommand");
  CoreAttachPlugin        = dlsym (self->core_handle, "CoreAttachPlugin");
  CoreDetachPlugin        = dlsym (self->core_handle, "CoreDetachPlugin");
  CoreOverrideVidExt      = dlsym (self->core_handle, "CoreOverrideVidExt");
  ConfigOpenSection       = dlsym (self->core_handle, "ConfigOpenSection");
  ConfigSaveSection       = dlsym (self->core_handle, "ConfigSaveSection");
  ConfigDeleteSection     = dlsym (self->core_handle, "ConfigDeleteSection");
  ConfigGetParamInt       = dlsym (self->core_handle, "ConfigGetParamInt");
  ConfigGetParameterType  = dlsym (self->core_handle, "ConfigGetParameterType");
  ConfigSetParameter      = dlsym (self->core_handle, "ConfigSetParameter");
  ConfigOverrideUserPaths = dlsym (self->core_handle, "ConfigOverrideUserPaths");
  DebugMemGetPointer      = dlsym (self->core_handle, "DebugMemGetPointer");
  PluginGetVersion        = dlsym (self->core_handle, "PluginGetVersion");

  int api_version;
  if (PluginGetVersion (NULL, NULL, &api_version, NULL, NULL) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to fetch to core API version");

    return FALSE;
  }

  if (CoreStartup (api_version, /* ConfigPath */ save_path, /* DataPath */ DATA_DIR,
                   (gpointer) self, debug_callback, (gpointer) self, state_callback) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to start the core");

    return FALSE;
  }

  if (CoreDoCommand (M64CMD_ROM_OPEN, (int) length, (gpointer) data) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_COULDNT_LOAD_ROM, "Failed to load ROM");

    return FALSE;
  }

  if (CoreDoCommand (M64CMD_ROM_GET_HEADER, sizeof(m64p_rom_header), &self->rom_header) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to retrieve ROM header");

    return FALSE;
  }

  if (CoreDoCommand (M64CMD_ROM_GET_SETTINGS, sizeof(m64p_rom_settings), &self->rom_settings) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to retrieve ROM settings");

    return FALSE;
  }

  g_autofree char *cache_path = hs_core_get_cache_path (core);
  if (!try_migrate_libretro_save (self, save_path, cache_path, error))
    return FALSE;

  if (ConfigOverrideUserPaths (/* DataPath */ save_path, /* CachePath */ cache_path) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to override user paths");

    return FALSE;
  }

  m64p_handle config;
  ConfigOpenSection ("Core", &config);
  ConfigSetParameter (config, "SaveSRAMPath", M64TYPE_STRING, save_path);
  ConfigSaveSection ("Core");

  ConfigSaveSection ("CoreEvents");

  int width = -1, height = -1;

  ConfigOpenSection ("Video-General", &config);

  if (has_setting (config, "ScreenWidth"))
    width = ConfigGetParamInt (config, "ScreenWidth");
  if (has_setting (config, "ScreenHeight"))
    height = ConfigGetParamInt (config, "ScreenHeight");

  ConfigDeleteSection ("Video-GLideN64");
  ConfigDeleteSection ("Video-Parallel");

  ConfigOpenSection ("Video-GLideN64", &config);
  int value = 0;
  ConfigSetParameter (config, "AspectRatio", M64TYPE_INT, &value);
  value = 1;
  ConfigSetParameter (config, "UseNativeResolutionFactor", M64TYPE_INT, &value);
  value = 0;
  ConfigSetParameter (config, "EnableDitheringPattern", M64TYPE_INT, &value);
  value = 0;
  ConfigSetParameter (config, "bilinearMode", M64TYPE_INT, &value);
  value = 2;
  ConfigSetParameter (config, "MultiSampling", M64TYPE_INT, &value);
  ConfigSaveSection ("Video-GLideN64");

  ConfigOpenSection ("Video-Parallel", &config);
  value = 0;
  ConfigSetParameter (config, "DeinterlaceMode", M64TYPE_INT, &value);
  value = 0;
  ConfigSetParameter (config, "CropOverscanV", M64TYPE_INT, &value);
  ConfigSetParameter (config, "CropOverscanH", M64TYPE_INT, &value);

  if (width >= 0)
    ConfigSetParameter (config, "ScreenWidth", M64TYPE_INT, &width);
  if (height >= 0)
    ConfigSetParameter (config, "ScreenHeight", M64TYPE_INT, &height);

  ConfigSaveSection ("Video-Parallel");

  // Set up video
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

  const char *gfx_plugin, *rsp_plugin;

  if (self->mode == HS_NINTENDO_64_HLE) {
    gfx_plugin = PLUGIN_VIDEO_GLIDEN64;
    rsp_plugin = PLUGIN_RSP_HLE;
  } else {
    gfx_plugin = PLUGIN_VIDEO_PARALLEL;
    rsp_plugin = PLUGIN_RSP_PARALLEL;
  }

  self->gfx_plugin = attach_plugin (self, M64PLUGIN_GFX, PLUGINS_DIR, gfx_plugin, error);
  if (!self->gfx_plugin)
    return FALSE;

  self->audio_plugin = attach_plugin (self, M64PLUGIN_AUDIO, CORE_DIR, "mupen64plus-audio-highscore.so", error);
  if (!self->audio_plugin)
    return FALSE;

  self->input_plugin = attach_plugin (self, M64PLUGIN_INPUT, CORE_DIR, "mupen64plus-input-highscore.so", error);
  if (!self->input_plugin)
    return FALSE;

  self->rsp_plugin = attach_plugin (self, M64PLUGIN_RSP, PLUGINS_DIR, rsp_plugin, error);
  if (!self->rsp_plugin)
    return FALSE;

  hs_setup_audio_t hs_setup_audio = dlsym (self->audio_plugin, "hs_setup_audio");
  hs_setup_input_t hs_setup_input = dlsym (self->input_plugin, "hs_setup_input");
  hs_poll_input = dlsym (self->input_plugin, "hs_poll_input");
  hs_set_controller = dlsym (self->input_plugin, "hs_set_controller");

  g_assert (hs_setup_audio != NULL);
  g_assert (hs_setup_input != NULL);
  g_assert (hs_poll_input != NULL);
  g_assert (hs_set_controller != NULL);

  hs_setup_audio (core, sample_rate_cb);
  hs_setup_input (core);

  self->vi_regs = DebugMemGetPointer (M64P_DBG_PTR_VI_REG);

  return TRUE;
}

static void
mupen64plus_core_start (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  self->emulation_thread = g_thread_new ("Mupen64Plus emulation thread",
                                         (GThreadFunc) run_emulation_thread, self);
}

static void
mupen64plus_core_poll_input (HsCore *core, HsInputState *input_state)
{
  hs_poll_input (input_state);
}

static void
mupen64plus_core_run_frame (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);
  m64p_system_type system_type = rom_country_code_to_system_type (self->rom_header.Country_code);

  g_mutex_lock (&self->audio_mutex);

  if (self->new_sample_rate > 0) {
    self->sample_rate = self->new_sample_rate;
    self->new_sample_rate = -1;
  }

  g_mutex_unlock (&self->audio_mutex);

  gboolean interlaced = self->vi_regs[VI_STATUS_REG] & VI_SERRATE_FLAG;
  gboolean is_even = self->vi_regs[VI_CURRENT_LINE_REG] > 0;

  g_mutex_lock (&self->video_mutex);
  int width = self->width;
  int height = self->height;

  int base_height = (system_type == SYSTEM_PAL) ? 288 : 240;

  if (!self->pending_resize && self->vi_regs[VI_STATUS_REG] != 0) {
    int new_width = 640;
    int new_height = base_height;

    if (interlaced && self->mode == HS_NINTENDO_64_HLE)
      new_height *= 2.0;

    if (new_width != width || new_height != height) {
      int size = (new_width << 16) + new_height;

      self->pending_resize = TRUE;
      self->pending_width = new_width;
      self->pending_height = new_height;

      if (CoreDoCommand (M64CMD_CORE_STATE_SET, M64CORE_VIDEO_SIZE, &size) != M64ERR_SUCCESS)
        self->pending_resize = FALSE;

      m64p_handle config;
      ConfigOpenSection ("Video-General", &config);
      ConfigSetParameter (config, "ScreenWidth", M64TYPE_INT, &new_width);
      ConfigSetParameter (config, "ScreenHeight", M64TYPE_INT, &new_height);
      ConfigSaveSection ("Video-General");

      if (self->pending_resize) {
        width = new_width;
        height = new_height;
      }
    }
  } else if (self->vi_regs[VI_STATUS_REG] == 0 || width == 0 || height == 0) {
    width = 640;
    height = base_height;
  }

  HsInterlacingMode interlacing;

  // GLideN64 always outputs progressive video
  if (interlaced && self->mode == HS_NINTENDO_64_LLE) {
    if (is_even)
      interlacing = HS_INTERLACING_EVEN_FIELD;
    else
      interlacing = HS_INTERLACING_ODD_FIELD;
  } else {
    interlacing = HS_INTERLACING_NONE;
  }

  hs_gl_context_set_interlacing (self->context, interlacing);

  hs_gl_context_set_overscan (self->context,
                              &HS_BORDER_INIT (OVERSCAN_H * width / 640,
                                               OVERSCAN_V * height / base_height));

  if (system_type == SYSTEM_PAL) {
    hs_gl_context_set_colorburst (self->context, 1.5 * width / 320.0, 0.7516, self->next_colorburst_offset);

    if (interlacing != HS_INTERLACING_ODD_FIELD) {
      self->next_colorburst_offset += 0.2508;

      if (self->next_colorburst_offset > 2499.9)
       self->next_colorburst_offset = 0;
    }
  } else if (system_type == SYSTEM_MPAL) {
    hs_gl_context_set_colorburst (self->context, 1.875 * width / 320.0, 0.25, self->next_colorburst_offset);

    if (interlacing != HS_INTERLACING_ODD_FIELD) {
      self->next_colorburst_offset += 0.25;

      if (self->next_colorburst_offset > 0.9)
       self->next_colorburst_offset = 0;
    }
  } else {
    hs_gl_context_set_colorburst (self->context, 1.875 * width / 320.0, 0.5, self->next_colorburst_offset);

    if (interlacing != HS_INTERLACING_ODD_FIELD) {
      self->next_colorburst_offset += 0.5;

      if (self->next_colorburst_offset > 0.9)
       self->next_colorburst_offset = 0;
    }
  }

  g_mutex_unlock (&self->video_mutex);

  // Wait until swap_buffers() so that we have a picture ready to go
  g_mutex_lock (&self->frame_mutex);
  g_cond_wait (&self->frame_cond, &self->frame_mutex);
  g_mutex_unlock (&self->frame_mutex);

  if (g_atomic_int_get (&self->paused))
    CoreDoCommand (M64CMD_ADVANCE_FRAME, 0, NULL);
}

static gboolean
mupen64plus_core_reset (HsCore *core, gboolean hard, GError **error)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  if (CoreDoCommand (M64CMD_RESET, hard, NULL) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to reset game");
    return FALSE;
  }

  if (hard)
    self->next_colorburst_offset = 0;

  return TRUE;
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

  if (CoreDetachPlugin (M64PLUGIN_GFX) != M64ERR_SUCCESS)
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to detach GFX plugin");
  if (CoreDetachPlugin (M64PLUGIN_AUDIO) != M64ERR_SUCCESS)
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to detach audio plugin");
  if (CoreDetachPlugin (M64PLUGIN_INPUT) != M64ERR_SUCCESS)
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to detach input plugin");
  if (CoreDetachPlugin (M64PLUGIN_RSP) != M64ERR_SUCCESS)
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to detach RSP plugin");

  if (CoreShutdown () != M64ERR_SUCCESS)
    hs_core_log (core, HS_LOG_CRITICAL, "Failed to shutdown the core");

  g_clear_pointer (&self->core_handle,  dlclose);
  g_clear_pointer (&self->gfx_plugin,   dlclose);
  g_clear_pointer (&self->audio_plugin, dlclose);
  g_clear_pointer (&self->input_plugin, dlclose);
  g_clear_pointer (&self->rsp_plugin,   dlclose);
}

static void
mupen64plus_core_pause (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  g_atomic_int_set (&self->paused, TRUE);

  g_mutex_lock (&self->savestate_mutex);
  if (self->savestate_in_progress)
    self->should_pause_again = FALSE;
  g_mutex_unlock (&self->savestate_mutex);

  CoreDoCommand (M64CMD_PAUSE, 0, NULL);
}

static void
mupen64plus_core_resume (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  CoreDoCommand (M64CMD_RESUME, 0, NULL);

  g_mutex_lock (&self->savestate_mutex);
  if (self->savestate_in_progress)
    self->should_pause_again = FALSE;
  g_mutex_unlock (&self->savestate_mutex);

  g_atomic_int_set (&self->paused, FALSE);
}

static gboolean
mupen64plus_core_reload_save (HsCore      *core,
                              const char  *save_path,
                              GError     **error)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);
  g_autofree char *cache_path = hs_core_get_cache_path (core);

  if (!try_migrate_libretro_save (self, save_path, cache_path, error))
    return FALSE;

  if (CoreDoCommand (M64CMD_STOP, 0, NULL) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to stop core");

    return FALSE;
  }

  g_clear_pointer (&self->emulation_thread, g_thread_join);

  if (ConfigOverrideUserPaths (/* DataPath */ save_path, /* CachePath */ cache_path) != M64ERR_SUCCESS) {
    g_set_error (error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to override user paths");

    return FALSE;
  }

  m64p_handle config;
  ConfigOpenSection ("Core", &config);
  ConfigSetParameter (config, "SaveSRAMPath", M64TYPE_STRING, save_path);
  ConfigSaveSection ("Core");

  mupen64plus_core_start (core);

  return TRUE;
}

static void
mupen64plus_core_load_state (HsCore          *core,
                             const char      *path,
                             HsStateCallback  callback)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  g_mutex_lock (&self->savestate_mutex);

  if (self->savestate_in_progress) {
    if (self->savestate_load)
      hs_core_log (core, HS_LOG_CRITICAL, "Trying to load state when loading is already in progress");
    else
      hs_core_log (core, HS_LOG_CRITICAL, "Trying to load state when saving is already in progress");

    g_mutex_unlock (&self->savestate_mutex);
    return;
  }

  self->savestate_callback = callback;
  self->savestate_load = TRUE;
  self->savestate_in_progress = TRUE;
  g_mutex_unlock (&self->savestate_mutex);

  g_object_ref (self);

  if (CoreDoCommand (M64CMD_STATE_LOAD, 1, (gpointer) path) != M64ERR_SUCCESS) {
    GError *error = NULL;

    g_set_error (&error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to initiate loading state");
    callback (core, &error);

    g_mutex_lock (&self->savestate_mutex);
    self->savestate_callback = NULL;
    self->savestate_load = FALSE;
    self->savestate_in_progress = FALSE;
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
  gboolean paused;

  g_mutex_lock (&self->savestate_mutex);

  if (self->savestate_in_progress) {
    if (self->savestate_load)
      hs_core_log (core, HS_LOG_CRITICAL, "Trying to save state when loading is already in progress");
    else
      hs_core_log (core, HS_LOG_CRITICAL, "Trying to save state when saving is already in progress");

    g_mutex_unlock (&self->savestate_mutex);
    return;
  }
  self->savestate_callback = callback;
  self->savestate_load = FALSE;
  self->savestate_in_progress = TRUE;
  g_mutex_unlock (&self->savestate_mutex);

  g_object_ref (self);

  paused = g_atomic_int_get (&self->paused);

  if (paused) {
    CoreDoCommand (M64CMD_RESUME, 0, NULL);

    g_mutex_lock (&self->savestate_mutex);
    self->should_pause_again = TRUE;
    g_mutex_unlock (&self->savestate_mutex);
  }

  if (CoreDoCommand (M64CMD_STATE_SAVE, 1, (gpointer) path) != M64ERR_SUCCESS) {
    GError *error = NULL;

    g_set_error (&error, HS_CORE_ERROR, HS_CORE_ERROR_INTERNAL, "Failed to initiate saving state");
    callback (core, &error);

    g_mutex_lock (&self->savestate_mutex);
    self->savestate_callback = NULL;
    g_mutex_unlock (&self->savestate_mutex);

    g_object_unref (self);

    if (self->should_pause_again) {
      CoreDoCommand (M64CMD_PAUSE, 0, NULL);

      g_mutex_lock (&self->savestate_mutex);
      self->should_pause_again = FALSE;
      g_mutex_unlock (&self->savestate_mutex);
    }

    g_mutex_lock (&self->savestate_mutex);
    self->savestate_in_progress = FALSE;
    g_mutex_unlock (&self->savestate_mutex);

    return;
  }
}

static double
mupen64plus_core_get_frame_rate (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  m64p_system_type system_type = rom_country_code_to_system_type (self->rom_header.Country_code);

  if (system_type == SYSTEM_PAL)
    return 50;

  return 60;
}

static double
mupen64plus_core_get_aspect_ratio (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);
  m64p_system_type system_type = rom_country_code_to_system_type (self->rom_header.Country_code);

  double par, height;

  if (system_type == SYSTEM_PAL) {
    par = 6.0 / 5.0;
    height = 288;
  } else {
    par = 120.0 / 119.0;
    height = 240;
  }

  return 320.0 / height * par;
}

static double
mupen64plus_core_get_sample_rate (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  return self->sample_rate;
}

static HsRegion
mupen64plus_core_get_region (HsCore *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);
  m64p_system_type system_type = rom_country_code_to_system_type (self->rom_header.Country_code);

  if (system_type == SYSTEM_NTSC)
    return HS_REGION_NTSC;
  else
    return HS_REGION_PAL;
}

static gboolean
mupen64plus_core_has_internal_frame_clock (HsCore *core)
{
  return TRUE;
}

static void
mupen64plus_core_finalize (GObject *object)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (object);

  g_mutex_clear (&self->audio_mutex);
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

  core_class->load_rom = mupen64plus_core_load_rom;
  core_class->start = mupen64plus_core_start;
  core_class->poll_input = mupen64plus_core_poll_input;
  core_class->run_frame = mupen64plus_core_run_frame;
  core_class->reset = mupen64plus_core_reset;
  core_class->stop = mupen64plus_core_stop;
  core_class->pause = mupen64plus_core_pause;
  core_class->resume = mupen64plus_core_resume;

  core_class->reload_save = mupen64plus_core_reload_save;

  core_class->load_state = mupen64plus_core_load_state;
  core_class->save_state = mupen64plus_core_save_state;

  core_class->get_frame_rate = mupen64plus_core_get_frame_rate;
  core_class->get_aspect_ratio = mupen64plus_core_get_aspect_ratio;

  core_class->get_sample_rate = mupen64plus_core_get_sample_rate;

  core_class->get_region = mupen64plus_core_get_region;

  core_class->has_internal_frame_clock = mupen64plus_core_has_internal_frame_clock;
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
  g_mutex_init (&self->savestate_mutex);
}

static guint
mupen64plus_nintendo_64_core_get_players (HsNintendo64Core *core)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  return self->rom_settings.players;
}

static void
mupen64plus_nintendo_64_core_set_controller (HsNintendo64Core *core, guint player, gboolean present, HsNintendo64Pak pak)
{
  hs_set_controller (player, present, pak);
}

static void
mupen64plus_nintendo_64_core_set_emulation_mode (HsNintendo64Core *core, HsNintendo64EmulationMode mode)
{
  Mupen64PlusCore *self = MUPEN64PLUS_CORE (core);

  self->mode = mode;
}

static void
mupen64plus_nintendo_64_core_init (HsNintendo64CoreInterface *iface)
{
  iface->get_players = mupen64plus_nintendo_64_core_get_players;
  iface->set_controller = mupen64plus_nintendo_64_core_set_controller;
  iface->set_emulation_mode = mupen64plus_nintendo_64_core_set_emulation_mode;
}

GType
hs_get_core_type (void)
{
  return MUPEN64PLUS_TYPE_CORE;
}
