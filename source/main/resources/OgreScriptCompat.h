/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2026 Petr Ohlidal

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

/// @file OgreScriptCompat.h
/// @author Rafael Galvan, 2026
/// @brief OGRE script compatibility 
///https://github.com/CuriousMike56/rigs-of-rods-ogre14/pull/1#issuecomment-5841428761)

#pragma once

#include <OgreScriptCompiler.h>

#include <string>
#include <vector>

namespace RoR {

/// Apply legacy OGRE script compatibility.
/// @param text Modified only if something needed it.
/// @param messages What was repaired, with line numbers, for the log.
/// @return true if `text` was modified.
bool ApplyOgre111ScriptCompat(std::string& text, std::vector<std::string>& messages);

/// anim_texture has a short form <base> <frame count> <duration> and a long form <frame> <frame>... <duration>.
/// Older OGRE versions took the short form only if the 2nd value was a whole number; OGRE 14 takes it if the value starts
/// with one, so anim_texture flag101.dds 2af11UID-flag202.dds ... became "202 frames of flag101.dds".
/// Run on the parsed tree (ScriptCompilerListener::postConversion), right before OGRE translates it: such a 2nd value
/// gets a prefix so OGRE takes the long form; RemoveFramePrefix() takes it off again when OGRE reports the name.
/// @param messages What was repaired, with file and line, for the log.
void ApplyOgre111AnimTextureCompat(const Ogre::AbstractNodeListPtr& nodes, std::vector<std::string>& messages);

/// Removes the prefix added by `ApplyOgre111AnimTextureCompat()`; for texture names reported by OGRE
/// (ProcessResourceNameScriptCompilerEvent).
void RemoveFramePrefix(std::string& texture_name);

} // namespace RoR
