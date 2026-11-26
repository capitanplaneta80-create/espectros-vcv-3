// Nucleo.hpp — RYDBERG · el átomo de hidrógeno como instrumento espectral
// Núcleo compartido (sin dependencias de plataforma): VCV Rack, MetaModule, Daisy y Cardputer.
//
// Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado
// Escuela de Artes y Humanidades, Universidad EAFIT
//
// Niveles E_n = −R/n². Cada salto n → n' emite un fotón de frecuencia proporcional a 1/n'² − 1/n²:
// la escala es el espectro del hidrógeno (Lyman, Balmer, Paschen…), nunca el temperamento igual.
//   · reglas de selección Δl = ±1 y cascadas al azar ponderadas por ν³
//   · el nivel 2s es metaestable y decae con dos fotones: un espectro continuo (se oye como ruido)
//   · estructura fina (dobletes) y efecto Zeeman normal (tripletes σ−, π, σ+)
// Tres modos: 1 cascada (cada fotón suena) · 2 espectro (las líneas más emitidas quedan como bordones)
// · 3 Zeeman (el campo B abre cada línea en tres).
//
// Port del firmware RYDBERG de la suite ESPECTROS del Cardputer (v2, sin saturación). Cambios del port:
//   · estéreo: cada átomo tiene su lugar en el campo, y el triplete Zeeman se abre en el espacio
//   · osciladores por rotación de fasor (sin tabla de seno) y reverberación FDN más corta: cabe en 32 KB
//   · salida por comun/Salida.hpp (paso alto, compresor suave y limitador con anticipación)
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "../comun/Salida.hpp"

namespace rydberg {

using namespace espectros;

static const int NMAX = 12;          // nivel más alto
static const int NATOMO = 4;
static const int NPOOL = 64;         // parciales del banco modal
static const int NDRONE = 24;
static const int NFOTON = 32;        // fotones recientes (para la pantalla)
static const float NU_HALFA = 0.25f - 1.0f / 9.0f;   // H-alfa: 3 → 2

enum Modo : int { CASCADA = 0, ESPECTRO = 1, ZEEMAN = 2, N_MODOS };
static const char* const NOMBRE_MODO[N_MODOS] = {"CASCADA", "ESPECTRO", "ZEEMAN"};

inline float nu(int hi, int lo) { return 1.0f / (float)(lo * lo) - 1.0f / (float)(hi * hi); }
inline float lambdaNm(int hi, int lo) { return 91.1267f / nu(hi, lo); }        // longitud de onda real
inline float energia(int n) { return 1.0f - 1.0f / (float)(n * n); }           // 0 = fundamental, 1 = ionización

struct Atomo { int n = 1, l = 0, m = 0; float espera = 0.5f; };
struct Foton { int8_t hi = 0, lo = 0; uint8_t atomo = 0; float t = -10.f; };

// generador propio y reproducible (regla 6 del contrato)
struct Azar {
  uint32_t s = 0x12345678u;
  inline uint32_t sig() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  inline float uni() { return (sig() >> 8) * (1.0f / 16777216.0f); }
  inline float bi() { return uni() * 2.f - 1.f; }
  inline float expo(float tasa) { return -logf(uni() + 1e-7f) / tasa; }
};

// normalizador de densidad: si los eventos se superponen, cada uno se escala por 1/√superposición
// (suma de eventos independientes): el nivel total queda estable y los acentos se conservan
struct Densidad {
  float acc = 0, tau = 2.f;
  inline void evento(float w = 1.f) { acc += w; }
  inline void decae(float dt) { acc *= expf(-dt / tau); }
  inline float norma(float solape, float n0) const {
    const float N = acc / tau * solape / n0;
    return N > 1.f ? 1.f / sqrtf(N) : 1.f;
  }
};

class Nucleo {
 public:
  // ---- parámetros (unidades físicas)
  int modo = CASCADA;
  int nmax = 6;                // nivel máximo de excitación (2–12)
  float temperatura = 1.2f;    // tasa de excitación desde el fundamental (0,1–12 por segundo)
  float campo = 0.f;           // campo magnético B (0–1): separación Zeeman
  float refHz = 440.f;         // altura de H-alfa; el resto del espectro es proporcional
  int atomos = 2;              // 1–4
  bool fina = true;            // estructura fina: dobletes
  float espacio = 0.28f;       // mezcla de la reverberación
  float nivel = 1.f;

  // ---- lecturas (para salidas y pantalla)
  Atomo atomo[NATOMO];
  float intensidad[NMAX + 1][NMAX + 1];   // espectro acumulado [hi][lo], se olvida en ~25 s
  Foton fotones[NFOTON];
  int fotonCabeza = 0;
  float tiempo = 0.f;
  uint32_t emitidos = 0;       // cuenta de fotones: cambia en cada emisión
  uint32_t dobles = 0;         // cuenta de decaimientos 2s → 1s (dos fotones, continuo)
  int ultimoHi = 3, ultimoLo = 2, ultimoAtomo = 0;
  float ultimoHz = 440.f;
  float continuo = 0.f;        // nivel del continuo de dos fotones

  float lineaHz(int hi, int lo) const { return refHz * nu(hi, lo) / NU_HALFA; }

  void init(float sr) {
    sr_ = sr;
    ctrl_ = (int)(sr / 500.f); if (ctrl_ < 1) ctrl_ = 1;
    cnt_ = 0;
    memset(pool_, 0, sizeof pool_);
    for (int i = 0; i < NDRONE; ++i) { drones_[i] = Drone(); drones_[i].ampGlide = 1.f - expf(-1.f / (0.0625f * sr)); }
    memset(intensidad, 0, sizeof intensidad);
    for (int i = 0; i < NATOMO; ++i) { atomo[i] = Atomo(); atomo[i].espera = 0.3f + 0.7f * i; }
    for (int i = 0; i < NFOTON; ++i) fotones[i] = Foton();
    rev_.limpiar();
    salL_.init(sr, 30.f); salR_.init(sr, 30.f);
    salL_.trim = salR_.trim = TRIM;
    dens_ = Densidad();
    ruido_ = ruidoL_ = ruidoR_ = 0;
    dronT_ = 0;
  }

  // gesto: los átomos en el fundamental absorben ya; si ninguno está ahí, el primero salta ya
  void excitar() {
    bool alguno = false;
    for (int a = 0; a < atomos; ++a) if (atomo[a].n == 1) { atomo[a].espera = 0; alguno = true; }
    if (!alguno) atomo[0].espera = 0;
  }

  // una muestra estéreo (±1 aprox.; nunca pasa de 0,98)
  void process(float& izq, float& der) {
    if (cnt_ == 0) control();
    if (++cnt_ >= ctrl_) cnt_ = 0;
    float l = 0, r = 0;
    for (int i = 0; i < NPOOL; ++i) {
      Parcial& q = pool_[i];
      if (q.a < 2e-5f) continue;
      const float re = q.re * q.c - q.im * q.s;
      q.im = q.re * q.s + q.im * q.c; q.re = re;
      const float v = (q.a - q.b) * q.im;
      l += v * q.gl; r += v * q.gr;
      q.a *= q.da; q.b *= q.db;
      if (q.b < 1e-9f) q.b = 0.f;   // el ataque ya terminó: sin números subnormales (lentísimos en x86)
    }
    l *= 0.6f; r *= 0.6f;
    for (int i = 0; i < NDRONE; ++i) {
      Drone& d = drones_[i];
      if (d.amp < 1e-4f && d.ampT <= 0.f) continue;
      d.amp += d.ampGlide * (d.ampT - d.amp);
      const float re = d.re * d.c - d.im * d.s;
      d.im = d.re * d.s + d.im * d.c; d.re = re;
      const float v = d.amp * d.im;
      l += v * d.gl; r += v * d.gr;
    }
    if (ruido_ > 1e-4f) {   // continuo de dos fotones, ancho (dos ruidos independientes)
      ruidoL_ += 0.08f * (azar_.bi() - ruidoL_);
      ruidoR_ += 0.08f * (azar_.bi() - ruidoR_);
      l += ruido_ * ruidoL_; r += ruido_ * ruidoR_;
      ruido_ *= ruidoDecae_;
    }
    continuo = ruido_;
    float rl, rr;
    rev_.procesar(0.5f * (l + r), rl, rr);
    const float seco = 1.f - espacio;
    izq = salL_((l * seco + espacio * rl) * nivel);
    der = salR_((r * seco + espacio * rr) * nivel);
  }

 private:
  static constexpr float TRIM = 0.8f;   // el mismo ajuste de nivel del firmware v2

  struct Parcial { float re, im, c, s, a, b, da, db, gl, gr; };
  struct Drone {
    float re = 0, im = 0, c = 1, s = 0, hz = 110, objetivo = 110, amp = 0, ampT = 0, gl = 1, gr = 1;
    float glide = 0.062f, ampGlide = 0.0005f;   // glissando por paso de control (2 ms) y volumen por muestra (~60 ms)
  };

  // reverberación FDN de 4 líneas (Householder) con amortiguación; dos salidas decorrelacionadas
  struct Reverb {
    static const int L0 = 1123, L1 = 1367, L2 = 1627, L3 = 1999;
    float b0[L0], b1[L1], b2[L2], b3[L3];
    int i0 = 0, i1 = 0, i2 = 0, i3 = 0;
    float l0 = 0, l1 = 0, l2 = 0, l3 = 0, fb = 0.83f, damp = 0.35f;
    void limpiar() {
      memset(b0, 0, sizeof b0); memset(b1, 0, sizeof b1); memset(b2, 0, sizeof b2); memset(b3, 0, sizeof b3);
      i0 = i1 = i2 = i3 = 0; l0 = l1 = l2 = l3 = 0;
    }
    inline void procesar(float x, float& yl, float& yr) {
      const float o0 = b0[i0], o1 = b1[i1], o2 = b2[i2], o3 = b3[i3];
      l0 += damp * (o0 - l0); l1 += damp * (o1 - l1); l2 += damp * (o2 - l2); l3 += damp * (o3 - l3);
      const float h = 0.5f * (l0 + l1 + l2 + l3), in = 0.35f * x;
      b0[i0] = (l0 - h) * fb + in; b1[i1] = (l1 - h) * fb + in;
      b2[i2] = (l2 - h) * fb + in; b3[i3] = (l3 - h) * fb + in;
      if (++i0 >= L0) i0 = 0;
      if (++i1 >= L1) i1 = 0;
      if (++i2 >= L2) i2 = 0;
      if (++i3 >= L3) i3 = 0;
      yl = 1.2f * (o0 + o2); yr = 1.2f * (o1 + o3);
    }
  };

  float sr_ = 48000.f;
  int ctrl_ = 96, cnt_ = 0;
  Parcial pool_[NPOOL];
  Drone drones_[NDRONE];
  Reverb rev_;
  Salida salL_, salR_;
  Densidad dens_;
  Azar azar_;
  float ruido_ = 0, ruidoL_ = 0, ruidoR_ = 0, ruidoDecae_ = 0.9999f, dronT_ = 0;

  static void paneo(float p, float& gl, float& gr) {    // potencia constante, 1 en el centro
    const float th = 0.78539816f * (1.f + p);
    gl = 1.41421356f * cosf(th); gr = 1.41421356f * sinf(th);
  }
  static float lugar(int atomo) {                        // cada átomo tiene su lugar en el estéreo
    static const float P[NATOMO] = {-0.45f, 0.45f, -0.15f, 0.15f};
    return P[atomo & 3];
  }

  void golpe(float hz, float amp, float decae, float pan, float ataque = 0.002f) {
    if (hz < 15.f || hz > sr_ * 0.45f || amp <= 1e-5f) return;
    int mejor = 0; float bajo = 1e9f;
    for (int i = 0; i < NPOOL; ++i) if (pool_[i].a < bajo) { bajo = pool_[i].a; mejor = i; }
    Parcial& q = pool_[mejor];
    const float w = TWO_PI_F * hz / sr_;
    q.re = 1.f; q.im = 0.f; q.c = cosf(w); q.s = sinf(w);
    q.a = q.b = amp;
    q.da = expf(-1.0f / (decae * sr_));
    q.db = expf(-1.0f / (ataque * sr_));
    paneo(pan, q.gl, q.gr);
  }

  void foton(int a, int hi, int lo) {
    const float f = lineaHz(hi, lo);
    dens_.evento();
    const float amp = dens_.norma(1.2f, 4.f) * (modo == CASCADA ? 0.32f : 0.2f) * (0.6f + 0.4f / (1.f + 0.3f * (hi - lo)));
    const float dec = 0.6f + 1.8f / (1.f + nu(hi, lo) * 2.f);
    const float p = lugar(a);
    if (modo == ZEEMAN && campo > 0.001f) {
      const float d = campo * 24.f;   // Zeeman normal: desplazamiento constante en Hz
      golpe(f - d, amp * 0.55f, dec, p - 0.35f);
      golpe(f, amp * 0.7f, dec, p);
      golpe(f + d, amp * 0.55f, dec, p + 0.35f);
    } else {
      golpe(f, amp, dec, p);
      if (fina) golpe(f * (1.0f + 0.0016f / (hi * 0.5f)), amp * 0.45f, dec, -p);   // doblete
    }
    intensidad[hi][lo] += 1.f;
    Foton& ph = fotones[fotonCabeza];
    ph.hi = (int8_t)hi; ph.lo = (int8_t)lo; ph.t = tiempo; ph.atomo = (uint8_t)a;
    fotonCabeza = (fotonCabeza + 1) % NFOTON;
    ultimoHi = hi; ultimoLo = lo; ultimoAtomo = a; ultimoHz = f;
    ++emitidos;
  }

  void paso(int a) {
    Atomo& at = atomo[a];
    if (at.n == 1) {
      // absorción: sube a un nivel al azar (más probable cuanto más bajo)
      const int top = nmax < 2 ? 2 : (nmax > NMAX ? NMAX : nmax);
      float wsum = 0, w[NMAX + 1];
      for (int n = 2; n <= top; ++n) { w[n] = 1.f / n; wsum += w[n]; }
      float u = azar_.uni() * wsum;
      int nn = 2;
      for (int n = 2; n <= top; ++n) { u -= w[n]; if (u <= 0) { nn = n; break; } }
      at.n = nn;
      at.l = (int)(azar_.uni() * nn);
      if (at.l >= nn) at.l = nn - 1;
      at.m = at.l ? (int)(azar_.uni() * (2 * at.l + 1)) - at.l : 0;
      at.espera = 0.08f + azar_.expo(4.f + 30.f / (nn * nn));
      return;
    }
    // emisión: transiciones permitidas Δl = ±1, ponderadas por ν³
    float wsum = 0, w[2 * NMAX];
    int cand[2 * NMAX][2];
    int nc = 0;
    for (int lo = 1; lo < at.n; ++lo) {
      for (int dl = -1; dl <= 1; dl += 2) {
        const int l2 = at.l + dl;
        if (l2 < 0 || l2 >= lo) continue;
        const float v = nu(at.n, lo);
        w[nc] = v * v * v * (dl < 0 ? 1.4f : 1.f);
        cand[nc][0] = lo; cand[nc][1] = l2;
        wsum += w[nc]; ++nc;
      }
    }
    if (nc == 0) {
      if (at.n == 2 && at.l == 0) {   // 2s metaestable: dos fotones → espectro continuo
        ruido_ = (modo == CASCADA ? 0.12f : 0.07f);
        ruidoDecae_ = expf(-1.f / (0.35f * sr_));
        at.n = 1; at.l = 0; at.m = 0;
        at.espera = azar_.expo(temperatura);
        ++dobles;
        return;
      }
      at.l = 0;   // reorientación colisional
      at.espera = 0.05f;
      return;
    }
    float u = azar_.uni() * wsum;
    int k = 0;
    for (; k < nc - 1; ++k) { u -= w[k]; if (u <= 0) break; }
    const int lo = cand[k][0];
    foton(a, at.n, lo);
    at.n = lo; at.l = cand[k][1];
    at.m = at.l ? (int)(azar_.uni() * (2 * at.l + 1)) - at.l : 0;
    if (at.n == 1) at.espera = azar_.expo(temperatura);
    else if (at.n == 2 && at.l == 0) at.espera = 0.4f + azar_.expo(1.2f);
    else at.espera = 0.05f + azar_.expo(2.f + 400.f * wsum);
  }

  void control() {
    const float dt = (float)ctrl_ / sr_;
    tiempo += dt;
    dens_.decae(dt);
    if (temperatura < 0.05f) temperatura = 0.05f;
    const int na = atomos < 1 ? 1 : (atomos > NATOMO ? NATOMO : atomos);
    for (int a = 0; a < na; ++a) {
      atomo[a].espera -= dt;
      if (atomo[a].espera <= 0) paso(a);
    }
    // el espectro acumulado se olvida lentamente (~25 s)
    const float olvido = expf(-dt / 25.f);
    for (int h = 2; h <= NMAX; ++h) for (int l = 1; l < h; ++l) intensidad[h][l] *= olvido;
    // bordones: glissando a tasa de control y fasor renormalizado
    for (int i = 0; i < NDRONE; ++i) {
      Drone& d = drones_[i];
      if (d.amp < 1e-4f && d.ampT <= 0.f) continue;
      d.hz += d.glide * (d.objetivo - d.hz);
      const float w = TWO_PI_F * d.hz / sr_;
      d.c = cosf(w); d.s = sinf(w);
      const float m = sqrtf(d.re * d.re + d.im * d.im);
      if (m > 1e-6f) { d.re /= m; d.im /= m; } else { d.re = 1.f; d.im = 0.f; }
    }
    dronT_ -= dt;
    if (dronT_ <= 0) { dronT_ = 0.05f; actualizarBordones(); }
  }

  // ESPECTRO / ZEEMAN: bordones con las líneas más intensas
  void actualizarBordones() {
    const int nd = (modo == ZEEMAN) ? 8 : (modo == ESPECTRO ? 12 : 0);
    int bh[12], bl[12]; float bi[12];
    int hallados = 0;
    for (int k = 0; k < nd; ++k) {
      float mejor = 0.05f; int h0 = 0, l0 = 0;
      for (int h = 2; h <= NMAX; ++h) for (int l = 1; l < h; ++l) {
        bool usado = false;
        for (int j = 0; j < hallados; ++j) usado |= (bh[j] == h && bl[j] == l);
        if (!usado && intensidad[h][l] > mejor) { mejor = intensidad[h][l]; h0 = h; l0 = l; }
      }
      if (!h0) break;
      bh[hallados] = h0; bl[hallados] = l0; bi[hallados] = mejor; ++hallados;
    }
    float total = 0;
    for (int j = 0; j < hallados; ++j) total += bi[j];
    for (int d = 0; d < NDRONE; ++d) drones_[d].ampT = 0;
    for (int j = 0; j < hallados; ++j) {
      const float f = lineaHz(bh[j], bl[j]);
      const float a = 0.16f * sqrtf(bi[j] / (total + 1e-6f));
      const float p = (j & 1 ? 1.f : -1.f) * (0.1f + 0.06f * j);   // las líneas se abren en abanico
      if (modo == ZEEMAN) {
        const float dz = campo * 24.f;
        for (int s = 0; s < 3; ++s) {
          Drone& d = drones_[j * 3 + s];
          d.objetivo = f + (s - 1) * dz; d.ampT = a * (s == 1 ? 0.6f : 0.45f);
          if (d.amp < 1e-4f) d.hz = d.objetivo;
          paneo(p + (s - 1) * 0.3f, d.gl, d.gr);
        }
      } else {
        Drone& d = drones_[j];
        d.objetivo = f; d.ampT = a;
        if (d.amp < 1e-4f) d.hz = f;
        paneo(p, d.gl, d.gr);
      }
    }
  }
};

}  // namespace rydberg
