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

#include <terminal_renderer/RenderTarget.h>
#include <terminal_renderer/GridMetrics.h>

#include <terminal_renderer/BackgroundRenderer.h>
#include <terminal_renderer/CursorRenderer.h>
#include <terminal_renderer/DecorationRenderer.h>
#include <terminal_renderer/ImageRenderer.h>
#include <terminal_renderer/TextRenderer.h>

#include <terminal/Terminal.h>

#include <fmt/format.h>

#include <chrono>
#include <memory>
#include <vector>
#include <utility>

namespace terminal::renderer {

/**
 * Renders a terminal's screen to the current OpenGL context.
 */
class Renderer {
  public:
    /** Constructs a Renderer instances.
     *
     * @p _fonts reference to the set of loaded fonts to be used for rendering text.
     * @p _colorProfile user-configurable color profile to use to map terminal colors to.
     * @p _projectionMatrix projection matrix to apply to the rendered scene when rendering the screen.
     */
    Renderer(Size const& _screenSize,
             int _logicalDpiX,
             int _logicalDpiY,
             FontDescriptions const& _fontDescriptions,
             ColorProfile _colorProfile,
             Opacity _backgroundOpacity,
             Decorator _hyperlinkNormal,
             Decorator _hyperlinkHover,
             std::unique_ptr<RenderTarget> _renderTarget);

    Size cellSize() const noexcept { return gridMetrics_.cellSize; }

    void setColorProfile(ColorProfile const& _colors);
    void setBackgroundOpacity(terminal::Opacity _opacity);
    void setRenderSize(int _width, int _height);
    bool setFontSize(text::font_size _fontSize);
    void updateFontMetrics();

    FontDescriptions const& fontDescriptions() const noexcept { return fontDescriptions_; }
    void setFonts(FontDescriptions _fontDescriptions);

    GridMetrics const& gridMetrics() const noexcept { return gridMetrics_; }

    void setHyperlinkDecoration(Decorator _normal, Decorator _hover)
    {
        decorationRenderer_.setHyperlinkDecoration(_normal, _hover);
    }

    void setScreenSize(Size const& _screenSize) noexcept
    {
        gridMetrics_.pageSize = _screenSize;
    }

    void setMargin(int _leftMargin, int _bottomMargin) noexcept
    {
        renderTarget_->setMargin(_leftMargin, _bottomMargin);
        gridMetrics_.pageMargin.left = _leftMargin;
        gridMetrics_.pageMargin.bottom = _bottomMargin;
    }

    /**
     * Renders the given @p _terminal to the current OpenGL context.
     *
     * @p _now The time hint to use when rendering the eventually blinking cursor.
     */
    uint64_t render(Terminal& _terminal,
                    std::chrono::steady_clock::time_point _now,
                    terminal::Coordinate const& _currentMousePosition,
                    bool _pressure);

    // Converts given RGBColor with its given opacity to a 4D-vector of values between 0.0 and 1.0
    static constexpr std::array<float, 4> canonicalColor(RGBColor const& _rgb, Opacity _opacity = Opacity::Opaque)
    {
        return std::array<float, 4>{
            static_cast<float>(_rgb.red) / 255.0f,
            static_cast<float>(_rgb.green) / 255.0f,
            static_cast<float>(_rgb.blue) / 255.0f,
            static_cast<float>(_opacity) / 255.0f
        };
    }

    void discardImage(Image const& _image);

    void clearCache();

    void dumpState(std::ostream& _textOutput) const;

  private:
    /// Invoked internally by render() function.
    uint64_t renderInternalNoFlush(Terminal& _terminal,
                                   std::chrono::steady_clock::time_point _now,
                                   terminal::Coordinate const& _currentMousePosition,
                                   bool _pressure);

    void renderCell(Coordinate const& _pos, Cell const& _cell, bool _reverseVideo, bool _selected);
    void renderCursor(Terminal const& _terminal);

    void executeImageDiscards();

    std::unique_ptr<text::shaper> textShaper_;

    FontDescriptions fontDescriptions_;
    FontKeys fonts_;

    GridMetrics gridMetrics_;

    ColorProfile colorProfile_;
    Opacity backgroundOpacity_;

    std::mutex imageDiscardLock_;               //!< Lock guard for accessing discardImageQueue_.
    std::vector<Image::Id> discardImageQueue_;  //!< List of images to be discarded.

    std::unique_ptr<RenderTarget> renderTarget_;

    BackgroundRenderer backgroundRenderer_;
    ImageRenderer imageRenderer_;
    TextRenderer textRenderer_;
    DecorationRenderer decorationRenderer_;
    CursorRenderer cursorRenderer_;
};

} // end namespace
