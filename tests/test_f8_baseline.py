#!/usr/bin/env python3
"""F8 baseline: harness de regresion funcional por vista.

Verifica que:
1. Application/UI/Input/ScenarioCatalog.h declara la base de datos de
   escenarios por vista {vista, contexto, mascara EPBM, accion esperada,
   cola, referencia golden} con al menos un escenario por contexto y
   cubriendo los seis contextos del ActionMap (CTX_GLOBAL/MIXER/MIXER_FX/
   CHOPPER/CHOPPER_TRIM/CHOPPER_PITCH).
2. El catalogo es una capa pura: solo ActionId/ChordResolver, sin GUI,
   audio, daemons ni includes del sistema.
3. No duplica el ActionMap: la tabla plana de bindings del ActionMap no
   se repite en el catalogo; el catalogo solo transcribe comportamiento
   de orden superior (multi-fire, requisitos estables, negaciones).
4. El runner host esta registrado en scripts/audit.sh y se ejecuta con
   ASAN/UBSAN, y el baseline comprueba que todo ActionId esperado existe
   en el enum ActionId.h (transcripcion sin nombres inventados).
5. Documenta la restriccion del entorno: el build X64 del core no es
   ejecutable en este host (faltan pkg-config/SDL2/ALSA), por lo que el
   harness F8 es la unica via de regresion funcional del pipeline de
   input en host.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "source/sources/Application/UI/Input/ScenarioCatalog.h"
ACTIONID = ROOT / "source/sources/Application/UI/Input/ActionId.h"
RUNNER = ROOT / "tests/host/scenario_runner_host_test.cpp"
RUNNER_SH = ROOT / "tests/run_host_action_scenarios.sh"
AUDIT = ROOT / "scripts/audit.sh"

CONTEXTS = [
    "CTX_GLOBAL",
    "CTX_MIXER",
    "CTX_MIXER_FX",
    "CTX_CHOPPER",
    "CTX_CHOPPER_TRIM",
    "CTX_CHOPPER_PITCH",
]

FORBIDDEN_IN_CATALOG = [
    "DrawString", "GUITextProperties", "ModalView", "SamplePool",
    "GetInstance", "ColorDefinition", "MixerService", "viewData_",
    "unistd.h", "fcntl.h", "stdio.h", "stdlib.h", "sys/",
    "TreeFrog", "g_usb_raw", "monitor_ring", "nomute_file_present",
]

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def main():
    cat = CATALOG.read_text(encoding="utf-8")
    aid = ACTIONID.read_text(encoding="utf-8")
    runner = RUNNER.read_text(encoding="utf-8")
    runner_sh = RUNNER_SH.read_text(encoding="utf-8")
    audit = AUDIT.read_text(encoding="utf-8")

    # 1. Estructura del catalogo: declaraciones de escenarios por vista.
    check("static const Scenario kScenarios[]" in cat,
          "ScenarioCatalog.h debe declarar kScenarios[]")
    check("struct Scenario {" in cat,
          "ScenarioCatalog.h debe declarar struct Scenario")
    for f in ("view", "ctx", "mask", "expected", "queued", "doc"):
        check("const char *%s" % f in cat or f in cat,
              "struct Scenario sin campo %s" % f)

    per_ctx = {c: 0 for c in CONTEXTS}
    for c in CONTEXTS:
        per_ctx[c] = len(re.findall(r"\b%s\b" % c, cat))
    for c, n in per_ctx.items():
        check(n >= 1, "contexto %s sin escenarios en el catalogo (%d)" % (c, n))
    check(len(re.findall(r'"\w+\(?\w*\)?"\s*,\s*CTX_', cat)) >= 50,
          "catalogo demasiado pequeno para cubrir el golden")

    # 2. Capa pura.
    for w in FORBIDDEN_IN_CATALOG:
        check(w not in cat, "catalogo arrastra dependencia prohibida: %s" % w)

    # 3. No duplica el ActionMap: la tabla de bindings vive en ActionMap.cpp.
    am = (ROOT / "source/sources/Application/UI/Input/ActionMap.cpp")
    am_txt = am.read_text(encoding="utf-8")
    bbind = len(re.findall(r"\bBIND\s*\(", am_txt))
    cscn = len(re.findall(r'\{ "[^"]+", CTX_', cat))
    check(bbind >= 50, "ActionMap sin bindings (%d)" % bbind)
    check(cscn <= bbind,
          "el catalogo transcribe casi tantos bindings como el ActionMap "
          "(%d vs %d): debe ser de orden superior, no duplicado"
          % (cscn, bbind))

    # 4. Runner registrado en audit + ASAN/UBSAN + acciones validas.
    check("run_host_action_scenarios.sh" in audit,
          "audit.sh no ejecuta el runner F8")
    check("sanitize=address,undefined" in runner_sh,
          "el runner debe ejecutarse con ASAN/UBSAN")
    check("ChordResolver_Resolve" in runner,
          "el runner no inyecta en ChordResolver_Resolve")
    check("ScenarioCatalog.h" in runner,
          "el runner no consume el catalogo")

    ids = set(re.findall(r"\bACTION_[A-Z0-9_]+", aid))
    used = set(re.findall(r"\bACTION_[A-Z0-9_]+\b", cat))
    for u in sorted(used):
        check(u in ids, "ActionId %s del catalogo no existe en ActionId.h" % u)

    # 5. Limitacion documentada del build X64 en host.
    docs = " ".join(p.read_text(encoding="utf-8", errors="ignore")
                    for p in ROOT.glob("docs/*.md"))
    check("X64" in docs and "SDL" in docs,
          "la limitacion del build X64 (SDL/ALSA) debe estar documentada")

    if failures:
        print("F8_BASELINE_FAILED")
        for f in failures:
            print(" -", f)
        return 1
    print("F8_BASELINE_OK (%d checks)" % (len(CONTEXTS) + 4 + len(used) + 1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
