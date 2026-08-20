// hat_probe.cpp: feed a real WAV through SpectrumAnalyzer and print the
// per-block bar levels as the EQ8 view would draw them (px height).
#include "Application/Audio/SpectrumAnalyzer.h"
#include "Application/Utils/fixed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int readWav16(const char *path, short **out, int *outFrames, int *outCh, int *outRate) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    unsigned char h[12];
    if (fread(h, 1, 12, f) != 12) { fclose(f); return -1; }
    int ch = 0, rate = 0, bits = 0;
    unsigned int dataSz = 0;
    int foundData = 0;
    for (;;) {
        unsigned char c[8];
        if (fread(c, 1, 8, f) != 8) { fclose(f); return -5; }
        unsigned int sz = c[4] | (c[5] << 8) | (c[6] << 16) | (c[7] << 24);
        if (!memcmp(c, "fmt ", 4)) {
            unsigned char fh[16];
            if (fread(fh, 1, 16, f) != 16) { fclose(f); return -5; }
            ch = fh[2] | (fh[3] << 8);
            rate = fh[4] | (fh[5] << 8) | (fh[6] << 16) | (fh[7] << 24);
            bits = fh[14] | (fh[15] << 8);
            if (sz > 16) fseek(f, sz - 16, SEEK_CUR);
        } else if (!memcmp(c, "data", 4)) {
            dataSz = sz;
            foundData = 1;
            break;
        } else {
            fseek(f, sz, SEEK_CUR);
        }
    }
    if (!foundData || bits != 16) { fclose(f); return -2; }
    int frames = (int)(dataSz / (ch * 2));
    short *s = (short *)malloc(sizeof(short) * (size_t)frames * ch);
    if (!s) { fclose(f); return -3; }
    if (fread(s, 2, (size_t)frames * ch, f) != (size_t)frames * ch) { free(s); fclose(f); return -4; }
    fclose(f);
    *out = s; *outFrames = frames; *outCh = ch; *outRate = rate;
    return 0;
}

static float mixVULevel(float p) {
    if (p <= 0.0f) return 0.0f;
    float db = 20.0f * log10f(p);
    float v = (db + 24.0f) / 27.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

static float eq8Frac(float p) {
    float db = (p > 0.0f) ? 20.0f * log10f(p) : -80.0f;
    float frac = (db + 36.0f) / 40.0f;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    return frac;
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: hat_probe <wav>\n"); return 1; }
    short *s; int frames, ch, rate;
    int rc = readWav16(argv[1], &s, &frames, &ch, &rate);
    if (rc) { printf("wav read rc=%d\n", rc); return 1; }
    printf("wav: %d frames, %d ch, %d Hz\n", frames, ch, rate);

    // Resample to 48k (simple linear) if needed.
    const int RATE = 48000;
    const int kFrames = 2048;
    int outFrames = (int)((double)frames * RATE / rate);
    short *rs = (short *)malloc(sizeof(short) * (size_t)outFrames * 2);
    for (int i = 0; i < outFrames; i++) {
        double pos = (double)i * rate / RATE;
        int i0 = (int)pos;
        double frac = pos - i0;
        int i1 = i0 + 1 < frames ? i0 + 1 : i0;
        short l0 = ch >= 1 ? s[i0 * ch] : 0;
        short l1 = ch >= 1 ? s[i1 * ch] : 0;
        short r0 = ch >= 2 ? s[i0 * ch + 1] : l0;
        short r1 = ch >= 2 ? s[i1 * ch + 1] : l1;
        rs[i * 2] = (short)(l0 + (l1 - l0) * frac);
        rs[i * 2 + 1] = (short)(r0 + (r1 - r0) * frac);
    }

    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();
    sp.SetArmed(true);

    // Peak per bar over the whole file + per-block px mapping (EQ8 view:
    // h = mixVULevel(fp2fl(bin)*4) * 208, min 2 px).
    float barPeak[32] = {0};
    int maxPxOld[32] = {0};
    int maxPxNew[32] = {0};
    int blocks = 0;
    fixed *buf = (fixed *)malloc(sizeof(fixed) * 2 * kFrames);
    for (int off = 0; off < outFrames; off += kFrames) {
        int n = outFrames - off < kFrames ? outFrames - off : kFrames;
        for (int i = 0; i < kFrames; i++) {
            if (i < n) {
                fixed l = ((fixed)rs[(off + i) * 2]) << 15;
                fixed r = ((fixed)rs[(off + i) * 2 + 1]) << 15;
                buf[i * 2] = l; buf[i * 2 + 1] = r;
            } else {
                buf[i * 2] = 0; buf[i * 2 + 1] = 0;
            }
        }
        sp.FeedMix(buf, kFrames);
        if (!sp.Compute()) continue;
        blocks++;
        const fixed *bb = sp.Bins();
        for (int i = 0; i < sp.BinCount(); i++) {
            float v = fp2fl(bb[i]);
            if (v > barPeak[i]) barPeak[i] = v;
            int pxOld = (int)(mixVULevel(v * 4.0f) * 208.0f);
            if (pxOld < 2) pxOld = 2;
            if (pxOld > maxPxOld[i]) maxPxOld[i] = pxOld;
            int pxNew = (int)(eq8Frac(v * 4.0f) * 208.0f);
            if (pxNew < 2) pxNew = 2;
            if (pxNew > maxPxNew[i]) maxPxNew[i] = pxNew;
        }
    }
    printf("blocks=%d\n", blocks);
    const char *names[24] = {"20","46","72","100","140","200","280","390","550","780","1100","1550","2200","3100","4400","6200","8800","12500","17800","25000","?","?","?","?"};
    printf("bar: ");
    for (int i = 0; i < sp.BinCount(); i++) printf("%4s ", names[i] ? names[i] : "?");
    printf("\npeak: ");
    for (int i = 0; i < sp.BinCount(); i++) printf("%.4f ", barPeak[i]);
    printf("\npx-24: ");
    for (int i = 0; i < sp.BinCount(); i++) printf("%4d ", maxPxOld[i]);
    printf("\npx-36: ");
    for (int i = 0; i < sp.BinCount(); i++) printf("%4d ", maxPxNew[i]);
    printf("\n");
    free(s); free(rs); free(buf);
    return 0;
}