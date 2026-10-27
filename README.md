# LVGL 8.4 × 9.6 on the ESP32-S3 (LilyGO T-Display-S3)

LVGL 9 brought a new drawing pipeline and several optimizations. This project measures **how much
that changes in practice** on a real ESP32-S3: the same UI code runs on both versions, on the same
board, with the same settings, and the time of each frame is sent to the PC over USB.

![The 12 benchmark scenes](docs/scenes.png)

*Left to right, top to bottom: `fill`, `gradient`, `rects`, `rects_opa`, `shadow`, `text`, `image`,
`image_opa`, `rotate`, `arc`, `grad_rot`, `ui`.*

## Summary

- **v9 is much faster where the work is blending pixels:** anti-aliased text (−54%), rotated image
  (−41%), translucent rectangles (−32%) and translucent image (−25%).
- **v9 is slower where there are many small operations:** opaque fill and copy (+44% and +64%, but
  under 5 ms), the `ui` chart (+36%) and the rotated objects in `grad_rot` (+40%). In v9 each drawing
  primitive (each line segment, each chart point) has a larger fixed cost.
- **For a typical widget UI (`ui`), v8 still wins:** 43 vs 32 FPS with the display at 40 MHz.
  For text-heavy screens, v9 doubles it: 60 vs 29 FPS.
- **Rendering on both cores (`LV_DRAW_SW_DRAW_UNIT_CNT=2`) made v9 slower in every scene** (+13% to
  +92%). Just enabling FreeRTOS in LVGL already costs up to 29%.
- **LCD bus clock:** with the LilyGO example value (6.67 MHz) every light scene is stuck at 60 FPS.
  At 40 MHz the light scenes go past 300 FPS; the heavy ones do not change, because there the CPU is
  the limit.

## Hardware and method

| | |
|---|---|
| Board | LilyGO T-Display-S3: ESP32-S3 (2 cores at 240 MHz), ST7789 320×170 on an 8-bit i80 bus |
| SDK | ESP-IDF 5.5.4, LVGL 8.4.0 and 9.6.0 from the component registry |
| Color | RGB565, two 40-line partial buffers in internal RAM with DMA |

Each scene redraws **the whole screen 60 times** (`lv_obj_invalidate` + `lv_refr_now`) and the time
of each frame is measured with `esp_timer`. Each scene runs in two modes:

- **render**: the `flush_cb` discards the pixels. It measures only the CPU drawing, which is what changes between versions.
- **lcd**: the pixels go through DMA to the display. It includes the bus, which depends on the clock (PCLK).

To keep the comparison fair, both versions use:

- the same buffers, libc `malloc`/`memcpy`, `-O2`, instruction and data caches at maximum and the
  critical LVGL functions in IRAM (`LV_ATTRIBUTE_FAST_MEM_USE_IRAM`);
- RGB565 byte swapping done by the LCD hardware (`swap_color_bytes`), with no CPU cost in either of them;
- synthetic scenes with no theme style (`lv_obj_remove_style_all`): the default themes of v8 and v9
  are different, and this way both draw exactly the same thing. The exception is `ui`, which uses the
  default theme on purpose, like an app migrated from v8 to v9.

The only mandatory configuration difference: v8 with 16-bit color needs `LV_COLOR_SCREEN_TRANSP=1`
to rotate objects that are not images (scene `grad_rot`). Without it, it silently skips the object.

The time of each frame is kept in RAM during the measurement and only then goes out over the S3's
native USB, one CSV line per frame. `bench.py` captures a full cycle, checks that no line was lost
and saves it to `results/`.

## Results

### Rendering: v8 × v9

![Rendering time per frame, v8 and v9](docs/render-v8-v9.svg)

| scene | what it draws | v8 (ms) | v9 (ms) | v9 vs v8 |
|---|---|---:|---:|---:|
| `fill` | only the screen background | 0.53 | 0.77 | +44% |
| `gradient` | vertical gradient over the whole screen | 1.50 | 1.58 | +5% |
| `rects` | 24 rounded rectangles with border | 10.11 | 11.40 | +13% |
| `rects_opa` | the same, at 50% opacity | 24.45 | 16.67 | −32% |
| `shadow` | 6 rectangles with a wide shadow | 27.07 | 24.22 | −11% |
| `text` | 14 px paragraph + 28 px title | 34.35 | 15.70 | −54% |
| `image` | 15 RGB565 images of 64×64 | 2.93 | 4.79 | +64% |
| `image_opa` | the same, at 50% opacity | 13.70 | 10.28 | −25% |
| `rotate` | 2× magnified image rotating | 21.44 | 12.72 | −41% |
| `arc` | 4 concentric arcs | 11.30 | 10.45 | −8% |
| `grad_rot` | 8 rotating circles with gradient | 50.73 | 71.07 | +40% |
| `ui` | chart, meter, bar, slider, switch and scrolling list | 21.83 | 29.75 | +36% |

### Why v9 loses in `ui`

To isolate the cause, each part of the `ui` scene was measured on its own, in a headless test on
the PC (x86, clang `-O2`; the absolute values do not apply to the S3, only the ratio between the
versions):

| `ui` part | v8 | v9 | v9 vs v8 |
|---|---:|---:|---:|
| chart (40 points) with shadow | 0.138 ms | 0.191 ms | +38% |
| list with 15 items scrolling | 0.079 ms | 0.096 ms | +22% |
| meter + label | 0.045 ms | 0.043 ms | −3% |
| bar, slider and switch | 0.023 ms | 0.028 ms | +21% |
| **full screen** | **0.338 ms** | **0.409 ms** | **+21%** |

The chart accounts for three quarters of the difference and the list for the rest. Varying the number
of chart points (10 to 160), each extra segment costs ~2.9 µs in v8 and ~4.1 µs in v9, and each extra
dot ~0.8 µs in v9 (in v8 it is almost free).
In v9 each primitive becomes a *draw task* with its own fixed cost; this pays off when the primitive
covers many pixels (text, transparency) and hurts when there are many small primitives (chart lines,
dots, small layers). The same pattern shows up in `fill`, `rects` and `image`.

### One core × two cores (v9)

v9 can split the drawing between threads (`LV_USE_OS=FREERTOS` + `LV_DRAW_SW_DRAW_UNIT_CNT=2`).
In this scenario it made everything worse. Rendering times in ms:

| scene | v9 | v9 + FreeRTOS | v9 + 2 cores |
|---|---:|---:|---:|
| `fill` | 0.77 | 0.87 (+14%) | 1.47 (+92%) |
| `gradient` | 1.58 | 1.68 (+7%) | 2.72 (+73%) |
| `rects` | 11.40 | 14.74 (+29%) | 20.05 (+76%) |
| `rects_opa` | 16.67 | 20.01 (+20%) | 25.31 (+52%) |
| `shadow` | 24.22 | 25.38 (+5%) | 38.19 (+58%) |
| `text` | 15.70 | 16.25 (+3%) | 23.33 (+49%) |
| `image` | 4.79 | 5.47 (+14%) | 9.00 (+88%) |
| `image_opa` | 10.28 | 10.94 (+6%) | 14.48 (+41%) |
| `rotate` | 12.72 | 12.69 (−0%) | 14.35 (+13%) |
| `arc` | 10.45 | 11.73 (+12%) | 17.03 (+63%) |
| `grad_rot` | 71.07 | 71.22 (+0%) | 89.30 (+26%) |
| `ui` | 29.75 | 36.54 (+23%) | 52.06 (+75%) |

The middle column separates the cost of just enabling the OS (task switches for synchronization) from
the cost of splitting across the cores. The most likely hypothesis is that, with 40-line buffers and
many small primitives, the synchronization between the threads costs more than the second core saves.
This was not investigated in depth.

### LCD bus clock

![FPS with display transfer as a function of PCLK](docs/fps-pclk.svg)

The S3 generates PCLK as 80 MHz / n, so the possible values are 40, 26.67, 20, 16, 13.33, … and
6.67 MHz (this is what the LilyGO example value, 6.528 MHz, actually becomes). A full frame is 108,800
bytes, so the bus alone limits to ~61 FPS at 6.67 MHz and to ~367 FPS at 40 MHz.

<details>
<summary>FPS of all scenes at all clocks</summary>

LVGL 8.4:

| scene | 6.67 MHz | 10 MHz | 16 MHz | 20 MHz | 26.67 MHz | 40 MHz |
|---|---:|---:|---:|---:|---:|---:|
| `fill` | 60 | 89 | 140 | 172 | 225 | 325 |
| `gradient` | 59 | 87 | 136 | 167 | 215 | 305 |
| `rects` | 55 | 78 | 86 | 89 | 93 | 95 |
| `rects_opa` | 36 | 38 | 40 | 40 | 40 | 40 |
| `shadow` | 34 | 36 | 36 | 36 | 36 | 36 |
| `text` | 28 | 28 | 29 | 29 | 29 | 29 |
| `image` | 58 | 85 | 131 | 160 | 204 | 260 |
| `image_opa` | 51 | 61 | 66 | 68 | 70 | 70 |
| `rotate` | 38 | 41 | 43 | 43 | 44 | 45 |
| `arc` | 54 | 68 | 76 | 78 | 81 | 83 |
| `grad_rot` | 19 | 19 | 19 | 19 | 19 | 19 |
| `ui` | 37 | 40 | 42 | 42 | 43 | 43 |

LVGL 9.6:

| scene | 6.67 MHz | 10 MHz | 16 MHz | 20 MHz | 26.67 MHz | 40 MHz |
|---|---:|---:|---:|---:|---:|---:|
| `fill` | 60 | 88 | 138 | 170 | 221 | 316 |
| `gradient` | 59 | 87 | 135 | 165 | 213 | 299 |
| `rects` | 55 | 72 | 79 | 82 | 84 | 84 |
| `rects_opa` | 49 | 53 | 57 | 58 | 58 | 58 |
| `shadow` | 38 | 39 | 40 | 40 | 40 | 40 |
| `text` | 50 | 58 | 59 | 60 | 60 | 60 |
| `image` | 57 | 83 | 125 | 150 | 169 | 183 |
| `image_opa` | 53 | 75 | 85 | 88 | 91 | 91 |
| `rotate` | 52 | 63 | 68 | 70 | 72 | 73 |
| `arc` | 54 | 72 | 81 | 84 | 87 | 89 |
| `grad_rot` | 14 | 14 | 14 | 14 | 14 | 14 |
| `ui` | 30 | 31 | 32 | 32 | 32 | 32 |

</details>

The ST7789 is specified for ~15 MHz in 8080 mode; above that, whether it works depends on the board.
The times were measured up to 40 MHz, but check the image on yours before using a high clock in a product.

### Official LVGL benchmark (`lv_demo_benchmark`)

Each version ships its own benchmark, with **different scenes and metrics**: the numbers of one
cannot be compared with those of the other, which is why this project has its own scenes. Both were
run here with the LCD at 20 and 40 MHz; the full summaries are in `results/*-demo@*.txt`.

**LVGL 8.4** (maximum speed mode, FPS computed from the redraw time of the area that changed):

| | 20 MHz | 40 MHz |
|---|---:|---:|
| Weighted FPS | 449 | 483 |
| Opa. speed | 36% | 35% |
| Text small / large | 37 / 38 | 37 / 38 |
| Shadow large | 34 | 34 |
| Image RGB rotate anti aliased | 91 | 98 |
| Rectangle | 383 | 566 |

The compressed-font scenes gave 0 FPS in both measurements (not investigated).

**LVGL 9.6:**

| scene | FPS (20 MHz) | FPS (40 MHz) | render (ms) |
|---|---:|---:|---:|
| Empty screen | 127 | 176 | 4 |
| Moving wallpaper | 717 | 744 | 7 |
| Single rectangle | 984 | 984 | 0 |
| Multiple rectangles | 208 | 208 | 3 |
| Multiple RGB images | 935 | 934 | 1 |
| Multiple ARGB images | 905 | 904 | 2 |
| Rotated ARGB images | 463 | 458 | 10 |
| Multiple labels | 234 | 233 | 3 |
| Screen sized text | 32 | 32 | 31 |
| Multiple arcs | 983 | 983 | 0 |
| Containers | 831 | 829 | 7 |
| Containers with overlay | 60 | 61 | 14 |
| Containers with opa | 790 | 788 | 9 |
| Containers with opa_layer | 675 | 675 | 15 |
| Containers with scrolling | 68 | 69 | 12 |
| Widgets demo | 91 | 91 | 8 |
| **average** | **506** | **510** | **7** |

Both barely change from 20 to 40 MHz: the official scenes redraw small areas (v9 reports 0 ms of
flush time in most), so the CPU is the limit. The v9 "Avg. CPU" column (80–99%) only reflects the
measurement loop, which never sleeps.

## How to run

You need ESP-IDF 5.5 (tested with 5.5.4) and a T-Display-S3 connected over USB-C. In a terminal with
ESP-IDF activated (it already ships Python with `pyserial`):

```sh
python bench.py run v9 COM6        # builds, flashes, captures and saves results/v9.csv
python bench.py run v8@40 COM6     # same thing with the LCD at 40 MHz
python bench.py compare results/v8.csv results/v9.csv
python docs/charts.py              # regenerates the charts and prints the tables
```

The port is optional (without it `bench.py` looks for the Espressif USB). Variants:

| variant | what it is |
|---|---|
| `v8`, `v9` | the comparison baseline, LVGL without OS, one core |
| `v9-freertos` | v9 with `LV_USE_OS=FREERTOS` and one draw unit |
| `v9-mt` | v9 with FreeRTOS and two draw units (both cores) |
| `v8-demo`, `v9-demo` | the official `lv_demo_benchmark` of each version |
| `…@MHz` | any of the above with another LCD clock, for example `v9-demo@40` |

Each variant builds into `v8/build-<variant>` or `v9/build-<variant>`; changing only the clock
rebuilds just `main`. If the board resets in the middle of the capture (crash), `bench.py` stops and
shows the log.

### PC simulator

`sim/` runs the same scenes with LVGL 9 and SDL2 in a window 3× larger, to see how each scene should
look on the screen. With `--demo`, it runs `lv_demo_benchmark`. It uses the LVGL that ESP-IDF
downloads to `v9/managed_components`, so build `v9` first. Without SDL2 installed, the LVGL CMake
downloads and builds SDL2 by itself.

```sh
# Linux (with libsdl2-dev installed) or macOS
cmake -S sim -B sim/build -G Ninja && cmake --build sim/build && sim/build/lvgl_bench_sim

# Windows with LLVM (clang) and Visual Studio Build Tools
cmake -S sim -B sim/build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_RC_COMPILER=llvm-rc
cmake --build sim/build
sim\build\lvgl_bench_sim.exe --demo
```

## Structure

```
bench.c            firmware: LCD driver, measurement and USB output (builds on both v8 and v9)
scenes.c, .h       the 12 scenes, with the API common to both versions
bench.py           build, flash, capture and comparison
sdkconfig.common   common configuration (CPU, caches, USB console, fonts, IRAM)
v8/, v9/           ESP-IDF projects; each one pulls its own LVGL version
sim/               PC simulator (LVGL 9 + SDL2)
results/           raw data: one CSV per variant (time of each frame) and the official benchmark summaries
docs/              charts (generated by charts.py from results/) and the scenes image
```

## Problems found along the way

- **Garbage in random parts of the screen.** Newer T-Display-S3 boards come with a different display
  batch and need LilyGO's extra initialization sequence (`LCD_MODULE_CMD_1` in their examples). It is
  in `lilygo_init` in `bench.c`.
- **USB does not show up or gives error 31 / "Device Descriptor Request Failed".** Hold BOOT, press
  RST and release BOOT to enter flashing mode; if it still does not show up, the cable is
  charge-only.
- **v8 does not draw rotated objects.** With 16-bit color it needs `LV_COLOR_SCREEN_TRANSP=1`;
  without it, it only logs a warning and skips the object, and the frame comes out too cheap. A
  headless test on the PC, saving the frames of both versions as PNG, revealed this.
- **Stack overflow with LVGL + FreeRTOS.** The ESP-IDF `main` task has 3.5 KB by default, which
  overflows in the `ui` scene; here it has 16 KB.
- **The requested PCLK is not the real PCLK.** The i80 driver rounds the divider down, so a value
  between 26.67 and 40 MHz becomes 40 MHz. `bench.py` already picks the closest 80/n.
- **`lv_demo_benchmark` needs the tick running during drawing.** It measures with the LVGL clock;
  with `lv_tick_inc` only between the `lv_timer_handler` calls, the frames seem to take 0 ms and the
  FPS explodes. Here the tick comes from a 1 ms `esp_timer`.
- In v9.6 the `lv_list` used in the `ui` scene is marked as deprecated (it works, but produces a
  warning). In v8 the `ui` switch appears gray because the theme's color animation depends on the
  tick, which does not run in the measured scenes; this does not change the frame cost.
