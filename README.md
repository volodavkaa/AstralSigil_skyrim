# Astral Sigil 1.0.0 — source and release configuration

Source for AstralSigilPortal.dll, AstralSigilSettings.dll and the SkyUI MCM, plus the release installer and aimed-casting configuration.

## Native build

Requires Windows x64, Visual Studio 2022 C++ tools, a Windows SDK, Git and CMake 3.24+. CMake fetches pinned dependencies; corresponding-source archives also build offline.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 6
ctest --test-dir build -C Release --output-on-failure
```

Outputs are in `build/Release`. The default builds the SE/AE portal and settings modules. For the separate SE 1.5.97 portal, configure another directory with `-DASTRAL_LEGACY_SE1597=ON`. Install the matching portal DLL; settings are shared by both Full runtime selections. Exact runtime versions are listed in `runtime-policy.json`. Match SKSE64 and Address Library to the game version.

## MCM

`config/enhancements.json` defines the settings, visibility, labels and descriptions. Generate the Papyrus source without changing the plugin or native schema:

```powershell
python tools/build_mcm.py --unified --script-only
```

Compile `scripts/source/AS_MCM.psc` with the Papyrus compiler, vanilla/SKSE script sources, SkyUI SDK headers and TESV_Papyrus_Flags.flg. MCM v7 exposes 5 sliders, 8 toggles and a keyboard binding. Native setting slots and FormIDs remain stable across menu updates.

Full plugin/asset generation uses development inputs distributed separately; this checkout supports the script-only generator and standalone native build.

## Aimed casting and installer

Full uses five vanilla aimed right-hand staff clips. `config/aimed-casting-clips.json` records their source paths, destination paths and SHA-256 hashes. Obtain the clips from the installed Skyrim SE animation archive. The production OAR configurations are under `release/payload/12 Full Data/meshes/actors/character/animations/OpenAnimationReplacer/Astral Sigil - Aimed Casting`.

The scope is the player with right-hand Stormheart/Cinderfang, weapon drawn, standing/walking and no left-hand staff. First-person, sneak and dual-staff casting use the existing animation setup. Four-second charge, held readiness and release-to-fire retain their existing mechanics.

`release/fomod` contains the installer configuration and finished option cards. `release/INSTALL.txt` contains installation instructions. Game assets, compiled binaries and the complete FOMOD are distributed separately. Full requires SKSE64, Address Library, SkyUI and Open Animation Replacer. In Lite, craft both staves at a forge. The bundled vanilla Papyrus script synchronizes right-hand casting visuals in first and third person. Lite runs without SKSE, SkyUI or OAR.

The artwork tools under `tools` run in the full development workspace. The Blender 5.2 renderer needs PyNifly, the release staff meshes and the original Dragon Nest Staff PNG atlases passed with `--texture-source`. Run `python tools/prepare_nexus_staves.py --installer-only` with Pillow and the workspace font assets to lay out the rendered cards. The release packager puts the finished cards under `fomod/images` in the ZIP.

License terms are in LICENSE, EXCEPTIONS.md and THIRD-PARTY-NOTICES.txt.
