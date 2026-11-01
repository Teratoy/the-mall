#include "audio/voice_english.h"
#include "audio/synth.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace voice {
namespace english {
namespace {

#include "audio/voice_english_data.inc"

constexpr double kPi = 3.141592653589793;
// The Mall's additions: the shake's tremble (Hz, semitones at full shake, share of the loudness), and how
// strongly a stop is let go before a pause and before another consonant.
constexpr double kShakeHz = 9.0, kShakeSemis = 1.4, kShakeLoud = 0.35;
constexpr double kFinalBurst = 0.25, kClusterBurst = 0.6;

// ================================================================================ english.py

struct Phone {
    std::string p;
    int stress = 0;
    int index = -1;
};

struct Word {
    std::vector<Phone> phones;
    bool function = false;
    int start = 0;
    int end = 0;
    char brk = 0;  // 0 , . ? !
};

bool in_list(const char *const *list, int n, const std::string &s) {
    for (int i = 0; i < n; i++) {
        if (s == list[i]) {
            return true;
        }
    }
    return false;
}

bool is_vowel_phone(const std::string &p) { return in_list(kVowels, kVowels_n, p); }
bool alpha(char c) { return c >= 'a' && c <= 'z'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool vowel_letter(char c) { return c != 0 && std::strchr("aeiouy", c) != nullptr; }
bool cons_letter(char c) { return alpha(c) && !vowel_letter(c); }
bool voiced_letter(char c) { return c != 0 && std::strchr("bdvgjlmnrwz", c) != nullptr; }

bool starts_with(const std::string &s, const std::string &prefix, size_t at = 0) {
    return s.size() >= at + prefix.size() && s.compare(at, prefix.size(), prefix) == 0;
}

bool ends_with(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<std::string> split(const std::string &s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ') {
            i++;
        }
        size_t j = i;
        while (j < s.size() && s[j] != ' ') {
            j++;
        }
        if (j > i) {
            out.push_back(s.substr(i, j - i));
        }
        i = j;
    }
    return out;
}

char32_t lower32(char32_t c) {
    if ((c >= 65 && c <= 90) || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) {
        return c + 32;
    }
    if (c == 0x100 || c == 0x112 || c == 0x12A || c == 0x14C || c == 0x16A) {
        return c + 1;
    }
    return c;
}

// One byte per character: ASCII as is, folded letters / quotes / dashes, anything else 0x01.
std::string fold(const std::u32string &text) {
    std::string out(text.size(), '\x01');
    for (size_t i = 0; i < text.size(); i++) {
        const char32_t c = lower32(text[i]);
        bool done = false;
        for (int k = 0; k < kFold_n; k++) {
            if (kFoldFrom[k] == c) {
                out[i] = kFoldTo[k];
                done = true;
                break;
            }
        }
        if (!done && c < 128 && c != 0) {
            out[i] = char(c);
        }
    }
    return out;
}

const std::unordered_map<std::string, std::string> &table(const LexEntry *rows, int n) {
    static std::unordered_map<const LexEntry *, std::unordered_map<std::string, std::string>> cache;
    auto &m = cache[rows];
    if (m.empty()) {
        for (int i = 0; i < n; i++) {
            m[rows[i].word] = rows[i].phones;
        }
    }
    return m;
}

const std::unordered_map<std::string, std::string> &lexicon() { return table(kLexicon, kLexicon_n); }
const std::unordered_map<std::string, std::string> &specials() {
    return table(kSpecialContractions, kSpecialContractions_n);
}

// -- rule matching (word padded with spaces) --

bool match_left(const std::string &pattern, const std::string &word, int pos) {
    int i = pos - 1;
    for (int k = int(pattern.size()) - 1; k >= 0; k--) {
        const char sym = pattern[size_t(k)];
        auto at = [&](int j) { return j >= 0 ? word[size_t(j)] : '\0'; };
        if (sym == '#') {
            if (i < 0 || !vowel_letter(at(i))) {
                return false;
            }
            while (i >= 0 && vowel_letter(at(i))) {
                i--;
            }
        } else if (sym == ':') {
            while (i >= 0 && cons_letter(at(i))) {
                i--;
            }
        } else if (sym == '*') {
            if (i < 0 || !cons_letter(at(i))) {
                return false;
            }
            while (i >= 0 && cons_letter(at(i))) {
                i--;
            }
        } else if (sym == '^') {
            if (i < 0 || !cons_letter(at(i))) {
                return false;
            }
            i--;
        } else if (sym == '.') {
            if (i < 0 || !voiced_letter(at(i))) {
                return false;
            }
            i--;
        } else if (sym == '&') {
            if (i >= 1 && (word.compare(size_t(i - 1), 2, "ch") == 0 || word.compare(size_t(i - 1), 2, "sh") == 0)) {
                i -= 2;
            } else if (i >= 0 && std::strchr("scgzxj", at(i)) != nullptr && at(i) != 0) {
                i--;
            } else {
                return false;
            }
        } else if (sym == '@') {
            if (i >= 1 && (word.compare(size_t(i - 1), 2, "th") == 0 || word.compare(size_t(i - 1), 2, "ch") == 0 ||
                                  word.compare(size_t(i - 1), 2, "sh") == 0)) {
                i -= 2;
            } else if (i >= 0 && std::strchr("tsrdlznj", at(i)) != nullptr && at(i) != 0) {
                i--;
            } else {
                return false;
            }
        } else if (sym == '+') {
            if (i < 0 || std::strchr("eiy", at(i)) == nullptr || at(i) == 0) {
                return false;
            }
            i--;
        } else {
            if (i < 0 || at(i) != sym) {
                return false;
            }
            i--;
        }
    }
    return true;
}

bool match_right(const std::string &pattern, const std::string &word, int pos) {
    int i = pos;
    const int n = int(word.size());
    auto at = [&](int j) { return j < n ? word[size_t(j)] : '\0'; };
    for (size_t k = 0; k < pattern.size(); k++) {
        const char sym = pattern[k];
        if (sym == '#') {
            if (i >= n || !vowel_letter(at(i))) {
                return false;
            }
            while (i < n && vowel_letter(at(i))) {
                i++;
            }
        } else if (sym == ':') {
            while (i < n && cons_letter(at(i))) {
                i++;
            }
        } else if (sym == '*') {
            if (i >= n || !cons_letter(at(i))) {
                return false;
            }
            while (i < n && cons_letter(at(i))) {
                i++;
            }
        } else if (sym == '^') {
            if (i >= n || !cons_letter(at(i)) || at(i) == 'x') {
                return false;
            }
            i++;
        } else if (sym == '.') {
            if (i >= n || !voiced_letter(at(i))) {
                return false;
            }
            i++;
        } else if (sym == '$') {
            if (i + 1 >= n || !cons_letter(at(i)) || (at(i + 1) != 'e' && at(i + 1) != 'i')) {
                return false;
            }
            i++;
        } else if (sym == '%') {
            static const char *const suffixes[] = { "ing", "ely", "er", "es", "ed", "e" };
            bool found = false;
            for (const char *suf : suffixes) {
                const int len = int(std::strlen(suf));
                if (starts_with(word, suf, size_t(i)) && (i + len >= n || !alpha(at(i + len)))) {
                    i += len;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        } else if (sym == '+') {
            if (i >= n || std::strchr("eiy", at(i)) == nullptr || at(i) == 0) {
                return false;
            }
            i++;
        } else {
            if (i >= n || at(i) != sym) {
                return false;
            }
            i++;
        }
    }
    return true;
}

std::vector<std::pair<std::string, int>> rule_phonemes(const std::string &word) {
    const std::string padded = " " + word + " ";
    std::vector<std::pair<std::string, int>> out;
    int pos = 1;
    while (pos < int(padded.size()) - 1) {
        const char ch = padded[size_t(pos)];
        bool matched = false;
        for (int r = 0; r < kRules_n; r++) {
            const RuleEntry &rule = kRules[r];
            if (rule.letter != ch) {
                continue;
            }
            const std::string match = rule.match;
            if (!starts_with(padded, match, size_t(pos))) {
                continue;
            }
            if (!match_left(rule.left, padded, pos) || !match_right(rule.right, padded, pos + int(match.size()))) {
                continue;
            }
            const std::vector<std::string> phon = split(rule.phones);
            for (size_t k = 0; k < phon.size(); k++) {
                out.emplace_back(phon[k], pos - 1 + std::min(int(k), int(match.size()) - 1));
            }
            pos += int(match.size());
            matched = true;
            break;
        }
        if (!matched) {
            pos += 1;
        }
    }
    return out;
}

// -- stress --

void assign_stress(const std::string &word, std::vector<Phone> &phones) {
    std::vector<int> vow;
    for (size_t i = 0; i < phones.size(); i++) {
        if (is_vowel_phone(phones[i].p)) {
            vow.push_back(int(i));
        }
    }
    if (vow.empty()) {
        return;
    }
    int target = 0;
    if (vow.size() > 1) {
        bool last = false;
        for (int i = 0; i < kStressLast_n; i++) {
            if (ends_with(word, kStressLast[i])) {
                last = true;
            }
        }
        if (last) {
            target = int(vow.size()) - 1;
        } else {
            bool found = false;
            for (int s = 0; s < kStressBefore_n && !found; s++) {
                const std::string suf = kStressBefore[s];
                if (ends_with(word, suf) && word.size() > suf.size() + 1) {
                    const int suf_at = int(word.size() - suf.size());
                    int before = -1;
                    for (size_t k = 0; k < vow.size(); k++) {
                        if (phones[size_t(vow[k])].index < suf_at) {
                            before = int(k);
                        }
                    }
                    target = before >= 0 ? before : 0;
                    found = true;
                }
            }
            if (!found && vow.size() >= 2) {
                for (int p = 0; p < kUnstressedPrefixes_n; p++) {  // longest first
                    const std::string pre = kUnstressedPrefixes[p];
                    if (starts_with(word, pre)) {
                        if (word.size() - pre.size() >= 3) {
                            target = 1;
                        }
                        break;
                    }
                }
            }
        }
    }
    for (size_t k = 0; k < vow.size(); k++) {
        phones[size_t(vow[k])].stress = int(k) == target ? 1 : 0;
    }
    if (target >= 2) {
        phones[size_t(vow[0])].stress = 2;
    }
    for (size_t k = 0; k < vow.size(); k++) {
        Phone &ph = phones[size_t(vow[k])];
        if (ph.stress == 0 && in_list(kReducible, kReducible_n, ph.p) && k > 0) {
            ph.p = "AX";
        }
    }
    Phone &first = phones[size_t(vow[0])];
    if (target >= 1 && first.stress == 0) {
        if (first.p == "EH") {
            first.p = "IH";
        } else if (in_list(kReducible, kReducible_n, first.p)) {
            first.p = "AX";
        }
    }
}

void parse_part(const std::string &part, std::string &p, int &stress) {
    if (!part.empty() && digit(part.back())) {
        p = part.substr(0, part.size() - 1);
        stress = part.back() - '0';
    } else {
        p = part;
        stress = 0;
    }
}

std::vector<Phone> from_lexicon(const std::string &entry, int start, int length, bool &function) {
    function = starts_with(entry, "* ");
    const std::vector<std::string> parts = split(function ? entry.substr(2) : entry);
    std::vector<Phone> phones;
    const int count = std::max(1, int(parts.size()));
    for (size_t k = 0; k < parts.size(); k++) {
        Phone ph;
        parse_part(parts[k], ph.p, ph.stress);
        ph.index = start + std::min(length - 1, (int(k) * length) / count);
        phones.push_back(ph);
    }
    return phones;
}

void assimilate(std::vector<Phone> &phones) {
    for (size_t i = 0; i < phones.size(); i++) {
        Phone &ph = phones[i];
        if (ph.p == "Z*" || ph.p == "D*") {
            const std::string prev = i > 0 ? phones[i - 1].p : "";
            const bool voiceless = in_list(kVoiceless, kVoiceless_n, prev);
            ph.p = ph.p == "Z*" ? (voiceless ? "S" : "Z") : (voiceless ? "T" : "D");
        }
    }
}

std::vector<Phone> word_phones(const std::string &word, int start, bool &function) {
    function = false;
    auto sp = specials().find(word);
    if (sp != specials().end()) {
        std::vector<Phone> phones = from_lexicon(sp->second, start, int(word.size()), function);
        assimilate(phones);
        return phones;
    }
    auto lx = lexicon().find(word);
    if (lx != lexicon().end()) {
        return from_lexicon(lx->second, start, int(word.size()), function);
    }
    for (int c = 0; c < kContractions_n; c++) {
        const std::string ending = kContractions[c].word;
        if (ends_with(word, ending) && word.size() > ending.size()) {
            const std::string stem = word.substr(0, word.size() - ending.size());
            std::vector<Phone> phones = word_phones(stem, start, function);
            const std::vector<std::string> extra = split(kContractions[c].phones);
            for (size_t k = 0; k < extra.size(); k++) {
                Phone ph;
                parse_part(extra[k], ph.p, ph.stress);
                ph.index = start + int(stem.size()) + std::min(int(k), int(ending.size()) - 1);
                phones.push_back(ph);
            }
            assimilate(phones);
            return phones;
        }
    }
    std::string letters;
    for (char c : word) {
        if (c != '\'') {
            letters += c;
        }
    }
    const char *const plural_suffix[] = { "es", "s" };
    const char *const plural_extra[] = { "IH Z", "Z*" };
    for (int s = 0; s < 2; s++) {
        const std::string suffix = plural_suffix[s];
        if (!ends_with(letters, suffix)) {
            continue;
        }
        const std::string stem = letters.substr(0, letters.size() - suffix.size());
        auto it = lexicon().find(stem);
        if (it == lexicon().end() || starts_with(it->second, "*")) {
            continue;
        }
        std::vector<Phone> phones = from_lexicon(it->second, start, int(stem.size()), function);
        std::string extra = plural_extra[s];
        if (suffix == "es" && !phones.empty()) {
            const std::string &last = phones.back().p;
            if (last != "S" && last != "Z" && last != "SH" && last != "ZH" && last != "CH" && last != "JH") {
                extra = "Z*";
            }
        }
        const std::vector<std::string> ex = split(extra);
        for (size_t k = 0; k < ex.size(); k++) {
            Phone ph;
            ph.p = ex[k];
            ph.index = start + int(stem.size()) + std::min(int(k), int(suffix.size()) - 1);
            phones.push_back(ph);
        }
        assimilate(phones);
        return phones;
    }
    if (ends_with(letters, "ing")) {
        auto it = lexicon().find(letters.substr(0, letters.size() - 3));
        if (it != lexicon().end()) {
            std::vector<Phone> phones = from_lexicon(it->second, start, int(letters.size()) - 3, function);
            Phone ih, ng;
            ih.p = "IH";
            ih.index = start + int(letters.size()) - 3;
            ng.p = "NG";
            ng.index = start + int(letters.size()) - 2;
            phones.push_back(ih);
            phones.push_back(ng);
            return phones;
        }
    }
    const std::vector<std::pair<std::string, int>> raw = rule_phonemes(letters);
    std::vector<int> positions;
    for (size_t i = 0; i < word.size(); i++) {
        if (word[i] != '\'') {
            positions.push_back(int(i));
        }
    }
    std::vector<Phone> phones;
    for (const auto &r : raw) {
        Phone ph;
        ph.p = r.first;
        ph.index = start + positions[size_t(std::min(r.second, int(positions.size()) - 1))];
        phones.push_back(ph);
    }
    assimilate(phones);
    assign_stress(letters, phones);
    function = false;
    return phones;
}

void number_words(long long n, std::vector<std::string> &out) {
    if (n < 20) {
        out.push_back(kOnes[n]);
    } else if (n < 100) {
        out.push_back(kTens[n / 10]);
        if (n % 10) {
            number_words(n % 10, out);
        }
    } else if (n < 1000) {
        out.push_back(kOnes[n / 100]);
        out.push_back("hundred");
        if (n % 100) {
            number_words(n % 100, out);
        }
    } else if (n < 1000000) {
        number_words(n / 1000, out);
        out.push_back("thousand");
        if (n % 1000) {
            number_words(n % 1000, out);
        }
    } else {
        number_words(n / 1000000, out);
        out.push_back("million");
        if (n % 1000000) {
            number_words(n % 1000000, out);
        }
    }
}

int break_rank(char b) {
    switch (b) {
        case ',':
            return 1;
        case '.':
            return 2;
        case '!':
        case '?':
            return 3;
        default:
            return 0;
    }
}

std::vector<Word> phonemize(const std::string &s) {
    std::vector<Word> words;
    const int n = int(s.size());
    int i = 0;
    while (i < n) {
        const char c = s[size_t(i)];
        if (alpha(c)) {
            int j = i;
            while (j < n && (alpha(s[size_t(j)]) || (s[size_t(j)] == '\'' && j + 1 < n && alpha(s[size_t(j + 1)])))) {
                j++;
            }
            bool fn = false;
            std::vector<Phone> phones = word_phones(s.substr(size_t(i), size_t(j - i)), i, fn);
            if (!phones.empty()) {
                Word w;
                w.phones = phones;
                w.function = fn;
                w.start = i;
                w.end = j;
                words.push_back(w);
            }
            i = j;
        } else if (digit(c)) {
            int j = i;
            while (j < n && (digit(s[size_t(j)]) ||
                                    ((s[size_t(j)] == '.' || s[size_t(j)] == ',') && j + 1 < n && digit(s[size_t(j + 1)])))) {
                j++;
            }
            std::string token;
            for (int k = i; k < j; k++) {
                if (s[size_t(k)] != ',') {
                    token += s[size_t(k)];
                }
            }
            std::vector<std::string> parts;
            size_t a = 0;
            while (true) {
                const size_t dot = token.find('.', a);
                parts.push_back(token.substr(a, dot == std::string::npos ? std::string::npos : dot - a));
                if (dot == std::string::npos) {
                    break;
                }
                a = dot + 1;
            }
            std::string whole = parts[0];
            const size_t nz = whole.find_first_not_of('0');
            whole = nz == std::string::npos ? "0" : whole.substr(nz);
            std::vector<std::string> names;
            if (whole.size() <= 9) {
                number_words(std::stoll(whole), names);
            } else {
                for (char d : whole) {
                    names.push_back(kOnes[d - '0']);
                }
            }
            for (size_t p = 1; p < parts.size(); p++) {
                names.push_back("point");
                for (char d : parts[p]) {
                    names.push_back(kOnes[d - '0']);
                }
            }
            const int span = std::max(1, j - i);
            const int count = std::max(1, int(names.size()));
            for (size_t k = 0; k < names.size(); k++) {
                const int at = i + std::min(span - 1, (int(k) * span) / count);
                bool fn = false;
                std::vector<Phone> phones = word_phones(names[k], at, fn);
                for (Phone &ph : phones) {
                    ph.index = at;
                }
                Word w;
                w.phones = phones;
                w.start = at;
                w.end = at + 1;
                words.push_back(w);
            }
            i = j;
        } else {
            char brk = 0;
            for (int k = 0; k < kBreak_n; k++) {
                if (kBreakFrom[k] == c) {
                    brk = kBreakTo[k];
                }
            }
            if (brk && !words.empty() && break_rank(brk) >= break_rank(words.back().brk)) {
                words.back().brk = brk;
            }
            i++;
        }
    }
    for (size_t k = 0; k < words.size(); k++) {
        Word &w = words[k];
        if (s.compare(size_t(w.start), size_t(w.end - w.start), "the") == 0 && w.end - w.start == 3 &&
                k + 1 < words.size() && !words[k + 1].phones.empty() && is_vowel_phone(words[k + 1].phones[0].p) &&
                !w.brk) {
            w.phones.back().p = "IY";
        }
    }
    return words;
}

// ================================================================================ princess.py

struct Seg {
    std::string p;
    std::string kind;  // vowel, pause or a consonant kind
    double start = 0.0;
    double end = 0.0;
    int stress = 0;
    bool function = false;
    int index = -1;
    bool final = false;
    char brk = 0;
};

const VowelData &vowel_data(const std::string &p) {
    for (int i = 0; i < kVowelData_n; i++) {
        if (p == kVowelData[i].p) {
            return kVowelData[i];
        }
    }
    return kVowelData[0];
}

const VowelData &vowel_ax() { return vowel_data("AX"); }

const ConsData *cons_data(const std::string &p) {
    for (int i = 0; i < kConsData_n; i++) {
        if (p == kConsData[i].p) {
            return &kConsData[i];
        }
    }
    return nullptr;
}

const FricData &fric(const char *key) {
    for (int i = 0; i < kFric_n; i++) {
        if (std::strcmp(key, kFric[i].key) == 0) {
            return kFric[i];
        }
    }
    return kFric[0];
}

double break_pause(char b) {
    for (int i = 0; i < kBreakPause_n; i++) {
        if (kBreakPause[i].brk == b) {
            return kBreakPause[i].seconds;
        }
    }
    return 0.0;
}

struct Setup {
    EmotionData emo;
    double pitch, speed, size, clarity, expression, breath, vibrato, goblin, volume, seed;
};

Setup make_setup(const Params &p) {
    Setup s;
    s.emo = kEmotions[std::clamp(int(p.emotion), 0, kEmotions_n - 1)];
    s.pitch = std::clamp(p.pitch / kBaseHz, 0.4, 2.6);
    s.speed = std::clamp(p.speed, 0.35, 3.0);
    s.size = std::clamp(p.size, 0.7, 1.5);
    s.clarity = std::clamp(p.clarity, 0.0, 1.0);
    s.expression = std::clamp(p.expression, 0.0, 1.0);
    s.breath = std::clamp(p.breath, 0.0, 1.0);
    s.vibrato = std::clamp(p.vibrato, 0.0, 1.0);
    s.goblin = std::clamp(p.goblin, 0.0, 1.0);
    s.volume = std::clamp(p.volume, 0.0, 1.0);
    s.seed = double(p.seed % 10000);
    return s;
}

std::vector<Seg> timeline(const std::string &folded, const Setup &st) {
    const double speed = st.speed * st.emo.speed;
    const double clarity = st.clarity;
    const std::vector<Word> words = phonemize(folded);
    std::vector<Seg> segs;
    double t = 0.0;
    std::vector<bool> final_word(words.size(), false);
    for (size_t k = 0; k < words.size(); k++) {
        final_word[k] = words[k].brk != 0 || k == words.size() - 1;
    }
    for (size_t k = 0; k < words.size(); k++) {
        const Word &w = words[k];
        const std::vector<Phone> &phones = w.phones;
        for (size_t j = 0; j < phones.size(); j++) {
            const Phone &ph = phones[j];
            const bool is_vowel = is_vowel_phone(ph.p);
            double d;
            std::string kind;
            if (is_vowel) {
                d = vowel_data(ph.p).ms / 1000.0;
                if (ph.stress == 0) {
                    d *= 0.6 + 0.2 * clarity;
                } else if (ph.stress == 2) {
                    d *= 0.85;
                }
                const std::string nxt = j + 1 < phones.size() ? phones[j + 1].p : "";
                const ConsData *nc = cons_data(nxt);
                if (nxt == "P" || nxt == "T" || nxt == "K" || nxt == "CH") {
                    d *= 0.8;
                } else if (nc != nullptr && nc->voiced) {
                    d *= 1.1;
                }
                kind = "vowel";
            } else {
                const ConsData *c = cons_data(ph.p);
                kind = c->kind;
                d = c->ms / 1000.0 * (1.0 + 0.25 * clarity);
                const bool prev_c = j > 0 && !is_vowel_phone(phones[j - 1].p);
                const bool next_c = j + 1 < phones.size() && !is_vowel_phone(phones[j + 1].p);
                if (prev_c || next_c) {
                    d *= 0.78;
                }
            }
            if (w.function) {
                d *= 0.7;
            }
            if (final_word[k]) {
                int last_vowel = -1;
                for (size_t q = 0; q < phones.size(); q++) {
                    if (is_vowel_phone(phones[q].p)) {
                        last_vowel = int(q);
                    }
                }
                if (int(j) >= last_vowel) {
                    d *= 1.35;
                }
            }
            d = std::max(is_vowel ? 0.045 : 0.03, d / speed);
            Seg s;
            s.p = ph.p;
            s.kind = kind;
            s.start = t;
            s.end = t + d;
            s.stress = ph.stress;
            s.function = w.function;
            s.index = ph.index;
            s.final = final_word[k];
            s.brk = j == phones.size() - 1 ? w.brk : 0;
            segs.push_back(s);
            t += d;
        }
        double gap = 0.0;
        if (w.brk) {
            gap = break_pause(w.brk) / std::sqrt(speed);
        } else if (k + 1 < words.size()) {
            gap = clarity * 0.035 / speed * (w.function ? 0.5 : 1.0);
        }
        if (gap > 0.0) {
            Seg s;
            s.p = "_";
            s.kind = "pause";
            s.start = t;
            s.end = t + gap;
            s.index = w.end;
            s.brk = w.brk;
            segs.push_back(s);
            t += gap;
        }
    }
    return segs;
}

// -- keyframe tracks (numpy.interp semantics) --

using Points = std::vector<std::pair<double, double>>;

struct Track {
    std::vector<double> xs, ys;
    double def = 0.0;

    Track(Points pts, double default_value) :
            def(default_value) {
        std::stable_sort(pts.begin(), pts.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        for (size_t i = 0; i < pts.size(); i++) {
            xs.push_back(i == 0 ? pts[i].first : std::max(pts[i].first, xs.back() + 1e-7));
            ys.push_back(pts[i].second);
        }
    }

    double at(double x) const {
        const size_t n = xs.size();
        if (n == 0) {
            return def;
        }
        if (x > xs[n - 1]) {
            return ys[n - 1];
        }
        if (x < xs[0]) {
            return ys[0];
        }
        // largest j with xs[j] <= x
        size_t j = size_t(std::upper_bound(xs.begin(), xs.end(), x) - xs.begin()) - 1;
        if (j == n - 1 || xs[j] == x) {
            return ys[j];
        }
        const double slope = (ys[j + 1] - ys[j]) / (xs[j + 1] - xs[j]);
        return slope * (x - xs[j]) + ys[j];
    }
};

struct Tracks {
    Points f[3], av, ah, af, spec[6], nz, nasal, gate;
};

const double *vowel_at(const std::vector<Seg> &segs, int i, int step, double out[3]) {
    int j = i + step;
    while (j >= 0 && j < int(segs.size())) {
        const Seg &s = segs[size_t(j)];
        if (s.kind == "pause") {
            return nullptr;
        }
        if (s.kind == "vowel") {
            const VowelData &d = vowel_data(s.p);
            if (step > 0) {
                out[0] = d.f1a;
                out[1] = d.f2a;
                out[2] = d.f3a;
            } else {
                out[0] = d.f1b;
                out[1] = d.f2b;
                out[2] = d.f3b;
            }
            return out;
        }
        if (s.kind == "liquid" || s.kind == "glide") {
            const ConsData *c = cons_data(s.p);
            out[0] = c->l1;
            out[1] = c->l2;
            out[2] = c->l3;
            return out;
        }
        j += step;
    }
    return nullptr;
}

Tracks make_tracks(const std::vector<Seg> &segs, const Setup &st) {
    const double clarity = st.clarity;
    const double art = 0.75 + 0.5 * clarity;
    const double goblin = st.goblin;
    Tracks tr;
    auto put_spec = [&](double time, const std::string &key) {
        const FricData &f = fric(key.c_str());
        const double vals[6] = { f.cf1, f.bw1, f.g1, f.cf2, f.bw2, f.g2 };
        for (int k = 0; k < 6; k++) {
            tr.spec[k].emplace_back(time, vals[k]);
        }
    };
    for (int i = 0; i < int(segs.size()); i++) {
        const Seg &s = segs[size_t(i)];
        const double a = s.start, b = s.end, d = b - a;
        if (s.kind == "pause") {
            tr.av.emplace_back(a + std::min(0.012, d / 3.0), 0.0);
            tr.av.emplace_back(b - std::min(0.012, d / 3.0), 0.0);
            continue;
        }
        if (s.kind == "vowel") {
            const VowelData &data = vowel_data(s.p);
            const double fs[3] = { data.f1a, data.f2a, data.f3a };
            const double fe[3] = { data.f1b, data.f2b, data.f3b };
            const bool diph = fs[0] != fe[0] || fs[1] != fe[1] || fs[2] != fe[2];
            const double edge = std::min(0.045, d * 0.3);
            for (int k = 0; k < 3; k++) {
                if (diph) {
                    tr.f[k].emplace_back(a + d * 0.25, fs[k]);
                    tr.f[k].emplace_back(b - d * 0.2, fe[k]);
                } else {
                    tr.f[k].emplace_back(a + edge, fs[k]);
                    tr.f[k].emplace_back(b - edge, fe[k]);
                }
            }
            const double level = s.stress ? 1.0 : 0.85;
            tr.av.emplace_back(a + 0.008, level);
            tr.av.emplace_back(b - 0.008, level);
            std::vector<const Seg *> near;
            for (int j : { i - 1, i + 1 }) {
                if (j >= 0 && j < int(segs.size()) && segs[size_t(j)].kind == "nasal") {
                    near.push_back(&segs[size_t(j)]);
                }
            }
            const double amount = 0.25 * goblin + (near.empty() ? 0.0 : 0.2);
            tr.nasal.emplace_back(a + 0.02, amount);
            tr.nasal.emplace_back(b - 0.02, amount);
            if (!near.empty()) {
                tr.nz.emplace_back(a + 0.02, cons_data(near.front()->p)->zero);
                tr.nz.emplace_back(b - 0.02, cons_data(near.back()->p)->zero);
            }
            continue;
        }
        const ConsData &c = *cons_data(s.p);
        const std::string kind = c.kind;
        const bool voiced = c.voiced;
        double prev_buf[3], next_buf[3];
        const double *prev_v = vowel_at(segs, i, -1, prev_buf);
        const double *next_v = vowel_at(segs, i, 1, next_buf);
        const double locus[3] = { c.l1, c.l2, c.l3 };
        if (kind == "liquid" || kind == "glide") {
            for (int k = 0; k < 3; k++) {
                tr.f[k].emplace_back(a + d * 0.3, locus[k]);
                tr.f[k].emplace_back(b - d * 0.3, locus[k]);
            }
            const double lvl = kind == "liquid" ? 0.72 : 0.8;
            tr.av.emplace_back(a + 0.01, lvl);
            tr.av.emplace_back(b - 0.01, lvl);
            tr.nasal.emplace_back(a + 0.01, 0.25 * goblin);
            tr.nasal.emplace_back(b - 0.01, 0.25 * goblin);
            continue;
        }
        if (kind == "aspir") {
            const VowelData &ax = vowel_ax();
            const double ax3[3] = { ax.f1a, ax.f2a, ax.f3a };
            const double *target = next_v ? next_v : ax3;
            for (int k = 0; k < 3; k++) {
                tr.f[k].emplace_back(a + 0.005, target[k]);
            }
            tr.av.emplace_back(a + 0.01, 0.0);
            tr.av.emplace_back(b - 0.004, 0.0);
            tr.ah.emplace_back(a, 0.0);
            tr.ah.emplace_back(a + 0.015, 0.45 * art);
            tr.ah.emplace_back(b - 0.008, 0.4 * art);
            tr.ah.emplace_back(b + 0.01, 0.0);
            continue;
        }
        double loc[3] = { locus[0], locus[1], locus[2] };
        if (kind == "stop" && std::string(c.key) == "vel") {
            const VowelData &ax = vowel_ax();
            const double ax3[3] = { ax.f1a, ax.f2a, ax.f3a };
            const double *ref = next_v ? next_v : (prev_v ? prev_v : ax3);
            loc[1] = std::min(2600.0, std::max(1300.0, ref[1] * 1.08 + 150.0));
        }
        double left[3], right[3];
        for (int k = 0; k < 3; k++) {
            left[k] = prev_v ? loc[k] + kLocusK * (prev_v[k] - loc[k]) : loc[k];
            right[k] = next_v ? loc[k] + kLocusK * (next_v[k] - loc[k]) : loc[k];
            tr.f[k].emplace_back(a, left[k]);
            tr.f[k].emplace_back(b, right[k]);
        }
        if (kind == "nasal") {
            tr.av.emplace_back(a + 0.01, 0.3);
            tr.av.emplace_back(b - 0.01, 0.3);
            tr.nz.emplace_back(a + 0.008, c.zero);
            tr.nz.emplace_back(b - 0.008, c.zero);
            tr.nasal.emplace_back(a + 0.012, 1.0);
            tr.nasal.emplace_back(b - 0.012, 1.0);
            continue;
        }
        tr.nasal.emplace_back(a + 0.006, 0.0);
        tr.nasal.emplace_back(b - 0.006, 0.0);
        if (kind == "fric") {
            const double level = fric(c.key).level * (voiced ? 0.5 : 1.0) * art;
            put_spec(a, c.key);
            put_spec(b, c.key);
            tr.af.emplace_back(a, 0.0);
            tr.af.emplace_back(a + std::min(0.02, d / 3.0), level);
            tr.af.emplace_back(b - std::min(0.02, d / 3.0), level);
            tr.af.emplace_back(b, 0.0);
            const double vl = voiced ? 0.45 : 0.0;
            tr.av.emplace_back(a + 0.01, vl);
            tr.av.emplace_back(b - 0.01, vl);
            continue;
        }
        const double close = d * (kind == "affr" ? 0.55 : 1.0);
        const double rel = a + close;
        const double bar = voiced ? 0.14 : 0.0;
        tr.av.emplace_back(a + 0.006, bar);
        tr.av.emplace_back(rel - 0.002, bar);
        const std::string burst_key = kind == "affr" ? "alv" : c.key;
        put_spec(rel - 0.001, burst_key);
        put_spec(rel + 0.008, burst_key);
        // The Mall: a stop before a pause or the end is barely let go, as English mostly leaves it, and one
        // before another consonant softly; at full strength they rang out after the word like a spoken dot.
        const bool before_pause = i + 1 >= int(segs.size()) || segs[size_t(i + 1)].kind == "pause";
        const double let_go = next_v != nullptr ? 1.0 : before_pause ? kFinalBurst : kClusterBurst;
        const double burst = fric(burst_key.c_str()).level * (voiced ? 0.6 : 1.0) * art * let_go;
        tr.af.emplace_back(rel - 0.001, 0.0);
        tr.af.emplace_back(rel, burst);
        tr.af.emplace_back(rel + 0.012, burst * 0.3);
        tr.af.emplace_back(rel + 0.02, 0.0);
        if (kind == "affr") {
            const double lvl = fric("sh").level * (voiced ? 0.5 : 1.0) * art;
            put_spec(rel + 0.02, "sh");
            put_spec(b, "sh");
            tr.af.emplace_back(rel + 0.021, lvl * 0.8);
            tr.af.emplace_back(b - 0.01, lvl);
            tr.af.emplace_back(b, 0.0);
            tr.av.emplace_back(rel + 0.01, voiced ? 0.4 : 0.0);
            tr.av.emplace_back(b - 0.005, voiced ? 0.4 : 0.0);
        }
        if (!voiced && next_v != nullptr) {
            const double vot = std::min(0.055, std::max(0.025, 0.05 / std::max(0.5, st.speed)));
            const double end_rel = kind == "affr" ? b : rel;
            tr.ah.emplace_back(end_rel, 0.0);
            tr.ah.emplace_back(end_rel + 0.005, 0.35 * art);
            tr.ah.emplace_back(end_rel + vot, 0.0);
            tr.gate.emplace_back(end_rel - 0.003, 1.0);
            tr.gate.emplace_back(end_rel, 0.0);
            tr.gate.emplace_back(end_rel + vot * 0.8, 0.0);
            tr.gate.emplace_back(end_rel + vot + 0.012, 1.0);
        } else if (voiced) {
            tr.av.emplace_back(rel + 0.006, 0.3);
        }
    }
    return tr;
}

// -- noise and filters --

uint32_t text_hash(const std::u32string &text) {
    uint32_t h = 2166136261u;
    for (char32_t ch : text) {
        h = (h ^ uint32_t(ch)) * 16777619u;
    }
    return h;
}

std::vector<double> hash_noise(size_t n, uint32_t seed) {
    std::vector<double> out(n);
    for (size_t i = 0; i < n; i++) {
        uint32_t x = uint32_t(uint64_t(i) * 0x9E3779B9ull + uint64_t(seed));
        x ^= x >> 16;
        x *= 0x7FEB352Du;
        x ^= x >> 15;
        x *= 0x846CA68Bu;
        x ^= x >> 16;
        out[i] = double(x) / 2147483648.0 - 1.0;
    }
    return out;
}

// One-pole low-pass y = (1 - c) x + c y1, like scipy lfilter([1 - c], [1, -c], x, zi=[z0]).
void one_pole(std::vector<double> &x, double c, double z0) {
    double z = z0;
    const double b0 = 1.0 - c;
    for (double &v : x) {
        const double y = b0 * v + z;
        z = c * y;
        v = y;
    }
}

struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// Direct form II transposed, as scipy.signal.sosfilt.
struct BiquadState {
    double z1 = 0.0, z2 = 0.0;

    double process(const Biquad &q, double x) {
        const double y = q.b0 * x + z1;
        z1 = q.b1 * x - q.a1 * y + z2;
        z2 = q.b2 * x - q.a2 * y;
        return y;
    }
};

Biquad resonator(double f, double bw, double sr) {
    f = std::min(f, 0.45 * sr);
    const double r = std::exp(-kPi * bw / sr);
    const double c = -r * r;
    const double b = 2.0 * r * std::cos(2.0 * kPi * f / sr);
    const double a = 1.0 - b - c;
    Biquad q;
    q.b0 = a;
    q.a1 = -b;
    q.a2 = -c;
    return q;
}

Biquad peaking(double f, double gain_db, double qf, double sr) {
    f = std::min(f, 0.45 * sr);
    const double amp = std::pow(10.0, gain_db / 40.0);
    const double w0 = 2.0 * kPi * f / sr;
    const double alpha = std::sin(w0) / (2.0 * qf);
    const double a0 = 1.0 + alpha / amp;
    Biquad q;
    q.b0 = (1.0 + alpha * amp) / a0;
    q.b1 = -2.0 * std::cos(w0) / a0;
    q.b2 = (1.0 - alpha * amp) / a0;
    q.a1 = -2.0 * std::cos(w0) / a0;
    q.a2 = (1.0 - alpha / amp) / a0;
    return q;
}

Biquad bandpass(double f, double bw, double sr) {
    f = std::min(f, 0.45 * sr);
    const double w0 = 2.0 * kPi * f / sr;
    const double alpha = std::sin(w0) * bw / (2.0 * f);
    const double a0 = 1.0 + alpha;
    Biquad q;
    q.b0 = alpha / a0;
    q.b1 = 0.0 * 1.0;
    q.b2 = -alpha / a0;
    q.a1 = -2.0 * std::cos(w0) / a0;
    q.a2 = (1.0 - alpha) / a0;
    return q;
}

// -- pitch --

std::vector<double> pitch_track(const std::vector<Seg> &segs, size_t n, double sr, const Setup &st, uint32_t seed) {
    const double expr = st.expression * st.emo.range;
    const double base = kBaseHz * st.pitch * std::pow(2.0, st.emo.pitch / 12.0);
    Points pts, vib_pts;
    const double vib_depth = st.vibrato * st.emo.vibrato * 0.35;
    std::vector<std::pair<std::vector<const Seg *>, char>> phrases;
    std::vector<const Seg *> phrase;
    for (const Seg &s : segs) {
        if (s.kind == "pause") {
            if (s.brk && !phrase.empty()) {
                phrases.emplace_back(phrase, s.brk);
                phrase.clear();
            }
            continue;
        }
        phrase.push_back(&s);
    }
    if (!phrase.empty()) {
        phrases.emplace_back(phrase, '.');
    }
    for (const auto &pb : phrases) {
        const std::vector<const Seg *> &phr = pb.first;
        const char brk = pb.second;
        const double p0 = phr.front()->start, p1 = phr.back()->end;
        const double span = std::max(1e-3, p1 - p0);
        std::vector<const Seg *> vowels, accents;
        for (const Seg *s : phr) {
            if (s->kind == "vowel") {
                vowels.push_back(s);
                if (s->stress == 1 && !s->function) {
                    accents.push_back(s);
                }
            }
        }
        const Seg *nuclear = !accents.empty() ? accents.back() : (!vowels.empty() ? vowels.back() : nullptr);
        auto decl = [&](double time) { return (1.5 - 2.5 * (time - p0) / span) * expr; };
        bool after_nuclear = false;
        for (const Seg *s : vowels) {
            const double mid = s->start + 0.55 * (s->end - s->start);
            if (after_nuclear) {
            } else if (s == nuclear && brk == '?') {
                pts.emplace_back(s->start, decl(s->start) - 0.5 * expr);
                pts.emplace_back(s->end, decl(s->end) + 5.0 * expr);
            } else if (s == nuclear && (brk == '.' || brk == '!')) {
                const double peak = 3.5 * expr * (brk == '!' ? 1.35 : 1.0);
                pts.emplace_back(s->start, decl(s->start) + 0.6 * peak);
                pts.emplace_back(mid, decl(mid) + peak);
            } else if (s->stress == 1 && !s->function) {
                const double peak = 3.0 * expr;
                pts.emplace_back(s->start, decl(s->start) + 0.4 * peak);
                pts.emplace_back(mid, decl(mid) + peak);
                pts.emplace_back(s->end, decl(s->end) + 0.5 * peak);
            } else {
                const double drop = s->function ? 1.0 : 0.5;
                pts.emplace_back(mid, decl(mid) - drop * expr);
            }
            if (s == nuclear) {
                after_nuclear = true;
            }
            const double dur = s->end - s->start;
            if (dur > 0.16 && vib_depth > 0.0) {
                vib_pts.emplace_back(s->start + 0.07, 0.0);
                vib_pts.emplace_back(s->end, vib_depth * std::min(1.0, (dur - 0.1) / 0.15));
                vib_pts.emplace_back(s->end + 0.03, 0.0);
            }
        }
        if (brk == '.' || brk == '!') {
            pts.emplace_back(p1, decl(p1) - 2.5 * expr);
        } else if (brk == ',') {
            pts.emplace_back(p1, decl(p1) + 1.8 * expr);
        } else if (brk == '?') {
            pts.emplace_back(p1, decl(p1) + 6.0 * expr);
        }
    }
    const Track semis_track(pts, 0.0);
    std::vector<double> semis(n);
    for (size_t i = 0; i < n; i++) {
        semis[i] = semis_track.at(double(i) / sr);
    }
    const double c = std::exp(-1.0 / (0.025 * sr));
    one_pole(semis, c, semis[0] * c);
    one_pole(semis, c, semis[0] * c);
    std::vector<double> wander = hash_noise(n, seed ^ 0x51ED27u);
    const double cj = std::exp(-2.0 * kPi * 6.0 / sr);
    one_pole(wander, cj, 0.0);
    for (size_t i = 0; i < n; i++) {
        semis[i] += wander[i] * 6.0 * 0.1 * st.emo.jitter;
    }
    if (!vib_pts.empty()) {
        const Track vib(vib_pts, 0.0);
        const double phase0 = double(seed % 1000u) / 1000.0 * 2.0 * kPi;
        for (size_t i = 0; i < n; i++) {
            const double t = double(i) / sr;
            semis[i] += vib.at(t) * std::sin(2.0 * kPi * 5.4 * t + phase0);
        }
    }
    std::vector<double> f0(n);
    for (size_t i = 0; i < n; i++) {
        f0[i] = base * std::pow(2.0, semis[i] / 12.0);
    }
    return f0;
}

} // namespace

std::vector<float> render(const std::u32string &text, const Params &params) {
    std::vector<float> out;
    const int sample_rate = synth::SAMPLE_RATE;
    const Setup st = make_setup(params);
    const std::string folded = fold(text);
    const std::vector<Seg> segs = timeline(folded, st);
    const Seg *last = nullptr;
    for (const Seg &s : segs) {
        if (s.kind != "pause") {
            last = &s;
        }
    }
    if (last == nullptr) {
        return out;
    }
    const double sr = double(sample_rate);
    const uint32_t seed = text_hash(text) ^ uint32_t((uint64_t(st.seed) * 0x9E3779B1ull) & 0xFFFFFFFFull);
    const size_t n = size_t(std::ceil((last->end + 0.06) * sr));
    const Tracks tr = make_tracks(segs, st);
    std::vector<double> f0 = pitch_track(segs, n, sr, st, seed);
    // The Mall: a tremble in pitch for fear and dread.
    for (size_t i = 0; i < n && params.shake > 0.0; i++) {
        f0[i] *= std::pow(2.0, params.shake * kShakeSemis * std::sin(2.0 * kPi * kShakeHz * double(i) / sr) / 12.0);
    }

    const double size = st.size * (1.0 + 0.04 * st.goblin);
    const double breath = st.emo.breath + 0.25 * st.breath;
    const double tilt = std::min(0.92, st.emo.tilt + 0.2 * (st.breath - 0.5));
    const double oq = st.emo.oq;
    // glottal source
    std::vector<double> g(n), open(n);
    double acc = 0.0;
    for (size_t i = 0; i < n; i++) {
        const double dt = f0[i] / sr;
        acc += dt;
        const double phase = std::fmod(acc, 1.0);
        double v = phase < oq ? (2.0 * phase * oq - 3.0 * phase * phase) / (oq * oq) : 0.0;
        const double d = phase - oq;
        const double x1 = d / dt;
        if (d >= 0.0 && d < dt) {
            v = v - 0.5 * (1.0 - x1) * (1.0 - x1);
        }
        if (d < 0.0 && d >= -dt) {
            v = v + 0.5 * (x1 + 1.0) * (x1 + 1.0);
        }
        g[i] = v;
        open[i] = phase < oq ? 1.0 : 0.35;
    }
    one_pole(g, tilt, 0.0);
    const std::vector<double> n_breath = hash_noise(n, seed ^ 0x1B873593u);
    const std::vector<double> n_asp = hash_noise(n, seed ^ 0x27D4EB2Fu);
    const std::vector<double> n_fric = hash_noise(n, seed ^ 0x165667B1u);
    const Track av_t(tr.av, 0.0), gate_t(tr.gate, 1.0), ah_t(tr.ah, 0.0), af_t(tr.af, 0.0);
    std::vector<double> src(n), fr(n);
    for (size_t i = 0; i < n; i++) {
        const double t = double(i) / sr;
        const double av = av_t.at(t) * gate_t.at(t);
        src[i] = av * (g[i] * 0.5 + n_breath[i] * breath * open[i] * 0.18) + ah_t.at(t) * n_asp[i] * 0.5;
        fr[i] = af_t.at(t) * n_fric[i];
    }
    // filters, updated per block
    const size_t nb = (n + kBlock - 1) / kBlock;
    const VowelData &ax = vowel_ax();
    const double ax_f[3] = { ax.f1a, ax.f2a, ax.f3a };
    const Track f_t[3] = { Track(tr.f[0], ax_f[0]), Track(tr.f[1], ax_f[1]), Track(tr.f[2], ax_f[2]) };
    const Track nz_t(tr.nz, 1600.0), nasal_t(tr.nasal, 0.0);
    const FricData &s_def = fric("s");
    const double spec_def[6] = { s_def.cf1, s_def.bw1, s_def.g1, s_def.cf2, s_def.bw2, s_def.g2 };
    const Track spec_t[6] = { Track(tr.spec[0], spec_def[0]), Track(tr.spec[1], spec_def[1]),
        Track(tr.spec[2], spec_def[2]), Track(tr.spec[3], spec_def[3]), Track(tr.spec[4], spec_def[4]),
        Track(tr.spec[5], spec_def[5]) };
    BiquadState cascade[7];
    BiquadState f1s, f2s;
    std::vector<double> y(n);
    for (size_t blk = 0; blk < nb; blk++) {
        const double tc = (double(blk * kBlock) + kBlock / 2.0) / sr;
        const size_t center = blk * kBlock + kBlock / 2;  // numpy.interp(tc, t, f0) hits a sample exactly
        const double f0b = center < n ? f0[center] : f0[n - 1];
        double f[5];
        for (int k = 0; k < 3; k++) {
            f[k] = f_t[k].at(tc) * size;
        }
        f[3] = kF4 * size * (1.0 + 0.05 * st.goblin);
        f[4] = kF5 * size;
        const double bw[5] = { 60.0 + 0.1 * f0b + 40.0 * breath, 90.0 + 0.08 * f0b, 140.0 + 0.06 * f0b, 250.0, 320.0 };
        Biquad sec[7];
        for (int k = 0; k < 5; k++) {
            sec[k] = resonator(f[k], bw[k], sr);
        }
        const double nasal = nasal_t.at(tc);
        sec[5] = peaking(kNasalPole, 8.0 * nasal, 2.0, sr);
        sec[6] = peaking(nz_t.at(tc) * size, -24.0 * nasal, 2.5, sr);
        double spec[6];
        for (int k = 0; k < 6; k++) {
            spec[k] = spec_t[k].at(tc);
        }
        const Biquad bp1 = bandpass(spec[0] * std::pow(size, 0.5), spec[1], sr);
        const Biquad bp2 = bandpass(spec[3] * std::pow(size, 0.5), spec[4], sr);
        const size_t s0 = blk * kBlock, s1 = std::min(n, s0 + kBlock);
        for (size_t i = s0; i < s1; i++) {
            double v = src[i];
            for (int k = 0; k < 7; k++) {
                v = cascade[k].process(sec[k], v);
            }
            const double y1 = f1s.process(bp1, fr[i]);
            const double y2 = f2s.process(bp2, fr[i]);
            y[i] = v + 2.2 * (y1 * spec[2] + y2 * spec[5]);
        }
    }
    // DC blocker: lfilter([1, -1], [1, -0.995])
    double z = 0.0;
    for (size_t i = 0; i < n; i++) {
        const double x = y[i];
        const double o = x + z;
        z = -x + 0.995 * o;
        y[i] = o;
    }
    double peak = 0.0;
    for (double v : y) {
        peak = std::max(peak, std::abs(v));
    }
    const double gain = peak > 0.0 ? 0.89 * st.volume / peak : 0.0;
    out.resize(n);
    for (size_t i = 0; i < n; i++) {
        // The Mall: and in loudness.
        const double tremble = 0.5 + 0.5 * std::sin(2.0 * kPi * kShakeHz * double(i) / sr);
        out[i] = float(y[i] * gain * (1.0 - params.shake * kShakeLoud * tremble));
    }
    return out;
}

} // namespace english
} // namespace voice
