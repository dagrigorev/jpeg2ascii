# JPEG to ASCII Console Viewer

A **standalone, pure C++20** program that decodes any baseline JPEG image (grayscale or color) and displays it as ASCII art directly in the terminal. No external libraries – the JPEG decoder is implemented from scratch.

## Features

- ✅ **Full JPEG decoding** – handles SOI, DQT, DHT, SOF0, SOS, EOI markers; reads Huffman tables, quantization tables, and entropy‑coded data.
- ✅ **Supports**:
  - Grayscale JPEGs (1 component)
  - Color JPEGs (YCbCr, 3 components)
  - 4:2:0 chroma subsampling (common for most photos)
  - 4:4:4 subsampling (no subsampling)
- ✅ **Inverse DCT** – correct 8×8 IDCT with proper scaling and level shift.
- ✅ **Huffman decoding** – full symbol lookup using generated code tables.
- ✅ **Bit‑perfect reader** – handles JPEG’s byte‑stuffing (`0xFF 0x00`).
- ✅ **Console scaling** – automatically fits the image to your terminal size (default 80×25) while preserving aspect ratio.
- ✅ **ASCII palette** – maps luminance (0‑255) to ` .:-=+#@` for visual depth.
- ✅ **No external dependencies** – uses only the C++17 standard library.

## How It Works

1. **Parse JPEG markers** – locates quantization tables, Huffman tables, image dimensions, and component subsampling factors.
2. **Read entropy‑coded data** – decodes MCU (Minimum Coded Unit) blocks using the Huffman tables.
3. **Dequantize & IDCT** – reconstructs 8×8 pixel blocks.
4. **Color conversion** – YCbCr → RGB (if color).
5. **Scale to console** – computes the largest scaling factor that fits the image within your terminal dimensions.
6. **ASCII rendering** – converts each RGB pixel to luminance, then picks an ASCII character and prints it line by line.

## Requirements

- A **C++17 compiler** (GCC 7+, Clang 5+, MSVC 2017+)
- A terminal that supports standard output (any console on Windows, Linux, macOS)

## Compilation

### Linux / macOS (GCC or Clang)
```bash
g++ -std=c++17 -O2 main.cpp -o jpeg2ascii
```

### Windows (MSVC)
```cmd
cl /EHsc /std:c++17 /O2 main.cpp
```

### Windows (MinGW)
```bash
g++ -std=c++17 -O2 main.cpp -o jpeg2ascii.exe
```

## Usage

```bash
./jpeg2ascii path/to/your/image.jpg
```

### Example
```bash
./jpeg2ascii photo.jpg
```

The program will print an ASCII art version of the image directly to the terminal.

## Customisation

You can change the console size by editing the constants at the top of `main.cpp`:

```cpp
constexpr int MAX_CONSOLE_WIDTH  = 80;   // columns
constexpr int MAX_CONSOLE_HEIGHT = 25;   // rows
```

You can also modify the ASCII palette (line ~520):

```cpp
constexpr std::string_view palette = " .:-=+#@";
```

Use fewer characters for lower “resolution” or more characters for smoother gradients.

## Limitations (Known)

- **Baseline sequential JPEG only** – no progressive JPEG, no arithmetic coding, no 12‑bit samples.
- **Subsampling** – only 4:2:0 and 4:4:4 are tested; 4:2:2 and 4:1:1 are not supported.
- **No EXIF / metadata handling** – irrelevant for the viewer.
- **Memory** – the entire decoded image is stored as RGB (width × height × 3 bytes). For very large images (e.g., 8K) this can be a few hundred MB, but the console scaling still works.

## Troubleshooting

| Issue | Possible Fix |
|-------|---------------|
| `Failed to decode JPEG` | The image may be progressive or use unsupported features. Try a different JPEG. |
| Garbled output / random characters | The Huffman lookup failed – the JPEG might have unusual tables. Ensure it’s a standard baseline JPEG. |
| Exception `array subscript out of range` | Already fixed in the provided code. If you see it, you are using an older version – use the final corrected code above. |
| Very slow rendering | The decoder processes every MCU – this is expected for large images. The ASCII display itself is fast. |

## Example Outputs

The following screenshots (simulated) show the program in action:

```
$ ./jpeg2ascii dog.jpg

==================================================
ASCII Art (71x40)
--------------------------------------------------
::....::-:::::.::::::--:-----=======-------========+++++###############
::...:::---::::::--::-------========------========+++++################
::..:.::-------::---::--===========-------========++++#################
:......:---===-------:-----=========-----==========++++################
:.:....:---===-------------========----=--=========++++################
::::.::----------------------=======--=======+==+=++++++###########+##+
-:::::-------------------------========+#====++++=++++++##########+++++
:-:--------------==----------+=#-======+++=====++=+++++##########++++++
--:--------------====--::--:+===+=====+++++=====++++++++##########+++++
-----------------------::-::+=+++====+#++++=====+++++++++++#######++=++
-:------------=--------::--:++++++===##++++=====+++++++++++++###+#+++++
--:---:-::-------------:----=+==++-=+++==++=====+++++++++++++##++++++++
-:::::::::--::-:--------:---====+++-=+=+=++=====+=++=++++++++++++++=+++
:::-:--:::--::-:-:----------=+=-=+####+#==+============++++++++++++=+++
:::-----::::::::-:----::----====++##++++=+#===-==+====++++++++++++==+++
:::::::-:-:::--:::----:::--:=+=++#+#+++++++===-========+++++++++++===++
:::::::--::-::--::---::-:---===+::=#= .++=====-=====+++++++++++=++++===
:::::::--::--:--:::--:::::-=#=+-  +#+. =++++=--======+=++++++++==++++==
:-::.:---::---:::--:-:::::::#=#=-=###+=++#+#=--==++========+++====+++==
:-::::--------::--:--:::::::##++=#=+####+++#+---============++======+==
:--------=--------:::::.:::+###+##   ##-=+###---=======================
:------====-==--==---::..::###+=##. -##.++###-:--=======--======-======
-------===========----:..::###++.+- :-.+++###---------==-==============
-=--==============-------:-+###++.-:- =+++###================+++==+++=+
==========================####++++::.+=++++##++++++++++++++++++++++++++
=========================##=#+++++=-=++++#+###+++++++++++++++++++++++++
====--=--==============+####:-++++===+++++####+++++++++++++++++++++++++
+==============--===--=##++#:::=======+==++###+++++++++#+++++++++++++++
+==+++++==============+#++###-: .-==++--+++###++++++++++++++++++++++#++
====+++++++++++++++++++#++###++-:::::-==++####+++++++++==++=+=+==++++++
===+++++++++++++++++=+++=##+##++++=+==+++#+###+====+++=-=-==++===++==++
++++=+++++++=+=++=+=--=#++++++++++==++++=++###+++++====-===++++++++++++
=+==+++++========++=.-++=+=====+++++=+++++++##++===-=+=======+==+++++++
====++++++===-=-++++==+===-+.====#=+=+===-+++=+++-+++=++====-=-=++=++++
+++++======-----=++=+++=++======-====-+++++++++++++++====+=--=:=+++++++
++++==-==-----=--====+==++===+=+++++++++++++=====+===+=++++=+++=+++++==
+====--=-=--==---========+++++++++++++++++++=============++=+++=+======
======-=-=-=---==---=======++=++++===+=====+=+++++++=+====+============
++++===-=--=-----=--======================+++++++++++++++=+++=+-==--=--
=+++++++====--=--====+=====--==-======++===+++++=++++++++=++++++++==+==
--------------------------------------------------
```

```
$ ./jpeg2ascii dogs.jpg

==================================================
ASCII Art (71x40)
--------------------------------------------------
 ::   :::  :::   ::   :::  :::   ::   :::  :::   ::   :::  :::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
 ::   :::  :::   ::   :::  :::   ::   :::  :::   ::   :::  :::
 ::   :::  :-=-. ::   :::  :::   ::   :::  :::   ::   :::  :::
 ::   ::: .===++#===  :::  :::   ::   :::  :::   ::   :::  :::
::::::::-============-::::::::::::::::::::::::::::::::::::::::::
::::::::===============:::::::::::::::::::::::::::::::::::::::::
---::::-===============-::::::::::::::::::::::::::::::::::::::::
:--:..-=--+===##+==#++==:  :::.--::   :::  :::   ::   :::  :::
-==::.-=-=====+#+==+===-:  :==++#==++=:::  :::   ::   :::  :::
===----=-=-=============::-===========::::::::::::::::::::::::::
===---=:---=====:======================:::::::::======::::::::::
=-------========-======================-::::::=========-::::::::
-=-:.----:====+##==+++==-=====-=+==+#===:  :===++==+++==-  :::
-=-:.----:+===+++==++====:====+====.- ==: :========+++===  :::
-=-:..:--:-:---+===-+-====+===-:===.. ==: .=-=+====+++===+=:::
==--:::----:::=====-===-=====--====--===-:-=--=-============::::
----:::--==::-========--:====--==-:::===-------=============::::
.--.  :--.+-:-+++===:.----===:=..:: ::==--:--- .===--+===---::
:--:..:--..-:--====+=:=----==-.  =-. .:--.:--=:-===-- -===-==:
:==-:.==-:.=== ====+#+=--=====   ===:.=-: .:--:-====-====--==-
-===--=-------==-=====--===-===:-======--:-:---==========--=--::
=====-======-==========-====--=:-:====----=:::==--===========-::
=====-========================--==:=--=-::-:--=:::-=====-=--=--:
-==---=========+#==++#===++===.:-==--==--: :::-. :-+=:==:::=:-..
====--====+====+#==++#===+=====:.:--===-=::-:::-:--==:=-::--:-..
================================--=======-=-:::-::-===-:-=-=:---
===========================================--:::==:---==--==::::
========================================:==--::====-=---====-:::
-========++===++#==+++===:+===+=+==++===--====.  ::.:.--=-===-
-====--===+===+++==++====+===-++===+++========+..:-.::-=======
.-=--=====#===+++==++=========+.===+#+==--+===+=:--:  :======-:
-=========================--===:=======-:-==:===--====---=====-:
==========-=============-=--:-----==--===:::--:====-===:----::==
.-=++#====+========+++===+====-: ---:.---. :-:...::.. :::..:::-:
.==-=+===-==-=#:.:-.. :::..:--...::.  ::: .:--...::...-::..:::--
::--..==:.-=:==# ::...:::..:-:...-:...--:..:::...::.. ::: .::---
-:-::-==-::::-==-::--::---:---------:-:-::::--::::-:-::::::::--=
==-=-::---:::=======:=::::-:::---:::::::::::::::::::::::::::--=-
--------------------------------------------------
```

The ASCII art preserves the overall shape, contrast, and structure of the original image.

## Code Structure

| Function / Section | Purpose |
|--------------------|---------|
| `read_be16` | Read big‑endian 16‑bit word from file |
| `BitReader` | Bit‑level reader with byte‑stuffing handling |
| `HuffTable::build` / `read_huff_symbol` | Huffman table generation and symbol decoding |
| `idct_1d` / `idct_2d` | Inverse DCT (8×8 block) |
| `decode_block` | Decode one 8×8 block: DC+AC Huffman, dequantize |
| `decode_jpeg` | Main parsing + MCU decoding + YCbCr→RGB |
| `display_rgb_as_ascii` | Scale image to console, map to ASCII |
| `main` | Command‑line interface and error handling |

## License

This project is **public domain** (or MIT if you prefer – do whatever you like with it). No warranty, but it works for most JPEGs.

## Acknowledgments

- Inspired by the desire to understand JPEG internals without relying on `libjpeg` or `stb_image`.
- Built from scratch using the official JPEG standard (ITU T.81) as reference.

---

Enjoy seeing your photos in pure text! If you find a bug or want to add support for progressive JPEGs, feel free to contribute.