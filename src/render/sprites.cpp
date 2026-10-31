#include "render/sprites.h"
#include "core/files.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace {

/// What a sprite that can't be found is drawn in, so it is noticed.
constexpr Rgba MISSING = {255, 0, 255, 255};

struct Entry {
    Sprite sprite;
    /// The size the code asked for it at; 0 until it is asked for.
    int w = 0, h = 0;
};

std::map<std::string, Entry, std::less<>>& table() {
    static std::map<std::string, Entry, std::less<>> t;
    return t;
}

bool loaded = false;

/// Where the file may be: named by THEMALL_SPRITES, beside where the game
/// was started, or beside the game itself.
std::vector<std::filesystem::path> places() {
    std::vector<std::filesystem::path> p;
    if (const char* env = std::getenv("THEMALL_SPRITES")) p.emplace_back(env);
    p.emplace_back("data/sprites.txt");
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        p.push_back(exe.parent_path() / "../data/sprites.txt");
        p.push_back(exe.parent_path() / "data/sprites.txt");
    }
    return p;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

/// A colour written rrggbb or rrggbbaa, or false.
bool parseColour(const std::string& s, Rgba& out) {
    if (s.size() != 6 && s.size() != 8) return false;
    char* end = nullptr;
    unsigned long v = std::strtoul(s.c_str(), &end, 16);
    if (*end) return false;
    if (s.size() == 6) v = v << 8 | 0xff;
    out = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
    return true;
}

Sprite missing(int w, int h) {
    Sprite s;
    s.w = w, s.h = h;
    s.rows.assign(size_t(h), std::string(size_t(w), '?'));
    s.inks['?'] = MISSING;
    return s;
}

// Lines are a sprite's name (sprite NAME), its inks (ink LETTER COLOUR [what
// it is]), the letters that blink (blink A B) and its rows; comments start
// "# ", groups (group NAME) only sort the file for the editor. Anything it
// can't make out is reported and the sprite left out.
std::map<std::string, Sprite> read(std::istream& in, const std::string& file) {
    std::map<std::string, Sprite> out;
    std::string name;
    Sprite cur;
    bool bad = false;
    int lineNo = 0;
    auto finish = [&] {
        if (name.empty()) return;
        if (!bad && cur.rows.empty()) std::fprintf(stderr, "%s: sprite %s has no rows\n", file.c_str(), name.c_str());
        else if (!bad) {
            cur.w = int(cur.rows[0].size()), cur.h = int(cur.rows.size());
            out[name] = cur;
        }
        name.clear();
    };
    std::string raw;
    while (std::getline(in, raw)) {
        lineNo++;
        std::string line = trim(raw);
        if (line.empty() || line == "#" || line.rfind("# ", 0) == 0) continue;
        std::istringstream words(line);
        std::string word;
        words >> word;
        // One complaint a sprite: the rest of a bad one tends to follow from the first.
        auto complain = [&](const char* what) {
            if (!bad) std::fprintf(stderr, "%s:%d: %s: %s\n", file.c_str(), lineNo, what, line.c_str());
            bad = true;
        };
        // A row is one word; everything else is a keyword and what follows it.
        if (line.find(' ') == std::string::npos) {
            if (name.empty()) complain("outside any sprite");
            else if (!cur.rows.empty() && line.size() != cur.rows[0].size()) complain("row of another width");
            else cur.rows.push_back(line);
        } else if (word == "group") {
        } else if (word == "sprite") {
            finish();
            name = trim(line.substr(6));
            cur = {};
            bad = false;
        } else if (name.empty()) {
            complain("outside any sprite");
        } else if (word == "ink") {
            std::string letter, colour;
            words >> letter >> colour;
            Rgba c;
            if (letter.size() != 1 || letter[0] == '.' || uint8_t(letter[0]) > 127 || !parseColour(colour, c))
                complain("bad ink");
            else cur.inks[size_t(letter[0])] = c;
        } else if (word == "blink") {
            std::string a, b;
            words >> a >> b;
            if (a.size() != 1 || b.size() != 1) complain("bad blink");
            else cur.blinkA = a[0], cur.blinkB = b[0];
        } else {
            complain("can't make out");
        }
    }
    finish();
    return out;
}

/// Puts what was read in the table, keeping a sprite as it was where the
/// new one is not the size the code draws it at.
void store(std::map<std::string, Sprite> sprites) {
    for (auto& [name, s] : sprites) {
        Entry& e = table()[name];
        if (e.w && (s.w != e.w || s.h != e.h)) {
            std::fprintf(stderr, "sprite %s is %dx%d, not %dx%d: kept as it was\n", name.c_str(), s.w, s.h, e.w, e.h);
            continue;
        }
        e.sprite = std::move(s);
    }
}

} // namespace

namespace sprites {

bool reload() {
    loaded = true;
    if (auto in = files::asset("data/sprites.txt")) {
        store(read(*in, "data/sprites.txt"));
        return true;
    }
    for (const auto& path : places()) {
        std::ifstream in(path);
        if (!in) continue;
        store(read(in, path.string()));
        return true;
    }
    std::fprintf(stderr, "no data/sprites.txt found: sprites will be magenta boxes\n");
    return false;
}

const Sprite& get(std::string_view name, int w, int h) {
    if (!loaded) reload();
    auto it = table().find(name);
    if (it == table().end()) {
        std::fprintf(stderr, "no sprite %.*s in data/sprites.txt\n", int(name.size()), name.data());
        it = table().emplace(std::string(name), Entry{missing(w, h), w, h}).first;
    }
    Entry& e = it->second;
    if (!e.w) {
        e.w = w, e.h = h;
        if (e.sprite.w != w || e.sprite.h != h) {
            std::fprintf(stderr, "sprite %.*s is %dx%d, not %dx%d\n", int(name.size()), name.data(), e.sprite.w,
                         e.sprite.h, w, h);
            e.sprite = missing(w, h);
        }
    }
    return e.sprite;
}

} // namespace sprites
