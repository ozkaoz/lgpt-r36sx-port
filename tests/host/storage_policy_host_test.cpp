// F5: StoragePolicy - politica de storage/SD estricta (capa pura).
// Oraculos: clasificacion de cada ruta real de source/ y device/ en
// Volatile / Persistent / Diagnostic; regla "nada nuevo escribe fuera".
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Services/Storage/StoragePolicy.h"

static int g_checks = 0;

static void expect_kind(const char *path, int want, const char *what) {
    ++g_checks;
    const int got = StoragePolicy::StoragePolicyClassify(path);
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: '%s' got %s want %d\n", what, path,
                     got < 0 ? "-1" :
                     StoragePolicy::StorageCategoryName(
                         (StoragePolicy::StorageCategory)got),
                     want);
        std::exit(1);
    }
}

int main() {
    const int V = StoragePolicy::kStorageCategoryVolatile;
    const int P = StoragePolicy::kStorageCategoryPersistent;
    const int D = StoragePolicy::kStorageCategoryDiagnostic;

    // --- Persistent: datos de usuario sobre la raiz del core ---
    expect_kind("/mnt/sdcard/lgpt/config.xml", P, "config.xml");
    expect_kind("/mnt/sdcard/lgpt/last_project", P, "last_project");
    expect_kind("/mnt/sdcard/lgpt/projects/foo/lgptsav.dat", P, "projects");
    expect_kind("/mnt/sdcard/lgpt/samples/records", P, "samples/records");
    expect_kind("/mnt/sdcard/lgpt/instruments", P, "instruments");
    expect_kind("/mnt/sdcard/lgpt/otg/audio_driver_mode", P, "otg flag");
    expect_kind("/mnt/sdcard/lgpt/otg/bin/otg_u241_apply_profile_once.sh",
                P, "otg script");
    expect_kind("/mnt/sdcard/lgpt/otg/audio_usb_profile", P, "otg profile");

    // --- Diagnostic: logs fuera de la raiz de datos ---
    expect_kind("/mnt/sdcard/LGPT_OTG_LOGS/H38_HOST_MODULE_LOAD.err", D,
                "OTG_LOGS");
    expect_kind("/mnt/sdcard/lgpt/otg/logs/u241_setup_from_lgpt.log", D,
                "otg/logs");
    expect_kind("/mnt/sdcard/lgpt/otg/logs", D, "otg/logs dir");

    // --- Volatile: tmpfs y cache ---
    expect_kind("/tmp/r36sx_lgpt_logs/lgpt.log", V, "lgpt.log");
    expect_kind("/tmp/r36sx_lgpt_logs/boot_debug.log", V, "boot_debug.log");
    expect_kind("/tmp/r36sx_lgpt_logs/uac2_bridge_lgpt.log", V, "bridge log");
    expect_kind("/tmp/r36sx_lgpt_usb/audio_driver_mode", V, "usb state");
    expect_kind("/tmp/r36sx_lgpt_record/take_1_2_3.wav", V, "record cache");
    expect_kind("/tmp/r36sx_uac2_bridge_fifo", V, "uac2 fifo");
    expect_kind("/tmp/r36sx_sp404_pcm_fifo", V, "sp404 fifo");
    expect_kind("/tmp/r36sx_midi_pcm_fifo", V, "midi fifo");
    expect_kind("/tmp/r36sx_aoa_bulk_pcm_fifo", V, "aoa fifo");
    expect_kind("/tmp/r36sx_usb_capture_monitor_fifo", V, "capture fifo");
    expect_kind("/tmp/r36sx_h35_audio_mode.lock", V, "lock");
    expect_kind("/tmp/joy_key", V, "joy_key ftok");
    expect_kind("/tmp/u2414_insmod.err", V, "insmod err");
    expect_kind("/tmp/u2517_usb_audio_daemon.log", V, "u2517 daemon log");
    expect_kind("/tmp/u241_setup_from_lgpt.log", V, "setup fallback log");
    expect_kind("/mnt/sdcard/lgpt/tmp/record/preview.wav", V,
                "launcher bind-mount");

    // --- Fuera de los tres tipos: no permitido ---
    expect_kind("/mnt/sdcard/random_new_dir", -1, "new sdcard root");
    expect_kind("/mnt/sdcard/lgpt2/x", -1, "raiz nueva");
    expect_kind("/mnt/sdcard/LGPT_OTG_LOGS_OLD/x", -1, "nombre nuevo");
    expect_kind("/home/user/foo", -1, "fuera de la SD");
    expect_kind(0, -1, "null");
    expect_kind("", -1, "vacia");

    // --- Nombres canonicos ---
    ++g_checks;
    if (!StoragePolicy::StorageCategoryName(
            StoragePolicy::kStorageCategoryVolatile) ||
        !StoragePolicy::StorageCategoryName(
            StoragePolicy::kStorageCategoryPersistent) ||
        !StoragePolicy::StorageCategoryName(
            StoragePolicy::kStorageCategoryDiagnostic)) {
        std::fprintf(stderr, "FAIL names\n");
        std::exit(1);
    }

    // --- Raiz unica ---
    ++g_checks;
    if (std::strcmp(StoragePolicy::StoragePolicyRoot(),
                    "/mnt/sdcard/lgpt") != 0) {
        std::fprintf(stderr, "FAIL root\n");
        std::exit(1);
    }

    // --- Inventario completo: cada entrada clasifica en su categoria ---
    for (int i = 0; i < StoragePolicy::StorageInventoryCount(); ++i) {
        const StoragePolicy::StorageInventoryEntry *e =
            StoragePolicy::StorageInventoryAt(i);
        ++g_checks;
        if (!e) { std::fprintf(stderr, "FAIL entry %d\n", i); std::exit(1); }
        const int got = StoragePolicy::StoragePolicyClassify(e->path);
        if (got != (int)e->category) {
            std::fprintf(stderr, "FAIL inventory[%d] '%s': got %d want %d\n",
                         i, e->path, got, (int)e->category);
            std::exit(1);
        }
    }

    std::printf("STORAGE_POLICY_HOST_ALL_OK (%d checks)\n", g_checks);
    return 0;
}
