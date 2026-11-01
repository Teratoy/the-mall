#pragma once
#include <string>
#include <vector>

/// The knobs that shape what is grown, read from data/worldgen.txt: room
/// sizes, how often a plaza has a fountain, how likely each kind of chunk
/// is. Each file that grows something names its knobs at its top, as it
/// did its constants, and reads them through these handles. The file is
/// read the first time a knob is, and F5 reads it again, so chunks grown
/// from then on follow the new values; those already grown stay.
namespace tune {

/// One line of the file: a knob in its section, its value (or values, for
/// a list), the range the editor keeps it in, and the comment above it.
struct Knob {
    std::string section, name;
    std::vector<float> values;
    float lo = 0, hi = 0;
    std::string comment;
};

/// Every knob, in the file's order.
const std::vector<Knob>& all();
/// Reads the file again. False if it could not be found.
bool reload();

namespace detail {
/// The knob's place in all(), looked up by name the first time. A knob the
/// file lacks, or with the wrong count of values, stops the game: it can't
/// grow a world without it.
int find(const char* section, const char* name, int count, bool whole);
} // namespace detail

/// A whole number.
class Int {
public:
    constexpr Int(const char* section, const char* name) : section_(section), name_(name) {}
    operator int() {
        if (at_ < 0) at_ = detail::find(section_, name_, 1, true);
        return int(all()[size_t(at_)].values[0]);
    }

private:
    const char *section_, *name_;
    int at_ = -1;
};

/// A fraction, a chance or a size in cells that need not be whole.
class Float {
public:
    constexpr Float(const char* section, const char* name) : section_(section), name_(name) {}
    operator float() {
        if (at_ < 0) at_ = detail::find(section_, name_, 1, false);
        return all()[size_t(at_)].values[0];
    }

private:
    const char *section_, *name_;
    int at_ = -1;
};

/// A list of weights, one for each choice, in the order the code names
/// them; or a few whole numbers that go together, such as a size's range.
class List {
public:
    constexpr List(const char* section, const char* name, int count, bool whole = false)
        : section_(section), name_(name), count_(count), whole_(whole) {}
    operator const std::vector<float>&() { return values(); }
    const std::vector<float>& values() {
        if (at_ < 0) at_ = detail::find(section_, name_, count_, whole_);
        return all()[size_t(at_)].values;
    }
    int operator[](int i) { return int(values()[size_t(i)]); }

private:
    const char *section_, *name_;
    int count_;
    bool whole_;
    int at_ = -1;
};

} // namespace tune
