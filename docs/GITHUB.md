# GitHub publication

The source repository is prepared locally. Creating a GitHub repository, pushing, tagging and publishing releases are separate actions.

Use repository name **dLab.Series**, default branch **main**, and this Description:

> dLab plugins are independent emulations of dbx hardware, based on their operating principles and publicly available information.

The exact description is also stored in `.github/repository-description.txt`. That file does not automatically change GitHub metadata.

After creating an empty repository in the intended account, add its URL as `origin` and push `main`. Keep `deps/JUCE` as a Git submodule: do not replace it with an uploaded dependency folder. Enable GitHub Actions; the workflow builds/tests Windows x64 and macOS Universal and uploads the EXE and PKG installers directly, without ZIP wrapping. The Windows job uses NSIS already installed in the windows-2022 image, with its version checked by the packaging script. Review that first run before creating the v1.0 release.

For v1.0, commit the workflow changes before creating the tag:

```powershell
git add .
git commit -m "Add automatic tagged releases"
git push origin main
git tag v1.0
git push origin v1.0
```

Pushing `v1.0` triggers both builds and tests. Only after both platforms succeed, a release job creates **v1.0 — Initial Realese**, takes the exact notes from CHANGELOG, and attaches the Windows EXE, macOS PKG and SHA256SUMS.txt. Only tag pushes publish releases; branch pushes, pull requests and manual runs build artifacts without publishing. Future `vX.Y`/`vX.Y.Z` tags require a matching changelog section. Existing releases are not silently overwritten. Do not move a published tag to a different commit; fix a failed build before publishing a new version.

GitHub's Download ZIP does not include submodule contents. Document `git clone --recurse-submodules` and `git submodule update --init --recursive`, as in README.

MIT applies to original project code. Preserve all dependency notices with source and installers. Binary distribution must satisfy the JUCE and VeSTige licenses described in NOTICE. Unsigned macOS PKGs are development artifacts; Developer ID signing/notarization remain separate release steps.
