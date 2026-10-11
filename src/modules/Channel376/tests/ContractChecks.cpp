#include "ModelIdentity.h"
#include "ParameterContract.h"
#include <iostream>

int main() {
    using namespace channel376;
    if(pluginCode=="C286" || bundleId=="com.frozz.channel286" || stateTag=="Channel286State") return 1;
    for(size_t i=0;i<continuous.size();++i) {
        const auto& p=continuous[i];
        if(p.minimum>=p.maximum || p.initial<p.minimum || p.initial>p.maximum) return 2;
        for(size_t j=0;j<i;++j) if(p.id==continuous[j].id) return 3;
    }
    for(size_t i=0;i<choices.size();++i) {
        const auto& p=choices[i];
        if(p.initial<0 || p.initial>=static_cast<int>(p.values.size())) return 4;
        for(const auto& continuousParameter:continuous) if(p.id==continuousParameter.id) return 5;
        for(size_t j=0;j<i;++j) if(p.id==choices[j].id) return 6;
    }
    std::cout<<"376 identity and parameter contract valid\n";
    return 0;
}
