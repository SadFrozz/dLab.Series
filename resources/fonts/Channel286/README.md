# Embedded panel fonts

The typefaces below were selected by visual comparison with the user's original dbx 286 Project 1 photographs. They are visual substitutes, not an authenticated identification of the type used on the manufactured panel.

| File | GUI role |
|---|---|
| BarlowCondensed-Bold.ttf | Control headings and small captions |
| BarlowCondensed-ExtraBold.ttf | Input / Compressor / De-esser / Enhancer / Expander-Gate / Output section names |
| BarlowCondensed-SemiBold.ttf | Rotary scales and LED meter numerals |
| Barlow-LightItalic.ttf | Project 1 and 286 wordmarks |
| Barlow-Regular.ttf | Editable parameter values and digital readouts |

Author/source: [The Barlow Project](https://github.com/jpt/barlow). These static TTF files were downloaded on 2026-10-05 from the [Google Fonts Barlow Condensed directory](https://github.com/google/fonts/tree/main/ofl/barlowcondensed) and [Barlow directory](https://github.com/google/fonts/tree/main/ofl/barlow). Copyright 2017 The Barlow Project Authors. The complete SIL Open Font License 1.1 is preserved in `OFL.txt` and must accompany redistribution.

CMake embeds the unmodified font files into Channel286Fonts. The editor constructs JUCE typefaces directly from the embedded bytes, avoiding installed-font lookup on Windows and macOS. No fonts are installed into the operating system.
