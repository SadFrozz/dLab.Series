#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <complex>
namespace channel376 {
template<int N, class T=double> using Vector376=std::array<T,N>;
template<int N, class T=double> using Matrix376=std::array<Vector376<N,T>,N>;
template<int N, class T> bool solve376(Matrix376<N,T> a,Vector376<N,T> b,Vector376<N,T>& x)noexcept {
    for(int k=0;k<N;++k){
        int p=k;for(int j=k+1;j<N;++j)if(std::abs(a[j][k])>std::abs(a[p][k]))p=j;
        if(std::abs(a[p][k])<1e-28)return false;
        std::swap(a[k],a[p]);std::swap(b[k],b[p]);
        for(int j=k+1;j<N;++j){const T m=a[j][k]/a[k][k];
            for(int l=k+1;l<N;++l)a[j][l]-=m*a[k][l];b[j]-=m*b[k];}
    }
    for(int k=N-1;k>=0;--k){T v=b[k];for(int l=k+1;l<N;++l)v-=a[k][l]*x[l];x[k]=v/a[k][k];}
    return true;
}
// Trapezoidal companion network; ground is node -1. Resistor changes preserve
// capacitor voltage/current. A fixed inverse is cached between pot changes.
template<int N,int Capacity=16> class LinearNetwork376 {
public:
    struct Capacitor {int a=-1,b=-1;double farads=0,voltage=0,current=0;};
    Matrix376<N> conductance{},inverse{};
    Vector376<N> nodes{};
    std::array<Capacitor,Capacity> capacitors{};
    int count=0;double fs=192000;bool valid=false;
    void reset(double rate)noexcept{*this={};fs=rate;}
    void resistor(int a,int b,double ohms)noexcept{stamp(conductance,a,b,1/ohms);}
    void capacitor(int a,int b,double farads)noexcept{if(count<Capacity)capacitors[count++]={a,b,farads,0,0};}
    static void stamp(Matrix376<N>& m,int a,int b,double g)noexcept{
        if(a>=0)m[a][a]+=g;if(b>=0)m[b][b]+=g;
        if(a>=0&&b>=0){m[a][b]-=g;m[b][a]-=g;}
    }
    bool compile()noexcept{
        auto a=conductance;for(int i=0;i<count;++i)stamp(a,capacitors[i].a,capacitors[i].b,2*fs*capacitors[i].farads);
        valid=true;for(int i=0;i<N;++i){Vector376<N> b{},x{};b[i]=1;
            if(!solve376<N>(a,b,x)){valid=false;return false;}for(int j=0;j<N;++j)inverse[j][i]=x[j];}
        return true;
    }
    Vector376<N> tick(Vector376<N> source)noexcept{
        if(!valid)return nodes;
        for(int i=0;i<count;++i){const auto& c=capacitors[i];const double h=2*fs*c.farads*c.voltage+c.current;
            if(c.a>=0)source[c.a]+=h;if(c.b>=0)source[c.b]-=h;}
        Vector376<N> next{};for(int i=0;i<N;++i)for(int j=0;j<N;++j)next[i]+=inverse[i][j]*source[j];
        for(double v:next)if(!std::isfinite(v))return nodes;
        for(int i=0;i<count;++i){auto& c=capacitors[i];const double v=(c.a>=0?next[c.a]:0)-(c.b>=0?next[c.b]:0);
            c.current=2*fs*c.farads*(v-c.voltage)-c.current;c.voltage=v;}
        nodes=next;return nodes;
    }
};
}
