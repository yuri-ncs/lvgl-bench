"""Build, gravação e captura do benchmark LVGL pela USB do T-Display-S3 (USB-Serial-JTAG).

Rode num terminal com o ESP-IDF ativado (precisa de idf.py e pyserial):

  python bench.py run <variante> [COMx]    compila, grava e captura em results/<variante>.csv (ou .txt nos -demo)
  python bench.py capture [COMx]           só captura o próximo ciclo da placa como está
  python bench.py compare a.csv b.csv ...  tabela lado a lado (ms médio por quadro, % vs o primeiro)

Variantes: v8, v9, v9-freertos, v9-mt, v8-demo, v9-demo
Sufixo @MHz muda o clock do barramento do LCD (padrão 6.67): v8@20, v9@20, v9-demo@20...
O S3 só gera 80 MHz / n: 40, 26.67, 20, 16, 13.33, 11.43, 10 ... 6.67; outros valores vão para o mais próximo.
"""
import csv
import os
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent
VARIANTS = {  # nome: (projeto, sdkconfig.defaults extras)
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

    port = port or next(p.device for p in list_ports.comports() if p.vid == 0x303A)  # USB nativa da Espressif
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = port, 115200, 1
    ser.dtr = ser.rts = False  # DTR/RTS baixos: abrir a porta não reseta a placa
    ser.open()
    return ser


def run(variant, port=None):
    base, _, mhz = variant.partition("@")  # "v9@20": v9 com o barramento do LCD a 20 MHz
    proj, extra = VARIANTS[base]
    build = f"build-{base}"  # mesmo build dir: trocar o clock só recompila o main
    # o S3 só gera PCLK = 80 MHz / n (n >= 2) e o driver arredonda n para baixo; pede o 80/n mais próximo
    hz = int(80e6 / max(2, round(80 / float(mhz)))) if mhz else 0
    idf = [sys.executable, str(Path(os.environ["IDF_PATH"]) / "tools" / "idf.py"), "-B", build,
           f"-DSDKCONFIG={build}/sdkconfig", f"-DBENCH_PCLK_HZ={hz}",
           "-DSDKCONFIG_DEFAULTS=" + ";".join(["../sdkconfig.common", "sdkconfig.defaults", *extra])]
    cwd = ROOT / proj
    # no primeiro build de um diretório novo o IDF às vezes pede "re-run" por causa do sdkconfig fora do padrão
    if subprocess.run(idf + ["build"], cwd=cwd).returncode:
        subprocess.run(idf + ["build"], cwd=cwd, check=True)
    if port:
        idf += ["-p", port]
    subprocess.run(idf + ["flash"], cwd=cwd, check=True)
    (capture_demo if base.endswith("-demo") else capture)(port, variant)


def read_line(ser, log):
    """Lê uma linha; se a placa reiniciar (crash), para com as últimas linhas de log em vez de esperar para sempre."""
    line = ser.readline().decode(errors="replace").strip()
    if not line.startswith("F,"):
        log.append(line)
        del log[:-15]
    if "Rebooting..." in line:
        sys.exit("a placa reiniciou no meio da captura:\n" + "\n".join(log))
    return line


def capture(port=None, name=None):
    ser = open_port(port)
    print(f"aguardando o próximo ciclo em {ser.port}...")
    head, rows, log = None, [], []
    while True:
        line = read_line(ser, log)
        if line.startswith("B,"):
            head, rows = line.split(",")[1:], []
        elif head and line.startswith("F,"):
            rows.append(line.split(",")[1:])
        elif head and line.startswith("E,"):
            assert len(rows) == int(line[2:]), f"recebi {len(rows)} de {line[2:]} linhas: a porta perdeu dados"
            break
    out = ROOT / "results" / f"{name or f'{head[0]}-u{head[1]}'}.csv"
    out.parent.mkdir(exist_ok=True)
    with open(out, "w", newline="") as f:
        csv.writer(f).writerows([["scene", "mode", "frame", "us"], *rows])
    print("salvo em", out)
    compare([out])


def capture_demo(port, name):
    ser = open_port(port)
    print(f"aguardando o resumo do lv_demo_benchmark em {ser.port} (leva alguns minutos)...")
    lines, idle, log = [], 0, []
    while idle < 3:  # o resumo não tem marcador de fim: para após 3 s sem linhas novas
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
    print("salvo em", out)


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
    print(f"{'cena':<12}{'modo':<8}" + "".join(f"{n:>{w}}" for n in names))
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
