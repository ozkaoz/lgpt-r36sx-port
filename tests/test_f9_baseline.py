#!/usr/bin/env python3
"""F9 baseline: riesgos y limites del camino critico de audio.

Verifica coherencia docs <-> codigo: cada limite documentado en
docs/F9_RISKS_ES.md debe existir con el valor exacto en los daemons o en
el bridge del core, y ninguna escritura runtime del camino critico cae
bajo /mnt/sdcard (politica F5: tmpfs para todo estado runtime).
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / "docs/F9_RISKS_ES.md"
U2523 = ROOT / "device/r36s_u2523_usb_audio_io.c"
SP404 = ROOT / "device/r36s_sp404_host_audio_io.c"
MIDI = ROOT / "device/r36s_midi_host_io.c"
BRIDGE = ROOT / "source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp"
LIBRETRO = ROOT / "source/sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp"

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def must_define(text, macro, value, where):
    m = re.search(r"#define\s+%s\s+(\S+)" % macro, text)
    if not m:
        failures.append("%s no definida en %s" % (macro, where.name))
        return
    if m.group(1) != value:
        failures.append("%s=%s en %s, docs esperan %s"
                        % (macro, m.group(1), where.name, value))


def main():
    doc = DOC.read_text(encoding="utf-8")
    u2523 = U2523.read_text(encoding="utf-8")
    sp404 = SP404.read_text(encoding="utf-8")
    midi = MIDI.read_text(encoding="utf-8")
    bridge = BRIDGE.read_text(encoding="utf-8")
    libretro = LIBRETRO.read_text(encoding="utf-8")

    # 1. Limites con valores exactos.
    must_define(u2523, "ASRC_MAX_CORRECTION_PPM", "1200", U2523)
    must_define(u2523, "ASRC_INTEGRAL_LIMIT_PPM", "1000", U2523)
    must_define(u2523, "ASRC_TARGET_BACKLOG_FRAMES", "2400U", U2523)
    must_define(u2523, "ASRC_PRIME_BACKLOG_FRAMES", "2400U", U2523)
    must_define(sp404, "ASRC_MAX_CORRECTION_PPM", "1200", SP404)
    must_define(sp404, "ASRC_INTEGRAL_LIMIT_PPM", "30000", SP404)
    must_define(sp404, "ASRC_HOLD_FLOOR_FRAMES", "2400U", SP404)

    # 2. Re-enumeracion SP404: 8 intentos, ventana 30 s.
    check("g_reenum_count < 8" in sp404, "tope de reenum <8 ausente")
    check("g_reenum_count >= 8" in sp404, "condicion de agotamiento ausente")
    check(">= 30000" in sp404, "ventana de 30 s ausente")
    check("sp404-stall-exhausted" in sp404, "marcador stall-exhausted ausente")
    check("return 3" in sp404, "exit code 3 del reenum agotado ausente")

    # 3. H39/H40 del core-side.
    check("H40_FIFO_PENDING_CAP_SAMPLES = 16384" in bridge
          or "H40_FIFO_PENDING_CAP_SAMPLES=16384" in bridge.replace(" ", ""),
          "staging FIFO 16384 ausente en bridge")
    check("frames > 48000" in libretro and "frames = 48000" in libretro,
          "cap de 1 s (48000) del budget retro ausente")

    # 4. Todo runtime en /tmp: ninguna escritura PERIODICA bajo /mnt/sdcard.
    #    Excepciones de la politica F5: diagnostic (/otg/logs) y config
    #    persistente escrita por evento (sentinel lowlat, modo driver).
    SD_PERSISTENT_EVENT = [
        "/mnt/sdcard/lgpt/otg/lowlat_240",
        "/mnt/sdcard/lgpt/otg/audio_driver_mode",
    ]
    runtime_files = [U2523, SP404, MIDI]
    for f in runtime_files:
        text = f.read_text(encoding="utf-8")
        for m in re.finditer(r'"(/mnt/sdcard[^"\']*)"', text):
            p = m.group(1)
            if p in SD_PERSISTENT_EVENT:
                continue
            if p.startswith("/mnt/sdcard/lgpt/otg/logs"):
                continue
            if p == "/mnt/sdcard/lgpt/otg":
                continue
            failures.append("%s: ruta runtime bajo /mnt/sdcard: %s"
                            % (f.name, p))
    check('"/tmp/r36sx_uac2_bridge_fifo"' in u2523,
          "fifo u2523 no es /tmp")
    check('"/tmp/r36sx_sp404_pcm_fifo"' in sp404,
          "fifo sp404 no es /tmp")
    check('"/tmp/r36sx_midi_pcm_fifo"' in midi,
          "fifo midi no es /tmp")
    check("r36sx_lgpt_record" in u2523, "staging USB-REC ausente")

    # 5. Coherencia docs <-> codigo (los limites clave citados en docs).
    for token in ("ASRC_MAX_CORRECTION_PPM", "1200", "2400", "8 intentos",
                  "30000", "kAoaPcmFifo", "H39", "H40"):
        check(token in doc, "docs/F9 no documenta %s" % token)

    if failures:
        print("F9_BASELINE_FAILED")
        for f in failures:
            print(" -", f)
        return 1
    print("F9_BASELINE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())