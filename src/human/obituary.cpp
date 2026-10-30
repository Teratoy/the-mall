#include "human/obituary.h"
#include "core/files.h"
#include <fstream>

namespace obituary {

namespace {

/// The save file: data/obituary.txt, or the app's own storage on a phone.
std::string path() { return files::savePath("obituary.txt"); }

// One field per line, a keyword, a tab and the value; a record starts at
// HUMAN and ends at END. Names and log lines never hold tabs or newlines.

/// Tabs and newlines become spaces, so a value stays on its line.
std::string flat(std::string s) {
    for (char& c : s)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return s;
}

} // namespace

std::vector<Record> load() {
    std::vector<Record> records;
    std::ifstream in(path());
    std::string line;
    Record r;
    bool open = false;
    while (std::getline(in, line)) {
        size_t tab = line.find('\t');
        std::string key = line.substr(0, tab), value = tab == std::string::npos ? "" : line.substr(tab + 1);
        try {
            if (key == "HUMAN") {
                r = Record{};
                r.name = value;
                open = true;
            } else if (!open) {
                continue;
            } else if (key == "SURVIVED") {
                r.survived = std::stof(value);
            } else if (key == "CAUSE") {
                r.cause = value;
            } else if (key == "WHEN") {
                r.when = std::time_t(std::stoll(value));
            } else if (key == "LOG") {
                size_t sep = value.find('\t');
                if (sep != std::string::npos) r.log.push_back({std::stof(value.substr(0, sep)), value.substr(sep + 1)});
            } else if (key == "END") {
                records.push_back(std::move(r));
                open = false;
            }
        } catch (const std::exception&) {
            // A damaged number loses that field, not the whole record.
        }
    }
    return records;
}

namespace {

void write(std::ofstream& out, const Record& r) {
    out << "HUMAN\t" << flat(r.name) << '\n';
    out << "SURVIVED\t" << r.survived << '\n';
    out << "CAUSE\t" << flat(r.cause) << '\n';
    out << "WHEN\t" << (long long)r.when << '\n';
    for (const Record::Line& l : r.log) out << "LOG\t" << l.time << '\t' << flat(l.what) << '\n';
    out << "END\n";
}

} // namespace

void add(const Record& r) {
    std::ofstream out(path(), std::ios::app);
    write(out, r);
}

void save(const std::vector<Record>& records) {
    std::ofstream out(path(), std::ios::trunc);
    for (const Record& r : records) write(out, r);
}

} // namespace obituary
