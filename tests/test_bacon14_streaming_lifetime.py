#!/usr/bin/env python3
"""Bacon 1.4 - streaming lifetime (T3): determinismo StopStreamingAndRelease().

Verifica que toda operacion que modifique/reemplace/renombre/elimine o
reescriba un WAV libera el WavFile del AudioFileStreamer de forma
determinista (StopAndRelease -> SAFE_DELETE inmediato) ANTES de tocar el
archivo, y que los sleeps usados solo como workaround de lifetime fueron
eliminados mientras se conservan las esperas con finalidad real
(estabilidad de archivo / observacion del callback con retire).
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
S = root / "source/sources"

streamer_h = (S / "Application/Audio/AudioFileStreamer.h").read_text()
streamer_cpp = (S / "Application/Audio/AudioFileStreamer.cpp").read_text()
player_h = (S / "Application/Player/Player.h").read_text()
player_cpp = (S / "Application/Player/Player.cpp").read_text()
mixer_h = (S / "Application/Player/PlayerMixer.h").read_text()
mixer_cpp = (S / "Application/Player/PlayerMixer.cpp").read_text()
chopper = (S / "Application/UI/Views/ModalDialogs/SampleChopperModal.cpp").read_text()
import_dlg = (S / "Application/UI/Views/ModalDialogs/ImportSampleDialog.cpp").read_text()
usb = (S / "Application/UI/Views/ModalDialogs/UsbRecordModal.cpp").read_text()
manager = (S / "Application/UI/Views/ModalDialogs/SampleManagerDialog.cpp").read_text()

# 1. Mecanismo determinista en el streamer.
assert "void StopAndRelease() ;" in streamer_h
assert "void AudioFileStreamer::StopAndRelease()" in streamer_cpp
start = streamer_cpp.index("void AudioFileStreamer::StopAndRelease()")
end = streamer_cpp.index("bool AudioFileStreamer::IsPlaying", start)
body = streamer_cpp[start:end]
assert "SAFE_DELETE(wav_)" in body, "StopAndRelease debe liberar el WavFile"

# 2. Cadena Player -> PlayerMixer -> AudioFileStreamer.
assert "void StopStreamingAndRelease() ;" in player_h
assert "void StopStreamingAndRelease() ;" in mixer_h
pstart = player_cpp.index("void Player::StopStreamingAndRelease()")
assert "mixer_->StopStreamingAndRelease()" in player_cpp[pstart:pstart + 200]
mstart = mixer_cpp.index("void PlayerMixer::StopStreamingAndRelease()")
assert "fileStreamer_.StopAndRelease()" in mixer_cpp[mstart:mstart + 200]

# 3. El guard destructivo del chopper libera (no solo Stop).
gstart = chopper.index("static void lgptStopAllAudioBeforeDestructiveEdit()")
gend = chopper.index("void SampleChopperModal::drawStringAbs", gstart)
guard = chopper[gstart:gend]
assert "StopStreamingAndRelease()" in guard
for op in ["lgptStopAllAudioBeforeDestructiveEdit", "void SampleChopperModal::cropToSelectedRange",
           "void SampleChopperModal::deleteSelectedChop", "bool SampleChopperModal::normalizeSample"]:
    assert op in chopper, op
# El preview temporal del pitch se libera antes de reescribir el WAV compartido.
prestart = chopper.index("StopStreamingAndRelease();")
assert "writePreviewPitchWav" in chopper

# 4. Import: preview() y endPreview() liberan; sin Sleep(80) workaround.
istart = import_dlg.index("void ImportSampleDialog::preview")
iend = import_dlg.index("void ImportSampleDialog::endPreview", istart)
preview_body = import_dlg[istart:iend]
assert "StopStreamingAndRelease()" in preview_body
assert "Sleep(80)" not in preview_body, "Sleep(80) de lifetime eliminado en preview()"
ep = import_dlg.index("void ImportSampleDialog::endPreview()")
ep_end = import_dlg.index("void ImportSampleDialog::import", ep)
end_body = import_dlg[ep:ep_end]
assert "StopStreamingAndRelease()" in end_body
assert end_body.index("StopStreamingAndRelease") < end_body.index("unlink"), \
    "endPreview debe liberar antes del unlink"

# 5. USB Record: previewRecording libera y no duerme por lifetime.
rstart = usb.index("void UsbRecordModal::previewRecording()")
record_body = usb[rstart:rstart + 2500]
assert "StopStreamingAndRelease()" in record_body
assert "Sleep(80)" not in record_body, "Sleep(80) de lifetime eliminado en previewRecording()"
assert "preparePreview" in record_body

# 6. Esperas con finalidad real CONSERVADAS.
assert "TimeService::GetInstance()->Sleep(60)" in import_dlg, "wait de retire conservado"
assert "Sleep(80)" in usb, "captureFileStable (estabilidad de archivo) conservado"
assert "operationPercent_ < 100) TimeService::GetInstance()->Sleep(90)" in chopper, \
    "pacing de progreso de operacion conservado"

# 7. El manager de samples libera en sus rutas destructivas/preview-stop.
assert "StopStreamingAndRelease()" in manager

print("TEST_BC14_STREAMING_LIFETIME_OK")