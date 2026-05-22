#include "ml/cfr_util.h"

#include <algorithm>
#include <cstring>

namespace riftbound::ml {

uint64_t computeInfoSetId(const std::vector<float>& features, int player) {
    // FNV-1a 64-bit. Seed: standard offset basis, mixed with (player+1)
    // so perspective P1 vs P2 produces distinct ids for identical feats.
    uint64_t h = 14695981039346656037ULL;
    h ^= static_cast<uint64_t>(player + 1);
    h *= 1099511628211ULL;
    for (float v : features) {
        // Bit-cast float→uint32 so identical floats hash identically.
        // memcpy is the standards-compliant type-pun.
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        for (int b = 0; b < 4; ++b) {
            h ^= static_cast<uint64_t>((bits >> (b * 8)) & 0xFF);
            h *= 1099511628211ULL;
        }
    }
    return h;
}

std::vector<double>
regretMatching(const std::vector<double>& cumulative_regrets) {
    std::vector<double> out;
    regretMatching(cumulative_regrets, out);
    return out;
}

void regretMatching(const std::vector<double>& cumulative_regrets,
                     std::vector<double>& out) {
    const size_t n = cumulative_regrets.size();
    out.assign(n, 0.0);
    if (n == 0) return;

    // Sum positive regrets. Negative entries don't contribute.
    double pos_sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double r = cumulative_regrets[i];
        if (r > 0.0) pos_sum += r;
    }

    if (pos_sum > 0.0) {
        // Normalize positive regrets into a probability distribution.
        for (size_t i = 0; i < n; ++i) {
            double r = cumulative_regrets[i];
            out[i] = (r > 0.0) ? (r / pos_sum) : 0.0;
        }
    } else {
        // All regrets <= 0 → no preference learned yet → uniform.
        // Matches the "no positive regret" branch of Zinkevich 2007 §3.
        const double uniform = 1.0 / static_cast<double>(n);
        std::fill(out.begin(), out.end(), uniform);
    }
}

} // namespace riftbound::ml
