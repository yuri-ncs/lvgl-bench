"""Build, flash and capture of the LVGL benchmark over the T-Display-S3 USB (USB-Serial-JTAG).

Run in a terminal with ESP-IDF activated (needs idf.py and pyserial):

  python bench.py run <variant> [COMx]     builds, flashes and captures into results/<variant>.csv (or .txt for -demo)
  python bench.py capture [COMx]           only captures the board's next cycle as is
  python bench.py compare a.csv b.csv ...  side-by-side table (mean ms per frame, % vs the first)

Variants: v8, v9, v9-freertos, v9-mt, v8-demo, v9-demo
The @MHz suffix changes the LCD bus clock (default 6.67): v8@20, v9@20, v9-demo@20...
The S3 only generates 80 MHz / n: 40, 26.67, 20, 16, 13.33, 11.43, 10 ... 6.67; other values go to the nearest one.
"""
import csv
import os
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent
VARIANTS = {  # name: (project, extra sdkconfig.defaults)
    "v8": ("v8", []),
    "v9": ("v9", []),
    "v9-freertos": ("v9", ["sdkconfig.defaults.freertos"]),
    "v9-mt": ("v9", ["sdkconfig.defaults.freertos", "sdkconfig.defaults.mt"]),
    "v8-demo": ("v8", ["sdkconfig.defaults.demo"]),
    "v9-demo": ("v9", ["sdkconfig.defaults.demo"]),
}


def open_port(port=None):
    import serial
    from serial.tools import list_ports

    port = port or next(p.device for p in list_ports.comports() if p.vid == 0x303A)  # Espressif native USB
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = port, 115200, 1
    ser.dtr = ser.rts = False  # DTR/RTS low: opening the port does not reset the board
    ser.open()
    return ser


def run(variant, port=None):
    base, _, mhz = variant.partition("@")  # "v9@20": v9 with the LCD bus at 20 MHz
    proj, extra = VARIANTS[base]
    build = f"build-{base}"  # same build directory: changing the clock only rebuilds main
    # the S3 only generates PCLK = 80 MHz / n (n >= 2) and the driver rounds n down; ask for the nearest 80/n
    hz = int(80e6 / max(2, round(80 / float(mhz)))) if mhz else 0
    idf = [sys.executable, str(Path(os.environ["IDF_PATH"]) / "tools" / "idf.py"), "-B", build,
           f"-DSDKCONFIG={build}/sdkconfig", f"-DBENCH_PCLK_HZ={hz}",
           "-DSDKCONFIG_DEFAULTS=" + ";".join(["../sdkconfig.common", "sdkconfig.defaults", *extra])]
    cwd = ROOT / proj
    # on the first build of a new directory IDF sometimes asks for a "re-run" because of the non-default sdkconfig
    if subprocess.run(idf + ["build"], cwd=cwd).returncode:
        subprocess.run(idf + ["build"], cwd=cwd, check=True)
    if port:
        idf += ["-p", port]
    subprocess.run(idf + ["flash"], cwd=cwd, check=True)
    (capture_demo if base.endswith("-demo") else capture)(port, variant)


def read_line(ser, log):
    """Reads a line; if the board reboots (crash), stops with the last log lines instead of waiting forever."""
    line = ser.readline().decode(errors="replace").strip()
    if not line.startswith("F,"):
        log.append(line)
        del log[:-15]
    if "Rebooting..." in line:
        sys.exit("the board rebooted in the middle of the capture:\n" + "\n".join(log))
    return line


def capture(port=None, name=None):
    ser = open_port(port)
    print(f"waiting for the next cycle on {ser.port}...")
    head, rows, log = None, [], []
    while True:
        line = read_line(ser, log)
        if line.startswith("B,"):
            head, rows = line.split(",")[1:], []
        elif head and line.startswith("F,"):
            rows.append(line.split(",")[1:])
        elif head and line.startswith("E,"):
            assert len(rows) == int(line[2:]), f"received {len(rows)} of {line[2:]} lines: the port lost data"
            break
    out = ROOT / "results" / f"{name or f'{head[0]}-u{head[1]}'}.csv"
    out.parent.mkdir(exist_ok=True)
    with open(out, "w", newline="") as f:
        csv.writer(f).writerows([["scene", "mode", "frame", "us"], *rows])
    print("saved to", out)
    compare([out])


def capture_demo(port, name):
    ser = open_port(port)
    print(f"waiting for the lv_demo_benchmark summary on {ser.port} (takes a few minutes)...")
    lines, idle, log = [], 0, []
    while idle < 3:  # the summary has no end marker: stop after 3 s without new lines
        line = read_line(ser, log)
        if not lines and not ("Benchmark" in line and ("csv" in line or "Summary" in line)):
            continue
        idle = 0 if line else idle + 1
        if line:
            lines.append(line)
    out = ROOT / "results" / f"{name}.txt"
    out.parent.mkdir(exist_ok=True)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    print("saved to", out)


def compare(files):
    runs = {}
    for f in map(Path, files):
        with open(f) as fh:
            t = {}
            for r in csv.DictReader(fh):
                t.setdefault((r["scene"], r["mode"]), []).append(int(r["us"]) / 1000)
            runs[f.stem] = t
    names = list(runs)
    w = max(16, *(len(n) + 2 for n in names))
    print(f"{'scene':<12}{'mode':<8}" + "".join(f"{n:>{w}}" for n in names))
    for k in runs[names[0]]:
        base = statistics.mean(runs[names[0]][k])
        cells = []
        for n in names:
            m = statistics.mean(runs[n][k])
            cells.append(f"{m:.2f}" + (f" ({100 * (m / base - 1):+.0f}%)" if n != names[0] else ""))
        print(f"{k[0]:<12}{k[1]:<8}" + "".join(f"{c:>{w}}" for c in cells))


if __name__ == "__main__":
    cmd, *args = sys.argv[1:] or ["capture"]
    {"run": run, "capture": capture, "compare": lambda *a: compare(a)}[cmd](*args)
