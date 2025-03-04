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
//
// OpenGL includes and definitions for various platforms
//
//=============================================================================

#include "core/platform.h"

#if AGS_OPENGL_ES2
#include <SDL.h>
#include <EGL/egl.h>
#include "glad/glad.h"
#elif AGS_PLATFORM_OS_WINDOWS
#include <SDL.h>
#include "platform/windows/windows.h"
#include "glad/glad.h"

#elif AGS_PLATFORM_OS_LINUX || AGS_PLATFORM_OS_FREEBSD
#include <SDL.h>
#include "glad/glad.h"

#elif AGS_PLATFORM_OS_MACOS
#include "SDL.h"
#include "glad/glad.h"

#elif AGS_PLATFORM_OS_ANDROID

#include <GLES/gl.h>
#include <GLES2/gl2.h>

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

// TODO: we probably should not use GLExt since we use GLES2
#include <GLES/glext.h>

#elif AGS_PLATFORM_OS_IOS

#include <OpenGLES/ES1/gl.h>
#include <OpenGLES/ES2/gl.h>

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <OpenGLES/ES1/glext.h>

#else

#error "opengl: unsupported platform"

#endif

#ifndef GLAPI
#define GLAD_GL_VERSION_2_0 (0)
#endif
