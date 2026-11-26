"""The sounds of the scene, made here from noise and tones (no recordings): wind, rain, the town's traffic, birds by
day, crickets at night, thunder.

usage: python tools/sounds.py      (from the project folder; needs numpy and scipy)
Output: sounds/*.wav (22 kHz, 16 bit, mono). The ambient ones loop without a seam (their end is faded into their
start); the program (Environment.cpp: Sound) sets their volumes from the weather, the time of day and the traffic.
"""
import os
import wave

import numpy as np
from scipy import signal

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "sounds")
SR = 22050
rng = np.random.default_rng(5)


def lowpass(x, f, order=2):
    b, a = signal.butter(order, f / (SR / 2), "low")
    return signal.lfilter(b, a, x)


def highpass(x, f, order=2):
    b, a = signal.butter(order, f / (SR / 2), "high")
    return signal.lfilter(b, a, x)


def bandpass(x, lo, hi, order=2):
    b, a = signal.butter(order, [lo / (SR / 2), hi / (SR / 2)], "band")
    return signal.lfilter(b, a, x)


def brown(n):
    w = rng.standard_normal(n)
    return signal.lfilter([1.0], [1.0, -0.995], w)


def loopable(x, fade=1.0):
    """the last `fade` seconds mixed into the first ones: the end runs into the start without a click"""
    k = int(fade * SR)
    head, tail = x[:k].copy(), x[-k:]
    t = np.linspace(0, 1, k)
    x = x[:-k].copy()
    x[:k] = head * np.sqrt(t) + tail * np.sqrt(1 - t)
    return x


def save(name, x, peak=0.8):
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    data = (x * 32767).astype(np.int16)
    with wave.open(os.path.join(OUT, name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())
    print("%-12s %.1f s" % (name, len(x) / SR))


def wind(seconds=25):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    body = lowpass(brown(n), 500)
    gust = 0.55 + 0.3 * np.sin(2 * np.pi * 0.07 * t) + 0.15 * np.sin(2 * np.pi * 0.19 * t + 1.3)
    # a soft whistle whose pitch follows the gusts
    whistle = np.zeros(n)
    noise = rng.standard_normal(n)
    for k, (lo, hi) in enumerate(((280, 340), (420, 500), (650, 760))):
        whistle += bandpass(noise, lo, hi, order=3) * (0.25 / (k + 1))
    x = body * gust + whistle * gust ** 2 * 3.0
    return loopable(x, 2.0)


def rain(seconds=14):
    n = int(seconds * SR)
    hiss = highpass(lowpass(rng.standard_normal(n), 7000), 400) * 0.35
    patter = np.zeros(n)
    drops = rng.integers(0, n - 400, int(seconds * 900))
    env = np.exp(-np.arange(300) / 25.0)
    for d in drops:
        f = rng.uniform(1500, 5000)
        patter[d:d + 300] += env * np.sin(2 * np.pi * f * np.arange(300) / SR) * rng.uniform(0.05, 0.3)
    rumble = lowpass(brown(n), 250) * 0.6
    return loopable(hiss + patter + rumble, 1.5)


def traffic(seconds=30):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    x = lowpass(brown(n), 180) * 0.8          # the distant hum of the town
    for k in range(9):                        # cars passing by, now and then
        c = rng.uniform(1.5, seconds - 3.5)
        width = rng.uniform(1.2, 2.5)
        env = np.exp(-((t - c) / width) ** 2)
        f0 = rng.uniform(55, 95)
        pitch = f0 * (1 + 0.06 * np.tanh(-(t - c) / 0.6))     # a little higher coming, lower going
        engine = signal.sawtooth(2 * np.pi * np.cumsum(pitch) / SR)
        tyres = bandpass(rng.standard_normal(n), 300, 1500)
        x += env * (lowpass(engine, 400) * 0.25 + tyres * 0.35)
    return loopable(x, 2.0)


def birds(seconds=25):
    n = int(seconds * SR)
    x = np.zeros(n)
    k = 0
    while k < seconds * 1.2:
        start = rng.uniform(0, seconds - 2)
        species = rng.integers(0, 3)
        f0 = (2600, 3800, 5200)[species] * rng.uniform(0.9, 1.1)
        count = int(rng.integers(2, 7))
        gap = rng.uniform(0.08, 0.2)
        vol = rng.uniform(0.15, 0.6)
        for c in range(count):
            d = rng.uniform(0.05, 0.14)
            m = int(d * SR)
            tt = np.arange(m) / SR
            sweep = f0 * (1 + rng.uniform(-0.35, 0.35) * tt / d)
            tone = np.sin(2 * np.pi * np.cumsum(sweep) / SR) * np.sin(np.pi * tt / d) ** 2
            i = int((start + c * (d + gap)) * SR)
            if i + m < n:
                x[i:i + m] += tone * vol
        k += 1
    return loopable(x, 1.0)


def crickets(seconds=12):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    x = np.zeros(n)
    for f, rate, pulses, vol, off in ((4600, 1.9, 3, 0.6, 0.0), (4150, 1.45, 4, 0.4, 0.3), (5100, 2.3, 2, 0.25, 0.7)):
        carrier = np.sin(2 * np.pi * f * t)
        chirp = np.zeros(n)
        period = 1.0 / rate
        s = off
        while s < seconds - 0.3:
            for p in range(pulses):
                a = int((s + p * 0.035) * SR)
                m = int(0.022 * SR)
                chirp[a:a + m] += np.sin(np.pi * np.arange(m) / m) ** 2
            s += period * rng.uniform(0.9, 1.1)
        x += carrier * chirp * vol
    return loopable(x, 0.5)


def thunder(seconds=7):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    crack = highpass(rng.standard_normal(n), 800) * np.exp(-t / 0.12) * 0.7
    rumble = lowpass(brown(n), 160)
    rolls = np.zeros(n)
    for k in range(8):   # the rumble comes back in rolls
        c = rng.uniform(0.2, seconds - 1.5)
        rolls += np.exp(-((t - c) / rng.uniform(0.3, 0.9)) ** 2) * rng.uniform(0.4, 1.0)
    env = (np.minimum(t / 0.08, 1.0) * np.exp(-t / 2.2)) * (0.5 + rolls)
    return crack + rumble * env * 1.6


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    save("wind.wav", wind())
    save("rain.wav", rain())
    save("traffic.wav", traffic())
    save("birds.wav", birds(), 0.6)
    save("crickets.wav", crickets(), 0.5)
    save("thunder.wav", thunder(), 0.95)
