"""Generates the README charts from results/*.csv, no dependencies: python docs/charts.py

Also prints the Markdown tables used in the README."""
import csv
import math
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "results"
CLOCKS = [("", 6.67), ("@10", 10), ("@16", 16), ("@20", 20), ("@26.67", 26.67), ("@40", 40)]  # PCLK real = 80 MHz / n
SCENES = ["fill", "gradient", "rects", "rects_opa", "shadow", "text", "image", "image_opa", "rotate", "arc",
          "grad_rot", "ui"]

# Validated palette (dataviz): series 1 = v8, series 2 = v9; the dark theme follows GitHub's.
STYLE = """<style>
svg{--bg:#fcfcfb;--ink:#0b0b0b;--ink2:#52514e;--muted:#898781;--grid:#e1e0d9;--axis:#c3c2b7;--s1:#2a78d6;--s2:#eb6834}
@media (prefers-color-scheme:dark){svg{--bg:#1a1a19;--ink:#f0efec;--ink2:#c3c2b7;--grid:#2c2c2a;--axis:#383835;--s1:#3987e5;--s2:#d95926}}
text{font:12px system-ui,-apple-system,'Segoe UI',sans-serif;fill:var(--ink2);font-variant-numeric:tabular-nums}
.t{font-size:15px;font-weight:600;fill:var(--ink)}.h{font-size:13px;font-weight:600;fill:var(--ink)}.m{fill:var(--muted)}
.g{stroke:var(--grid)}.a{stroke:var(--axis)}.s1{fill:var(--s1)}.s2{fill:var(--s2)}
.l1,.l2{fill:none;stroke-width:2}.l1{stroke:var(--s1)}.l2{stroke:var(--s2)}.d1,.d2{stroke:var(--bg);stroke-width:2}
.d1{fill:var(--s1)}.d2{fill:var(--s2)}
</style>"""


def load(name):
    path = RES / f"{name}.csv"
    if not path.exists():
        return None
    t = {}
    with open(path) as f:
        for r in csv.DictReader(f):
            t.setdefault((r["scene"], r["mode"]), []).append(int(r["us"]) / 1000)
    return {k: statistics.mean(v) for k, v in t.items()}


def svg(w, h, body):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">{STYLE}'
            f'<rect width="{w}" height="{h}" rx="8" style="fill:var(--bg)"/>{"".join(body)}</svg>\n')


def bar(x, y, w, h, cls, tip):
    r = min(4, w / 2, h / 2)  # rounded tip, flat base
    d = f"M{x:.1f},{y:.1f}h{w - r:.1f}a{r:.1f},{r:.1f} 0 0 1 {r:.1f},{r:.1f}v{h - 2 * r:.1f}" \
        f"a{r:.1f},{r:.1f} 0 0 1 -{r:.1f},{r:.1f}h-{w - r:.1f}z"
    return f'<path class="{cls}" d="{d}"><title>{tip}</title></path>'


def legend(x, y):
    return (f'<rect class="s1" x="{x}" y="{y - 9}" width="10" height="10" rx="2"/><text x="{x + 15}" y="{y}">LVGL 8.4.0</text>'
            f'<rect class="s2" x="{x + 100}" y="{y - 9}" width="10" height="10" rx="2"/><text x="{x + 115}" y="{y}">LVGL 9.6.0</text>')


def nice(v, step):
    return math.ceil(v / step) * step


def render_chart(v8, v9):
    W, L, R, top, row = 780, 96, 64, 84, 30
    pw = W - L - R
    vmax = nice(max(max(v8[(s, "render")], v9[(s, "render")]) for s in SCENES), 10)
    x = lambda v: L + v / vmax * pw
    h = top + row * len(SCENES) + 34
    b = [f'<text class="t" x="20" y="28">Render time per frame (ms, lower is better)</text>',
         f'<text class="m" x="20" y="47">ESP32-S3 at 240 MHz · full screen 320×170 · CPU only (pixels discarded) · average of 60 frames</text>',
         legend(20, 70)]
    for t in range(0, vmax + 1, 10):
        b.append(f'<line class="{"a" if t == 0 else "g"}" x1="{x(t):.1f}" x2="{x(t):.1f}" y1="{top - 6}" y2="{h - 28}"/>')
        b.append(f'<text class="m" x="{x(t):.1f}" y="{h - 12}" text-anchor="middle">{t}</text>')
    for i, s in enumerate(SCENES):
        y = top + i * row
        a, c = v8[(s, "render")], v9[(s, "render")]
        b.append(f'<text x="{L - 10}" y="{y + 16}" text-anchor="end">{s}</text>')
        b.append(bar(L, y + 2, x(a) - L, 10, "s1", f"{s} · v8: {a:.2f} ms"))
        b.append(bar(L, y + 14, x(c) - L, 10, "s2", f"{s} · v9: {c:.2f} ms"))
        d = 100 * (c / a - 1)
        b.append(f'<text x="{x(max(a, c)) + 6:.1f}" y="{y + 16}">{d:+.0f}%</text>'.replace("-", "−"))
    return svg(W, h, b)


def fps_chart(series):
    picks = [("fill", "fill · light, bus-limited"), ("text", "text · anti-aliased text"),
             ("ui", "ui · widget interface"), ("grad_rot", "grad_rot · rotated layers")]
    W, PW, PH, top = 780, 370, 210, 84
    xs = lambda mhz, ox: ox + 44 + (mhz - 5) / 35 * (PW - 84)
    b = [f'<text class="t" x="20" y="28">Frames per second with LCD transfer × bus clock (PCLK)</text>',
         f'<text class="m" x="20" y="47">Same CPU (240 MHz); only the 8-bit i80 bus clock changes · X axis in MHz</text>',
         legend(20, 70)]
    for k, (scene, title) in enumerate(picks):
        ox, oy = 20 + (k % 2) * (PW + 10), top + (k // 2) * (PH + 16)
        pts = {v: [(mhz, 1000 / d[(scene, "lcd")]) for (sfx, mhz) in CLOCKS if (d := series[v].get(sfx))] for v in (8, 9)}
        ymax = nice(max(f for p in pts.values() for _, f in p) * 1.05, 20)
        ys = lambda f: oy + PH - 30 - f / ymax * (PH - 64)
        b.append(f'<text class="h" x="{ox}" y="{oy + 14}">{title}</text>')
        for t in range(0, ymax + 1, ymax // 4):
            b.append(f'<line class="{"a" if t == 0 else "g"}" x1="{ox + 44}" x2="{ox + PW - 40}" y1="{ys(t):.1f}" y2="{ys(t):.1f}"/>')
            b.append(f'<text class="m" x="{ox + 38}" y="{ys(t) + 4:.1f}" text-anchor="end">{t}</text>')
        for mhz in (10, 20, 30, 40):
            b.append(f'<text class="m" x="{xs(mhz, ox):.1f}" y="{oy + PH - 12}" text-anchor="middle">{mhz}</text>')
        ends = {}
        for v, cls in ((8, "1"), (9, "2")):
            p = pts[v]
            b.append(f'<polyline class="l{cls}" points="{" ".join(f"{xs(m, ox):.1f},{ys(f):.1f}" for m, f in p)}"/>')
            for m, f in p:
                b.append(f'<circle class="d{cls}" cx="{xs(m, ox):.1f}" cy="{ys(f):.1f}" r="4"><title>{scene} · v{v} · {m} MHz: {f:.0f} FPS</title></circle>')
            ends[v] = (xs(p[-1][0], ox) + 8, ys(p[-1][1]), p[-1][1])
        if abs(ends[8][1] - ends[9][1]) >= 14:
            for v in (8, 9):
                b.append(f'<text x="{ends[v][0]:.1f}" y="{ends[v][1] + 4:.1f}">v{v} {ends[v][2]:.0f}</text>')
        else:  # lines close together: a single label, no stacking
            b.append(f'<text x="{ends[8][0]:.1f}" y="{ends[8][1] + 4:.1f}">{ends[8][2]:.0f}</text>')
    return svg(W, top + 2 * (PH + 16), b)


def md_tables(series, v8, v9):
    print("| scene | v8 (ms) | v9 (ms) | v9 vs v8 |\n|---|---:|---:|---:|")
    for s in SCENES:
        a, c = v8[(s, "render")], v9[(s, "render")]
        print(f"| `{s}` | {a:.2f} | {c:.2f} | {100 * (c / a - 1):+.0f}% |")
    for v in (8, 9):
        print(f"\nFPS with LCD transfer, LVGL {v}:\n\n| scene | " + " | ".join(f"{m} MHz" for _, m in CLOCKS) + " |")
        print("|---|" + "---:|" * len(CLOCKS))
        for s in SCENES:
            print(f"| `{s}` | " + " | ".join(f"{1000 / d[(s, 'lcd')]:.0f}" if (d := series[v].get(sfx)) else "–"
                                         for sfx, _ in CLOCKS) + " |")
    mt = {n: load(n) for n in ("v9", "v9-freertos", "v9-mt")}
    if all(mt.values()):
        print("\n| scene | v9 (ms) | v9 + FreeRTOS | v9 + 2 cores |\n|---|---:|---:|---:|")
        for s in SCENES:
            a = mt["v9"][(s, "render")]
            print(f"| `{s}` | {a:.2f} | " + " | ".join(f"{mt[n][(s, 'render')]:.2f} ({100 * (mt[n][(s, 'render')] / a - 1):+.0f}%)"
                                               for n in ("v9-freertos", "v9-mt")) + " |")


if __name__ == "__main__":
    series = {v: {sfx: load(f"v{v}{sfx}") for sfx, _ in CLOCKS} for v in (8, 9)}
    v8, v9 = series[8][""], series[9][""]
    (ROOT / "docs" / "render-v8-v9.svg").write_text(render_chart(v8, v9), encoding="utf-8")
    (ROOT / "docs" / "fps-pclk.svg").write_text(fps_chart(series), encoding="utf-8")
    md_tables(series, v8, v9)
