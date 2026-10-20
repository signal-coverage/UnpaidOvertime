"""Procedural sound synthesis for UNPAID: OVERTIME (pure Python 3, standard library only).

Writes 16-bit PCM WAVs (44.1 kHz; mono for 3D emitters and one-shots, stereo for music and 2D beds) to
Content/UO/Audio/Source/, plus manifest.json (what uo_audio.py imports) and a loudness report.

Runs inside Unreal's Python (pipeline stage "audio") or standalone with any Python 3.9+:
    python Scripts/Editor/uo_audio_synth.py [--only Id.Prefix,...] [--report-only]

Everything is seeded, so the output is reproducible byte for byte.

Loop technique (no clicks at the seam): every loop is built as a circular signal
  * periodic tones use frequencies with a whole number of cycles in the loop length;
  * noise beds are rendered longer and their tail is equal-power crossfaded into the head;
  * discrete events (bubbles, key clicks, notes) are added with wrap-around indices;
  * filters/reverb run over [tail warm-up + loop] and only the loop part is kept (steady state).
"""
import array
import json
import math
import os
import random
import sys
import time
import wave

SR = 44100
TWO_PI = 2.0 * math.pi
SYNTH_VERSION = 1

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT_DIR = os.path.join(PROJECT_DIR, "Content", "UO", "Audio", "Source")
DOC_PATH = os.path.join(PROJECT_DIR, "Docs", "Audio.md")


def log(msg):
    print("[UOAudioSynth] " + str(msg))
    sys.stdout.flush()


# ==============================================================================================
# Basic signal helpers (signals are Python lists of floats in -1..1)
# ==============================================================================================

def N(seconds):
    return max(1, int(round(seconds * SR)))


def zeros(n):
    return [0.0] * n


def white(n, rng):
    r = rng.random
    return [r() * 2.0 - 1.0 for _ in range(n)]


def pink(n, rng):
    """Paul Kellet's pink noise filter over white noise (about -10 dB re white)."""
    r = rng.random
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0
    out = [0.0] * n
    for i in range(n):
        w = r() * 2.0 - 1.0
        b0 = 0.99886 * b0 + w * 0.0555179
        b1 = 0.99332 * b1 + w * 0.0750759
        b2 = 0.96900 * b2 + w * 0.1538520
        b3 = 0.86650 * b3 + w * 0.3104856
        b4 = 0.55000 * b4 + w * 0.5329522
        b5 = -0.7616 * b5 - w * 0.0168980
        out[i] = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362) * 0.11
        b6 = w * 0.115926
    return out


def brown(n, rng):
    """Leaky-integrated white noise (red/brown), roughly unit level."""
    r = rng.random
    y = 0.0
    out = [0.0] * n
    for i in range(n):
        y = (y + (r() * 2.0 - 1.0) * 0.05) * 0.996
        out[i] = y * 2.2
    return out


def lp1(x, fc):
    a = math.exp(-TWO_PI * fc / SR)
    b = 1.0 - a
    y = 0.0
    out = [0.0] * len(x)
    for i, s in enumerate(x):
        y = b * s + a * y
        out[i] = y
    return out


def hp1(x, fc):
    low = lp1(x, fc)
    return [s - t for s, t in zip(x, low)]


def biquad(x, kind, f0, q=0.707, gain_db=0.0):
    """RBJ cookbook biquad: lp, hp, bp (0 dB peak), peak."""
    f0 = min(f0, SR * 0.45)
    w0 = TWO_PI * f0 / SR
    cw = math.cos(w0)
    sw = math.sin(w0)
    alpha = sw / (2.0 * q)
    if kind == "lp":
        b0, b1, b2 = (1 - cw) / 2, 1 - cw, (1 - cw) / 2
        a0, a1, a2 = 1 + alpha, -2 * cw, 1 - alpha
    elif kind == "hp":
        b0, b1, b2 = (1 + cw) / 2, -(1 + cw), (1 + cw) / 2
        a0, a1, a2 = 1 + alpha, -2 * cw, 1 - alpha
    elif kind == "bp":
        b0, b1, b2 = alpha, 0.0, -alpha
        a0, a1, a2 = 1 + alpha, -2 * cw, 1 - alpha
    elif kind == "peak":
        A = 10 ** (gain_db / 40.0)
        b0, b1, b2 = 1 + alpha * A, -2 * cw, 1 - alpha * A
        a0, a1, a2 = 1 + alpha / A, -2 * cw, 1 - alpha / A
    else:
        raise ValueError(kind)
    b0 /= a0
    b1 /= a0
    b2 /= a0
    a1 /= a0
    a2 /= a0
    x1 = x2 = y1 = y2 = 0.0
    out = [0.0] * len(x)
    for i, s in enumerate(x):
        y = b0 * s + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2 = x1
        x1 = s
        y2 = y1
        y1 = y
        out[i] = y
    return out


def svf(x, cutoffs, q=0.7, mode="lp"):
    """Chamberlin state-variable filter with a per-sample cutoff (list or constant)."""
    n = len(x)
    if not isinstance(cutoffs, list):
        cutoffs = [cutoffs] * n
    damp = 1.0 / max(0.5, q)
    low = band = 0.0
    out = [0.0] * n
    lim = SR / 6.5
    for i in range(n):
        fc = cutoffs[i]
        if fc > lim:
            fc = lim
        f = 2.0 * math.sin(math.pi * fc / SR)
        # two passes per sample keep it stable at higher cutoffs
        for _ in range(2):
            low += f * 0.5 * band
            high = x[i] - low - damp * band
            band += f * 0.5 * high
        if mode == "lp":
            out[i] = low
        elif mode == "bp":
            out[i] = band
        else:
            out[i] = high
    return out


def scale(x, g):
    return [s * g for s in x]


def mix(*signals):
    n = max(len(s) for s in signals)
    out = [0.0] * n
    for s in signals:
        for i, v in enumerate(s):
            out[i] += v
    return out


def add_into(buf, x, offset, gain=1.0, wrap=False):
    n = len(buf)
    if wrap:
        for i, v in enumerate(x):
            buf[(offset + i) % n] += v * gain
    else:
        end = min(n, offset + len(x))
        for i in range(max(0, offset), end):
            buf[i] += x[i - offset] * gain


def mul_env(x, env):
    return [a * b for a, b in zip(x, env)]


def env_exp(n, decay_s, attack_s=0.002):
    """Linear attack, exponential decay (time constant decay_s)."""
    na = max(1, int(attack_s * SR))
    k = math.exp(-1.0 / max(1e-4, decay_s * SR))
    out = [0.0] * n
    e = 1.0
    for i in range(n):
        if i < na:
            out[i] = i / na
        else:
            out[i] = e
            e *= k
    return out


def env_ahr(n, attack_s, hold_s, release_s):
    """Attack, hold, raised-cosine release; zero after."""
    na = max(1, int(attack_s * SR))
    nh = int(hold_s * SR)
    nr = max(1, int(release_s * SR))
    out = [0.0] * n
    for i in range(n):
        if i < na:
            out[i] = math.sin(0.5 * math.pi * i / na) ** 2
        elif i < na + nh:
            out[i] = 1.0
        elif i < na + nh + nr:
            t = (i - na - nh) / nr
            out[i] = 0.5 + 0.5 * math.cos(math.pi * t)
    return out


def env_window(n):
    """Hann bump over the whole length."""
    return [0.5 - 0.5 * math.cos(TWO_PI * i / max(1, n - 1)) for i in range(n)]


def fades(x, fade_in_s=0.002, fade_out_s=0.012):
    n = len(x)
    fi = min(n // 2, max(1, int(fade_in_s * SR)))
    fo = min(n // 2, max(1, int(fade_out_s * SR)))
    y = list(x)
    for i in range(fi):
        y[i] *= i / fi
    for i in range(fo):
        y[n - 1 - i] *= i / fo
    return y


def sine(freq, n, phase=0.0, amp=1.0):
    """Constant-frequency sine, computed directly from the sample index so long loops stay exactly periodic
    (a recursive oscillator drifts enough over minutes of samples to leave a seam)."""
    w = TWO_PI * freq / SR
    sin = math.sin
    return [amp * sin(w * i + phase) for i in range(n)]


def sine_var(freqs, phase=0.0):
    out = [0.0] * len(freqs)
    ph = phase
    k = TWO_PI / SR
    sin = math.sin
    for i, f in enumerate(freqs):
        out[i] = sin(ph)
        ph += k * f
    return out


def saw_blep(freqs):
    """Band-limited sawtooth (PolyBLEP), per-sample frequency list."""
    out = [0.0] * len(freqs)
    ph = 0.0
    for i, f in enumerate(freqs):
        dt = f / SR
        v = 2.0 * ph - 1.0
        if ph < dt:
            t = ph / dt
            v -= t + t - t * t - 1.0
        elif ph > 1.0 - dt:
            t = (ph - 1.0) / dt
            v -= t * t + t + t + 1.0
        out[i] = v
        ph += dt
        if ph >= 1.0:
            ph -= 1.0
    return out


def softclip(x, drive=1.0):
    th = math.tanh
    return [th(s * drive) for s in x]


def lin_ramp(n, a, b):
    if n <= 1:
        return [b] * n
    return [a + (b - a) * i / (n - 1) for i in range(n)]


def exp_ramp(n, a, b):
    if n <= 1:
        return [b] * n
    r = math.log(b / a)
    return [a * math.exp(r * i / (n - 1)) for i in range(n)]


def resonators(x, modes):
    """Sum of band-pass resonators: modes = [(freq, q, gain)]."""
    out = [0.0] * len(x)
    for f, q, g in modes:
        y = biquad(x, "bp", f, q)
        for i, v in enumerate(y):
            out[i] += v * g
    return out


def peak(x):
    return max(1e-12, max(abs(v) for v in x))


def normalize(x, peak_db):
    g = (10 ** (peak_db / 20.0)) / peak(x)
    return [v * g for v in x]


def normalize_stereo(l, r, peak_db):
    g = (10 ** (peak_db / 20.0)) / max(peak(l), peak(r))
    return [v * g for v in l], [v * g for v in r]


def dc_block(x):
    return hp1(x, 18.0)


# ----------------------------------------------------------------------------------------------
# Loop builders
# ----------------------------------------------------------------------------------------------

def loopify(x, xfade):
    """Signal of length T + xfade -> seamless loop of length T (equal-power tail-into-head crossfade)."""
    t = len(x) - xfade
    y = x[:t]
    for i in range(xfade):
        a = i / xfade
        y[i] = x[i] * math.sin(0.5 * math.pi * a) + x[t + i] * math.cos(0.5 * math.pi * a)
    return y


def loop_noise(kind, n, rng, xfade_s=0.5):
    xf = N(xfade_s)
    gen = {"white": white, "pink": pink, "brown": brown}[kind]
    return loopify(gen(n + xf, rng), xf)


def loop_process(x, fn, warm_s=1.5):
    """Runs a (causal) process over a circular signal and keeps the steady-state loop."""
    w = min(len(x), N(warm_s))
    y = fn(x[-w:] + x)
    return y[w:]


def cyc(freq, loop_s):
    """Nearest frequency with a whole number of cycles in loop_s."""
    return max(1, round(freq * loop_s)) / loop_s


# ----------------------------------------------------------------------------------------------
# Reverb (Freeverb-style, mono in -> stereo out)
# ----------------------------------------------------------------------------------------------

_COMBS = [1116, 1188, 1277, 1356, 1422, 1491]
_ALLPASS = [556, 441, 341]


def _comb_bank(x, delays, feedback, damp):
    out = [0.0] * len(x)
    for d in delays:
        buf = [0.0] * d
        idx = 0
        filt = 0.0
        for i, s in enumerate(x):
            y = buf[idx]
            filt = y * (1.0 - damp) + filt * damp
            buf[idx] = s + filt * feedback
            idx += 1
            if idx == d:
                idx = 0
            out[i] += y
    return out


def _allpass(x, d, g=0.5):
    buf = [0.0] * d
    idx = 0
    out = [0.0] * len(x)
    for i, s in enumerate(x):
        b = buf[idx]
        y = -s + b
        buf[idx] = s + b * g
        idx += 1
        if idx == d:
            idx = 0
        out[i] = y
    return out


def reverb_stereo(x, size=0.8, damp=0.35, spread=23, combs=4):
    """Returns (wetL, wetR). size 0..1 maps to feedback 0.7..0.92."""
    fb = 0.7 + 0.22 * size
    outs = []
    for side in (0, 1):
        delays = [d + side * spread for d in _COMBS[:combs]]
        y = _comb_bank(x, delays, fb, damp)
        for a in _ALLPASS[:2]:
            y = _allpass(y, a + side * spread)
        outs.append([v * (0.12 / combs * 4) for v in y])
    return outs[0], outs[1]


def reverb_mono(x, size=0.6, damp=0.4, mix_wet=0.25):
    wl, wr = reverb_stereo(x, size, damp, combs=4)
    return [d + (a + b) * 0.5 * mix_wet for d, a, b in zip(x, wl, wr)]


def pad(x, seconds):
    return x + zeros(N(seconds))


# ==============================================================================================
# Building blocks for sounds
# ==============================================================================================

def bell(freq, dur, decay=1.2, partials=None, amp=1.0):
    """Additive bell: partials (ratio, amplitude, decay multiplier)."""
    if partials is None:
        partials = [(1.0, 1.0, 1.0), (2.0, 0.45, 0.65), (2.76, 0.3, 0.45), (5.4, 0.12, 0.25), (8.93, 0.06, 0.15)]
    n = N(dur)
    out = [0.0] * n
    for ratio, a, dm in partials:
        f = freq * ratio
        if f > SR * 0.42:
            continue
        w = TWO_PI * f / SR
        c = 2.0 * math.cos(w)
        s1 = math.sin(-w)
        s2 = math.sin(-2 * w)
        k = math.exp(-1.0 / (decay * dm * SR))
        e = a * amp
        for i in range(n):
            s0 = c * s1 - s2
            out[i] += s0 * e
            e *= k
            s2 = s1
            s1 = s0
    att = N(0.002)
    for i in range(min(att, n)):
        out[i] *= i / att
    return out


def fm_note(freq, dur, index=1.5, ratio=1.0, index_decay=0.3, amp_decay=1.5, release=0.08, attack=0.003,
            vib_hz=0.0, vib_cents=0.0, detune_cents=0.0, tail=0.3):
    """Two-operator FM note (electric-piano / soft lead). Returns dur + tail samples."""
    n = N(dur + tail)
    nd = N(dur)
    out = [0.0] * n
    f = freq * (2 ** (detune_cents / 1200.0))
    wc = TWO_PI * f / SR
    wm = wc * ratio
    ki = math.exp(-1.0 / (index_decay * SR))
    ka = math.exp(-1.0 / (amp_decay * SR))
    kr = math.exp(-1.0 / (release * SR))
    na = max(1, N(attack))
    sin = math.sin
    pc = pm = 0.0
    idx = index
    a = 1.0
    vib_w = TWO_PI * vib_hz / SR
    vib_depth = (2 ** (vib_cents / 1200.0)) - 1.0
    for i in range(n):
        if vib_hz > 0.0 and i > na * 20:
            m = 1.0 + vib_depth * sin(vib_w * i)
        else:
            m = 1.0
        v = sin(pc + idx * sin(pm))
        g = a * (i / na if i < na else 1.0)
        out[i] = v * g
        pc += wc * m
        pm += wm * m
        idx = idx * ki + 0.15 * (1 - ki) * index
        a *= ka
        if i >= nd:
            a *= kr
    return out


def noise_burst(rng, dur, fc, q=1.0, decay=0.05, attack=0.001, kind="bp"):
    n = N(dur)
    x = white(n, rng)
    x = biquad(x, kind, fc, q)
    return mul_env(x, env_exp(n, decay, attack))


def click(rng, freq=3000, dur=0.02, decay=0.003, q=2.0, body=0.0, body_freq=400):
    n = N(dur)
    x = noise_burst(rng, dur, freq, q, decay, 0.0003)
    if body > 0:
        b = mul_env(sine(body_freq, n), env_exp(n, decay * 3, 0.0005))
        x = [a + c * body for a, c in zip(x, b)]
    return x


def midi_hz(m):
    return 440.0 * 2 ** ((m - 69) / 12.0)


# ==============================================================================================
# Sound definitions. Each returns mono list or (left, right) tuple.
# ==============================================================================================

# ---------------- Player ----------------

def footstep_carpet(v):
    rng = random.Random(1000 + v)
    n = N(0.32)
    out = zeros(n)
    # heel: muffled thump
    heel = noise_burst(rng, 0.22, 380 + rng.uniform(-60, 80), 0.7, 0.045 + rng.uniform(0, 0.02), 0.003, "lp")
    body = mul_env(sine(75 + rng.uniform(-10, 15), N(0.2)), env_exp(N(0.2), 0.04, 0.002))
    add_into(out, heel, 0, 1.0)
    add_into(out, body, 0, 0.35)
    # toe roll and fabric scuff
    t2 = N(0.055 + rng.uniform(0, 0.03))
    toe = noise_burst(rng, 0.15, 520 + rng.uniform(-80, 80), 0.8, 0.03, 0.004, "lp")
    add_into(out, toe, t2, 0.55)
    scuff = noise_burst(rng, 0.12, 2400 + rng.uniform(-400, 600), 1.2, 0.035, 0.012)
    add_into(out, scuff, N(0.03), 0.12 + rng.uniform(0, 0.06))
    return fades(dc_block(out), 0.001, 0.03)


def footstep_tile(v):
    rng = random.Random(2000 + v)
    n = N(0.38)
    out = zeros(n)
    heel_click = noise_burst(rng, 0.05, 3200 + rng.uniform(-500, 700), 0.9, 0.004, 0.0002)
    ring = resonators(click(rng, 2500, 0.15, 0.002, 1.0), [(2300 + rng.uniform(-200, 300), 18, 0.5), (3600 + rng.uniform(-300, 300), 22, 0.35)])
    ring = mul_env(ring, env_exp(len(ring), 0.03))
    thud = mul_env(sine(115 + rng.uniform(-15, 15), N(0.12)), env_exp(N(0.12), 0.025, 0.001))
    low = noise_burst(rng, 0.1, 700, 0.8, 0.02, 0.0005, "lp")
    add_into(out, heel_click, 0, 0.9)
    add_into(out, ring, 0, 0.6)
    add_into(out, thud, 0, 0.5)
    add_into(out, low, 0, 0.6)
    t2 = N(0.06 + rng.uniform(0, 0.03))
    toe = noise_burst(rng, 0.05, 2600 + rng.uniform(-300, 300), 1.0, 0.006, 0.0005)
    add_into(out, toe, t2, 0.35)
    add_into(out, noise_burst(rng, 0.06, 600, 0.8, 0.012, 0.001, "lp"), t2, 0.3)
    out = reverb_mono(out, 0.35, 0.5, 0.12)
    return fades(dc_block(out), 0.0005, 0.03)


def player_jump(v):
    rng = random.Random(3000 + v)
    n = N(0.45)
    x = white(n, rng)
    sweep = exp_ramp(n, 500, 1700 + 300 * v)
    w = svf(x, sweep, 1.2, "bp")
    w = mul_env(w, env_ahr(n, 0.08, 0.05, 0.25))
    push = noise_burst(rng, 0.2, 420, 0.7, 0.05, 0.003, "lp")
    out = zeros(n)
    add_into(out, w, 0, 0.5)
    add_into(out, push, 0, 0.9)
    return fades(dc_block(out), 0.002, 0.04)


def player_land(v):
    rng = random.Random(3100 + v)
    n = N(0.55)
    out = zeros(n)
    thump = noise_burst(rng, 0.4, 320 + 40 * v, 0.7, 0.07, 0.002, "lp")
    sub = mul_env(sine_var(exp_ramp(N(0.3), 80, 45)), env_exp(N(0.3), 0.07, 0.002))
    cloth = mul_env(biquad(white(N(0.3), rng), "bp", 1800, 0.9), env_exp(N(0.3), 0.08, 0.02))
    add_into(out, thump, 0, 1.0)
    add_into(out, sub, 0, 0.6)
    add_into(out, cloth, N(0.01), 0.12)
    add_into(out, noise_burst(rng, 0.15, 500, 0.8, 0.03, 0.003, "lp"), N(0.09), 0.4)
    return fades(dc_block(out), 0.0008, 0.05)


# ---------------- Interactions ----------------

def interact_start(v):
    rng = random.Random(4000)
    out = zeros(N(0.12))
    add_into(out, click(rng, 2600, 0.04, 0.004, 2.0, 0.5, 520), 0, 1.0)
    add_into(out, click(rng, 3400, 0.04, 0.003, 2.0, 0.3, 700), N(0.035), 0.6)
    return fades(dc_block(out), 0.0005, 0.02)


def interact_hold_loop(v):
    """Soft paper/pen activity, 2 s seamless loop."""
    rng = random.Random(4100)
    T = 2.0
    n = N(T)
    x = loop_noise("white", n, rng, 0.25)
    x = loop_process(x, lambda s: biquad(s, "bp", 2600, 0.9), 0.3)
    # 6 strokes per loop with varied strength (circular)
    am = [0.0] * n
    strokes = 6
    amps = [0.6 + 0.4 * rng.random() for _ in range(strokes)]
    for k in range(strokes):
        start = int(k * n / strokes + rng.uniform(-0.03, 0.03) * SR)
        ln = N(0.22)
        e = env_ahr(ln, 0.04, 0.05, 0.13)
        for i in range(ln):
            am[(start + i) % n] += e[i] * amps[k]
    y = [a * (0.12 + 0.88 * b) for a, b in zip(x, am)]
    taps = zeros(n)
    for k in range(4):
        add_into(taps, click(rng, 1800, 0.03, 0.004, 1.5, 0.4, 300), int((k + 0.35) * n / 4), 0.25, wrap=True)
    return mix(y, taps)


def interact_complete(v):
    rng = random.Random(4200)
    n = N(0.3)
    out = zeros(n)
    body = mul_env(sine(210, n), env_exp(n, 0.05, 0.001))
    add_into(out, body, 0, 0.7)
    add_into(out, click(rng, 2200, 0.05, 0.005, 1.5), 0, 0.6)
    add_into(out, noise_burst(rng, 0.12, 600, 0.8, 0.03, 0.001, "lp"), 0, 0.5)
    return fades(dc_block(out), 0.0005, 0.03)


def interact_denied(v):
    """Soft low double buzz (beating squares, low-passed)."""
    n = N(0.42)
    out = zeros(n)
    for k, start in enumerate((0.0, 0.17)):
        ln = N(0.12)
        f1 = saw_blep([138.0] * ln)
        f2 = saw_blep([146.5] * ln)
        tone = [(a + b) * 0.5 for a, b in zip(f1, f2)]
        tone = biquad(tone, "lp", 1500, 0.7)
        tone = mul_env(tone, env_ahr(ln, 0.006, 0.08, 0.03))
        add_into(out, tone, N(start), 1.0)
    return fades(dc_block(out), 0.001, 0.03)


def printer_cartridge(v):
    rng = random.Random(4300)
    n = N(0.55)
    out = zeros(n)
    slide_n = N(0.2)
    slide = svf(white(slide_n, rng), exp_ramp(slide_n, 1200, 2600), 1.5, "bp")
    slide = mul_env(slide, env_ahr(slide_n, 0.05, 0.08, 0.07))
    add_into(out, slide, 0, 0.35)
    snap = mix(click(rng, 3800, 0.08, 0.004, 3.0, 0.6, 900),
               resonators(click(rng, 3000, 0.08, 0.001, 1.0), [(1850, 25, 0.5), (4200, 30, 0.3)]))
    add_into(out, snap, N(0.19), 1.0)
    add_into(out, click(rng, 2400, 0.05, 0.003, 2.0, 0.3, 600), N(0.27), 0.35)
    return fades(dc_block(out), 0.002, 0.04)


def printer_whir_loop(v):
    """Laser printer running: motor, fan, roller ticks. 3 s seamless loop."""
    rng = random.Random(4400)
    T = 3.0
    n = N(T)
    motor_f = cyc(108.0, T)
    motor = saw_blep([motor_f] * (2 * n))[n:]
    motor = loop_process(motor, lambda s: biquad(biquad(s, "lp", 520, 0.9), "lp", 900, 0.7), 0.5)
    whine = sine(cyc(1840.0, T), n, amp=0.05)
    am = sine(cyc(2.0, T), n)
    whine = [w * (0.7 + 0.3 * a) for w, a in zip(whine, am)]
    fan = loop_noise("pink", n, rng, 0.3)
    fan = loop_process(fan, lambda s: biquad(s, "bp", 900, 0.6), 0.3)
    ticks = zeros(n)
    for k in range(12):
        add_into(ticks, click(rng, 2200, 0.03, 0.004, 2.5, 0.3, 450), int((k + 0.2) * n / 12), 0.22, wrap=True)
    paper = loop_noise("white", n, rng, 0.3)
    paper = loop_process(paper, lambda s: biquad(s, "bp", 3800, 0.8), 0.3)
    pam = sine(cyc(1.0, T), n)
    paper = [p * 0.08 * (0.5 + 0.5 * a) for p, a in zip(paper, pam)]
    return mix(scale(motor, 0.55), whine, scale(fan, 0.35), ticks, paper)


def printer_feed(v):
    rng = random.Random(4500)
    n = N(1.5)
    out = zeros(n)
    # roller ratchet
    k = 0
    t = 0.05
    while t < 0.95:
        add_into(out, click(rng, 2400 + rng.uniform(-200, 200), 0.025, 0.003, 2.0, 0.25, 500), N(t), 0.35 * (1.0 - t * 0.5))
        t += 1.0 / 26.0
        k += 1
    swish_n = N(0.9)
    swish = svf(white(swish_n, rng), exp_ramp(swish_n, 900, 4200), 1.0, "bp")
    swish = mul_env(swish, env_ahr(swish_n, 0.25, 0.3, 0.35))
    add_into(out, swish, N(0.15), 0.45)
    motor_n = N(1.3)
    motor = saw_blep(exp_ramp(motor_n, 120, 85))
    motor = biquad(motor, "lp", 600, 0.8)
    motor = mul_env(motor, env_ahr(motor_n, 0.05, 0.85, 0.4))
    add_into(out, motor, 0, 0.35)
    add_into(out, click(rng, 1600, 0.12, 0.012, 1.5, 0.6, 220), N(1.05), 0.6)
    return fades(dc_block(out), 0.003, 0.08)


def coffee_gurgle_loop(v):
    """Drip brewer: bubbles, trickle, faint steam. 4 s seamless loop."""
    rng = random.Random(4600)
    T = 4.0
    n = N(T)
    bubbles = zeros(n)
    t = 0.0
    while t < T:
        dur = rng.uniform(0.02, 0.06)
        bn = N(dur)
        f0 = rng.uniform(250, 520)
        f1 = f0 * rng.uniform(1.6, 2.6)
        b = sine_var(exp_ramp(bn, f0, f1))
        b = mul_env(b, env_exp(bn, dur * 0.35, 0.002))
        add_into(bubbles, b, N(t), rng.uniform(0.2, 0.6), wrap=True)
        t += rng.expovariate(14.0) + (rng.random() < 0.08) * 0.25
    bubbles = loop_process(bubbles, lambda s: biquad(s, "lp", 2400, 0.7), 0.2)
    trickle = loop_noise("white", n, rng, 0.4)
    trickle = loop_process(trickle, lambda s: biquad(s, "bp", 1500, 1.4), 0.3)
    tam = sine(cyc(3.0, T), n)
    trickle = [x * (0.08 + 0.03 * a) for x, a in zip(trickle, tam)]
    steam = loop_noise("white", n, rng, 0.4)
    steam = loop_process(steam, lambda s: biquad(s, "hp", 4500, 0.7), 0.3)
    boil = loop_noise("brown", n, rng, 0.4)
    boil = loop_process(boil, lambda s: biquad(s, "lp", 220, 0.7), 0.5)
    return mix(bubbles, trickle, scale(steam, 0.025), scale(boil, 0.25))


def coffee_beep(v):
    n = N(0.75)
    out = zeros(n)
    for k in range(3):
        ln = N(0.09)
        tone = mix(sine(2093.0, ln), sine(4186.0, ln, amp=0.08), sine(6279.0, ln, amp=0.05))
        tone = mul_env(tone, env_ahr(ln, 0.004, 0.07, 0.015))
        add_into(out, tone, N(0.02 + k * 0.16), 0.8)
    return fades(dc_block(out), 0.001, 0.03)


def drawer_slide(rng, dur, f_scrape=1400):
    n = N(dur)
    rumble = biquad(white(n, rng), "lp", 260, 0.8)
    am = sine(34.0, n)
    rumble = [r * (0.6 + 0.4 * a) for r, a in zip(rumble, am)]
    scrape = resonators(white(n, rng), [(f_scrape, 6, 0.4), (f_scrape * 1.73, 8, 0.25), (f_scrape * 2.61, 10, 0.15)])
    body = mix(scale(rumble, 1.0), scale(scrape, 0.5))
    return mul_env(body, env_ahr(n, dur * 0.25, dur * 0.4, dur * 0.35))


def metal_clunk(rng, dur=0.5, low=95, amp_ring=1.0):
    n = N(dur)
    out = zeros(n)
    exc = click(rng, 1800, 0.01, 0.0015, 0.8)
    ring = resonators(exc + zeros(n - len(exc)), [(420, 20, 1.0), (1130, 25, 0.7), (2370, 30, 0.45), (3810, 35, 0.25)])
    ring = mul_env(ring, env_exp(n, 0.09))
    add_into(out, ring, 0, 1.6 * amp_ring)
    add_into(out, mul_env(sine(low, n), env_exp(n, 0.06, 0.001)), 0, 0.6)
    add_into(out, noise_burst(rng, 0.15, 500, 0.7, 0.03, 0.0005, "lp"), 0, 0.8)
    return out


def drawer_open(v):
    rng = random.Random(4700)
    n = N(0.85)
    out = zeros(n)
    add_into(out, click(rng, 2800, 0.04, 0.003, 2.0, 0.4, 600), 0, 0.5)
    add_into(out, drawer_slide(rng, 0.55), N(0.03), 0.8)
    add_into(out, metal_clunk(rng, 0.25, 90, 0.35), N(0.56), 0.5)
    return fades(dc_block(reverb_mono(out, 0.3, 0.5, 0.1)), 0.001, 0.05)


def drawer_close(v):
    rng = random.Random(4800 + v)
    n = N(0.9)
    out = zeros(n)
    slide_len = 0.3 if v == 1 else 0.22
    add_into(out, drawer_slide(rng, slide_len, 1550 + 150 * v), 0, 0.7)
    add_into(out, metal_clunk(rng, 0.6, 85 + 10 * v, 1.0 if v == 1 else 1.25), N(slide_len - 0.02), 1.0)
    add_into(out, click(rng, 3500, 0.03, 0.002, 3.0, 0.3, 800), N(slide_len + 0.05), 0.3)
    return fades(dc_block(reverb_mono(out, 0.3, 0.5, 0.12)), 0.002, 0.06)


def paper_rustle(v):
    rng = random.Random(4900 + v)
    dur = 0.45 + 0.12 * v
    n = N(dur)
    base = white(n, rng)
    base = biquad(biquad(base, "hp", 1600, 0.7), "peak", 4200, 1.0, 4.0)
    # crinkle: random micro-transients modulating the noise
    am = [0.0] * n
    t = 0.0
    while t < dur - 0.03:
        ln = N(rng.uniform(0.008, 0.03))
        e = env_exp(ln, ln / SR * 0.4, 0.001)
        add_into(am, e, N(t), rng.uniform(0.3, 1.0))
        t += rng.expovariate(70.0)
    shape = env_ahr(n, 0.05, dur * 0.5, dur * 0.4)
    y = [b * (0.15 + a) * s for b, a, s in zip(base, am, shape)]
    return fades(dc_block(y), 0.002, 0.04)


def supply_rummage(v):
    rng = random.Random(5000 + v)
    dur = 1.15 + 0.2 * v
    n = N(dur)
    out = zeros(n)
    t = 0.04
    while t < dur - 0.25:
        kind = rng.random()
        if kind < 0.45:
            # cardboard / box knock
            add_into(out, noise_burst(rng, 0.12, rng.uniform(280, 520), 1.2, 0.03, 0.001, "bp"), N(t), rng.uniform(0.5, 1.0))
        elif kind < 0.8:
            # plastic clack
            add_into(out, click(rng, rng.uniform(1800, 3500), 0.05, 0.004, 3.0, 0.4, rng.uniform(500, 900)), N(t), rng.uniform(0.3, 0.6))
        else:
            # small metal (staples, clips)
            exc = click(rng, 4000, 0.01, 0.001, 1.0)
            ring = resonators(exc + zeros(N(0.15)), [(rng.uniform(2800, 4200), 40, 1.0), (rng.uniform(5200, 6800), 40, 0.6)])
            add_into(out, mul_env(ring, env_exp(len(ring), 0.04)), N(t), 0.5)
        t += rng.uniform(0.06, 0.2)
    rustle_n = N(dur)
    rustle = biquad(white(rustle_n, rng), "bp", 2200, 0.8)
    rustle = mul_env(rustle, env_ahr(rustle_n, 0.1, dur * 0.5, dur * 0.35))
    add_into(out, rustle, 0, 0.12)
    return fades(dc_block(reverb_mono(out, 0.25, 0.5, 0.1)), 0.002, 0.06)


def item_pickup(v):
    rng = random.Random(5100)
    n = N(0.3)
    w = svf(white(n, rng), exp_ramp(n, 700, 3200), 1.3, "bp")
    w = mul_env(w, env_ahr(n, 0.04, 0.03, 0.12))
    out = zeros(n)
    add_into(out, w, 0, 0.6)
    add_into(out, click(rng, 2400, 0.05, 0.004, 2.0, 0.5, 700), N(0.1), 0.7)
    return fades(dc_block(out), 0.002, 0.03)


def item_drop(v):
    rng = random.Random(5200)
    n = N(0.5)
    out = zeros(n)
    add_into(out, noise_burst(rng, 0.25, 420, 0.8, 0.04, 0.0008, "lp"), 0, 1.0)
    add_into(out, mul_env(sine(140, N(0.15)), env_exp(N(0.15), 0.03, 0.0008)), 0, 0.5)
    add_into(out, click(rng, 1900, 0.04, 0.004, 2.0, 0.3, 400), N(0.004), 0.4)
    add_into(out, noise_burst(rng, 0.12, 480, 0.8, 0.02, 0.0008, "lp"), N(0.13), 0.35)
    return fades(dc_block(out), 0.0005, 0.04)


def task_chime(v):
    """Cheerful two-note 'ding' (G5 -> C6), soft FM bell with a little room."""
    n = N(1.6)
    out = zeros(n)
    partials = [(1.0, 1.0, 1.0), (2.0, 0.25, 0.5), (3.0, 0.08, 0.3), (4.2, 0.05, 0.2)]
    add_into(out, bell(midi_hz(79), 1.2, 0.5, partials), 0, 0.7)
    add_into(out, bell(midi_hz(84), 1.45, 0.6, partials), N(0.11), 0.9)
    l, r = reverb_stereo(out, 0.45, 0.45, combs=4)
    y = [d + (a + b) * 0.3 for d, a, b in zip(out, l, r)]
    return fades(dc_block(y), 0.0005, 0.1)


def brass(freq, dur, rng, attack=0.04):
    n = N(dur + 0.35)
    nd = N(dur)
    voices = []
    for det in (-6.0, 0.0, 7.0):
        f = freq * 2 ** (det / 1200.0)
        voices.append(saw_blep([f] * n))
    raw = [(a + b + c) / 3 for a, b, c in zip(*voices)]
    cut = [0.0] * n
    na = N(attack)
    for i in range(n):
        if i < na:
            cut[i] = 400 + 3400 * (i / na)
        elif i < nd:
            cut[i] = 1600 + 2200 * math.exp(-(i - na) / (0.18 * SR))
        else:
            cut[i] = 1600 * math.exp(-(i - nd) / (0.1 * SR)) + 200
    y = svf(raw, cut, 0.9, "lp")
    return mul_env(y, env_ahr(n, attack, max(0.0, dur - attack), 0.3))


def task_all_done(v):
    """Short corporate fanfare: C-E-G arpeggio up to a bright C major chord."""
    rng = random.Random(5300)
    total = 2.6
    n = N(total)
    out = zeros(n)
    step = 0.13
    for k, m in enumerate((60, 64, 67)):
        add_into(out, brass(midi_hz(m), 0.12, rng, 0.02), N(k * step), 0.45)
        add_into(out, bell(midi_hz(m + 24), 0.6, 0.35), N(k * step), 0.12)
    chord_t = N(3 * step)
    for m in (60, 64, 67, 72):
        add_into(out, brass(midi_hz(m), 1.1, rng, 0.03), chord_t, 0.32)
    add_into(out, bell(midi_hz(84), 1.6, 0.8), chord_t, 0.25)
    add_into(out, bell(midi_hz(91), 1.4, 0.6), chord_t + N(0.05), 0.12)
    # timpani-ish hit
    timp = mul_env(sine_var(exp_ramp(N(0.8), 110, 92)), env_exp(N(0.8), 0.25, 0.002))
    add_into(out, timp, chord_t, 0.5)
    l, r = reverb_stereo(out, 0.6, 0.4, combs=4)
    lo = [d + a * 0.35 for d, a in zip(out, l)]
    ro = [d + b * 0.35 for d, b in zip(out, r)]
    lo = fades(dc_block(lo), 0.001, 0.25)
    ro = fades(dc_block(ro), 0.001, 0.25)
    return lo, ro


# ---------------- Day flow ----------------

def pa_filter(x):
    y = biquad(x, "hp", 280, 0.8)
    y = biquad(y, "lp", 4200, 0.8)
    y = biquad(y, "peak", 1800, 1.2, 3.0)
    return y


def day_shift_bell(v):
    """Classic 3-tone PA chime (ding-dong-dong): E5, C5, G4 through a ceiling speaker."""
    total = 4.2
    n = N(total)
    out = zeros(n)
    partials = [(1.0, 1.0, 1.0), (2.0, 0.35, 0.55), (3.0, 0.12, 0.35), (4.07, 0.05, 0.25)]
    for k, (m, t) in enumerate(((76, 0.0), (72, 0.55), (67, 1.1))):
        add_into(out, bell(midi_hz(m), 2.8, 1.1, partials), N(t), 0.8)
    out = pa_filter(out)
    l, r = reverb_stereo(out, 0.7, 0.45, combs=6)
    lo = [d + a * 0.45 for d, a in zip(out, l)]
    ro = [d + b * 0.45 for d, b in zip(out, r)]
    return fades(dc_block(lo), 0.001, 0.4), fades(dc_block(ro), 0.001, 0.4)


def day_overtime_stinger(v):
    """Dissonant low swell into a hit: A2 + Bb2 + Eb3 brass cluster, sub boom, reverse noise rise."""
    rng = random.Random(6100)
    total = 4.5
    n = N(total)
    out = zeros(n)
    rise_n = N(0.6)
    rise = svf(white(rise_n, rng), exp_ramp(rise_n, 300, 3000), 1.0, "bp")
    rise = mul_env(rise, [(i / rise_n) ** 2.5 for i in range(rise_n)])
    add_into(out, rise, 0, 0.35)
    hit = N(0.6)
    for m in (45, 46, 51, 57):
        add_into(out, brass(midi_hz(m), 2.2, rng, 0.02), hit, 0.35)
    boom = mul_env(sine_var(exp_ramp(N(2.0), 62, 34)), env_exp(N(2.0), 0.5, 0.003))
    add_into(out, boom, hit, 0.9)
    add_into(out, noise_burst(rng, 0.6, 300, 0.7, 0.12, 0.001, "lp"), hit, 0.6)
    # high dissonant shimmer
    sh = mix(sine(midi_hz(81), N(2.5), amp=0.5), sine(midi_hz(82) + 3, N(2.5), amp=0.5))
    sh = mul_env(sh, env_ahr(N(2.5), 0.3, 0.8, 1.4))
    add_into(out, sh, hit, 0.05)
    l, r = reverb_stereo(out, 0.85, 0.35, combs=6)
    lo = [d + a * 0.5 for d, a in zip(out, l)]
    ro = [d + b * 0.5 for d, b in zip(out, r)]
    return fades(dc_block(lo), 0.005, 0.6), fades(dc_block(ro), 0.005, 0.6)


def day_power_down(v):
    """Lights out: falling electrical whine, falling whoosh, relay clunk."""
    rng = random.Random(6200)
    total = 3.0
    n = N(total)
    out = zeros(n)
    fall_n = N(1.8)
    whine = sine_var(exp_ramp(fall_n, 520, 38))
    whine = mul_env(whine, env_ahr(fall_n, 0.01, 0.6, 1.2))
    buzz = softclip(sine_var(exp_ramp(fall_n, 120, 18)), 3.0)
    buzz = biquad(buzz, "lp", 900, 0.7)
    buzz = mul_env(buzz, env_ahr(fall_n, 0.01, 0.5, 1.3))
    add_into(out, whine, 0, 0.3)
    add_into(out, buzz, 0, 0.35)
    wn = N(2.0)
    whoosh = svf(white(wn, rng), exp_ramp(wn, 2500, 120), 0.8, "lp")
    whoosh = mul_env(whoosh, env_ahr(wn, 0.05, 0.4, 1.5))
    add_into(out, whoosh, 0, 0.5)
    add_into(out, metal_clunk(rng, 0.6, 70, 0.4), 0, 0.5)
    add_into(out, mul_env(sine(48, N(1.2)), env_exp(N(1.2), 0.3, 0.003)), 0, 0.4)
    return fades(dc_block(reverb_mono(out, 0.6, 0.4, 0.2)), 0.001, 0.3)


def day_closing_bell(v):
    """Building closing alarm: electric striker bell, 2.4 s of strikes then ring-out."""
    rng = random.Random(6300)
    total = 4.0
    n = N(total)
    exc = zeros(n)
    t = 0.0
    while t < 2.4:
        add_into(exc, [1.0, -0.6, 0.3], N(t), rng.uniform(0.8, 1.0))
        t += 1.0 / 19.0
    modes = [(1180, 60, 1.0), (2950, 70, 0.6), (5100, 80, 0.3), (2240, 50, 0.4)]
    ring = resonators(exc, modes)
    ring = softclip(scale(ring, 3.0), 1.0)
    ring = biquad(ring, "hp", 300, 0.7)
    l, r = reverb_stereo(ring, 0.75, 0.4, combs=6)
    lo = [d + a * 0.4 for d, a in zip(ring, l)]
    ro = [d + b * 0.4 for d, b in zip(ring, r)]
    return fades(dc_block(lo), 0.001, 0.5), fades(dc_block(ro), 0.001, 0.5)


def eval_star(v):
    """Star reveal ping (C6, E6, G6) with a sparkle on top."""
    m = (84, 88, 91)[v - 1]
    rng = random.Random(6400 + v)
    n = N(1.3)
    out = zeros(n)
    add_into(out, bell(midi_hz(m), 1.2, 0.5, [(1.0, 1.0, 1.0), (2.0, 0.3, 0.5), (3.01, 0.15, 0.3), (4.17, 0.08, 0.2)]), 0, 0.8)
    for k in range(5):
        add_into(out, bell(midi_hz(m + 12 + rng.choice((0, 4, 7, 12))), 0.3, 0.08), N(0.03 + k * 0.035), 0.08)
    l, r = reverb_stereo(out, 0.5, 0.4, combs=4)
    y = [d + (a + b) * 0.25 for d, a, b in zip(out, l, r)]
    return fades(dc_block(y), 0.0005, 0.15)


def eval_fired(v):
    """Low 'you're fired' thud: sub drop, dull impact, low minor-second cluster."""
    rng = random.Random(6500)
    n = N(2.4)
    out = zeros(n)
    add_into(out, mul_env(sine_var(exp_ramp(N(1.6), 70, 30)), env_exp(N(1.6), 0.4, 0.002)), 0, 1.0)
    add_into(out, noise_burst(rng, 0.5, 260, 0.7, 0.09, 0.001, "lp"), 0, 0.8)
    for m in (36, 37, 43):
        note = fm_note(midi_hz(m), 1.2, 1.2, 1.0, 0.4, 0.9, 0.2, 0.004, tail=0.6)
        add_into(out, biquad(note, "lp", 700, 0.7), 0, 0.22)
    return fades(dc_block(reverb_mono(out, 0.7, 0.45, 0.25)), 0.0008, 0.4)


def eval_stamp(v):
    """Rubber stamp + gavel: a padded low thump, then a dry wooden crack with a short room tail."""
    rng = random.Random(6600)
    n = N(1.1)
    out = zeros(n)
    add_into(out, mul_env(sine_var(exp_ramp(N(0.35), 140, 55)), env_exp(N(0.35), 0.09, 0.001)), 0, 0.9)
    add_into(out, noise_burst(rng, 0.12, 900, 0.8, 0.025, 0.0008, "lp"), 0, 0.6)
    add_into(out, click(rng, 2400, 0.06, 0.006, 1.8, 0.9, 520), N(0.11), 0.9)
    add_into(out, resonators(noise_burst(rng, 0.05, 1800, 1.0, 0.004, 0.0005), [(520, 12.0, 0.6), (1130, 9.0, 0.3)]), N(0.11), 0.35)
    return fades(dc_block(reverb_mono(out, 0.5, 0.5, 0.18)), 0.0005, 0.25)


# ---------------- Boss questions and the cut vote ----------------

def boss_question(v):
    """Office intercom 'ding-dong' (two soft bells, a fourth apart) announcing the boss's question."""
    n = N(1.4)
    out = zeros(n)
    partials = [(1.0, 1.0, 1.0), (2.0, 0.25, 0.6), (3.0, 0.08, 0.4)]
    add_into(out, bell(midi_hz(76), 0.9, 0.7, partials), 0, 0.7)
    add_into(out, bell(midi_hz(72), 1.1, 0.8, partials), N(0.32), 0.7)
    l, r = reverb_stereo(out, 0.55, 0.45, combs=4)
    y = [d + (a + b) * 0.2 for d, a, b in zip(out, l, r)]
    return fades(dc_block(y), 0.0005, 0.2)


def boss_tick(v):
    """Dry countdown tick (wood block)."""
    rng = random.Random(6700)
    n = N(0.09)
    out = zeros(n)
    add_into(out, click(rng, 3000, 0.05, 0.004, 2.0, 0.8, 1250), 0, 1.0)
    return fades(dc_block(out), 0.0003, 0.02)


def boss_correct(v):
    """Bright rising arpeggio (C6 E6 G6 C7): the boss is satisfied."""
    n = N(1.1)
    out = zeros(n)
    for k, m in enumerate((84, 88, 91, 96)):
        add_into(out, bell(midi_hz(m), 0.6, 0.35, [(1.0, 1.0, 1.0), (2.0, 0.2, 0.4)]), N(k * 0.07), 0.55)
    l, r = reverb_stereo(out, 0.45, 0.4, combs=4)
    y = [d + (a + b) * 0.2 for d, a, b in zip(out, l, r)]
    return fades(dc_block(y), 0.0005, 0.15)


def boss_wrong(v):
    """Game-show buzzer: two detuned low saws, a short falling second note."""
    n = N(0.7)
    out = zeros(n)
    for k, (m, dur) in enumerate(((46, 0.22), (44, 0.34))):
        ln = N(dur)
        tone = mix(saw_blep([midi_hz(m)] * ln), saw_blep([midi_hz(m) * 1.012] * ln))
        tone = biquad(tone, "lp", 900, 0.8)
        tone = mul_env(tone, env_ahr(ln, 0.004, dur - 0.06, 0.05))
        add_into(out, tone, N(k * 0.24), 0.6)
    return fades(dc_block(out), 0.001, 0.04)


def vote_cast(v):
    """A folded ballot slipped into the box: paper swish, then a soft cardboard knock."""
    rng = random.Random(6800)
    n = N(0.45)
    out = zeros(n)
    swish = svf(white(N(0.16), rng), exp_ramp(N(0.16), 2600, 5200), 1.2, "bp")
    add_into(out, mul_env(swish, env_ahr(N(0.16), 0.03, 0.04, 0.09)), 0, 0.5)
    add_into(out, mul_env(sine_var(exp_ramp(N(0.12), 210, 120)), env_exp(N(0.12), 0.035, 0.001)), N(0.15), 0.8)
    add_into(out, noise_burst(rng, 0.05, 700, 0.9, 0.012, 0.0008, "lp"), N(0.15), 0.4)
    return fades(dc_block(out), 0.0005, 0.05)


# ---------------- UI ----------------

def ui_hover(v):
    n = N(0.05)
    tone = mul_env(sine(2350, n), env_exp(n, 0.008, 0.0008))
    return fades(tone, 0.0005, 0.01)


def ui_click(v):
    rng = random.Random(7100)
    n = N(0.1)
    out = zeros(n)
    add_into(out, mul_env(sine(1550, n), env_exp(n, 0.012, 0.0005)), 0, 0.6)
    add_into(out, mul_env(sine(320, n), env_exp(n, 0.015, 0.0005)), 0, 0.4)
    add_into(out, click(rng, 4000, 0.02, 0.0015, 1.5), 0, 0.4)
    return fades(dc_block(out), 0.0003, 0.02)


def ui_back(v):
    n = N(0.18)
    out = zeros(n)
    add_into(out, mul_env(sine(1480, N(0.06)), env_exp(N(0.06), 0.012, 0.0005)), 0, 0.6)
    add_into(out, mul_env(sine(1050, N(0.09)), env_exp(N(0.09), 0.018, 0.0005)), N(0.055), 0.6)
    return fades(out, 0.0003, 0.02)


def ui_toggle(v):
    rng = random.Random(7300)
    n = N(0.08)
    out = zeros(n)
    add_into(out, click(rng, 3200, 0.03, 0.002, 2.5, 0.6, 1100), 0, 1.0)
    add_into(out, click(rng, 2500, 0.03, 0.002, 2.5, 0.4, 900), N(0.022), 0.6)
    return fades(dc_block(out), 0.0003, 0.015)


def ui_slider_tick(v):
    n = N(0.03)
    return fades(mul_env(sine(3100, n), env_exp(n, 0.004, 0.0003)), 0.0002, 0.008)


def ui_error(v):
    n = N(0.4)
    out = zeros(n)
    for k, m in enumerate((64, 60)):
        ln = N(0.16)
        tone = saw_blep([midi_hz(m)] * ln)
        tone = biquad(tone, "lp", 1100, 0.7)
        tone = mul_env(tone, env_ahr(ln, 0.005, 0.09, 0.06))
        add_into(out, tone, N(k * 0.13), 0.7)
    return fades(dc_block(out), 0.001, 0.03)


def ui_whoosh(up):
    rng = random.Random(7500 + (1 if up else 0))
    n = N(0.42)
    sweep = exp_ramp(n, 450, 3200) if up else exp_ramp(n, 3000, 380)
    w = svf(white(n, rng), sweep, 1.1, "bp")
    shape = env_ahr(n, 0.16 if up else 0.05, 0.05, 0.2 if up else 0.3)
    return fades(dc_block(mul_env(w, shape)), 0.002, 0.03)


# ---------------- Ambience (all seamless loops) ----------------

def amb_hum(v):
    """Fluorescent ballast hum: 60 Hz family, slight flutter, faint buzz. 6 s loop."""
    T = 6.0
    n = N(T)
    parts = [(60, 0.22), (120, 1.0), (180, 0.38), (240, 0.45), (300, 0.12), (360, 0.2), (480, 0.1), (600, 0.05), (720, 0.04)]
    sig = zeros(n)
    for f, a in parts:
        add_into(sig, sine(f, n, phase=f * 0.37), 0, a)
    buzz = softclip(scale(sine(120, n), 4.0), 1.0)
    buzz = loop_process(buzz, lambda s: biquad(s, "bp", 1900, 1.2), 0.2)
    flutter1 = sine(cyc(7.5, T), n)
    flutter2 = sine(cyc(0.5, T), n)
    y = [(s + b * 0.12) * (1.0 + 0.035 * f1 + 0.05 * f2) for s, b, f1, f2 in zip(sig, buzz, flutter1, flutter2)]
    return y


def amb_room_tone(v):
    """HVAC / air-conditioning room tone (stereo bed). 20 s loop."""
    T = 20.0
    n = N(T)
    out = []
    for ch in range(2):
        rng = random.Random(8100 + ch)
        b = loop_noise("brown", n, rng, 2.0)
        b = loop_process(b, lambda s: biquad(s, "lp", 300, 0.7), 1.0)
        p = loop_noise("pink", n, rng, 2.0)
        p = loop_process(p, lambda s: biquad(biquad(s, "lp", 1600, 0.7), "hp", 120, 0.7), 1.0)
        swell = sine(cyc(0.1, T), n, phase=ch * 1.3)
        rumble = sine(cyc(29.0, T), n, amp=0.05)
        out.append([0.8 * x + 0.35 * y * (0.85 + 0.15 * s) + r for x, y, s, r in zip(b, p, swell, rumble)])
    return out[0], out[1]


def amb_vent(v):
    """Ceiling vent air (mono emitter). 12 s loop."""
    T = 12.0
    n = N(T)
    rng = random.Random(8200)
    p = loop_noise("pink", n, rng, 1.0)
    p = loop_process(p, lambda s: biquad(biquad(s, "bp", 650, 0.5), "lp", 2500, 0.7), 0.5)
    w = loop_noise("white", n, rng, 1.0)
    w = loop_process(w, lambda s: biquad(s, "bp", 1150, 6.0), 0.5)
    swell = sine(cyc(0.25, T), n)
    return [a + b * 0.12 * (0.7 + 0.3 * s) for a, b, s in zip(p, w, swell)]


def amb_office_murmur(v):
    """Distant open-plan bed: indistinct voices, typing bursts, mouse clicks. 30 s loop (mono)."""
    T = 30.0
    n = N(T)
    rng = random.Random(8300)
    vowels = [(730, 1090), (270, 2290), (530, 1840), (570, 840), (440, 1020), (300, 870), (660, 1720)]
    voices = zeros(n)
    for vi in range(4):
        # syllable envelope (circular), with phrase pauses
        env = [0.0] * n
        t = rng.uniform(0, 2)
        f1 = [0.0] * n
        f2 = [0.0] * n
        cur = rng.choice(vowels)
        while t < T + 1.0:
            phrase = rng.uniform(1.2, 3.5)
            end = t + phrase
            while t < end:
                syl = rng.uniform(0.12, 0.26)
                ln = N(syl)
                e = env_ahr(ln, syl * 0.3, syl * 0.3, syl * 0.4)
                vw = rng.choice(vowels)
                s0 = N(t)
                for i in range(ln):
                    k = (s0 + i) % n
                    env[k] += e[i]
                    f1[k] = vw[0]
                    f2[k] = vw[1]
                t += syl + rng.uniform(0.0, 0.06)
            t += rng.uniform(0.6, 3.0)
        # fill formant gaps with the previous value
        last1, last2 = 500.0, 1500.0
        for i in range(n):
            if f1[i] == 0.0:
                f1[i], f2[i] = last1, last2
            else:
                last1, last2 = f1[i], f2[i]
        f1 = lp1(lp1(f1, 18.0), 18.0)
        f2 = lp1(lp1(f2, 18.0), 18.0)
        base_f0 = rng.choice((105, 120, 185, 210))
        pitch_lfo = sine(cyc(0.3 + 0.1 * vi, T), n)
        f0 = [base_f0 * (1.0 + 0.08 * p) for p in pitch_lfo]
        src = saw_blep(f0)
        a = svf(src, f1, 4.0, "bp")
        b = svf(src, f2, 5.0, "bp")
        voice = [(x + 0.6 * y) * e for x, y, e in zip(a, b, env)]
        add_into(voices, voice, 0, 0.5 if vi < 2 else 0.3)
    voices = loop_process(voices, lambda s: biquad(biquad(s, "lp", 900, 0.7), "hp", 120, 0.7), 0.5)
    # keyboards
    keys = zeros(n)
    t = 0.3
    while t < T:
        burst = rng.uniform(1.0, 4.0)
        end = t + burst
        while t < end:
            add_into(keys, click(rng, rng.uniform(2200, 3600), 0.025, 0.003, 1.6, 0.5, rng.uniform(700, 1100)), N(t), rng.uniform(0.2, 0.45), wrap=True)
            t += rng.uniform(0.07, 0.2)
        if rng.random() < 0.5:
            add_into(keys, click(rng, 2800, 0.02, 0.002, 2.5, 0.2, 900), N(t + 0.2), 0.35, wrap=True)
            add_into(keys, click(rng, 2600, 0.02, 0.002, 2.5, 0.2, 900), N(t + 0.29), 0.25, wrap=True)
        t += rng.uniform(0.8, 4.0)
    keys = loop_process(keys, lambda s: biquad(s, "lp", 5000, 0.7), 0.2)
    bed = loop_noise("pink", n, rng, 1.0)
    bed = loop_process(bed, lambda s: biquad(s, "lp", 700, 0.7), 0.5)
    y = mix(scale(voices, 1.0), scale(keys, 0.5), scale(bed, 0.15))
    return loop_process(y, lambda s: reverb_mono(s, 0.55, 0.5, 0.35), 2.0)


def amb_fridge(v):
    """Kitchen fridge compressor hum. 8 s loop."""
    T = 8.0
    n = N(T)
    rng = random.Random(8400)
    sig = zeros(n)
    for f, a in ((60, 0.3), (120, 1.0), (240, 0.3), (360, 0.12), (480, 0.05)):
        add_into(sig, sine(f, n, phase=f * 0.11), 0, a)
    sig = softclip(scale(sig, 0.9), 1.0)
    am = sine(cyc(0.25, T), n)
    noise = loop_noise("brown", n, rng, 1.0)
    noise = loop_process(noise, lambda s: biquad(s, "lp", 400, 0.7), 0.5)
    rattle = loop_noise("white", n, rng, 1.0)
    rattle = loop_process(rattle, lambda s: biquad(s, "bp", 2400, 4.0), 0.3)
    rat_am = sine(cyc(120.0, T), n)
    return [s * (0.9 + 0.1 * a) + 0.25 * b + 0.02 * r * (0.5 + 0.5 * m) for s, a, b, r, m in zip(sig, am, noise, rattle, rat_am)]


def amb_vending(v):
    """Vending machine: compressor, ballast buzz, fan, faint whine. 8 s loop."""
    T = 8.0
    n = N(T)
    rng = random.Random(8500)
    sig = zeros(n)
    for f, a in ((120, 1.0), (240, 0.45), (360, 0.3), (480, 0.15), (720, 0.08)):
        add_into(sig, sine(f, n, phase=f * 0.23), 0, a)
    buzz = softclip(scale(sine(120, n), 5.0), 1.0)
    buzz = loop_process(buzz, lambda s: biquad(s, "bp", 2600, 1.5), 0.2)
    fan = loop_noise("pink", n, rng, 1.0)
    fan = loop_process(fan, lambda s: biquad(s, "bp", 800, 0.8), 0.5)
    whine = sine(3100.0, n, amp=0.03)
    am = sine(cyc(0.5, T), n)
    return [s * (0.92 + 0.08 * a) + 0.18 * b + 0.5 * f + w for s, a, b, f, w in zip(sig, am, buzz, fan, whine)]


def amb_clock_tick(v):
    """Wall clock tick-tock, 2 s loop (ticks kept away from the seam)."""
    rng = random.Random(8600)
    n = N(2.0)
    out = zeros(n)
    for k, (t, f) in enumerate(((0.25, 3600), (1.25, 2900))):
        exc = click(rng, f, 0.012, 0.0012, 2.0)
        ring = resonators(exc + zeros(N(0.08)), [(f, 30, 0.8), (f * 0.47, 20, 0.5), (f * 1.9, 30, 0.2)])
        ring = mul_env(ring, env_exp(len(ring), 0.012, 0.0002))
        add_into(out, ring, N(t), 1.0)
        add_into(out, mul_env(sine(900 if k == 0 else 760, N(0.04)), env_exp(N(0.04), 0.006, 0.0002)), N(t), 0.3)
    return out


def amb_exterior(v):
    """Faint traffic and birds heard through the windows. 30 s loop (mono)."""
    T = 30.0
    n = N(T)
    rng = random.Random(8700)
    traffic = loop_noise("brown", n, rng, 2.0)
    traffic = loop_process(traffic, lambda s: biquad(s, "lp", 380, 0.7), 1.0)
    cars = zeros(n)
    for k in range(4):
        cn = N(rng.uniform(4.0, 6.0))
        hiss = biquad(white(cn, rng), "bp", 1100, 0.6)
        hiss = svf(hiss, exp_ramp(cn, 1500, 500), 0.8, "lp")
        shape = [math.exp(-((i / cn - 0.5) ** 2) / 0.03) for i in range(cn)]
        add_into(cars, mul_env(hiss, shape), N(k * T / 4 + rng.uniform(0, 3)), rng.uniform(0.25, 0.45), wrap=True)
    birds = zeros(n)
    t = 1.0
    while t < T - 0.5:
        reps = rng.randint(2, 5)
        f0 = rng.uniform(2800, 4200)
        span = rng.uniform(800, 1800)
        for r in range(reps):
            ln = N(rng.uniform(0.05, 0.09))
            f = sine_var(exp_ramp(ln, f0, f0 + span) if r % 2 == 0 else exp_ramp(ln, f0 + span, f0))
            chirp = mul_env(f, env_ahr(ln, 0.01, ln / SR * 0.4, ln / SR * 0.4))
            add_into(birds, chirp, N(t), rng.uniform(0.05, 0.1), wrap=True)
            t += ln / SR + rng.uniform(0.04, 0.1)
        t += rng.uniform(1.5, 5.0)
    birds = loop_process(birds, lambda s: biquad(s, "lp", 5500, 0.7), 0.2)
    return mix(scale(traffic, 0.7), cars, birds)


def amb_overtime_drone(v):
    """Low unsettling pad with slow beating (stereo). 24 s loop, every component periodic."""
    T = 24.0
    n = N(T)
    out = []
    for ch in range(2):
        rng = random.Random(8800 + ch)
        sig = zeros(n)
        d = 0.42 if ch == 0 else -0.33
        for f, a in ((55.0, 0.8), (55.0 + d, 0.7), (82.4, 0.35), (87.3 + d * 0.5, 0.25), (110.0 - d, 0.2), (155.6, 0.08)):
            add_into(sig, sine(cyc(f, T), n, phase=rng.uniform(0, 6.28)), 0, a)
        swell = sine(cyc(1.0 / 12.0, T), n, phase=ch * 1.7)
        shimmer = mix(sine(cyc(659.25, T), n, amp=0.5), sine(cyc(698.5 + d, T), n, amp=0.5))
        sh_am = sine(cyc(1.0 / 8.0, T), n, phase=ch)
        air = loop_noise("pink", n, rng, 2.0)
        air = loop_process(air, lambda s: biquad(biquad(s, "bp", 420, 0.9), "lp", 900, 0.7), 1.0)
        sig = softclip(scale(sig, 0.5), 1.0)
        out.append([s * (0.75 + 0.25 * w) + 0.025 * h * max(0.0, m) + 0.35 * a
                    for s, w, h, m, a in zip(sig, swell, shimmer, sh_am, air)])
    return out[0], out[1]


def amb_creak(v):
    """Overtime building noises: 1-2 = wood/metal creaks (stick-slip), 3-4 = distant knocks."""
    rng = random.Random(8900 + v)
    if v <= 2:
        dur = 1.6 + 0.6 * v
        n = N(dur + 0.8)
        exc = zeros(n)
        t = 0.05
        rate0, rate1 = (28, 70) if v == 1 else (60, 22)
        while t < dur:
            rate = rate0 + (rate1 - rate0) * (t / dur)
            add_into(exc, [1.0, -0.8], N(t), rng.uniform(0.4, 1.0) * (math.sin(math.pi * t / dur) ** 0.7))
            t += 1.0 / (rate * rng.uniform(0.8, 1.25))
        modes = [(240, 12, 1.0), (610, 14, 0.7), (1330, 18, 0.45), (2750, 22, 0.2)] if v == 1 else \
                [(180, 10, 1.0), (470, 12, 0.6), (1120, 20, 0.5), (3300, 30, 0.15)]
        y = resonators(exc, modes)
    else:
        n = N(2.2)
        y = zeros(n)
        times = (0.0, 0.32, 0.61) if v == 3 else (0.0, 0.5)
        for t in times:
            k = noise_burst(rng, 0.3, 160 + rng.uniform(-20, 20), 0.8, 0.05, 0.001, "lp")
            res = resonators(click(rng, 800, 0.02, 0.002, 1.0) + zeros(N(0.25)), [(130, 10, 1.0), (390, 12, 0.5)])
            add_into(y, k, N(t), 1.0)
            add_into(y, mul_env(res, env_exp(len(res), 0.07)), N(t), 0.8)
    y = biquad(y, "lp", 2600, 0.7)
    l, r = reverb_stereo(y, 0.8, 0.45, combs=6)
    y = [d + (a + b) * 0.35 for d, a, b in zip(y, l, r)]
    return fades(dc_block(y), 0.003, 0.4)


# ---------------- Music ----------------

CHORDS = {
    "Cmaj7": (36, [60, 64, 67, 71]), "Am7": (45, [60, 64, 67, 69]), "Dm7": (38, [60, 62, 65, 69]),
    "G7": (43, [59, 62, 65, 67]), "Fmaj7": (41, [60, 64, 65, 69]), "Em7": (40, [59, 62, 64, 67]),
    "G7sus": (43, [60, 62, 65, 67]),
}
# Darker re-harmonisation of the same progression (same roots/rhythm)
DARK_CHORDS = {
    "Cmaj7": (36, [60, 63, 67, 70]), "Am7": (44, [60, 63, 67, 68]), "Dm7": (38, [60, 62, 65, 68]),
    "G7": (43, [59, 62, 65, 68]), "Fmaj7": (41, [60, 63, 65, 68]), "Em7": (39, [58, 62, 63, 67]),
    "G7sus": (43, [60, 62, 65, 68]),
}
PROGRESSION = ["Cmaj7", "Am7", "Dm7", "G7", "Cmaj7", "Am7", "Dm7", "G7",
               "Fmaj7", "Em7", "Dm7", "G7", "Fmaj7", "Em7", "Dm7", ("G7sus", "G7")]
# (beat, length, midi) per bar
MELODY = [
    [(0, 1.5, 76), (1.5, 0.5, 79), (2, 1, 81), (3, 1, 79)],
    [(0, 2, 76), (2, 1, 74), (3, 1, 72)],
    [(0, 1.5, 74), (1.5, 0.5, 76), (2, 1, 77), (3, 1, 81)],
    [(0, 3, 79)],
    [(0, 1.5, 76), (1.5, 0.5, 79), (2, 1, 84), (3, 1, 83)],
    [(0, 2, 81), (2, 1, 79), (3, 1, 76)],
    [(0, 1, 77), (1, 1, 76), (2, 1, 74), (3, 0.5, 72), (3.5, 0.5, 74)],
    [(0, 3, 74)],
    [(0, 1.5, 81), (1.5, 0.5, 84), (2, 1, 81), (3, 1, 79)],
    [(0, 1.5, 79), (1.5, 0.5, 76), (2, 1, 74), (3, 1, 76)],
    [(0, 1, 77), (1, 1, 81), (2, 1.5, 84), (3.5, 0.5, 81)],
    [(0, 2, 83), (2, 1, 79), (3, 1, 77)],
    [(0, 1.5, 76), (1.5, 0.5, 77), (2, 2, 81)],
    [(0, 1.5, 79), (1.5, 0.5, 76), (2, 2, 71)],
    [(0, 1, 74), (1, 1, 77), (2, 1, 81), (3, 1, 79)],
    [(0, 2, 77), (2, 1, 74), (3, 1, 71)],
]


def to_minor(m):
    """C major melody note -> C natural minor (E->Eb, A->Ab, B->Bb)."""
    pc = m % 12
    return m - 1 if pc in (4, 9, 11) else m


def render_music(dark):
    bpm = 86.0 if dark else 100.0
    beat = 60.0 / bpm
    bars = len(PROGRESSION)
    T = bars * 4 * beat
    n = N(T)
    rng = random.Random(9000 + (1 if dark else 0))
    chords = DARK_CHORDS if dark else CHORDS
    ep = zeros(n)
    bass = zeros(n)
    lead = zeros(n)
    perc = zeros(n)

    comp = [(0.0, 1.4, 0.85), (1.5, 0.45, 0.55), (3.0, 0.9, 0.65)]
    for bar, entry in enumerate(PROGRESSION):
        halves = entry if isinstance(entry, tuple) else (entry, entry)
        bar_t = bar * 4 * beat
        # electric piano comping
        for beat_pos, length, vel in comp:
            name = halves[0] if beat_pos < 2 else halves[1]
            root, notes = chords[name]
            for j, m in enumerate(notes):
                det = 0.0
                if dark:
                    det = (-14.0 if j % 2 == 0 else 13.0)
                note = fm_note(midi_hz(m), length * beat, index=1.8 if not dark else 1.2, ratio=1.0, index_decay=0.25,
                               amp_decay=1.4, release=0.12, attack=0.003, detune_cents=det, tail=0.4)
                start = N(bar_t + beat_pos * beat + j * 0.006 + rng.uniform(0, 0.004))
                add_into(ep, note, start, vel * 0.16, wrap=True)
                if dark:
                    note2 = fm_note(midi_hz(m), length * beat, index=0.8, ratio=1.0, index_decay=0.4, amp_decay=1.6,
                                    release=0.15, attack=0.01, detune_cents=-det * 0.6, tail=0.4)
                    add_into(ep, note2, start, vel * 0.1, wrap=True)
        # bass: root, fifth, root, chromatic approach to next root
        nxt = PROGRESSION[(bar + 1) % bars]
        nxt_root = chords[nxt[0] if isinstance(nxt, tuple) else nxt][0]
        r1 = chords[halves[0]][0]
        r2 = chords[halves[1]][0]
        line = [r1, r1 + 7, r2, nxt_root - 1 if (bar % 2 == 1) else r2 + 7]
        for k, m in enumerate(line):
            f = midi_hz(m)
            ln = beat * (0.9 if k < 3 else 0.6)
            bn = N(ln + 0.15)
            ph = sine(f, bn)
            h2 = sine(2 * f, bn)
            body = [a + 0.28 * b for a, b in zip(ph, h2)]
            env = env_ahr(bn, 0.006, ln * 0.6, ln * 0.4 + 0.1)
            dec = env_exp(bn, 0.45, 0.006)
            body = [b * e * (0.55 + 0.45 * d) for b, e, d in zip(body, env, dec)]
            add_into(bass, body, N(bar_t + k * beat), 0.42, wrap=True)
        # lead melody
        for beat_pos, length, m in MELODY[bar]:
            if dark:
                m = to_minor(m) - 12
            note = fm_note(midi_hz(m), length * beat * 0.95, index=0.9 if not dark else 0.5, ratio=2.0, index_decay=0.6,
                           amp_decay=3.5, release=0.18, attack=0.035, vib_hz=5.2, vib_cents=14, tail=0.4)
            add_into(lead, note, N(bar_t + beat_pos * beat), 0.17 if not dark else 0.14, wrap=True)
        # percussion
        for k in range(4):
            t = bar_t + k * beat
            if k in (0, 2):
                kn = N(0.3)
                kick = mul_env(sine_var(exp_ramp(kn, 115, 48)), env_exp(kn, 0.09, 0.002))
                add_into(perc, kick, N(t), 0.35 if not dark else 0.28, wrap=True)
            if not dark:
                if k in (1, 3):
                    rim = mix(noise_burst(rng, 0.06, 1900, 4.0, 0.012, 0.0005), mul_env(sine(530, N(0.06)), env_exp(N(0.06), 0.01, 0.0005)))
                    add_into(perc, rim, N(t), 0.16, wrap=True)
                for s in range(4):
                    sh = noise_burst(rng, 0.05, 7000, 0.8, 0.015, 0.004, "hp")
                    add_into(perc, sh, N(t + s * beat / 4), 0.05 if s % 2 == 0 else 0.03, wrap=True)
    log("music %s: %.2f s, %d bars at %d bpm" % ("dark" if dark else "menu", T, bars, bpm))

    # stereo placement and room
    ep_l = [v * 0.62 for v in ep]
    ep_r = [v * 0.42 for v in ep]
    lead_l = [v * 0.42 for v in lead]
    lead_r = [v * 0.62 for v in lead]
    send = [a * 0.8 + b * 0.9 for a, b in zip(ep, lead)]
    size = 0.8 if dark else 0.55
    wl, wr = loop_process(send, lambda s: _pair(reverb_stereo(s, size, 0.45, combs=4)), 3.0)
    wet = 0.55 if dark else 0.3
    L = [a + b + c + d + e * wet for a, b, c, d, e in zip(ep_l, lead_l, bass, perc, wl)]
    R = [a + b + c + d * 0.9 + e * wet for a, b, c, d, e in zip(ep_r, lead_r, bass, perc, wr)]
    if dark:
        # tape-like wow, gloomier tone, low drone underneath
        wow_f = cyc(0.27, T)
        L = loop_process(L, lambda s: wow(s, wow_f, 0.0035), 1.0)
        R = loop_process(R, lambda s: wow(s, wow_f, 0.0035, 0.6), 1.0)
        L = loop_process(L, lambda s: biquad(biquad(s, "lp", 1900, 0.7), "peak", 180, 0.8, 2.0), 1.0)
        R = loop_process(R, lambda s: biquad(biquad(s, "lp", 1900, 0.7), "peak", 180, 0.8, 2.0), 1.0)
        drone = mix(sine(cyc(65.41, T), n, amp=0.05), sine(cyc(65.9, T), n, amp=0.045))
        L = [a + d for a, d in zip(L, drone)]
        R = [a + d for a, d in zip(R, drone)]
    else:
        L = loop_process(L, lambda s: biquad(s, "lp", 9000, 0.7), 0.5)
        R = loop_process(R, lambda s: biquad(s, "lp", 9000, 0.7), 0.5)
    L = loop_process(L, dc_block, 1.0)
    R = loop_process(R, dc_block, 1.0)
    return L, R


class _pair(list):
    """Lets loop_process slice a (left, right) result: slicing applies to both channels."""

    def __init__(self, lr):
        super().__init__([lr[0], lr[1]])

    def __getitem__(self, item):
        if isinstance(item, slice):
            return (list.__getitem__(self, 0)[item], list.__getitem__(self, 1)[item])
        return list.__getitem__(self, item)


def wow(x, freq, depth_s, phase=0.0):
    """Modulated delay (tape wow). Reads circularly so it stays loop-safe after loop_process."""
    n = len(x)
    base = depth_s * SR * 1.5
    d = depth_s * SR
    out = [0.0] * n
    w = TWO_PI * freq / SR
    sin = math.sin
    for i in range(n):
        pos = i - (base + d * sin(w * i + phase))
        i0 = int(math.floor(pos))
        frac = pos - i0
        a = x[i0 % n]
        b = x[(i0 + 1) % n]
        out[i] = a + (b - a) * frac
    return out


def music_menu(v):
    return render_music(False)


def music_dark(v):
    return render_music(True)


# ---------------- The killer and the investigation (M4) ----------------

def role_employee(v):
    """Role card, employee: a warm corporate major-seventh chime with a soft pad under it."""
    n = N(2.6)
    out = zeros(n)
    partials = [(1.0, 1.0, 1.0), (2.0, 0.3, 0.6), (3.0, 0.1, 0.4)]
    for k, m in enumerate((60, 64, 67, 71)):
        add_into(out, bell(midi_hz(m + 12), 1.8, 1.0, partials), N(k * 0.09), 0.35)
    pad = mix(*[sine(midi_hz(m), N(2.2), amp=0.25) for m in (48, 55, 64)])
    pad = mul_env(pad, env_ahr(N(2.2), 0.25, 0.6, 1.2))
    add_into(out, pad, 0, 0.25)
    l, r = reverb_stereo(out, 0.7, 0.4, combs=6)
    lo = [d + a * 0.45 for d, a in zip(out, l)]
    ro = [d + b * 0.45 for d, b in zip(out, r)]
    return fades(dc_block(lo), 0.002, 0.5), fades(dc_block(ro), 0.002, 0.5)


def role_killer(v):
    """Role card, saboteur: two heartbeat thumps, then a low dissonant stab with a detuned high scrape."""
    rng = random.Random(9100)
    n = N(3.2)
    out = zeros(n)
    for t in (0.0, 0.32):
        add_into(out, mul_env(sine_var(exp_ramp(N(0.25), 90, 42)), env_exp(N(0.25), 0.07, 0.002)), N(t), 0.9)
    hit = N(0.75)
    for m in (38, 39, 45, 50):
        add_into(out, brass(midi_hz(m), 2.0, rng, 0.015), hit, 0.3)
    add_into(out, mul_env(sine_var(exp_ramp(N(1.6), 55, 30)), env_exp(N(1.6), 0.45, 0.003)), hit, 0.8)
    sc = svf(white(N(1.6), rng), exp_ramp(N(1.6), 3200, 5200), 6.0, "bp")
    sc = mul_env(sc, env_ahr(N(1.6), 0.05, 0.3, 1.1))
    add_into(out, sc, hit, 0.12)
    l, r = reverb_stereo(out, 0.85, 0.35, combs=6)
    lo = [d + a * 0.5 for d, a in zip(out, l)]
    ro = [d + b * 0.5 for d, b in zip(out, r)]
    return fades(dc_block(lo), 0.002, 0.6), fades(dc_block(ro), 0.002, 0.6)


def sabotage_printer_jam(v):
    """Paper jam: a grinding crunch of paper in the rollers, a motor stalling, an error double-beep."""
    rng = random.Random(9200)
    n = N(1.5)
    out = zeros(n)
    crunch_n = N(0.55)
    crunch = biquad(white(crunch_n, rng), "bp", 2400, 0.8)
    am = [0.0] * crunch_n
    t = 0.0
    while t < 0.5:
        k = N(t)
        for j in range(N(0.012)):
            if k + j < crunch_n:
                am[k + j] = max(am[k + j], rng.uniform(0.4, 1.0) * (1.0 - j / N(0.012)))
        t += rng.uniform(0.008, 0.03)
    add_into(out, [c * a for c, a in zip(crunch, am)], 0, 0.9)
    stall = softclip(sine_var(exp_ramp(N(0.7), 140, 40)), 2.5)
    stall = mul_env(biquad(stall, "lp", 700, 0.7), env_ahr(N(0.7), 0.01, 0.3, 0.35))
    add_into(out, stall, N(0.05), 0.5)
    add_into(out, metal_clunk(rng, 0.4, 80, 0.5), N(0.55), 0.6)
    for k in range(2):
        beep = mul_env(sine(1900, N(0.09)), env_ahr(N(0.09), 0.003, 0.06, 0.02))
        add_into(out, beep, N(0.85 + k * 0.16), 0.25)
    return fades(dc_block(reverb_mono(out, 0.3, 0.5, 0.12)), 0.001, 0.08)


def sabotage_power_cut(v):
    """Camera and lights cut: a breaker snap, a short falling hum, the fluorescent tubes ticking off."""
    rng = random.Random(9300)
    n = N(1.8)
    out = zeros(n)
    add_into(out, click(rng, 2600, 0.05, 0.004, 1.5, 1.0, 380), 0, 0.9)
    add_into(out, metal_clunk(rng, 0.35, 75, 0.5), 0, 0.5)
    fall = N(1.1)
    hum = softclip(sine_var(exp_ramp(fall, 120, 30)), 2.0)
    hum = mul_env(biquad(hum, "lp", 800, 0.7), env_ahr(fall, 0.005, 0.2, 0.85))
    add_into(out, hum, N(0.02), 0.45)
    for t in (0.18, 0.31, 0.52, 0.66):
        add_into(out, click(rng, 5200, 0.015, 0.0012, 3.0), N(t), 0.25)
    return fades(dc_block(reverb_mono(out, 0.5, 0.45, 0.18)), 0.0005, 0.2)


def sabotage_lock(v):
    """Maintenance lock: a heavy bolt sliding home and a short denial chirp."""
    rng = random.Random(9400)
    n = N(1.0)
    out = zeros(n)
    add_into(out, drawer_slide(rng, 0.16, 1900), 0, 0.5)
    add_into(out, metal_clunk(rng, 0.6, 70, 1.4), N(0.15), 1.0)
    add_into(out, click(rng, 3000, 0.03, 0.002, 2.0, 0.6, 700), N(0.2), 0.4)
    chirp = mul_env(sine_var(exp_ramp(N(0.12), 1400, 900)), env_ahr(N(0.12), 0.003, 0.07, 0.04))
    add_into(out, chirp, N(0.42), 0.2)
    return fades(dc_block(reverb_mono(out, 0.3, 0.5, 0.12)), 0.001, 0.08)


def sabotage_file_corrupt(v):
    """A file overwritten: fast keyboard clatter, then a glitchy, bit-crushed descending blip."""
    rng = random.Random(9500)
    n = N(1.3)
    out = zeros(n)
    t = 0.0
    while t < 0.55:
        add_into(out, click(rng, rng.uniform(2500, 3800), 0.02, 0.0015, 2.0, 0.3, 500), N(t), rng.uniform(0.25, 0.45))
        t += rng.uniform(0.045, 0.09)
    gl = N(0.5)
    tone = saw_blep(exp_ramp(gl, 1500, 220))
    crushed = [round(x * 4.0) / 4.0 for x in tone]
    held = [crushed[i - (i % 6)] for i in range(gl)]
    held = mul_env(biquad(held, "lp", 3200, 0.7), env_ahr(gl, 0.005, 0.25, 0.2))
    add_into(out, held, N(0.62), 0.22)
    return fades(dc_block(out), 0.001, 0.06)


def sabotage_prop_move(v):
    """A pot dragged across the floor: a low scrape and a dull set-down."""
    rng = random.Random(9600)
    n = N(1.0)
    out = zeros(n)
    add_into(out, drawer_slide(rng, 0.55, 700), 0, 0.8)
    add_into(out, noise_burst(rng, 0.2, 260, 0.8, 0.05, 0.001, "lp"), N(0.56), 0.8)
    add_into(out, mul_env(sine(95, N(0.25)), env_exp(N(0.25), 0.06, 0.001)), N(0.56), 0.5)
    return fades(dc_block(reverb_mono(out, 0.3, 0.5, 0.1)), 0.002, 0.08)


def terminal_open(v):
    """An old terminal waking up: a CRT thunk, a rising bleep pair."""
    rng = random.Random(9700)
    n = N(0.5)
    out = zeros(n)
    add_into(out, click(rng, 1800, 0.04, 0.004, 1.2, 0.7, 240), 0, 0.5)
    whine = mul_env(sine(15700 * 0 + 7400, N(0.3)), env_ahr(N(0.3), 0.01, 0.15, 0.12))
    add_into(out, whine, 0, 0.03)
    for k, f in enumerate((1320, 1980)):
        add_into(out, mul_env(sine(f, N(0.07)), env_ahr(N(0.07), 0.003, 0.04, 0.02)), N(0.08 + k * 0.08), 0.3)
    return fades(dc_block(out), 0.0005, 0.04)


def terminal_beep(v):
    """Pin/unpin: a short square-ish confirmation beep."""
    n = N(0.12)
    tone = saw_blep([1760.0] * n)
    tone = biquad(tone, "lp", 3000, 0.7)
    return fades(mul_env(tone, env_ahr(n, 0.002, 0.06, 0.04)), 0.0005, 0.02)


def meeting_gavel(v):
    """Verdict: two gavel strikes on a wooden block."""
    rng = random.Random(9800)
    n = N(1.2)
    out = zeros(n)
    for k, t in enumerate((0.0, 0.32)):
        add_into(out, click(rng, 2200, 0.06, 0.006, 1.8, 1.0, 480), N(t), 0.9 if k else 0.7)
        add_into(out, resonators(noise_burst(rng, 0.05, 1700, 1.0, 0.004, 0.0005), [(480, 12.0, 0.7), (1060, 9.0, 0.35)]), N(t), 0.4)
        add_into(out, mul_env(sine_var(exp_ramp(N(0.2), 160, 70)), env_exp(N(0.2), 0.05, 0.001)), N(t), 0.5)
    return fades(dc_block(reverb_mono(out, 0.55, 0.45, 0.2)), 0.0005, 0.25)


def meeting_bell(v):
    """Meeting called: an urgent intercom tri-tone, twice."""
    n = N(2.2)
    out = zeros(n)
    partials = [(1.0, 1.0, 1.0), (2.0, 0.2, 0.5)]
    for rep in range(2):
        for k, m in enumerate((79, 74, 79)):
            add_into(out, bell(midi_hz(m), 0.5, 0.4, partials), N(rep * 0.75 + k * 0.17), 0.55)
    y = pa_filter(out)
    l, r = reverb_stereo(y, 0.6, 0.4, combs=4)
    lo = [d + a * 0.3 for d, a in zip(y, l)]
    ro = [d + b * 0.3 for d, b in zip(y, r)]
    return fades(dc_block(lo), 0.0005, 0.3), fades(dc_block(ro), 0.0005, 0.3)


def kill_sting(v):
    """A dismissal (stylized, not gory): a paper-slap stamp, a sub drop and a long string-like dissonance."""
    rng = random.Random(9900)
    n = N(3.0)
    out = zeros(n)
    add_into(out, noise_burst(rng, 0.08, 1500, 0.8, 0.02, 0.0005, "lp"), 0, 0.8)
    add_into(out, mul_env(sine_var(exp_ramp(N(1.4), 70, 28)), env_exp(N(1.4), 0.4, 0.002)), 0, 1.0)
    for m, det in ((62, 0.0), (63, 1.5), (68, -2.0)):
        tone = saw_blep([midi_hz(m) + det] * N(2.4))
        tone = mul_env(biquad(tone, "lp", 2200, 0.7), env_ahr(N(2.4), 0.25, 0.8, 1.3))
        add_into(out, tone, N(0.1), 0.12)
    l, r = reverb_stereo(out, 0.8, 0.35, combs=6)
    lo = [d + a * 0.45 for d, a in zip(out, l)]
    ro = [d + b * 0.45 for d, b in zip(out, r)]
    return fades(dc_block(lo), 0.0005, 0.6), fades(dc_block(ro), 0.0005, 0.6)


# ---------------- The meeting (M4.1) ----------------

def meeting_vote_tick(v):
    """A live ballot placed or moved: a small plastic chip snapping onto a board, with a soft pitched blip."""
    rng = random.Random(10100)
    n = N(0.22)
    out = zeros(n)
    add_into(out, click(rng, 3400, 0.03, 0.002, 2.5, 0.5, 900), 0, 0.7)
    blip = mul_env(sine_var(exp_ramp(N(0.08), 1250, 1500)), env_ahr(N(0.08), 0.002, 0.02, 0.05))
    add_into(out, blip, N(0.012), 0.25)
    return fades(dc_block(out), 0.0003, 0.04)


def meeting_timer_tick(v):
    """The last seconds of a meeting countdown: a tense, slightly hollow double tick (wood block + low pulse)."""
    rng = random.Random(10200)
    n = N(0.32)
    out = zeros(n)
    add_into(out, click(rng, 2100, 0.05, 0.004, 2.2, 0.9, 720), 0, 0.8)
    add_into(out, click(rng, 2600, 0.04, 0.003, 2.2, 0.6, 900), N(0.09), 0.45)
    add_into(out, mul_env(sine(82, N(0.25)), env_exp(N(0.25), 0.08, 0.002)), 0, 0.35)
    return fades(dc_block(out), 0.0003, 0.05)


def meeting_rps_reveal(v):
    """Rock-paper-scissors reveal: a quick rising whoosh, a hand slap on the table, a bright two-note sting."""
    rng = random.Random(10300)
    n = N(0.9)
    out = zeros(n)
    whoosh = svf(white(N(0.22), rng), exp_ramp(N(0.22), 700, 4200), 1.4, "bp")
    add_into(out, mul_env(whoosh, env_ahr(N(0.22), 0.12, 0.04, 0.06)), 0, 0.45)
    add_into(out, noise_burst(rng, 0.08, 1300, 0.9, 0.02, 0.0005, "lp"), N(0.22), 0.8)
    add_into(out, mul_env(sine_var(exp_ramp(N(0.18), 170, 90)), env_exp(N(0.18), 0.05, 0.001)), N(0.22), 0.6)
    for k, m in enumerate((79, 86)):
        add_into(out, bell(midi_hz(m), 0.45, 0.3, [(1.0, 1.0, 1.0), (2.0, 0.25, 0.4)]), N(0.25 + k * 0.08), 0.35)
    return fades(dc_block(reverb_mono(out, 0.4, 0.45, 0.15)), 0.0005, 0.12)


def meeting_probation(v):
    """ON PROBATION: a heavy rubber stamp on paper, a short rustle, and a dry two-note 'uh-oh' on muted brass."""
    rng = random.Random(10400)
    n = N(1.4)
    out = zeros(n)
    add_into(out, mul_env(sine_var(exp_ramp(N(0.3), 120, 50)), env_exp(N(0.3), 0.08, 0.001)), 0, 0.9)
    add_into(out, noise_burst(rng, 0.1, 800, 0.8, 0.022, 0.0006, "lp"), 0, 0.7)
    rustle = biquad(white(N(0.25), rng), "bp", 3600, 0.8)
    add_into(out, mul_env(rustle, env_ahr(N(0.25), 0.02, 0.05, 0.18)), N(0.1), 0.12)
    for k, (m, dur) in enumerate(((60, 0.22), (55, 0.4))):
        note = brass(midi_hz(m), dur, rng, 0.02)
        add_into(out, biquad(note, "lp", 1400, 0.7), N(0.38 + k * 0.24), 0.32)
    return fades(dc_block(reverb_mono(out, 0.45, 0.5, 0.16)), 0.0005, 0.2)


def meeting_forgery(v):
    """The shield was a forgery: cheap foil crumpling, a paper tear, then a descending 'wah-wah' cartoon fall."""
    rng = random.Random(10500)
    n = N(1.6)
    out = zeros(n)
    t = 0.0
    while t < 0.3:
        add_into(out, click(rng, rng.uniform(3000, 6000), 0.015, 0.0015, 1.5, 0.0, 400), N(t), rng.uniform(0.2, 0.45))
        t += rng.uniform(0.012, 0.035)
    tear = svf(white(N(0.28), rng), exp_ramp(N(0.28), 1800, 5200), 1.0, "bp")
    add_into(out, mul_env(tear, env_ahr(N(0.28), 0.01, 0.16, 0.1)), N(0.3), 0.5)
    for k, m in enumerate((67, 66, 65, 63)):
        dur = 0.18 if k < 3 else 0.42
        ln = N(dur)
        tone = mix(saw_blep([midi_hz(m)] * ln), saw_blep([midi_hz(m) * 1.008] * ln))
        tone = biquad(tone, "lp", 1100, 1.2)
        tone = mul_env(tone, env_ahr(ln, 0.01, dur - 0.08, 0.07))
        add_into(out, tone, N(0.62 + k * 0.19), 0.22)
    return fades(dc_block(reverb_mono(out, 0.4, 0.5, 0.14)), 0.0005, 0.15)


# ==============================================================================================
# Registry
# ==============================================================================================
# id, variants, generator, loop, category, peak dBFS target
# Categories map to sound classes in uo_audio.py: music, sfx, ambience, ui.
SOUNDS = [
    # Player
    ("Footstep.Carpet", 6, footstep_carpet, False, "sfx", -3.0),
    ("Footstep.Tile", 6, footstep_tile, False, "sfx", -3.0),
    ("Player.Jump", 2, player_jump, False, "sfx", -3.0),
    ("Player.Land", 2, player_land, False, "sfx", -3.0),
    # Interactions
    ("Interact.Start", 1, interact_start, False, "sfx", -3.0),
    ("Interact.HoldLoop", 1, interact_hold_loop, True, "sfx", -6.0),
    ("Interact.Complete", 1, interact_complete, False, "sfx", -3.0),
    ("Interact.Denied", 1, interact_denied, False, "sfx", -3.0),
    ("Printer.Cartridge", 1, printer_cartridge, False, "sfx", -3.0),
    ("Printer.WhirLoop", 1, printer_whir_loop, True, "sfx", -6.0),
    ("Printer.Feed", 1, printer_feed, False, "sfx", -3.0),
    ("Coffee.GurgleLoop", 1, coffee_gurgle_loop, True, "sfx", -6.0),
    ("Coffee.Beep", 1, coffee_beep, False, "sfx", -3.0),
    ("Drawer.Open", 1, drawer_open, False, "sfx", -3.0),
    ("Drawer.Close", 2, drawer_close, False, "sfx", -3.0),
    ("Paper.Rustle", 3, paper_rustle, False, "sfx", -3.0),
    ("Supply.Rummage", 2, supply_rummage, False, "sfx", -3.0),
    ("Item.Pickup", 1, item_pickup, False, "sfx", -3.0),
    ("Item.Drop", 1, item_drop, False, "sfx", -3.0),
    ("Task.Chime", 1, task_chime, False, "sfx", -3.0),
    ("Task.AllDone", 1, task_all_done, False, "sfx", -3.0),
    # Day flow
    ("Day.ShiftBell", 1, day_shift_bell, False, "sfx", -3.0),
    ("Day.OvertimeStinger", 1, day_overtime_stinger, False, "sfx", -3.0),
    ("Day.PowerDown", 1, day_power_down, False, "sfx", -3.0),
    ("Day.ClosingBell", 1, day_closing_bell, False, "sfx", -3.0),
    ("Eval.Star1", 1, lambda v: eval_star(1), False, "sfx", -3.0),
    ("Eval.Star2", 1, lambda v: eval_star(2), False, "sfx", -3.0),
    ("Eval.Star3", 1, lambda v: eval_star(3), False, "sfx", -3.0),
    ("Eval.Fired", 1, eval_fired, False, "sfx", -3.0),
    ("Eval.Stamp", 1, eval_stamp, False, "sfx", -3.0),
    # Boss questions and the cut vote
    ("Boss.Question", 1, boss_question, False, "ui", -3.0),
    ("Boss.Tick", 1, boss_tick, False, "ui", -6.0),
    ("Boss.Correct", 1, boss_correct, False, "ui", -3.0),
    ("Boss.Wrong", 1, boss_wrong, False, "ui", -4.0),
    ("Vote.Cast", 1, vote_cast, False, "ui", -4.0),
    # The killer and the investigation (M4)
    ("Role.Employee", 1, role_employee, False, "ui", -3.0),
    ("Role.Killer", 1, role_killer, False, "ui", -3.0),
    ("Sabotage.PrinterJam", 1, sabotage_printer_jam, False, "sfx", -3.0),
    ("Sabotage.PowerCut", 1, sabotage_power_cut, False, "sfx", -3.0),
    ("Sabotage.Lock", 1, sabotage_lock, False, "sfx", -3.0),
    ("Sabotage.FileCorrupt", 1, sabotage_file_corrupt, False, "sfx", -3.0),
    ("Sabotage.PropMove", 1, sabotage_prop_move, False, "sfx", -3.0),
    ("Terminal.Open", 1, terminal_open, False, "ui", -4.0),
    ("Terminal.Beep", 1, terminal_beep, False, "ui", -6.0),
    ("Meeting.Gavel", 1, meeting_gavel, False, "ui", -3.0),
    ("Meeting.Bell", 1, meeting_bell, False, "ui", -3.0),
    ("Kill.Sting", 1, kill_sting, False, "sfx", -3.0),
    # The meeting (M4.1)
    ("Meeting.VoteTick", 1, meeting_vote_tick, False, "ui", -6.0),
    ("Meeting.TimerTick", 1, meeting_timer_tick, False, "ui", -5.0),
    ("Meeting.RPSReveal", 1, meeting_rps_reveal, False, "ui", -3.0),
    ("Meeting.Probation", 1, meeting_probation, False, "ui", -3.0),
    ("Meeting.Forgery", 1, meeting_forgery, False, "ui", -3.0),
    # UI
    ("UI.Hover", 1, ui_hover, False, "ui", -6.0),
    ("UI.Click", 1, ui_click, False, "ui", -3.0),
    ("UI.Back", 1, ui_back, False, "ui", -3.0),
    ("UI.Toggle", 1, ui_toggle, False, "ui", -3.0),
    ("UI.SliderTick", 1, ui_slider_tick, False, "ui", -6.0),
    ("UI.Error", 1, ui_error, False, "ui", -3.0),
    ("UI.MenuOpen", 1, lambda v: ui_whoosh(True), False, "ui", -4.0),
    ("UI.MenuClose", 1, lambda v: ui_whoosh(False), False, "ui", -4.0),
    # Ambience (quieter masters; playback level is set per wave in uo_audio.py)
    ("Amb.Hum", 1, amb_hum, True, "ambience", -12.0),
    ("Amb.RoomTone", 1, amb_room_tone, True, "ambience", -14.0),
    ("Amb.Vent", 1, amb_vent, True, "ambience", -14.0),
    ("Amb.OfficeMurmur", 1, amb_office_murmur, True, "ambience", -10.0),
    ("Amb.Fridge", 1, amb_fridge, True, "ambience", -12.0),
    ("Amb.Vending", 1, amb_vending, True, "ambience", -12.0),
    ("Amb.ClockTick", 1, amb_clock_tick, True, "ambience", -10.0),
    ("Amb.Exterior", 1, amb_exterior, True, "ambience", -12.0),
    ("Amb.OvertimeDrone", 1, amb_overtime_drone, True, "ambience", -8.0),
    ("Amb.Creak", 4, amb_creak, False, "ambience", -4.0),
    # Music
    ("Music.Menu", 1, music_menu, True, "music", -3.0),
    ("Music.Dark", 1, music_dark, True, "music", -3.0),
]


# Playback per sound id: (wave volume, attenuation preset or None for 2D). Starting points, tune by ear.
WAVE_SETTINGS = {
    "Footstep.Carpet": (0.5, "ATT_Footstep"),
    "Footstep.Tile": (0.7, "ATT_Footstep"),
    "Player.Jump": (0.35, "ATT_Footstep"),
    "Player.Land": (0.6, "ATT_Footstep"),
    "Interact.Start": (0.45, "ATT_Interaction"),
    "Interact.HoldLoop": (0.35, "ATT_Interaction"),
    "Interact.Complete": (0.5, "ATT_Interaction"),
    "Interact.Denied": (0.35, None),
    "Printer.Cartridge": (0.85, "ATT_Interaction"),
    "Printer.WhirLoop": (0.4, "ATT_Interaction"),
    "Printer.Feed": (0.55, "ATT_Interaction"),
    "Coffee.GurgleLoop": (0.55, "ATT_Interaction"),
    "Coffee.Beep": (0.25, "ATT_Interaction"),
    "Drawer.Open": (0.6, "ATT_Interaction"),
    "Drawer.Close": (0.65, "ATT_Interaction"),
    "Paper.Rustle": (0.6, "ATT_Interaction"),
    "Supply.Rummage": (0.7, "ATT_Interaction"),
    "Item.Pickup": (0.5, "ATT_Interaction"),
    "Item.Drop": (0.6, "ATT_Interaction"),
    "Task.Chime": (0.45, None),
    "Task.AllDone": (0.55, None),
    "Day.ShiftBell": (0.6, None),
    "Day.OvertimeStinger": (0.7, None),
    "Day.PowerDown": (0.55, None),
    "Day.ClosingBell": (0.6, None),
    "Eval.Star1": (0.45, None),
    "Eval.Star2": (0.45, None),
    "Eval.Star3": (0.45, None),
    "Eval.Fired": (0.7, None),
    "Eval.Stamp": (0.7, None),
    "Boss.Question": (0.45, None),
    "Boss.Tick": (0.35, None),
    "Boss.Correct": (0.45, None),
    "Boss.Wrong": (0.4, None),
    "Vote.Cast": (0.5, None),
    "Role.Employee": (0.55, None),
    "Role.Killer": (0.6, None),
    "Sabotage.PrinterJam": (0.75, "ATT_Interaction"),
    "Sabotage.PowerCut": (0.7, "ATT_Interaction"),
    "Sabotage.Lock": (0.75, "ATT_Interaction"),
    "Sabotage.FileCorrupt": (0.55, "ATT_Interaction"),
    "Sabotage.PropMove": (0.65, "ATT_Interaction"),
    "Terminal.Open": (0.45, None),
    "Terminal.Beep": (0.35, None),
    "Meeting.Gavel": (0.85, None),
    "Meeting.Bell": (0.55, None),
    "Kill.Sting": (0.6, None),
    "Meeting.VoteTick": (0.4, None),
    "Meeting.TimerTick": (0.45, None),
    "Meeting.RPSReveal": (0.55, None),
    "Meeting.Probation": (0.65, None),
    "Meeting.Forgery": (0.6, None),
    "UI.Hover": (0.22, None),
    "UI.Click": (0.45, None),
    "UI.Back": (0.4, None),
    "UI.Toggle": (0.4, None),
    "UI.SliderTick": (0.2, None),
    "UI.Error": (0.3, None),
    "UI.MenuOpen": (0.35, None),
    "UI.MenuClose": (0.35, None),
    "Amb.Hum": (0.3, "ATT_Hum"),
    "Amb.RoomTone": (0.35, None),
    "Amb.Vent": (0.35, "ATT_EmitterSmall"),
    "Amb.OfficeMurmur": (0.7, "ATT_EmitterRoom"),
    "Amb.Fridge": (0.45, "ATT_EmitterSmall"),
    "Amb.Vending": (0.4, "ATT_EmitterSmall"),
    "Amb.ClockTick": (0.4, "ATT_Clock"),
    "Amb.Exterior": (0.4, "ATT_EmitterRoom"),
    "Amb.OvertimeDrone": (0.45, None),
    "Amb.Creak": (0.75, "ATT_Creak"),
    "Music.Menu": (0.5, None),
    "Music.Dark": (0.5, None),
}


def file_stem(sound_id, variant, variants):
    stem = sound_id.replace(".", "_")
    return "%s_%02d" % (stem, variant) if variants > 1 else stem


# ==============================================================================================
# Output, analysis, report
# ==============================================================================================

def write_wav(path, channels):
    n = len(channels[0])
    data = array.array("h", [0]) * (n * len(channels))
    nc = len(channels)
    for c, ch in enumerate(channels):
        for i, v in enumerate(ch):
            if v > 1.0:
                v = 1.0
            elif v < -1.0:
                v = -1.0
            data[i * nc + c] = int(round(v * 32767.0))
    if sys.byteorder != "little":
        data.byteswap()
    with wave.open(path, "wb") as w:
        w.setnchannels(nc)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())


def read_wav(path):
    with wave.open(path, "rb") as w:
        nc = w.getnchannels()
        sr = w.getframerate()
        n = w.getnframes()
        raw = w.readframes(n)
    data = array.array("h")
    data.frombytes(raw)
    if sys.byteorder != "little":
        data.byteswap()
    chans = [[data[i * nc + c] / 32768.0 for i in range(n)] for c in range(nc)]
    return chans, sr


def db(v):
    return 20.0 * math.log10(max(v, 1e-9))


def analyze(chans, sr, loop):
    n = len(chans[0])
    pk = max(max(abs(v) for v in ch) for ch in chans)
    sq = sum(sum(v * v for v in ch) for ch in chans) / (n * len(chans))
    rms = math.sqrt(sq)
    dc = max(abs(sum(ch) / n) for ch in chans)
    seam = None
    if loop:
        # Click detector: second difference across the seam (last two samples -> first sample) vs. the 99th
        # percentile of the file's own |second difference|. A smooth continuation scores <= ~1; a click >> 2.
        worst = 0.0
        for ch in chans:
            d2 = sorted(abs(ch[i] - 2.0 * ch[i - 1] + ch[i - 2]) for i in range(2, n))
            p99 = d2[int(len(d2) * 0.99)]
            seam_d2 = max(abs(ch[0] - 2.0 * ch[-1] + ch[-2]), abs(ch[1] - 2.0 * ch[0] + ch[-1]))
            worst = max(worst, seam_d2 / max(p99, 1e-9))
        seam = worst
    edges = max(max(abs(ch[0]), abs(ch[-1])) for ch in chans)
    return {"duration": n / float(sr), "peak_db": db(pk), "rms_db": db(rms), "crest_db": db(pk) - db(rms),
            "dc": dc, "seam_ratio": seam, "edge": edges, "channels": len(chans)}


def verdict(stats, loop):
    issues = []
    if stats["peak_db"] > -0.5:
        issues.append("clipping risk")
    if stats["dc"] > 0.01:
        issues.append("DC offset")
    if loop and stats["seam_ratio"] is not None and stats["seam_ratio"] > 2.0:
        issues.append("loop seam jump")
    if not loop and stats["edge"] > 0.002:
        issues.append("edge not faded")
    return ", ".join(issues) if issues else "ok"


def generate(only=None):
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)
    manifest = []
    t_all = time.time()
    for sound_id, variants, fn, loop, category, peak_db in SOUNDS:
        selected = not only or any(sound_id.startswith(p) for p in only)
        for v in range(1, variants + 1):
            stem = file_stem(sound_id, v, variants)
            path = os.path.join(OUT_DIR, stem + ".wav")
            entry = {"id": sound_id, "variant": v, "file": stem + ".wav", "asset": "SW_" + stem, "loop": loop, "category": category}
            if selected:
                t0 = time.time()
                sig = fn(v)
                if isinstance(sig, tuple):
                    l, r = normalize_stereo(sig[0], sig[1], peak_db)
                    chans = [l, r]
                else:
                    chans = [normalize(sig, peak_db)]
                write_wav(path, chans)
                log("%-24s -> %s (%.1fs)" % (sound_id, stem, time.time() - t0))
            manifest.append(entry)
    with open(os.path.join(OUT_DIR, "manifest.json"), "w") as f:
        json.dump({"version": SYNTH_VERSION, "sample_rate": SR, "sounds": manifest}, f, indent=1)
    log("generated in %.1fs" % (time.time() - t_all))
    return manifest


def report(manifest=None):
    if manifest is None:
        with open(os.path.join(OUT_DIR, "manifest.json")) as f:
            manifest = json.load(f)["sounds"]
    rows = []
    for e in manifest:
        path = os.path.join(OUT_DIR, e["file"])
        if not os.path.isfile(path):
            continue
        chans, sr = read_wav(path)
        s = analyze(chans, sr, e["loop"])
        s["verdict"] = verdict(s, e["loop"])
        rows.append((e, s))
    lines = ["| File | Ch | Dur (s) | Peak dBFS | RMS dBFS | Crest dB | Loop seam | Game vol | Game RMS dB | 3D | Check |",
             "|---|---|---|---|---|---|---|---|---|---|---|"]
    for e, s in rows:
        seam = "%.2f" % s["seam_ratio"] if s["seam_ratio"] is not None else "-"
        vol, att = WAVE_SETTINGS.get(e["id"], (1.0, None))
        lines.append("| %s | %d | %.2f | %.1f | %.1f | %.1f | %s | %.2f | %.1f | %s | %s |" % (
            e["file"], s["channels"], s["duration"], s["peak_db"], s["rms_db"], s["crest_db"], seam, vol,
            s["rms_db"] + db(vol), att or "2D", s["verdict"]))
    table = "\n".join(lines)
    bad = [(e, s) for e, s in rows if s["verdict"] != "ok"]
    total = sum(s["duration"] for _, s in rows)
    summary = "%d files, %.1f s of audio, %d flagged" % (len(rows), total, len(bad))
    log(summary)
    for e, s in bad:
        log("FLAGGED %s: %s" % (e["file"], s["verdict"]))
    print(table)
    with open(os.path.join(OUT_DIR, "loudness.md"), "w", encoding="utf-8") as f:
        f.write(summary + "\n\n" + table + "\n")
    _update_doc(summary, table)
    return rows


def _update_doc(summary, table):
    """Rewrites the generated loudness section of Docs/Audio.md (between markers) if the doc exists."""
    if not os.path.isfile(DOC_PATH):
        return
    with open(DOC_PATH, encoding="utf-8") as f:
        text = f.read()
    begin = "<!-- loudness:begin -->"
    end = "<!-- loudness:end -->"
    if begin not in text or end not in text:
        return
    head = text.split(begin)[0]
    tail = text.split(end)[1]
    body = "%s\n_Generated by `uo_audio_synth.py`. Loop seam = largest second difference across the wrap point / 99th percentile of the file's own second differences (<= ~1 is a smooth continuation, > 2 is flagged as a click). Game vol = wave volume set on import; Game RMS = file RMS + wave volume (before sound class, attenuation and player sliders)._\n\n%s\n\n%s\n%s" % (
        begin, summary, table, end)
    with open(DOC_PATH, "w", encoding="utf-8") as f:
        f.write(head + body + tail)


def main(argv):
    only = None
    report_only = False
    for i, a in enumerate(argv):
        if a == "--only" and i + 1 < len(argv):
            only = [s for s in argv[i + 1].split(",") if s]
        if a == "--report-only":
            report_only = True
    manifest = None if report_only else generate(only)
    report(manifest)


if __name__ == "__main__":
    main(sys.argv[1:])
