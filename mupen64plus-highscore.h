#pragma once

#include <highscore/libhighscore.h>

G_BEGIN_DECLS

#define MUPEN64PLUS_TYPE_CORE (mupen64plus_core_get_type())

G_DECLARE_FINAL_TYPE (Mupen64PlusCore, mupen64plus_core, MUPEN64PLUS, CORE, HsCore)

GType hs_get_core_type (void);

G_END_DECLS
