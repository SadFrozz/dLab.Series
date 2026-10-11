#pragma once
#include <array>
#include <cmath>
#include "PilotCircuit.h"

namespace c286 {
struct Parameters {
    float inputDb = 0, drive = 0, density = 5;
    float deFrequency = 5000, deThreshold = 0, lfDetail = 0, hfDetail = 0;
    float gateThresholdDb = -45, gateRatio = 1, outputDb = 0;
    bool highPass = false, processBypass = false, gateOff = false, modelS = false;
};
struct Meters {
    float inputPeak = 0, outputPeak = 0, inputRms = 0;
    float compressorReductionDb = 0, deEsserReductionDb = 0, gateReductionDb = 0;
};
struct Biquad {
    double b0=1, b1=0, b2=0, a1=0, a2=0, z1=0, z2=0;
    void highPass(double sr, double hz, double q);
    void bandPass(double sr, double hz, double q);
    void peak(double sr, double hz, double q, double db);
    void analogue(double sr, const std::array<double,3>& numerator,
                  const std::array<double,3>& denominator);
    float tick(float x) noexcept;
    void reset() noexcept { z1=z2=0; }
};
class Engine {
public:
    void prepare(double sampleRate, int channels);
    void reset() noexcept;
    void setParameters(const Parameters&) noexcept;
    // In-place planar mono/stereo, no heap allocation or locks in processing.
    Meters process(float* const* audio, int channels, int samples) noexcept;
    static float railLimit(float sample) noexcept;
private:
    struct Channel {
        Biquad inputHp, deHp, deCoupling, broadDetector, hfAudio, lf, lfCut, pilotInput, pilotDetector;
        float previousPilotInput=0;
    };
    struct Smooth {
        double value=0, target=0;
        float next(float c) noexcept {
            value=target+c*(value-target);
            if(std::abs(value-target)<1.e-9) value=target;
            return static_cast<float>(value);
        }
    };
    struct Profile {
        std::array<Channel,2> channels{};
        float dePower=0, broadPower=0, comparisonPower=0, gatePower=0;
        float compGr=0, deGr=0, gateGr=0;
        PilotCircuit pilot;
        double deVoltage=0;
        std::array<float,4097> deVoltageToDb{};
    };
    std::array<Profile,2> profiles{};
    std::array<Smooth,10> control{};
    Smooth hpMix{}, wetMix{}, modelMix{};
    Parameters params{};
    double sr=48000;
    int preparedChannels=2;
    float smoothCoeff=0, rmsCoeff=0;
    float previousDeHz=-1;
    bool controlsInitialised=false;
    float coefficient(float seconds) const noexcept;
};
}
