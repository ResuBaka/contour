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
#pragma once

#include <crispy/reference.h>

#include <cmath>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H

#if defined(_MSC_VER)
// XXX purely for IntelliSense
#include <freetype/freetype.h>
#endif

#include <array>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

// TODO: move all font stuff into crispy::text namespace

namespace crispy {
    using CharSequence = std::vector<char32_t>;

    struct Codepoint {
        char32_t value;
        int cluster;
    };
    using CodepointSequence = std::vector<Codepoint>;

    inline bool operator==(CodepointSequence const& a, CodepointSequence const& b) noexcept
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].value != b[i].value)
                return false;

        return true;
    }

    inline bool operator==(CharSequence const& a, CodepointSequence const& b) noexcept
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
            if (a[i] != b[i].value)
                return false;
        return true;
    }

    inline bool operator==(CodepointSequence const& a, CharSequence const& b) noexcept { return b == a; }
    inline bool operator!=(CodepointSequence const& a, CharSequence const& b) noexcept { return !(a == b); }
    inline bool operator!=(CharSequence const& a, CodepointSequence const& b) noexcept { return !(a == b); }
}

namespace std {
    template<>
    struct hash<crispy::CharSequence> {
        std::size_t operator()(crispy::CharSequence const& seq) const noexcept
        {
            // Using FNV to create a hash value for the character sequence.
            auto constexpr basis = 2166136261llu;
            auto constexpr prime = 16777619llu;

            if (!seq.empty())
            {
                auto h = basis;
                for (auto const ch : seq)
                {
                    h ^= ch;
                    h *= prime;
                }
                return h;
            }
            else
                return 0;
        }
    };

    template<>
    struct hash<crispy::CodepointSequence> {
        std::size_t operator()(crispy::CodepointSequence const& seq) const noexcept
        {
            // Using FNV to create a hash value for the character sequence.
            auto constexpr basis = 2166136261llu;
            auto constexpr prime = 16777619llu;

            if (!seq.empty())
            {
                auto h = basis;
                for (auto const& ch : seq)
                {
                    h ^= ch.value;
                    h *= prime;
                }
                return h;
            }
            else
                return 0;
        }
    };
}

namespace crispy::text {

// TODO: remove FontStyle?
enum class FontStyle {
    Regular = 0,
    Bold = 1,
    Italic = 2,
    BoldItalic = 3,
};

constexpr FontStyle operator|(FontStyle lhs, FontStyle rhs)
{
    return static_cast<FontStyle>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
}

constexpr FontStyle& operator|=(FontStyle& lhs, FontStyle rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

struct GlyphBitmap {
    int width;
    int height;
    std::vector<uint8_t> buffer;
};

class Font;

struct GlyphPosition {
    std::reference_wrapper<Font> font;
    int x;
    int y;
    uint32_t glyphIndex;
    int cluster;

    GlyphPosition(Font& _font, int _x, int _y, uint32_t _gi, int _cluster) :
        font{_font}, x{_x}, y{_y}, glyphIndex{_gi}, cluster{_cluster} {}
};

using GlyphPositionList = std::vector<GlyphPosition>;

/**
 * Represents one Font face along with support for its fallback fonts.
 */
class Font {
  public:
    Font(FT_Library _ft, FT_Face _face, int _fontSize, std::string _fontPath);
    Font(Font const&) = delete;
    Font& operator=(Font const&) = delete;
    Font(Font&&) noexcept;
    Font& operator=(Font&&) noexcept;
    ~Font();

    std::string const& filePath() const noexcept { return filePath_; }
    std::size_t hashCode() const noexcept { return hashCode_; }

    void setFontSize(int _fontSize);
    int fontSize() const noexcept { return fontSize_; }

    bool hasColor() const noexcept { return FT_HAS_COLOR(face_); }

    int bitmapWidth() const noexcept { return bitmapWidth_; }
    int bitmapHeight() const noexcept { return bitmapHeight_; }

    int lineHeight() const noexcept
    {
        return static_cast<int>(std::ceil(static_cast<double>(FT_MulFix(face_->height, face_->size->metrics.y_scale)) / 64.0));
    }

    int maxAdvance() const noexcept { return maxAdvance_; }
    int baseline() const noexcept { return static_cast<int>(abs(face_->size->metrics.descender) >> 6); }

    bool isFixedWidth() const noexcept { return face_->face_flags & FT_FACE_FLAG_FIXED_WIDTH; }

    void loadGlyphByChar(char32_t _char) { loadGlyphByIndex(FT_Get_Char_Index(face_, _char)); }

    std::optional<GlyphBitmap> loadGlyphByIndex(int _glyphIndex);

    operator FT_Face () noexcept { return face_; }
    FT_Face operator->() noexcept { return face_; }

    static FT_Face loadFace(FT_Library _ft, std::string const& _fontPath, int _fontSize);

  private:
    static bool doSetFontSize(FT_Face _face, int _fontSize);
    void updateBitmapDimensions();

  private:
    FT_Library ft_;
    FT_Face face_;
    int fontSize_ = 0;

    int bitmapWidth_ = 0;
    int bitmapHeight_ = 0;
    int maxAdvance_;

    std::string filePath_;
    std::size_t hashCode_;
};

using FontRef = std::reference_wrapper<Font>;
using FontFallbackList = std::vector<FontRef>;
using FontList = std::pair<FontRef, FontFallbackList>;

} // end namespace

namespace std {
    template<>
    struct hash<crispy::text::Font> {
        std::size_t operator()(crispy::text::Font const& _font) const noexcept
        {
            return _font.hashCode();
        }
    };

    inline ostream& operator<<(ostream& _os, crispy::text::GlyphPosition const& _gpos)
    {
        _os << '{'
            << "x:" << _gpos.x
            << " y:" << _gpos.y
            << " i:" << _gpos.glyphIndex
            << " c:" << _gpos.cluster
            << '}';
        return _os;
    }

    inline ostream& operator<<(ostream& _os, crispy::text::GlyphPositionList const& _list)
    {
        int i = 0;
        for (auto const& gp : _list)
        {
            _os << (i ? " " : "") << gp;
            i++;
        }
        return _os;
    }
}
