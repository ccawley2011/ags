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
#include "ac/dynobj/cc_audioclip.h"
#include "ac/dynobj/scriptaudioclip.h"
#include "ac/gamesetupstruct.h"
#include "util/stream.h"

using namespace AGS::Common;

extern GameSetupStruct game;

const char *CCAudioClip::GetType() {
    return "AudioClip";
}

size_t CCAudioClip::CalcSerializeSize()
{
    return sizeof(int32_t);
}

void CCAudioClip::Serialize(const char *address, Stream *out) {
    ScriptAudioClip *ach = (ScriptAudioClip*)address;
    out->WriteInt32(ach->id);
}

void CCAudioClip::Unserialize(int index, Stream *in, size_t /*data_sz*/) {
    int id = in->ReadInt32();
    ccRegisterUnserializedObject(index, &game.audioClips[id], this);
}
