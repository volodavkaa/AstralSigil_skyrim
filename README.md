# Astral Sigil native modules

C++ source for AstralSigilPortal.dll and AstralSigilSettings.dll, shipped with Astral Sigil 2.4.5. Requires Windows x64, Visual Studio 2022 C++ tools, a Windows SDK, Git and CMake 3.24+. CMake fetches pinned source dependencies; bundled corresponding-source archives also build offline.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 6
ctest --test-dir build -C Release --output-on-failure
```

Outputs are in `build/Release`. The default builds the SE/AE portal and settings modules. For the separate SE 1.5.97 portal only, configure another directory with `-DASTRAL_LEGACY_SE1597=ON`. Install the matching portal DLL; settings are shared by both Full selections. Supported runtimes are listed in runtime-policy.json; gameplay on every listed runtime is unverified.

Third-person crosshair alignment in the OmenRim camera setup remains under investigation. This repository contains native source and tests; mod assets and the FOMOD are distributed separately. License terms are in LICENSE, EXCEPTIONS.md and THIRD-PARTY-NOTICES.txt.
