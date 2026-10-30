#pragma once
#include <cstddef>
#include <deque>
#include <string>

/// What one human went through, as it saw it, oldest first: whom it met,
/// what it found, ate, gave and took, who hit it, what it was told and
/// told, whom it came to like or hate, and how it died. Others are kept by
/// id, so each reads under the name the human knows them by when the log
/// is read, or as someone if it was never told.
class LifeLog {
public:
    struct Entry {
        /// World seconds when it first happened.
        float time;
        /// In capitals; @ stands for who and # for whom.
        std::string what;
        int who, whom;
        /// The same thing again and again in a row, as blows in a fight.
        int times = 1;
    };
    /// Entries kept; the oldest go first.
    static constexpr size_t MAX = 300;

    void add(float time, std::string what, int who = -1, int whom = -1) {
        if (!entries_.empty()) {
            Entry& last = entries_.back();
            if (last.what == what && last.who == who && last.whom == whom) {
                last.times++;
                return;
            }
        }
        entries_.push_back({time, std::move(what), who, whom});
        if (entries_.size() > MAX) entries_.pop_front();
    }
    const std::deque<Entry>& entries() const { return entries_; }

private:
    std::deque<Entry> entries_;
};
