#!/usr/bin/env python3
"""Model the U2.51.4 producer burst and ALSA-clocked consumer."""

def simulate(seconds=60.0, period=480, periods=4, grace=0.024):
    producer_step = 1.0 / 60.0
    consumer_step = period / 48000.0
    burst = 800
    prime_target = period * periods
    ring = 0
    producer_t = 0.0
    consumer_t = None
    starve_deadline = None
    starvation_silence = 0
    writes = 0
    minimum = 10**9

    while True:
        events = [producer_t]
        if consumer_t is not None:
            events.append(consumer_t)
        if starve_deadline is not None:
            events.append(starve_deadline)
        now = min(events)
        if now > seconds:
            break

        if producer_t == now:
            ring += burst
            producer_t += producer_step
            if consumer_t is None and ring >= prime_target:
                consumer_t = now

        if consumer_t is not None and consumer_t == now:
            if ring >= period:
                ring -= period
                writes += 1
                minimum = min(minimum, ring)
                consumer_t += consumer_step
                starve_deadline = None
            else:
                starve_deadline = now + grace
                consumer_t = None

        if starve_deadline is not None and starve_deadline == now:
            if ring >= period:
                ring -= period
            else:
                starvation_silence += 1
            writes += 1
            minimum = min(minimum, ring)
            consumer_t = now + consumer_step
            starve_deadline = None

    return starvation_silence, writes, minimum, ring

starvation, writes, minimum, final_ring = simulate()
assert writes > 5000
assert starvation == 0, starvation
assert minimum >= 0
assert final_ring < 6000
print('U2514_ALSA_CLOCKED_PLAYBACK_MODEL_OK')
