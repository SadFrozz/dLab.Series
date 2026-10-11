#include "ReducedLinear376.h"
#include "OutputCircuit376.h"
#include <chrono>
#include <iostream>
#include <vector>
using namespace channel376;
using Net=OpampNetwork376<8,2>;
void prepareOutput(Net& net,double fs){net.reset(fs);
    net.resistor(0,-1,10000);net.resistor(0,6,6040);net.resistor(1,-1,2670);net.resistor(1,4,4530);net.resistor(1,5,3160);
    net.resistor(2,-1,10000);net.resistor(2,7,5760);net.resistor(3,-1,3160);net.resistor(3,4,3240);net.resistor(3,5,4530);
    net.resistor(6,4,60.4);net.resistor(7,5,60.4);net.capacitor(4,-1,470e-12);net.capacitor(5,-1,470e-12);
    net.amplifier(0,6,1,0,100000,10e6,13,8e6);net.amplifier(1,7,3,2,100000,10e6,13,8e6);net.compile();}
volatile double outputSum=0;
int main(){
    for(double fs:{32000.,192000.,384000.,1536000.}){
        Net net;prepareOutput(net,fs);ReducedLinear376<8,2> fast;fast.compile(net,(1u<<4)|(1u<<5));
        OutputCircuit376 reference;reference.prepare(fs);double error=0;unsigned fallbacks=0;
        for(int n=0;n<64000;++n){double input=n<16000?.3*std::sin(n*.131):n<32000?200*std::sin(n*.47):n<48000?.3*std::sin(n*.031):0;
            Vector376<8> b{},x{};b[0]=input/10000;b[3]=input/3160;
            if(!fast.tick(b,x)){fast.restore(net);x=net.tick(b);fast.sync(net);++fallbacks;}
            const double y=x[5]-x[4];error=std::max(error,std::abs(y-reference.tick(input)));}
        std::cout<<"fs="<<fs<<" max_error_V="<<error<<" fallbacks="<<fallbacks<<'\n';
        if(error>1e-8||net.failures||reference.failedSolves()||fallbacks==0)return 1;
    }
    std::vector<double> pcm(768000);for(int n=0;n<int(pcm.size());++n)pcm[n]=.3*std::sin(n*.131);
    OutputCircuit376 reference;reference.prepare(192000);double sum=0;
    auto start=std::chrono::steady_clock::now();for(double input:pcm)sum+=reference.tick(input);outputSum=sum;
    const double original=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    Net net;prepareOutput(net,192000);ReducedLinear376<8,2> fast;fast.compile(net,(1u<<4)|(1u<<5));sum=0;
    start=std::chrono::steady_clock::now();
    for(double input:pcm){Vector376<8> b{},x{};b[0]=input/10000;b[3]=input/3160;
        if(!fast.tick(b,x)){fast.restore(net);x=net.tick(b);fast.sync(net);}sum+=x[5]-x[4];}outputSum=sum;
    const double reduced=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"original_seconds="<<original<<" reduced_seconds="<<reduced<<" speedup="<<original/reduced<<'\n';
}
