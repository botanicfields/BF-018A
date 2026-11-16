#!/usr/bin/env python3
"""Physics model of the BF-018A antenna board: carrier current, magnetic field, range, and battery life.

    python3 tools/antenna_model.py          # print the tables and write the figures in docs/analysis/

Everything here is calculated from the schematic (bf-018a_scm_v01l02.pdf), the coil in the Gerber files
(PCBV01L02/gerber/JLCPCB/bf-018al02.zip) and published figures. It is calibrated against the two
measurements BotanicFields published, and it is an estimate: nothing here has been measured on this board
by this project. See README.md section 10 for what the numbers mean.

The parts:
1. Drive: the GPIO's 50% square wave goes through two RC low-pass stages (R1/C3, R2/C4: about 16 kHz each),
   a high-pass coupling stage (C5/R5), and a non-inverting amplifier (U1B, gain 1 + R6/R7 = 6.1) into
   C8, R8 (510 ohms) and the printed coil. Only the fundamental of the square wave matters to a receiver.
2. Coil: a two-layer printed spiral, 10 turns per layer from 12.4 to 17.4 mm radius (measured from the
   Gerbers). Its magnetic moment is current x total turn area.
3. Field: up to a few metres from a 40-60 kHz source, everything is deep in the near field (the wavelength
   is 5-7.5 km), so the field is the quasi-static field of a magnetic dipole and falls as 1/r^3: 18 dB
   per doubling of distance. A clock's ferrite-bar antenna responds to the magnetic field along the bar.
   Receiver sensitivities are quoted as the electric field of a radio wave, E = c x B, so the magnetic
   field is expressed as that "equivalent field strength" in dB(uV/m).
4. Calibration: the BF-018A manual and the Qiita article report a clock syncing at 70 cm but not 90 cm with
   the board in line with the clock's bar antenna, at 40 kHz in Japan.
5. Power: ESP32 datasheet currents, the regulator, the USB serial chip, LED and op-amp, and the antenna.
"""
import cmath
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "analysis"

MU0 = 4e-7 * math.pi
C_LIGHT = 299_792_458.0

# --- 1. drive circuit (bf-018a_scm_v01l02.pdf) ---
V_GPIO = 3.3                  # GPIO high level; LEDC duty 50% at every resolution
R1 = R2 = 1e3
C3 = C4 = 10e-9
C5, R5 = 10e-9, 10e3          # high-pass into U1B; R5 returns to the VDD/2 bias from U1A (AC ground)
R6, R7, C7 = 51e3, 10e3, 10e-9  # U1B gain 1 + R6 / (R7 + 1/jwC7)
C8, R8 = 100e-9, 510.0

# --- 2. coil (measured from the Gerber files) ---
TURNS_PER_LAYER = 10
R_INNER, R_OUTER = 12.39e-3, 17.39e-3
LAYER_GAP = 1.6e-3            # board thickness: assumed 1.6 mm
TRACE_W, TRACE_T = 0.25e-3, 35e-6  # 0.25 mm trace; 1 oz copper assumed
TRACE_LENGTH = 2 * 0.942      # m, both layers
AREA_SUM = 2 * 71.3e-4        # m^2, sum of the enclosed area of every turn, both layers (from the Gerbers)
RHO_CU = 1.72e-8


def turns():
    """(radius, z) of each turn, both layers; the two layers carry the current the same way round."""
    pitch = (R_OUTER - R_INNER) / TURNS_PER_LAYER
    radii = [R_INNER + (k + 0.5) * pitch for k in range(TURNS_PER_LAYER)]
    return [(a, z) for z in (-LAYER_GAP / 2, LAYER_GAP / 2) for a in radii]


def ellip_ke(m):
    """Complete elliptic integrals K(m), E(m) by the arithmetic-geometric mean."""
    a, b, c = 1.0, math.sqrt(1 - m), math.sqrt(m)
    s, p = c * c / 2, 1.0
    while abs(c) > 1e-15:
        a, b, c = (a + b) / 2, math.sqrt(a * b), (a - b) / 2
        p *= 2
        s += p * c * c / 2
    k = math.pi / (2 * a)
    return k, k * (1 - s)


def mutual(a1, a2, d):
    """Mutual inductance of two coaxial circular filaments (Maxwell)."""
    m = 4 * a1 * a2 / ((a1 + a2) ** 2 + d * d)
    k, e = ellip_ke(m)
    kk = math.sqrt(m)
    return MU0 * math.sqrt(a1 * a2) * ((2 / kk - kk) * k - 2 / kk * e)


def coil_inductance():
    gmd = 0.2235 * (TRACE_W + TRACE_T)  # geometric mean distance of a thin rectangular trace
    t = turns()
    total = 0.0
    for i, (a1, z1) in enumerate(t):
        for j, (a2, z2) in enumerate(t):
            if i == j:
                total += MU0 * a1 * (math.log(8 * a1 / gmd) - 2)
            else:
                total += mutual(a1, a2, max(abs(z1 - z2), 0.0) or 1e-9) if (a1 != a2 or z1 != z2) else 0.0
    return total


def coil_resistance():
    return RHO_CU * TRACE_LENGTH / (TRACE_W * TRACE_T)


def drive(freq, harmonic=1):
    """Peak amplitudes at the amplifier input and in the coil, for one harmonic of the square wave."""
    w = 2 * math.pi * freq * harmonic
    vs = 2 * V_GPIO / (math.pi * harmonic)  # Fourier amplitude of a 0..V square wave, odd harmonics
    y = lambda c: 1j * w * c
    # nodal equations for A (C3), B (C4) and C (U1B input): M x = b
    m = [[1 / R1 + y(C3) + 1 / R2, -1 / R2, 0],
         [-1 / R2, 1 / R2 + y(C4) + y(C5), -y(C5)],
         [0, -y(C5), y(C5) + 1 / R5]]
    b = [vs / R1, 0, 0]
    va, vb, vc = solve3(m, b)
    gain = 1 + R6 / (R7 + 1 / y(C7))
    vout = gain * vc
    z_load = 1 / y(C8) + R8 + coil_resistance() + 1j * w * coil_inductance()
    i = vout / z_load
    return {"vs": vs, "v_in": abs(vc), "gain": abs(gain), "v_out": abs(vout), "i": abs(i), "v_r8": abs(i) * R8}


def solve3(m, b):
    m = [row[:] + [bb] for row, bb in zip(m, b)]
    n = 3
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(m[r][c]))
        m[c], m[p] = m[p], m[c]
        for r in range(n):
            if r != c:
                f = m[r][c] / m[c][c]
                m[r] = [x - f * y for x, y in zip(m[r], m[c])]
    return [m[r][n] / m[r][r] for r in range(n)]


# --- 3. field ---
def b_axial(i_peak, r):
    """Peak field on the coil axis at distance r from the board, exact for the turns (not a dipole)."""
    return sum(MU0 * i_peak * a * a / (2 * (a * a + (r - z) ** 2) ** 1.5) for a, z in turns())


def b_dipole(i_peak, r, factor):
    """Dipole field: factor 2 on the axis, 1 in the board's plane (side by side)."""
    return MU0 * i_peak * AREA_SUM / (4 * math.pi * r ** 3) * factor


def dbuv(b_peak):
    """Equivalent field strength of a peak magnetic field: E = c B, rms, in dB(uV/m)."""
    return 20 * math.log10(C_LIGHT * b_peak / math.sqrt(2) / 1e-6)


def distance_for(i_peak, threshold_db, factor=2.0):
    """Distance at which the field falls to the threshold (on the axis: factor 2; side by side: 1)."""
    lo, hi = 0.005, 50.0
    f = (lambda r: dbuv(b_axial(i_peak, r))) if factor == 2.0 else (lambda r: dbuv(b_dipole(i_peak, r, factor)))
    for _ in range(100):
        mid = math.sqrt(lo * hi)
        if f(mid) > threshold_db:
            lo = mid
        else:
            hi = mid
    return lo


# --- 5. power ---
# Current at 3.3 V for the ESP32 with Wi-Fi connected and power saving off (the firmware calls WiFi.setSleep(false),
# so the receiver never sleeps): the datasheet's RX current with the CPU idle is 113 mA (ESP32-PICO series datasheet,
# table 19), plus the CPU, the 10 ms Ticker, the web server and occasional transmits. A DevKit measured 145 mA in
# this mode (mischianti.org), which includes a USB bridge and LDO. ESP32-S3 is taken as about the same.
ESP_MA = {"low": 110, "mid": 125, "high": 145}
BOARDS = {
    # name: (5 V overhead at the board in mA: USB serial chip, LED, LDO quiescent; regulator efficiency or None for linear)
    "ATOM Lite": {"overhead": 8, "eff": None},       # USB serial chip and LED; regulator taken as linear (upper bound)
    "ATOMS3 Lite": {"overhead": 2, "eff": 0.88},     # native USB; SY8089 buck converter
}
BF018A_MA = 2.0 + 0.05  # MCP6022: 1 mA per amplifier, both run; bias divider 5 V / 102 kOhm
CARRIER_DUTY = (7 * 0.2 + 26 * 0.5 + 27 * 0.8) / 60  # share of time the carrier is on, a typical minute (about 60%)


def supply_ma(board, esp_ma, coil_ipk):
    b = BOARDS[board]
    at_5v = esp_ma if b["eff"] is None else esp_ma * 3.3 / (5.0 * b["eff"])
    antenna = coil_ipk * 1e3 / math.pi * CARRIER_DUTY  # rail-to-rail output stage sourcing half of each cycle
    return at_5v + b["overhead"] + BF018A_MA + antenna


def battery_hours(cell_mah, load_ma, boost_eff=0.85):
    """A power bank: capacity in mAh at the 3.7 V cell, a boost converter to 5 V."""
    return cell_mah * 3.7 * boost_eff / (5.0 * load_ma)


# --- figures (SVG, light and dark) ---
STYLE = """<style>
  .bg{fill:#fcfcfb} .ink{fill:#0b0b0b} .ink2{fill:#52514e} .ink3{fill:#7a7974}
  .grid{stroke:#e4e3df} .axis{stroke:#a3a29c} .band{fill:#ecebe7}
  .s1{stroke:#2a78d6} .s2{stroke:#eb6834} .f1{fill:#2a78d6} .f2{fill:#eb6834}
  .zone{fill:#2a78d6;fill-opacity:.10} .zone2{fill:#eb6834;fill-opacity:.10} .ref{stroke:#52514e}
  .board{fill:#1b6b3a} .copper{stroke:#d9a441} .clock{fill:#fcfcfb;stroke:#52514e} .bar{fill:#52514e}
  @media (prefers-color-scheme: dark){
    .bg{fill:#1a1a19} .ink{fill:#f0efec} .ink2{fill:#c3c2b7} .ink3{fill:#94938c}
    .grid{stroke:#2f2f2d} .axis{stroke:#5d5c57} .band{fill:#262624}
    .s1{stroke:#3987e5} .s2{stroke:#d95926} .f1{fill:#3987e5} .f2{fill:#d95926}
    .zone{fill:#3987e5;fill-opacity:.16} .zone2{fill:#d95926;fill-opacity:.16} .ref{stroke:#c3c2b7}
    .clock{fill:#1a1a19;stroke:#c3c2b7} .bar{fill:#c3c2b7}
  }
  text{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
</style>"""


def svg(width, height, body, title):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}" '
            f'role="img" aria-label="{title}">\n<title>{title}</title>\n{STYLE}\n'
            f'<rect class="bg" width="{width}" height="{height}" rx="8"/>\n{body}\n</svg>\n')


def text(x, y, s, cls="ink2", size=13, anchor="start", weight="normal"):
    return f'<text x="{x:.1f}" y="{y:.1f}" class="{cls}" font-size="{size}" text-anchor="{anchor}" font-weight="{weight}">{s}</text>'


def figure_field(i40, i60, thresholds):
    w, h = 760, 470
    l, r, t, b = 70, 150, 56, 62
    xmin, xmax = 0.05, 5.0
    ymin, ymax = 30, 130
    X = lambda d: l + (math.log10(d) - math.log10(xmin)) / (math.log10(xmax) - math.log10(xmin)) * (w - l - r)
    Y = lambda v: t + (ymax - v) / (ymax - ymin) * (h - t - b)
    out = [text(l, 26, "Equivalent field strength from the BF-018A coil", "ink", 16, weight="600"),
           text(l, 44, "Clock's bar antenna in line with the coil (best) and side by side (in the board's plane)", "ink2", 12)]
    # real JJY band
    out.append(f'<rect class="band" x="{l}" y="{Y(62):.1f}" width="{w - l - r}" height="{Y(40) - Y(62):.1f}"/>')
    out.append(text(l + 8, Y(40) - 6, "Real JJY across Japan: about 40 to over 60 dB(µV/m) (NICT)", "ink3", 11))
    for v in range(ymin, ymax + 1, 10):
        out.append(f'<line class="grid" x1="{l}" x2="{w - r}" y1="{Y(v):.1f}" y2="{Y(v):.1f}"/>')
        out.append(text(l - 8, Y(v) + 4, str(v), "ink3", 11, "end"))
    for d, lab in ((0.05, "5 cm"), (0.1, "10 cm"), (0.2, "20 cm"), (0.5, "50 cm"), (1, "1 m"), (2, "2 m"), (5, "5 m")):
        out.append(f'<line class="grid" x1="{X(d):.1f}" x2="{X(d):.1f}" y1="{t}" y2="{h - b}"/>')
        out.append(text(X(d), h - b + 18, lab, "ink3", 11, "middle"))
    out.append(f'<line class="axis" x1="{l}" x2="{w - r}" y1="{h - b}" y2="{h - b}"/>')
    out.append(text((l + w - r) / 2, h - 16, "Distance from the board to the clock (log scale)", "ink2", 12, "middle"))
    out.append(f'<text x="18" y="{(t + h - b) / 2:.0f}" class="ink2" font-size="12" text-anchor="middle" '
               f'transform="rotate(-90 18 {(t + h - b) / 2:.0f})">dB(µV/m), equivalent</text>')
    # thresholds
    for name, v in thresholds:
        out.append(f'<line class="ref" stroke-width="1" stroke-dasharray="2 4" x1="{l}" x2="{w - r}" y1="{Y(v):.1f}" y2="{Y(v):.1f}"/>')
        out.append(text(w - r + 8, Y(v) + 4, name, "ink2", 11))
    # curves
    ds = [xmin * (xmax / xmin) ** (k / 200) for k in range(201)]
    for cls, ip in (("s1", i40), ("s2", i60)):
        for dash, fn in (("", lambda d: dbuv(b_axial(ip, d))), ("6 5", lambda d: dbuv(b_dipole(ip, d, 1.0)))):
            pts = " ".join(f"{X(d):.1f},{Y(min(ymax, max(ymin, fn(d)))):.1f}" for d in ds if ymin <= fn(d) <= ymax + 20)
            out.append(f'<polyline class="{cls}" fill="none" stroke-width="2" stroke-linecap="round" '
                       f'{"stroke-dasharray=" + chr(34) + dash + chr(34) if dash else ""} points="{pts}"/>')
    # legend
    lx, ly = w - r - 190, t + 16
    out.append(f'<rect class="bg" x="{lx - 8}" y="{ly - 14}" width="190" height="76" rx="6" opacity=".92"/>')
    for k, (cls, lab) in enumerate((("s1", "40 kHz"), ("s2", "60 kHz"))):
        out.append(f'<line class="{cls}" stroke-width="2" x1="{lx}" x2="{lx + 22}" y1="{ly + k * 18}" y2="{ly + k * 18}"/>')
        out.append(text(lx + 30, ly + k * 18 + 4, lab, "ink", 12))
    out.append(f'<line class="ref" stroke-width="2" x1="{lx}" x2="{lx + 22}" y1="{ly + 38}" y2="{ly + 38}"/>')
    out.append(text(lx + 30, ly + 42, "solid: in line (best)", "ink2", 11))
    out.append(f'<line class="ref" stroke-width="2" stroke-dasharray="6 5" x1="{lx}" x2="{lx + 22}" y1="{ly + 54}" y2="{ly + 54}"/>')
    out.append(text(lx + 30, ly + 58, "dashed: side by side", "ink2", 11))
    out.append(text(w - r + 8, Y(50) + 22, "* no station on", "ink3", 10))
    out.append(text(w - r + 8, Y(50) + 35, "the same frequency", "ink3", 10))
    return svg(w, h, "\n".join(out), "Equivalent field strength from the BF-018A coil against distance, 40 and 60 kHz")


def figure_placement(d40, d60):
    """Top view: where a clock can sit, for a bar antenna parallel to the coil axis."""
    w, h = 760, 540
    cx, cy = 380, 300
    scale = 190 / max(d40, d60)  # px per metre
    out = [text(24, 28, "Where to put the clock: reach around the board", "ink", 16, weight="600"),
           text(24, 46, "Top view, the clock's bar antenna parallel to the coil's axis (left-right here). The outline is the",
                "ink2", 12),
           text(24, 62, "farthest a clock syncs, for the clock BotanicFields measured in Japan (70 cm yes, 90 cm no, at 40 kHz).",
                "ink2", 12)]
    for rr in (0.25, 0.5, 0.75, 1.0):
        if rr * scale < 225:
            out.append(f'<circle class="grid" fill="none" cx="{cx}" cy="{cy}" r="{rr * scale:.1f}"/>')
            out.append(text(cx + 4, cy - rr * scale - 4, f"{int(rr * 100)} cm", "ink3", 10))
    for cls, zone, d in (("s1", "zone", d40), ("s2", "zone2", d60)):
        pts = []
        for k in range(721):
            th = 2 * math.pi * k / 720
            g = abs(3 * math.cos(th) ** 2 - 1) / 2  # field along the axis direction, relative to on-axis
            rr = d * g ** (1 / 3) * scale
            pts.append(f"{cx + rr * math.cos(th):.1f},{cy - rr * math.sin(th):.1f}")
        out.append(f'<polygon class="{zone} {cls}" stroke-width="2" points="{" ".join(pts)}"/>')
    # dead-zone rays at 54.7 degrees
    th0 = math.degrees(math.acos(1 / math.sqrt(3)))
    for sgn_x in (1, -1):
        for sgn_y in (1, -1):
            th = math.radians(th0)
            x2, y2 = cx + sgn_x * 235 * math.cos(th), cy - sgn_y * 235 * math.sin(th)
            out.append(f'<line class="ref" stroke-width="1" stroke-dasharray="3 4" x1="{cx}" y1="{cy}" x2="{x2:.1f}" y2="{y2:.1f}"/>')
    out.append(text(cx + 150, cy - 215, "dead zone (55° off axis): no signal", "ink2", 11))
    # board (edge-on in top view: a thin bar with the coil axis left-right)
    out.append(f'<rect class="board" x="{cx - 4}" y="{cy - 24}" width="8" height="48" rx="2"/>')
    out.append(f'<line class="copper" stroke-width="3" x1="{cx}" y1="{cy - 18}" x2="{cx}" y2="{cy + 18}"/>')
    out.append(text(cx, cy + 44, "board", "ink2", 11, "middle"))
    out.append(f'<line class="ref" stroke-width="1" x1="{cx - 230}" y1="{cy}" x2="{cx + 230}" y2="{cy}"/>')

    def clock(x, y, label, sub, horizontal=True):
        bw, bh = (34, 8) if horizontal else (8, 34)
        s = [f'<rect class="clock" x="{x - 26}" y="{y - 22}" width="52" height="44" rx="8"/>',
             f'<rect class="bar" x="{x - bw / 2:.1f}" y="{y - bh / 2:.1f}" width="{bw}" height="{bh}" rx="3"/>',
             text(x, y + 38, label, "ink", 12, "middle", "600"), text(x, y + 53, sub, "ink2", 11, "middle")]
        return "\n".join(s)
    out.append(clock(cx + 0.55 * d40 * scale, cy, "Best", "in line"))
    out.append(clock(cx - 0.55 * d40 * scale, cy, "Best", "in line"))
    out.append(clock(cx, cy - 0.55 * d40 * scale * 0.79, "Fair", "side by side"))
    out.append(clock(cx, cy + 0.55 * d40 * scale * 0.79, "Fair", "side by side"))
    # legend
    lx, ly = 24, h - 46
    out.append(f'<rect class="zone s1" stroke-width="2" x="{lx}" y="{ly - 10}" width="18" height="12"/>')
    out.append(text(lx + 26, ly + 1, f"40 kHz: up to {d40 * 100:.0f} cm in line", "ink", 12))
    out.append(f'<rect class="zone2 s2" stroke-width="2" x="{lx + 230}" y="{ly - 10}" width="18" height="12"/>')
    out.append(text(lx + 256, ly + 1, f"60 kHz: up to {d60 * 100:.0f} cm in line (same clock threshold)", "ink", 12))
    out.append(text(lx, ly + 22, "Grey bar in each clock: its ferrite-bar antenna. Turn a clock 90° and the in-line spots become "
                    "the weak ones.", "ink2", 11))
    return svg(w, h, "\n".join(out), "Top view of where a clock can sit around the BF-018A board")


def main():
    l = coil_inductance()
    rc = coil_resistance()
    d40, d60 = drive(40e3), drive(60e3)
    print("## Drive (fundamental, peak)")
    print(f"coil: {2 * TURNS_PER_LAYER} turns, sum of turn areas {AREA_SUM * 1e4:.1f} cm^2, L = {l * 1e6:.1f} uH, R = {rc:.1f} ohm")
    for f, d in ((40, d40), (60, d60)):
        print(f"{f} kHz: square-wave fundamental {d['vs']:.2f} V, amp input {d['v_in'] * 1e3:.0f} mV "
              f"({2 * d['v_in'] * 1e3:.0f} mVpp), gain {d['gain']:.2f}, output {d['v_out']:.2f} V, "
              f"coil current {d['i'] * 1e3:.2f} mA peak, across R8 {2 * d['v_r8']:.2f} Vpp")
    print(f"article, 40 kHz: amp input 440 mVpp, across R8 2.74 Vpp")
    rel = 20 * math.log10(d60["i"] / d40["i"])
    print(f"60 kHz vs 40 kHz coil current: {rel:+.1f} dB")
    h3 = drive(40e3, 3)
    print(f"3rd harmonic (120 kHz) coil current: {20 * math.log10(h3['i'] / d40['i']):+.1f} dB below the 40 kHz carrier")

    # calibration: in line, 70 cm works, 90 cm does not (40 kHz, Japan)
    t70, t90 = dbuv(b_axial(d40["i"], 0.70)), dbuv(b_axial(d40["i"], 0.90))
    t_cal = (t70 + t90) / 2
    print(f"\n## Calibration\nfield in line at 70 cm {t70:.1f}, at 90 cm {t90:.1f} dB(uV/m): threshold about {t_cal:.0f}")
    t30, t50 = dbuv(b_dipole(d40["i"], 0.30, 1.0)), dbuv(b_dipole(d40["i"], 0.50, 1.0))
    print(f"side by side, 30 cm {t30:.1f}, 50 cm {t50:.1f} dB(uV/m)")

    scenarios = [("Measured clock in Japan", round(t_cal)),
                 ("No same-frequency station, typical room", 60),
                 ("No same-frequency station, quiet room", 50)]
    print("\n## Distances (cm): in line / side by side / comfortable (10 dB margin, in line)")
    rows = []
    for name, thr in scenarios:
        for f, d in ((40, d40), (60, d60)):
            a = distance_for(d["i"], thr)
            s = distance_for(d["i"], thr, 1.0)
            c = distance_for(d["i"], thr + 10)
            rows.append((name, thr, f, a, s, c))
            print(f"{name} ({thr} dB) {f} kHz: {a * 100:.0f} / {s * 100:.0f} / {c * 100:.0f}")

    print("\n## Field at 3 m in line (Japan's limit for licence-free low-power emitters: 500 uV/m at 3 m, 54 dB)")
    for f, d in ((40, d40), (60, d60)):
        print(f"{f} kHz: {dbuv(b_axial(d['i'], 3.0)):.1f} dB(uV/m)")

    print("\n## Power at 5 V (mA): low / mid / high")
    loads = {}
    for board in BOARDS:
        v = [supply_ma(board, ESP_MA[k], d40["i"]) for k in ("low", "mid", "high")]
        loads[board] = v
        print(f"{board}: " + " / ".join(f"{x:.0f}" for x in v))
    ant = d40["i"] * 1e3 / math.pi * CARRIER_DUTY
    print(f"antenna share: {ant:.2f} mA average (carrier on {CARRIER_DUTY * 100:.0f}% of the time), op-amps {BF018A_MA:.1f} mA")
    print("\n## Battery (power bank, 3.7 V cells, 85% boost): hours, low load / mid / high load")
    for cap in (1000, 5000, 10000):
        for board, v in loads.items():
            hrs = [battery_hours(cap, x) for x in v]
            print(f"{cap} mAh, {board}: {hrs[0]:.1f} / {hrs[1]:.1f} / {hrs[2]:.1f}")

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "field-vs-distance.svg").write_text(figure_field(
        d40["i"], d60["i"], [(f"{v} dB: {short}", v) for (n, v), short in
                             zip(scenarios, ("measured clock", "typical room*", "quiet room*"))]), encoding="utf-8")
    (OUT / "placement.svg").write_text(figure_placement(
        distance_for(d40["i"], round(t_cal)), distance_for(d60["i"], round(t_cal))), encoding="utf-8")
    print(f"\nwrote {OUT.relative_to(ROOT)}/field-vs-distance.svg and placement.svg")


if __name__ == "__main__":
    main()
