#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
F1 input policy (REFACTOR_ROADMAP_ES.md, fase 1): el ActionMap.cpp es la
UNICA fuente de verdad de los bindings. Este test verifica que el catalogo
declara los acordes flagship del golden Bacon 1.2.1 (los que derivan de
requisitos estables) y que no ha perdido ninguno de los contextos.

Los bindings se escriben como filas BIND(action, require|KEY_, forbid, prov)
en ActionMap.cpp; aqui se comprueban solo los pares (tecla, accion).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
AM = (ROOT / "source/sources/Application/UI/Input/ActionMap.cpp").read_text()

# (action, require-bits) que DEBEN existir en el catalogo.
KEY_MAP = {
    "KEY_LEFT": "KEY_LEFT", "KEY_RIGHT": "KEY_RIGHT", "KEY_UP": "KEY_UP",
    "KEY_DOWN": "KEY_DOWN", "KEY_L1": "KEY_L1", "KEY_R1": "KEY_R1",
    "KEY_L2": "KEY_L2", "KEY_R2": "KEY_R2", "KEY_A": "KEY_A",
    "KEY_B": "KEY_B", "KEY_X": "KEY_X", "KEY_Y": "KEY_Y",
    "KEY_SELECT": "KEY_SELECT", "KEY_START": "KEY_START",
}

FLAGSHIP = [
    # B -> Preview / A -> Apply en Pitch (requisito explicito).
    ("ACTION_PITCH_PREVIEW", ["KEY_B"]),
    ("ACTION_PITCH_APPLY", ["KEY_A"]),
    # L1+X undo / R1+X redo (global, chopper main/trim/pitch).
    ("ACTION_UNDO", ["KEY_L1", "KEY_X"]),
    ("ACTION_REDO", ["KEY_R1", "KEY_X"]),
    # L1+A -> menu en Mixer.
    ("ACTION_OPEN_MENU", ["KEY_L1", "KEY_A"]),
    # SELECT+R1 help / SELECT+R2 audio driver (global).
    ("ACTION_OPEN_HELP", ["KEY_SELECT", "KEY_R1"]),
    ("ACTION_OPEN_AUDIO_DRIVER", ["KEY_SELECT", "KEY_R2"]),
    # Chopper main: B preview del chop, A anade chop, Y borra, R1+B cierra.
    ("ACTION_PLAY_CHOP_PREVIEW", ["KEY_B"]),
    ("ACTION_ADD_CHOP", ["KEY_A"]),
    ("ACTION_DELETE_CHOP", ["KEY_Y"]),
    ("ACTION_CLOSE", ["KEY_R1", "KEY_B"]),
    # Trim: R1+A crop, L2+Y delete range, R2+Y normalize,
    # L1+A/L1+B snap a zero-cross.
    ("ACTION_CROP", ["KEY_R1", "KEY_A"]),
    ("ACTION_DELETE_RANGE", ["KEY_L2", "KEY_Y"]),
    ("ACTION_NORMALIZE", ["KEY_R2", "KEY_Y"]),
    ("ACTION_SNAP_BOUNDARY_START", ["KEY_L1", "KEY_A"]),
    ("ACTION_SNAP_BOUNDARY_END", ["KEY_L1", "KEY_B"]),
    # L1+R1 alterna pitch mode.
    ("ACTION_TOGGLE_PITCH_MODE", ["KEY_L1", "KEY_R1"]),
]

def binding_lines():
    lines = [l.strip() for l in AM.splitlines()]
    return lines

def test_contexts_present():
    for ctx in ("CTX_GLOBAL", "CTX_MIXER", "CTX_MIXER_FX", "CTX_CHOPPER",
                "CTX_CHOPPER_TRIM", "CTX_CHOPPER_PITCH"):
        assert ctx in AM, "ctx " + ctx
    CC = (ROOT / "source/sources/Application/UI/Input/ChordResolver.h").read_text()
    assert "CTX_COUNT" in CC
    print("ActionMap: 6 contextos declarados OK")

def test_flagship_bindings():
    text = AM
    missing = []
    for action, keys in FLAGSHIP:
        # busca la primera fila BIND(action, y comprueba las teclas en el
        # segmento requerido (hasta el primer "forbid").
        marker = "BIND(" + action + ","
        idx = text.find(marker)
        found = False
        while idx != -1:
            seg = text[idx:idx + 400]
            end = seg.find("forbid")
            req = seg[:end] if end != -1 else seg
            if all(k in req for k in keys):
                found = True
                break
            idx = text.find(marker, idx + 1)
        if not found:
            missing.append((action, keys))
    assert not missing, "bindings flagship faltantes: %r" % missing
    print("ActionMap: %d bindings flagship verificados OK" % len(FLAGSHIP))

def test_no_magic_pad_bits():
    # El catalogo usa solo los nombres simbolicos KEY_*; no debe haber
    # literales hex (0x...) en el fichero, ni siquiera en comentarios.
    import re
    assert not re.search(r"0x[0-9a-fA-F]+", AM), "bit numerico magico"
    print("ActionMap: sin bits magicos OK")

test_contexts_present()
test_flagship_bindings()
test_no_magic_pad_bits()
print("INPUT_POLICY_CATALOG_OK")