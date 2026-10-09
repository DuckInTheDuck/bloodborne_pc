# Changelog

## DuckInTheDuck Windows fork — 2026-10-09

- Correct gameplay aspect ratio for ultrawide output and preserve window/output consistency.
- Native mouse camera input for Bloodborne 1.09; independent sensitivity and aspect compensation.
- Live keyboard/mouse rebinding, including wheel bindings, and arrow-key menu navigation.
- Simplify player-facing launcher settings and persist control preferences.
- Improve frame pacing and window transitions; retain diagnostic tools for reported hangs.
- Provide portable packaging with first-run desktop resolution and relative game paths.
- Keep frozen script output alive after the launcher window closes.
- Bundle native DLL dependencies recursively, including OpenSSL and the Vulkan loader.
- Replace fork documentation and update links while retaining upstream attribution.

The maintainer confirmed successful portable launches on multiple PCs.
Original controller prompts remain; experimental prompt replacement is not enabled.
Newer upstream DLSS and online-play additions are not included in this branch.
