#pragma once
#include <ctime>
#include <string>
#include <vector>

/// The humans you made, once their run is over: dead, or left behind when
/// you went back to the menu. Kept in data/obituary.txt, one record after
/// another, so the list grows over every launch.
namespace obituary {

struct Record {
    std::string name;
    /// World seconds from entering the Mall to the end.
    float survived = 0.0f;
    /// The last line of their log, or LEFT BEHIND.
    std::string cause;
    /// When the run ended, on the wall clock.
    std::time_t when = 0;
    struct Line {
        /// World seconds since entering.
        float time;
        std::string what;
    };
    /// Their log as they read it, oldest first.
    std::vector<Line> log;
};

/// Every record, oldest first; none if the file is missing.
std::vector<Record> load();
/// Adds r at the end of the file.
void add(const Record& r);
/// Writes the file afresh with just these, oldest first.
void save(const std::vector<Record>& records);

} // namespace obituary
