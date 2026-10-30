#include "human/name.h"
#include "core/rng.h"
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

/// What the chain learns from when data/names.txt is missing: short first
/// names from all over, lowercase.
constexpr std::string_view CORPUS[] = {
    "anna",    "maria",   "sofia",   "luca",    "marco",   "giulia",  "elena",   "paolo",   "chiara",  "matteo",
    "sara",    "laura",   "andrea",  "nina",    "lena",    "mila",    "jonas",   "felix",   "emma",    "noah",
    "liam",    "olivia",  "ava",     "mia",     "leo",     "theo",    "ella",    "ivy",     "ruby",    "finn",
    "oscar",   "hugo",    "lucas",   "louis",   "jules",   "chloe",   "manon",   "camille", "lea",     "zoe",
    "ines",    "lucia",   "carmen",  "pablo",   "diego",   "mateo",   "javier",  "alba",    "vera",    "irene",
    "nora",    "ida",     "ebba",    "alma",    "elsa",    "saga",    "freja",   "signe",   "astrid",  "ingrid",
    "erik",    "lars",    "sven",    "olav",    "bjorn",   "anders",  "kari",    "liv",     "tove",    "runa",
    "aino",    "eero",    "ilkka",   "jussi",   "kaisa",   "satu",    "tiina",   "ville",   "aleksi",  "onni",
    "piotr",   "kasia",   "ola",     "zofia",   "jan",     "marek",   "hanna",   "ewa",     "iwona",   "tomek",
    "ivan",    "olga",    "dmitri",  "katya",   "sasha",   "misha",   "vlad",    "tanya",   "yuri",    "nadia",
    "yusuf",   "amira",   "layla",   "omar",    "zara",    "tariq",   "samir",   "leila",   "nour",    "karim",
    "hana",    "yuki",    "akira",   "haru",    "sora",    "kenji",   "mei",     "ren",     "aoi",     "rin",
    "minho",   "jisoo",   "sora",    "yuna",    "jiho",    "hana",    "wei",     "ling",    "jun",     "xiu",
    "arjun",   "priya",   "ravi",    "asha",    "dev",     "kiran",   "meera",   "rohan",   "tara",    "nikhil",
    "kofi",    "ama",     "kwame",   "abena",   "zuri",    "amani",   "imani",   "jabari",  "nia",     "femi",
    "tomas",   "beatriz", "joao",    "ines",    "rui",     "tiago",   "duarte",  "catarina", "pedro",  "ana",
    "bruno",   "rafael",  "lucas",   "camila",  "thiago",  "isabela", "mateus",  "larissa", "caio",    "bianca",
    "james",   "john",    "mary",    "susan",   "peter",   "alice",   "edith",   "harold",  "arthur",  "rose",
    "grace",   "henry",   "clara",   "otto",    "greta",   "karl",    "frida",   "hans",    "lotte",   "max",
    "milo",    "iris",    "esme",    "arlo",    "juno",    "remy",    "lucie",   "bram",    "sanne",   "daan",
    "fleur",   "tess",    "lotte",   "noor",    "sem",     "jesse",   "lieke",   "koen",    "roos",    "teun",
    "aisling", "ciara",   "niamh",   "siobhan", "sean",    "cian",    "oisin",   "tadhg",   "eoin",    "roisin",
    "rhys",    "gwen",    "dylan",   "megan",   "owen",    "carys",   "ioan",    "efa",     "elin",    "gareth",
    "dimitra", "nikos",   "eleni",   "yannis",  "sofia",   "kostas",  "maria",   "petros",  "ioanna",  "thanos",
    "elif",    "emre",    "defne",   "cem",     "zeynep",  "kaan",    "ayse",    "mert",    "ece",     "deniz",
    "levi",    "noa",     "tamar",   "yael",    "eitan",   "shira",   "omer",    "maya",    "idan",    "lior",
    "aroha",   "nikau",   "manaia",  "wiremu",  "hemi",    "ariki",   "moana",   "kai",     "tane",    "awhina",
    "ximena",  "santiago", "valentina", "sebastian", "renata", "emilio", "regina", "andres", "paula",  "joaquin",
    "jack",    "harry",   "amelia",  "isla",    "poppy",   "freddie", "evie",    "oliver",  "lily",    "archie",
    "beth",    "rosa",    "nell",    "wren",    "sage",    "ash",     "blue",    "pip",     "rue",     "fern",
};

/// Where the big list may be: named by THEMALL_NAMES, beside where the game
/// was started, or beside the game itself.
std::vector<std::filesystem::path> places() {
    std::vector<std::filesystem::path> p;
    if (const char* env = std::getenv("THEMALL_NAMES")) p.emplace_back(env);
    p.emplace_back("data/names.txt");
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        p.push_back(exe.parent_path() / "../data/names.txt");
        p.push_back(exe.parent_path() / "data/names.txt");
    }
    return p;
}

/// One name per row, kept only if it is all letters, lowercased.
std::vector<std::string> readNames() {
    std::vector<std::string> names;
    for (const auto& path : places()) {
        std::ifstream in(path);
        if (!in) continue;
        std::string row;
        while (std::getline(in, row)) {
            while (!row.empty() && (row.back() == '\r' || row.back() == ' ')) row.pop_back();
            bool letters = !row.empty();
            for (char& c : row) {
                if (!std::isalpha(uint8_t(c))) letters = false;
                c = char(std::tolower(uint8_t(c)));
            }
            if (letters) names.push_back(row);
        }
        if (!names.empty()) return names;
    }
    std::fprintf(stderr, "no data/names.txt found: names come from the small built-in list\n");
    for (std::string_view name : CORPUS) names.emplace_back(name);
    return names;
}

/// How often each letter followed the three before it, with '\0' standing
/// in for the start of a name; slot 0 counts names that ended there.
struct Chain {
    using Counts = std::array<uint32_t, 27>;
    std::unordered_map<uint32_t, Counts> next;

    static uint32_t push(uint32_t key, char c) { return (key << 8 | uint8_t(c)) & 0xffffff; }
    static int slot(char c) { return c ? c - 'a' + 1 : 0; }

    Chain() {
        for (const std::string& name : readNames()) {
            uint32_t key = 0;
            for (char c : name) {
                next[key][slot(c)]++;
                key = push(key, c);
            }
            next[key][0]++;
        }
    }

    /// A letter drawn by those counts, or '\0' to end the name.
    static char pick(const Counts& counts, Rng& rng) {
        uint32_t total = 0;
        for (uint32_t n : counts) total += n;
        uint32_t r = uint32_t(rng.next() % total);
        for (int i = 0; i < 27; i++) {
            if (r < counts[i]) return i ? char('a' + i - 1) : '\0';
            r -= counts[i];
        }
        return '\0';
    }
};

constexpr int MIN_LETTERS = 3;
constexpr int MAX_LETTERS = 12;
constexpr int TRIES = 40;

} // namespace

std::string randomName(uint64_t seed) {
    static const Chain chain;
    Rng rng(seed ^ 0x6e616d6573ULL);
    std::string name;
    for (int t = 0; t < TRIES; t++) {
        name.clear();
        uint32_t key = 0;
        while (int(name.size()) <= MAX_LETTERS) {
            auto it = chain.next.find(key);
            if (it == chain.next.end()) break;
            char c = Chain::pick(it->second, rng);
            if (c == '\0') break;
            name.push_back(c);
            key = Chain::push(key, c);
        }
        // Finished on an ending that the corpus knows, and at a sensible length.
        if (int(name.size()) >= MIN_LETTERS && int(name.size()) <= MAX_LETTERS) break;
    }
    if (name.empty()) name = "ann";
    name[0] = char(std::toupper(uint8_t(name[0])));
    return name;
}
