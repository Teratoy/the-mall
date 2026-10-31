// Saves raylib's default font, which the game draws all its text in, for the
// lines editor (tools/lines_editor.py): its atlas as a PNG and each glyph's
// place in it as JSON, so the editor measures and draws lines as the game does.
//   build/font_dump tools/lines_editor
#include <raylib.h>
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "tools/lines_editor";
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "font dump");
    Font font = GetFontDefault();
    Image atlas = LoadImageFromTexture(font.texture);
    ExportImage(atlas, (dir + "/font.png").c_str());
    UnloadImage(atlas);
    FILE* out = std::fopen((dir + "/font.json").c_str(), "w");
    if (!out) {
        std::fprintf(stderr, "can't write %s/font.json\n", dir.c_str());
        return 1;
    }
    std::fprintf(out, "{\"base\": %d, \"glyphs\": {\n", font.baseSize);
    for (int i = 0; i < font.glyphCount; i++) {
        const GlyphInfo& g = font.glyphs[i];
        const Rectangle& r = font.recs[i];
        std::fprintf(out, "  \"%d\": [%d, %d, %d, %d, %d, %d, %d]%s\n", g.value, int(r.x), int(r.y), int(r.width),
                     int(r.height), g.offsetX, g.offsetY, g.advanceX, i + 1 < font.glyphCount ? "," : "");
    }
    std::fprintf(out, "}}\n");
    std::fclose(out);
    CloseWindow();
    return 0;
}
