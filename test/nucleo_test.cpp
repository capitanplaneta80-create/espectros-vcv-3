#include "../nucleos/lyapunov/Nucleo.hpp"
#include "../nucleos/comun/Scala.hpp"
#include <cstdio>
#include <chrono>
using namespace lyap;
static void cargar(Afin& a, int k, float root) { const Afinacion& t = AFINACIONES[k]; a.n = t.n; a.periodo = t.periodo; for (int i = 0; i < t.n; ++i) a.cents[i] = t.cents[i]; a.raizHz = root; }
struct R { double pk = 0, rms = 0; int nA = 0, nB = 0, hor = 0, nan = 0; double corrAB = 0; };
R run(Nucleo& n, double sec, float sr) {
  R r; double sAB = 0, sAA = 0, sBB = 0; long N = (long)(sec * sr);
  for (long i = 0; i < N; ++i) { float a, b, m; n.process(a, b, m);
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(m)) r.nan++;
    r.pk = std::max(r.pk, (double)std::max(fabsf(a), std::max(fabsf(b), fabsf(m)))); r.rms += m * m;
    sAB += a * b; sAA += a * a; sBB += b * b; r.nA += n.notaA.on; r.nB += n.notaB.on; r.hor += n.horizonte; }
  r.rms = sqrt(r.rms / N); r.corrAB = sAB / sqrt(sAA * sBB + 1e-12); return r;
}
int main() {
  const float sr = 48000;
  const char* reg[] = {"pequeños", "grandes", "divergencia"}, *son[] = {"audif", "FM"};
  for (int s = 0; s < 2; ++s) for (int g = 0; g < 3; ++g) for (float imp : {-0.5f, 0.f, 1.f}) {
    Nucleo n; n.regimen = g; n.sonido = s; n.impulso = imp; n.f0 = s ? 0.35f : 30.f; cargar(n.afin, 1, 110); n.init(sr);
    R r = run(n, 20, sr);
    printf("%-6s %-11s imp %+.1f | pico %.3f rms %.3f | notas A %d B %d | horizonte %d | corr A·B %.2f | λ %.3f | nan %d\n", son[s], reg[g], imp, r.pk, r.rms, r.nA, r.nB, r.hor, r.corrAB, n.lambda, r.nan);
  }
  // frecuencia alta (V/OCT) y estabilidad
  for (float f : {110.f, 220.f, 440.f}) { Nucleo n; n.f0 = f; n.impulso = 1; n.init(sr); R r = run(n, 10, sr); printf("f0 %.0f Hz: pico %.3f nan %d notas %d\n", f, r.pk, r.nan, r.nA); }
  // congelado
  { Nucleo n; n.init(sr); run(n, 1, sr); n.tiempo = 0; Estado a = n.A; run(n, 1, sr); printf("congelado: θ1 %.5f → %.5f\n", a.t1, n.A.t1); }
  // CPU
  { Nucleo n; n.impulso = 1; n.init(sr); auto t0 = std::chrono::steady_clock::now(); run(n, 30, sr); double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); printf("CPU: %.2f%% de un núcleo (30 s en %.3f s)\n", el / 30 * 100, el); }
  { Nucleo n; n.impulso = 1; n.f0 = 220; n.init(sr); auto t0 = std::chrono::steady_clock::now(); run(n, 10, sr); double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); printf("CPU f0=220: %.2f%%\n", el / 10 * 100); }
  // notas cuantizadas: ¿caen en la afinación?
  { Nucleo n; cargar(n.afin, 1, 110); n.impulso = 1; n.init(sr); int ok = 0, tot = 0; for (long i = 0; i < 48000L * 20; ++i) { float a, b, m; n.process(a, b, m); if (n.notaA.on) { tot++; float c = 1200 * log2f(n.notaA.hz / 110.f); c = fmodf(c, 1200); if (c < 0) c += 1200; for (int k = 0; k < 5; ++k) if (fabsf(c - AFINACIONES[1].cents[k]) < 1 || fabsf(c - 1200) < 1) { ok++; break; } } } printf("Slendro: %d de %d notas en la escala\n", ok, tot); }
  // Scala
  { ScalaArchivo f; bool ok = scalaParse("! prueba.scl\n!\nBohlen-Pierce justa\n 3\n!\n 9/7\n 5/3\n 3\n", f); printf("Scala: %d %s n=%zu periodo %.1f c1 %.1f\n", ok, f.descripcion.c_str(), f.cents.size(), f.periodo, f.cents[1]); }
}
