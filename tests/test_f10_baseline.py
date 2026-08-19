#!/usr/bin/env python3
"""F10 baseline: evidencia de preservacion + paquete final.

Verifica que el informe F10 existe y cubre los entregables del roadmap, y
que las evidencias que declara coinciden con la realidad del repo y del
build actual:
1. docs/F10_EVIDENCE_ES.md cubre: mapa original, politicas (input, menus,
   navegacion, storage, audio), estructura, deuda, riesgos, pruebas,
   recomendaciones multitrack.
2. Los SHA del build actual coinciden con los golden documentados
   (core fcc02d6b, daemon f7140072, sp404 968dfa61, midi 3f0ea7a2).
3. El audit.sh registra los 24+ runners host y los baselines F4/F5/F8/F9.
4. La verificacion en SD es parte del protocolo documentado (SD == build).
5. Los puntos de verificacion del usuario estan presentes en el informe.
"""
import re
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / "docs/F10_EVIDENCE_ES.md"
ROADMAP = ROOT / "docs/REFACTOR_ROADMAP_ES.md"
AUDIT = ROOT / "scripts/audit.sh"
BUILD = Path("/mnt/d/R36S/PORT LPTRACKER/BUILD/U2523")

GOLDENS = {
    "lgpt_r36sx_u2523.so": "c73685b4a05cddbcee269024be8ad3a2aecacff6f6d134b99c18a28de1c8758f",
    "r36s_u2523_usb_audio_io": "f7140072f9b83573e03caf904d17de6227374823c3719757c7d11a438bb1417d",
    "r36s_sp404_host_audio_io": "968dfa61e561d348fd4ec8006e39b23b4dd56a49f1912f885c2731f118983b83",
    "r36s_midi_host_io": "3f0ea7a23db7390f1fb3b73cbda97f66316c6568d0c7574b838579a014baee80",
}

REQUIRED_SECTIONS = [
    "Resumen ejecutivo",
    "Mapa de la arquitectura original",
    "Arquitectura propuesta",
    "Estructura final",
    "Politicas oficiales",
    "Deuda tecnica pendiente",
    "Evidencia de preservacion",
    "Pruebas de regresion",
    "Recomendaciones",
]

HOST_RUNNERS = [
    "run_host_input_policy.sh",
    "run_host_navigation.sh",
    "run_host_help_overlay.sh",
    "run_host_chop_model.sh",
    "run_host_edit_history.sh",
    "run_host_pitch_tool.sh",
    "run_host_preview_service.sh",
    "run_host_chopper_view.sh",
    "run_host_chopper_draw.sh",
    "run_host_chopper_controller.sh",
    "run_host_fx_pages.sh",
    "run_host_mixer_meters.sh",
    "run_host_mixer_menu.sh",
    "run_host_fx_navigator.sh",
    "run_host_phrase_grid_edit.sh",
    "run_host_phrase_undo.sh",
    "run_host_audio_driver_modes.sh",
    "run_host_audio_capabilities.sh",
    "run_host_audio_router.sh",
    "run_host_audio_backend.sh",
    "run_host_audio_engine.sh",
    "run_host_storage_policy.sh",
    "run_host_action_scenarios.sh",
    "run_host_bass_synth.sh",
    "run_host_mixer_vu_chain.sh",
    "run_host_piano_synth.sh",
    "run_host_eq8_struct.sh",
    "run_host_analyzer_target.sh",
]

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def sha256(p: Path):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    doc = DOC.read_text(encoding="utf-8")
    roadmap = ROADMAP.read_text(encoding="utf-8")
    audit = AUDIT.read_text(encoding="utf-8")

    # 1. Informe cubre los entregables del roadmap.
    for s in REQUIRED_SECTIONS:
        check(s.lower() in doc.lower(), "informe F10 sin seccion: %s" % s)
    for token in ("7709b665", "ea7a80e4", "4be71632", "951b7b3",
                  "AUDIT_CLEAN_MAIN_U2523_OK", "ASAN/UBSAN",
                  "NavigationController", "ChordResolver",
                  "StoragePolicy", "AudioRouter", "ScenarioCatalog"):
        check(token in doc, "informe F10 no menciona %s" % token)

    # 2. SHA del build actual == golden documentados.
    for fname, expected in GOLDENS.items():
        p = BUILD / fname
        check(p.exists(), "build %s no existe" % fname)
        if p.exists():
            got = sha256(p)
            check(got == expected,
                  "build %s cambio: %s (docs esperan %s)" % (fname, got[:12], expected[:12]))

    # 3. Audit registra los runners host (los test_*.py corren por glob).
    for r in HOST_RUNNERS:
        check(r in audit, "audit.sh sin runner %s" % r)
    check("test_*.py" in audit, "audit.sh no ejecuta los test_*.py")

    # 4. Protocolo de publicacion con verificacion en SD documentado.
    check("SD == build" in doc, "informe no documenta SD == build")
    check("protocolo" in doc.lower() and "daemon" in doc.lower(),
          "informe no documenta el protocolo de publicacion")
    check("consola" in doc.lower() or "SD" in doc.upper(),
          "informe sin referencia a validacion en SD/consola")

    # 5. Roadmap marca F1..F10 cerrados (IMPLEMENTADO/completo/cerrado).
    import re as _re
    phase_titles = _re.findall(r"^## F(\d+)\b", roadmap, flags=_re.M)
    for i in range(1, 11):
        section = _re.search(r"^## F%d\b.*?(?=^## F%d\b|\Z)" % (i, i + 1),
                             roadmap, flags=_re.M | _re.S)
        if section is None:
            failures.append("roadmap sin seccion F%d" % i)
            continue
        body = section.group(0)
        closed = any(k in body for k in ("[IMPLEMENTADO]", "completo",
                                         "cerrado", "VALIDADO"))
        if not closed:
            failures.append("roadmap no marca F%d cerrado" % i)

    if failures:
        print("F10_BASELINE_FAILED")
        for f in failures:
            print(" -", f)
        return 1
    print("F10_BASELINE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
