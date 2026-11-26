// LYAPUNOV — péndulos gemelos caóticos · ESPECTROS para VCV Rack
// Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado
// Escuela de Artes y Humanidades, Universidad EAFIT
//
// Dos péndulos dobles casi idénticos se sueltan juntos. Lo que suena, se ve y sale como voltaje es
// cómo se separan: los gemelos son el estéreo, cada uno compone sus propias notas en una afinación no
// temperada, y el instante en que dejan de parecerse (el horizonte de predictibilidad) es un evento.
#include "plugin.hpp"
#include "lyapunov/Nucleo.hpp"
#include "comun/Scala.hpp"
#include <osdialog.h>
#include <atomic>
#include <fstream>
#include <sstream>

using namespace lyap;

static const int AFIN_SCALA = N_AFINACIONES;   // la última posición del selector: archivo Scala propio

struct Lyapunov : Module {
	enum ParamId {
		F0_PARAM, BASE_PARAM, ENERGIA_PARAM, IMPULSO_PARAM, D0_PARAM, TIEMPO_PARAM,
		MEZCLA_PARAM, AFIN_PARAM, ATRACCION_PARAM, REGIMEN_PARAM, SONIDO_PARAM, NIVEL_PARAM,
		SOLTAR_PARAM, PERTURBAR_PARAM, SINCRO_PARAM, GOLPE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT, F0_INPUT, ENERGIA_INPUT, IMPULSO_INPUT, D0_INPUT, TIEMPO_INPUT, MEZCLA_INPUT, ATRACCION_INPUT,
		SOLTAR_INPUT, PERTURBAR_INPUT, SINCRO_INPUT, GOLPE_INPUT, CONGELAR_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		A_OUTPUT, B_OUTPUT, MEZCLA_OUTPUT, NOTA_A_OUTPUT, GATE_A_OUTPUT, NOTA_B_OUTPUT, GATE_B_OUTPUT,
		DELTA_OUTPUT, LAMBDA_OUTPUT, HORIZONTE_OUTPUT, ESTADO_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		SOLTAR_LIGHT, PERTURBAR_LIGHT, SINCRO_LIGHT, GOLPE_LIGHT, HORIZONTE_LIGHT, CONGELAR_LIGHT, GATE_A_LIGHT, GATE_B_LIGHT,
		LIGHTS_LEN
	};

	Nucleo nucleo;
	dsp::ClockDivider control;
	dsp::SchmittTrigger trig[4];
	dsp::BooleanTrigger boton[4];
	dsp::PulseGenerator pulsoLuz[4], gateA, gateB;
	float notaA = 0.f, notaB = 0.f, tiempoEf = 1.f;
	bool autoSoltar = true;
	int afinCargada = -1;

	// afinación Scala propia: la escribe la interfaz, la toma el hilo de audio
	struct Escala { std::string descripcion; std::vector<float> cents; float periodo = 1200.f; };
	Escala scala;
	std::atomic<bool> scalaNueva{false};
	Escala scalaPendiente;

	Lyapunov() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		auto* p = configParam(F0_PARAM, std::log2(0.05f), std::log2(220.f), std::log2(30.f), "f0 · frecuencia natural del péndulo", " Hz", 2.f);
		p->description = "Fija la gravedad: el tempo del caos. Bajo, el péndulo es un gesto lento (para FM y voltajes); alto, la oscilación se oye.";
		p = configParam(BASE_PARAM, std::log2(27.5f), std::log2(880.f), std::log2(110.f), "Base", " Hz", 2.f);
		p->description = "Portadora de FM, raíz de la afinación y registro de las notas que componen los gemelos.";
		p = configParam(ENERGIA_PARAM, 0.f, 1.f, 0.5f, "Energía al soltar", "%", 0.f, 100.f);
		p->description = "Altura desde la que se sueltan los péndulos. Actúa en el próximo SOLTAR.";
		p = configParam(IMPULSO_PARAM, -1.f, 1.f, 0.f, "Impulso", "%", 0.f, 100.f);
		p->description = "Negativo: fricción, el caos se apaga hacia el reposo. Cero: conservativo. Positivo: bombea energía.";
		p = configParam(D0_PARAM, -5.f, 1.f, -1.f, "δ0 · separación inicial de los gemelos", "°", 10.f);
		p->description = "Cuánto difieren A y B al soltarlos. Más pequeño = más tiempo antes del horizonte.";
		p = configParam(TIEMPO_PARAM, 0.f, 2.f, 1.f, "Tiempo", "×");
		p->description = "Escala del tiempo físico. En 0 la física se detiene (también con la compuerta CONGELAR).";
		p = configParam(MEZCLA_PARAM, 0.f, 2.f, 1.f, "Mezcla A·B");
		p->description = "0 = solo A · 1 = A+B · 2 = A−B: en A−B solo se oye la diferencia, es decir, el caos.";
		std::vector<std::string> nombres;
		for (int i = 0; i < N_AFINACIONES; ++i) nombres.push_back(std::string(AFINACIONES[i].nombre) + " · " + AFINACIONES[i].grupo);
		nombres.push_back("Archivo Scala (cárgalo con clic derecho)");
		auto* sq = configSwitch(AFIN_PARAM, 0.f, (float)N_AFINACIONES, 1.f, "Afinación", nombres);
		sq->description = "Afinaciones no temperadas y no occidentales. Las notas de los vuelcos y la FM caen en sus grados.";
		p = configParam(ATRACCION_PARAM, 0.f, 1.f, 1.f, "Atracción a la afinación", "%", 0.f, 100.f);
		p->description = "0 = las alturas que da la física · 100 % = fijas en los grados de la afinación.";
		configSwitch(REGIMEN_PARAM, 0.f, 2.f, 1.f, "Régimen", {"Oscilaciones pequeñas (sin caos)", "Oscilaciones grandes (caos)", "Divergencia (se suelta de nuevo tras el horizonte)"});
		configSwitch(SONIDO_PARAM, 0.f, 1.f, 0.f, "Sonido", {"Audificación: la masa ES la onda", "FM: los ángulos modulan dos osciladores"});
		configParam(NIVEL_PARAM, 0.f, 1.5f, 1.f, "Nivel", "%", 0.f, 100.f);
		configButton(SOLTAR_PARAM, "Soltar los dos péndulos");
		configButton(PERTURBAR_PARAM, "Perturbar a B (la divergencia recomienza desde aquí)");
		configButton(SINCRO_PARAM, "Sincronizar: B = A + δ0");
		configButton(GOLPE_PARAM, "Golpe a ambos");

		configInput(VOCT_INPUT, "V/OCT (base; en audificación también f0)");
		configInput(F0_INPUT, "f0 · 1 V/oct");
		configInput(ENERGIA_INPUT, "Energía · 0–10 V");
		configInput(IMPULSO_INPUT, "Impulso · ±5 V");
		configInput(D0_INPUT, "δ0 · 2 V por década");
		configInput(TIEMPO_INPUT, "Tiempo · 5 V = ×1");
		configInput(MEZCLA_INPUT, "Mezcla · 5 V por paso");
		configInput(ATRACCION_INPUT, "Atracción · 0–10 V");
		configInput(SOLTAR_INPUT, "Soltar (disparo)");
		configInput(PERTURBAR_INPUT, "Perturbar (disparo)");
		configInput(SINCRO_INPUT, "Sincronizar (disparo)");
		configInput(GOLPE_INPUT, "Golpe (disparo; el voltaje es la fuerza, 10 V = 1)");
		configInput(CONGELAR_INPUT, "Congelar (compuerta: el tiempo se detiene mientras está alta)");

		configOutput(A_OUTPUT, "Gemelo A (izquierda)");
		configOutput(B_OUTPUT, "Gemelo B (derecha)");
		configOutput(MEZCLA_OUTPUT, "Mezcla A·B");
		configOutput(NOTA_A_OUTPUT, "Nota de A · V/OCT (0 V = C4), en la afinación");
		configOutput(GATE_A_OUTPUT, "Compuerta de A · cada vuelco");
		configOutput(NOTA_B_OUTPUT, "Nota de B · V/OCT (0 V = C4), en la afinación");
		configOutput(GATE_B_OUTPUT, "Compuerta de B · cada vuelco");
		configOutput(DELTA_OUTPUT, "δ · separación de los gemelos (logarítmica, 0–10 V)");
		configOutput(LAMBDA_OUTPUT, "λ · exponente de Lyapunov (1 V = 0.1)");
		configOutput(HORIZONTE_OUTPUT, "Horizonte · compuerta alta desde que los gemelos dejan de parecerse");
		configOutput(ESTADO_OUTPUT, "Estado · 8 canales: x/y de las cuatro masas (±5 V)");
		configLight(HORIZONTE_LIGHT, "Horizonte cruzado");
		configLight(CONGELAR_LIGHT, "Congelado");
		configBypass(A_OUTPUT, A_OUTPUT);

		control.setDivision(16);
		nucleo.init(48000.f);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { nucleo.init(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); autoSoltar = true; nucleo.soltar(); }

	float entrada(int id, float escala) { return inputs[id].isConnected() ? inputs[id].getVoltage() * escala : 0.f; }

	void aplicarAfinacion(int k) {
		Afin& a = nucleo.afin;
		if (k < N_AFINACIONES) {
			const Afinacion& t = AFINACIONES[k];
			a.n = t.n; a.periodo = t.periodo;
			for (int i = 0; i < t.n; ++i) a.cents[i] = t.cents[i];
		} else {
			a.n = (int)std::min<size_t>(scala.cents.size(), 128); a.periodo = scala.periodo;
			for (int i = 0; i < a.n; ++i) a.cents[i] = scala.cents[i];
		}
		afinCargada = k;
	}

	void process(const ProcessArgs& args) override {
		if (scalaNueva.exchange(false)) { scala = scalaPendiente; afinCargada = -1; }

		if (control.process()) {
			const float dt = args.sampleTime * control.getDivision();
			const float voct = entrada(VOCT_INPUT, 1.f);
			nucleo.sonido = (int)params[SONIDO_PARAM].getValue();
			nucleo.regimen = (int)params[REGIMEN_PARAM].getValue();
			float lf0 = params[F0_PARAM].getValue() + entrada(F0_INPUT, 1.f) + (nucleo.sonido == AUDIFICACION ? voct : 0.f);
			nucleo.f0 = clamp(std::pow(2.f, lf0), 0.02f, 1000.f);
			nucleo.baseHz = clamp(std::pow(2.f, params[BASE_PARAM].getValue() + voct), 8.f, 8000.f);
			nucleo.energia = clamp(params[ENERGIA_PARAM].getValue() + entrada(ENERGIA_INPUT, 0.1f), 0.f, 1.f);
			nucleo.impulso = clamp(params[IMPULSO_PARAM].getValue() + entrada(IMPULSO_INPUT, 0.2f), -1.f, 1.f);
			nucleo.d0Grados = std::pow(10.f, clamp(params[D0_PARAM].getValue() + entrada(D0_INPUT, 0.5f), -6.f, 1.5f));
			nucleo.mezcla = clamp(params[MEZCLA_PARAM].getValue() + entrada(MEZCLA_INPUT, 0.2f), 0.f, 2.f);
			nucleo.nivel = params[NIVEL_PARAM].getValue();
			nucleo.autoSoltar = autoSoltar;
			// congelar: glissando de 0,25 s hacia 0 (y de regreso), como en la suite
			const bool congelado = inputs[CONGELAR_INPUT].getVoltage() >= 1.f;
			const float tObj = congelado ? 0.f : clamp(params[TIEMPO_PARAM].getValue() + entrada(TIEMPO_INPUT, 0.2f), 0.f, 4.f);
			tiempoEf += (tObj - tiempoEf) * (1.f - std::exp(-dt / 0.08f));
			if (std::fabs(tiempoEf - tObj) < 1e-3f) tiempoEf = tObj;
			nucleo.tiempo = tiempoEf;
			lights[CONGELAR_LIGHT].setBrightness(congelado ? 1.f : 0.f);
			// afinación: raíz = base (el V/OCT transpone la escala entera)
			const int k = clamp((int)std::round(params[AFIN_PARAM].getValue()), 0, AFIN_SCALA);
			if (k != afinCargada) aplicarAfinacion(k);
			nucleo.afin.raizHz = nucleo.baseHz;
			nucleo.afin.atraccion = clamp(params[ATRACCION_PARAM].getValue() + entrada(ATRACCION_INPUT, 0.1f), 0.f, 1.f);
		}

		// disparos y botones
		const int ids[4] = {SOLTAR_INPUT, PERTURBAR_INPUT, SINCRO_INPUT, GOLPE_INPUT};
		for (int i = 0; i < 4; ++i) {
			const bool t = trig[i].process(inputs[ids[i]].getVoltage(), 0.1f, 1.f);
			const bool b = boton[i].process(params[SOLTAR_PARAM + i].getValue() > 0.f);
			if (!(t || b)) continue;
			pulsoLuz[i].trigger(0.12f);
			if (i == 0) nucleo.soltar();
			else if (i == 1) nucleo.perturbar();
			else if (i == 2) nucleo.sincronizar();
			else nucleo.golpe(t ? clamp(inputs[GOLPE_INPUT].getVoltage() / 10.f, 0.1f, 1.5f) : 1.f);
		}

		float a, b, m;
		nucleo.process(a, b, m);
		outputs[A_OUTPUT].setVoltage(a * 10.f);
		outputs[B_OUTPUT].setVoltage(b * 10.f);
		outputs[MEZCLA_OUTPUT].setVoltage(m * 10.f);

		// notas de los vuelcos, ya en la afinación
		if (nucleo.notaA.on) { notaA = std::log2(nucleo.notaA.hz / dsp::FREQ_C4); gateA.trigger(0.1f); }
		if (nucleo.notaB.on) { notaB = std::log2(nucleo.notaB.hz / dsp::FREQ_C4); gateB.trigger(0.1f); }
		const bool gA = gateA.process(args.sampleTime), gB = gateB.process(args.sampleTime);
		outputs[NOTA_A_OUTPUT].setVoltage(clamp(notaA, -10.f, 10.f));
		outputs[NOTA_B_OUTPUT].setVoltage(clamp(notaB, -10.f, 10.f));
		outputs[GATE_A_OUTPUT].setVoltage(gA ? 10.f : 0.f);
		outputs[GATE_B_OUTPUT].setVoltage(gB ? 10.f : 0.f);

		// la física como voltaje
		const bool split = nucleo.tauSplit > 0.f;
		outputs[DELTA_OUTPUT].setVoltage(clamp((std::log10(nucleo.delta + 1e-7f) + 6.f) * (10.f / 7.f), 0.f, 10.f));
		outputs[LAMBDA_OUTPUT].setVoltage(clamp(nucleo.lambdaVivo * 10.f, 0.f, 10.f));
		outputs[HORIZONTE_OUTPUT].setVoltage(split ? 10.f : 0.f);
		if (outputs[ESTADO_OUTPUT].isConnected()) {
			const Estado* s[2] = {&nucleo.A, &nucleo.B};
			outputs[ESTADO_OUTPUT].setChannels(8);
			for (int j = 0; j < 2; ++j) {
				const float x1 = std::sin(s[j]->t1), y1 = std::cos(s[j]->t1);
				const float x2 = x1 + std::sin(s[j]->t2), y2 = y1 + std::cos(s[j]->t2);
				outputs[ESTADO_OUTPUT].setVoltage(x1 * 5.f, j * 4 + 0);
				outputs[ESTADO_OUTPUT].setVoltage(y1 * 5.f, j * 4 + 1);
				outputs[ESTADO_OUTPUT].setVoltage(x2 * 2.5f, j * 4 + 2);
				outputs[ESTADO_OUTPUT].setVoltage(y2 * 2.5f, j * 4 + 3);
			}
		}

		const float st = args.sampleTime;
		for (int i = 0; i < 4; ++i) lights[SOLTAR_LIGHT + i].setBrightnessSmooth(pulsoLuz[i].process(st) ? 1.f : 0.f, st);
		lights[HORIZONTE_LIGHT].setBrightnessSmooth(split ? 1.f : 0.f, st);
		lights[GATE_A_LIGHT].setBrightnessSmooth(gA ? 1.f : 0.f, st);
		lights[GATE_B_LIGHT].setBrightnessSmooth(gB ? 1.f : 0.f, st);
	}

	bool cargarScala(const std::string& ruta, std::string& error) {
		std::ifstream f(ruta, std::ios::binary);
		if (!f) { error = "no se pudo abrir el archivo"; return false; }
		std::stringstream ss; ss << f.rdbuf();
		ScalaArchivo sc;
		if (!scalaParse(ss.str(), sc)) { error = sc.error; return false; }
		scalaPendiente.descripcion = sc.descripcion.empty() ? system::getFilename(ruta) : sc.descripcion;
		scalaPendiente.cents = sc.cents;
		scalaPendiente.periodo = sc.periodo;
		scalaNueva = true;
		actualizarEtiquetaScala(scalaPendiente.descripcion);
		params[AFIN_PARAM].setValue((float)AFIN_SCALA);
		return true;
	}
	void actualizarEtiquetaScala(const std::string& d) {
		if (auto* sq = dynamic_cast<SwitchQuantity*>(paramQuantities[AFIN_PARAM])) sq->labels.back() = "Scala · " + d;
	}
	std::string nombreAfinacion() {
		const int k = clamp((int)std::round(params[AFIN_PARAM].getValue()), 0, AFIN_SCALA);
		if (k < N_AFINACIONES) return AFINACIONES[k].nombre;
		const std::string& d = scalaNueva ? scalaPendiente.descripcion : scala.descripcion;
		return d.empty() ? "Scala (sin archivo)" : "Scala · " + d;
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "autoSoltar", json_boolean(autoSoltar));
		const Escala& e = scalaNueva ? scalaPendiente : scala;
		if (!e.cents.empty()) {
			json_t* s = json_object();
			json_object_set_new(s, "descripcion", json_string(e.descripcion.c_str()));
			json_object_set_new(s, "periodo", json_real(e.periodo));
			json_t* c = json_array();
			for (float x : e.cents) json_array_append_new(c, json_real(x));
			json_object_set_new(s, "cents", c);
			json_object_set_new(r, "scala", s);
		}
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* a = json_object_get(r, "autoSoltar")) autoSoltar = json_boolean_value(a);
		if (json_t* s = json_object_get(r, "scala")) {
			Escala e;
			if (json_t* d = json_object_get(s, "descripcion")) e.descripcion = json_string_value(d);
			if (json_t* p = json_object_get(s, "periodo")) e.periodo = (float)json_number_value(p);
			if (json_t* c = json_object_get(s, "cents")) { size_t i; json_t* v; json_array_foreach(c, i, v) e.cents.push_back((float)json_number_value(v)); }
			if (!e.cents.empty() && e.periodo > 1.f) { scalaPendiente = e; scalaNueva = true; actualizarEtiquetaScala(e.descripcion); }
		}
	}
};

// ============================================================ estética
// neón sobre negro, como la suite ESPECTROS: cian = gemelo A, magenta = gemelo B, ámbar = horizonte
static const NVGcolor COL_A = nvgRGB(0x2e, 0xe6, 0xff);
static const NVGcolor COL_B = nvgRGB(0xff, 0x3d, 0xc8);
static const NVGcolor COL_H = nvgRGB(0xff, 0xb4, 0x2e);
static const NVGcolor COL_TXT = nvgRGB(0xd8, 0xe2, 0xea);
static const NVGcolor COL_DIM = nvgRGB(0x7d, 0x8a, 0x96);

static std::shared_ptr<window::Font> fuente() { return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf")); }

// pantalla: los dos péndulos con sus estelas, la separación δ en el tiempo y el horizonte
struct Pantalla : TransparentWidget {
	Lyapunov* module = nullptr;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3.f);
		nvgFillColor(args.vg, nvgRGB(0x04, 0x05, 0x07));
		nvgFill(args.vg);
		if (!module) dibujar(args, true);   // vista previa en el navegador de módulos
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) dibujar(args, false);
		Widget::drawLayer(args, layer);
	}

	void linea(NVGcontext* vg, float x0, float y0, float x1, float y1, NVGcolor c, float w) {
		nvgBeginPath(vg); nvgMoveTo(vg, x0, y0); nvgLineTo(vg, x1, y1);
		nvgStrokeColor(vg, c); nvgStrokeWidth(vg, w); nvgStroke(vg);
	}
	void masa(NVGcontext* vg, float x, float y, float r, NVGcolor c) {
		nvgBeginPath(vg); nvgCircle(vg, x, y, r); nvgFillColor(vg, c); nvgFill(vg);
	}

	void dibujar(const DrawArgs& args, bool previa) {
		NVGcontext* vg = args.vg;
		const float W = box.size.x, H = box.size.y;
		nvgSave(vg);
		nvgScissor(vg, 0, 0, W, H);
		nvgGlobalCompositeOperation(vg, NVG_LIGHTER);   // los gemelos se suman: superpuestos se ven blancos

		// ---- péndulos (izquierda)
		const float px = H * 0.62f, py = H * 0.5f, u = H * 0.22f;
		Estado A{}, B{};
		if (previa) { A = {2.1f, 2.6f, 0, 0}; B = {2.05f, 2.9f, 0, 0}; }
		else { A = module->nucleo.A; B = module->nucleo.B; }
		const Nucleo* n = previa ? nullptr : &module->nucleo;

		// estelas de la segunda masa
		if (n) {
			const int cnt = n->trazaCount, head = n->trazaHead, N = Nucleo::TRAZA;
			const int L = std::min(cnt, 240);
			for (int g = 0; g < 2; ++g) {
				const NVGcolor c = g ? COL_B : COL_A;
				for (int i = 1; i < L; ++i) {
					const PuntoTraza& p0 = n->traza[(head - i - 1 + 2 * N) % N];
					const PuntoTraza& p1 = n->traza[(head - i + 2 * N) % N];
					const float x0 = g ? p0.xB : p0.xA, y0 = g ? p0.yB : p0.yA, x1 = g ? p1.xB : p1.xA, y1 = g ? p1.yB : p1.yA;
					NVGcolor cc = c; cc.a = 0.55f * (1.f - (float)i / L);
					linea(vg, px + x0 * u, py + y0 * u, px + x1 * u, py + y1 * u, cc, 1.1f);
				}
			}
		}
		const Estado* s[2] = {&A, &B};
		for (int g = 0; g < 2; ++g) {
			const NVGcolor c = g ? COL_B : COL_A;
			const float x1 = px + std::sin(s[g]->t1) * u, y1 = py + std::cos(s[g]->t1) * u;
			const float x2 = x1 + std::sin(s[g]->t2) * u, y2 = y1 + std::cos(s[g]->t2) * u;
			NVGcolor cr = c; cr.a = 0.8f;
			linea(vg, px, py, x1, y1, cr, 1.4f);
			linea(vg, x1, y1, x2, y2, cr, 1.4f);
			masa(vg, x1, y1, 2.2f, c);
			masa(vg, x2, y2, 2.8f, c);
		}
		masa(vg, px, py, 1.6f, COL_TXT);

		// ---- separación δ en el tiempo (derecha): la recta inclinada ES el exponente de Lyapunov
		const float gx = H * 1.32f, gw = W - gx - 6.f, gy = 13.f, gh = H - 25.f;
		auto yDe = [&](float logd) { return gy + gh * (1.f - clamp((logd + 7.f) / 8.f, 0.f, 1.f)); };
		{
			NVGcolor c = COL_DIM; c.a = 0.35f;
			for (int d = -6; d <= 0; d += 2) linea(vg, gx, yDe((float)d), gx + gw, yDe((float)d), c, 0.5f);
			NVGcolor h = COL_H; h.a = 0.5f;
			linea(vg, gx, yDe(std::log10(0.5f)), gx + gw, yDe(std::log10(0.5f)), h, 0.7f);   // umbral del horizonte
		}
		if (n) {
			const int cnt = std::min((int)n->histCount, Nucleo::HIST), N = Nucleo::HIST;
			for (int i = 1; i < cnt; ++i) {
				const Nucleo::PuntoHist& p0 = n->hist[i - 1];
				const Nucleo::PuntoHist& p1 = n->hist[i];
				const float x0 = gx + gw * (i - 1) / (N - 1.f), x1 = gx + gw * i / (N - 1.f);
				linea(vg, x0, yDe(p0.logd), x1, yDe(p1.logd), p1.split ? COL_H : COL_TXT, 1.2f);
			}
		} else {
			for (int i = 1; i < 40; ++i) linea(vg, gx + gw * (i - 1) / 40.f, yDe(-6.f + 0.17f * (i - 1)), gx + gw * i / 40.f, yDe(-6.f + 0.17f * i), i > 30 ? COL_H : COL_TXT, 1.2f);
		}

		// ---- textos
		auto f = fuente();
		if (f && f->handle >= 0) {
			nvgGlobalCompositeOperation(vg, NVG_SOURCE_OVER);
			nvgFontFaceId(vg, f->handle);
			nvgFontSize(vg, 8.5f);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgFillColor(vg, COL_TXT);
			const std::string af = previa ? "Slendro" : module->nombreAfinacion();
			nvgText(vg, gx, 1.f, af.c_str(), nullptr);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			char buf[96];
			const float lam = n ? n->lambdaVivo : 0.37f, del = n ? n->delta : 0.002f;
			snprintf(buf, sizeof buf, "λ %.3f   δ %.0e", lam, del);
			nvgFillColor(vg, COL_TXT);
			nvgText(vg, gx, H - 2.f, buf, nullptr);
			static const char* reg[] = {"PEQUEÑAS", "GRANDES", "DIVERGENCIA"};
			const int rg = n ? clamp(n->regimen, 0, 2) : 1;
			const bool fm = n && n->sonido == FM;
			snprintf(buf, sizeof buf, "%s · %s", reg[rg], fm ? "FM" : "AUDIF.");
			nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
			nvgFillColor(vg, COL_DIM);
			nvgText(vg, gx + gw, H - 2.f, buf, nullptr);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgFillColor(vg, COL_A); nvgText(vg, 4.f, 2.f, "A", nullptr);
			nvgFillColor(vg, COL_B); nvgText(vg, 12.f, 2.f, "B", nullptr);
			if (n && n->tauSplit > 0.f) {
				nvgFillColor(vg, COL_H);
				snprintf(buf, sizeof buf, "HORIZONTE · τ %.1f", n->tauSplit);
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
				nvgText(vg, gx + gw, 1.f, buf, nullptr);
			}
			if (n && n->tiempo < 1e-3f) {
				nvgFillColor(vg, COL_H);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
				nvgText(vg, 4.f, H - 2.f, "CONGELADO", nullptr);
			}
		}
		nvgResetScissor(vg);
		nvgRestore(vg);
	}
};

// rótulos del panel (texto vivo: el SVG de Rack no dibuja texto)
struct Rotulo { float x, y, tam; const char* txt; int alin; int color; };
struct Rotulos : TransparentWidget {
	std::vector<Rotulo> lista;
	void draw(const DrawArgs& args) override {
		auto f = fuente();
		if (!f || f->handle < 0) return;
		nvgFontFaceId(args.vg, f->handle);
		for (const Rotulo& r : lista) {
			nvgFontSize(args.vg, r.tam);
			nvgTextAlign(args.vg, (r.alin < 0 ? NVG_ALIGN_LEFT : r.alin > 0 ? NVG_ALIGN_RIGHT : NVG_ALIGN_CENTER) | NVG_ALIGN_MIDDLE);
			const NVGcolor cs[] = {COL_TXT, COL_DIM, COL_A, COL_B, COL_H};
			nvgFillColor(args.vg, cs[r.color]);
			const Vec p = mm2px(Vec(r.x, r.y));
			nvgText(args.vg, p.x, p.y, r.txt, nullptr);
		}
	}
};

// ============================================================ panel (24 HP)
static const float C6[6] = {11.f, 31.f, 51.f, 71.f, 91.f, 110.9f};
static const float C8[8] = {8.5f, 23.5f, 38.5f, 53.4f, 68.4f, 83.4f, 98.4f, 113.4f};

struct LyapunovWidget : ModuleWidget {
	LyapunovWidget(Lyapunov* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Lyapunov.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		Pantalla* p = new Pantalla;
		p->module = module;
		p->box.pos = mm2px(Vec(4.f, 9.5f));
		p->box.size = mm2px(Vec(113.92f, 32.f));
		addChild(p);

		typedef Lyapunov L;
		// fila 1: la física
		const int k1[6] = {L::F0_PARAM, L::BASE_PARAM, L::ENERGIA_PARAM, L::IMPULSO_PARAM, L::D0_PARAM, L::TIEMPO_PARAM};
		for (int i = 0; i < 6; ++i) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(C6[i], 51.f)), module, k1[i]));
		// fila 2: la escucha
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(C6[0], 66.5f)), module, L::MEZCLA_PARAM));
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(C6[1], 66.5f)), module, L::AFIN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(C6[2], 66.5f)), module, L::ATRACCION_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(C6[3], 66.5f)), module, L::REGIMEN_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(C6[4], 66.5f)), module, L::SONIDO_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(C6[5], 66.5f)), module, L::NIVEL_PARAM));
		// gestos: botón + disparo
		const int lz[4] = {L::SOLTAR_LIGHT, L::PERTURBAR_LIGHT, L::SINCRO_LIGHT, L::GOLPE_LIGHT};
		const int ti[4] = {L::SOLTAR_INPUT, L::PERTURBAR_INPUT, L::SINCRO_INPUT, L::GOLPE_INPUT};
		for (int i = 0; i < 4; ++i) {
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(C8[2 * i], 80.5f)), module, L::SOLTAR_PARAM + i, lz[i]));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(C8[2 * i + 1], 80.5f)), module, ti[i]));
		}
		// entradas de control (mismo orden que las perillas)
		const int ci[8] = {L::VOCT_INPUT, L::F0_INPUT, L::ENERGIA_INPUT, L::IMPULSO_INPUT, L::D0_INPUT, L::TIEMPO_INPUT, L::MEZCLA_INPUT, L::ATRACCION_INPUT};
		for (int i = 0; i < 8; ++i) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(C8[i], 94.2f)), module, ci[i]));
		// salidas
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(C8[0], 105.6f)), module, L::CONGELAR_INPUT));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(C8[0] + 4.6f, 101.6f)), module, L::CONGELAR_LIGHT));
		const int o1[7] = {L::DELTA_OUTPUT, L::LAMBDA_OUTPUT, L::HORIZONTE_OUTPUT, L::ESTADO_OUTPUT, L::A_OUTPUT, L::B_OUTPUT, L::MEZCLA_OUTPUT};
		for (int i = 0; i < 7; ++i) addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(C8[i + 1], 105.6f)), module, o1[i]));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(C8[3] + 4.6f, 101.6f)), module, L::HORIZONTE_LIGHT));
		const int o2[4] = {L::NOTA_A_OUTPUT, L::GATE_A_OUTPUT, L::NOTA_B_OUTPUT, L::GATE_B_OUTPUT};
		for (int i = 0; i < 4; ++i) addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(C8[i + 4], 116.8f)), module, o2[i]));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(C8[5] + 4.6f, 112.8f)), module, L::GATE_A_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(C8[7] + 4.6f, 112.8f)), module, L::GATE_B_LIGHT));

		// rótulos
		Rotulos* r = new Rotulos;
		r->box.size = box.size;
		const float t = 7.2f, s = 6.4f;
		r->lista = {
			{12.f, 4.6f, 13.f, "LYAPUNOV", -1, 0},
			{109.5f, 3.2f, 6.2f, "ESPECTROS · péndulos gemelos", 1, 1},
			{109.5f, 6.3f, 5.6f, "dos casi iguales · cuándo dejan de serlo", 1, 1},
			{C6[0], 44.6f, t, "f0", 0, 0}, {C6[1], 44.6f, t, "BASE", 0, 0}, {C6[2], 44.6f, t, "ENERGÍA", 0, 0},
			{C6[3], 44.6f, t, "IMPULSO", 0, 0}, {C6[4], 44.6f, t, "δ0", 0, 0}, {C6[5], 44.6f, t, "TIEMPO", 0, 0},
			{C6[0], 59.6f, t, "MEZCLA", 0, 0}, {C6[1], 59.6f, t, "AFINACIÓN", 0, 0}, {C6[2], 59.6f, t, "ATRACCIÓN", 0, 0},
			{C6[3], 59.6f, t, "RÉGIMEN", 0, 0}, {C6[4], 59.6f, t, "SONIDO", 0, 0}, {C6[5], 59.6f, t, "NIVEL", 0, 0},
			{C6[0] - 6.3f, 72.f, 5.4f, "A", 0, 2}, {C6[0] + 6.3f, 72.f, 5.4f, "A−B", 0, 3},
			{C6[3] + 4.f, 62.2f, 5.4f, "div", -1, 1}, {C6[3] + 4.f, 66.6f, 5.4f, "grand", -1, 1}, {C6[3] + 4.f, 71.f, 5.4f, "peq", -1, 1},
			{C6[4] + 4.f, 63.6f, 5.4f, "FM", -1, 1}, {C6[4] + 4.f, 69.6f, 5.4f, "audif", -1, 1},
			{(C8[0] + C8[1]) / 2, 75.4f, s, "SOLTAR", 0, 0}, {(C8[2] + C8[3]) / 2, 75.4f, s, "PERTURBAR", 0, 0},
			{(C8[4] + C8[5]) / 2, 75.4f, s, "SINCRO", 0, 0}, {(C8[6] + C8[7]) / 2, 75.4f, s, "GOLPE", 0, 0},
			{C8[0], 88.8f, s, "V/OCT", 0, 0}, {C8[1], 88.8f, s, "f0", 0, 1}, {C8[2], 88.8f, s, "ENERG", 0, 1}, {C8[3], 88.8f, s, "IMPUL", 0, 1},
			{C8[4], 88.8f, s, "δ0", 0, 1}, {C8[5], 88.8f, s, "TIEMPO", 0, 1}, {C8[6], 88.8f, s, "MEZCLA", 0, 1}, {C8[7], 88.8f, s, "ATRAC", 0, 1},
			{C8[0], 100.2f, s, "CONGEL", 0, 4}, {C8[1], 100.2f, s, "δ", 0, 0}, {C8[2], 100.2f, s, "λ", 0, 0}, {C8[3], 100.2f, s, "HORIZ", 0, 4},
			{C8[4], 100.2f, s, "ESTADO", 0, 0}, {C8[5], 100.2f, s, "A", 0, 2}, {C8[6], 100.2f, s, "B", 0, 3}, {C8[7], 100.2f, s, "MEZCLA", 0, 0},
			{C8[4], 111.4f, s, "NOTA A", 0, 2}, {C8[5], 111.4f, s, "GATE A", 0, 2}, {C8[6], 111.4f, s, "NOTA B", 0, 3}, {C8[7], 111.4f, s, "GATE B", 0, 3},
			{4.5f, 110.6f, 5.6f, "Ph.D. Mauricio Vásquez Arias", -1, 0},
			{4.5f, 113.7f, 4.8f, "Profesor titular – Investigador asociado", -1, 1},
			{4.5f, 116.5f, 4.8f, "Escuela de Artes y Humanidades · EAFIT", -1, 1},
		};
		addChild(r);
	}

	void appendContextMenu(Menu* menu) override {
		Lyapunov* m = getModule<Lyapunov>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Afinación: " + m->nombreAfinacion()));
		const int k = clamp((int)std::round(m->params[Lyapunov::AFIN_PARAM].getValue()), 0, AFIN_SCALA);
		if (k < N_AFINACIONES && AFINACIONES[k].nota[0]) menu->addChild(createMenuLabel(AFINACIONES[k].nota));
		menu->addChild(createMenuItem("Cargar afinación Scala (.scl)…", "", [=]() {
			osdialog_filters* filtros = osdialog_filters_parse("Scala:scl");
			char* ruta = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filtros);
			osdialog_filters_free(filtros);
			if (!ruta) return;
			std::string error;
			if (!m->cargarScala(ruta, error)) osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, ("No se pudo leer la afinación: " + error).c_str());
			std::free(ruta);
		}));
		menu->addChild(createBoolPtrMenuItem("En divergencia, volver a soltar tras el horizonte", "", &m->autoSoltar));
	}
};

Model* modelLyapunov = createModel<Lyapunov, LyapunovWidget>("Lyapunov");
