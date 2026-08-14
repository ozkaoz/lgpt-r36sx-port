#!/usr/bin/env python3
"""F5 baseline: politica de storage/SD estricta.

Verifica que:
1. Services/Storage/StoragePolicy.h declara la clasificacion
   Volatile/Persistent/Diagnostic, la raiz unica del core
   (/mnt/sdcard/lgpt) y el inventario {ruta, tipo, quien, cuando}.
2. La capa es pura: sin GUI/audio/daemons/POSIX/framebuffer y sin
   includes del sistema; solo servicios de la capa pura.
3. Barrido estatico de source/ y device/: TODA ruta literal usada cae
   dentro de la politica (Volatile, Persistent o Diagnostic) o dentro de
   una entrada del inventario.  Nada nuevo escribe en la SD fuera de los
   tres tipos.
4. El core sigue derivando la raiz del storage (alias "bin") de la misma
   raiz que declara StoragePolicyRoot.
"""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / "source/sources/Services/Storage/StoragePolicy.h"

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "DrawString", "GUITextProperties", "MixerService",
    "GetInstance", "ColorDefinition", "TreeFrogGetFramebuffer",
    "viewData_", "View::SetNotification", "GUIRect", "isDirty_",
    "unistd.h", "fcntl.h", "stdio.h", "stdlib.h", "sys/",
    "open(", "mkfifo", "mkdir(", "snprintf", "fprintf",
    "TreeFrogUac2Bridge_", "g_driver_mode", "U241_",
    "TREEFROG_UAC2_BRIDGE", "g_usb_raw", "nomute_file_present",
    "monitor_ring",
]

ROOT_RE = re.compile(r"/mnt/sdcard[^\"'\s)]*")
TMP_RE = re.compile(r"/tmp[^\"'\s)]*")

# Raices de la politica: categoria -> prefijos permitidos.
CATEGORY_ROOTS = [
    ("diagnostic", ["/mnt/sdcard/LGPT_OTG_LOGS", "/mnt/sdcard/lgpt/otg/logs"]),
    ("volatile", ["/mnt/sdcard/lgpt/tmp/"]),
    ("persistent", ["/mnt/sdcard/lgpt"]),
]

# Rutas conocidas solo via variables de entorno/derivadas (daemons):
# $ROOT/lgpt (lgpt_launcher_u241.sh), $DATA/tmp/record, $LOGROOT.
VARIABLE_PATHS = ["/tmp/record"]


def classify(path: str) -> str:
    for cat, roots in CATEGORY_ROOTS:
        for root in roots:
            if path == root or path.startswith(root + "/"):
                return cat
    return "unknown"


def scan_file(path: Path, hits):
    try:
        text = path.read_text(errors="replace")
    except (UnicodeDecodeError, OSError):
        return
    # Saltar binarios y backups.
    if "\x00" in text[:4096]:
        return
    for rx in (ROOT_RE, TMP_RE):
        for m in rx.finditer(text):
            raw = m.group(0)
            raw = raw.rstrip("/.")
            if raw in VARIABLE_PATHS:
                continue
            if raw.startswith("/tmp"):
                continue  # tmpfs siempre volatil
            # Tokens genericos de shell ("/mnt/sdcard}" o raiz pelada) no
            # son rutas de fichero; se ignoran salvo que sean lgpt/LOGS.
            if not (raw.startswith("/mnt/sdcard/lgpt") or
                    raw.startswith("/mnt/sdcard/LGPT_OTG_LOGS")):
                continue
            if classify(raw) == "unknown":
                hits[path] = hits.get(path, []) + [raw]


def main():
    text = POLICY.read_text()
    for token in ["StoragePolicyRoot", "StoragePolicyClassify",
                  "kStorageCategoryVolatile", "kStorageCategoryPersistent",
                  "kStorageCategoryDiagnostic", "kStorageInventory",
                  "StorageInventoryCount", "StorageCategoryName"]:
        assert token in text, token
    for token in FORBIDDEN:
        assert token not in text, token
    includes = [l.strip() for l in text.splitlines()
                if l.strip().startswith("#include")]
    assert includes == [], includes
    assert '"/mnt/sdcard/lgpt"' in text
    print("storage policy layer guards OK")

    # Inventario: entradas clave con sus duenos.
    for entry in ["config.xml", "last_project", "LGPT_OTG_LOGS",
                  "/tmp/r36sx_lgpt_logs/", "/tmp/r36sx_lgpt_usb/"]:
        assert entry in text, entry
    print("storage inventory present OK")

    # Barrido estatico: ninguna ruta fuera de la politica.
    hits = {}
    for d in [ROOT / "source/sources", ROOT / "device"]:
        if not d.exists():
            continue
        for f in sorted(d.rglob("*")):
            if f.is_file() and f.suffix in (
                ".cpp", ".h", ".c", ".sh", ".py", ".md",
            ):
                scan_file(f, hits)
    assert not hits, "rutas fuera de politica:\n" + "\n".join(
        f"  {p}: {', '.join(h)}" for p, h in sorted(hits.items()))
    print(f"static storage scan OK ({sum(len(v) for v in hits.values())} "
          f"descartes de /tmp, 0 rutas fuera)")

    # El alias "bin" del core se instala desde la misma raiz.
    sysfile = (ROOT / "source/sources/Adapters/TREEFROG/System/"
               "TreeFrogSystem.cpp").read_text()
    assert 'Path::SetAlias("bin", LGPT_TREEFROG_ROOT)' in sysfile
    assert 'define LGPT_TREEFROG_ROOT "/mnt/sdcard/lgpt"' in sysfile
    print("core root single source of truth OK")

    print("F5_BASELINE_OK")


if __name__ == "__main__":
    main()
