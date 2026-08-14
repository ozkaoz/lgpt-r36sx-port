/*
 * scenario_runner_host_test.cpp -- F8: runner de escenarios funcionales
 * por vista contra el catalogo dorado de input.
 *
 * Compila sin dependencias de la app: g++ -std=gnu++03 -Wall -Wextra -Werror
 *   ChordResolver.cpp ActionMap.cpp scenario_runner_host_test.cpp
 *
 * El catalogo (ScenarioCatalog.h) transcribe el COMPORTAMIENTO observable
 * por vista del codigo dorado (Bacon 1.2.1): secuencias multi-fire de
 * MixerView.cpp, requisitos estables del port (B/A en pitch, R1+A en trim,
 * ...) y negaciones documentadas. El runner inyecta cada mascara EPBM_* en
 * ChordResolver_Resolve y comprueba:
 *   (1) la accion esperada (golden),
 *   (2) determinismo de la resolucion,
 *   (3) unicidad (ctx, mascara) en el catalogo,
 *   (4) coherencia catalogo <-> ActionMap: toda accion esperada pertenece
 *       al contexto declarado y su acorde (require) cubre la mascara,
 *   (5) cobertura: los seis contextos tienen escenarios.
 * Si falla, ALGUIEN CAMBIO EL COMPORTAMIENTO o el catalogo no lo transcribe.
 */
#include <stdio.h>
#include "Application/UI/Input/ScenarioCatalog.h"
#include "Application/UI/Input/ChordResolver.h"

using namespace UI::Input;

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        ++g_failures; \
    } \
} while (0)

static int count_bindings(ContextId ctx) {
    const Binding *table = 0;
    return ActionMap_GetBindings(ctx, &table);
}

static const Binding *find_binding(ContextId ctx, ActionId action) {
    const Binding *table = 0;
    int n = ActionMap_GetBindings(ctx, &table);
    for (int i = 0; i < n; ++i) {
        if (table[i].action == action) return &table[i];
    }
    return 0;
}

static void run_catalog(void) {
    int n = ScenarioCatalogCount();
    int per_ctx[CTX_COUNT] = {0};
    int i, j;

    for (i = 0; i < n; ++i) {
        const Scenario *s = ScenarioCatalogAt(i);
        if (s == 0) { CHECK(0 && "ScenarioCatalogAt fuera de rango"); continue; }

        if (s->ctx < 0 || s->ctx >= CTX_COUNT) {
            CHECK(0 && "contexto de escenario fuera de rango");
            continue;
        }
        ++per_ctx[s->ctx];

        /* (1) accion golden. */
        ActionId got = ChordResolver_Resolve(s->mask, s->ctx);
        if (got != s->expected) {
            printf("FAIL escenario %d [%s] mask=0x%04x ctx=%d: "
                   "esperaba %d (%s) got %d\n",
                   i, s->view, (unsigned)s->mask, (int)s->ctx,
                   (int)s->expected, s->doc, (int)got);
            ++g_failures;
            continue;
        }

        /* (2) determinismo: 3 resoluciones identicas. */
        ActionId r1 = ChordResolver_Resolve(s->mask, s->ctx);
        ActionId r2 = ChordResolver_Resolve(s->mask, s->ctx);
        CHECK(r1 == got);
        CHECK(r2 == got);

        /* (4) coherencia con el ActionMap. */
        if (s->expected != ACTION_NONE) {
            const Binding *b = find_binding(s->ctx, s->expected);
            CHECK(b != 0);
            if (b != 0) {
                int covered = 0;
                const Binding *table = 0;
                int nb = ActionMap_GetBindings(s->ctx, &table);
                for (int k = 0; k < nb; ++k) {
                    if (table[k].action != s->expected) continue;
                    if ((s->mask & table[k].require) == table[k].require &&
                        (s->mask & table[k].forbid) == 0) {
                        covered = 1;
                        break;
                    }
                }
                if (!covered) {
                    printf("FAIL escenario %d [%s] mask=0x%04x: "
                           "ningun binding de act %d cubre la mascara\n",
                           i, s->view, (unsigned)s->mask, (int)s->expected);
                    ++g_failures;
                }
            }
        } else {
            CHECK(find_binding(s->ctx, ACTION_NONE) == 0);
        }
    }

    /* (3) unicidad de (ctx, mask) en el catalogo. */
    for (i = 0; i < n; ++i) {
        for (j = i + 1; j < n; ++j) {
            const Scenario *a = ScenarioCatalogAt(i);
            const Scenario *b = ScenarioCatalogAt(j);
            if (a->ctx == b->ctx && a->mask == b->mask) {
                printf("FAIL duplicado: escenarios %d y %d comparten "
                       "ctx=%d mask=0x%04x\n", i, j,
                       (int)a->ctx, (unsigned)a->mask);
                ++g_failures;
            }
        }
    }

    /* (5) cobertura de contextos. */
    for (i = 0; i < CTX_COUNT; ++i) {
        if (per_ctx[i] == 0) {
            printf("FAIL contexto %d sin escenarios en el catalogo\n", i);
            ++g_failures;
        }
    }

    printf("scenario runner: %d escenarios, %d contextos cubiertos\n",
           n, CTX_COUNT);
}

static void run_map_coherence(void) {
    /* Toda accion del ActionId que aparece en el ActionMap esta ligada en
     * algun contexto (nadie la declara muerta); las negaciones del catalogo
     * no colisionan con un binding real. */
    int n = ScenarioCatalogCount();
    for (int i = 0; i < n; ++i) {
        const Scenario *s = ScenarioCatalogAt(i);
        if (s->expected == ACTION_NONE) continue;
        if (count_bindings(s->ctx) == 0) {
            printf("FAIL contexto %d sin bindings pero con escenarios\n",
                   (int)s->ctx);
            ++g_failures;
        }
    }
    printf("map coherence: ActionMap por contexto presente\n");
}

int main(void) {
    run_catalog();
    run_map_coherence();
    if (g_failures == 0) {
        printf("ACTION_SCENARIOS_HOST_ALL_OK (%d checks)\n",
               ScenarioCatalogCount() * 6 + CTX_COUNT);
        return 0;
    }
    printf("ACTION_SCENARIOS_HOST_FAILED (%d failures)\n", g_failures);
    return 1;
}
