# Astral Sigil

Stormheart and Cinderfang staves for Skyrim Special Edition, with animated casting circles, crystal projectiles and elemental impacts.

Install the FOMOD ZIP through your mod manager. Choose Lite, Full for SE 1.5.97, or the Full NG11 test build for SE/AE. Textures are available in 2K and 4K; quest objectives are optional.

Full requires matching SKSE, Address Library and Open Animation Replacer. NG11 targets selected Steam/GOG runtimes through 1.7.104; cross-version gameplay testing is pending. LE and VR are excluded.

To find the staves, read **Strange Lights** at the Sleeping Giant Inn in Riverwood, then search Brittleshin Pass. Bring the crafting plans and materials to a forge.

## Build

Windows x64, Visual Studio 2022 C++ tools, CMake 3.24+ and Git are required. CMake retrieves pinned source dependencies.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 6
ctest --test-dir build -C Release --output-on-failure
```

The native output is `build/Release/AstralSigilPortal.dll`. Install it over the Full edition. Third-party licenses and corresponding library source accompany the release.
