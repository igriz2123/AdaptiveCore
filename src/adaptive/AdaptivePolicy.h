#pragma once

#include "IndexChoice.h"
#include "WorkloadAnalyzer.h"

class AdaptivePolicy {
public:
    IndexChoice choose(const WorkloadSnapshot& snapshot) const;
};
