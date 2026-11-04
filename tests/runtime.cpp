#include "RuntimePolicy.h"
#include <iostream>

int main() {
    using namespace Portal::Runtime;
    for (auto v : Candidates) if (!Accepts(v)) return 1;
    
    for (Version v : {Version{1,7,105,0}, Version{1,6,1170,2},
                      Version{1,4,15,0}, Version{1,9,32,0},
                      Version{1,6,659,0}, Version{0,0,0,0}})
        if (Accepts(v)) return 2;
    if (!Accepts({1,6,1170,1}) || !Accepts({1,6,1179,1})) return 3;
    std::cout << "PASS runtime candidates, GOG discriminator, future/VR/LE rejection\n";
}
