// Prueba del núcleo RYDBERG sin adaptador: física, NaN, saturación, saltos, CPU y un WAV de demostración.
// g++ -O2 -std=c++11 -Inucleos -o build/rydberg_test test/rydberg_test.cpp && build/rydberg_test [demo.wav]
// Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado · Escuela de Artes y Humanidades, Universidad EAFIT
#include "rydberg/Nucleo.hpp"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>
#include <algorithm>
using namespace rydberg;

struct R { double pico = 0, rms = 0, salto = 0, corr = 0; int nan = 0; uint32_t fotones = 0, dobles = 0; };

template <class F> static R correr(Nucleo& n, double seg, float sr, F cb, std::vector<float>* wav = nullptr) {
  R r; long M = (long)(seg * sr); float pl = 0, pr = 0; double sLR = 0, sLL = 0, sRR = 0;
  const uint32_t e0 = n.emitidos, d0 = n.dobles;
  for (long k = 0; k < M; ++k) {
    if ((k & 255) == 0) cb(k / sr);
    float l, d; n.process(l, d);
    if (!std::isfinite(l) || !std::isfinite(d)) { r.nan++; l = d = 0; }
    r.pico = std::max(r.pico, (double)std::max(fabsf(l), fabsf(d)));
    r.rms += 0.5 * (l * l + d * d);
    if (k > sr) r.salto = std::max(r.salto, (double)std::max(fabsf(l - pl), fabsf(d - pr)));
    pl = l; pr = d; sLR += l * d; sLL += l * l; sRR += d * d;
    if (wav) { wav->push_back(l); wav->push_back(d); }
  }
  r.rms = sqrt(r.rms / M); r.corr = sLR / sqrt(sLL * sRR + 1e-12);
  r.fotones = n.emitidos - e0; r.dobles = n.dobles - d0;
  return r;
}

static void escribirWav(const char* ruta, const std::vector<float>& x, int sr) {
  FILE* f = fopen(ruta, "wb"); if (!f) return;
  const uint32_t datos = x.size() * 2, total = 36 + datos; const uint16_t canales = 2, bits = 16, fmt = 1, bloque = 4;
  const uint32_t bps = sr * bloque, fmtLen = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&total, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtLen, 4, 1, f);
  fwrite(&fmt, 2, 1, f); fwrite(&canales, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&bps, 4, 1, f); fwrite(&bloque, 2, 1, f); fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f); fwrite(&datos, 4, 1, f);
  for (float v : x) { const int16_t s = (int16_t)lrintf(std::max(-1.f, std::min(1.f, v)) * 32767.f); fwrite(&s, 2, 1, f); }
  fclose(f);
}

int main(int argc, char** argv) {
  const float sr = 48000;
  int fallas = 0;
  { Nucleo n; n.init(sr);
    const double razon = n.lineaHz(2, 1) / n.lineaHz(3, 2);
    printf("física: H-alfa %.1f Hz · Lyman-alfa %.1f Hz · razón %.3f (hidrógeno: 5,400)\n", n.lineaHz(3, 2), n.lineaHz(2, 1), razon);
    printf("        λ H-alfa %.1f nm (real 656,3) · λ Lyman-alfa %.1f nm (real 121,6)\n", lambdaNm(3, 2), lambdaNm(2, 1));
    if (fabs(razon - 5.4) > 1e-3) { printf("✗ la razón no es la del hidrógeno\n"); fallas++; }
    printf("estado del núcleo: %u bytes (presupuesto 32768)\n", (unsigned)sizeof(Nucleo));
  }
  // los tres modos en uso normal y en el extremo
  for (int m = 0; m < 3; ++m) for (int ext = 0; ext < 2; ++ext) {
    Nucleo n; n.modo = m; if (ext) { n.atomos = 4; n.nmax = 12; n.temperatura = 12; n.campo = 1; } else { n.atomos = 2; n.nmax = 6; n.campo = 0.3f; }
    n.init(sr);
    R r = correr(n, 30, sr, [](double) {});
    printf("%-8s %-7s | pico %.3f rms %.3f salto %.3f | fotones %4u dobles %3u | corr L·R %.2f | nan %d\n",
           NOMBRE_MODO[m], ext ? "extremo" : "normal", r.pico, r.rms, r.salto, r.fotones, r.dobles, r.corr, r.nan);
    if (r.nan || r.pico > 0.985 || r.fotones == 0) fallas++;
  }
  // altura extrema de H-alfa y gesto EXCITAR
  for (float f : {55.f, 1760.f}) { Nucleo n; n.refHz = f; n.atomos = 4; n.nmax = 12; n.temperatura = 6; n.init(sr); R r = correr(n, 10, sr, [](double) {});
    printf("H-alfa %.0f Hz: pico %.3f nan %d fotones %u\n", f, r.pico, r.nan, r.fotones); if (r.nan || r.pico > 0.985) fallas++; }
  { Nucleo n; n.temperatura = 0.1f; n.init(sr); correr(n, 2, sr, [](double) {});
    uint32_t e = n.emitidos; R r = correr(n, 3, sr, [&](double t) { static double u = -1; if (t - u > 0.25) { n.excitar(); u = t; } });
    printf("excitar cada 250 ms (T mínima): %u fotones en 3 s (antes %u)\n", r.fotones, e); }
  // CPU (el caso más cargado: Zeeman con 4 átomos)
  { Nucleo n; n.modo = ZEEMAN; n.atomos = 4; n.nmax = 12; n.temperatura = 12; n.campo = 0.6f; n.init(sr);
    auto t0 = std::chrono::steady_clock::now(); correr(n, 30, sr, [](double) {});
    const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("CPU: %.2f%% de un núcleo (30 s en %.3f s)\n", el / 30 * 100, el); }
  // demostración: cascada → espectro → Zeeman con el campo abriéndose
  if (argc > 1) {
    std::vector<float> w; Nucleo n; n.atomos = 3; n.nmax = 7; n.init(sr);
    correr(n, 45, sr, [&](double t) {
      n.modo = t < 15 ? CASCADA : t < 30 ? ESPECTRO : ZEEMAN;
      n.campo = t < 30 ? 0.f : 0.15f + 0.6f * (float)(t - 30) / 15.f;
    }, &w);
    escribirWav(argv[1], w, (int)sr);
    printf("demo: %s (%.0f s)\n", argv[1], w.size() / 2 / sr);
  }
  printf(fallas ? "✗ %d fallas\n" : "todo bien\n", fallas);
  return fallas ? 1 : 0;
}
