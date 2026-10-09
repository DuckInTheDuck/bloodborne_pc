// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace Vulkan::UiComposition {

constexpr uint32_t PillarboxMargin(uint32_t width, uint32_t height) {
    const auto content = uint64_t(height) * 16 / 9;
    return content < width ? uint32_t((width - content) / 2) : 0;
}

enum class Background { None, Copy, Temporal };

// UI composition must not require scene depth/camera or an active FSR context.
constexpr Background Choose(bool scaled, bool ui_draw, bool scene_ready, bool fsr_active) {
    if (!scaled || !ui_draw) {
        return Background::None;
    }
    return scene_ready && fsr_active ? Background::Temporal : Background::Copy;
}

inline bool NativeViewport(float width, float height) {
    return std::abs(std::abs(width) - 1920.0f) < 0.5f &&
           std::abs(std::abs(height) - 1080.0f) < 0.5f;
}

// Scaleform draws, including the first stencil/movie pass. A native-size viewport
// alone also matches every fullscreen post pass when guest targets stay at 1080p.
constexpr bool MovieShader(uint64_t hash) {
    return hash == 0x34e8a281 || hash == 0x81d336ce || hash == 0x09957251 ||
           hash == 0x24042a9b || hash == 0xa400228b;
}

inline std::array<float, 2> Scale(uint32_t guest_width, uint32_t guest_height,
                                uint32_t output_width, uint32_t output_height,
                                bool native_coordinates) {
    if (native_coordinates) {
        // Scaleform uses 1920x1080 coordinates. Fit by height without stretching
        // the UI horizontally on ultrawide output; preserve scaling at 720p/4K.
        const float scale = float(output_height) / 1080.0f;
        return {scale, scale};
    }
    return {float(output_width) / float(guest_width),
            float(output_height) / float(guest_height)};
}

} // namespace Vulkan::UiComposition
