/*
 * cAvatar - a simple avatar generator in C inspired by Identicon.
 * 
 * This program reads a text seed from seed.txt and generates unique 
 * identicon-style avatars based on that seed and a user-provided name.
 *
 * Usage:
 *   ./a.out
 *    ^ - interactive cli mode 
 *   ./a.out Name
 *   ^ - generates Name.ppm
 *   ./a.out Name type(int>3 && int< <=1024) size(int>0)
 *  ^ - generates Name.ppm with type*type cells and type*size pixels large image 
 * 
 * CLI mode:
 *   when ran as just `./a.out`, it will ask for the following things:
 *       - avatar count or filename
 *       - grid size - how many unique pixels per side (originally the Identicon is 5)
 *       - size multiplier - how many pixels per cell in the output image
 *
 * The generated images are in PPM format, which can be easily converted to PNG/JPEG without loss of quality.
 *
 * License: GPL-2.0 - Gyurkovics Dominik
 * 2026-04-22
 *
*/

#include <ctype.h>
#include <errno.h>

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// For mkdir() and EEXIST
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_LINE 1024
#define MAX_NAME 256

#define DEFAULT_GRID 5
#define DEFAULT_MULTIPLIER 100

#define SEED_FILE "seed.txt"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

/*
 * trim_whitespace - strip leading and trailing whitespace from a string.
 * https://stackoverflow.com/q/122616
 */
static void trim_whitespace(char *s) {
    size_t start = 0;
    size_t end = strlen(s);

    while (start < end && isspace((unsigned char)s[start])) start++;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;

    if (start > 0) memmove(s, s + start, end - start);
    s[end - start] = '\0';
}

/*
 * read_seed - load the text seed from # defined seed file 
 * The file must exist and not be empty.
 */
static bool read_seed(char *out, size_t out_size) {
    FILE *f = fopen(SEED_FILE, "r");
    if (!f) return false;

    if (!fgets(out, (int)out_size, f)) {
        fclose(f);
        return false;
    }
    fclose(f);

    trim_whitespace(out);
    return out[0] != '\0';
}

/*
 * parse_positive_int - parse a decimal integer from a string.
 * Uses strtol() to verify the entire string is numeric.
 * See: https://en.cppreference.com/w/c/string/byte/strtol
 */
static bool parse_positive_int(const char *s, int *value) {
    char *end = NULL;
    long v;

    if (!s || !*s) return false;

    errno = 0;
    v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return false;
    if (v <= 0 || v > 10000) return false;

    *value = (int)v;
    return true;
}

/*
 * fnv1a_64 - compute 64-bit FNV-1a hash for a string.
 * See: https://www.wikiwand.com/en/Fowler-Noll-Vo_hash_function
 */
static uint64_t fnv1a_64(const char *s) {
    const uint64_t FNV_OFFSET = 1469598103934665603ULL;
    const uint64_t FNV_PRIME = 1099511628211ULL;
    uint64_t hash = FNV_OFFSET;

    while (*s) {
        hash ^= (uint8_t)(*s++);
        hash *= FNV_PRIME;
    }
    return hash;
}

/*
 * mix_seed_and_name - deterministic mixing of seed and name.
 * We use FNV-1a on seed + delimiter + name for simplicity and repeatability.
 */
static uint64_t mix_seed_and_name(const char *seed, const char *name) {
    uint64_t hash = 1469598103934665603ULL;

    while (*seed) {
        hash ^= (uint8_t)*seed++;
        hash *= 1099511628211ULL;
    }
    hash ^= (uint8_t)0;
    hash *= 1099511628211ULL;
    while (*name) {
        hash ^= (uint8_t)*name++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

/*
 * splitmix64_next - simple pseudorandom integer generator for internal use.
 * See: https://en.wikipedia.org/wiki/SplitMix64
 */
static uint64_t splitmix64_next(uint64_t *state) {
    uint64_t z;

    *state += 0x9e3779b97f4a7c15ULL;
    z = *state;
    z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31U);
}

/*
 * sanitize_filename - make a safe file name from arbitrary input.
 * This prevents spaces, slashes, and other unsafe characters from appearing in filenames.
 */
static void sanitize_filename(const char *name, char *out, size_t out_size) {
    size_t i;
    size_t j = 0;

    for (i = 0; name[i] != '\0' && j + 1 < out_size; i++) {
        unsigned char c = (unsigned char)name[i];
        if (isalnum(c) || c == '-' || c == '_') {
            out[j++] = (char)c;
        } else if (isspace(c) || c == '.') {
            out[j++] = '_';
        }
    }

    if (j == 0) {
        strncpy(out, "avatar", out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    out[j] = '\0';
}

/*
 * ensure_output_dir - create the generated directory if it does not exist.
 */
static bool ensure_output_dir(void) {
    int rc = mkdir("generated", 0755);
    return rc == 0 || errno == EEXIST;
}

/*
 * is_valid_grid - validate that the grid size is allowed.
 * We require at least 3 cells per side -- in order to avoid different pixel write errors.
 * 1024 maximum to prevent excessive memory usage.
 */
static bool is_valid_grid(int grid) {
    return grid >= 3 && grid <= 1024;
}

/*
 * generate_colors - choose one background and two similar foreground colors using simple rgb.
 */
static void generate_colors(uint64_t *rng, Color *bg, Color *fg1, Color *fg2) {
    bg->r = (uint8_t)(40 + (splitmix64_next(rng) % 60));
    bg->g = (uint8_t)(40 + (splitmix64_next(rng) % 60));
    bg->b = (uint8_t)(40 + (splitmix64_next(rng) % 60));

    uint8_t base_r = (uint8_t)(130 + (splitmix64_next(rng) % 80));
    uint8_t base_g = (uint8_t)(50 + (splitmix64_next(rng) % 80));
    uint8_t base_b = (uint8_t)(50 + (splitmix64_next(rng) % 80));

    fg1->r = base_r;
    fg1->g = base_g;
    fg1->b = base_b;

    fg2->r = (uint8_t)((int)base_r + (int)(splitmix64_next(rng) % 31) - 15);
    fg2->g = (uint8_t)((int)base_g + (int)(splitmix64_next(rng) % 31) - 15);
    fg2->b = (uint8_t)((int)base_b + (int)(splitmix64_next(rng) % 31) - 15);
}

/*
 * fill_background - paint every pixel with the same background color.
 */
static void fill_background(uint8_t *pixels, int width, int height, Color bg) {
    size_t pixel_count = (size_t)width * (size_t)height * 3U;
    for (size_t i = 0; i < pixel_count; i += 3U) {
        pixels[i + 0] = bg.r;
        pixels[i + 1] = bg.g;
        pixels[i + 2] = bg.b;
    }
}

/*
 * make_output_filename - turn a name into a sanitized filename with .ppm suffix.
 */
static void make_output_filename(const char *name, char *out_path, size_t out_size) {
    char safe_name[MAX_NAME];
    sanitize_filename(name, safe_name, sizeof(safe_name));
    snprintf(out_path, out_size, "generated/%s.ppm", safe_name);
}

/*
 * write_ppm - write raw RGB pixels to a PPM (P6 - Portable Bitmap Format) file.
 * This is a very simple uncompressed image format supported by many tools.
 *
 * TODO: choice for PNG/JPEG/BMP with compression support
 */
static bool write_ppm(const char *path, const uint8_t *pixels, int width, int height) {
    FILE *f = fopen(path, "wb");
    size_t count;

    if (!f) return false;

    fprintf(f, "P6\n%d %d\n255\n", width, height);
    count = fwrite(pixels, 1, (size_t)width * (size_t)height * 3U, f);
    fclose(f);

    return count == (size_t)width * (size_t)height * 3U;
}

/*
 * generate_symmetry - fill the grid with mirror-symmetric active cells.
 * This makes the identicon visually balanced.
 */
static void generate_symmetry(uint64_t *rng, bool *cells, int grid, int *active_count) {
    int half = (grid + 1) / 2;
    *active_count = 0;

    for (int y = 0; y < grid; y++) {
        for (int x = 0; x < half; x++) {
            bool on = ((splitmix64_next(rng) & 0x3ULL) != 0ULL);
            int left = y * grid + x;
            int right = y * grid + (grid - 1 - x);

            cells[left] = on;
            cells[right] = on;
            if (on) *active_count += (left == right) ? 1 : 2;
        }
    }
}

/*
 * paint_cells - write each active grid cell as a solid color block.
 * Uses fg1 and fg2 alternately so the icon has two similar foreground tones.
 */
static void paint_cells(uint8_t *pixels, int width, int grid, int multiplier, bool *cells, uint64_t *rng, Color fg1, Color fg2) {
    for (int y = 0; y < grid; y++) {
        for (int x = 0; x < grid; x++) {
            if (!cells[y * grid + x]) continue;

            int sx = x * multiplier;
            int sy = y * multiplier;
            int ex = sx + multiplier;
            int ey = sy + multiplier;
            Color fg = ((splitmix64_next(rng) & 1ULL) == 0ULL) ? fg1 : fg2;

            for (int py = sy; py < ey; py++) {
                for (int px = sx; px < ex; px++) {
                    size_t idx = ((size_t)py * (size_t)width + (size_t)px) * 3U;
                    pixels[idx + 0] = fg.r;
                    pixels[idx + 1] = fg.g;
                    pixels[idx + 2] = fg.b;
                }
            }
        }
    }
}

/*
 * generate_identicon - create one identicon from seed and name.
 * Writes a PPM file named <sanitized-name>.ppm.
 */
static bool generate_identicon(const char *seed, const char *name, int grid, int multiplier) {
    int width = grid * multiplier;
    int height = grid * multiplier;
    size_t pixel_count = (size_t)width * (size_t)height * 3U;
    uint8_t *pixels = NULL;
    bool *cells = NULL;
    uint64_t rng;
    int active_count = 0;
    Color bg;
    Color fg1, fg2;
    char out_path[512];

    if (!is_valid_grid(grid) || multiplier < 1) return false;

    pixels = (uint8_t *)malloc(pixel_count);
    cells = (bool *)calloc((size_t)grid * (size_t)grid, sizeof(bool));
    if (!pixels || !cells) {
        free(pixels);
        free(cells);
        return false;
    }

    rng = mix_seed_and_name(seed, name);
    generate_colors(&rng, &bg, &fg1, &fg2);
    generate_symmetry(&rng, cells, grid, &active_count);

    if (active_count == 0) {
        int center = (grid / 2) * grid + (grid / 2);
        cells[center] = true;
    }

    fill_background(pixels, width, height, bg);
    paint_cells(pixels, width, grid, multiplier, cells, &rng, fg1, fg2);

    make_output_filename(name, out_path, sizeof(out_path));

    if (!write_ppm(out_path, pixels, width, height)) {
        free(pixels);
        free(cells);
        return false;
    }

    printf("Generated: %s\n", out_path);

    free(pixels);
    free(cells);
    return true;
}

/*
 * read_names_from_file - read one name per line from a text file.
 * Returns an array of heap-allocated strings, or NULL on failure.
 */
static char **read_names_from_file(const char *filename, int *out_count) {
    FILE *f = fopen(filename, "r");
    char **names = NULL;
    int count = 0;
    int cap = 0;
    char line[MAX_LINE];

    if (!f) return NULL;

    while (fgets(line, sizeof(line), f)) {
        trim_whitespace(line);
        if (line[0] == '\0') continue;

        if (count == cap) {
            int new_cap = cap ? cap * 2 : 8;
            char **tmp = realloc(names, (size_t)new_cap * sizeof *tmp);
            if (!tmp) goto fail;
            names = tmp;
            cap = new_cap;
        }

        names[count] = strdup(line);
        if (!names[count]) goto fail;
        count++;
    }

    fclose(f);
    *out_count = count;
    return names;

fail:
    if (f) fclose(f);
    while (count--) free(names[count]);
    free(names);
    return NULL;
}

/*
 * free_names - free the string array returned by read_names_from_file.
 */
static void free_names(char **names, int count) {
    if (!names) return;
    for (int i = 0; i < count; i++) free(names[i]);
    free(names);
}

/*
 * run_interactive - interactive batch mode for name count or filename input.
 */
static int run_interactive(const char *seed) {
    char input[MAX_LINE];
    char **names = NULL;
    int count = 0;
    int grid;
    int multiplier;

    printf("How many avatars do you need or give a text filename: ");
    if (!fgets(input, sizeof(input), stdin)) {
        return 1;
    }
    trim_whitespace(input);

    if (parse_positive_int(input, &count)) {
        names = (char **)calloc((size_t)count, sizeof(char *));
        if (!names) {
            fprintf(stderr, "Memory allocation error.\n");
            return 1;
        }

        {
            uint64_t rnd = fnv1a_64(seed) ^ (uint64_t)count;
            const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
            for (int i = 0; i < count; i++) {
                int len = 6 + (int)(splitmix64_next(&rnd) % 3);
                char temp[MAX_NAME];
                for (int j = 0; j < len && j + 1 < (int)sizeof(temp); j++) {
                    temp[j] = chars[splitmix64_next(&rnd) % (sizeof(chars) - 1)];
                }
                temp[len] = '\0';
                names[i] = strdup(temp);
                if (!names[i]) {
                    free_names(names, count);
                    fprintf(stderr, "Memory allocation error.\n");
                    return 1;
                }
            }
        }
    } else {
        names = read_names_from_file(input, &count);
        if (!names) {
            fprintf(stderr, "Could not read names from file: %s\n", input);
            return 1;
        }
        if (count == 0) {
            fprintf(stderr, "The file does not contain any names.\n");
            free_names(names, count);
            return 1;
        }
    }

    printf("Will generate %d image(s).\n", count);

    printf("Type (grid size, example: 7 for 7x7): ");
    if (!fgets(input, sizeof(input), stdin)) {
        fprintf(stderr, "Invalid type value.\n");
        free_names(names, count);
        return 1;
    }
    trim_whitespace(input);
    if (!parse_positive_int(input, &grid) || !is_valid_grid(grid)) {
        fprintf(stderr, "Invalid type value. Must be 3 or higher.\n");
        free_names(names, count);
        return 1;
    }

    printf("Size multiplier (example: 100 -> final size %d x %d): ", grid * 100, grid * 100);
    if (!fgets(input, sizeof(input), stdin)) {
        fprintf(stderr, "Invalid size multiplier value.\n");
        free_names(names, count);
        return 1;
    }
    trim_whitespace(input);
    if (!parse_positive_int(input, &multiplier)) {
        fprintf(stderr, "Invalid size multiplier value.\n");
        free_names(names, count);
        return 1;
    }

    for (int i = 0; i < count; i++) {
        if (!generate_identicon(seed, names[i], grid, multiplier)) fprintf(stderr, "Failed to generate avatar for %s\n", names[i]);
    }

    free_names(names, count);
    return 0;
}

/*
 * main - entry point for cAvatar.
 * Supports zero arguments for interactive mode and one or three arguments for CLI mode.
 */
int main(int argc, char **argv) {
    char seed[MAX_LINE];

    if (!read_seed(seed, sizeof(seed))) {
        fprintf(stderr, "Could not read seed from file (must exist and not be empty!)\n");
        return 1;
    }

    if (!ensure_output_dir()) {
        fprintf(stderr, "Could not create/access generated directory.\n");
        return 1;
    }

    if (argc == 1) return run_interactive(seed);

    if (argc == 2) {
        if (!generate_identicon(seed, argv[1], DEFAULT_GRID, DEFAULT_MULTIPLIER)) {
            fprintf(stderr, "Generation failed.\n");
            return 1;
        } return 0;
    }

    if (argc == 4) {
        int grid, multiplier;
        if (!parse_positive_int(argv[2], &grid) || !is_valid_grid(grid) || !parse_positive_int(argv[3], &multiplier)) {
            fprintf(stderr, "Invalid arguments. Usage: ./cavatar.out Name [type size]\n");
            return 1;
        }

        if (!generate_identicon(seed, argv[1], grid, multiplier)) {
            fprintf(stderr, "Generation failed.\n");
            return 1;
        } return 0;
    }
    return 1;
}
