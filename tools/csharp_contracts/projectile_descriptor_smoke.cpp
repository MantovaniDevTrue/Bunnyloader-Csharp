#include "experiments/csharp/ProjectilePolicy.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <string>

int main() {
    using bl::csharp::parseProjectileTuning;
    using bl::csharp::ProjectilePolicy;
    for (size_t size : {16u, 17u, 18u, 19u, 21u}) {
        std::vector<std::string> fields(size, "-");
        fields[13] = "5";
        fields[14] = "3";
        fields[15] = "950";
        ProjectilePolicy policy;
        std::string error;
        assert(parseProjectileTuning(fields, &policy, &error));
        assert(policy.animationFrameTicks == 5);
        assert(policy.minionScanTicks == 3);
        assert(policy.minionTargetRange == 950.f);
    }
    for (size_t size : {15u, 20u, 22u}) {
        std::vector<std::string> fields(size, "-");
        ProjectilePolicy policy;
        std::string error;
        assert(!parseProjectileTuning(fields, &policy, &error));
        assert(error.find("InvalidDescriptor") != std::string::npos);
    }
    std::vector<std::string> fields(21,"-");
    fields[13]="5";fields[14]="3";fields[15]="Infinity";
    ProjectilePolicy policy;
    std::string error;
    assert(!parseProjectileTuning(fields,&policy,&error));
    assert(policy.animationFrameTicks==6&&policy.minionTargetRange==700.f);
    std::cout << "PROJECTILE_DESCRIPTOR_16_17_18_19_21_PASS\n";
}
