
/*
* MULTI-CHANNEL SIGNED DISTANCE FIELD ATLAS GENERATOR - standalone console program
* --------------------------------------------------------------------------------
* A utility by Viktor Chlumsky, (c) 2020 - 2024
*/

#ifdef MSDF_ATLAS_STANDALONE

#define _USE_MATH_DEFINES
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cassert>
#include <vector>
#include <algorithm>
#include <thread>

#include "msdf-atlas-gen.h"

using namespace msdf_atlas;

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define DEFAULT_MITER_LIMIT 1.0
#define DEFAULT_PIXEL_RANGE 2.0
#define SDF_ERROR_ESTIMATE_PRECISION 19
#define GLYPH_FILL_RULE msdfgen::FILL_NONZERO
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull

#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define MSDF_ATLAS_VERSION_STRING STRINGIZE(MSDF_ATLAS_VERSION)
#define MSDFGEN_VERSION_STRING STRINGIZE(MSDFGEN_VERSION)
#ifdef MSDF_ATLAS_VERSION_UNDERLINE
    #define VERSION_UNDERLINE STRINGIZE(MSDF_ATLAS_VERSION_UNDERLINE)
#else
    #define VERSION_UNDERLINE "--------"
#endif

#ifdef MSDFGEN_USE_SKIA
    #define TITLE_SUFFIX    " & Skia"
    #define EXTRA_UNDERLINE "-------"
#else
    #define TITLE_SUFFIX
    #define EXTRA_UNDERLINE
#endif

static const char *const versionText =
    "MSDF-Atlas-Gen v" MSDF_ATLAS_VERSION_STRING "\n"
    "  with MSDFgen v" MSDFGEN_VERSION_STRING TITLE_SUFFIX "\n"
    "(c) 2020 - " STRINGIZE(MSDF_ATLAS_COPYRIGHT_YEAR) " Viktor Chlumsky";

static const char *const helpText = R"(
MSDF Atlas Generator by Viktor Chlumsky v)" MSDF_ATLAS_VERSION_STRING R"( (with MSDFgen v)" MSDFGEN_VERSION_STRING TITLE_SUFFIX R"()
----------------------------------------------------------------)" VERSION_UNDERLINE EXTRA_UNDERLINE R"(

INPUT SPECIFICATION
  -font <filename.ttf/otf>
      Specifies the input TrueType / OpenType font file. A font specification is required.
  -varfont <filename.ttf/otf?var0=value0&var1=value1>
      Specifies an input variable font file and configures its variables.
  -charset <filename>
      Specifies the input character set. Refer to the documentation for format of charset specification. Defaults to ASCII.
  -glyphset <filename>
      Specifies the set of input glyphs as glyph indices within the font file.
  -allglyphs
      Specifies that all glyphs within the font file are to be processed.
  -fontscale <scale>
      Specifies the scale to be applied to the glyph geometry of the font.
  -fontname <name>
      Specifies a name for the font that will be propagated into the output files as metadata.
  -and
      Separates multiple inputs to be combined into a single atlas.

ATLAS CONFIGURATION
  -type <hardmask / softmask / sdf / psdf / msdf / mtsdf>
      Selects the type of atlas to be generated.
  -format <png / bmp / tiff / text / textfloat / bin / binfloat / binfloatbe>
      Selects the format for the atlas image output. Some image formats may be incompatible with embedded output formats.
  -dimensions <width> <height>
      Sets the atlas to have fixed dimensions (width x height).
  -pots / -potr / -square / -square2 / -square4
      Picks the minimum atlas dimensions that fit all glyphs and satisfy the selected constraint:
      power of two square / ... rectangle / any square / square with side divisible by 2 / ... 4
  -uniformgrid
      Lays out the atlas into a uniform grid. Enables following options starting with -uniform:
    -uniformcols <N>
        Sets the number of grid columns.
    -uniformcell <width> <height>
        Sets fixed dimensions of the grid's cells.
    -uniformcellconstraint <none / pots / potr / square / square2 / square4>
        Constrains cell dimensions to the given rule (see -pots / ... above)
    -uniformorigin <off / on / horizontal / vertical>
        Sets whether the glyph's origin point should be fixed at the same position in each cell
  -yorigin <bottom / top>
      Determines whether the Y-axis is oriented upwards (bottom origin, default) or downwards (top origin).

OUTPUT SPECIFICATION - one or more can be specified
  -imageout <filename.*>
      Saves the atlas as an image file with the specified format. Layout data must be stored separately.
  -json <filename.json>
      Writes the atlas's layout data, as well as other metrics into a structured JSON file.
  -csv <filename.csv>
      Writes the layout data of the glyphs into a simple CSV file.)"
#ifndef MSDF_ATLAS_NO_ARTERY_FONT
R"(
  -arfont <filename.arfont>
      Stores the atlas and its layout data as an Artery Font file. Supported formats: png, bin, binfloat.)"
#endif
R"(
  -shadronpreview <filename.shadron> <sample text>
      Generates a Shadron script that uses the generated atlas to draw a sample text as a preview.

GLYPH CONFIGURATION
  -size <em size>
      Specifies the size of the glyphs in the atlas bitmap in pixels per em.
  -minsize <em size>
      Specifies the minimum size. The largest possible size that fits the same atlas dimensions will be used.
  -emrange <em range>
      Specifies the SDF distance range in em's.
  -pxrange <pixel range>
      Specifies the SDF distance range in output pixels. The default value is 2.
  -pxalign <off / on / horizontal / vertical>
      Specifies whether each glyph's origin point should be aligned with the pixel grid.
  -nokerning
      Disables inclusion of kerning pair table in output files.

DISTANCE FIELD GENERATOR SETTINGS
  -angle <angle>
      Specifies the minimum angle between adjacent edges to be considered a corner. Append D for degrees. (msdf / mtsdf only)
  -coloringstrategy <simple / inktrap / distance>
      Selects the strategy of the edge coloring heuristic.
  -errorcorrection <mode>
      Changes the MSDF/MTSDF error correction mode. Use -errorcorrection help for a list of valid modes.
  -errordeviationratio <ratio>
      Sets the minimum ratio between the actual and maximum expected distance delta to be considered an error.
  -errorimproveratio <ratio>
      Sets the minimum ratio between the pre-correction distance error and the post-correction distance error.
  -miterlimit <value>
      Sets the miter limit that limits the extension of each glyph's bounding box due to very sharp corners. (psdf / msdf / mtsdf only))"
#ifdef MSDFGEN_USE_SKIA
R"(
  -overlap
      Switches to distance field generator with support for overlapping contours.
  -nopreprocess
      Disables path preprocessing which resolves self-intersections and overlapping contours.
  -scanline
      Performs an additional scanline pass to fix the signs of the distances.)"
#else
R"(
  -nooverlap
      Disables resolution of overlapping contours.
  -noscanline
      Disables the scanline pass, which corrects the distance field's signs according to the non-zero fill rule.)"
#endif
R"(
  -seed <N>
      Sets the initial seed for the edge coloring heuristic.
  -threads <N>
      Sets the number of threads for the parallel computation. (0 = auto)
)";

static const char *errorCorrectionHelpText = R"(
ERROR CORRECTION MODES
  auto-fast
      Detects inversion artifacts and distance errors that do not affect edges by range testing.
  auto-full
      Detects inversion artifacts and distance errors that do not affect edges by exact distance evaluation.
  auto-mixed (default)
      Detects inversions by distance evaluation and distance errors that do not affect edges by range testing.
  disabled
      Disables error correction.
  distance-fast
      Detects distance errors by range testing. Does not care if edges and corners are affected.
  distance-full
      Detects distance errors by exact distance evaluation. Does not care if edges and corners are affected, slow.
  edge-fast
      Detects inversion artifacts only by range testing.
  edge-full
      Detects inversion artifacts only by exact distance evaluation.
  help
      Displays this help.
)";

static char toupper(char c) {
    return c >= 'a' && c <= 'z' ? c-'a'+'A' : c;
}

static bool parseUnsigned(unsigned &value, const char *arg) {
    static char c;
    return sscanf(arg, "%u%c", &value, &c) == 1;
}

static bool parseUnsignedLL(unsigned long long &value, const char *arg) {
    static char c;
    return sscanf(arg, "%llu%c", &value, &c) == 1;
}

static bool parseDouble(double &value, const char *arg) {
    static char c;
    return sscanf(arg, "%lf%c", &value, &c) == 1;
}

static bool parseAngle(double &value, const char *arg) {
    char c1, c2;
    int result = sscanf(arg, "%lf%c%c", &value, &c1, &c2);
    if (result == 1)
        return true;
    if (result == 2 && (c1 == 'd' || c1 == 'D')) {
        value *= M_PI/180;
        return true;
    }
    return false;
}

static bool cmpExtension(const char *path, const char *ext) {
    for (const char *a = path+strlen(path)-1, *b = ext+strlen(ext)-1; b >= ext; --a, --b)
        if (a < path || toupper(*a) != toupper(*b))
            return false;
    return true;
}

static msdfgen::FontHandle *loadVarFont(msdfgen::FreetypeHandle *library, const char *filename) {
    std::string buffer;
    while (*filename && *filename != '?')
        buffer.push_back(*filename++);
    msdfgen::FontHandle *font = msdfgen::loadFont(library, buffer.c_str());
    if (font && *filename++ == '?') {
        do {
            buffer.clear();
            while (*filename && *filename != '=')
                buffer.push_back(*filename++);
            if (*filename == '=') {
                double value = 0;
                int skip = 0;
                if (sscanf(++filename, "%lf%n", &value, &skip) == 1) {
                    msdfgen::setFontVariationAxis(library, font, buffer.c_str(), value);
                    filename += skip;
                }
            }
        } while (*filename++ == '&');
    }
    return font;
}

struct FontInput {
    const char *fontFilename;
    bool variableFont;
    GlyphIdentifierType glyphIdentifierType;
    const char *charsetFilename;
    double fontScale;
    const char *fontName;
};

struct Configuration {
    ImageType imageType;
    ImageFormat imageFormat;
    YDirection yDirection;
    int width, height;
    double emSize;
    double pxRange;
    double angleThreshold;
    double miterLimit;
    bool pxAlignOriginX, pxAlignOriginY;
    struct {
        int cellWidth, cellHeight;
        int cols, rows;
        bool fixedOriginX, fixedOriginY;
    } grid;
    void (*edgeColoring)(msdfgen::Shape &, double, unsigned long long);
    bool expensiveColoring;
    unsigned long long coloringSeed;
    GeneratorAttributes generatorAttributes;
    bool preprocessGeometry;
    bool kerning;
    int threadCount;
    const char *arteryFontFilename;
    const char *imageFilename;
    const char *jsonFilename;
    const char *csvFilename;
    const char *shadronPreviewFilename;
    const char *shadronPreviewText;
};

template <typename T, typename S, int N, GeneratorFunction<S, N> GEN_FN>
static bool makeAtlas(const std::vector<GlyphGeometry> &glyphs, const std::vector<FontGeometry> &fonts, const Configuration &config) {
    ImmediateAtlasGenerator<S, N, GEN_FN, BitmapAtlasStorage<T, N> > generator(config.width, config.height);
    generator.setAttributes(config.generatorAttributes);
    generator.setThreadCount(config.threadCount);
    generator.generate(glyphs.data(), glyphs.size());
    msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>) generator.atlasStorage();

    bool success = true;

    if (config.imageFilename) {
        if (saveImage(bitmap, config.imageFormat, config.imageFilename, config.yDirection))
            fputs("Atlas image file saved.\n", stderr);
        else {
            success = false;
            fputs("Failed to save the atlas as an image file.\n", stderr);
        }
    }

#ifndef MSDF_ATLAS_NO_ARTERY_FONT
    if (config.arteryFontFilename) {
        ArteryFontExportProperties arfontProps;
        arfontProps.fontSize = config.emSize;
        arfontProps.pxRange = config.pxRange;
        arfontProps.imageType = config.imageType;
        arfontProps.imageFormat = config.imageFormat;
        arfontProps.yDirection = config.yDirection;
        if (exportArteryFont<float>(fonts.data(), fonts.size(), bitmap, config.arteryFontFilename, arfontProps))
            fputs("Artery Font file generated.\n", stderr);
        else {
            success = false;
            fputs("Failed to generate Artery Font file.\n", stderr);
        }
    }
#endif

    return success;
}

int main(int argc, const char *const *argv) {
    #define ABORT(msg) do { fputs(msg "\n", stderr); return 1; } while (false)

    int result = 0;
    std::vector<FontInput> fontInputs;
    FontInput fontInput = { };
    Configuration config = { };
    fontInput.glyphIdentifierType = GlyphIdentifierType::UNICODE_CODEPOINT;
    fontInput.fontScale = -1;
    config.imageType = ImageType::MSDF;
    config.imageFormat = ImageFormat::UNSPECIFIED;
    config.yDirection = YDirection::BOTTOM_UP;
    config.grid.fixedOriginX = false, config.grid.fixedOriginY = true;
    config.edgeColoring = msdfgen::edgeColoringInkTrap;
    config.kerning = true;
    const char *imageFormatName = nullptr;
    int fixedWidth = -1, fixedHeight = -1;
    int fixedCellWidth = -1, fixedCellHeight = -1;
    config.preprocessGeometry = (
        #ifdef MSDFGEN_USE_SKIA
            true
        #else
            false
        #endif
    );
    config.generatorAttributes.config.overlapSupport = !config.preprocessGeometry;
    config.generatorAttributes.scanlinePass = !config.preprocessGeometry;
    double minEmSize = 0;
    enum {
        /// Range specified in ems
        RANGE_EM,
        /// Range specified in output pixels
        RANGE_PIXEL,
    } rangeMode = RANGE_PIXEL;
    double rangeValue = 0;
    PackingStyle packingStyle = PackingStyle::TIGHT;
    DimensionsConstraint atlasSizeConstraint = DimensionsConstraint::NONE;
    DimensionsConstraint cellSizeConstraint = DimensionsConstraint::NONE;
    config.angleThreshold = DEFAULT_ANGLE_THRESHOLD;
    config.miterLimit = DEFAULT_MITER_LIMIT;
    config.pxAlignOriginX = false, config.pxAlignOriginY = true;
    config.threadCount = 0;

    // Parse command line
    int argPos = 1;
    bool suggestHelp = false;
    bool explicitErrorCorrectionMode = false;
    while (argPos < argc) {
        const char *arg = argv[argPos];
        #define ARG_CASE(s, p) if (!strcmp(arg, s) && argPos+(p) < argc)

        // Accept arguments prefixed with -- instead of -
        if (arg[0] == '-' && arg[1] == '-')
            ++arg;

        ARG_CASE("-type", 1) {
            arg = argv[++argPos];
            if (!strcmp(arg, "hardmask"))
                config.imageType = ImageType::HARD_MASK;
            else if (!strcmp(arg, "softmask"))
                config.imageType = ImageType::SOFT_MASK;
            else if (!strcmp(arg, "sdf"))
                config.imageType = ImageType::SDF;
            else if (!strcmp(arg, "psdf"))
                config.imageType = ImageType::PSDF;
            else if (!strcmp(arg, "msdf"))
                config.imageType = ImageType::MSDF;
            else if (!strcmp(arg, "mtsdf"))
                config.imageType = ImageType::MTSDF;
            else
                ABORT("Invalid atlas type. Valid types are: hardmask, softmask, sdf, psdf, msdf, mtsdf");
            ++argPos;
            continue;
        }
        ARG_CASE("-format", 1) {
            arg = argv[++argPos];
            if (!strcmp(arg, "png"))
                config.imageFormat = ImageFormat::PNG;
            else if (!strcmp(arg, "bmp"))
                config.imageFormat = ImageFormat::BMP;
            else if (!strcmp(arg, "tiff"))
                config.imageFormat = ImageFormat::TIFF;
            else if (!strcmp(arg, "text"))
                config.imageFormat = ImageFormat::TEXT;
            else if (!strcmp(arg, "textfloat"))
                config.imageFormat = ImageFormat::TEXT_FLOAT;
            else if (!strcmp(arg, "bin"))
                config.imageFormat = ImageFormat::BINARY;
            else if (!strcmp(arg, "binfloat"))
                config.imageFormat = ImageFormat::BINARY_FLOAT;
            else if (!strcmp(arg, "binfloatbe"))
                config.imageFormat = ImageFormat::BINARY_FLOAT_BE;
            else
                ABORT("Invalid image format. Valid formats are: png, bmp, tiff, text, textfloat, bin, binfloat");
            imageFormatName = arg;
            ++argPos;
            continue;
        }
        ARG_CASE("-font", 1) {
            fontInput.fontFilename = argv[++argPos];
            fontInput.variableFont = false;
            ++argPos;
            continue;
        }
        ARG_CASE("-varfont", 1) {
            fontInput.fontFilename = argv[++argPos];
            fontInput.variableFont = true;
            ++argPos;
            continue;
        }
        ARG_CASE("-charset", 1) {
            fontInput.charsetFilename = argv[++argPos];
            fontInput.glyphIdentifierType = GlyphIdentifierType::UNICODE_CODEPOINT;
            ++argPos;
            continue;
        }
        ARG_CASE("-glyphset", 1) {
            fontInput.charsetFilename = argv[++argPos];
            fontInput.glyphIdentifierType = GlyphIdentifierType::GLYPH_INDEX;
            ++argPos;
            continue;
        }
        ARG_CASE("-allglyphs", 0) {
            fontInput.charsetFilename = nullptr;
            fontInput.glyphIdentifierType = GlyphIdentifierType::GLYPH_INDEX;
            ++argPos;
            continue;
        }
        ARG_CASE("-fontscale", 1) {
            double fs;
            if (!(parseDouble(fs, argv[++argPos]) && fs > 0))
                ABORT("Invalid font scale argument. Use -fontscale <font scale> with a positive real number.");
            fontInput.fontScale = fs;
            ++argPos;
            continue;
        }
        ARG_CASE("-fontname", 1) {
            fontInput.fontName = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-and", 0) {
            if (!fontInput.fontFilename && !fontInput.charsetFilename && fontInput.fontScale < 0)
                ABORT("No font, character set, or font scale specified before -and separator.");
            if (!fontInputs.empty() && !memcmp(&fontInputs.back(), &fontInput, sizeof(FontInput)))
                ABORT("No changes between subsequent inputs. A different font, character set, or font scale must be set inbetween -and separators.");
            fontInputs.push_back(fontInput);
            fontInput.fontName = nullptr;
            ++argPos;
            continue;
        }
    #ifndef MSDF_ATLAS_NO_ARTERY_FONT
        ARG_CASE("-arfont", 1) {
            config.arteryFontFilename = argv[++argPos];
            ++argPos;
            continue;
        }
    #endif
        ARG_CASE("-imageout", 1) {
            config.imageFilename = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-json", 1) {
            config.jsonFilename = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-csv", 1) {
            config.csvFilename = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-shadronpreview", 2) {
            config.shadronPreviewFilename = argv[++argPos];
            config.shadronPreviewText = argv[++argPos];
            ++argPos;
            continue;
        }
        ARG_CASE("-dimensions", 2) {
            unsigned w, h;
            if (!(parseUnsigned(w, argv[argPos+1]) && parseUnsigned(h, argv[argPos+2]) && w && h))
                ABORT("Invalid atlas dimensions. Use -dimensions <width> <height> with two positive integers.");
            fixedWidth = w, fixedHeight = h;
            argPos += 3;
            continue;
        }
        ARG_CASE("-pots", 0) {
            atlasSizeConstraint = DimensionsConstraint::POWER_OF_TWO_SQUARE;
            fixedWidth = -1, fixedHeight = -1;
            ++argPos;
            continue;
        }
        ARG_CASE("-potr", 0) {
            atlasSizeConstraint = DimensionsConstraint::POWER_OF_TWO_RECTANGLE;
            fixedWidth = -1, fixedHeight = -1;
            ++argPos;
            continue;
        }
        ARG_CASE("-square", 0) {
            atlasSizeConstraint = DimensionsConstraint::SQUARE;
            fixedWidth = -1, fixedHeight = -1;
            ++argPos;
            continue;
        }
        ARG_CASE("-square2", 0) {
            atlasSizeConstraint = DimensionsConstraint::EVEN_SQUARE;
            fixedWidth = -1, fixedHeight = -1;
            ++argPos;
            continue;
        }
        ARG_CASE("-square4", 0) {
            atlasSizeConstraint = DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE;
            fixedWidth = -1, fixedHeight = -1;
            ++argPos;
            continue;
        }
        ARG_CASE("-yorigin", 1) {
            arg = argv[++argPos];
            if (!strcmp(arg, "bottom"))
                config.yDirection = YDirection::BOTTOM_UP;
            else if (!strcmp(arg, "top"))
                config.yDirection = YDirection::TOP_DOWN;
            else
                ABORT("Invalid Y-axis origin. Use bottom or top.");
            ++argPos;
            continue;
        }
        ARG_CASE("-size", 1) {
            double s;
            if (!(parseDouble(s, argv[++argPos]) && s > 0))
                ABORT("Invalid em size argument. Use -size <em size> with a positive real number.");
            config.emSize = s;
            ++argPos;
            continue;
        }
        ARG_CASE("-minsize", 1) {
            double s;
            if (!(parseDouble(s, argv[++argPos]) && s > 0))
                ABORT("Invalid minimum em size argument. Use -minsize <em size> with a positive real number.");
            minEmSize = s;
            ++argPos;
            continue;
        }
        ARG_CASE("-emrange", 1) {
            double r;
            if (!(parseDouble(r, argv[++argPos]) && r >= 0))
                ABORT("Invalid range argument. Use -emrange <em range> with a positive real number.");
            rangeMode = RANGE_EM;
            rangeValue = r;
            ++argPos;
            continue;
        }
        ARG_CASE("-pxrange", 1) {
            double r;
            if (!(parseDouble(r, argv[++argPos]) && r >= 0))
                ABORT("Invalid range argument. Use -pxrange <pixel range> with a positive real number.");
            rangeMode = RANGE_PIXEL;
            rangeValue = r;
            ++argPos;
            continue;
        }
        ARG_CASE("-pxalign", 1) {
            if (!strcmp(argv[argPos+1], "off") || !memcmp(argv[argPos+1], "disable", 7) || !strcmp(argv[argPos+1], "0") || !strcmp(argv[argPos+1], "false") || argv[argPos+1][0] == 'n')
                config.pxAlignOriginX = false, config.pxAlignOriginY = false;
            else if (!strcmp(argv[argPos+1], "on") || !memcmp(argv[argPos+1], "enable", 6) || !strcmp(argv[argPos+1], "1") || !strcmp(argv[argPos+1], "true") || !strcmp(argv[argPos+1], "hv") || argv[argPos+1][0] == 'y')
                config.pxAlignOriginX = true, config.pxAlignOriginY = true;
            else if (argv[argPos+1][0] == 'h')
                config.pxAlignOriginX = true, config.pxAlignOriginY = false;
            else if (argv[argPos+1][0] == 'v' || !strcmp(argv[argPos+1], "baseline") || !strcmp(argv[argPos+1], "default"))
                config.pxAlignOriginX = false, config.pxAlignOriginY = true;
            else
                ABORT("Unknown -pxalign setting. Use one of: off, on, horizontal, vertical.");
            argPos += 2;
            continue;
        }
        ARG_CASE("-angle", 1) {
            double at;
            if (!parseAngle(at, argv[argPos+1]))
                ABORT("Invalid angle threshold. Use -angle <min angle> with a positive real number less than PI or a value in degrees followed by 'd' below 180d.");
            config.angleThreshold = at;
            argPos += 2;
            continue;
        }
        ARG_CASE("-uniformgrid", 0) {
            packingStyle = PackingStyle::GRID;
            ++argPos;
            continue;
        }
        ARG_CASE("-uniformcols", 1) {
            packingStyle = PackingStyle::GRID;
            unsigned c;
            if (!(parseUnsigned(c, argv[argPos+1]) && c))
                ABORT("Invalid number of grid columns. Use -uniformcols <N> with a positive integer.");
            config.grid.cols = c;
            argPos += 2;
            continue;
        }
        ARG_CASE("-uniformcell", 2) {
            packingStyle = PackingStyle::GRID;
            unsigned w, h;
            if (!(parseUnsigned(w, argv[argPos+1]) && parseUnsigned(h, argv[argPos+2]) && w && h))
                ABORT("Invalid cell dimensions. Use -uniformcell <width> <height> with two positive integers.");
            fixedCellWidth = w, fixedCellHeight = h;
            argPos += 3;
            continue;
        }
        ARG_CASE("-uniformcellconstraint", 1) {
            packingStyle = PackingStyle::GRID;
            if (!strcmp(argv[argPos+1], "none") || !strcmp(argv[argPos+1], "rect"))
                cellSizeConstraint = DimensionsConstraint::NONE;
            else if (!strcmp(argv[argPos+1], "pots"))
                cellSizeConstraint = DimensionsConstraint::POWER_OF_TWO_SQUARE;
            else if (!strcmp(argv[argPos+1], "potr"))
                cellSizeConstraint = DimensionsConstraint::POWER_OF_TWO_RECTANGLE;
            else if (!strcmp(argv[argPos+1], "square"))
                cellSizeConstraint = DimensionsConstraint::SQUARE;
            else if (!strcmp(argv[argPos+1], "square2"))
                cellSizeConstraint = DimensionsConstraint::EVEN_SQUARE;
            else if (!strcmp(argv[argPos+1], "square4"))
                cellSizeConstraint = DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE;
            else
                ABORT("Unknown dimensions constaint. Use -uniformcellconstraint with one of: none, pots, potr, square, square2, or square4.");
            argPos += 2;
            continue;
        }
        ARG_CASE("-uniformorigin", 1) {
            packingStyle = PackingStyle::GRID;
            if (!strcmp(argv[argPos+1], "off") || !memcmp(argv[argPos+1], "disable", 7) || !strcmp(argv[argPos+1], "0") || !strcmp(argv[argPos+1], "false") || argv[argPos+1][0] == 'n')
                config.grid.fixedOriginX = false, config.grid.fixedOriginY = false;
            else if (!strcmp(argv[argPos+1], "on") || !memcmp(argv[argPos+1], "enable", 6) || !strcmp(argv[argPos+1], "1") || !strcmp(argv[argPos+1], "true") || !strcmp(argv[argPos+1], "hv") || argv[argPos+1][0] == 'y')
                config.grid.fixedOriginX = true, config.grid.fixedOriginY = true;
            else if (argv[argPos+1][0] == 'h')
                config.grid.fixedOriginX = true, config.grid.fixedOriginY = false;
            else if (argv[argPos+1][0] == 'v' || !strcmp(argv[argPos+1], "baseline") || !strcmp(argv[argPos+1], "default"))
                config.grid.fixedOriginX = false, config.grid.fixedOriginY = true;
            else
                ABORT("Unknown -uniformorigin setting. Use one of: off, on, horizontal, vertical.");
            argPos += 2;
            continue;
        }
        ARG_CASE("-errorcorrection", 1) {
            msdfgen::ErrorCorrectionConfig &ec = config.generatorAttributes.config.errorCorrection;
            if (!memcmp(argv[argPos+1], "disable", 7) || !strcmp(argv[argPos+1], "0") || !strcmp(argv[argPos+1], "none")) {
                ec.mode = msdfgen::ErrorCorrectionConfig::DISABLED;
                ec.distanceCheckMode = msdfgen::ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
            } else if (!strcmp(argv[argPos+1], "default") || !strcmp(argv[argPos+1], "auto") || !strcmp(argv[argPos+1], "auto-mixed") || !strcmp(argv[argPos+1], "mixed")) {
                ec.mode = msdfgen::ErrorCorrectionConfig::EDGE_PRIORITY;
                ec.distanceCheckMode = msdfgen::ErrorCorrectionConfig::CHECK_DISTANCE_AT_EDGE;
            } else if (!strcmp(argv[argPos+1], "auto-fast") || !strcmp(argv[argPos+1], "fast")) {
                ec.mode = msdfgen::ErrorCorrectionConfig::EDGE_PRIORITY;
                ec.distanceCheckMode = msdfgen::ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
            } else if (!strcmp(argv[argPos+1], "auto-full") || !strcmp(argv[argPos+1], "full")) {
                ec.mode = msdfgen::ErrorCorrectionConfig::EDGE_PRIORITY;
                ec.distanceCheckMode = msdfgen::ErrorCorrectionConfig::ALWAYS_CHECK_DISTANCE;
            } else if (!strcmp(argv[argPos+1], "distance") || !strcmp(argv[argPos+1], "distance-fast") || !strcmp(argv[argPos+1], "indiscriminate") || !strcmp(argv[argPos+1], "indiscriminate-fast")) {
                ec.mode = msdfgen::ErrorCorrectionConfig::INDISCRIMINATE;
                ec.distanceCheckMode = msdfgen::ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
            } else if (!strcmp(argv[argPos+1], "distance-full") || !strcmp(argv[argPos+1], "indiscriminate-full")) {
                ec.mode = msdfgen::ErrorCorrectionConfig::INDISCRIMINATE;
                ec.distanceCheckMode = msdfgen::ErrorCorrectionConfig::ALWAYS_CHECK_DISTANCE;
            } else if (!strcmp(argv[argPos+1], "edge-fast")) {
                ec.mode = msdfgen::ErrorCorrectionConfig::EDGE_ONLY;
                ec.distanceCheckMode = msdfgen::ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
            } else if (!strcmp(argv[argPos+1], "edge") || !strcmp(argv[argPos+1], "edge-full")) {
                ec.mode = msdfgen::ErrorCorrectionConfig::EDGE_ONLY;
                ec.distanceCheckMode = msdfgen::ErrorCorrectionConfig::ALWAYS_CHECK_DISTANCE;
            } else if (!strcmp(argv[argPos+1], "help")) {
                puts(errorCorrectionHelpText);
                return 0;
            } else
                ABORT("Unknown error correction mode. Use -errorcorrection help for more information.");
            explicitErrorCorrectionMode = true;
            argPos += 2;
            continue;
        }
        ARG_CASE("-errordeviationratio", 1) {
            double edr;
            if (!(parseDouble(edr, argv[argPos+1]) && edr > 0))
                ABORT("Invalid error deviation ratio. Use -errordeviationratio <ratio> with a positive real number.");
            config.generatorAttributes.config.errorCorrection.minDeviationRatio = edr;
            argPos += 2;
            continue;
        }
        ARG_CASE("-errorimproveratio", 1) {
            double eir;
            if (!(parseDouble(eir, argv[argPos+1]) && eir > 0))
                ABORT("Invalid error improvement ratio. Use -errorimproveratio <ratio> with a positive real number.");
            config.generatorAttributes.config.errorCorrection.minImproveRatio = eir;
            argPos += 2;
            continue;
        }
        ARG_CASE("-coloringstrategy", 1) {
            if (!strcmp(argv[argPos+1], "simple")) config.edgeColoring = msdfgen::edgeColoringSimple, config.expensiveColoring = false;
            else if (!strcmp(argv[argPos+1], "inktrap")) config.edgeColoring = msdfgen::edgeColoringInkTrap, config.expensiveColoring = false;
            else if (!strcmp(argv[argPos+1], "distance")) config.edgeColoring = msdfgen::edgeColoringByDistance, config.expensiveColoring = true;
            else
                fputs("Unknown coloring strategy specified.\n", stderr);
            argPos += 2;
            continue;
        }
        ARG_CASE("-miterlimit", 1) {
            double m;
            if (!(parseDouble(m, argv[++argPos]) && m >= 0))
                ABORT("Invalid miter limit argument. Use -miterlimit <limit> with a positive real number.");
            config.miterLimit = m;
            ++argPos;
            continue;
        }
        ARG_CASE("-nokerning", 0) {
            config.kerning = false;
            ++argPos;
            continue;
        }
        ARG_CASE("-kerning", 0) {
            config.kerning = true;
            ++argPos;
            continue;
        }
        ARG_CASE("-nopreprocess", 0) {
            config.preprocessGeometry = false;
            ++argPos;
            continue;
        }
        ARG_CASE("-preprocess", 0) {
            config.preprocessGeometry = true;
            ++argPos;
            continue;
        }
        ARG_CASE("-nooverlap", 0) {
            config.generatorAttributes.config.overlapSupport = false;
            ++argPos;
            continue;
        }
        ARG_CASE("-overlap", 0) {
            config.generatorAttributes.config.overlapSupport = true;
            ++argPos;
            continue;
        }
        ARG_CASE("-noscanline", 0) {
            config.generatorAttributes.scanlinePass = false;
            ++argPos;
            continue;
        }
        ARG_CASE("-scanline", 0) {
            config.generatorAttributes.scanlinePass = true;
            ++argPos;
            continue;
        }
        ARG_CASE("-seed", 1) {
            if (!parseUnsignedLL(config.coloringSeed, argv[argPos+1]))
                ABORT("Invalid seed. Use -seed <N> with N being a non-negative integer.");
            argPos += 2;
            continue;
        }
        ARG_CASE("-threads", 1) {
            unsigned tc;
            if (!parseUnsigned(tc, argv[argPos+1]) || (int) tc < 0)
                ABORT("Invalid thread count. Use -threads <N> with N being a non-negative integer.");
            config.threadCount = (int) tc;
            argPos += 2;
            continue;
        }
        ARG_CASE("-version", 0) {
            puts(versionText);
            return 0;
        }
        ARG_CASE("-help", 0) {
            puts(helpText);
            return 0;
        }
        fprintf(stderr, "Unknown setting or insufficient parameters: %s\n", argv[argPos]);
        suggestHelp = true;
        ++argPos;
    }
    if (suggestHelp)
        fputs("Use -help for more information.\n", stderr);

    // Nothing to do?
    if (argc == 1) {
        fputs(
            "Usage: msdf-atlas-gen"
            #ifdef _WIN32
                ".exe"
            #endif
            " -font <filename.ttf/otf> -charset <charset> <output specification> <options>\n"
            "Use -help for more information.\n",
            stderr
        );
        return 0;
    }
    if (!fontInput.fontFilename)
        ABORT("No font specified.");
    if (!(config.arteryFontFilename || config.imageFilename || config.jsonFilename || config.csvFilename || config.shadronPreviewFilename)) {
        fputs("No output specified.\n", stderr);
        return 0;
    }
    bool layoutOnly = !(config.arteryFontFilename || config.imageFilename);

    // Finalize font inputs
    const FontInput *nextFontInput = &fontInput;
    for (std::vector<FontInput>::reverse_iterator it = fontInputs.rbegin(); it != fontInputs.rend(); ++it) {
        if (!it->fontFilename && nextFontInput->fontFilename)
            it->fontFilename = nextFontInput->fontFilename;
        if (!it->charsetFilename && nextFontInput->charsetFilename) {
            it->charsetFilename = nextFontInput->charsetFilename;
            it->glyphIdentifierType = nextFontInput->glyphIdentifierType;
        }
        if (it->fontScale < 0 && nextFontInput->fontScale >= 0)
            it->fontScale = nextFontInput->fontScale;
        nextFontInput = &*it;
    }
    if (fontInputs.empty() || memcmp(&fontInputs.back(), &fontInput, sizeof(FontInput)))
        fontInputs.push_back(fontInput);

    // Fix up configuration based on related values
    if (packingStyle == PackingStyle::TIGHT && atlasSizeConstraint == DimensionsConstraint::NONE)
        atlasSizeConstraint = DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE;
    if (!(config.imageType == ImageType::PSDF || config.imageType == ImageType::MSDF || config.imageType == ImageType::MTSDF))
        config.miterLimit = 0;
    if (config.emSize > minEmSize)
        minEmSize = config.emSize;
    if (!(fixedWidth > 0 && fixedHeight > 0) && !(fixedCellWidth > 0 && fixedCellHeight > 0) && !(minEmSize > 0)) {
        fputs("Neither atlas size nor glyph size selected, using default...\n", stderr);
        minEmSize = MSDF_ATLAS_DEFAULT_EM_SIZE;
    }
    if (config.imageType == ImageType::HARD_MASK || config.imageType == ImageType::SOFT_MASK) {
        rangeMode = RANGE_PIXEL;
        rangeValue = 1;
    } else if (rangeValue <= 0) {
        rangeMode = RANGE_PIXEL;
        rangeValue = DEFAULT_PIXEL_RANGE;
    }
    if (config.kerning && !(config.arteryFontFilename || config.jsonFilename || config.shadronPreviewFilename))
        config.kerning = false;
    if (config.threadCount <= 0)
        config.threadCount = std::max((int) std::thread::hardware_concurrency(), 1);
    if (config.generatorAttributes.scanlinePass) {
        if (explicitErrorCorrectionMode && config.generatorAttributes.config.errorCorrection.distanceCheckMode != msdfgen::ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE) {
            const char *fallbackModeName = "unknown";
            switch (config.generatorAttributes.config.errorCorrection.mode) {
                case msdfgen::ErrorCorrectionConfig::DISABLED: fallbackModeName = "disabled"; break;
                case msdfgen::ErrorCorrectionConfig::INDISCRIMINATE: fallbackModeName = "distance-fast"; break;
                case msdfgen::ErrorCorrectionConfig::EDGE_PRIORITY: fallbackModeName = "auto-fast"; break;
                case msdfgen::ErrorCorrectionConfig::EDGE_ONLY: fallbackModeName = "edge-fast"; break;
            }
            fprintf(stderr, "Selected error correction mode not compatible with scanline mode, falling back to %s.\n", fallbackModeName);
        }
        config.generatorAttributes.config.errorCorrection.distanceCheckMode = msdfgen::ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
    }

    // Finalize image format
    ImageFormat imageExtension = ImageFormat::UNSPECIFIED;
    if (config.imageFilename) {
        if (cmpExtension(config.imageFilename, ".png")) imageExtension = ImageFormat::PNG;
        else if (cmpExtension(config.imageFilename, ".bmp")) imageExtension = ImageFormat::BMP;
        else if (cmpExtension(config.imageFilename, ".tif") || cmpExtension(config.imageFilename, ".tiff")) imageExtension = ImageFormat::TIFF;
        else if (cmpExtension(config.imageFilename, ".txt")) imageExtension = ImageFormat::TEXT;
        else if (cmpExtension(config.imageFilename, ".bin")) imageExtension = ImageFormat::BINARY;
    }
    if (config.imageFormat == ImageFormat::UNSPECIFIED) {
        config.imageFormat = ImageFormat::PNG;
        imageFormatName = "png";
        // If image format is not specified and -imageout is the only image output, infer format from its extension
        if (!config.arteryFontFilename) {
            if (imageExtension != ImageFormat::UNSPECIFIED)
                config.imageFormat = imageExtension;
            else if (config.imageFilename)
                fputs("Warning: Could not infer image format from file extension, PNG will be used.\n", stderr);
        }
    }
    if (config.imageType == ImageType::MTSDF && config.imageFormat == ImageFormat::BMP)
        ABORT("Atlas type not compatible with image format. MTSDF requires a format with alpha channel.");
#ifndef MSDF_ATLAS_NO_ARTERY_FONT
    if (config.arteryFontFilename && !(config.imageFormat == ImageFormat::PNG || config.imageFormat == ImageFormat::BINARY || config.imageFormat == ImageFormat::BINARY_FLOAT)) {
        config.arteryFontFilename = nullptr;
        result = 1;
        fputs("Error: Unable to create an Artery Font file with the specified image format!\n", stderr);
        // Recheck whether there is anything else to do
        if (!(config.arteryFontFilename || config.imageFilename || config.jsonFilename || config.csvFilename || config.shadronPreviewFilename))
            return result;
        layoutOnly = !(config.arteryFontFilename || config.imageFilename);
    }
#endif
    if (imageExtension != ImageFormat::UNSPECIFIED) {
        // Warn if image format mismatches -imageout extension
        bool mismatch = false;
        switch (config.imageFormat) {
            case ImageFormat::TEXT: case ImageFormat::TEXT_FLOAT:
                mismatch = imageExtension != ImageFormat::TEXT;
                break;
            case ImageFormat::BINARY: case ImageFormat::BINARY_FLOAT: case ImageFormat::BINARY_FLOAT_BE:
                mismatch = imageExtension != ImageFormat::BINARY;
                break;
            default:
                mismatch = imageExtension != config.imageFormat;
        }
        if (mismatch)
            fprintf(stderr, "Warning: Output image file extension does not match the image's actual format (%s)!\n", imageFormatName);
    }
    imageFormatName = nullptr; // No longer consistent with imageFormat
    bool floatingPointFormat = (
        config.imageFormat == ImageFormat::TIFF ||
        config.imageFormat == ImageFormat::TEXT_FLOAT ||
        config.imageFormat == ImageFormat::BINARY_FLOAT ||
        config.imageFormat == ImageFormat::BINARY_FLOAT_BE
    );
    // TODO: In this case (if spacing is -1), the border pixels of each glyph are black, but still computed. For floating-point output, this may play a role.
    int spacing = config.imageType == ImageType::MSDF || config.imageType == ImageType::MTSDF ? 0 : -1;
    double uniformOriginX, uniformOriginY;

    // Load fonts
    std::vector<GlyphGeometry> glyphs;
    std::vector<FontGeometry> fonts;
    bool anyCodepointsAvailable = false;
    {
        class FontHolder {
            msdfgen::FreetypeHandle *ft;
            msdfgen::FontHandle *font;
            const char *fontFilename;
        public:
            FontHolder() : ft(msdfgen::initializeFreetype()), font(nullptr), fontFilename(nullptr) { }
            ~FontHolder() {
                if (ft) {
                    if (font)
                        msdfgen::destroyFont(font);
                    msdfgen::deinitializeFreetype(ft);
                }
            }
            bool load(const char *fontFilename, bool isVarFont) {
                if (ft && fontFilename) {
                    if (this->fontFilename && !strcmp(this->fontFilename, fontFilename))
                        return true;
                    if (font)
                        msdfgen::destroyFont(font);
                    if ((font = isVarFont ? loadVarFont(ft, fontFilename) : msdfgen::loadFont(ft, fontFilename))) {
                        this->fontFilename = fontFilename;
                        return true;
                    }
                    this->fontFilename = nullptr;
                }
                return false;
            }
            operator msdfgen::FontHandle *() const {
                return font;
            }
        } font;

        for (FontInput &fontInput : fontInputs) {
            if (!font.load(fontInput.fontFilename, fontInput.variableFont))
                ABORT("Failed to load specified font file.");
            if (fontInput.fontScale <= 0)
                fontInput.fontScale = 1;

            // Load character set
            Charset charset;
            unsigned allGlyphCount = 0;
            if (fontInput.charsetFilename) {
                if (!charset.load(fontInput.charsetFilename, fontInput.glyphIdentifierType != GlyphIdentifierType::UNICODE_CODEPOINT))
                    ABORT(fontInput.glyphIdentifierType == GlyphIdentifierType::GLYPH_INDEX ? "Failed to load glyph set specification." : "Failed to load character set specification.");
            } else if (fontInput.glyphIdentifierType == GlyphIdentifierType::GLYPH_INDEX)
                msdfgen::getGlyphCount(allGlyphCount, font);
            else
                charset = Charset::ASCII;

            // Load glyphs
            FontGeometry fontGeometry(&glyphs);
            int glyphsLoaded = -1;
            switch (fontInput.glyphIdentifierType) {
                case GlyphIdentifierType::GLYPH_INDEX:
                    if (allGlyphCount)
                        glyphsLoaded = fontGeometry.loadGlyphRange(font, fontInput.fontScale, 0, allGlyphCount, config.preprocessGeometry, config.kerning);
                    else
                        glyphsLoaded = fontGeometry.loadGlyphset(font, fontInput.fontScale, charset, config.preprocessGeometry, config.kerning);
                    break;
                case GlyphIdentifierType::UNICODE_CODEPOINT:
                    glyphsLoaded = fontGeometry.loadCharset(font, fontInput.fontScale, charset, config.preprocessGeometry, config.kerning);
                    anyCodepointsAvailable |= glyphsLoaded > 0;
                    break;
            }
            if (glyphsLoaded < 0)
                ABORT("Failed to load glyphs from font.");
            printf("Loaded geometry of %d out of %d glyphs", glyphsLoaded, (int) (allGlyphCount+charset.size()));
            if (fontInputs.size() > 1)
                printf(" from font \"%s\"", fontInput.fontFilename);
            printf(".\n");
            // List missing glyphs
            if (glyphsLoaded < (int) charset.size()) {
                fprintf(stderr, "Missing %d %s", (int) charset.size()-glyphsLoaded, fontInput.glyphIdentifierType == GlyphIdentifierType::UNICODE_CODEPOINT ? "codepoints" : "glyphs");
                bool first = true;
                switch (fontInput.glyphIdentifierType) {
                    case GlyphIdentifierType::GLYPH_INDEX:
                        for (unicode_t cp : charset)
                            if (!fontGeometry.getGlyph(msdfgen::GlyphIndex(cp)))
                                fprintf(stderr, "%c 0x%02X", first ? ((first = false), ':') : ',', cp);
                        break;
                    case GlyphIdentifierType::UNICODE_CODEPOINT:
                        for (unicode_t cp : charset)
                            if (!fontGeometry.getGlyph(cp))
                                fprintf(stderr, "%c 0x%02X", first ? ((first = false), ':') : ',', cp);
                        break;
                }
                fprintf(stderr, "\n");
            } else if (glyphsLoaded < (int) allGlyphCount) {
                fprintf(stderr, "Missing %d glyphs", (int) allGlyphCount-glyphsLoaded);
                bool first = true;
                for (unsigned i = 0; i < allGlyphCount; ++i)
                    if (!fontGeometry.getGlyph(msdfgen::GlyphIndex(i)))
                        fprintf(stderr, "%c 0x%02X", first ? ((first = false), ':') : ',', i);
                fprintf(stderr, "\n");
            }

            if (fontInput.fontName)
                fontGeometry.setName(fontInput.fontName);

            fonts.push_back((FontGeometry &&) fontGeometry);
        }
    }
    if (glyphs.empty())
        ABORT("No glyphs loaded.");

    // Determine final atlas dimensions, scale and range, pack glyphs
    {
        double unitRange = 0, pxRange = 0;
        switch (rangeMode) {
            case RANGE_EM:
                unitRange = rangeValue;
                break;
            case RANGE_PIXEL:
                pxRange = rangeValue;
                break;
        }
        bool fixedDimensions = fixedWidth >= 0 && fixedHeight >= 0;
        bool fixedScale = config.emSize > 0;
        switch (packingStyle) {

            case PackingStyle::TIGHT: {
                TightAtlasPacker atlasPacker;
                if (fixedDimensions)
                    atlasPacker.setDimensions(fixedWidth, fixedHeight);
                else
                    atlasPacker.setDimensionsConstraint(atlasSizeConstraint);
                atlasPacker.setSpacing(spacing);
                if (fixedScale)
                    atlasPacker.setScale(config.emSize);
                else
                    atlasPacker.setMinimumScale(minEmSize);
                atlasPacker.setPixelRange(pxRange);
                atlasPacker.setUnitRange(unitRange);
                atlasPacker.setMiterLimit(config.miterLimit);
                atlasPacker.setOriginPixelAlignment(config.pxAlignOriginX, config.pxAlignOriginY);
                if (int remaining = atlasPacker.pack(glyphs.data(), glyphs.size())) {
                    if (remaining < 0) {
                        ABORT("Failed to pack glyphs into atlas.");
                    } else {
                        fprintf(stderr, "Error: Could not fit %d out of %d glyphs into the atlas.\n", remaining, (int) glyphs.size());
                        return 1;
                    }
                }
                atlasPacker.getDimensions(config.width, config.height);
                if (!(config.width > 0 && config.height > 0))
                    ABORT("Unable to determine atlas size.");
                config.emSize = atlasPacker.getScale();
                config.pxRange = atlasPacker.getPixelRange();
                if (!fixedScale)
                    printf("Glyph size: %.9g pixels/em\n", config.emSize);
                if (!fixedDimensions)
                    printf("Atlas dimensions: %d x %d\n", config.width, config.height);
                break;
            }

            case PackingStyle::GRID: {
                GridAtlasPacker atlasPacker;
                atlasPacker.setFixedOrigin(config.grid.fixedOriginX, config.grid.fixedOriginY);
                if (fixedCellWidth >= 0 && fixedCellHeight >= 0)
                    atlasPacker.setCellDimensions(fixedCellWidth, fixedCellHeight);
                else
                    atlasPacker.setCellDimensionsConstraint(cellSizeConstraint);
                if (config.grid.cols > 0)
                    atlasPacker.setColumns(config.grid.cols);
                if (fixedDimensions)
                    atlasPacker.setDimensions(fixedWidth, fixedHeight);
                else
                    atlasPacker.setDimensionsConstraint(atlasSizeConstraint);
                atlasPacker.setSpacing(spacing);
                if (fixedScale)
                    atlasPacker.setScale(config.emSize);
                else
                    atlasPacker.setMinimumScale(minEmSize);
                atlasPacker.setPixelRange(pxRange);
                atlasPacker.setUnitRange(unitRange);
                atlasPacker.setMiterLimit(config.miterLimit);
                atlasPacker.setOriginPixelAlignment(config.pxAlignOriginX, config.pxAlignOriginY);
                if (int remaining = atlasPacker.pack(glyphs.data(), glyphs.size())) {
                    if (remaining < 0) {
                        ABORT("Failed to pack glyphs into atlas.");
                    } else {
                        fprintf(stderr, "Error: Could not fit %d out of %d glyphs into the atlas.\n", remaining, (int) glyphs.size());
                        return 1;
                    }
                }
                if (atlasPacker.hasCutoff())
                    fputs("Warning: Grid cell too constrained to fully fit all glyphs, some may be cut off!\n", stderr);
                atlasPacker.getDimensions(config.width, config.height);
                if (!(config.width > 0 && config.height > 0))
                    ABORT("Unable to determine atlas size.");
                config.emSize = atlasPacker.getScale();
                config.pxRange = atlasPacker.getPixelRange();
                atlasPacker.getCellDimensions(config.grid.cellWidth, config.grid.cellHeight);
                config.grid.cols = atlasPacker.getColumns();
                config.grid.rows = atlasPacker.getRows();
                if (!fixedScale)
                    printf("Glyph size: %.9g pixels/em\n", config.emSize);
                if (config.grid.fixedOriginX || config.grid.fixedOriginY) {
                    atlasPacker.getFixedOrigin(uniformOriginX, uniformOriginY);
                    printf("Grid cell origin: ");
                    if (config.grid.fixedOriginX)
                        printf("X = %.9g", uniformOriginX);
                    if (config.grid.fixedOriginX && config.grid.fixedOriginY)
                        printf(", ");
                    if (config.grid.fixedOriginY) {
                        switch (config.yDirection) {
                            case YDirection::BOTTOM_UP:
                                printf("Y = %.9g", uniformOriginY);
                                break;
                            case YDirection::TOP_DOWN:
                                printf("Y = %.9g", (config.grid.cellHeight-spacing-1)/config.emSize-uniformOriginY);
                                break;
                        }
                    }
                    printf("\n");
                }
                printf("Grid cell dimensions: %d x %d\n", config.grid.cellWidth, config.grid.cellHeight);
                printf("Atlas dimensions: %d x %d (%d columns x %d rows)\n", config.width, config.height, config.grid.cols, config.grid.rows);
                break;
            }

        }
    }

    // Generate atlas bitmap
    if (!layoutOnly) {

        // Edge coloring
        if (config.imageType == ImageType::MSDF || config.imageType == ImageType::MTSDF) {
            if (config.expensiveColoring) {
                Workload([&glyphs, &config](int i, int threadNo) -> bool {
                    unsigned long long glyphSeed = (LCG_MULTIPLIER*(config.coloringSeed^i)+LCG_INCREMENT)*!!config.coloringSeed;
                    glyphs[i].edgeColoring(config.edgeColoring, config.angleThreshold, glyphSeed);
                    return true;
                }, glyphs.size()).finish(config.threadCount);
            } else {
                unsigned long long glyphSeed = config.coloringSeed;
                for (GlyphGeometry &glyph : glyphs) {
                    glyphSeed *= LCG_MULTIPLIER;
                    glyph.edgeColoring(config.edgeColoring, config.angleThreshold, glyphSeed);
                }
            }
        }

        bool success = false;
        switch (config.imageType) {
            case ImageType::HARD_MASK:
                if (floatingPointFormat)
                    success = makeAtlas<float, float, 1, scanlineGenerator>(glyphs, fonts, config);
                else
                    success = makeAtlas<byte, float, 1, scanlineGenerator>(glyphs, fonts, config);
                break;
            case ImageType::SOFT_MASK:
            case ImageType::SDF:
                if (floatingPointFormat)
                    success = makeAtlas<float, float, 1, sdfGenerator>(glyphs, fonts, config);
                else
                    success = makeAtlas<byte, float, 1, sdfGenerator>(glyphs, fonts, config);
                break;
            case ImageType::PSDF:
                if (floatingPointFormat)
                    success = makeAtlas<float, float, 1, psdfGenerator>(glyphs, fonts, config);
                else
                    success = makeAtlas<byte, float, 1, psdfGenerator>(glyphs, fonts, config);
                break;
            case ImageType::MSDF:
                if (floatingPointFormat)
                    success = makeAtlas<float, float, 3, msdfGenerator>(glyphs, fonts, config);
                else
                    success = makeAtlas<byte, float, 3, msdfGenerator>(glyphs, fonts, config);
                break;
            case ImageType::MTSDF:
                if (floatingPointFormat)
                    success = makeAtlas<float, float, 4, mtsdfGenerator>(glyphs, fonts, config);
                else
                    success = makeAtlas<byte, float, 4, mtsdfGenerator>(glyphs, fonts, config);
                break;
        }
        if (!success)
            result = 1;
    }

    if (config.csvFilename) {
        if (exportCSV(fonts.data(), fonts.size(), config.width, config.height, config.yDirection, config.csvFilename))
            fputs("Glyph layout written into CSV file.\n", stderr);
        else {
            result = 1;
            fputs("Failed to write CSV output file.\n", stderr);
        }
    }

    if (config.jsonFilename) {
        JsonAtlasMetrics jsonMetrics = { };
        JsonAtlasMetrics::GridMetrics gridMetrics = { };
        jsonMetrics.distanceRange = config.pxRange;
        jsonMetrics.size = config.emSize;
        jsonMetrics.width = config.width, jsonMetrics.height = config.height;
        jsonMetrics.yDirection = config.yDirection;
        if (packingStyle == PackingStyle::GRID) {
            gridMetrics.cellWidth = config.grid.cellWidth, gridMetrics.cellHeight = config.grid.cellHeight;
            gridMetrics.columns = config.grid.cols, gridMetrics.rows = config.grid.rows;
            if (config.grid.fixedOriginX)
                gridMetrics.originX = &uniformOriginX;
            if (config.grid.fixedOriginY)
                gridMetrics.originY = &uniformOriginY;
            gridMetrics.spacing = spacing;
            jsonMetrics.grid = &gridMetrics;
        }
        if (exportJSON(fonts.data(), fonts.size(), config.imageType, jsonMetrics, config.jsonFilename, config.kerning))
            fputs("Glyph layout and metadata written into JSON file.\n", stderr);
        else {
            result = 1;
            fputs("Failed to write JSON output file.\n", stderr);
        }
    }

    if (config.shadronPreviewFilename && config.shadronPreviewText) {
        if (anyCodepointsAvailable) {
            std::vector<unicode_t> previewText;
            utf8Decode(previewText, config.shadronPreviewText);
            previewText.push_back(0);
            if (generateShadronPreview(fonts.data(), fonts.size(), config.imageType, config.width, config.height, config.pxRange, previewText.data(), config.imageFilename, floatingPointFormat, config.shadronPreviewFilename))
                fputs("Shadron preview script generated.\n", stderr);
            else {
                result = 1;
                fputs("Failed to generate Shadron preview file.\n", stderr);
            }
        } else {
            result = 1;
            fputs("Shadron preview not supported in -glyphset mode.\n", stderr);
        }
    }

    return result;
}

#endif
