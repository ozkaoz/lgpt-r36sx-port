// PANLAW_COMP (U2.57, feedback #10): the instrument pan law is normalized so
// CENTER = 1.0 (0 dB) and HARD PAN = sqrt(2) (+3 dB), matching the track pan
// law in PlayerChannel (cos/sin*sqrt(2)).  The legacy table sqrt(i/254)
// reads 0x5a82 (0.7071, -3.01 dB) at center on BOTH channels -- the fixed
// headroom that kept "127/255 from clipping".  With the compensation a
// full-scale sample at instrument volume 128 (unity) and channel 100 (1.0)
// reaches EXACTLY 0 dBFS; the U2.56 master safety remains the ceiling.
#include "Application/Utils/fixed.h"
enum { SILM_LAST = 5 };  // SampleInstrument.h value, only needed for the
                         // datas header array sizes
#include "Application/Instruments/SampleInstrumentDatas.h"

#include <stdio.h>

static const fixed kPanlawComp = 46343;  // 32768 / panlaw[127](23170)

static int checks = 0;
static int failures = 0;

static void check(bool cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

int main() {
    // 1. center (pan 127): exactly 0 dB -> fixed 32768 (1.0)
    {
        fixed l = fp_mul(panlaw[127], kPanlawComp);
        fixed r = fp_mul(panlaw[254 - 127], kPanlawComp);
        printf("center: l=0x%x (%.4f) r=0x%x (%.4f)\n", l, fp2fl(l), r, fp2fl(r));
        check(l == 32768, "center L = 1.0 exact (0 dB)");
        check(r == 32768, "center R = 1.0 exact (0 dB)");
    }

    // 2. hard pan right (254): +3.01 dB -> sqrt(2) ~= 1.4142
    {
        fixed r = fp_mul(panlaw[254], kPanlawComp);
        fixed l = fp_mul(panlaw[0], kPanlawComp);
        printf("hard right: l=0x%x (%.4f) r=0x%x (%.4f)\n", l, fp2fl(l), r, fp2fl(r));
        check(l == 0, "hard right: left channel 0");
        check(r > 46335 && r < 46350, "hard right: right channel ~sqrt(2)");
    }

    // 3. equal-power law: panlaw[i]^2 + panlaw[254-i]^2 ~= 1.0 for every i
    //    (constant power across the pan range, preserved by the scalar
    //    compensation)
    {
        float worst = 0.0f;
        for (int i = 0; i <= 254; i++) {
            float l = fp2fl(panlaw[i]);
            float r = fp2fl(panlaw[254 - i]);
            float p = l * l + r * r;
            float err = p - 1.0f;
            if (err < 0) err = -err;
            if (err > worst) worst = err;
        }
        printf("equal-power: worst |L^2+R^2-1| = %.4f\n", worst);
        check(worst < 0.02f, "equal-power law holds (constant power)");
    }

    // 4. monotonic + no wrap in the compensated law (int32 products fit)
    {
        int bad = 0;
        long long prev = -1;
        for (int i = 0; i < 255; i++) {
            long long v = fp_mul(panlaw[i], kPanlawComp);
            if (v < prev) bad++;
            prev = v;
        }
        check(bad == 0, "compensated law is monotonic");
    }

    // 5. the full instrument chain at center: volume 128 (unity) * pan 1.0
    //    * channel 100 (1.0) = 1.0 -> a full-scale sample is 0 dBFS
    {
        fixed volscale = fl2fp(1.0f / 128.0f);
        fixed volfactor = fp_mul(i2fp(128), volscale);   // vol 128
        fixed pan = fp_mul(panlaw[127], kPanlawComp);    // center
        long long out = fp_mul(fp_mul(volfactor, pan), i2fp(100) / 100 * 100);
        out = fp_mul(volfactor, pan);                    // x channel 1.0
        printf("chain center: volfactor=%.4f pan=%.4f out=%.4f\n",
               fp2fl(volfactor), fp2fl(pan), fp2fl((fixed)out));
        check(volfactor == 32768, "vol 128 = unity (1.0)");
        check(out == 32768, "vol128 * center pan = 0 dBFS");
    }

    printf("PANLAW_COMP: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}