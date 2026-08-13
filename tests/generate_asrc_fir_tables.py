#!/usr/bin/env python3
"""Deterministic generator for the ASRC polyphase FIR tables.

Produces two headers:
  h36_14_fir8_q14.h   - 160 phases x 8 taps,  Lanczos-4 (U2.63 deployed table)
  h36_14_fir16_q14.h  - 160 phases x 16 taps, Lanczos-8 (maximum quality build)

Convention (verified against the deployed U2.63 table, maxerr=0 at phases
0,1,2,5,80): for phase p, tap t, the kernel is evaluated at
    x = t - half - p/160,   half = (TAPS - 2) / 2
i.e. input sample index fi = idx - half + t for output position idx + p/160.

Every row sums exactly to 16384 (Q14) to preserve DC gain.

NOTE: the deployed U2.63 header duplicates phase 81 into phase 79 (row 79 ==
row 81 exactly). This script regenerates phase 79 correctly; the numerical
difference is tiny (phase shift of 2/160 of a sample) and inaudible, but the
clean table is used as the baseline for the FIR16 comparison below.
"""
import math

PHASES = 160
AMP = 16384


def lanczos(x, lobes):
    if abs(x) >= lobes:
        return 0.0
    if x == 0:
        return 1.0
    sx = math.sin(math.pi * x) / (math.pi * x)
    sw = math.sin(math.pi * x / lobes) / (math.pi * x / lobes)
    return sx * sw


def gen_rows(taps, lobes):
    half = (taps - 2) / 2.0
    rows = []
    for p in range(PHASES):
        raw = [lanczos(t - half - p / PHASES, lobes) for t in range(taps)]
        s = sum(raw)
        if s == 0:
            raw = [0.0] * taps
        else:
            raw = [v / s for v in raw]
        rows.append(raw)
    return rows


def rows_to_q14(rows):
    out = []
    for raw in rows:
        q = [int(round(v * AMP)) for v in raw]
        diff = AMP - sum(q)
        q[-1] += diff
        out.append(q)
    return out


def emit_header(taps, rows, path):
    name = "kTreeFrogFrontendFir%dQ14" % taps
    guard = "TREEFROG_FRONTEND_FIR%d_COEFFICIENTS_H" % taps
    lobes = taps // 2
    with open(path, "w") as f:
        f.write("#ifndef %s\n#define %s\n" % (guard, guard))
        f.write("#include <stdint.h>\n")
        f.write("/* %d fractional phases, %d-tap Lanczos-%d interpolation, Q14.\n"
                % (PHASES, taps, lobes))
        f.write(" * Generated deterministically by tests/generate_asrc_fir_tables.py.\n")
        f.write(" * Every row sums exactly to %d to preserve DC gain. */\n" % AMP)
        f.write("static const int16_t %s[%d][%d] = {\n" % (name, PHASES, taps))
        for row in rows:
            f.write("    {%s},\n" % ", ".join(str(v) for v in row))
        f.write("};\n#endif\n")


def row_to_q14_one(p, taps, lobes):
    half = (taps - 2) / 2.0
    raw = [lanczos(t - half - p / PHASES, lobes) for t in range(taps)]
    s = sum(raw)
    raw = [v / s for v in raw]
    q = [int(round(v * AMP)) for v in raw]
    q[-1] += AMP - sum(q)
    return q


def spectral_report(taps, lobes, label):
    """Worst-phase deviation from the ideal fractional delay at key
    frequencies. For a polyphase interpolator that is the meaningful
    quality metric: |H_p(f) - 1| in dB across the 160 phases."""
    dev = 0.0
    rows = []
    for p in range(PHASES):
        rows.append(row_to_q14_one(p, taps, lobes))
    for fk in (10.0, 20.0, 23.0, 24.0):
        w = 2.0 * math.pi * fk / 48.0
        worst_mag = -1e9
        worst_ph = -1e9
        for p in range(PHASES):
            mu = p / PHASES
            re = 0.0
            im = 0.0
            for t in range(taps):
                a = -w * (t - (taps - 2) / 2.0 - mu)
                re += (rows[p][t] / AMP) * math.cos(a)
                im += (rows[p][t] / AMP) * math.sin(a)
            err = math.hypot(re - 1.0, im)
            db = 20.0 * math.log10(err + 1e-12)
            if db > worst_mag:
                worst_mag = db
            pd = math.degrees(math.atan2(im, re)) - math.degrees((-w * mu) % (2.0 * math.pi))
            if abs(pd) > worst_ph:
                worst_ph = abs(pd)
        if worst_mag > dev:
            dev = worst_mag
        print("%-8s f=%4.0f kHz  worst |H-dev| = %7.1f dB   worst phase err = %6.3f deg"
              % (label, fk, worst_mag, worst_ph))


def main():
    rows8 = gen_rows(8, 4)
    rows16 = gen_rows(16, 8)
    q8 = rows_to_q14(rows8)
    q16 = rows_to_q14(rows16)
    for row in q8:
        assert sum(row) == AMP, "FIR8 row sum != %d" % AMP
    for row in q16:
        assert sum(row) == AMP, "FIR16 row sum != %d" % AMP

    deployed79 = [-204, 972, -2702, 10006, 10273, -2737, 986, -210]
    regen79 = q8[79]
    print("FIR8 row 79 regenerated : %s" % regen79)
    print("FIR8 row 79 deployed    : %s" % deployed79)
    print("FIR8 row 79 clean match : %s" % (regen79 == deployed79))
    print("FIR8 row 81 regenerated : %s" % q8[81])

    spectral_report(8, 4, "FIR8  (Lanczos-4)")
    spectral_report(16, 8, "FIR16 (Lanczos-8)")

    emit_header(8, q8, "device/h36_14_fir8_q14.h")
    emit_header(16, q16, "device/h36_14_fir16_q14.h")
    print("headers written: device/h36_14_fir8_q14.h, device/h36_14_fir16_q14.h")


if __name__ == "__main__":
    main()
