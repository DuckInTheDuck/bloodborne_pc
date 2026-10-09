// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cmath>
#include <cstdio>
#include "gpu/shim/frame_timing.h"
int main() {
    BbTiming::History<120> h;
    double time = 0; h.Mark(time);
    for (int i=0; i<120; ++i) { time += 1000.0/75.0; h.Mark(time); }
    auto stable = h.Get();
    assert(stable.count==120 && std::abs(stable.mean-1000.0/75.0)<0.001 && stable.deviation<0.001);
    // Same mean FPS, visibly uneven pacing. A smoothed FPS readout hides this.
    h.Reset(); time=0; h.Mark(time);
    for (int i=0; i<120; ++i) { time += i%2 ? 20 : 20.0/3.0; h.Mark(time); }
    const auto uneven=h.Get();
    assert(std::abs(uneven.mean-stable.mean)<0.001 && uneven.deviation>6 && uneven.p99==20);
    time+=150; h.Mark(time); assert(h.Get().maximum==150);
    // Minimize/restore resets the timeline, not a bogus multi-second frame.
    h.Reset(); h.Mark(10000); h.Mark(10013); assert(h.Get().count==1 && h.Get().maximum==13);
    std::puts("PASS: stable 75 Hz, equal-FPS judder, long-frame maximum, bounded history and resume reset");
}
