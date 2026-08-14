// F5: StoragePolicy - politica de storage/SD estricta (capa pura).
//
// Declara, para todo el port R36SX (core + daemons), la clasificacion de
// cada ruta de fichero en uno de los tres tipos permitidos:
//
//   Volatile    cache/tmpfs: nunca llega a la SD.
//               /tmp/r36sx_lgpt_logs, /tmp/r36sx_lgpt_usb,
//               /tmp/r36sx_lgpt_record, fifos y locks de la union USB.
//   Persistent  configuracion y datos de usuario sobre la raiz del core.
//               <root>/config.xml, <root>/last_project, projects/, samples/,
//               instruments/, otg/ (flags, perfil y scripts del driver).
//   Diagnostic  logs de diagnostico, fuera de la raiz de datos.
//               /mnt/sdcard/LGPT_OTG_LOGS y <root>/otg/logs.
//
// La raiz del core es /mnt/sdcard/lgpt (la misma que TreeFrogSystem
// instala como alias "bin").  Las rutas se derivan de StoragePolicyRoot()
// para que core y daemons compartan una unica fuente de verdad, y el
// inventario {ruta, tipo, quien, cuando} documenta cada acceso actual.
//
// Regla estricta: nada nuevo escribe en la SD fuera de los tres tipos;
// el baseline estatico (tests/test_f5_baseline.py) la audita sobre
// source/ y device/.

#ifndef _LGPT_STORAGE_POLICY_H_
#define _LGPT_STORAGE_POLICY_H_

namespace StoragePolicy {

enum StorageCategory {
    kStorageCategoryVolatile = 0,
    kStorageCategoryPersistent = 1,
    kStorageCategoryDiagnostic = 2
};

// Raiz unica del core en la SD (misma que LGPT_TREEFROG_ROOT de
// TreeFrogSystem.cpp, instalada como alias "bin").
static const char *StoragePolicyRoot() { return "/mnt/sdcard/lgpt"; }

// Nombres canonicos de las tres categorias.
static const char *StorageCategoryName(StorageCategory c) {
    switch (c) {
        case kStorageCategoryVolatile:   return "volatile";
        case kStorageCategoryPersistent: return "persistent";
        case kStorageCategoryDiagnostic: return "diagnostic";
    }
    return "unknown";
}

// Comparacion de prefijo sin <string.h> (capa pura).
static bool StorageStartsWith(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s != *prefix) return false;
        ++s;
        ++prefix;
    }
    return true;
}

static bool StorageEndsWith(const char *s, const char *suffix) {
    const char *a = s;
    while (*a) ++a;
    const char *b = suffix;
    while (*b) ++b;
    while (a > s && b > suffix) {
        --a; --b;
        if (*a != *b) return false;
    }
    return b == suffix;
}

static bool StorageStartsWithAny(const char *s, const char *const *prefixes, int n) {
    for (int i = 0; i < n; ++i) {
        if (StorageStartsWith(s, prefixes[i])) return true;
    }
    return false;
}

// Clasifica una ruta de fichero segun la politica estricta.
// Devuelve la categoria, o -1 si la ruta esta fuera de los tres tipos
// (escritura nueva no permitida).
static int StoragePolicyClassify(const char *path) {
    if (!path || !path[0]) return -1;

    // Diagnostic: fuera de la raiz de datos (frontera de segmento).
    if (StorageStartsWith(path, "/mnt/sdcard/LGPT_OTG_LOGS")) {
        const char *tail = path + 25;  // len("/mnt/sdcard/LGPT_OTG_LOGS")
        if (tail[0] == '/' || tail[0] == '\0') {
            return kStorageCategoryDiagnostic;
        }
    }
    // Diagnostic: logs del driver bajo otg/logs.
    if (StorageEndsWith(path, "/otg/logs")) return kStorageCategoryDiagnostic;
    if (StorageStartsWith(path, "/mnt/sdcard/lgpt/otg/logs/")) {
        return kStorageCategoryDiagnostic;
    }

    // Volatile: tmpfs y cache; nunca SD.
    {
        static const char *const volatilePrefixes[] = {
            "/mnt/sdcard/lgpt/tmp/", // bind-mount tmpfs del launcher
            "/tmp/r36sx_lgpt_logs",
            "/tmp/r36sx_lgpt_usb",
            "/tmp/r36sx_lgpt_record",
            "/tmp/r36sx_",       // fifos y locks de la union USB
            "/tmp/joy_key",      // ftok de la shm Cubevol
            "/tmp/h38_2_",       // logs del modulo
            "/tmp/u2414_",       // modprobe/insmod/bind err
            "/tmp/u2517_",       // logs del daemon u2517
            "/tmp/u241_setup_from_lgpt.log",
        };
        if (StorageStartsWithAny(path, volatilePrefixes,
                                 sizeof(volatilePrefixes) /
                                 sizeof(volatilePrefixes[0]))) {
            return kStorageCategoryVolatile;
        }
    }

    // Persistent: sobre la raiz del core (frontera de segmento: / o fin).
    if (StorageStartsWith(path, "/mnt/sdcard/lgpt")) {
        const char *tail = path + 16;  // len("/mnt/sdcard/lgpt")
        if (tail[0] == '/' || tail[0] == '\0') {
            return kStorageCategoryPersistent;
        }
    }

    return -1;
}

// Inventario {ruta, tipo, quien, cuando} de los accesos actuales.
// Tabla unica de verdad: el baseline estatico verifica que cada ruta
// real de source/ y device/ coincide con una entrada o su prefijo.
struct StorageInventoryEntry {
    const char *path;
    StorageCategory category;
    const char *owner;   // modulo o daemon que accede
    const char *when;    // momento del acceso
};

static const StorageInventoryEntry kStorageInventory[] = {
    // --- Persistent: configuracion y datos de usuario ---
    { "/mnt/sdcard/lgpt/config.xml", kStorageCategoryPersistent,
      "Config (Application/Model)", "boot y guardado de config" },
    { "/mnt/sdcard/lgpt/last_project", kStorageCategoryPersistent,
      "AppWindow (LAST_PROJECT_NAME)", "cambio de proyecto" },
    { "/mnt/sdcard/lgpt/projects/", kStorageCategoryPersistent,
      "Persistency/AppWindow (bin:projects)", "guardar/cargar proyecto" },
    { "/mnt/sdcard/lgpt/samples/", kStorageCategoryPersistent,
      "UsbRecordModal (bin:samples)", "grabaciones y samples" },
    { "/mnt/sdcard/lgpt/samples/records", kStorageCategoryPersistent,
      "UsbRecordModal (kRecordDirectory)", "grabacion USB en curso" },
    { "/mnt/sdcard/lgpt/instruments/", kStorageCategoryPersistent,
      "TreeFrogSystem::Boot (bin:instruments)", "arranque del core" },
    { "/mnt/sdcard/lgpt/otg/", kStorageCategoryPersistent,
      "TreeFrogUac2Bridge (flags y perfil)", "estado persistente OTG" },
    { "/mnt/sdcard/lgpt/otg/bin/", kStorageCategoryPersistent,
      "TreeFrogUac2Bridge (scripts del driver)", "aplicar perfil/modo" },

    // --- Diagnostic: logs fuera de la raiz de datos ---
    { "/mnt/sdcard/LGPT_OTG_LOGS/", kStorageCategoryDiagnostic,
      "daemons OTG + flush de apagado", "apagado y fallos" },
    { "/mnt/sdcard/lgpt/otg/logs/", kStorageCategoryDiagnostic,
      "TreeFrogUac2Bridge (u241_setup_from_lgpt.log)", "setup del driver" },

    // --- Volatile: tmpfs y cache ---
    { "/tmp/r36sx_lgpt_logs/", kStorageCategoryVolatile,
      "TreeFrogSystem/FileLogger y TreeFrogLibretro", "logs runtime" },
    { "/tmp/r36sx_lgpt_usb/", kStorageCategoryVolatile,
      "TreeFrogUac2Bridge <-> daemons (ABI)", "estado de la union USB" },
    { "/tmp/r36sx_lgpt_record/", kStorageCategoryVolatile,
      "UsbRecordModal", "cache de grabacion" },
    { "/mnt/sdcard/lgpt/tmp/record/", kStorageCategoryVolatile,
      "lgpt_launcher_u241.sh (bind-mount tmpfs en DATA/tmp/record)",
      "volcado de previews USB" },
    { "/tmp/r36sx_uac2_bridge_fifo", kStorageCategoryVolatile,
      "TreeFrogUac2Bridge <-> daemon", "stream FIFO" },
    { "/tmp/r36sx_sp404_pcm_fifo", kStorageCategoryVolatile,
      "driver SP404 <-> daemon", "stream FIFO" },
    { "/tmp/r36sx_midi_pcm_fifo", kStorageCategoryVolatile,
      "driver MIDI <-> daemon", "stream FIFO" },
    { "/tmp/r36sx_aoa_bulk_pcm_fifo", kStorageCategoryVolatile,
      "driver AOA <-> daemon", "stream FIFO" },
    { "/tmp/r36sx_usb_capture_monitor_fifo", kStorageCategoryVolatile,
      "TreeFrogUac2Bridge <-> daemon", "monitor de captura" },
    { "/tmp/joy_key", kStorageCategoryVolatile,
      "TreeFrogLibretro (Cubevol shm ftok)", "entrada compartida" },
};

static int StorageInventoryCount() {
    return (int)(sizeof(kStorageInventory) / sizeof(kStorageInventory[0]));
}

static const StorageInventoryEntry *StorageInventoryAt(int i) {
    if (i < 0 || i >= StorageInventoryCount()) return 0;
    return &kStorageInventory[i];
}

}  // namespace StoragePolicy

#endif  // _LGPT_STORAGE_POLICY_H_
