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
#include <cstdio>
#include "ac/audiocliptype.h"
#include "ac/dialogtopic.h"
#include "ac/gamesetupstruct.h"
#include "ac/spritecache.h"
#include "ac/view.h"
#include "ac/wordsdictionary.h"
#include "ac/dynobj/scriptaudioclip.h"
#include "core/asset.h"
#include "core/assetmanager.h"
#include "debug/out.h"
#include "game/main_game_file.h"
#include "gui/guimain.h"
#include "script/cc_common.h"
#include "util/alignedstream.h"
#include "util/data_ext.h"
#include "util/directory.h"
#include "util/path.h"
#include "util/string_compat.h"
#include "util/string_utils.h"
#include "font/fonts.h"

namespace AGS
{
namespace Common
{

const String MainGameSource::DefaultFilename_v3 = "game28.dta";
const String MainGameSource::DefaultFilename_v2 = "ac2game.dta";
const String MainGameSource::Signature = "Adventure Creator Game File v2";

MainGameSource::MainGameSource()
    : DataVersion(kGameVersion_Undefined)
{
}

String GetMainGameFileErrorText(MainGameFileErrorType err)
{
    switch (err)
    {
    case kMGFErr_NoError:
        return "No error.";
    case kMGFErr_FileOpenFailed:
        return "Main game file not found or could not be opened.";
    case kMGFErr_SignatureFailed:
        return "Not an AGS main game file or unsupported format.";
    case kMGFErr_FormatVersionTooOld:
        return "Format version is too old; this engine can only run games made with AGS 2.5 or later.";
    case kMGFErr_FormatVersionNotSupported:
        return "Format version not supported.";
    case kMGFErr_CapsNotSupported:
        return "The game requires extended capabilities which aren't supported by the engine.";
    case kMGFErr_InvalidNativeResolution:
        return "Unable to determine native game resolution.";
    case kMGFErr_TooManySprites:
        return "Too many sprites for this engine to handle.";
    case kMGFErr_InvalidPropertySchema:
        return "Failed to deserialize custom properties schema.";
    case kMGFErr_InvalidPropertyValues:
        return "Errors encountered when reading custom properties.";
    case kMGFErr_CreateGlobalScriptFailed:
        return "Failed to load global script.";
    case kMGFErr_CreateDialogScriptFailed:
        return "Failed to load dialog script.";
    case kMGFErr_CreateScriptModuleFailed:
        return "Failed to load script module.";
    case kMGFErr_GameEntityFailed:
        return "Failed to load one or more game entities.";
    case kMGFErr_PluginDataFmtNotSupported:
        return "Format version of plugin data is not supported.";
    case kMGFErr_PluginDataSizeTooLarge:
        return "Plugin data size is too large.";
    case kMGFErr_ExtListFailed:
        return "There was error reading game data extensions.";
    case kMGFErr_ExtUnknown:
        return "Unknown extension.";
    }
    return "Unknown error.";
}

LoadedGameEntities::LoadedGameEntities(GameSetupStruct &game)
    : Game(game)
    , SpriteCount(0)
{
}

LoadedGameEntities::~LoadedGameEntities() = default;

bool IsMainGameLibrary(const String &filename)
{
    // We must not only detect if the given file is a correct AGS data library,
    // we also have to assure that this library contains main game asset.
    // Library may contain some optional data (digital audio, speech etc), but
    // that is not what we want.
    AssetLibInfo lib;
    if (AssetManager::ReadDataFileTOC(filename, lib) != kAssetNoError)
        return false;
    for (size_t i = 0; i < lib.AssetInfos.size(); ++i)
    {
        if (lib.AssetInfos[i].FileName.CompareNoCase(MainGameSource::DefaultFilename_v3) == 0 ||
            lib.AssetInfos[i].FileName.CompareNoCase(MainGameSource::DefaultFilename_v2) == 0)
        {
            return true;
        }
    }
    return false;
}

// Scans given directory for game data libraries, returns first found or none.
// Uses fn_testfile callback to test the file.
// Tracks files with standard AGS package names:
// - *.ags is a standart cross-platform file pattern for AGS games,
// - ac2game.dat is a legacy file name for very old games,
// - *.exe is a MS Win executable; it is included to this case because
//   users often run AGS ports with Windows versions of games.
String FindGameData(const String &path, std::function<bool(const String&)> fn_testfile)
{
    String test_file;
    Debug::Printf("Searching for game data in: %s", path.GetCStr());
    for (FindFile ff = FindFile::OpenFiles(path); !ff.AtEnd(); ff.Next())
    {
        test_file = ff.Current();
        if (test_file.CompareRightNoCase(".ags") == 0 ||
            test_file.CompareNoCase("ac2game.dat") == 0 ||
            test_file.CompareRightNoCase(".exe") == 0)
        {
            test_file = Path::ConcatPaths(path, test_file);
            if (fn_testfile(test_file))
            {
                Debug::Printf("Found game data pak: %s", test_file.GetCStr());
                return test_file;
            }
        }
    }
    return "";
}

String FindGameData(const String &path)
{
    return FindGameData(path, IsMainGameLibrary);
}

// Begins reading main game file from a generic stream
static HGameFileError OpenMainGameFileBase(Stream *in, MainGameSource &src)
{
    // Check data signature
    String data_sig = String::FromStreamCount(in, MainGameSource::Signature.GetLength());
    if (data_sig.Compare(MainGameSource::Signature))
        return new MainGameFileError(kMGFErr_SignatureFailed);
    // Read data format version and requested engine version
    src.DataVersion = (GameDataVersion)in->ReadInt32();
    if (src.DataVersion >= kGameVersion_230)
        src.CompiledWith = StrUtil::ReadString(in);
    if (src.DataVersion < kGameVersion_250)
        return new MainGameFileError(kMGFErr_FormatVersionTooOld, String::FromFormat("Required format version: %d, supported %d - %d", src.DataVersion, kGameVersion_250, kGameVersion_Current));
    if (src.DataVersion > kGameVersion_Current)
        return new MainGameFileError(kMGFErr_FormatVersionNotSupported,
            String::FromFormat("Game was compiled with %s. Required format version: %d, supported %d - %d", src.CompiledWith.GetCStr(), src.DataVersion, kGameVersion_250, kGameVersion_Current));
    // Read required capabilities
    if (src.DataVersion >= kGameVersion_341)
    {
        size_t count = in->ReadInt32();
        for (size_t i = 0; i < count; ++i)
            src.Caps.insert(StrUtil::ReadString(in));
    }
    // Remember loaded game data version
    // NOTE: this global variable is embedded in the code too much to get
    // rid of it too easily; the easy way is to set it whenever the main
    // game file is opened.
    loaded_game_file_version = src.DataVersion;
    game_compiled_version.SetFromString(src.CompiledWith);
    return HGameFileError::None();
}

HGameFileError OpenMainGameFile(const String &filename, MainGameSource &src)
{
    // Cleanup source struct
    src = MainGameSource();
    // Try to open given file
    Stream *in = File::OpenFileRead(filename);
    if (!in)
        return new MainGameFileError(kMGFErr_FileOpenFailed, String::FromFormat("Filename: %s.", filename.GetCStr()));
    src.Filename = filename;
    src.InputStream.reset(in);
    return OpenMainGameFileBase(in, src);
}

HGameFileError OpenMainGameFileFromDefaultAsset(MainGameSource &src, AssetManager *mgr)
{
    // Cleanup source struct
    src = MainGameSource();
    // Try to find and open main game file
    String filename = MainGameSource::DefaultFilename_v3;
    Stream *in = mgr->OpenAsset(filename);
    if (!in)
    {
        filename = MainGameSource::DefaultFilename_v2;
        in = mgr->OpenAsset(filename);
    }
    if (!in)
        return new MainGameFileError(kMGFErr_FileOpenFailed, String::FromFormat("Filename: %s.", filename.GetCStr()));
    src.Filename = filename;
    src.InputStream.reset(in);
    return OpenMainGameFileBase(in, src);
}

HGameFileError ReadDialogScript(PScript &dialog_script, Stream *in, GameDataVersion data_ver)
{
    if (data_ver > kGameVersion_310) // 3.1.1+ dialog script
    {
        dialog_script.reset(ccScript::CreateFromStream(in));
        if (dialog_script == nullptr)
            return new MainGameFileError(kMGFErr_CreateDialogScriptFailed, cc_get_error().ErrorString);
    }
    else // 2.x and < 3.1.1 dialog
    {
        dialog_script.reset();
    }
    return HGameFileError::None();
}

HGameFileError ReadScriptModules(std::vector<PScript> &sc_mods, Stream *in, GameDataVersion data_ver)
{
    if (data_ver >= kGameVersion_270) // 2.7.0+ script modules
    {
        int count = in->ReadInt32();
        sc_mods.resize(count);
        for (int i = 0; i < count; ++i)
        {
            sc_mods[i].reset(ccScript::CreateFromStream(in));
            if (sc_mods[i] == nullptr)
                return new MainGameFileError(kMGFErr_CreateScriptModuleFailed, cc_get_error().ErrorString);
        }
    }
    else
    {
        sc_mods.resize(0);
    }
    return HGameFileError::None();
}

void ReadViewStruct272_Aligned(std::vector<ViewStruct272> &oldv, Stream *in, size_t count)
{
    AlignedStream align_s(in, Common::kAligned_Read);
    oldv.resize(count);
    for (size_t i = 0; i < count; ++i)
    {
        oldv[i].ReadFromFile(&align_s);
        align_s.Reset();
    }
}

void ReadViews(GameSetupStruct &game, std::vector<ViewStruct> &views, Stream *in, GameDataVersion data_ver)
{
    views.resize(game.numviews);
    if (data_ver > kGameVersion_272) // 3.x views
    {
        for (int i = 0; i < game.numviews; ++i)
        {
            views[i].ReadFromFile(in);
        }
    }
    else // 2.x views
    {
        std::vector<ViewStruct272> oldv;
        ReadViewStruct272_Aligned(oldv, in, game.numviews);
        Convert272ViewsToNew(oldv, views);
    }
}

void ReadDialogs(std::vector<DialogTopic> &dialog,
                 std::vector< std::shared_ptr<unsigned char> > &old_dialog_scripts,
                 std::vector<String> &old_dialog_src,
                 std::vector<String> &old_speech_lines,
                 Stream *in, GameDataVersion data_ver, int dlg_count)
{
    dialog.resize(dlg_count);
    for (int i = 0; i < dlg_count; ++i)
    {
        dialog[i].ReadFromFile(in);
    }

    if (data_ver > kGameVersion_310)
        return;

    old_dialog_scripts.resize(dlg_count);
    old_dialog_src.resize(dlg_count);
    for (int i = 0; i < dlg_count; ++i)
    {
        // NOTE: originally this was read into dialog[i].optionscripts
        old_dialog_scripts[i].reset(new unsigned char[dialog[i].codesize]);
        in->Read(old_dialog_scripts[i].get(), dialog[i].codesize);

        // Encrypted text script
        int script_text_len = in->ReadInt32();
        if (script_text_len > 1)
        {
            // Originally in the Editor +20000 bytes more were allocated, with comment:
            //   "add a large buffer because it will get added to if another option is added"
            // which probably refered to this data used by old editor directly to edit dialogs
            char *buffer = new char[script_text_len + 1];
            in->Read(buffer, script_text_len);
            if (data_ver > kGameVersion_260)
                decrypt_text(buffer, script_text_len);
            buffer[script_text_len] = 0;
            old_dialog_src[i] = buffer;
            delete [] buffer;
        }
        else
        {
            in->Seek(script_text_len);
        }
    }

    // Read the dialog lines
    //
    // TODO: investigate this: these strings were read much simplier in the editor, see code:
    /*
        char stringbuffer[1000];
        for (bb=0;bb<thisgame.numdlgmessage;bb++) {
            if ((filever >= 26) && (encrypted))
                read_string_decrypt(iii, stringbuffer);
            else
                fgetstring(stringbuffer, iii);
        }
    */
    int i = 0;
    char buffer[1000];
    if (data_ver <= kGameVersion_260)
    {
        // Plain text on <= 2.60
        bool end_reached = false;

        while (!end_reached)
        {
            char* nextchar = buffer;

            while (1)
            {
                *nextchar = in->ReadInt8();
                if (*nextchar == 0)
                    break;

                if ((unsigned char)*nextchar == 0xEF)
                {
                    end_reached = true;
                    in->Seek(-1);
                    break;
                }

                nextchar++;
            }

            if (end_reached)
                break;

            old_speech_lines.push_back(buffer);
            i++;
        }
    }
    else
    {
        // Encrypted text on > 2.60
        while (1)
        {
            size_t newlen = static_cast<uint32_t>(in->ReadInt32());
            if (newlen == 0xCAFEBEEF)  // GUI magic
            {
                in->Seek(-4);
                break;
            }

            newlen = std::min(newlen, sizeof(buffer) - 1);
            in->Read(buffer, newlen);
            decrypt_text(buffer, newlen);
            buffer[newlen] = 0;
            old_speech_lines.push_back(buffer);
            i++;
        }
    }
}

HGameFileError ReadPlugins(std::vector<PluginInfo> &infos, Stream *in)
{
    int fmt_ver = in->ReadInt32();
    if (fmt_ver != 1)
        return new MainGameFileError(kMGFErr_PluginDataFmtNotSupported, String::FromFormat("Version: %d, supported: %d", fmt_ver, 1));

    int pl_count = in->ReadInt32();
    for (int i = 0; i < pl_count; ++i)
    {
        String name = String::FromStream(in);
        size_t datasize = in->ReadInt32();
        // just check for silly datasizes
        if (datasize > PLUGIN_SAVEBUFFERSIZE)
            return new MainGameFileError(kMGFErr_PluginDataSizeTooLarge, String::FromFormat("Required: %zu, max: %zu", datasize, (size_t)PLUGIN_SAVEBUFFERSIZE));

        PluginInfo info;
        info.Name = name;
        if (datasize > 0)
        {
            info.Data.reset(new char[datasize]);
            in->Read(info.Data.get(), datasize);
        }
        info.DataLen = datasize;
        infos.push_back(info);
    }
    return HGameFileError::None();
}

// Create the missing audioClips data structure for 3.1.x games.
// This is done by going through the data files and adding all music*.*
// and sound*.* files to it.
void BuildAudioClipArray(const std::vector<String> &assets, std::vector<ScriptAudioClip> &audioclips)
{
    char temp_name[30];
    int temp_number;
    char temp_extension[10];

    for (const String &asset : assets)
    {
        if (sscanf(asset.GetCStr(), "%5s%d.%3s", temp_name, &temp_number, temp_extension) != 3)
            continue;

        ScriptAudioClip clip;
        if (ags_stricmp(temp_extension, "mp3") == 0)
            clip.fileType = eAudioFileMP3;
        else if (ags_stricmp(temp_extension, "wav") == 0)
            clip.fileType = eAudioFileWAV;
        else if (ags_stricmp(temp_extension, "voc") == 0)
            clip.fileType = eAudioFileVOC;
        else if (ags_stricmp(temp_extension, "mid") == 0)
            clip.fileType = eAudioFileMIDI;
        else if ((ags_stricmp(temp_extension, "mod") == 0) || (ags_stricmp(temp_extension, "xm") == 0)
            || (ags_stricmp(temp_extension, "s3m") == 0) || (ags_stricmp(temp_extension, "it") == 0))
            clip.fileType = eAudioFileMOD;
        else if (ags_stricmp(temp_extension, "ogg") == 0)
            clip.fileType = eAudioFileOGG;
        else
            continue;

        if (ags_stricmp(temp_name, "music") == 0)
        {
            clip.scriptName.Format("aMusic%d", temp_number);
            clip.fileName.Format("music%d.%s", temp_number, temp_extension);
            clip.bundlingType = (ags_stricmp(temp_extension, "mid") == 0) ? AUCL_BUNDLE_EXE : AUCL_BUNDLE_VOX;
            clip.type = 2;
            clip.defaultRepeat = 1;
        }
        else if (ags_stricmp(temp_name, "sound") == 0)
        {
            clip.scriptName.Format("aSound%d", temp_number);
            clip.fileName.Format("sound%d.%s", temp_number, temp_extension);
            clip.bundlingType = AUCL_BUNDLE_EXE;
            clip.type = 3;
            clip.defaultRepeat = 0;
        }
        else
        {
            continue;
        }

        clip.defaultVolume = 100;
        clip.defaultPriority = 50;
        clip.id = audioclips.size();
        audioclips.push_back(clip);
    }
}

void ApplySpriteData(GameSetupStruct &game, const LoadedGameEntities &ents, GameDataVersion data_ver)
{
    if (ents.SpriteCount == 0)
        return;

    // Apply sprite flags read from original format (sequential array)
    spriteset.EnlargeTo(ents.SpriteCount - 1);
    for (size_t i = 0; i < ents.SpriteCount; ++i)
    {
        game.SpriteInfos[i].Flags = ents.SpriteFlags[i];
    }

    // Promote sprite resolutions and mark legacy resolution setting
    if (data_ver < kGameVersion_350)
    {
        for (size_t i = 0; i < ents.SpriteCount; ++i)
        {
            SpriteInfo &info = game.SpriteInfos[i];
            if (game.IsLegacyHiRes() == info.IsLegacyHiRes())
                info.Flags &= ~(SPF_HIRES | SPF_VAR_RESOLUTION);
            else
                info.Flags |= SPF_VAR_RESOLUTION;
        }
    }
}

void UpgradeFonts(GameSetupStruct &game, GameDataVersion data_ver)
{
    if (data_ver < kGameVersion_350)
    {
        for (int i = 0; i < game.numfonts; ++i)
        {
            FontInfo &finfo = game.fonts[i];
            // If the game is hi-res but font is designed for low-res, then scale it up
            if (game.IsLegacyHiRes() && game.options[OPT_HIRES_FONTS] == 0)
            {
                finfo.SizeMultiplier = HIRES_COORD_MULTIPLIER;
            }
            else
            {
                finfo.SizeMultiplier = 1;
            }
        }
    }
    if (data_ver < kGameVersion_360)
    {
        for (int i = 0; i < game.numfonts; ++i)
        {
            FontInfo &finfo = game.fonts[i];
            if (finfo.Outline == FONT_OUTLINE_AUTO)
            {
                finfo.AutoOutlineStyle = FontInfo::kSquared;
                finfo.AutoOutlineThickness = 1;
            }
        }
    }
    if (data_ver < kGameVersion_360_11)
    { // use global defaults for the font load flags
        for (int i = 0; i < game.numfonts; ++i)
        {
            game.fonts[i].Flags |= FFLG_TTF_BACKCOMPATMASK;
        }
    }
}

// Convert audio data to the current version
void UpgradeAudio(GameSetupStruct &game, LoadedGameEntities &ents, GameDataVersion data_ver)
{
    if (data_ver >= kGameVersion_320)
        return;

    // An explanation of building audio clips array for pre-3.2 games.
    //
    // When AGS version 3.2 was released, it contained new audio system.
    // In the nutshell, prior to 3.2 audio files had to be manually put
    // to game project directory and their IDs were taken out of filenames.
    // Since 3.2 this information is stored inside the game data.
    // To make the modern engine compatible with pre-3.2 games, we have
    // to scan game data packages for audio files, and enumerate them
    // ourselves, then add this information to game struct.

    // Create soundClips and audioClipTypes structures.
    std::vector<AudioClipType> audiocliptypes;
    std::vector<ScriptAudioClip> audioclips;

    // TODO: find out what is 4 (maybe music, sound, ambient sound, voice?)
    audiocliptypes.resize(4);
    for (int i = 0; i < 4; i++)
    {
        audiocliptypes[i].reservedChannels = 1;
        audiocliptypes[i].id = i;
        audiocliptypes[i].volume_reduction_while_speech_playing = 10;
    }
    audiocliptypes[3].reservedChannels = 0;

    audioclips.reserve(1000);
    std::vector<String> assets;
    // Read audio clip names from from registered libraries
    for (size_t i = 0; i < AssetMgr->GetLibraryCount(); ++i)
    {
        const AssetLibInfo *game_lib = AssetMgr->GetLibraryInfo(i);
        if (File::IsDirectory(game_lib->BasePath))
            continue; // might be a directory

        for (const AssetInfo &info : game_lib->AssetInfos)
        {
            if (info.FileName.CompareLeftNoCase("music", 5) == 0 || info.FileName.CompareLeftNoCase("sound", 5) == 0)
                assets.push_back(info.FileName);
        }
    }
    // Append contents of the registered directories
    // TODO: implement pattern search or asset query with callback (either of two or both)
    // within AssetManager to avoid doing this in place here. Alternatively we could maybe
    // make AssetManager to do directory scans by demand and fill AssetInfos...
    // but that have to be done consistently if done at all.
    for (size_t i = 0; i < AssetMgr->GetLibraryCount(); ++i)
    {
        const AssetLibInfo *game_lib = AssetMgr->GetLibraryInfo(i);
        if (!File::IsDirectory(game_lib->BasePath))
            continue; // might be a library

        for (FindFile ff = FindFile::OpenFiles(game_lib->BasePath, "*.*"); !ff.AtEnd(); ff.Next())
        {
            String filename = ff.Current();
            if (ags_strnicmp(filename.GetCStr(), "music", 5) == 0 || ags_strnicmp(filename.GetCStr(), "sound", 5) == 0)
                assets.push_back(filename);
        }
    }
    BuildAudioClipArray(assets, audioclips);

    // Copy gathered data over to game
    game.audioClipTypes = audiocliptypes;
    game.audioClips = audioclips;
    
    RemapLegacySoundNums(game, ents.Views, data_ver);
}

// Convert character data to the current version
void UpgradeCharacters(GameSetupStruct &game, GameDataVersion data_ver)
{
    // TODO: this was done to simplify code transition; ideally we should be
    // working with GameSetupStruct's getters and setters here
    CharacterInfo *&chars = game.chars;
    const int numcharacters = game.numcharacters;

    // Fixup charakter script names for 2.x (EGO -> cEgo)
    if (data_ver <= kGameVersion_272)
    {
        String tempbuffer;
        for (int i = 0; i < numcharacters; i++)
        {
            if (chars[i].scrname[0] == 0)
                continue;
            tempbuffer.Format("c%c%s", chars[i].scrname[0], ags_strlwr(&chars[i].scrname[1]));
            snprintf(chars[i].scrname, MAX_SCRIPT_NAME_LEN, "%s", tempbuffer.GetCStr());
        }
    }

    // Fix character walk speed for < 3.1.1
    if (data_ver <= kGameVersion_310)
    {
        for (int i = 0; i < numcharacters; i++)
        {
            if (game.options[OPT_ANTIGLIDE])
                chars[i].flags |= CHF_ANTIGLIDE;
        }
    }

    // Characters can always walk through each other on < 2.54
    if (data_ver < kGameVersion_254)
    {
        for (int i = 0; i < numcharacters; i++)
        {
            chars[i].flags |= CHF_NOBLOCKING;
        }
    }
}

void UpgradeMouseCursors(GameSetupStruct &game, GameDataVersion data_ver)
{
    if (data_ver <= kGameVersion_272)
    {
        // Change cursor.view from 0 to -1 for non-animating cursors.
        for (int i = 0; i < game.numcursors; ++i)
        {
            if (game.mcurs[i].view == 0)
                game.mcurs[i].view = -1;
        }
    }
}

// Adjusts score clip id, depending on game data version
void RemapLegacySoundNums(GameSetupStruct &game, std::vector<ViewStruct> &views, GameDataVersion data_ver)
{
    if (data_ver >= kGameVersion_320)
        return;

    // Setup sound clip played on score event
    game.scoreClipID = -1;
    if (game.options[OPT_SCORESOUND] > 0)
    {
        ScriptAudioClip* clip = GetAudioClipForOldStyleNumber(game, false, game.options[OPT_SCORESOUND]);
        if (clip)
            game.scoreClipID = clip->id;
    }

    // Reset view frame clip refs
    // NOTE: we do not map these to real clips right away,
    // instead we do this at runtime whenever we find a non-mapped frame sound.
    for (size_t v = 0; v < (size_t)game.numviews; ++v)
    {
        for (size_t l = 0; l < (size_t)views[v].numLoops; ++l)
        {
            for (size_t f = 0; f < (size_t)views[v].loops[l].numFrames; ++f)
            {
                views[v].loops[l].frames[f].audioclip = -1;
            }
        }
    }
}

// Assigns default global message at given index
void SetDefaultGlmsg(GameSetupStruct &game, int msgnum, const char *val)
{
    // TODO: find out why the index should be lowered by 500
    // (or rather if we may pass correct index right away)
    msgnum -= 500;
    if (game.messages[msgnum] == nullptr)
        game.messages[msgnum] = ags_strdup(val);
}

// Sets up default global messages (these are used mainly in older games)
void SetDefaultGlobalMessages(GameSetupStruct &game)
{
    SetDefaultGlmsg(game, 983, "Sorry, not now.");
    SetDefaultGlmsg(game, 984, "Restore");
    SetDefaultGlmsg(game, 985, "Cancel");
    SetDefaultGlmsg(game, 986, "Select a game to restore:");
    SetDefaultGlmsg(game, 987, "Save");
    SetDefaultGlmsg(game, 988, "Type a name to save as:");
    SetDefaultGlmsg(game, 989, "Replace");
    SetDefaultGlmsg(game, 990, "The save directory is full. You must replace an existing game:");
    SetDefaultGlmsg(game, 991, "Replace:");
    SetDefaultGlmsg(game, 992, "With:");
    SetDefaultGlmsg(game, 993, "Quit");
    SetDefaultGlmsg(game, 994, "Play");
    SetDefaultGlmsg(game, 995, "Are you sure you want to quit?");
    SetDefaultGlmsg(game, 996, "You are carrying nothing.");
}

void FixupSaveDirectory(GameSetupStruct &game)
{
    // If the save game folder was not specified by game author, create one of
    // the game name, game GUID, or uniqueid, as a last resort
    if (!game.saveGameFolderName[0])
    {
        if (game.gamename[0])
            snprintf(game.saveGameFolderName, MAX_SG_FOLDER_LEN, "%s", game.gamename);
        else if (game.guid[0])
            snprintf(game.saveGameFolderName, MAX_SG_FOLDER_LEN, "%s", game.guid);
        else
            snprintf(game.saveGameFolderName, MAX_SG_FOLDER_LEN, "AGS-Game-%d", game.uniqueid);
    }
    // Lastly, fixup folder name by removing any illegal characters
    String s = Path::FixupSharedFilename(game.saveGameFolderName);
    snprintf(game.saveGameFolderName, MAX_SG_FOLDER_LEN, "%s", s.GetCStr());
}

HGameFileError ReadSpriteFlags(LoadedGameEntities &ents, Stream *in, GameDataVersion data_ver)
{
    size_t sprcount;
    if (data_ver < kGameVersion_256)
        sprcount = LEGACY_MAX_SPRITES_V25;
    else
        sprcount = in->ReadInt32();
    if (sprcount > (size_t)SpriteCache::MAX_SPRITE_INDEX + 1)
        return new MainGameFileError(kMGFErr_TooManySprites, String::FromFormat("Count: %zu, max: %zu", sprcount, (size_t)SpriteCache::MAX_SPRITE_INDEX + 1));

    ents.SpriteCount = sprcount;
    ents.SpriteFlags.reset(new char[sprcount]);
    in->Read(ents.SpriteFlags.get(), sprcount);
    return HGameFileError::None();
}


// GameDataExtReader reads main game data's extension blocks
class GameDataExtReader : public DataExtReader
{
public:
    GameDataExtReader(LoadedGameEntities &ents, GameDataVersion data_ver, Stream *in)
        : DataExtReader(in, kDataExt_NumID8 | kDataExt_File64)
        , _ents(ents)
        , _dataVer(data_ver)
    {}

protected:
    HError ReadBlock(int block_id, const String &ext_id,
        soff_t block_len, bool &read_next) override;

    LoadedGameEntities &_ents;
    GameDataVersion _dataVer {};
};

HError GameDataExtReader::ReadBlock(int /*block_id*/, const String &ext_id,
    soff_t /*block_len*/, bool &read_next)
{
    read_next = true;
    // Add extensions here checking ext_id, which is an up to 16-chars name, for example:
    // if (ext_id.CompareNoCase("GUI_NEWPROPS") == 0)
    // {
    //     // read new gui properties
    // }
    if (ext_id.CompareNoCase("v360_fonts") == 0)
    {
        for (int i = 0; i < _ents.Game.numfonts; ++i)
        {
            // adjustable font outlines
            _ents.Game.fonts[i].AutoOutlineThickness = _in->ReadInt32();
            _ents.Game.fonts[i].AutoOutlineStyle =
                static_cast<enum FontInfo::AutoOutlineStyle>(_in->ReadInt32());
            // reserved
            _in->ReadInt32();
            _in->ReadInt32();
            _in->ReadInt32();
            _in->ReadInt32();
        }
        return HError::None();
    }
    else if (ext_id.CompareNoCase("v360_cursors") == 0)
    {
        for (int i = 0; i < _ents.Game.numcursors; ++i)
        {
            _ents.Game.mcurs[i].animdelay = _in->ReadInt32();
            // reserved
            _in->ReadInt32();
            _in->ReadInt32();
            _in->ReadInt32();
        }
        return HError::None();
    }
    return new MainGameFileError(kMGFErr_ExtUnknown, String::FromFormat("Type: %s", ext_id.GetCStr()));
}


HGameFileError ReadGameData(LoadedGameEntities &ents, Stream *in, GameDataVersion data_ver)
{
    GameSetupStruct &game = ents.Game;

    //-------------------------------------------------------------------------
    // The classic data section.
    //-------------------------------------------------------------------------
    {
        AlignedStream align_s(in, Common::kAligned_Read);
        game.GameSetupStructBase::ReadFromFile(&align_s);
    }

    Debug::Printf(kDbgMsg_Info, "Game title: '%s'", game.gamename);

    if (game.GetGameRes().IsNull())
        return new MainGameFileError(kMGFErr_InvalidNativeResolution);

    game.read_savegame_info(in, data_ver);
    game.read_font_infos(in, data_ver);
    HGameFileError err = ReadSpriteFlags(ents, in, data_ver);
    if (!err)
        return err;
    game.ReadInvInfo_Aligned(in);
    err = game.read_cursors(in);
    if (!err)
        return err;
    game.read_interaction_scripts(in, data_ver);
    game.read_words_dictionary(in);

    if (game.load_compiled_script)
    {
        ents.GlobalScript.reset(ccScript::CreateFromStream(in));
        if (!ents.GlobalScript)
            return new MainGameFileError(kMGFErr_CreateGlobalScriptFailed, cc_get_error().ErrorString);
        err = ReadDialogScript(ents.DialogScript, in, data_ver);
        if (!err)
            return err;
        err = ReadScriptModules(ents.ScriptModules, in, data_ver);
        if (!err)
            return err;
    }

    ReadViews(game, ents.Views, in, data_ver);

    if (data_ver <= kGameVersion_251)
    {
        // skip unknown data
        int count = in->ReadInt32();
        in->Seek(count * 0x204);
    }

    game.read_characters(in);
    game.read_lipsync(in, data_ver);
    game.read_messages(in, data_ver);

    ReadDialogs(ents.Dialogs, ents.OldDialogScripts, ents.OldDialogSources, ents.OldSpeechLines,
                in, data_ver, game.numdialog);
    HError err2 = GUI::ReadGUI(in);
    if (!err2)
        return new MainGameFileError(kMGFErr_GameEntityFailed, err2);
    game.numgui = guis.size();

    if (data_ver >= kGameVersion_260)
    {
        err = ReadPlugins(ents.PluginInfos, in);
        if (!err)
            return err;
    }

    err = game.read_customprops(in, data_ver);
    if (!err)
        return err;
    err = game.read_audio(in, data_ver);
    if (!err)
        return err;
    game.read_room_names(in, data_ver);

    if (data_ver <= kGameVersion_350)
        return HGameFileError::None();

    //-------------------------------------------------------------------------
    // All the extended data, for AGS > 3.5.0.
    //-------------------------------------------------------------------------
    GameDataExtReader reader(ents, data_ver, in);
    HError ext_err = reader.Read();
    return ext_err ? HGameFileError::None() : new MainGameFileError(kMGFErr_ExtListFailed, ext_err);
}

HGameFileError UpdateGameData(LoadedGameEntities &ents, GameDataVersion data_ver)
{
    GameSetupStruct &game = ents.Game;
    ApplySpriteData(game, ents, data_ver);
    UpgradeFonts(game, data_ver);
    UpgradeAudio(game, ents, data_ver);
    UpgradeCharacters(game, data_ver);
    UpgradeMouseCursors(game, data_ver);
    SetDefaultGlobalMessages(game);
    // Global talking animation speed
    if (data_ver < kGameVersion_312)
    {
        // Fix animation speed for old formats
        game.options[OPT_GLOBALTALKANIMSPD] = 5;
    }
    else if (data_ver < kGameVersion_330)
    {
        // Convert game option for 3.1.2 - 3.2 games
        game.options[OPT_GLOBALTALKANIMSPD] = game.options[OPT_GLOBALTALKANIMSPD] != 0 ? 5 : (-5 - 1);
    }
    // Old dialog options API for pre-3.4.0.2 games
    if (data_ver < kGameVersion_340_2)
    {
        game.options[OPT_DIALOGOPTIONSAPI] = -1;
    }
    // Relative asset resolution in pre-3.5.0.8 (always enabled)
    if (data_ver < kGameVersion_350)
    {
        game.options[OPT_RELATIVEASSETRES] = 1;
    }
    FixupSaveDirectory(game);
    return HGameFileError::None();
}

void PreReadGameData(GameSetupStruct &game, Stream *in, GameDataVersion data_ver)
{
    {
        AlignedStream align_s(in, Common::kAligned_Read);
        game.ReadFromFile(&align_s);
    }
    // Discard game messages we do not need here
    delete[] game.load_messages;
    game.load_messages = nullptr;
    game.read_savegame_info(in, data_ver);
}

} // namespace Common
} // namespace AGS
