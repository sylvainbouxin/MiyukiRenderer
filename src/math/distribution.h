//
// Created by Shiina Miyuki on 2019/1/22.
//

#ifndef MIYUKI_DISTRIBUTION_H
#define MIYUKI_DISTRIBUTION_H

#include "../utils/util.h"

namespace Miyuki {
    // an arbitrary 1d distribution
    // discrete implementation of inverse transform sampling
    class Distribution1D {
        std::vector<Float> cdfArray;

        // solves cdf^(-1)
        int binarySearch(Float x) const;

    public:
        Float funcInt;
        Distribution1D(const Float *data, uint32_t N);

        int sampleInt(Float x) const;

        Float sampleFloat(Float x) const;

        Float cdf(Float x) const;
    };
}
#endif //MIYUKI_DISTRIBUTION_H
