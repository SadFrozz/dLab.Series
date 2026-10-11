#pragma once
#include <array>
#include <string_view>
namespace channel376 {
// User-approved clean tonal characters; legacy tubeModel ID retained.
struct TubeChoice376 {std::string_view name,hint;};
inline constexpr std::array<TubeChoice376,3> tubeModels376{{
    {"Warm", "More body, softer highs"},
    {"Bright", "More presence, clearer highs"},
    {"Muted", "Softer top end, restrained presence"}
}};
}
