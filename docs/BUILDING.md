# Building dLab.Series

Build from the repository root. The project produces plugins only; there is no standalone application. Building does not install plugins into your system folders.

## Requirements

- Git, CMake 3.22 or later, and Python 3.
- Windows: Visual Studio 2022 with the C++ workload and Windows SDK. To create the installer, install NSIS 3.10 or later.
- macOS: Xcode Command Line Tools and an SDK supporting Intel and Apple Silicon. The deployment target is macOS 11.0.

JUCE 8.0.12 is pinned as a Git submodule. Clone with its dependencies:

```sh
git clone --recurse-submodules https://github.com/SadFrozz/dLab.Series.git
cd dLab.Series
```

If you already cloned the repository, run:

```sh
git submodule update --init --recursive
```

GitHub's source ZIP does not include submodule contents. Use Git to obtain JUCE.

## Windows

Run from PowerShell:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --preset windows-x64
cmake --build build/windows-x64 --config Release --target dLab_Series_Package
./scripts/package-windows.ps1
```

The packaging script finds NSIS in PATH or its standard installation folder. For a portable copy, provide the compiler path:

```powershell
./scripts/package-windows.ps1 -NSIS 'C:/Tools/NSIS/makensis.exe'
```

Output: `build/windows-x64/dLab.Series-Windows-x64-Setup.exe`.

## macOS Universal

```sh
cmake --preset macos-universal
cmake --build --preset macos-universal
ctest --preset macos-universal
cmake --build build/macos-universal --target dLab_Series_Package
bash scripts/package-macos.sh
```

Output: `build/macos-universal/dLab.Series-macOS-Universal.pkg`. Plugin binaries include both `x86_64` and `arm64`. The installer is unsigned and not notarized; plugin bundles are ad-hoc signed.

## Plugin formats

The package directory is `build/<preset>/Package/dLab.Series`:

- VST3: one module exposing dLab 286 and dLab 376 as independent effects.
- AU on macOS: one component registering both effects.
- VST2: two separate plugins for OBS and other legacy hosts.

Set `DLAB_BUILD_VST2=OFF` at configuration time to omit VST2. The current macOS installer script requires all three formats.

## Repository layout

| Directory | Contents |
|---|---|
| `src/modules/Channel286/` | 286 processing, parameters, interface and tests |
| `src/modules/Channel376/` | 376 processing, parameters, interface and tests |
| `src/bundle/` | Shared plugin module, format wrappers and host tests |
| `include/dlab/` | Shared headers |
| `resources/` | Branding, fonts and installer resources |
| `deps/` | JUCE submodule and VST2 ABI headers |
| `cmake/` | Build configuration and generated wrapper support |
| `scripts/` | Installer packaging and release notes |
| `.github/workflows/` | Automated builds, tests and releases |

Original device documents, private recordings and local research files are excluded from the repository. They are not needed to build or test the plugins.

Tests cover DSP processing, plugin identities, separate states, state recall, rendering and editors. VST2 checks also cover host callbacks and oversized blocks. Automated checks do not replace testing in an actual DAW or OBS.

For release workflow details, see [GitHub publication](GITHUB.md). For dependency licenses, see [NOTICE](../NOTICE.md).
