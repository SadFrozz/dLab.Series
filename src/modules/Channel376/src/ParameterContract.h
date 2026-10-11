#pragma once
#include <array>
#include <string_view>

namespace channel376 {
struct Parameter {std::string_view id;double minimum,maximum,initial;};
inline constexpr std::array<Parameter,10> continuous{{
    {"drive",-15,15,0}, // Target gain of ONE pre-tube stage; never apply twice.
    {"eqLow",-15,15,0},{"eqMid",-15,15,0},{"eqMidFrequency",100,8000,1000},
    {"eqHigh",-15,15,0},{"threshold",-40,20,-10},
    {"ratio",0,1,0}, // Normalized: loaded VR5 law in ControlReference376.h.
    {"deFrequency",800,10000,4000},{"deAmount",0,10,0},{"output",-20,20,0}
}};
struct Switches {bool pad20=false,phase=false,lowCut=false,overEasy=false,slow=false;};
// DigitalModes is retained for historical DSP research. The product has PCM
// output only; dithering/noise shaping belong to the DAW's final export.
enum class DitherMode { none=0,tpdf=1,snr2=2 };
enum class ShapeMode { off=0,shape1=1,shape2=2 };
enum class OversamplingMode { off=0,x2=1,x4=2 };
struct ChoiceParameter {
    std::string_view id;
    std::array<std::string_view,3> values;
    int initial;
};
inline constexpr std::array<ChoiceParameter,2> choices{{
    {"oversampling",{{"Off","2x","4x"}},0},
    {"tubeModel",{{"Warm","Bright","Muted"}},0}
}};
struct DigitalModes {DitherMode dither=DitherMode::none;ShapeMode shape=ShapeMode::off;};
// +48V, AES/SPDIF and external word-clock are physical hardware functions,
// not fabricated sound-processing parameters.
}
