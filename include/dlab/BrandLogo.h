#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <dLabBranding.h>
namespace dlab {
inline std::unique_ptr<juce::Drawable> makeLogo() {
    return juce::Drawable::createFromImageData(dLabBranding::dLab_by_Frozz_vector_svg,
                                              dLabBranding::dLab_by_Frozz_vector_svgSize);
}
inline void drawLogo(juce::Graphics& g, juce::Drawable* logo, juce::Rectangle<float> bounds) {
    if (logo != nullptr) logo->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
}
}
