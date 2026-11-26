// Salida.hpp — etapa de salida común de ESPECTROS: paso alto + compresor suave + limitador con anticipación
// Núcleo compartido (sin dependencias de plataforma): lo usan VCV Rack, MetaModule, Daisy y el Cardputer.
//
// Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado
// Escuela de Artes y Humanidades, Universidad EAFIT
#pragma once
#include <math.h>
#include <string.h>

namespace espectros {

constexpr float PI_F = 3.14159265358979f;
constexpr float TWO_PI_F = 6.28318530717959f;

// ---------------------------------------------------------------- etapa de salida (de ESPECTROS)
// paso alto + compresor suave + limitador con anticipación: los gemelos nunca saturan
struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  void highpass(float sr, float fc, float q = 0.7071f) {
    const float w = TWO_PI_F * fc / sr, c = cosf(w), s = sinf(w), al = s / (2 * q), a0 = 1 + al;
    b0 = (1 + c) * 0.5f / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
  }
  inline float process(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};
struct Limitador {
  static constexpr int LA = 64;
  float buf[LA] = {};
  int idx = 0, holdCnt = 0;
  float techo = 0.708f, hold = 1.f, g = 1.f, att = 0.06f, rel = 0.0002f;
  void init(float sr) { memset(buf, 0, sizeof buf); idx = 0; g = hold = 1.f; att = 1.f - expf(-1.f / (LA * 0.22f)); rel = 1.f - expf(-1.f / (0.18f * sr)); }
  inline float process(float x) {
    const float a = fabsf(x), req = a > techo ? techo / a : 1.f;
    if (req <= hold) { hold = req; holdCnt = LA; } else if (holdCnt > 0) --holdCnt; else hold = req;
    g += (hold < g ? att : rel) * (hold - g);
    const float y = buf[idx] * g; buf[idx] = x; if (++idx >= LA) idx = 0;
    return y;
  }
};
struct Compresor {
  float env2 = 0, g = 1, gT = 1, thr = 0.25f, cA = 0.01f, cR = 0.0001f, gA = 0.01f, gR = 0.0001f; int c = 0;
  void init(float sr) {
    env2 = 0; g = gT = 1;
    cA = 1.f - expf(-1.f / (0.008f * sr)); cR = 1.f - expf(-1.f / (0.3f * sr));
    gA = 1.f - expf(-1.f / (0.005f * sr)); gR = 1.f - expf(-1.f / (0.4f * sr));
  }
  inline float process(float x) {
    const float ms = x * x; env2 += (ms - env2) * (ms > env2 ? cA : cR);
    if ((c++ & 15) == 0) { const float rms = sqrtf(env2); gT = rms > thr ? powf(thr / rms, 0.7f) : 1.f; }
    g += (gT - g) * (gT < g ? gA : gR);
    return x * g;
  }
};
struct Salida {
  Biquad hp; Compresor comp; Limitador lim; float trim = 0.62f;
  void init(float sr, float hz = 30.f) { hp = Biquad(); hp.highpass(sr, hz); comp.init(sr); lim.init(sr); }
  inline float operator()(float x) {
    float y = lim.process(comp.process(hp.process(x * trim)));
    return y > 0.98f ? 0.98f : (y < -0.98f ? -0.98f : y);
  }
};

}  // namespace espectros
