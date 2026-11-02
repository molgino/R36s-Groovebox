# Erzeugt ein kleines, selbst synthetisiertes Start-Sample-Paket (frei verwendbar) nach arkos/groovebox/samples
import numpy as np, wave, os, sys
SR = 44100
out = sys.argv[1] if len(sys.argv) > 1 else "arkos/groovebox/samples"
rng = np.random.default_rng(7)

def save(name, x):
    path = os.path.join(out, name + ".wav")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    x = np.asarray(x, dtype=np.float64)
    x = x / (np.max(np.abs(x)) + 1e-9) * 0.89
    fade = min(len(x), 256)
    x[-fade:] *= np.linspace(1, 0, fade)
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype("<i2").tobytes())

def t(sec): return np.arange(int(sec * SR)) / SR
def env(sec, dec): return np.exp(-t(sec) / dec)
def onepole_lp(x, a):
    y = np.zeros_like(x); s = 0.0
    for i in range(len(x)): s += a * (x[i] - s); y[i] = s
    return y

# Drums
tt = t(1.2); f = 45 + 160 * np.exp(-tt / 0.04)
save("drums/kick-boom", np.tanh(2.5 * np.sin(2 * np.pi * np.cumsum(f) / SR) * env(1.2, 0.35)))
tt = t(0.4); f = 60 + 300 * np.exp(-tt / 0.015)
save("drums/kick-punch", np.tanh(4 * np.sin(2 * np.pi * np.cumsum(f) / SR) * env(0.4, 0.09)) + 0.3 * rng.standard_normal(len(tt)) * env(0.4, 0.003))
n = rng.standard_normal(int(0.08 * SR))
save("drums/rim", (np.sin(2 * np.pi * 1700 * t(0.08)) + 0.5 * np.sin(2 * np.pi * 820 * t(0.08)) + 0.4 * (n - onepole_lp(n, 0.3))) * env(0.08, 0.012))
n = rng.standard_normal(int(0.25 * SR)); hp = n - onepole_lp(n, 0.2)
a = np.minimum(t(0.25) / 0.04, 1) * env(0.25, 0.05)
save("drums/shaker", hp * a)
tt = t(0.5); f = 110 + 90 * np.exp(-tt / 0.05)
save("drums/tom-low", np.sin(2 * np.pi * np.cumsum(f) / SR) * env(0.5, 0.16))
n = rng.standard_normal(int(1.5 * SR))
save("drums/crash", (n - onepole_lp(n, 0.15)) * env(1.5, 0.45))

# Toene (alle auf C4 gestimmt -> NOTE C-4 spielt Originaltonhoehe)
C4 = 261.63
def saw(fr, tt):
    ph = (fr * tt) % 1.0; return 2 * ph - 1
tt = t(0.9)
chord = sum(saw(C4 * r, tt) + saw(C4 * r * 1.005, tt) for r in (1, 1.189, 1.498, 2))
save("keys/stab-minor", onepole_lp(chord, 0.12) * env(0.9, 0.25))
# Karplus-Strong Pluck
N = int(SR / C4); buf = rng.uniform(-1, 1, N); y = np.zeros(int(1.5 * SR))
for i in range(len(y)):
    y[i] = buf[i % N]; buf[i % N] = 0.5 * (buf[i % N] + buf[(i + 1) % N]) * 0.996
save("keys/pluck", y)
tt = t(2.0)
save("keys/organ", (np.sin(2*np.pi*C4*tt) + 0.5*np.sin(2*np.pi*2*C4*tt) + 0.3*np.sin(2*np.pi*3*C4*tt) + 0.2*np.sin(2*np.pi*4*C4*tt)) * np.minimum(tt/0.01, 1))
tt = t(1.2)
save("keys/bass-808", np.tanh(1.5 * np.sin(2 * np.pi * (C4 / 4) * tt)) * env(1.2, 0.5) * np.minimum(tt / 0.003, 1))

# Vokal "aah" (Formantsynthese)
tt = t(1.5); f0 = C4 * (1 + 0.006 * np.sin(2 * np.pi * 5 * tt))
ph = np.cumsum(f0) / SR
src = np.zeros_like(tt)
for h in range(1, 40):
    fr = h * C4
    amp = sum(np.exp(-((fr - F) / B) ** 2) * g for F, B, g in ((730, 90, 1.0), (1090, 110, 0.5), (2440, 160, 0.25)))
    src += amp * np.sin(2 * np.pi * h * ph) / h ** 0.3
save("vox/aah", src * np.minimum(tt / 0.08, 1) * np.minimum((1.5 - tt) / 0.2, 1))

# FX
tt = t(2.0); n = rng.standard_normal(len(tt)); y = np.zeros_like(n); s = 0.0
for i in range(len(n)):
    a = 0.01 + 0.5 * (i / len(n)) ** 2; s += a * (n[i] - s); y[i] = s
save("fx/riser", y * np.minimum(tt / 1.9, 1) ** 2)
tt = t(0.6)
save("fx/zap", np.sin(2 * np.pi * np.cumsum(3000 * np.exp(-tt / 0.05) + 80) / SR) * env(0.6, 0.15))
print("Samples erzeugt in", out)
