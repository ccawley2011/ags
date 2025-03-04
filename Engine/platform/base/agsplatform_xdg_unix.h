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
// AGS Base Unix Driver Header
//
//=============================================================================

#ifndef __AGS_EE_PLATFORM__AGSPLATFORM_UNIX_H
#define __AGS_EE_PLATFORM__AGSPLATFORM_UNIX_H

#include "core/platform.h"

#if AGS_PLATFORM_IS_XDG_UNIX

#include "platform/base/agsplatformdriver.h"

struct AGSPlatformXDGUnix : AGSPlatformDriver {
    void DisplayAlert(const char*, ...) override;
    FSLocation GetAllUsersDataDirectory() override;
    FSLocation GetUserSavedgamesDirectory() override;
    FSLocation GetUserConfigDirectory() override;
    FSLocation GetUserGlobalConfigDirectory() override;
    FSLocation GetAppOutputDirectory() override;
    unsigned long GetDiskFreeSpaceMB() override;
    const char* GetBackendFailUserHint() override;
};

#endif // AGS_PLATFORM_IS_XDG_UNIX

#endif // __AGS_EE_PLATFORM__AGSPLATFORM_UNIX_H