// Afin.hpp — afinación de una instancia: atrae alturas libres hacia una escala (cents + periodo)
// Núcleo compartido (sin dependencias de plataforma).
//
// Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado
// Escuela de Artes y Humanidades, Universidad EAFIT
#pragma once
#include <math.h>
#include "Afinaciones.hpp"

namespace espectros {

struct Afin {
  float cents[128] = {};
  int n = 0;
  float periodo = 1200.f, raizHz = 110.f, atraccion = 1.f;
  bool on() const { return n > 0 && atraccion > 0.f; }
  float snapCents(float c) const {
    const float p = periodo, k = floorf(c / p), r = c - k * p;
    float best = 0, bd = 1e9f;
    for (int i = 0; i < n; ++i) { const float d = fabsf(r - cents[i]); if (d < bd) { bd = d; best = cents[i]; } }
    if (fabsf(r - p) < bd) best = p;
    return k * p + best;
  }
  float snapHz(float hz) const {
    if (!on() || !(hz > 1.f)) return hz;
    const float c = 1200.f * log2f(hz / raizHz), s = snapCents(c);
    return raizHz * exp2f((c + (s - c) * atraccion) / 1200.f);
  }
};

}  // namespace espectros
