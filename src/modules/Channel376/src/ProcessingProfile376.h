#pragma once
#include <array>
#include <cstdint>
namespace channel376 {
#if defined(CHANNEL376_PROFILE_PROCESSING)
struct ProcessingProfile376 {
    std::array<std::uint64_t,16> predictions{},overloads{},maskTrials{};
    std::uint64_t tubeScalarIterations=0,tubeFullSolves=0,tubeFullIterations=0;
};
inline ProcessingProfile376 processingProfile376;
#endif
}
