//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================
#ifndef __AC_COMMON_H
#define __AC_COMMON_H

#include "util/string.h"

// These are the project-dependent functions, they are defined both in Engine.App and AGS.Native.
void quit(const AGS::Common::String &str);
void quit(const char *);
void quitprintf(const char *fmt, ...);
void update_polled_stuff_if_runtime();
void set_our_eip(int eip);
int  get_our_eip();

#endif // __AC_COMMON_H
