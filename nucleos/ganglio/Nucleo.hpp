// Nucleo.hpp — GANGLIO · un cerebro de 8 neuronas que late solo y mueve el sonido
// Núcleo compartido (sin dependencias de plataforma): VCV Rack, MetaModule, Daisy y Cardputer.
//
// Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado
// Escuela de Artes y Humanidades, Universidad EAFIT
//
// En los insectos, ritmos como el canto del grillo los producen ganglios: pequeños grupos de neuronas
// acopladas. Aquí, una red recurrente de tiempo continuo (CTRNN) de 8 neuronas:
//
//     τ_i · dy_i/dt = −y_i + Σ_j W[i][j] · σ(y_j + θ_j) + I_i          σ(x) = 1 / (1 + e^−x)
//
// La salida de cada neurona, o_i = σ(y_i + θ_i), mueve un banco de síntesis:
//   1–4  volumen de 4 osciladores del bordón; su desafinación sale de la diferencia entre vecinas
//        (el batido aparece cuando dos neuronas «no están de acuerdo»)
//   5    índice de FM          6  plegador de onda: ganancia y realimentación
//   7    corte del filtro (150 Hz – 7,3 kHz)          8  resonancia
//
// Los 8 temperamentos son los del firmware Ganglio del Cardputer (Temperamentos.hpp). El cerebro corre a
// tasa de control (cada 32 muestras) y la síntesis interpola dentro de cada bloque: nunca hay saltos.
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "../comun/Salida.hpp"
#include "Temperamentos.hpp"

namespace ganglio {

using namespace espectros;

static const int N = 8;                       // neuronas
static const int BLOQUE = 32;                 // muestras por paso de control
static const int EEG = 256;                   // puntos del electroencefalograma (~9 s)

enum Filtro : int { LP = 0, BP = 1, LB = 2, N_FILTROS };
enum Acorde : int { QUINTA = 0, CUARTA, SEPTIMA, ARMONICOS, RACIMO, N_ACORDES };
enum Timbre : int { PURO = 0, FM2, CAMPANA, SUB, METAL, N_TIMBRES };

static const char* const NOMBRE_FILTRO[N_FILTROS] = {"LP", "BP", "LB"};
static const char* const NOMBRE_ACORDE[N_ACORDES] = {"QUINTA", "CUARTA", "SÉPTIMA", "ARMÓNICOS", "RACIMO"};
static const char* const NOMBRE_TIMBRE[N_TIMBRES] = {"PURO", "FM2", "CAMPANA", "SUB", "METAL"};

// razones de los 4 osciladores del bordón respecto a la tónica
static const float RAZONES[N_ACORDES][4] = {
  {1.f, 1.5f, 2.f, 3.f},                         // quinta justa y octavas
  {1.f, 4.f / 3.f, 2.f, 8.f / 3.f},              // cuarta justa
  {1.f, 1.25f, 1.5f, 1.75f},                     // séptima natural (armónicos 4:5:6:7)
  {1.f, 2.f, 3.f, 5.f},                          // armónicos de la tónica
  {1.f, 1.0293022f, 1.0905077f, 1.1224620f},     // racimo: 0, 50, 150 y 200 cents
};

// reparto estéreo de los 4 osciladores del bordón
static const float PANL[4] = {0.95f, 0.45f, 0.8f, 0.3f};
static const float PANR[4] = {0.3f, 0.8f, 0.45f, 0.95f};

// azar propio y reproducible (xorshift32)
struct Azar {
  uint32_t s = 0x9e3779b9u;
  inline uint32_t u32() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  inline float uni() { return (u32() >> 8) * (1.f / 16777216.f); }               // [0, 1)
  inline float bi() { return uni() * 2.f - 1.f; }                                  // [−1, 1)
  float gauss() { const float u = uni() + 1e-7f, v = uni(); return sqrtf(-2.f * logf(u)) * cosf(TWO_PI_F * v); }
};

static inline float sigma(float x) { return 1.f / (1.f + expf(-x)); }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }

// filtro de variable de estado TPT (Zavalishin): estable aunque se module rápido
struct SVF {
  float ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0;
  void ajustar(float sr, float fc, float q) {
    const float g = tanf(PI_F * clampf(fc, 20.f, 0.45f * sr) / sr), k = 1.f / q;
    a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
  }
  inline void process(float x, float& lp, float& bp) {
    const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2; lp = v2; bp = v1;
  }
};

class Nucleo {
 public:
  // ---------------------------------------------------------------- parámetros (los fija el adaptador)
  int temperamento = 0;          // 0–7; cambiarlo inicia una transición de 1,5 s
  float metabolismo = 1.f;       // ×0,125 a ×8: escala todo el tiempo de la red
  float intensidad = 1.f;        // 0,25–1,5: cuánto modula el cerebro a la síntesis
  float ruido = 0.5f;            // ruido sináptico: 0, 0,5, 1,5, 3
  float tonicaHz = 55.f;         // tónica del bordón
  int acorde = QUINTA;
  int filtro = LP;
  bool bordon = true;            // el bordón suena (la melodía sigue aunque no)
  bool escucha = true;           // el sonido vuelve al cerebro (neuronas 1 y 8)
  bool congelado = false;        // el cerebro se detiene; el sonido sigue
  float nivel = 1.f;             // volumen general
  float estimulo[N] = {};        // corriente externa por neurona, mientras dura (±4 ≈ muy fuerte)
  // escucha externa: si el adaptador tiene una entrada de audio, el cerebro escucha eso en vez del bordón
  bool escuchaExterna = false;
  float entrada = 0.f;           // muestra de audio externa (±1)

  // melodía (monofónica): cada nota también da un pulso a una neurona
  int timbre = PURO;
  void notaOn(float hz, float vel = 1.f) {
    mel_.hz = hz; mel_.vel = clampf(vel, 0.f, 1.f); mel_.gate = true; mel_.edad = 0.f;
    int n = (int)floorf(12.f * log2f(hz / 440.f) + 0.5f); n %= N; if (n < 0) n += N;
    pulso_[n] += 2.5f;
  }
  void notaOff() { mel_.gate = false; }
  void tonoMelodia(float hz) { mel_.hz = hz; }   // glissando sin volver a disparar

  // ---------------------------------------------------------------- estado que la interfaz dibuja
  float y[N] = {}, o[N] = {};                    // potencial y salida de cada neurona
  float W[N][N] = {}, tau[N] = {}, theta[N] = {}; // la red vigente (interpolada en las transiciones)
  float eeg[EEG][N] = {};                         // historia de las salidas, ~9 s
  int eegCabeza = 0, eegCuenta = 0;
  uint8_t cruces = 0;            // bit i: la neurona i cruzó hacia arriba en el último paso de control
  uint8_t activas = 0;           // bit i: la neurona i está alta (con histéresis)
  bool mutado = false;           // la red ya no es el temperamento de fábrica
  float despierta = 0.f;         // > 0 durante 2 s tras un despertar del vigía
  float nivelBordon = 0.f;       // envolvente del bordón (lo que escucha el cerebro)
  float transicion = 1.f;        // 0 → 1 durante el cambio de temperamento
  float fcVivo = 1000.f, qVivo = 1.f, fmVivo = 0.f, plegVivo = 1.f;

  void init(float sr) {
    sr_ = sr; isr_ = 1.f / sr;
    for (int i = 0; i <= TAB; ++i) seno_[i] = sinf(TWO_PI_F * i / TAB);
    salL_.init(sr); salR_.init(sr);
    svfL_ = SVF(); svfR_ = SVF();
    cargar(temperamento, true);
    for (int i = 0; i < N; ++i) { y[i] = 0.4f * azar_.bi(); o[i] = sigma(y[i] + theta[i]); }
    cuenta_ = 0; quietud_ = 0.f; eegT_ = 0.f; eegCabeza = eegCuenta = 0;
    for (int k = 0; k < 4; ++k) { fase_[k] = faseM_[k] = 0.f; fb_[k] = 0.f; }
    mapear(); for (int i = 0; i < NP; ++i) pAct_[i] = pObj_[i];
    svfL_.ajustar(sr_, fcVivo, qVivo); svfR_.ajustar(sr_, fcVivo, qVivo);
  }

  // cambiar la frecuencia de muestreo sin reiniciar el cerebro (el estado de la red se conserva)
  void frecuencia(float sr) {
    sr_ = sr; isr_ = 1.f / sr;
    salL_.init(sr); salR_.init(sr);
    svfL_.ajustar(sr_, fcVivo, qVivo); svfR_.ajustar(sr_, fcVivo, qVivo);
  }

  // guardar y recuperar la red vigente (pesos, τ y sesgos): una red mutada o nueva sobrevive al patch o a la SD
  static const int TAM_RED = N * N + 2 * N;
  void exportarRed(float* r) const {
    memcpy(r, W1_, sizeof W1_); memcpy(r + N * N, tau1_, sizeof tau1_); memcpy(r + N * N + N, th1_, sizeof th1_);
  }
  void importarRed(const float* r, bool esMutada = true) {
    memcpy(W1_, r, sizeof W1_); memcpy(tau1_, r + N * N, sizeof tau1_); memcpy(th1_, r + N * N + N, sizeof th1_);
    for (int i = 0; i < N; ++i) tau1_[i] = clampf(tau1_[i], 0.5f, 15.f);
    memcpy(W, W1_, sizeof W); memcpy(tau, tau1_, sizeof tau); memcpy(theta, th1_, sizeof theta);
    tempCargado_ = temperamento; transicion = 1.f; mutado = esMutada;
  }

  // ---------------------------------------------------------------- gestos
  void sacudir(float fuerza = 1.f) { for (int i = 0; i < N; ++i) y[i] += 3.f * fuerza * azar_.gauss(); }
  void mutar(float cuanto = 0.6f) {
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < N; ++j) if (i != j) W1_[i][j] += cuanto * azar_.gauss();
      tau1_[i] = clampf(tau1_[i] * expf(0.2f * azar_.gauss()), 0.5f, 15.f);
    }
    centrar(W1_, th1_);
    iniciarTransicion(); mutado = true;
  }
  void redNueva() {
    // 4 pares que oscilan solos (autoexcitación + acoplamiento antisimétrico), acoplados entre sí;
    // se prueban unas cuantas y se queda la más viva
    float mejor = -1.f;
    float Wc[N][N], tc[N], thc[N];
    for (int intento = 0; intento < 3; ++intento) {
      for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) Wc[i][j] = 3.5f * azar_.bi();
      for (int p = 0; p < 4; ++p) {
        const int a = 2 * p, b = a + 1;
        const float auto_ = 4.5f + 2.f * azar_.uni(), w = 1.5f + azar_.uni();
        Wc[a][a] = auto_; Wc[b][b] = auto_; Wc[a][b] = -w; Wc[b][a] = w;
        const float base = 0.5f * expf(logf(30.f) * azar_.uni());    // 0,5–15 s, logarítmica
        tc[a] = clampf(base * (1.f + 0.15f * azar_.bi()), 0.5f, 15.f);
        tc[b] = clampf(base * (1.f + 0.15f * azar_.bi()), 0.5f, 15.f);
      }
      centrar(Wc, thc);
      const float v = vitalidad(Wc, tc, thc);
      if (v > mejor) { mejor = v; memcpy(W1_, Wc, sizeof Wc); memcpy(tau1_, tc, sizeof tc); memcpy(th1_, thc, sizeof thc); }
    }
    iniciarTransicion(); mutado = true;
  }
  void restaurar() { cargar(temperamento, false); }   // volver al temperamento de fábrica

  // ---------------------------------------------------------------- una muestra
  void process(float& izq, float& der) {
    if (cuenta_ == 0) control();
    const float t = (float)(cuenta_ + 1) * (1.f / BLOQUE);   // interpolación dentro del bloque
    float p[NP];
    for (int i = 0; i < NP; ++i) p[i] = pIni_[i] + (pAct_[i] - pIni_[i]) * t;
    if (++cuenta_ >= BLOQUE) cuenta_ = 0;

    // bordón: 4 osciladores FM con plegador
    float l = 0.f, r = 0.f;
    if (bordon || nivelBordon > 1e-4f) {
      const float pleg = p[P_PLEG], fbk = p[P_FB], ind = p[P_FM];
      for (int k = 0; k < 4; ++k) {
        const float hz = frecuencia_[k];
        faseM_[k] += hz * isr_; faseM_[k] -= floorf(faseM_[k]);
        const float m = sen(faseM_[k]);
        fase_[k] += hz * isr_; fase_[k] -= floorf(fase_[k]);
        float s = sen(fase_[k] + ind * m * (1.f / TWO_PI_F) + fbk * fb_[k]);
        s = sen(0.25f * pleg * s);              // plegado suave: sin(π/2 · g · s)
        fb_[k] = s;
        const float a = p[P_A0 + k] * ganBordon_;
        l += a * s * PANL[k]; r += a * s * PANR[k];
      }
    }
    // filtro
    float lpL, bpL, lpR, bpR;
    svfL_.process(l, lpL, bpL); svfR_.process(r, lpR, bpR);
    float yl, yr;
    if (filtro == LP) { yl = lpL; yr = lpR; }
    else if (filtro == BP) { yl = bpL * 1.6f; yr = bpR * 1.6f; }
    else { yl = 0.6f * lpL + 0.9f * bpL; yr = 0.6f * lpR + 0.9f * bpR; }

    const float env = fabsf(yl) + fabsf(yr);
    nivelBordon += (env - nivelBordon) * (env > nivelBordon ? 0.002f : 0.0002f);
    if (escuchaExterna) { const float e = fabsf(entrada); envExt_ += (e - envExt_) * (e > envExt_ ? 0.002f : 0.0002f); }

    // melodía
    const float mel = melodia();
    izq = salL_((yl + mel) * nivel);
    der = salR_((yr + mel) * nivel);
  }

  const char* nombreTemperamento() const { return TEMPERAMENTOS[temperamento].nombre; }
  const char* caracterTemperamento() const { return TEMPERAMENTOS[temperamento].caracter; }

 private:
  static const int TAB = 1024;
  enum { P_A0 = 0, P_A1, P_A2, P_A3, P_FM, P_PLEG, P_FB, NP };

  float sr_ = 48000.f, isr_ = 1.f / 48000.f;
  float seno_[TAB + 1];
  Azar azar_;
  Salida salL_, salR_;
  SVF svfL_, svfR_;
  int cuenta_ = 0, tempCargado_ = -1;
  // transición entre redes
  float W0_[N][N], tau0_[N], th0_[N], W1_[N][N], tau1_[N], th1_[N];
  float pulso_[N] = {};
  float quietud_ = 0.f, eegT_ = 0.f, envExt_ = 0.f, ganBordon_ = 0.f, oidoLento_ = 0.f;
  float oPrev_[N] = {};
  // síntesis
  float pIni_[NP] = {}, pAct_[NP] = {}, pObj_[NP] = {};
  float frecuencia_[4] = {55.f, 82.5f, 110.f, 165.f};
  float fase_[4], faseM_[4], fb_[4];
  struct Melodia { float hz = 440.f, vel = 1.f, env = 0.f, edad = 0.f, fase = 0.f, faseM = 0.f, faseS = 0.f; bool gate = false; } mel_;

  inline float sen(float fase) const {           // seno por tabla, fase en vueltas
    fase -= floorf(fase);
    const float x = fase * TAB; const int i = (int)x; const float f = x - i;
    return seno_[i] + (seno_[i + 1] - seno_[i]) * f;
  }

  static void centrar(float Wm[N][N], float th[N]) {   // sesgos center-crossing
    for (int i = 0; i < N; ++i) { float s = 0.f; for (int j = 0; j < N; ++j) s += Wm[i][j]; th[i] = -0.5f * s; }
  }

  void cargar(int k, bool inmediato) {
    k = k < 0 ? 0 : (k >= N_TEMPERAMENTOS ? N_TEMPERAMENTOS - 1 : k);
    const Temperamento& t = TEMPERAMENTOS[k];
    memcpy(W1_, t.W, sizeof W1_); memcpy(tau1_, t.tau, sizeof tau1_); memcpy(th1_, t.theta, sizeof th1_);
    tempCargado_ = k; temperamento = k; mutado = false;
    if (inmediato) { memcpy(W, W1_, sizeof W); memcpy(tau, tau1_, sizeof tau); memcpy(theta, th1_, sizeof theta); transicion = 1.f; }
    else iniciarTransicion();
  }
  void iniciarTransicion() {
    memcpy(W0_, W, sizeof W0_); memcpy(tau0_, tau, sizeof tau0_); memcpy(th0_, theta, sizeof th0_);
    transicion = 0.f;
  }

  // vitalidad de una red candidata: cuánto se mueve su salida en 60 s simulados (fuera del audio continuo)
  float vitalidad(float Wc[N][N], const float tc[N], const float thc[N]) {
    float yy[N], oo[N], mn = 1.f, mx = 0.f;
    for (int i = 0; i < N; ++i) { yy[i] = 0.5f * azar_.bi(); }
    const float dt = 0.08f;
    for (int n = 0; n < 750; ++n) {
      for (int i = 0; i < N; ++i) oo[i] = sigma(yy[i] + thc[i]);
      for (int i = 0; i < N; ++i) { float s = 0.f; for (int j = 0; j < N; ++j) s += Wc[i][j] * oo[j]; yy[i] += dt * (s - yy[i]) / tc[i]; }
      if (n > 375) { mn = fminf(mn, oo[0]); mx = fmaxf(mx, oo[0]); }
    }
    return mx - mn;
  }

  // ---------------------------------------------------------------- cerebro, cada BLOQUE muestras
  void control() {
    const float dtReal = BLOQUE * isr_;
    if (temperamento != tempCargado_) cargar(temperamento, false);

    // transición de 1,5 s entre redes
    if (transicion < 1.f) {
      transicion = fminf(1.f, transicion + dtReal / 1.5f);
      const float a = transicion * transicion * (3.f - 2.f * transicion);
      for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) W[i][j] = W0_[i][j] + (W1_[i][j] - W0_[i][j]) * a;
        tau[i] = tau0_[i] + (tau1_[i] - tau0_[i]) * a;
        theta[i] = th0_[i] + (th1_[i] - th0_[i]) * a;
      }
    }

    // corrientes de entrada: estímulo, pulsos de la melodía y escucha
    float I[N];
    const float decPulso = expf(-dtReal / 0.3f);
    for (int i = 0; i < N; ++i) { I[i] = estimulo[i] + pulso_[i]; pulso_[i] *= decPulso; }
    // escucha: el cerebro oye los CAMBIOS del sonido (respecto a su propio promedio de ~4 s), no su nivel
    // absoluto; así el lazo sonido → red empuja sin clavar a las neuronas
    {
      const float oido = escuchaExterna ? envExt_ : nivelBordon;
      oidoLento_ += (oido - oidoLento_) * (1.f - expf(-dtReal / 4.f));
      if (escucha) {
        const float c = clampf((oido - oidoLento_) / (oidoLento_ + 0.05f), -1.f, 1.f);
        I[0] += 1.5f * c;
        I[7] -= 1.5f * c;
      }
    }

    // integración (Euler–Maruyama); el metabolismo escala el tiempo de la red
    if (!congelado) {
      const float dt = dtReal * clampf(metabolismo, 0.125f, 8.f);
      const float sq = sqrtf(dt);
      for (int i = 0; i < N; ++i) o[i] = sigma(y[i] + theta[i]);
      for (int i = 0; i < N; ++i) {
        float s = I[i];
        for (int j = 0; j < N; ++j) s += W[i][j] * o[j];
        y[i] += dt * (s - y[i]) / tau[i];
        if (ruido > 0.f) y[i] += ruido * sq * azar_.gauss() / tau[i];
        y[i] = clampf(y[i], -30.f, 30.f);
      }
    }
    float mov = 0.f;
    cruces = 0;
    for (int i = 0; i < N; ++i) {
      o[i] = sigma(y[i] + theta[i]);
      mov += fabsf(o[i] - oPrev_[i]); oPrev_[i] = o[i];
      if (!(activas & (1 << i)) && o[i] > 0.6f) { activas |= (1 << i); cruces |= (1 << i); }
      else if ((activas & (1 << i)) && o[i] < 0.4f) activas &= ~(1 << i);
    }

    // vigía: si la red se queda quieta más de 10 s, la despierta
    if (!congelado) {
      if (mov < 0.002f * dtReal * 60.f) quietud_ += dtReal; else quietud_ = 0.f;
      if (quietud_ > 10.f) { sacudir(1.f); quietud_ = 0.f; despierta = 2.f; }
    }
    if (despierta > 0.f) despierta -= dtReal;

    // electroencefalograma: ~9 s de historia
    eegT_ += dtReal;
    if (eegT_ >= 9.f / EEG) {
      eegT_ -= 9.f / EEG;
      for (int i = 0; i < N; ++i) eeg[eegCabeza][i] = o[i];
      eegCabeza = (eegCabeza + 1) % EEG; if (eegCuenta < EEG) ++eegCuenta;
    }

    mapear();
    // la síntesis sigue al cerebro con suavizado (~8 ms) e interpolación dentro del bloque
    const float a = 1.f - expf(-dtReal / 0.008f);
    for (int i = 0; i < NP; ++i) { pIni_[i] = pAct_[i]; pAct_[i] += (pObj_[i] - pAct_[i]) * a; }
    svfL_.ajustar(sr_, fcVivo, qVivo); svfR_.ajustar(sr_, fcVivo, qVivo);
    ganBordon_ += ((bordon ? 0.32f : 0.f) - ganBordon_) * (1.f - expf(-dtReal / 0.05f));
  }

  // de las neuronas a la síntesis
  void mapear() {
    float oi[N];
    const float in = clampf(intensidad, 0.25f, 1.5f);
    for (int i = 0; i < N; ++i) oi[i] = clampf(0.5f + (o[i] - 0.5f) * in, 0.f, 1.f);
    const int ac = acorde < 0 ? 0 : (acorde >= N_ACORDES ? N_ACORDES - 1 : acorde);
    for (int k = 0; k < 4; ++k) {
      pObj_[P_A0 + k] = oi[k] * sqrtf(oi[k]);
      const float cents = (oi[k] - oi[(k + 1) & 3]) * 14.f;          // batido entre vecinas
      frecuencia_[k] = tonicaHz * RAZONES[ac][k] * exp2f(cents / 1200.f);
    }
    const float agudo = clampf(300.f / (tonicaHz * RAZONES[ac][3]), 0.3f, 1.f);   // menos plegado en lo agudo
    fmVivo = oi[4] * 4.5f * clampf(500.f / tonicaHz, 0.4f, 1.f);
    plegVivo = 1.f + oi[5] * 5.f * agudo;
    pObj_[P_FM] = fmVivo;
    pObj_[P_PLEG] = plegVivo;
    pObj_[P_FB] = oi[5] * 0.12f * agudo;
    fcVivo = 150.f * expf(logf(7300.f / 150.f) * oi[6]);
    qVivo = 0.55f + oi[7] * 7.f;
  }

  // voz de la melodía: cinco timbres, envolvente AR (la campana decae sola)
  float melodia() {
    Melodia& m = mel_;
    const float ataque = 1.f - expf(-isr_ / 0.004f);
    const float caida = 1.f - expf(-isr_ / (timbre == CAMPANA ? 1.4f : 0.35f));
    const float obj = (m.gate && !(timbre == CAMPANA && m.edad > 0.02f)) ? m.vel : 0.f;
    m.env += (obj - m.env) * (obj > m.env ? ataque : caida);
    m.edad += isr_;
    if (m.env < 1e-5f) return 0.f;
    const float hz = m.hz;
    m.fase += hz * isr_; m.fase -= floorf(m.fase);
    float s;
    switch (timbre) {
      case FM2: m.faseM += 2.f * hz * isr_; m.faseM -= floorf(m.faseM); s = sen(m.fase + 0.24f * sen(m.faseM)); break;
      case CAMPANA: {
        m.faseM += 3.5f * hz * isr_; m.faseM -= floorf(m.faseM);
        const float idx = 0.5f * expf(-m.edad * 2.5f) + 0.05f;
        s = sen(m.fase + idx * sen(m.faseM)); break;
      }
      case SUB: m.faseS += 0.5f * hz * isr_; m.faseS -= floorf(m.faseS); s = 0.6f * sen(m.fase) + 0.55f * sen(m.faseS); break;
      case METAL: m.faseM += 1.4142f * hz * isr_; m.faseM -= floorf(m.faseM); s = sen(m.fase + 0.4f * sen(m.faseM)); break;
      default: s = sen(m.fase);
    }
    return 0.28f * m.env * s;
  }
};

}  // namespace ganglio
