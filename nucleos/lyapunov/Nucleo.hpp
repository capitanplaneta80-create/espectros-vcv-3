// Nucleo.hpp — LYAPUNOV · péndulos gemelos caóticos (motor por instancia, sin dependencias de Rack)
//
// Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado
// Escuela de Artes y Humanidades, Universidad EAFIT
//
// Física: péndulo doble con m1 = m2 = 1, L1 = L2 = 1 y g = w0², de modo que la frecuencia natural
// w0 = 2π·f0 fija el "tempo" o la "altura" del caos. Se integran DOS péndulos (A y B) que parten
// separados por δ0: su separación |δ(t)| ~ δ0·e^(λt) es lo que se escucha, se ve y sale como voltaje.
//
// Diferencias con el motor de la suite web: todo el estado es de la instancia (varios LYAPUNOV pueden
// convivir en un patch), A y B salen por separado (los gemelos son el estéreo), ambos componen notas
// en la afinación elegida, el horizonte de predictibilidad es un evento, y el integrador subdivide el
// paso cuando la frecuencia sube para no perder la física.
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "../comun/Salida.hpp"
#include "../comun/Afin.hpp"

namespace lyap {

using namespace espectros;
constexpr float DEG = PI_F / 180.0f;


struct Estado { float t1, t2, w1, w2; };

enum Regimen : int { PEQUENOS = 0, GRANDES = 1, DIVERGENCIA = 2 };
enum Sonido : int { AUDIFICACION = 0, FM = 1 };

static inline float wrapPi(float a) { return a - TWO_PI_F * floorf((a + PI_F) / TWO_PI_F); }

// Ecuaciones de Euler-Lagrange del péndulo doble (m = 1, L = 1, g = w0²)
static inline void derivs(const Estado& s, float g, float damp, Estado& d) {
  const float dl = s.t1 - s.t2, sd = sinf(dl), cd = cosf(dl);
  const float den = 2.0f + 2.0f * sd * sd;
  const float w1s = s.w1 * s.w1, w2s = s.w2 * s.w2;
  float a1 = (-3.0f * g * sinf(s.t1) - g * sinf(s.t1 - 2.0f * s.t2) - 2.0f * sd * (w2s + w1s * cd)) / den;
  float a2 = (2.0f * sd * (2.0f * w1s + 2.0f * g * cosf(s.t1) + w2s * cd)) / den;
  d.t1 = s.w1; d.t2 = s.w2; d.w1 = a1 - damp * s.w1; d.w2 = a2 - damp * s.w2;
}
static inline void rk4(Estado& s, float g, float damp, float dt) {
  Estado k1, k2, k3, k4, t;
  derivs(s, g, damp, k1);
  const float h = 0.5f * dt;
  t = {s.t1 + h * k1.t1, s.t2 + h * k1.t2, s.w1 + h * k1.w1, s.w2 + h * k1.w2}; derivs(t, g, damp, k2);
  t = {s.t1 + h * k2.t1, s.t2 + h * k2.t2, s.w1 + h * k2.w1, s.w2 + h * k2.w2}; derivs(t, g, damp, k3);
  t = {s.t1 + dt * k3.t1, s.t2 + dt * k3.t2, s.w1 + dt * k3.w1, s.w2 + dt * k3.w2}; derivs(t, g, damp, k4);
  const float c = dt / 6.0f;
  s.t1 += c * (k1.t1 + 2 * k2.t1 + 2 * k3.t1 + k4.t1);
  s.t2 += c * (k1.t2 + 2 * k2.t2 + 2 * k3.t2 + k4.t2);
  s.w1 += c * (k1.w1 + 2 * k2.w1 + 2 * k3.w1 + k4.w1);
  s.w2 += c * (k1.w2 + 2 * k2.w2 + 2 * k3.w2 + k4.w2);
}


struct PuntoTraza { float xA, yA, xB, yB, logd; uint8_t split; };

// ---------------------------------------------------------------- el instrumento
class Nucleo {
 public:
  static constexpr int TRAZA = 512;
  static constexpr int TAB = 1024;

  // ---- controles (los escribe el módulo en cada muestra)
  int regimen = GRANDES, sonido = AUDIFICACION;
  float energia = 0.5f;      // 0..1 → ángulos al soltar
  float impulso = 0.f;       // -1 fricción … 0 conservativo … +1 bombeo
  float mezcla = 1.f;        // 0 = A · 1 = (A+B)/2 · 2 = (A−B)/2
  float tiempo = 1.f;        // escala del tiempo físico (0 = congelado)
  float f0 = 30.f;           // Hz, frecuencia natural del péndulo
  float baseHz = 110.f;      // nota base: portadora de FM, raíz de la afinación y registro de las notas
  float d0Grados = 0.1f;     // separación inicial A/B
  float nivel = 1.f;
  bool autoSoltar = true;    // en DIVERGENCIA: vuelve a soltar tras cruzar el horizonte
  Afin afin;

  // ---- lecturas (para salidas y pantalla)
  Estado A{}, B{};
  double tau = 0;            // tiempo adimensional (radianes de oscilación natural)
  float tauSplit = -1, lambda = 0, lambdaVivo = 0, delta = 0, energiaG = 0;
  PuntoTraza traza[TRAZA];
  // historia de la separación desde que se soltó: la pendiente de log δ ES λ. Cuando se llena,
  // se comprime a la mitad y el paso se duplica: siempre se ve todo el recorrido desde SOLTAR
  static constexpr int HIST = 256;
  struct PuntoHist { float logd; uint8_t split; };
  PuntoHist hist[HIST];
  volatile int histCount = 0;
  float histPaso = 0.25f;   // τ por punto
  volatile int trazaHead = 0, trazaCount = 0;
  volatile uint32_t soltadas = 0;

  // ---- eventos de la última muestra (los consume el módulo)
  struct Nota { bool on = false; float hz = 0, vel = 0; };
  Nota notaA, notaB;         // vuelcos de cada gemelo, cuantizados a la afinación
  bool horizonte = false;    // flanco: los gemelos acaban de separarse (se perdió la predicción)

  // ---- acciones
  void soltar() { relReq_ = true; }
  void perturbar() { pertReq_ = true; }
  void sincronizar() { syncReq_ = true; }
  void golpe(float fuerza) { kickReq_ = true; kickAmt_ = fuerza; }

  void init(float sr) {
    sr_ = sr;
    aRel_ = expf(-1.f / (0.8f * sr));
    os_ = (int)roundf(sr / 8000.0f); if (os_ < 1) os_ = 1;
    physRate_ = sr / os_;
    for (int i = 0; i <= TAB; ++i) tab_[i] = sinf(TWO_PI_F * i / TAB);
    salA_.init(sr); salB_.init(sr); salM_.init(sr);
    cnt_ = 0;
    release();
  }

  float w0() const { return TWO_PI_F * f0; }

  // una muestra de audio: a, b y mezcla en ±1 (ya limitadas)
  void process(float& outA, float& outB, float& outM) {
    notaA.on = notaB.on = false; horizonte = false;
    if (cnt_ == 0) physicsTick();
    const float frac = (float)cnt_ / os_;
    if (++cnt_ >= os_) cnt_ = 0;
    float sA, sB;
    if (sonido == AUDIFICACION) {   // la coordenada x de la segunda masa ES la forma de onda
      const float xa = pxA_ + (xA_ - pxA_) * frac, xb = pxB_ + (xB_ - pxB_) * frac;
      const float pk = fmaxf(fabsf(xa), fabsf(xb));
      aEnv_ = pk > aEnv_ ? pk : aEnv_ * aRel_;
      const float gN = fminf(audNorm_, 0.85f / fmaxf(aEnv_, 1e-3f));
      sA = gN * xa; sB = gN * xb;
    } else { sA = fmVoice(vA_); sB = fmVoice(vB_); }
    const float m = mezcla;
    const float s = m <= 1.f ? (1.f - 0.5f * m) * sA + 0.5f * m * sB : 0.5f * sA + (0.5f - (m - 1.f)) * sB;
    const float g = 0.7f * nivel;
    outA = salA_(sA * g); outB = salB_(sB * g); outM = salM_(s * g);
  }

 private:
  struct Voz { float pc = 0, pm = 0, fc = 110, fm = 220, idx = 0, amp = 0, tfc = 110, tfm = 220, tidx = 0, tamp = 0; };
  float sr_ = 48000, physRate_ = 8000;
  int os_ = 6, cnt_ = 0, decim_ = 0;
  bool relReq_ = false, kickReq_ = false, pertReq_ = false, syncReq_ = false;
  float kickAmt_ = 1.f;
  double simT_ = 0, lastFlipA_ = 0, lastFlipB_ = 0;
  float prevT2A_ = 0, prevT2B_ = 0, delta0_ = 0.1f * DEG;
  double nextHist_ = 0;
  void reiniciarHist() { histCount = 0; nextHist_ = 0; histPaso = 0.25f; }
  float xA_ = 0, pxA_ = 0, xB_ = 0, pxB_ = 0, audNorm_ = 1, aEnv_ = 0, aRel_ = 0.99998f;
  Voz vA_, vB_;
  Salida salA_, salB_, salM_;
  float tab_[TAB + 1];

  inline float sinT(float ph) const { ph -= floorf(ph); const float x = ph * TAB; const int i = (int)x; const float f = x - i; return tab_[i] + (tab_[i + 1] - tab_[i]) * f; }
  float bHz() const { return baseHz; }

  void release() {
    const float e = energia;
    float a1, a2;
    if (regimen == PEQUENOS) { a1 = (4.0f + 18.0f * e) * DEG; a2 = a1 * 1.25f; }      // modos normales, λ = 0
    else { a1 = (60.0f + 80.0f * e) * DEG; a2 = (90.0f + 70.0f * e) * DEG; }          // caos
    A = {a1, a2, 0, 0}; B = A;
    delta0_ = d0Grados * DEG; B.t2 += delta0_;
    tau = 0; tauSplit = -1; lambda = 0; lambdaVivo = 0; delta = delta0_;
    xA_ = pxA_ = 0.5f * (sinf(A.t1) + sinf(A.t2));
    xB_ = pxB_ = 0.5f * (sinf(B.t1) + sinf(B.t2));
    { const float amp = fabsf(xA_); audNorm_ = 0.9f / (amp < 0.15f ? 0.15f : amp); if (audNorm_ > 1.2f && regimen != PEQUENOS) audNorm_ = 1.2f; }
    trazaCount = 0;
    reiniciarHist();
    soltadas = soltadas + 1;
  }

  // semitonos relativos a 2·base → grado de la afinación
  float quant(float semis) const {
    if (!afin.on()) return semis;
    const float ref = bHz() * 2.f, hz = ref * exp2f(semis / 12.f);
    return 12.f * log2f(afin.snapHz(hz) / ref);
  }

  void updateVoice(Voz& v, const Estado& s, float w0) {
    // portadora ← θ2, moduladora ← θ1, índice ← |sin(θ1 − θ2)| (el término de acoplamiento)
    const float b = bHz();
    v.tfc = b * exp2f(quant(24.0f * sinf(s.t2)) / 12.0f);
    v.tfm = b * exp2f(quant(12.0f * sinf(s.t1) + 12.0f) / 12.0f);
    v.tidx = 3.5f * fabsf(sinf(s.t1 - s.t2));
    float m = fabsf(s.w2) / (2.5f * w0); if (m > 1.f) m = 1.f;
    v.tamp = 0.25f + 0.75f * m;
    const float k = 0.02f;
    v.fc += k * (v.tfc - v.fc); v.fm += k * (v.tfm - v.fm); v.idx += k * (v.tidx - v.idx); v.amp += k * (v.tamp - v.amp);
  }
  inline float fmVoice(Voz& v) {
    v.pm += v.fm / sr_; if (v.pm >= 1.f) v.pm -= 1.f;
    v.pc += v.fc / sr_; if (v.pc >= 1.f) v.pc -= 1.f;
    return v.amp * sinT(v.pc + v.idx * sinT(v.pm) * (1.0f / TWO_PI_F));
  }

  // vuelco: la segunda masa pasa por arriba → una nota en la afinación (cada gemelo compone la suya)
  void vuelco(const Estado& s, float& prevT2, double& lastFlip, Nota& out, float w0) {
    const float t2w = wrapPi(s.t2);
    const bool flip = (prevT2 > 2.5f && t2w < -2.5f) || (prevT2 < -2.5f && t2w > 2.5f);
    prevT2 = t2w;
    if (flip && simT_ - lastFlip > 0.06) {
      lastFlip = simT_;
      out.on = true;
      out.hz = afin.snapHz(bHz() * 2.f * exp2f(quant(24.0f * sinf(s.t1)) / 12.0f));
      out.vel = 0.3f + 0.7f * fminf(1.f, fabsf(s.w2) / (3.f * w0));
    }
  }

  void physicsTick() {
    if (relReq_) { relReq_ = false; release(); }
    const float w0 = this->w0();
    if (kickReq_) { kickReq_ = false; A.w2 += 0.8f * kickAmt_ * w0; B.w2 += 0.8f * kickAmt_ * w0; }
    if (pertReq_) { pertReq_ = false; B.t2 += d0Grados * DEG; B.w2 += 0.05f * w0; delta0_ = fmaxf(delta, d0Grados * DEG); tau = 0; tauSplit = -1; reiniciarHist(); }
    if (syncReq_) { syncReq_ = false; B = A; delta0_ = d0Grados * DEG; B.t2 += delta0_; tau = 0; tauSplit = -1; reiniciarHist(); }
    const float g = w0 * w0;
    const float dt = tiempo / physRate_;
    {
      const float cd = cosf(A.t1 - A.t2);
      const float ke = 0.5f * (2.f * A.w1 * A.w1 + A.w2 * A.w2 + 2.f * A.w1 * A.w2 * cd);
      energiaG = ke / g - (2.f * cosf(A.t1) + cosf(A.t2));
    }
    float damp = -impulso * 0.06f * w0;
    if (energiaG > 4.5f) damp += 0.2f * w0 * (energiaG - 4.5f);   // el bombeo nunca pasa de 4.5 g
    if (energiaG < -2.9f && damp > 0) damp = 0;                    // no frena más allá del reposo
    // subpasos: con f0 alta (V/OCT) el paso se divide para que la física no se degrade
    const float fase = fabsf(w0 * dt) * (1.f + fmaxf(fabsf(A.w2), fabsf(B.w2)) / (w0 + 1e-6f));
    int nSub = (int)ceilf(fase / 0.25f); if (nSub < 1) nSub = 1; if (nSub > 16) nSub = 16;
    const float h = dt / nSub;
    for (int k = 0; k < nSub; ++k) { rk4(A, g, damp, h); rk4(B, g, damp, h); }
    if (!isfinite(A.w1) || !isfinite(A.w2) || !isfinite(B.w1) || !isfinite(B.w2)) { release(); return; }
    if (fabsf(A.t1) > 20.f) A.t1 = wrapPi(A.t1);
    if (fabsf(A.t2) > 20.f) A.t2 = wrapPi(A.t2);
    if (fabsf(B.t1) > 20.f) B.t1 = wrapPi(B.t1);
    if (fabsf(B.t2) > 20.f) B.t2 = wrapPi(B.t2);

    pxA_ = xA_; xA_ = 0.5f * (sinf(A.t1) + sinf(A.t2));
    pxB_ = xB_; xB_ = 0.5f * (sinf(B.t1) + sinf(B.t2));
    if (sonido == FM) { updateVoice(vA_, A, w0); updateVoice(vB_, B, w0); }
    simT_ += 1.0 / physRate_;
    vuelco(A, prevT2A_, lastFlipA_, notaA, w0);
    vuelco(B, prevT2B_, lastFlipB_, notaB, w0);

    // ---- divergencia y exponente de Lyapunov
    const float d1 = wrapPi(A.t1 - B.t1), d2 = wrapPi(A.t2 - B.t2);
    const float d3 = (A.w1 - B.w1) / w0, d4 = (A.w2 - B.w2) / w0;
    delta = sqrtf(d1 * d1 + d2 * d2 + d3 * d3 + d4 * d4);
    tau += (double)w0 * dt;
    const float tf = (float)tau;
    if (tauSplit < 0 && tf > 1.f && delta > 0 && delta0_ > 0) lambdaVivo = logf(delta / delta0_) / tf;   // exponente de tiempo finito
    if (tauSplit < 0 && delta > 0.5f && delta0_ > 0) {
      tauSplit = tf; lambda = logf(0.5f / delta0_) / fmaxf(tf, 1e-3f); lambdaVivo = lambda;
      horizonte = true;
    }
    if (tau >= nextHist_) {
      if (histCount >= HIST) {
        for (int i = 0; i < HIST / 2; ++i) hist[i] = hist[2 * i + 1];
        histCount = HIST / 2; histPaso *= 2.f;
      }
      nextHist_ = tau + histPaso;
      const int hh = histCount;
      hist[hh].logd = log10f(delta + 1e-7f); hist[hh].split = tauSplit > 0 ? 1 : 0;
      histCount = hh + 1;
    }
    if (regimen == DIVERGENCIA && autoSoltar && tauSplit > 0 && tf > tauSplit * 1.5f + 6.0f) release();

    // ---- traza para la pantalla (~48 puntos por periodo natural)
    int K = (int)(physRate_ / (fmaxf(f0, 0.01f) * 48.0f)); if (K < 1) K = 1;
    if (++decim_ >= K) {
      decim_ = 0;
      const int hd = trazaHead;
      PuntoTraza& p = traza[hd];
      p.xA = sinf(A.t1) + sinf(A.t2); p.yA = cosf(A.t1) + cosf(A.t2);
      p.xB = sinf(B.t1) + sinf(B.t2); p.yB = cosf(B.t1) + cosf(B.t2);
      p.logd = log10f(delta + 1e-7f); p.split = tauSplit > 0 ? 1 : 0;
      trazaHead = (hd + 1) % TRAZA;
      if (trazaCount < TRAZA) trazaCount = trazaCount + 1;
    }
  }
};

}  // namespace lyap
