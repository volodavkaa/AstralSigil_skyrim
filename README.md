# Astral Sigil native DLLs

This repository contains the C++ sources for the Astral Sigil SKSE plugins.

## Build for Skyrim SE/AE

Install Visual Studio 2022 with Desktop development with C++, a Windows SDK, Git, and CMake 3.24 or newer. CMake downloads dependencies at the revisions pinned in `CMakeLists.txt`.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 6
```

The build produces `AstralSigilPortal.dll` and `AstralSigilSettings.dll` in `build/Release`.

## Build the Skyrim SE 1.5.97 portal

```powershell
cmake -S . -B build-se1597 -G "Visual Studio 17 2022" -A x64 -DASTRAL_LEGACY_SE1597=ON
cmake --build build-se1597 --config Release --parallel 6
```

Use `build-se1597/Release/AstralSigilPortal.dll` with `AstralSigilSettings.dll` from the default build. The legacy option selects the 1.5.97 CommonLibSSE-NG build for the portal.

## License

See [LICENSE](LICENSE), [EXCEPTIONS.md](EXCEPTIONS.md), and [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt).
