# Public VST2 interoperability ABI

`aeffectx.h` is the VeSTige header distributed with OBS Studio 31.0.3, retrieved from https://github.com/obsproject/obs-studio/blob/31.0.3/plugins/obs-vst/vst_header/aeffectx.h . Copyright Javier Serrano Polo, GPL-2.0-or-later. Original notices are preserved. This is not the proprietary Steinberg VST2 SDK.

The dLab bridge implements the plugin side of this public ABI for the ordinary stereo effects expected by OBS. It does not use JUCE's proprietary-SDK-dependent VST2 wrapper. Built formats retain the same DSP, editor and parameter state as the VST3/AU models.
