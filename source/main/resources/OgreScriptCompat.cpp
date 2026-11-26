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
#include "OgreScriptCompat.h"

#include <cstdlib>
#include <cstring>

using namespace RoR;

static const char* const FRAME_PREFIX = "RoR-frame~"; //Must not start with a digit, sign or space

namespace {

bool StartsWithInteger(std::string const& s)
{
    char* end = nullptr;
    std::strtoul(s.c_str(), &end, 0);
    return end != s.c_str();
}

bool IsNumber(std::string const& s)
{
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return !s.empty() && *end == '\0';
}

void FixAnimTextures(const Ogre::AbstractNodeList& nodes, std::vector<std::string>& messages)
{
    for (const Ogre::AbstractNodePtr& node : nodes)
    {
        if (node->type == Ogre::ANT_OBJECT)
        {
            FixAnimTextures(static_cast<Ogre::ObjectAbstractNode*>(node.get())->children, messages);
        }
        else if (node->type == Ogre::ANT_PROPERTY)
        {
            auto* prop = static_cast<Ogre::PropertyAbstractNode*>(node.get());
            if (prop->id != Ogre::ID_ANIM_TEXTURE || prop->values.size() < 3)
                continue;

            Ogre::AbstractNodePtr second = *std::next(prop->values.begin());
            if (second->type != Ogre::ANT_ATOM)
                continue;
            std::string& value = static_cast<Ogre::AtomAbstractNode*>(second.get())->value;
            if (StartsWithInteger(value) && !IsNumber(value))
            {
                messages.push_back(prop->file + " line " + std::to_string(prop->line) + ": 'anim_texture' frame '"
                    + value + "' would be read as a frame count");
                value.insert(0, FRAME_PREFIX); //No longer starts with a digit, so it stays idempotent
            }
        }
    }
}

} // namespace

void RoR::ApplyOgre111AnimTextureCompat(const Ogre::AbstractNodeListPtr& nodes, std::vector<std::string>& messages)
{
    if (nodes)
        FixAnimTextures(*nodes, messages);
}

void RoR::RemoveFramePrefix(std::string& texture_name)
{
    const size_t len = std::strlen(FRAME_PREFIX);
    if (texture_name.compare(0, len, FRAME_PREFIX) == 0)
        texture_name.erase(0, len);
}

bool RoR::ApplyOgre111ScriptCompat(std::string& text, std::vector<std::string>& messages)
{
    // One pass over the characters, with the states taken from Ogre::ScriptLexer::_tokensize()
    // (same logic in 14.5, quirks included), so that what counts as a bracket, comment, or quote
    // here is exactly what counts there.
    // Braces count only outside comments and quotes (READY/WORD states): '{' +1, '}' -1. A '}' at depth 0 is replaced by a
    // space and logged; length and line numbers stay same, so OGRE's own logs still point to the right line
    // -----------------------------------------------------------------------------------------
    enum class State { READY, COMMENT, MULTICOMMENT, WORD, QUOTE, POSSIBLECOMMENT };

    State state = State::READY;
    char c = 0, lastc = 0;
    int line = 1;
    int brace_layer = 0;
    bool modified = false;

    for (size_t i = 0; i < text.size(); i++)
    {
        lastc = c;
        c = text[i];

        if (state == State::READY || state == State::WORD)
        {
            if (c == '{')
            {
                brace_layer++;
            }
            else if (c == '}')
            {
                if (brace_layer == 0)
                {
                    text[i] = c = ' ';
                    modified = true;
                    messages.push_back("line " + std::to_string(line) + ": '}' without matching '{' ignored");
                }
                else
                {
                    brace_layer--;
                }
            }
        }

        const bool newline = (c == '\n' || c == '\r');
        const bool space = (c == ' ' || c == '\t');

        switch (state)
        {
        case State::READY:
            if (c == '/' && lastc == '/')
                state = State::COMMENT;
            else if (c == '*' && lastc == '/')
                state = State::MULTICOMMENT;
            else if (c == '"')
                state = State::QUOTE;
            else if (!newline && !space)
                state = (c == '/') ? State::POSSIBLECOMMENT : State::WORD;
            break;

        case State::COMMENT:
            if (newline)
                state = State::READY;
            break;

        case State::MULTICOMMENT:
            if (c == '/' && lastc == '*')
                state = State::READY;
            break;

        case State::POSSIBLECOMMENT:
            if (c == '/' && lastc == '/')
            {
                state = State::COMMENT;
                break;
            }
            else if (c == '*' && lastc == '/')
            {
                state = State::MULTICOMMENT;
                break;
            }
            state = State::WORD;
            // fall through
        case State::WORD:
            if (newline || space || c == '{' || c == '}' || c == ':')
                state = State::READY;
            break;

        case State::QUOTE:
            if (c == '"' && lastc != '\\')
                state = State::READY;
            break;
        }

        if (c == '\r' || (c == '\n' && lastc != '\r'))
            line++;
    }
    return modified;
}
