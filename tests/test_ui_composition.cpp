// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include "gpu/shadps4/video_core/renderer_vulkan/ui_composition.h"

int main() {
    assert(Vulkan::UiComposition::PillarboxMargin(2560,1080)==320);
    assert(Vulkan::UiComposition::PillarboxMargin(3440,1440)==440);
    assert(Vulkan::UiComposition::PillarboxMargin(1920,1080)==0);
    using namespace Vulkan::UiComposition;
    // Title/loading menus have no depth/camera. UI must still go to the native target.
    assert(Choose(true, true, false, true) == Background::Copy);
    assert(Choose(true, true, false, false) == Background::Copy);
    // In-game HUD: first reconstruct the scene, then draw the native UI over it.
    assert(Choose(true, true, true, true) == Background::Temporal);
    // Runtime FSR disable/failure must not change UI resolution.
    assert(Choose(true, true, true, false) == Background::Copy);
    assert(Choose(true, false, true, true) == Background::None);
    assert(Choose(false, true, true, true) == Background::None);
    assert(NativeViewport(1920, -1080));
    assert(!NativeViewport(960, -540));
    assert(MovieShader(0x34e8a281) && MovieShader(0x24042a9b));
    assert(!MovieShader(0x0b0acf50)); // fullscreen post/tonemap also has a native viewport
    assert(!MovieShader(0x6c62a79f)); // shadow geometry
    const auto display = Scale(960, 540, 1920, 1080, false);
    assert(display[0] == 2 && display[1] == 2);
    const auto ui = Scale(960, 540, 1920, 1080, true);
    assert(ui[0] == 1 && ui[1] == 1); // don't double an already native UI viewport
    const auto larger = Scale(960, 540, 3840, 2160, true);
    assert(larger[0] == 2 && larger[1] == 2);
    const auto ultrawide = Scale(960, 540, 2560, 1080, true);
    assert(ultrawide[0] == 1 && ultrawide[1] == 1);
    const auto ultrawide1440 = Scale(960, 540, 3440, 1440, true);
    assert(ultrawide1440[0] == ultrawide1440[1] && ultrawide1440[1] == 1440.0f / 1080.0f);
    const auto smaller = Scale(960, 540, 1280, 720, true);
    assert(smaller[0] == smaller[1] && smaller[1] == 720.0f / 1080.0f);
    std::puts("UI composition: PASS (menu without camera, HUD, FSR off, viewport scaling)");
}
