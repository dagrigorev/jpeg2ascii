// main.cpp – Fixed JPEG decoder (no out-of-bounds)
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <string_view>
#include <cstring>

// -------------------------------------------------------------------
// Constants
// -------------------------------------------------------------------
constexpr int MAX_CONSOLE_WIDTH = 120;
constexpr int MAX_CONSOLE_HEIGHT = 40;
constexpr int BLOCK_SIZE = 8;
constexpr double PI = 3.14159265358979323846;

// -------------------------------------------------------------------
// Helper: read big-endian 16-bit
// -------------------------------------------------------------------
inline uint16_t read_be16(std::ifstream& file) {
    uint8_t b1, b2;
    file.read(reinterpret_cast<char*>(&b1), 1);
    file.read(reinterpret_cast<char*>(&b2), 1);
    return (static_cast<uint16_t>(b1) << 8) | b2;
}

// -------------------------------------------------------------------
// JPEG marker IDs
// -------------------------------------------------------------------
constexpr uint16_t MARKER_SOI = 0xFFD8;
constexpr uint16_t MARKER_APP0 = 0xFFE0;
constexpr uint16_t MARKER_DQT = 0xFFDB;
constexpr uint16_t MARKER_SOF0 = 0xFFC0;
constexpr uint16_t MARKER_DHT = 0xFFC4;
constexpr uint16_t MARKER_SOS = 0xFFDA;
constexpr uint16_t MARKER_EOI = 0xFFD9;

// -------------------------------------------------------------------
// Huffman table
// -------------------------------------------------------------------
struct HuffTable {
    std::array<uint8_t, 16> bits = {};
    std::vector<uint8_t> symbols;
    std::vector<uint16_t> codes;
    std::vector<uint8_t> sizes;
    bool valid = false;

    void build() {
        codes.clear();
        sizes.clear();
        uint16_t code = 0;
        for (int len = 1; len <= 16; ++len) {
            for (int i = 0; i < bits[len - 1]; ++i) {
                codes.push_back(code);
                sizes.push_back(static_cast<uint8_t>(len));
                ++code;
            }
            code <<= 1;
        }
    }

    uint8_t lookup(uint16_t code, uint8_t len) const {
        for (size_t i = 0; i < codes.size(); ++i) {
            if (sizes[i] == len && codes[i] == code)
                return symbols[i];
        }
        return 0; // should not happen
    }
};

// -------------------------------------------------------------------
// Quantization table
// -------------------------------------------------------------------
struct QuantTable {
    std::array<std::array<uint16_t, BLOCK_SIZE>, BLOCK_SIZE> data = {};
    bool valid = false;
};

// -------------------------------------------------------------------
// Bit reader (handles byte stuffing)
// -------------------------------------------------------------------
class BitReader {
    std::ifstream& file;
    uint8_t buffer = 0;
    int bits_left = 0;
public:
    BitReader(std::ifstream& f) : file(f) {}

    uint8_t get_byte() {
        uint8_t b;
        file.read(reinterpret_cast<char*>(&b), 1);
        if (b == 0xFF) {
            uint8_t next;
            file.read(reinterpret_cast<char*>(&next), 1);
            if (next != 0x00) {
                throw std::runtime_error("Unexpected marker in bitstream");
            }
        }
        return b;
    }

    uint16_t read_bits(int n) {
        uint16_t val = 0;
        while (n > 0) {
            if (bits_left == 0) {
                buffer = get_byte();
                bits_left = 8;
            }
            int take = std::min(n, bits_left);
            val = (val << take) | ((buffer >> (bits_left - take)) & ((1 << take) - 1));
            bits_left -= take;
            n -= take;
        }
        return val;
    }

    void align_byte() {
        bits_left = 0;
    }
};

// -------------------------------------------------------------------
// Huffman decoder
// -------------------------------------------------------------------
uint8_t read_huff_symbol(BitReader& reader, const HuffTable& table) {
    uint16_t code = 0;
    for (int len = 1; len <= 16; ++len) {
        code = (code << 1) | reader.read_bits(1);
        for (size_t i = 0; i < table.codes.size(); ++i) {
            if (table.sizes[i] == len && table.codes[i] == code) {
                return table.symbols[i];
            }
        }
    }
    throw std::runtime_error("Huffman code not found");
}

// -------------------------------------------------------------------
// 1D IDCT
// -------------------------------------------------------------------
void idct_1d(const double in[8], double out[8]) {
    double tmp[8];
    for (int i = 0; i < 8; ++i) {
        double sum = 0.0;
        for (int j = 0; j < 8; ++j) {
            double c = (j == 0) ? 1.0 / std::sqrt(2.0) : 1.0;
            sum += c * in[j] * std::cos(PI * j * (2 * i + 1) / 16.0);
        }
        tmp[i] = sum * 0.5;
    }
    for (int i = 0; i < 8; ++i) out[i] = tmp[i];
}

void idct_2d(std::array<std::array<double, BLOCK_SIZE>, BLOCK_SIZE>& block) {
    double temp[BLOCK_SIZE][BLOCK_SIZE];
    // rows
    for (int y = 0; y < BLOCK_SIZE; ++y) {
        double row_in[BLOCK_SIZE], row_out[BLOCK_SIZE];
        for (int x = 0; x < BLOCK_SIZE; ++x) row_in[x] = block[y][x];
        idct_1d(row_in, row_out);
        for (int x = 0; x < BLOCK_SIZE; ++x) temp[y][x] = row_out[x];
    }
    // columns
    for (int x = 0; x < BLOCK_SIZE; ++x) {
        double col_in[BLOCK_SIZE], col_out[BLOCK_SIZE];
        for (int y = 0; y < BLOCK_SIZE; ++y) col_in[y] = temp[y][x];
        idct_1d(col_in, col_out);
        for (int y = 0; y < BLOCK_SIZE; ++y) block[y][x] = col_out[y];
    }
}

// -------------------------------------------------------------------
// Zigzag order (JPEG)
// -------------------------------------------------------------------
constexpr int zigzag[64] = {
     0,  1,  5,  6, 14, 15, 27, 28,
     2,  4,  7, 13, 16, 26, 29, 42,
     3,  8, 12, 17, 25, 30, 41, 43,
     9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

// -------------------------------------------------------------------
// Decode one 8x8 block (fixed indexing)
// -------------------------------------------------------------------
void decode_block(BitReader& reader, const HuffTable& dc_table, const HuffTable& ac_table,
    const QuantTable& qt, std::array<std::array<double, BLOCK_SIZE>, BLOCK_SIZE>& coeffs,
    int& prev_dc) {
    // DC coefficient
    uint8_t dc_sym = read_huff_symbol(reader, dc_table);
    uint16_t dc_bits = dc_sym & 0x0F;
    int dc_val = 0;
    if (dc_bits != 0) {
        dc_val = reader.read_bits(dc_bits);
        if (dc_val < (1 << (dc_bits - 1)))
            dc_val -= (1 << dc_bits) - 1;
    }
    prev_dc += dc_val;
    coeffs[0][0] = static_cast<double>(prev_dc);

    // AC coefficients (zigzag order)
    int k = 1; // index in zigzag array
    while (k < 64) {
        uint8_t ac_sym = read_huff_symbol(reader, ac_table);
        uint8_t rrrr = ac_sym >> 4;
        uint8_t ssss = ac_sym & 0x0F;
        if (rrrr == 0 && ssss == 0) {
            // EOB
            break;
        }
        if (rrrr == 0x0F && ssss == 0) {
            // ZRL: 16 zeros
            k += 16;
            continue;
        }
        k += rrrr;
        if (k >= 64) break;
        int ac_val = 0;
        if (ssss != 0) {
            ac_val = reader.read_bits(ssss);
            if (ac_val < (1 << (ssss - 1)))
                ac_val -= (1 << ssss) - 1;
        }
        int pos = zigzag[k];
        int row = pos / 8;
        int col = pos % 8;
        coeffs[row][col] = static_cast<double>(ac_val);
        ++k;
    }

    // Dequantize
    for (int i = 0; i < BLOCK_SIZE; ++i)
        for (int j = 0; j < BLOCK_SIZE; ++j)
            coeffs[i][j] *= static_cast<double>(qt.data[i][j]);
}

// -------------------------------------------------------------------
// YCbCr to RGB
// -------------------------------------------------------------------
void ycbcr_to_rgb(uint8_t y, uint8_t cb, uint8_t cr, uint8_t& r, uint8_t& g, uint8_t& b) {
    double R = y + 1.402 * (cr - 128);
    double G = y - 0.344136 * (cb - 128) - 0.714136 * (cr - 128);
    double B = y + 1.772 * (cb - 128);
    r = static_cast<uint8_t>(std::clamp(R, 0.0, 255.0));
    g = static_cast<uint8_t>(std::clamp(G, 0.0, 255.0));
    b = static_cast<uint8_t>(std::clamp(B, 0.0, 255.0));
}

// -------------------------------------------------------------------
// Main JPEG decoding function
// -------------------------------------------------------------------
bool decode_jpeg(const std::string& filename, std::vector<uint8_t>& rgb, int& width, int& height) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;

    uint16_t marker = read_be16(file);
    if (marker != MARKER_SOI) return false;

    std::array<QuantTable, 4> quant_tables;
    std::array<HuffTable, 4> dc_huff;
    std::array<HuffTable, 4> ac_huff;
    int image_width = 0, image_height = 0;
    int num_components = 0;
    struct Component { int id; int h_samp; int v_samp; int qt_id; };
    std::vector<Component> components;
    bool sof_found = false;

    while (file.peek() != EOF) {
        marker = read_be16(file);
        if (marker == MARKER_EOI) break;
        uint16_t length = read_be16(file);
        uint16_t segment_end = file.tellg();
        segment_end += (length - 2);

        switch (marker) {
        case MARKER_APP0:
            file.seekg(segment_end);
            break;
        case MARKER_DQT:
            while (file.tellg() < segment_end) {
                uint8_t qt_info;
                file.read(reinterpret_cast<char*>(&qt_info), 1);
                int qt_id = qt_info & 0x0F;
                int prec = qt_info >> 4;
                if (prec != 0) return false;
                for (int i = 0; i < BLOCK_SIZE; ++i)
                    for (int j = 0; j < BLOCK_SIZE; ++j) {
                        uint8_t val;
                        file.read(reinterpret_cast<char*>(&val), 1);
                        quant_tables[qt_id].data[i][j] = val;
                    }
                quant_tables[qt_id].valid = true;
            }
            break;
        case MARKER_SOF0: {
            uint8_t precision;
            file.read(reinterpret_cast<char*>(&precision), 1);
            file.read(reinterpret_cast<char*>(&image_height), 2);
            file.read(reinterpret_cast<char*>(&image_width), 2);
            image_height = (image_height >> 8) | ((image_height & 0xFF) << 8);
            image_width = (image_width >> 8) | ((image_width & 0xFF) << 8);
            file.read(reinterpret_cast<char*>(&num_components), 1);
            components.resize(num_components);
            for (int i = 0; i < num_components; ++i) {
                uint8_t id;
                file.read(reinterpret_cast<char*>(&id), 1);
                uint8_t samp;
                file.read(reinterpret_cast<char*>(&samp), 1);
                uint8_t qt_id;
                file.read(reinterpret_cast<char*>(&qt_id), 1);
                components[i] = { id, samp >> 4, samp & 0x0F, qt_id };
            }
            sof_found = true;
            break;
        }
        case MARKER_DHT:
            while (file.tellg() < segment_end) {
                uint8_t ht_info;
                file.read(reinterpret_cast<char*>(&ht_info), 1);
                int table_class = ht_info >> 4;
                int table_id = ht_info & 0x0F;
                HuffTable ht;
                uint16_t total = 0;
                for (int i = 0; i < 16; ++i) {
                    uint8_t count;
                    file.read(reinterpret_cast<char*>(&count), 1);
                    ht.bits[i] = count;
                    total += count;
                }
                ht.symbols.resize(total);
                file.read(reinterpret_cast<char*>(ht.symbols.data()), total);
                ht.build();
                if (table_class == 0) dc_huff[table_id] = ht;
                else ac_huff[table_id] = ht;
            }
            break;
        case MARKER_SOS: {
            uint8_t num_scan_components;
            file.read(reinterpret_cast<char*>(&num_scan_components), 1);
            struct ScanComp { int id; int dc_tbl; int ac_tbl; };
            std::vector<ScanComp> scan_comps;
            for (int i = 0; i < num_scan_components; ++i) {
                uint8_t id;
                file.read(reinterpret_cast<char*>(&id), 1);
                uint8_t tbl;
                file.read(reinterpret_cast<char*>(&tbl), 1);
                scan_comps.push_back({ id, tbl >> 4, tbl & 0x0F });
            }
            uint8_t spectral_start, spectral_end, approx;
            file.read(reinterpret_cast<char*>(&spectral_start), 1);
            file.read(reinterpret_cast<char*>(&spectral_end), 1);
            file.read(reinterpret_cast<char*>(&approx), 1);

            BitReader reader(file);

            int max_h = 1, max_v = 1;
            for (auto& c : components) {
                max_h = std::max(max_h, c.h_samp);
                max_v = std::max(max_v, c.v_samp);
            }
            int mcu_width = max_h * BLOCK_SIZE;
            int mcu_height = max_v * BLOCK_SIZE;
            int mcus_x = (image_width + mcu_width - 1) / mcu_width;
            int mcus_y = (image_height + mcu_height - 1) / mcu_height;

            bool is_420 = (num_components == 3 && components[1].h_samp == 1 && components[1].v_samp == 1 &&
                components[2].h_samp == 1 && components[2].v_samp == 1);
            int cbcr_width = (image_width + 1) / 2;
            int cbcr_height = (image_height + 1) / 2;

            std::vector<std::vector<uint8_t>> y_plane(image_height, std::vector<uint8_t>(image_width));
            std::vector<std::vector<uint8_t>> cb_plane, cr_plane;
            if (num_components == 3) {
                cb_plane.resize(is_420 ? cbcr_height : image_height, std::vector<uint8_t>(is_420 ? cbcr_width : image_width));
                cr_plane.resize(is_420 ? cbcr_height : image_height, std::vector<uint8_t>(is_420 ? cbcr_width : image_width));
            }

            std::vector<int> prev_dc(num_components, 0);
            for (int mcu_row = 0; mcu_row < mcus_y; ++mcu_row) {
                for (int mcu_col = 0; mcu_col < mcus_x; ++mcu_col) {
                    for (int comp_idx = 0; comp_idx < num_components; ++comp_idx) {
                        const auto& comp = components[comp_idx];
                        int h_blocks = comp.h_samp;
                        int v_blocks = comp.v_samp;
                        for (int by = 0; by < v_blocks; ++by) {
                            for (int bx = 0; bx < h_blocks; ++bx) {
                                std::array<std::array<double, BLOCK_SIZE>, BLOCK_SIZE> coeffs = {};
                                decode_block(reader, dc_huff[scan_comps[comp_idx].dc_tbl],
                                    ac_huff[scan_comps[comp_idx].ac_tbl],
                                    quant_tables[comp.qt_id], coeffs, prev_dc[comp_idx]);
                                idct_2d(coeffs);
                                int x0 = mcu_col * max_h * BLOCK_SIZE + bx * BLOCK_SIZE;
                                int y0 = mcu_row * max_v * BLOCK_SIZE + by * BLOCK_SIZE;
                                for (int i = 0; i < BLOCK_SIZE; ++i) {
                                    for (int j = 0; j < BLOCK_SIZE; ++j) {
                                        double val = coeffs[i][j] + 128.0;
                                        int px = x0 + j;
                                        int py = y0 + i;
                                        if (px >= image_width || py >= image_height) continue;
                                        if (comp_idx == 0) {
                                            y_plane[py][px] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
                                        }
                                        else if (comp_idx == 1) {
                                            if (is_420) {
                                                int cx = px / 2, cy = py / 2;
                                                if (cx < cbcr_width && cy < cbcr_height)
                                                    cb_plane[cy][cx] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
                                            }
                                            else {
                                                cb_plane[py][px] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
                                            }
                                        }
                                        else if (comp_idx == 2) {
                                            if (is_420) {
                                                int cx = px / 2, cy = py / 2;
                                                if (cx < cbcr_width && cy < cbcr_height)
                                                    cr_plane[cy][cx] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
                                            }
                                            else {
                                                cr_plane[py][px] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            rgb.resize(image_width * image_height * 3);
            if (num_components == 1) {
                for (int y = 0; y < image_height; ++y)
                    for (int x = 0; x < image_width; ++x) {
                        uint8_t Y = y_plane[y][x];
                        int idx = (y * image_width + x) * 3;
                        rgb[idx + 0] = Y;
                        rgb[idx + 1] = Y;
                        rgb[idx + 2] = Y;
                    }
            }
            else {
                for (int y = 0; y < image_height; ++y) {
                    for (int x = 0; x < image_width; ++x) {
                        uint8_t Y = y_plane[y][x];
                        uint8_t Cb, Cr;
                        if (is_420) {
                            int cx = x / 2, cy = y / 2;
                            Cb = cb_plane[cy][cx];
                            Cr = cr_plane[cy][cx];
                        }
                        else {
                            Cb = cb_plane[y][x];
                            Cr = cr_plane[y][x];
                        }
                        uint8_t r, g, b;
                        ycbcr_to_rgb(Y, Cb, Cr, r, g, b);
                        int idx = (y * image_width + x) * 3;
                        rgb[idx + 0] = r;
                        rgb[idx + 1] = g;
                        rgb[idx + 2] = b;
                    }
                }
            }
            width = image_width;
            height = image_height;
            return true;
        }
        default:
            file.seekg(segment_end);
            break;
        }
    }
    return false;
}

// -------------------------------------------------------------------
// ASCII display
// -------------------------------------------------------------------
char luminance_to_ascii(uint8_t lum) {
    constexpr std::string_view palette = " .:-=+#@";
    size_t idx = lum * (palette.size() - 1) / 255;
    return palette[idx];
}

void display_rgb_as_ascii(const std::vector<uint8_t>& rgb, int width, int height) {
    if (rgb.empty() || width == 0 || height == 0) return;

    double sx = static_cast<double>(width) / MAX_CONSOLE_WIDTH;
    double sy = static_cast<double>(height) / MAX_CONSOLE_HEIGHT;
    double scale = std::max(sx, sy);
    int out_w = static_cast<int>(width / scale);
    int out_h = static_cast<int>(height / scale);
    out_w = std::min(out_w, MAX_CONSOLE_WIDTH);
    out_h = std::min(out_h, MAX_CONSOLE_HEIGHT);

    std::cout << "\n==================================================\n";
    std::cout << "ASCII Art (" << out_w << "x" << out_h << ")\n";
    std::cout << "--------------------------------------------------\n";
    for (int y = 0; y < out_h; ++y) {
        int src_y = static_cast<int>(y * scale);
        src_y = std::min(src_y, height - 1);
        for (int x = 0; x < out_w; ++x) {
            int src_x = static_cast<int>(x * scale);
            src_x = std::min(src_x, width - 1);
            int idx = (src_y * width + src_x) * 3;
            uint8_t r = rgb[idx];
            uint8_t g = rgb[idx + 1];
            uint8_t b = rgb[idx + 2];
            uint8_t lum = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
            std::cout << luminance_to_ascii(lum);
        }
        std::cout << '\n';
    }
    std::cout << "--------------------------------------------------\n";
}

// -------------------------------------------------------------------
// Main
// -------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " image.jpg\n";
        return 1;
    }
    std::vector<uint8_t> rgb;
    int width, height;
    try {
        if (!decode_jpeg(argv[1], rgb, width, height)) {
            std::cerr << "Failed to decode JPEG.\n";
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
    display_rgb_as_ascii(rgb, width, height);
    return 0;
}