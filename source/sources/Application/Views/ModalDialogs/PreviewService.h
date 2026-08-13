#ifndef _PREVIEW_SERVICE_H_
#define _PREVIEW_SERVICE_H_

/* F3-3a (docs/F3_ARCHITECTURE_ES.md): rango de playback del Chopper
 * extraido del SampleChopperModal como capa pura (header-only, sin
 * dependencias de la vista).
 *
 * Regla de oro: replicas EXACTAS de bacon 1.2.1:
 * - setPreviewPlaybackRange: active=true; start=clamp(start,0,size-1);
 *   end=clamp(end,start,size-1) (el orden importa: start primero).
 * - clearPreviewPlaybackRange: active=false; start=end=0.
 * - previewTrimStart: previewEnd = start + (rate>0 ? rate*5 : 220500),
 *   recortado al fin del chop ("Preview start").
 * - previewTrimEnd: previewStart = end - (rate>0 ? rate*1 : 44100),
 *   recortado al inicio del chop ("Preview end").
 * - playFromFrame / playFrameRange: clamps a [0, size-1] (o 0/0) y
 *   end>=start despues de clamp.
 * El audio (Player::Stop/StartStreaming) y el overlay quedan en el dueno.
 */

class PreviewService {
public:
    struct Range {
        int start;
        int end;
    };

    PreviewService() : active_(false), startFrame_(0), endFrame_(0) {}

    bool Active() const { return active_; }
    int StartFrame() const { return startFrame_; }
    int EndFrame() const { return endFrame_; }

    /* Golden clearPreviewPlaybackRange. */
    void ClearRange() {
        active_ = false;
        startFrame_ = 0;
        endFrame_ = 0;
    }

    /* Golden preview del pitch (SampleChopperModal 2472): solo desactiva
       el rango sin borrar start/end (el overlay los publica como esten). */
    void Deactivate() { active_ = false; }

    /* Golden setPreviewPlaybackRange (clamps en orden: start, luego end
       con el start ya clampeado como minimo). */
    void SetRange(int startFrame, int endFrame, int sourceSize) {
        active_ = true;
        startFrame_ = ClampInt(startFrame, 0,
                               sourceSize > 0 ? sourceSize - 1 : 0);
        endFrame_ = ClampInt(endFrame, startFrame_,
                             sourceSize > 0 ? sourceSize - 1 : startFrame_);
    }

    /* Golden previewTrimStart: ventana de 5s (o 220500 si rate<=0). */
    static Range TrimStart(int chopStart, int chopEnd, int sourceRate) {
        Range r;
        r.start = chopStart;
        r.end = chopStart + (sourceRate > 0 ? sourceRate * 5 : 220500);
        if (r.end > chopEnd) r.end = chopEnd;
        return r;
    }

    /* Golden previewTrimEnd: ventana de 1s (o 44100 si rate<=0). */
    static Range TrimEnd(int chopStart, int chopEnd, int sourceRate) {
        Range r;
        r.start = chopEnd - (sourceRate > 0 ? sourceRate * 1 : 44100);
        r.end = chopEnd;
        if (r.start < chopStart) r.start = chopStart;
        return r;
    }

    /* Golden playFromFrame. */
    static int ClampPlayFrame(int frame, int sourceSize) {
        if (sourceSize > 0) return ClampInt(frame, 0, sourceSize - 1);
        return 0;
    }

    /* Golden playFrameRange: clamps ambos a [0, size-1] (0/0 si size<=0)
       y luego end>=start. */
    static Range ClampPlayRange(int startFrame, int endFrame, int sourceSize) {
        Range r;
        if (sourceSize > 0) {
            r.start = ClampInt(startFrame, 0, sourceSize - 1);
            r.end = ClampInt(endFrame, 0, sourceSize - 1);
        } else {
            r.start = 0;
            r.end = 0;
        }
        if (r.end < r.start) r.end = r.start;
        return r;
    }

private:
    static int ClampInt(int v, int lo, int hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    bool active_;
    int startFrame_;
    int endFrame_;
};

#endif