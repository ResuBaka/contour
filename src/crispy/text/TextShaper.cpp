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
#include <crispy/text/TextShaper.h>
#include <crispy/algorithm.h>
#include <crispy/times.h>
#include <crispy/span.h>

#include <unicode/utf8.h>

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

#include <fmt/format.h>

#include <iostream>
#include <functional>

using namespace std;

namespace crispy::text {

namespace
{
    constexpr bool glyphMissing(GlyphPosition const& _gp) noexcept
    {
        return _gp.glyphIndex == 0;
    }
}

TextShaper::TextShaper() :
    hb_buf_(
        hb_buffer_create(),
        [](auto p) { hb_buffer_destroy(p); }
    )
{
    if (!hb_buf_)
        throw std::runtime_error("Could not initialize text shaper.");
}

TextShaper::~TextShaper()
{
    clearCache();
}

GlyphPositionList TextShaper::shape(unicode::Script _script,
                                    FontList const& _fonts,
                                    int _advanceX,
                                    int _size,
                                    char32_t const* _codepoints,
                                    int const* _clusters,
                                    int _clusterGap)
{
    GlyphPositionList glyphPositions;

    // try primary font
    if (shape(_size, _codepoints, _clusters, _clusterGap, _script, _fonts.first.get(), _advanceX, ref(glyphPositions)))
        return glyphPositions;

    // try fallback fonts
    for (reference_wrapper<Font> const& fallback : _fonts.second)
        if (shape(_size, _codepoints, _clusters, _clusterGap, _script, fallback.get(), _advanceX, ref(glyphPositions)))
            return glyphPositions;

#if !defined(NDEBUG)
    string joinedCodes;
    for (char32_t codepoint : crispy::span(_codepoints, _codepoints + _size))
    {
        if (!joinedCodes.empty())
            joinedCodes += " ";
        joinedCodes += fmt::format("{:<6x}", static_cast<unsigned>(codepoint));
    }
    cerr << fmt::format("Shaping failed codepoints: {}\n", joinedCodes);
#endif

    // render primary font with glyph-missing hints
    shape(_size, _codepoints, _clusters, _clusterGap, _script, _fonts.first.get(), _advanceX, ref(glyphPositions));
    replaceMissingGlyphs(_fonts.first.get(), glyphPositions);
    return glyphPositions;
}

void TextShaper::clearCache()
{
    for ([[maybe_unused]] auto [_, hbf] : hb_fonts_)
        hb_font_destroy(hbf);

    hb_fonts_.clear();
}

constexpr hb_script_t mapScriptToHarfbuzzScript(unicode::Script _script)
{
    using unicode::Script;
    switch (_script)
    {
        case Script::Latin:
            return HB_SCRIPT_LATIN;
        case Script::Greek:
            return HB_SCRIPT_GREEK;
        case Script::Common:
            return HB_SCRIPT_COMMON;
        default:
            // TODO: make this list complete
            return HB_SCRIPT_INVALID; // hb_buffer_guess_segment_properties() will fill it
    }
}

bool TextShaper::shape(int _size,
                       char32_t const* _codepoints,
                       int const* _clusters,
                       int _clusterGap,
                       unicode::Script _script,
                       Font& _font,
                       int _advanceX,
                       reference<GlyphPositionList> _result)
{
    hb_buffer_clear_contents(hb_buf_.get());

    for (size_t const i : times(_size))
        hb_buffer_add(hb_buf_.get(), _codepoints[i], _clusters[i] + _clusterGap);

    hb_buffer_set_content_type(hb_buf_.get(), HB_BUFFER_CONTENT_TYPE_UNICODE);
    hb_buffer_set_direction(hb_buf_.get(), HB_DIRECTION_LTR);
    hb_buffer_set_script(hb_buf_.get(), mapScriptToHarfbuzzScript(_script));
    hb_buffer_set_language(hb_buf_.get(), hb_language_get_default());
    hb_buffer_guess_segment_properties(hb_buf_.get());

    hb_font_t* hb_font = nullptr;
    if (auto i = hb_fonts_.find(&_font); i != hb_fonts_.end())
        hb_font = i->second;
    else
    {
        hb_font = hb_ft_font_create_referenced(_font);
        hb_fonts_[&_font] = hb_font;
    }

    hb_shape(hb_font, hb_buf_.get(), nullptr, 0);

    hb_buffer_normalize_glyphs(hb_buf_.get());

    auto const glyphCount = static_cast<int>(hb_buffer_get_length(hb_buf_.get()));
    hb_glyph_info_t const* info = hb_buffer_get_glyph_infos(hb_buf_.get(), nullptr);
    hb_glyph_position_t const* pos = hb_buffer_get_glyph_positions(hb_buf_.get(), nullptr);

    _result.get().clear();
    _result.get().reserve(glyphCount);

    int cx = 0;
    int cy = 0;
    for (auto const i : times(glyphCount))
    {
        cx = static_cast<int>(info[i].cluster) * _advanceX;
        //cx = _clusters[i] * _advanceX;

        // TODO: maybe right in here, apply incremented cx/xy only if cluster number has changed?
        _result.get().emplace_back(GlyphPosition{
            _font,
            cx + (pos[i].x_offset >> 6),
            cy + (pos[i].y_offset >> 6),
            info[i].codepoint,
            static_cast<int>(info[i].cluster)
        });
    }

#if 0 // !defined(NDEBUG))
    if (crispy::none_of(_result.get(), glyphMissing))
    {
        std::cout << "Shaping: " << unicode::to_utf8(_codepoints, _size);

        // for (size_t const i : times(_size))
        //     std::cout << fmt::format(" {:04X}", static_cast<uint32_t>(_codepoints[i]));

        for (auto const i : times(glyphCount))
            std::cout << fmt::format(
                " (cp:{} x:{}, y:{})",
                info[i].codepoint,
                pos[i].x_offset,// >> 6,
                pos[i].y_offset // >> 6
            );
        std::cout << '\n';
    }
#endif

    return !crispy::any_of(_result.get(), glyphMissing);
}

void TextShaper::replaceMissingGlyphs(Font& _font, GlyphPositionList& _result)
{
    auto constexpr missingGlyphId = 0xFFFDu;
    auto const missingGlyph = FT_Get_Char_Index(_font, missingGlyphId);

    if (missingGlyph)
    {
        for (auto i : times(_result.size()))
            if (glyphMissing(_result[i]))
                _result[i].glyphIndex = missingGlyph;
    }
}

} // end namespace
