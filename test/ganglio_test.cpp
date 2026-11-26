// Prueba del núcleo GANGLIO sin adaptador: vida de la red, NaN, saturación, saltos, CPU y un WAV de demostración.
// g++ -O2 -std=c++11 -Inucleos -o build/ganglio_test test/ganglio_test.cpp && build/ganglio_test [demo.wav]
#include "ganglio/Nucleo.hpp"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <vector>
#include <algorithm>
using namespace ganglio;

struct R { double pico = 0, rms = 0, salto = 0; int nan = 0, cruces = 0, despiertas = 0; float mn[N], mx[N]; };

static R correr(Nucleo& n, double seg, float sr, std::vector<float>* wav = nullptr) {
  R r; for (int i = 0; i < N; ++i) { r.mn[i] = 1; r.mx[i] = 0; }
  long M = (long)(seg * sr); float pl = 0, pr = 0; bool desp = false;
  for (long k = 0; k < M; ++k) {
    float l, d; n.process(l, d);
    if (!std::isfinite(l) || !std::isfinite(d)) { r.nan++; l = d = 0; }
    r.pico = std::max(r.pico, (double)std::max(fabsf(l), fabsf(d)));
    r.rms += 0.5 * (l * l + d * d);
    if (k > sr) r.salto = std::max(r.salto, (double)std::max(fabsf(l - pl), fabsf(d - pr)));
    pl = l; pr = d;
    if (k > M / 4) for (int i = 0; i < N; ++i) { r.mn[i] = std::min(r.mn[i], n.o[i]); r.mx[i] = std::max(r.mx[i], n.o[i]); }
    for (int i = 0; i < N; ++i) if (n.cruces & (1 << i)) r.cruces++;
    if (n.despierta > 0.f && !desp) r.despiertas++;
    desp = n.despierta > 0.f;
    if (wav) { wav->push_back(l); wav->push_back(d); }
  }
  r.rms = sqrt(r.rms / M);
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
  printf("temperamento  | pico  rms   salto | nan | neuronas vivas (rango de o > 0,3) | cruces | vigía\n");
  for (int t = 0; t < N_TEMPERAMENTOS; ++t) {
    static Nucleo n; n = Nucleo(); n.temperamento = t; n.init(sr);
    R r = correr(n, 120, sr);
    int vivas = 0; for (int i = 0; i < N; ++i) vivas += (r.mx[i] - r.mn[i]) > 0.3f;
    printf("%-12s  | %.3f %.3f %.3f | %3d | %d de 8 | %5d | %d\n", n.nombreTemperamento(), r.pico, r.rms, r.salto, r.nan, vivas, r.cruces, r.despiertas);
    if (r.nan || r.pico > 0.99 || vivas < 4) fallas++;
  }
  // gestos y extremos
  struct Caso { const char* nombre; int temp; float metab, ruido, inten; int acorde, filtro; bool escucha; };
  const Caso casos[] = {
    {"metabolismo ×8", 7, 8.f, 0.5f, 1.f, QUINTA, LP, true},
    {"metabolismo ×0,125", 0, 0.125f, 0.5f, 1.f, CUARTA, BP, true},
    {"ruido 3, intensidad 1,5", 2, 1.f, 3.f, 1.5f, RACIMO, LB, true},
    {"sin escucha, armónicos", 4, 1.f, 0.f, 0.25f, ARMONICOS, LP, false},
    {"séptima, paso banda", 6, 2.f, 1.5f, 1.2f, SEPTIMA, BP, true},
  };
  for (const Caso& c : casos) {
    static Nucleo n; n = Nucleo(); n.temperamento = c.temp; n.metabolismo = c.metab; n.ruido = c.ruido; n.intensidad = c.inten;
    n.acorde = c.acorde; n.filtro = c.filtro; n.escucha = c.escucha; n.init(sr);
    R r = correr(n, 60, sr);
    printf("%-26s pico %.3f rms %.3f salto %.3f nan %d cruces %d vigía %d\n", c.nombre, r.pico, r.rms, r.salto, r.nan, r.cruces, r.despiertas);
    if (r.nan || r.pico > 0.99) fallas++;
  }
  {  // gestos: mutar, red nueva, sacudir, estimular, congelar, melodía con los 5 timbres, cambio de temperamento
    static Nucleo n; n = Nucleo(); n.init(sr);
    correr(n, 10, sr); n.mutar(); R a = correr(n, 20, sr);
    n.redNueva(); R b = correr(n, 40, sr);
    int vivasNueva = 0; for (int i = 0; i < N; ++i) vivasNueva += (b.mx[i] - b.mn[i]) > 0.3f;
    n.sacudir(); n.estimulo[4] = 4.f; R c = correr(n, 5, sr); n.estimulo[4] = 0.f;
    n.congelado = true; float y0 = n.y[0]; correr(n, 3, sr); const bool quieto = n.y[0] == y0; n.congelado = false;
    double picoMel = 0; int nanMel = 0;
    for (int tb = 0; tb < N_TIMBRES; ++tb) { n.timbre = tb; n.notaOn(220.f * tb + 220.f); R m = correr(n, 0.6, sr); n.notaOff(); R m2 = correr(n, 0.6, sr); picoMel = std::max(picoMel, std::max(m.pico, m2.pico)); nanMel += m.nan + m2.nan; }
    n.temperamento = 5; R d = correr(n, 3, sr);
    printf("mutar: pico %.3f nan %d · red nueva: %d de 8 vivas, nan %d · estímulo: nan %d · congelar: %s · melodía: pico %.3f nan %d · transición: salto %.3f\n",
           a.pico, a.nan, vivasNueva, b.nan, c.nan, quieto ? "quieto" : "SE MOVIÓ", picoMel, nanMel, d.salto);
    if (a.nan || b.nan || c.nan || !quieto || nanMel || vivasNueva < 2) fallas++;
  }
  {  // 2.3 · CANTO: notas por temperamento, cola de eventos equilibrada, alturas y velocidades válidas
    printf("canto        | notas/min | on  off | vel mín–máx | Hz mín–máx | MIDI de ejemplo\n");
    for (int t = 0; t < N_TEMPERAMENTOS; ++t) {
      static Nucleo n; n = Nucleo(); n.temperamento = t; n.canta = true; n.init(sr);
      int on = 0, off = 0; float vmn = 1, vmx = 0, hmn = 1e9f, hmx = 0; bool malo = false; Evento e; Evento ej = {};
      const long M = (long)(60 * sr); double pico = 0;
      for (long k = 0; k < M; ++k) {
        float l, d; n.process(l, d); pico = std::max(pico, (double)std::max(fabsf(l), fabsf(d)));
        while (n.sacarEvento(e)) {
          if (e.on) { ++on; vmn = std::min(vmn, e.vel); vmx = std::max(vmx, e.vel); hmn = std::min(hmn, e.hz); hmx = std::max(hmx, e.hz); ej = e; }
          else ++off;
          if (!std::isfinite(e.hz) || e.hz <= 0 || e.neurona >= N) malo = true;
        }
      }
      int nota; float cents; hzAMidi(ej.hz, nota, cents);
      printf("%-12s | %9.1f | %3d %3d | %.2f–%.2f | %4.0f–%4.0f | n%d: %.1f Hz → %d %+.0f c\n", n.nombreTemperamento(), on / 1.0, on, off,
             vmn, vmx, hmn, hmx, ej.neurona + 1, ej.hz, nota, cents);
      if (malo || on == 0 || std::abs(on - off) > N || pico > 0.99) fallas++;
    }
    // umbral alto → menos notas
    static Nucleo a; a = Nucleo(); a.umbral = 0.55f; a.init(sr); correr(a, 60, sr);
    static Nucleo b; b = Nucleo(); b.umbral = 0.85f; b.init(sr); correr(b, 60, sr);
    printf("umbral 0,55: %u notas · umbral 0,85: %u notas\n", a.nNotas, b.nNotas);
  }
  {  // 2.3 · LECTURA: con mezcla 1 los parámetros siguen vivos, sin NaN y sin saturar
    static Nucleo n; n = Nucleo(); n.mezcla = 1.f; n.init(sr);
    float mn[N], mx[N]; for (int i = 0; i < N; ++i) { mn[i] = 1; mx[i] = 0; }
    R r; double pico = 0; int nan = 0;
    for (long k = 0; k < (long)(60 * sr); ++k) {
      float l, d; n.process(l, d); if (!std::isfinite(l)) nan++; pico = std::max(pico, (double)fabsf(l));
      if (k > 10 * sr) for (int i = 0; i < N; ++i) { mn[i] = std::min(mn[i], n.canal[i]); mx[i] = std::max(mx[i], n.canal[i]); }
    }
    int vivos = 0; for (int i = 0; i < N; ++i) vivos += (mx[i] - mn[i]) > 0.15f;
    n.nuevaLectura(); R r2 = correr(n, 20, sr);
    // correlación entre parámetros: la lectura poblacional debe mezclarlos (mayor correlación media que el mapeo directo)
    printf("lectura (mezcla 1): %d de 8 parámetros vivos · pico %.3f nan %d · nueva lectura: pico %.3f nan %d\n", vivos, pico, nan, r2.pico, r2.nan);
    if (nan || r2.nan || vivos < 4 || pico > 0.99) fallas++;
    (void)r;
  }
  {  // 2.3 · SENTIDOS: un tren de golpes por OÍDO produce ataques; ruido brillante vs. seno grave mueve el brillo
    static Nucleo n; n = Nucleo(); n.escuchaExterna = true; n.init(sr);
    Azar az; float brSeno = 0, brRuido = 0;
    for (long k = 0; k < (long)(20 * sr); ++k) {
      const float t = k / sr;
      const bool golpes = t < 10.f;
      const float fase = fmodf(t, 0.5f);
      n.entrada = golpes ? (fase < 0.08f ? 0.6f * expf(-fase * 40.f) * az.bi() : 0.f)       // 2 golpes por segundo
                         : (t < 15.f ? 0.4f * sinf(TWO_PI_F * 110.f * t) : 0.3f * az.bi()); // seno grave, luego ruido blanco
      float l, d; n.process(l, d);
      if (k == (long)(14.9f * sr)) brSeno = n.brillo;
      if (k == (long)(19.9f * sr)) brRuido = n.brillo;
      if (k == (long)(10 * sr)) printf("sentidos: %u ataques en 10 s de golpes (esperados ~20)", n.nAtaques);
    }
    printf(" · brillo seno 110 Hz %.3f · ruido blanco %.3f\n", brSeno, brRuido);
    if (n.nAtaques < 12 || n.nAtaques > 30 || !(brRuido > brSeno + 0.3f)) fallas++;
    static Nucleo s; s = Nucleo(); s.sensibilidad = 2.f; s.init(sr); R r = correr(s, 60, sr);
    printf("sensibilidad 2 con su propio bordón: pico %.3f nan %d ataques %u\n", r.pico, r.nan, s.nAtaques);
    if (r.nan || r.pico > 0.99) fallas++;
  }
  {  // 2.3 · PLASTICIDAD: el hábito crece, se acota, no mata a la red, sobrevive a exportar/importar y OLVIDAR lo borra
    printf("plasticidad  | hábito 1 min  3 min  5 min | máx |D| | vivas a los 5 min | pico\n");
    for (int t = 0; t < N_TEMPERAMENTOS; ++t) {
      static Nucleo n; n = Nucleo(); n.temperamento = t; n.plasticidad = 1.f; n.init(sr);
      float h[3]; R r;
      r = correr(n, 60, sr); h[0] = n.habito();
      r = correr(n, 120, sr); h[1] = n.habito();
      r = correr(n, 120, sr); h[2] = n.habito();
      float dmax = 0; for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) dmax = std::max(dmax, fabsf(n.D[i][j]));
      int vivas = 0; for (int i = 0; i < N; ++i) vivas += (r.mx[i] - r.mn[i]) > 0.3f;
      printf("%-12s | %10.2f %6.2f %6.2f | %7.2f | %d de 8 | %.3f\n", n.nombreTemperamento(), h[0], h[1], h[2], dmax, vivas, r.pico);
      if (r.nan || r.pico > 0.99 || dmax > 8.001f || h[2] <= 0.f || vivas < 2) fallas++;
    }
    static Nucleo n; n = Nucleo(); n.plasticidad = 1.f; n.init(sr); correr(n, 90, sr);
    float buf[Nucleo::TAM_MATRIZ]; n.exportarHabito(buf);
    static Nucleo m; m = Nucleo(); m.init(sr); m.importarHabito(buf);
    const bool igual = fabsf(m.habito() - n.habito()) < 1e-6f;
    n.plasticidad = 0.f; const float h0 = n.habito(); correr(n, 30, sr); const bool quieto = n.habito() == h0;
    n.olvidar();
    printf("hábito: exportar/importar %s · con plasticidad 0 %s · olvidar → %.3f\n", igual ? "idéntico" : "DISTINTO",
           quieto ? "se conserva" : "CAMBIÓ", n.habito());
    if (!igual || !quieto || n.habito() != 0.f) fallas++;
  }
  {  // CPU, con las cuatro capas encendidas
    static Nucleo n; n = Nucleo(); n.canta = true; n.mezcla = 1.f; n.plasticidad = 1.f; n.sensibilidad = 2.f; n.init(sr);
    auto t0 = std::chrono::steady_clock::now(); correr(n, 30, sr);
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("CPU: %.2f %% de un núcleo (30 s en %.3f s) · estado: %u bytes\n", el / 30 * 100, el, (unsigned)sizeof(Nucleo));
  }
  if (argc > 1) {  // demo de 130 s: GRILLO → CIGARRA (mutada) → LUCIÉRNAGA con melodía → ENJAMBRE estimulado → GRILLO que canta, lee y aprende
    static Nucleo n; n = Nucleo(); n.init(sr);
    std::vector<float> wav; wav.reserve((size_t)(sr * 135 * 2));
    correr(n, 20, sr, &wav);
    n.temperamento = 6; n.acorde = SEPTIMA; correr(n, 10, sr, &wav); n.mutar(); correr(n, 15, sr, &wav);
    n.temperamento = 5; n.filtro = LB; n.timbre = CAMPANA;
    const float notas[] = {220.f, 247.5f, 275.f, 330.f, 293.3f, 247.5f};
    for (int k = 0; k < 12; ++k) { n.notaOn(notas[k % 6]); correr(n, 0.9, sr, &wav); n.notaOff(); correr(n, 0.35, sr, &wav); }
    n.temperamento = 2; n.acorde = RACIMO; n.metabolismo = 2.f; correr(n, 8, sr, &wav);
    n.estimulo[4] = 3.f; correr(n, 4, sr, &wav); n.estimulo[4] = 0.f; n.estimulo[6] = -3.f; correr(n, 4, sr, &wav); n.estimulo[6] = 0.f;
    n.sacudir(); correr(n, 14, sr, &wav);
    // 2.3: GRILLO canta con la campana, la lectura se mezcla y la red aprende (metabolismo ×2 para oírlo en 40 s)
    n.temperamento = 0; n.acorde = SEPTIMA; n.filtro = LP; n.canta = true; n.timbre = CAMPANA; n.metabolismo = 2.f;
    correr(n, 12, sr, &wav);
    n.mezcla = 1.f; correr(n, 12, sr, &wav);
    n.plasticidad = 1.f; correr(n, 16, sr, &wav);
    escribirWav(argv[1], wav, (int)sr);
    printf("demo: %s (%.0f s)\n", argv[1], wav.size() / 2 / sr);
  }
  printf(fallas ? "FALLAS: %d\n" : "todo bien\n", fallas);
  return fallas ? 1 : 0;
}
