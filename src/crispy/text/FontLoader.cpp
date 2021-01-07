/**
 * This file is part of the "libterminal" project
 *   Copyright (c) 2019-2020 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <crispy/text/FontLoader.h>
#include <crispy/text/Font.h>
#include <crispy/logger.h>

#include <fmt/format.h>

#include <stdexcept>
#include <vector>
#include <iostream>

#if defined(__linux__) || defined(__APPLE__)
#define HAVE_FONTCONFIG // TODO: use cmake, dude!
#endif

#if defined(HAVE_FONTCONFIG)
#include <fontconfig/fontconfig.h>
#endif

namespace crispy::text {

using namespace std;

namespace {
    static bool endsWithIgnoreCase(string const& _text, string const& _suffix)
    {
        if (_text.size() < _suffix.size())
            return false;

        auto const* text = &_text[_text.size() - _suffix.size()];
        for (size_t i = 0; i < _suffix.size(); ++i)
            if (tolower(text[i]) != tolower(_suffix[i]))
                return false;

        return true;
    }

    static vector<string> getFontFilePaths([[maybe_unused]] string const& _fontPattern)
    {
        if (endsWithIgnoreCase(_fontPattern, ".ttf") || endsWithIgnoreCase(_fontPattern, ".otf"))
            return {_fontPattern};

        #if defined(HAVE_FONTCONFIG)
        string const& pattern = _fontPattern; // TODO: append bold/italic if needed

        FcConfig* fcConfig = FcInitLoadConfigAndFonts();
        FcPattern* fcPattern = FcNameParse((FcChar8 const*) pattern.c_str());

        FcDefaultSubstitute(fcPattern);
        FcConfigSubstitute(fcConfig, fcPattern, FcMatchPattern);

        FcResult fcResult = FcResultNoMatch;

        vector<string> paths;

        // find font along with all its fallback fonts
        FcCharSet* fcCharSet = nullptr;
        FcFontSet* fcFontSet = FcFontSort(fcConfig, fcPattern, /*trim*/FcTrue, &fcCharSet, &fcResult);
        for (int i = 0; i < fcFontSet->nfont; ++i)
        {
            FcChar8* fcFile = nullptr;
            if (FcPatternGetString(fcFontSet->fonts[i], FC_FILE, 0, &fcFile) == FcResultMatch)
            {
                // FcBool fcColor = false;
                // FcPatternGetBool(fcFontSet->fonts[i], FC_COLOR, 0, &fcColor);
                if (fcFile)
                    paths.emplace_back((char const*) fcFile);
            }
        }
        FcFontSetDestroy(fcFontSet);
        FcCharSetDestroy(fcCharSet);

        FcPatternDestroy(fcPattern);
        FcConfigDestroy(fcConfig);
        return paths;
        #endif

        #if defined(_WIN32)
        // TODO: Read https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-enumfontfamiliesexa
        // This is pretty damn hard coded, and to be properly implemented once the other font related code's done,
        // *OR* being completely deleted when FontConfig's windows build fix released and available via vcpkg.
        if (_fontPattern.find("bold italic") != string::npos)
            return {"C:\\Windows\\Fonts\\consolaz.ttf"};
        else if (_fontPattern.find("italic") != string::npos)
            return {"C:\\Windows\\Fonts\\consolai.ttf"};
        else if (_fontPattern.find("bold") != string::npos)
            return {"C:\\Windows\\Fonts\\consolab.ttf"};
        else
            return {"C:\\Windows\\Fonts\\consola.ttf"};
        #endif
    }
}

FontLoader::FontLoader() :
    ft_{},
    fonts_{}
{
    if (FT_Init_FreeType(&ft_))
        throw runtime_error{ "Failed to initialize FreeType." };
}

FontLoader::~FontLoader()
{
    fonts_.clear();
    FT_Done_FreeType(ft_);
}

FontList FontLoader::load(string const& _fontPattern, int _fontSize)
{
    vector<string> const filePaths = getFontFilePaths(_fontPattern);

    Font* primaryFont = loadFromFilePath(filePaths.front(), _fontSize);
    if (!primaryFont)
        throw runtime_error{fmt::format("Failed to load primary font \"{}\".", _fontPattern)};

    FontFallbackList fallbackList;
    for (size_t i = 1; i < filePaths.size(); ++i)
        if (auto fallbackFont = loadFromFilePath(filePaths[i], _fontSize); fallbackFont != nullptr)
            fallbackList.push_back(*fallbackFont);

    debuglog().write("FontLoader: loading font \"{}\" from \"{}\", baseline={}, height={}, size={}, fallbacks={}",
                     _fontPattern,
                     primaryFont->filePath(),
                     primaryFont->baseline(),
                     primaryFont->bitmapHeight(),
                     _fontSize,
                     fallbackList.size());

    return {*primaryFont, fallbackList};
}

Font* FontLoader::loadFromFilePath(std::string const& _path, int _fontSize)
{
    if (auto k = fonts_.find(_path); k != fonts_.end())
    {
        if (k->second.fontSize() != _fontSize)
            k->second.setFontSize(_fontSize);
        return &k->second;
    }

    if (auto face = Font::loadFace(ft_, _path, _fontSize); face != nullptr)
        return &fonts_.emplace(make_pair(_path, Font(ft_, face, _fontSize, _path))).first->second;

    return nullptr;
}

} // end namespace
