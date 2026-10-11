# Contributing

Initialize submodules and build from the repository root using a supported CMake preset. Do not edit the pinned JUCE checkout: wrapper changes belong in `cmake/generate_wrappers.py`.

Keep device DSP/editor code in `src/modules`, format glue in `src/bundle`, shared headers in `include/dlab`, and artwork/fonts/installer resources in `resources`. Build outputs belong in `build` and must not be committed.

Preserve published model IDs, all four VST3 IDs, parameter IDs/order and saved-state compatibility. Changing one effect must not silently change the other effect. For DSP changes, describe the settings and signal used to verify the result, and run relevant suite checks. Keep plugin text in English; update both README language sections when changing user-facing behavior.

Do not add private recordings or third-party device documents to pull requests. Preserve dependency licenses and copyright notices. See NOTICE before changing or distributing dependencies.
