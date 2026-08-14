#!/usr/bin/env python3
"""Bacon 1.4 - T5: ningun texto local cruza el limite de 36 columnas.

ModalView::SetWindow() limita el ancho a 36 (ModalView.cpp:35-36).  Todo
DrawString() de los dialogs hereda ese cliente de ventana: la columna de
salida (x + longitud del texto) no puede superar 36.  El chopper es
full-screen (40x30) y no hereda ModalView, por lo que se audita con 40.
Con x variable no hay cota estatica salvo la longitud del literal: un
texto de >36 chars no cabe en ningun x del cliente.
"""
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
dialogs = root / "source/sources/Application/UI/Views/ModalDialogs"
limits = {
    "AudioDriverModal.cpp": 36, "CommandSelectorModal.cpp": 36,
    "ImportSampleDialog.cpp": 36, "InstrumentEqModal.cpp": 36,
    "MessageBox.cpp": 36, "NewProjectDialog.cpp": 36,
    "SampleManagerDialog.cpp": 36, "SelectProjectDialog.cpp": 36,
    "TreeFrogMenuModal.cpp": 36, "TreeFrogTextEditor.cpp": 36,
    "UsbRecordModal.cpp": 36, "SampleChopperModal.cpp": 40,
}
total_draws = 0
literal_checked = 0
for fname, limit in limits.items():
    text = (dialogs / fname).read_text()
    total_draws += len(re.findall(r"DrawString\(", text))
    for ln, line in enumerate(text.splitlines(), 1):
        # x e y literales: x + len <= limit.
        for m in re.finditer(r'DrawString\((\d+),\s*\d+,\s*"((?:[^"\\]|\\.)*)"', line):
            x = int(m.group(1))
            n = len(m.group(2).encode().decode("unicode_escape"))
            assert x + n <= limit, \
                f"{fname}:{ln} desborda {limit} cols (x={x} len={n} total={x+n}): {line.strip()}"
            literal_checked += 1
        # x variable: el literal no puede superar el limite por si solo.
        for m in re.finditer(r'DrawString\([^,]+,\s*\d+,\s*"((?:[^"\\]|\\.)*)"', line):
            n = len(m.group(1).encode().decode("unicode_escape"))
            assert n <= limit, \
                f"{fname}:{ln} literal de {n} chars no cabe en {limit} cols: {line.strip()}"
assert total_draws >= 60, "la auditoria debe cubrir todos los DrawString de los dialogs"
print(f"TEST_BC14_MODAL_36COLS_OK ({literal_checked} literales con x fijo, {total_draws} DrawString en total)")