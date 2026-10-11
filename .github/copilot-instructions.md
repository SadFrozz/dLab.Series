# dLab.Series project guidance

- Build the suite from the root CMake project; device modules remain independent processors.
- Follow the `src`, `include`, `resources`, `deps`, `cmake`, `scripts` layout.
- JUCE is a pinned submodule. Generate wrapper adapters without modifying the checkout.
- VST3/AU expose two independent effects from one module; VST2 uses separate ordinary plugins.
- Preserve model codes, VST3 CIDs, parameter IDs/order and state tags.
- No standalone product and no automatic production installation during development.
- Original project code is MIT; third-party licenses remain separate.
