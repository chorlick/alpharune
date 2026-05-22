#include "training/replay_buffer.h"

namespace riftbound::training {

ReplayBuffer::ReplayBuffer(size_t capacity) : capacity_(capacity) {
    buf_.reserve(capacity);
}

void ReplayBuffer::push(ReplaySample sample) {
    std::lock_guard<std::mutex> lk(mu_);
    if (buf_.size() < capacity_) {
        buf_.push_back(std::move(sample));
    } else {
        buf_[next_idx_] = std::move(sample);
        next_idx_ = (next_idx_ + 1) % capacity_;
    }
    ++total_pushes_;
}

std::vector<ReplaySample> ReplayBuffer::sample(size_t k,
                                                 std::mt19937& rng) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<ReplaySample> out;
    if (buf_.empty()) return out;
    out.reserve(k);
    std::uniform_int_distribution<size_t> d(0, buf_.size() - 1);
    for (size_t i = 0; i < k; ++i) {
        out.push_back(buf_[d(rng)]);
    }
    return out;
}

size_t ReplayBuffer::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return buf_.size();
}

int64_t ReplayBuffer::total_pushes() const {
    std::lock_guard<std::mutex> lk(mu_);
    return total_pushes_;
}

} // namespace riftbound::training
