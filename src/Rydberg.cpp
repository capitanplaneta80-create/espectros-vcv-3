// RYDBERG — el átomo de hidrógeno como instrumento espectral · ESPECTROS para VCV Rack
// Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado
// Escuela de Artes y Humanidades, Universidad EAFIT
//
// Hasta cuatro átomos de hidrógeno se excitan y caen en cascada por sus niveles. Cada salto emite un
// fotón y cada fotón es una nota: la afinación es el espectro del hidrógeno (Lyman, Balmer, Paschen…).
// Adaptador delgado: perillas y voltajes → nucleos/rydberg/Nucleo.hpp; la pantalla dibuja lo que el
// núcleo expone (la nube |ψnlm|² del primer átomo, la escalera de niveles y el espectro acumulado).
#include "plugin.hpp"
#include "rydberg/Nucleo.hpp"
#include <cmath>

using namespace rydberg;

struct Rydberg : Module {
	enum ParamId {
		NMAX_PARAM, TEMP_PARAM, CAMPO_PARAM, HALFA_PARAM, ATOMOS_PARAM, ESPACIO_PARAM,
		MODO_PARAM, FINA_PARAM, NIVEL_PARAM, EXCITAR_PARAM, OLVIDAR_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT, NMAX_INPUT, TEMP_INPUT, CAMPO_INPUT, ATOMOS_INPUT, ESPACIO_INPUT, MODO_INPUT, NIVEL_INPUT,
		EXCITAR_INPUT, OLVIDAR_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		FOTON_VOCT_OUTPUT, FOTON_OUTPUT, SERIE_OUTPUT, NU_OUTPUT, NIVELES_OUTPUT, CONTINUO_OUTPUT, LINEAS_OUTPUT,
		L_OUTPUT, R_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId { EXCITAR_LIGHT, OLVIDAR_LIGHT, FOTON_LIGHT, CONTINUO_LIGHT, LIGHTS_LEN };

	Nucleo nucleo;
	dsp::ClockDivider control;
	dsp::SchmittTrigger trig[2];
	dsp::BooleanTrigger boton[2];
	dsp::PulseGenerator pulsoLuz[2], pulsoFoton;
	uint32_t vistos = 0;
	float fotonVoct = 0.f, serie = 2.f, nuV = 0.f;
	int nLineas = 0;
	float lineas[8] = {};

	Rydberg() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		auto* p = configParam(NMAX_PARAM, 2.f, 12.f, 6.f, "Nivel máximo n");
		p->snapEnabled = true;
		p->description = "Hasta dónde se excitan los átomos. Más alto = cascadas más largas, más líneas y fotones más graves (Paschen, Brackett…).";
		p = configParam(TEMP_PARAM, std::log2(0.1f), std::log2(12.f), std::log2(1.2f), "Temperatura · excitaciones por segundo", "/s", 2.f);
		p->description = "Cada cuánto vuelve a excitarse un átomo que llegó al fundamental: la densidad de las cascadas.";
		p = configParam(CAMPO_PARAM, 0.f, 1.f, 0.3f, "Campo magnético B", "%", 0.f, 100.f);
		p->description = "En el modo Zeeman abre cada línea en un triplete (σ−, π, σ+), separado en frecuencia y en el espacio estéreo.";
		p = configParam(HALFA_PARAM, std::log2(55.f), std::log2(1760.f), std::log2(440.f), "Altura de H-alfa", " Hz", 2.f);
		p->description = "La línea roja de Balmer (3 → 2). Todo el espectro se transpone con ella; V/OCT se suma aquí.";
		p = configParam(ATOMOS_PARAM, 1.f, 4.f, 2.f, "Átomos");
		p->snapEnabled = true;
		p->description = "Cuántos átomos caen a la vez. Cada uno tiene su lugar en el estéreo.";
		p = configParam(ESPACIO_PARAM, 0.f, 1.f, 0.28f, "Espacio · reverberación", "%", 0.f, 100.f);
		configSwitch(MODO_PARAM, 0.f, 2.f, 0.f, "Modo", {"Cascada: cada fotón suena", "Espectro: las líneas más emitidas quedan como bordones", "Zeeman: el campo abre cada línea en tres"});
		configSwitch(FINA_PARAM, 0.f, 1.f, 1.f, "Estructura fina", {"No", "Sí: cada línea es un doblete"});
		configParam(NIVEL_PARAM, 0.f, 1.5f, 1.f, "Nivel", "%", 0.f, 100.f);
		configButton(EXCITAR_PARAM, "Excitar: los átomos en el fundamental absorben ya");
		configButton(OLVIDAR_PARAM, "Olvidar el espectro acumulado (los bordones se apagan)");

		configInput(VOCT_INPUT, "V/OCT de H-alfa");
		configInput(NMAX_INPUT, "Nivel máximo · 1 V por nivel");
		configInput(TEMP_INPUT, "Temperatura · 1 V por duplicación");
		configInput(CAMPO_INPUT, "Campo B · 10 V = 100 %");
		configInput(ATOMOS_INPUT, "Átomos · 2,5 V por átomo");
		configInput(ESPACIO_INPUT, "Espacio · 10 V = 100 %");
		configInput(MODO_INPUT, "Modo · 3,3 V por paso");
		configInput(NIVEL_INPUT, "Nivel · VCA, 10 V = ×1");
		configInput(EXCITAR_INPUT, "Excitar (disparo)");
		configInput(OLVIDAR_INPUT, "Olvidar el espectro (disparo)");

		configOutput(FOTON_VOCT_OUTPUT, "Fotón · V/OCT de la última línea emitida (0 V = C4)");
		configOutput(FOTON_OUTPUT, "Fotón · disparo en cada emisión");
		configOutput(SERIE_OUTPUT, "Serie · nivel de llegada del último fotón (1 V Lyman, 2 V Balmer, 3 V Paschen…)");
		configOutput(NU_OUTPUT, "ν · energía del último fotón (10 V = límite de Lyman)");
		configOutput(NIVELES_OUTPUT, "Niveles · un canal por átomo: su energía (0 V fundamental, 10 V ionización)");
		configOutput(CONTINUO_OUTPUT, "Continuo · envolvente del decaimiento de dos fotones (2s → 1s)");
		configOutput(LINEAS_OUTPUT, "Líneas · hasta 8 canales V/OCT con las líneas más intensas del espectro");
		configOutput(L_OUTPUT, "Izquierda");
		configOutput(R_OUTPUT, "Derecha");
		configLight(FOTON_LIGHT, "Fotón");
		configLight(CONTINUO_LIGHT, "Continuo de dos fotones");

		control.setDivision(32);
		nucleo.init(48000.f);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { nucleo.init(e.sampleRate); vistos = nucleo.emitidos; }
	void onReset(const ResetEvent& e) override { Module::onReset(e); nucleo.init(APP->engine->getSampleRate()); vistos = 0; }

	float entrada(int id, float escala) { return inputs[id].isConnected() ? inputs[id].getVoltage() * escala : 0.f; }

	// las líneas más intensas del espectro acumulado, como V/OCT (para LÍNEAS)
	void buscarLineas() {
		bool usado[NMAX + 1][NMAX + 1] = {};
		nLineas = 0;
		for (int k = 0; k < 8; ++k) {
			float mejor = 0.05f; int h0 = 0, l0 = 0;
			for (int h = 2; h <= NMAX; ++h) for (int l = 1; l < h; ++l)
				if (!usado[h][l] && nucleo.intensidad[h][l] > mejor) { mejor = nucleo.intensidad[h][l]; h0 = h; l0 = l; }
			if (!h0) break;
			usado[h0][l0] = true;
			lineas[nLineas++] = std::log2(nucleo.lineaHz(h0, l0) / dsp::FREQ_C4);
		}
	}

	void process(const ProcessArgs& args) override {
		if (control.process()) {
			nucleo.nmax = clamp((int)std::round(params[NMAX_PARAM].getValue() + entrada(NMAX_INPUT, 1.f)), 2, NMAX);
			nucleo.temperatura = std::pow(2.f, clamp(params[TEMP_PARAM].getValue() + entrada(TEMP_INPUT, 1.f), std::log2(0.05f), std::log2(24.f)));
			nucleo.campo = clamp(params[CAMPO_PARAM].getValue() + entrada(CAMPO_INPUT, 0.1f), 0.f, 1.f);
			nucleo.refHz = clamp(std::pow(2.f, params[HALFA_PARAM].getValue() + entrada(VOCT_INPUT, 1.f)), 20.f, 4000.f);
			nucleo.atomos = clamp((int)std::round(params[ATOMOS_PARAM].getValue() + entrada(ATOMOS_INPUT, 0.4f)), 1, NATOMO);
			nucleo.espacio = clamp(params[ESPACIO_PARAM].getValue() + entrada(ESPACIO_INPUT, 0.1f), 0.f, 1.f);
			nucleo.modo = clamp((int)std::round(params[MODO_PARAM].getValue() + entrada(MODO_INPUT, 0.3f)), 0, 2);
			nucleo.fina = params[FINA_PARAM].getValue() > 0.5f;
			float nv = params[NIVEL_PARAM].getValue();
			if (inputs[NIVEL_INPUT].isConnected()) nv *= clamp(inputs[NIVEL_INPUT].getVoltage() / 10.f, 0.f, 1.f);
			nucleo.nivel = nv;
			buscarLineas();
		}

		// gestos: botón o disparo
		const int ti[2] = {EXCITAR_INPUT, OLVIDAR_INPUT};
		for (int i = 0; i < 2; ++i) {
			const bool b = boton[i].process(params[EXCITAR_PARAM + i].getValue() > 0.f);
			const bool t = trig[i].process(inputs[ti[i]].getVoltage(), 0.1f, 1.f);
			if (b || t) {
				if (i == 0) nucleo.excitar();
				else std::memset(nucleo.intensidad, 0, sizeof nucleo.intensidad);
				pulsoLuz[i].trigger(0.12f);
			}
		}

		float l, r;
		nucleo.process(l, r);
		outputs[L_OUTPUT].setVoltage(l * 5.f);
		outputs[R_OUTPUT].setVoltage(r * 5.f);

		// cada fotón es un evento
		if (nucleo.emitidos != vistos) {
			vistos = nucleo.emitidos;
			fotonVoct = clamp(std::log2(nucleo.ultimoHz / dsp::FREQ_C4), -10.f, 10.f);
			serie = (float)nucleo.ultimoLo;
			nuV = 10.f * nu(nucleo.ultimoHi, nucleo.ultimoLo);
			pulsoFoton.trigger(5e-3f);
		}
		const float st = args.sampleTime;
		const bool fotonAlto = pulsoFoton.process(st);
		outputs[FOTON_VOCT_OUTPUT].setVoltage(fotonVoct);
		outputs[FOTON_OUTPUT].setVoltage(fotonAlto ? 10.f : 0.f);
		outputs[SERIE_OUTPUT].setVoltage(serie);
		outputs[NU_OUTPUT].setVoltage(nuV);
		outputs[CONTINUO_OUTPUT].setVoltage(clamp(nucleo.continuo / 0.12f * 10.f, 0.f, 10.f));
		if (outputs[NIVELES_OUTPUT].isConnected()) {
			const int na = nucleo.atomos;
			outputs[NIVELES_OUTPUT].setChannels(na);
			for (int a = 0; a < na; ++a) outputs[NIVELES_OUTPUT].setVoltage(10.f * energia(nucleo.atomo[a].n), a);
		}
		if (outputs[LINEAS_OUTPUT].isConnected()) {
			outputs[LINEAS_OUTPUT].setChannels(std::max(nLineas, 1));
			if (!nLineas) outputs[LINEAS_OUTPUT].setVoltage(std::log2(nucleo.refHz / dsp::FREQ_C4), 0);
			for (int k = 0; k < nLineas; ++k) outputs[LINEAS_OUTPUT].setVoltage(lineas[k], k);
		}

		for (int i = 0; i < 2; ++i) lights[EXCITAR_LIGHT + i].setBrightnessSmooth(pulsoLuz[i].process(st) ? 1.f : 0.f, st);
		lights[FOTON_LIGHT].setBrightnessSmooth(fotonAlto ? 1.f : 0.f, st * 0.25f);
		lights[CONTINUO_LIGHT].setBrightness(clamp(nucleo.continuo / 0.12f, 0.f, 1.f));
	}
};

// ============================================================ estética
// neón sobre negro, como la suite ESPECTROS; los fotones visibles tienen su color real
static const NVGcolor COL_C = nvgRGB(0x2e, 0xe6, 0xff);   // cian: el átomo y la escalera
static const NVGcolor COL_M = nvgRGB(0xff, 0x3d, 0xc8);   // magenta: ultravioleta (Lyman)
static const NVGcolor COL_H = nvgRGB(0xff, 0xb4, 0x2e);   // ámbar: infrarrojo y avisos
static const NVGcolor COL_TXT = nvgRGB(0xd8, 0xe2, 0xea);
static const NVGcolor COL_DIM = nvgRGB(0x7d, 0x8a, 0x96);

static std::shared_ptr<window::Font> fuente() { return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf")); }

// color de una longitud de onda: el real si es visible; fuera de lo visible, magenta (UV) o ámbar (IR)
static NVGcolor colorLambda(float nm, float a = 1.f) {
	float r = 0, g = 0, b = 0;
	if (nm < 380.f) { NVGcolor c = COL_M; c.a = a; return c; }
	if (nm > 750.f) { NVGcolor c = COL_H; c.a = a; return c; }
	if (nm < 440.f) { r = (440.f - nm) / 60.f; b = 1.f; }
	else if (nm < 490.f) { g = (nm - 440.f) / 50.f; b = 1.f; }
	else if (nm < 510.f) { g = 1.f; b = (510.f - nm) / 20.f; }
	else if (nm < 580.f) { r = (nm - 510.f) / 70.f; g = 1.f; }
	else if (nm < 645.f) { r = 1.f; g = (645.f - nm) / 65.f; }
	else r = 1.f;
	return nvgRGBAf(0.15f + 0.85f * r, 0.15f + 0.85f * g, 0.15f + 0.85f * b, a);
}

// nube orbital |ψnlm|² (corte en el plano x-z), como en el firmware del Cardputer
struct NubeRydberg {
	static const int GR = 44;
	float d[GR * GR] = {}, ant[GR * GR] = {};
	int n = -1, l = -1, m = 99;
	double inicio = 0.0;

	static float laguerre(int k, int a, float x) {
		if (k == 0) return 1.f;
		float l0 = 1.f, l1 = 1.f + a - x;
		for (int i = 2; i <= k; ++i) { const float l2 = ((2 * i - 1 + a - x) * l1 - (i - 1 + a) * l0) / i; l0 = l1; l1 = l2; }
		return l1;
	}
	static float legendre(int l, int m, float x) {
		float pmm = 1.f;
		const float s = std::sqrt(std::max(0.f, 1.f - x * x));
		for (int i = 1; i <= m; ++i) pmm *= -(2 * i - 1) * s;
		if (l == m) return pmm;
		float pm1 = x * (2 * m + 1) * pmm;
		if (l == m + 1) return pm1;
		float pl = 0;
		for (int ll = m + 2; ll <= l; ++ll) { pl = ((2 * ll - 1) * x * pm1 - (ll + m - 1) * pmm) / (ll - m); pmm = pm1; pm1 = pl; }
		return pl;
	}
	void construir(int n_, int l_, int m_) {
		std::memcpy(ant, d, sizeof d);
		const float R = 2.2f * n_ * n_ + 4.f;
		const int am = std::abs(m_);
		float mx = 1e-12f;
		for (int j = 0; j < GR; ++j) for (int i = 0; i < GR; ++i) {
			const float x = ((i + 0.5f) / GR * 2 - 1) * R, z = ((j + 0.5f) / GR * 2 - 1) * R;
			const float r = std::sqrt(x * x + z * z) + 1e-4f, rho = 2.f * r / n_;
			const float rad = std::pow(rho, (float)l_) * std::exp(-rho * 0.5f) * laguerre(n_ - l_ - 1, 2 * l_ + 1, rho);
			const float ang = legendre(l_, am, z / r) * (m_ < 0 ? (x / r) : 1.f);
			const float v = rad * rad * ang * ang * r;
			d[j * GR + i] = v; mx = std::max(mx, v);
		}
		for (float& v : d) v = std::sqrt(v / mx);
		n = n_; l = l_; m = m_;
		inicio = system::getTime();
	}
};

struct PantallaRydberg : TransparentWidget {
	Rydberg* module = nullptr;
	NubeRydberg nube;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3.f);
		nvgFillColor(args.vg, nvgRGB(0x04, 0x05, 0x07));
		nvgFill(args.vg);
		if (!module) dibujar(args, true);
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) dibujar(args, false);
		Widget::drawLayer(args, layer);
	}
	void linea(NVGcontext* vg, float x0, float y0, float x1, float y1, NVGcolor c, float w) {
		nvgBeginPath(vg); nvgMoveTo(vg, x0, y0); nvgLineTo(vg, x1, y1);
		nvgStrokeColor(vg, c); nvgStrokeWidth(vg, w); nvgStroke(vg);
	}

	void dibujar(const DrawArgs& args, bool previa) {
		NVGcontext* vg = args.vg;
		const float W = box.size.x, H = box.size.y;
		const Nucleo* nu_ = previa ? nullptr : &module->nucleo;
		nvgSave(vg);
		nvgScissor(vg, 0, 0, W, H);
		nvgGlobalCompositeOperation(vg, NVG_LIGHTER);

		// ---- nube orbital del primer átomo (izquierda)
		const int an = nu_ ? nu_->atomo[0].n : 3, al = nu_ ? nu_->atomo[0].l : 2, am = nu_ ? nu_->atomo[0].m : 1;
		if (an != nube.n || al != nube.l || am != nube.m) nube.construir(an, al, am);
		const float mt = previa ? 1.f : std::min(1.f, (float)(system::getTime() - nube.inicio) / 0.35f);
		const float lado = H - 4.f, cel = lado / NubeRydberg::GR, ox = 2.f, oy = 2.f;
		const float resp = 0.92f + 0.08f * std::sin((float)system::getTime() * 4.f);
		for (int c = 0; c < NubeRydberg::GR * NubeRydberg::GR; ++c) {
			const float v = (nube.ant[c] * (1 - mt) + nube.d[c] * mt) * resp;
			if (v < 0.05f) continue;
			NVGcolor col = v < 0.6f ? nvgLerpRGBA(nvgRGBA(0x10, 0x60, 0x80, 0), COL_C, v / 0.6f) : nvgLerpRGBA(COL_C, nvgRGB(0xff, 0xf2, 0xd0), (v - 0.6f) / 0.4f);
			col.a = std::min(1.f, v * 1.3f);
			nvgBeginPath(vg);
			nvgRect(vg, ox + (c % NubeRydberg::GR) * cel, oy + (c / NubeRydberg::GR) * cel, cel + 0.3f, cel + 0.3f);
			nvgFillColor(vg, col);
			nvgFill(vg);
		}

		// ---- escalera de niveles (centro): E_n = −1/n², arriba la ionización
		const float ex = lado + 10.f, ew = W * 0.36f, ey0 = 9.f, eh = H - 22.f;
		auto yNivel = [&](int n) { return ey0 + eh * (1.f / (n * n)); };
		const int nmax = nu_ ? nu_->nmax : 6;
		for (int n = 1; n <= NMAX; ++n) {
			NVGcolor c = n <= nmax ? COL_C : COL_DIM;
			c.a = n <= nmax ? 0.55f : 0.18f;
			linea(vg, ex + 7.f, yNivel(n), ex + ew, yNivel(n), c, 0.6f);
		}
		{ NVGcolor c = COL_DIM; c.a = 0.4f; linea(vg, ex + 7.f, ey0, ex + ew, ey0, c, 0.4f); }   // ionización
		const float t = nu_ ? nu_->tiempo : 0.f;
		if (nu_) {
			for (int k = 0; k < NFOTON; ++k) {
				const Foton& p = nu_->fotones[k];
				const float edad = t - p.t;
				if (p.hi == 0 || edad > 1.6f || edad < 0) continue;
				const float x = ex + 12.f + (float)((p.hi * 7 + p.lo * 13 + p.atomo * 5) % 40) / 40.f * (ew - 16.f);
				linea(vg, x, yNivel(p.hi), x, yNivel(p.lo), colorLambda(lambdaNm(p.hi, p.lo), 1.f - edad / 1.6f), 1.6f);
			}
			for (int a = 0; a < nu_->atomos; ++a) {
				nvgBeginPath(vg);
				nvgCircle(vg, ex + 10.f + a * (ew - 14.f) / 3.f, yNivel(nu_->atomo[a].n), 2.f);
				nvgFillColor(vg, a == 0 ? COL_TXT : COL_C);
				nvgFill(vg);
			}
		} else {
			const int dem[][2] = {{5, 2}, {3, 2}, {4, 2}, {2, 1}, {6, 3}};
			for (int k = 0; k < 5; ++k) { const float x = ex + 14.f + k * (ew - 20.f) / 5.f; linea(vg, x, yNivel(dem[k][0]), x, yNivel(dem[k][1]), colorLambda(lambdaNm(dem[k][0], dem[k][1])), 1.6f); }
		}

		// ---- espectro acumulado (derecha): eje logarítmico de 90 a 2000 nm con la banda visible
		const float sx = ex + ew + 8.f, sw = W - sx - 4.f, sy = 12.f, sh = H - 25.f;
		auto X = [&](float nm) { return sx + sw * std::log(nm / 90.f) / std::log(2000.f / 90.f); };
		for (float nm = 380.f; nm < 750.f; nm += 4.f) linea(vg, X(nm), sy + sh + 1.5f, X(nm + 4.f), sy + sh + 1.5f, colorLambda(nm, 0.9f), 1.6f);
		float imax = 0.5f;
		if (nu_) for (int h = 2; h <= NMAX; ++h) for (int l = 1; l < h; ++l) imax = std::max(imax, nu_->intensidad[h][l]);
		const bool zee = nu_ && nu_->modo == ZEEMAN && nu_->campo > 0.01f;
		for (int h = 2; h <= NMAX; ++h) for (int l = 1; l < h; ++l) {
			float I = nu_ ? nu_->intensidad[h][l] : (l == 2 && h <= 6 ? 1.f / (h - 1) : (l == 1 && h <= 4 ? 0.6f : 0.f));
			if (I < 0.02f) continue;
			const float x = X(lambdaNm(h, l)), hh = 1.5f + sh * std::sqrt(I / imax);
			const NVGcolor c = colorLambda(lambdaNm(h, l), 0.9f);
			linea(vg, x, sy + sh, x, sy + sh - hh, c, 1.f);
			if (zee) { const float d = 0.8f + nu_->campo * 2.5f; linea(vg, x - d, sy + sh, x - d, sy + sh - hh * 0.7f, c, 0.7f); linea(vg, x + d, sy + sh, x + d, sy + sh - hh * 0.7f, c, 0.7f); }
		}

		// ---- textos
		auto f = fuente();
		if (f && f->handle >= 0) {
			nvgGlobalCompositeOperation(vg, NVG_SOURCE_OVER);
			nvgFontFaceId(vg, f->handle);
			nvgFontSize(vg, 8.5f);
			char buf[96];
			static const char* LN = "spdfghiklmno";
			snprintf(buf, sizeof buf, "%d%c  m=%d", an, LN[clamp(al, 0, 11)], am);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			nvgFillColor(vg, COL_TXT);
			nvgText(vg, 4.f, H - 2.f, buf, nullptr);
			const int md = nu_ ? nu_->modo : 0;
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgFillColor(vg, md == CASCADA ? COL_C : md == ESPECTRO ? COL_TXT : COL_M);
			snprintf(buf, sizeof buf, "%d %s", md + 1, NOMBRE_MODO[md]);
			nvgText(vg, ex + 7.f, 0.5f, buf, nullptr);
			nvgFillColor(vg, COL_DIM);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			if (md == ZEEMAN) snprintf(buf, sizeof buf, "n≤%d  B %.2f", nmax, nu_ ? nu_->campo : 0.3f);
			else snprintf(buf, sizeof buf, "n≤%d  T %.2f/s", nmax, nu_ ? nu_->temperatura : 1.2f);
			nvgText(vg, ex + 7.f, H - 2.f, buf, nullptr);
			snprintf(buf, sizeof buf, "Hα %.0f Hz", nu_ ? nu_->refHz : 440.f);
			nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
			nvgText(vg, W - 4.f, H - 2.f, buf, nullptr);
			if (nu_ && t - nu_->fotones[(nu_->fotonCabeza + NFOTON - 1) % NFOTON].t < 1.2f) {
				const int hi = nu_->ultimoHi, lo = nu_->ultimoLo;
				static const char* SERIES[] = {"", "Lyman", "Balmer", "Paschen", "Brackett", "Pfund", "Humphreys"};
				snprintf(buf, sizeof buf, "%d→%d %s %.0f nm", hi, lo, lo <= 6 ? SERIES[lo] : "", lambdaNm(hi, lo));
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
				nvgFillColor(vg, colorLambda(lambdaNm(hi, lo)));
				nvgText(vg, W - 4.f, 0.5f, buf, nullptr);
			}
		}
		nvgResetScissor(vg);
		nvgRestore(vg);
	}
};

// rótulos del panel (texto vivo: el SVG de Rack no dibuja texto)
struct RotuloRydberg { float x, y, tam; const char* txt; int alin; int color; };
struct RotulosRydberg : TransparentWidget {
	std::vector<RotuloRydberg> lista;
	void draw(const DrawArgs& args) override {
		auto f = fuente();
		if (!f || f->handle < 0) return;
		nvgFontFaceId(args.vg, f->handle);
		for (const RotuloRydberg& r : lista) {
			nvgFontSize(args.vg, r.tam);
			nvgTextAlign(args.vg, (r.alin < 0 ? NVG_ALIGN_LEFT : r.alin > 0 ? NVG_ALIGN_RIGHT : NVG_ALIGN_CENTER) | NVG_ALIGN_MIDDLE);
			const NVGcolor cs[] = {COL_TXT, COL_DIM, COL_C, COL_M, COL_H};
			nvgFillColor(args.vg, cs[r.color]);
			const Vec p = mm2px(Vec(r.x, r.y));
			nvgText(args.vg, p.x, p.y, r.txt, nullptr);
		}
	}
};

// ============================================================ panel (24 HP)
static const float C6[6] = {11.f, 31.f, 51.f, 71.f, 91.f, 110.9f};
static const float C8[8] = {8.5f, 23.5f, 38.5f, 53.4f, 68.4f, 83.4f, 98.4f, 113.4f};

struct RydbergWidget : ModuleWidget {
	RydbergWidget(Rydberg* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Rydberg.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		PantallaRydberg* p = new PantallaRydberg;
		p->module = module;
		p->box.pos = mm2px(Vec(4.f, 9.5f));
		p->box.size = mm2px(Vec(113.92f, 32.f));
		addChild(p);

		typedef Rydberg Y;
		// fila 1: el átomo
		const int k1[6] = {Y::NMAX_PARAM, Y::TEMP_PARAM, Y::CAMPO_PARAM, Y::HALFA_PARAM, Y::ATOMOS_PARAM, Y::ESPACIO_PARAM};
		for (int i = 0; i < 6; ++i) {
			if (i == 0 || i == 4) addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(C6[i], 51.f)), module, k1[i]));
			else addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(C6[i], 51.f)), module, k1[i]));
		}
		// fila 2: la escucha
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(C6[0], 66.5f)), module, Y::MODO_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(C6[1], 66.5f)), module, Y::FINA_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(C6[2], 66.5f)), module, Y::NIVEL_PARAM));
		addChild(createLightCentered<MediumLight<WhiteLight>>(mm2px(Vec(C6[4], 66.5f)), module, Y::FOTON_LIGHT));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(C6[5], 66.5f)), module, Y::CONTINUO_LIGHT));
		// gestos: botón + disparo
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(C8[0], 80.5f)), module, Y::EXCITAR_PARAM, Y::EXCITAR_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(C8[1], 80.5f)), module, Y::EXCITAR_INPUT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(C8[2], 80.5f)), module, Y::OLVIDAR_PARAM, Y::OLVIDAR_LIGHT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(C8[3], 80.5f)), module, Y::OLVIDAR_INPUT));
		// entradas de control
		const int ci[8] = {Y::VOCT_INPUT, Y::NMAX_INPUT, Y::TEMP_INPUT, Y::CAMPO_INPUT, Y::ATOMOS_INPUT, Y::ESPACIO_INPUT, Y::MODO_INPUT, Y::NIVEL_INPUT};
		for (int i = 0; i < 8; ++i) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(C8[i], 94.2f)), module, ci[i]));
		// salidas
		const int o1[7] = {Y::FOTON_VOCT_OUTPUT, Y::FOTON_OUTPUT, Y::SERIE_OUTPUT, Y::NU_OUTPUT, Y::NIVELES_OUTPUT, Y::CONTINUO_OUTPUT, Y::LINEAS_OUTPUT};
		for (int i = 0; i < 7; ++i) addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(C8[i + 1], 105.6f)), module, o1[i]));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(C8[6], 116.8f)), module, Y::L_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(C8[7], 116.8f)), module, Y::R_OUTPUT));

		RotulosRydberg* r = new RotulosRydberg;
		r->box.size = box.size;
		const float t = 7.2f, s = 6.4f;
		r->lista = {
			{12.f, 4.6f, 13.f, "RYDBERG", -1, 0},
			{109.5f, 3.2f, 6.2f, "ESPECTROS · el átomo de hidrógeno", 1, 1},
			{109.5f, 6.3f, 5.6f, "cada salto un fotón · cada fotón una nota", 1, 1},
			{C6[0], 44.6f, t, "n MÁX", 0, 0}, {C6[1], 44.6f, t, "TEMPERATURA", 0, 0}, {C6[2], 44.6f, t, "CAMPO B", 0, 0},
			{C6[3], 44.6f, t, "H-ALFA", 0, 0}, {C6[4], 44.6f, t, "ÁTOMOS", 0, 0}, {C6[5], 44.6f, t, "ESPACIO", 0, 0},
			{C6[0], 59.6f, t, "MODO", 0, 0}, {C6[1], 59.6f, t, "FINA", 0, 0}, {C6[2], 59.6f, t, "NIVEL", 0, 0},
			{C6[4], 59.6f, t, "FOTÓN", 0, 0}, {C6[5], 59.6f, t, "2 FOTONES", 0, 4},
			{C6[0] + 4.f, 62.2f, 5.4f, "zeeman", -1, 3}, {C6[0] + 4.f, 66.6f, 5.4f, "espectro", -1, 1}, {C6[0] + 4.f, 71.f, 5.4f, "cascada", -1, 2},
			{C6[1] + 4.f, 63.6f, 5.4f, "doblete", -1, 1}, {C6[1] + 4.f, 69.6f, 5.4f, "simple", -1, 1},
			{(C8[0] + C8[1]) / 2, 75.4f, s, "EXCITAR", 0, 0}, {(C8[2] + C8[3]) / 2, 75.4f, s, "OLVIDAR", 0, 0},
			{(C8[5] + C8[7]) / 2, 79.0f, 5.4f, "Lyman · Balmer · Paschen", 0, 1}, {(C8[5] + C8[7]) / 2, 82.4f, 5.4f, "1/n'² − 1/n²", 0, 1},
			{C8[0], 88.8f, s, "V/OCT", 0, 0}, {C8[1], 88.8f, s, "n MÁX", 0, 1}, {C8[2], 88.8f, s, "TEMP", 0, 1}, {C8[3], 88.8f, s, "CAMPO", 0, 1},
			{C8[4], 88.8f, s, "ÁTOMOS", 0, 1}, {C8[5], 88.8f, s, "ESPACIO", 0, 1}, {C8[6], 88.8f, s, "MODO", 0, 1}, {C8[7], 88.8f, s, "NIVEL", 0, 1},
			{C8[1], 100.2f, s, "FOTÓN V", 0, 2}, {C8[2], 100.2f, s, "FOTÓN", 0, 2}, {C8[3], 100.2f, s, "SERIE", 0, 0}, {C8[4], 100.2f, s, "ν", 0, 0},
			{C8[5], 100.2f, s, "NIVELES", 0, 0}, {C8[6], 100.2f, s, "CONTINUO", 0, 4}, {C8[7], 100.2f, s, "LÍNEAS", 0, 3},
			{C8[6], 111.4f, s, "IZQ", 0, 0}, {C8[7], 111.4f, s, "DER", 0, 0},
			{4.5f, 110.6f, 5.6f, "Ph.D. Mauricio Vásquez Arias", -1, 0},
			{4.5f, 113.7f, 4.8f, "Profesor titular – Investigador asociado", -1, 1},
			{4.5f, 116.5f, 4.8f, "Escuela de Artes y Humanidades · EAFIT", -1, 1},
		};
		addChild(r);
	}

	void appendContextMenu(Menu* menu) override {
		Rydberg* m = getModule<Rydberg>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		char buf[128];
		snprintf(buf, sizeof buf, "Fotones emitidos: %u · decaimientos de dos fotones: %u", m->nucleo.emitidos, m->nucleo.dobles);
		menu->addChild(createMenuLabel(buf));
		snprintf(buf, sizeof buf, "H-alfa %.1f Hz · Lyman-alfa %.1f Hz (razón 5,4 del hidrógeno)", m->nucleo.lineaHz(3, 2), m->nucleo.lineaHz(2, 1));
		menu->addChild(createMenuLabel(buf));
	}
};

Model* modelRydberg = createModel<Rydberg, RydbergWidget>("Rydberg");
