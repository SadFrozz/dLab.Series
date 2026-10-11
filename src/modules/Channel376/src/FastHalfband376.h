#pragma once
#include "HalfbandCoefficients.h"
#include <array>
namespace channel376 {
// Polyphase form of the same 129-tap linear-phase halfband. Even taps
// away from centre are structural sinc zeros (stored libm residues <1e-17).
// Odd taps retain the original coefficients; symmetric pairs are folded.
class Interpolate2x376 {
public:
    std::array<double,2> tick(double x)noexcept{
        memory[cursor]=memory[cursor+64]=x;const double* h=memory.data()+cursor+64;
        double odd=0;for(int k=0;k<32;++k)odd+=halfband376[2*k+1]*(h[-k]+h[-63+k]);
        const std::array<double,2> y{2*halfband376[64]*h[-32],2*odd};cursor=(cursor+1)&63;return y;
    }
private:std::array<double,128> memory{};int cursor=0;
};
class Decimate2x376 {
public:
    double even(double x)noexcept{
        evenMemory[cursor]=evenMemory[cursor+64]=x;const double* e=evenMemory.data()+cursor+64;
        const double* o=oddMemory.data()+cursor+63;double y=halfband376[64]*e[-32];
        for(int k=0;k<32;++k)y+=halfband376[2*k+1]*(o[-k]+o[-63+k]);return y;
    }
    void odd(double x)noexcept{oddMemory[cursor]=oddMemory[cursor+64]=x;cursor=(cursor+1)&63;}
private:std::array<double,128> evenMemory{},oddMemory{};int cursor=0;
};
class FastOversample4x376 {
public:
    void reset()noexcept{*this={};}
    template<class F>double process(double x,F&& f)noexcept{
        const auto low=up1.tick(x);double result=0;
        for(int a=0;a<2;++a){const auto high=up2.tick(low[a]);const double decimated=down2.even(f(high[0]));down2.odd(f(high[1]));
            if(a==0)result=down1.even(decimated);else down1.odd(decimated);}
        return result;
    }
private:Interpolate2x376 up1,up2;Decimate2x376 down2,down1;
};
}
