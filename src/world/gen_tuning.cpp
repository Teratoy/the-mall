#include "world/gen_tuning.h"
#include "core/files.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace tune {
namespace {

std::vector<Knob>& knobs() {
    static std::vector<Knob> k;
    return k;
}

/// Each knob's place in knobs(), by "section.NAME".
std::unordered_map<std::string, int>& places() {
    static std::unordered_map<std::string, int> p;
    return p;
}

bool loaded = false;

/// Where the file may be: named by THEMALL_WORLDGEN, beside where the game
/// was started, or beside the game itself.
std::vector<std::filesystem::path> paths() {
    std::vector<std::filesystem::path> p;
    if (const char* env = std::getenv("THEMALL_WORLDGEN")) p.emplace_back(env);
    p.emplace_back("data/worldgen.txt");
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        p.push_back(exe.parent_path() / "../data/worldgen.txt");
        p.push_back(exe.parent_path() / "data/worldgen.txt");
    }
    return p;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

/// A number written plainly or as a fraction, such as 1/3, worked out in
/// floats as the code once did.
bool number(const std::string& s, float& out) {
    size_t slash = s.find('/');
    char* end = nullptr;
    if (slash != std::string::npos) {
        float a = 0, b = 0;
        if (!number(s.substr(0, slash), a) || !number(s.substr(slash + 1), b) || b == 0) return false;
        out = a / b;
        return true;
    }
    out = std::strtof(s.c_str(), &end);
    return !s.empty() && end == s.c_str() + s.size();
}

// A section header in brackets; then a knob a line: its name, its values,
// and its range written lo..hi. The # lines just above a knob are its
// comment. Anything it can't make out is reported and left out.
std::vector<Knob> read(std::istream& in, const std::string& file) {
    std::vector<Knob> out;
    std::string section, comment, row;
    for (int n = 1; std::getline(in, row); n++) {
        auto complain = [&](const std::string& what) {
            std::fprintf(stderr, "%s:%d: %s\n", file.c_str(), n, what.c_str());
        };
        row = trim(row);
        if (row.empty()) {
            comment.clear();
            continue;
        }
        if (row.front() == '#') {
            std::string text = trim(row.substr(1));
            comment += comment.empty() ? text : " " + text;
            continue;
        }
        if (size_t hash = row.find('#'); hash != std::string::npos) row = trim(row.erase(hash));
        if (row.front() == '[') {
            section = trim(row.substr(1, row.find(']') - 1));
            comment.clear();
            continue;
        }
        std::istringstream words(row);
        Knob k{section, "", {}, 0, 0, comment};
        comment.clear();
        words >> k.name;
        std::string word;
        bool ranged = false, good = true;
        while (words >> word) {
            float v = 0;
            if (size_t dots = word.find(".."); dots != std::string::npos) {
                ranged = number(word.substr(0, dots), k.lo) && number(word.substr(dots + 2), k.hi);
                good = ranged && !(words >> word);
                break;
            }
            if (!number(word, v)) {
                good = false;
                break;
            }
            k.values.push_back(v);
        }
        if (!good || !ranged || k.values.empty()) {
            complain("can't make out " + k.name + ": want NAME value... lo..hi");
            continue;
        }
        for (float v : k.values)
            if (v < k.lo || v > k.hi) complain(k.name + " is outside its range");
        out.push_back(std::move(k));
    }
    return out;
}

/// Puts what was read in the table. A knob already there keeps its place,
/// so the handles that found it still do.
void store(std::vector<Knob> read) {
    for (Knob& k : read) {
        std::string key = k.section + "." + k.name;
        auto it = places().find(key);
        if (it == places().end()) {
            places()[key] = int(knobs().size());
            knobs().push_back(std::move(k));
        } else {
            Knob& old = knobs()[size_t(it->second)];
            if (old.values.size() != k.values.size()) {
                std::fprintf(stderr, "worldgen %s now has %zu values, not %zu: kept as it was\n", key.c_str(),
                             k.values.size(), old.values.size());
                continue;
            }
            old = std::move(k);
        }
    }
}

} // namespace

const std::vector<Knob>& all() {
    if (!loaded) reload();
    return knobs();
}

bool reload() {
    loaded = true;
    if (auto in = files::asset("data/worldgen.txt")) {
        store(read(*in, "data/worldgen.txt"));
        return true;
    }
    for (const auto& path : paths()) {
        std::ifstream in(path);
        if (!in) continue;
        store(read(in, path.string()));
        return true;
    }
    std::fprintf(stderr, "no data/worldgen.txt found\n");
    return false;
}

namespace detail {

int find(const char* section, const char* name, int count, bool whole) {
    all();
    auto it = places().find(std::string(section) + "." + name);
    if (it == places().end()) {
        std::fprintf(stderr, "data/worldgen.txt has no %s in [%s]: can't grow the world without it\n", name, section);
        std::exit(1);
    }
    const Knob& k = knobs()[size_t(it->second)];
    bool fits = int(k.values.size()) == count;
    for (float v : k.values) fits = fits && (!whole || v == std::floor(v));
    if (!fits) {
        std::fprintf(stderr, "data/worldgen.txt: %s in [%s] wants %d %s\n", name, section, count,
                     whole ? "whole numbers" : "numbers");
        std::exit(1);
    }
    return it->second;
}

} // namespace detail

} // namespace tune
