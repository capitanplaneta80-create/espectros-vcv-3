// GANGLIO — un cerebro de 8 neuronas que late solo y mueve el sonido · ESPECTROS para VCV Rack
// Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado
// Escuela de Artes y Humanidades, Universidad EAFIT
//
// Adaptador delgado: todo el sonido y la red viven en nucleos/ganglio/Nucleo.hpp (el mismo núcleo que usan
// MetaModule, Daisy y el Cardputer). Aquí solo se traducen perillas y voltajes, y se dibuja la pantalla.
#include "plugin.hpp"
#include "ganglio/Nucleo.hpp"

using namespace ganglio;

struct Ganglio : Module {
	enum ParamId {
		TEMP_PARAM, METAB_PARAM, INTENS_PARAM, RUIDO_PARAM, TONICA_PARAM, NIVEL_PARAM,
		ACORDE_PARAM, FILTRO_PARAM, TIMBRE_PARAM, ESCUCHA_PARAM, BORDON_PARAM, CONGELAR_PARAM,
		SACUDIR_PARAM, MUTAR_PARAM, NUEVA_PARAM, FABRICA_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT, TEMP_INPUT, METAB_INPUT, INTENS_INPUT, RUIDO_INPUT, ESTIMULO_INPUT, MEL_VOCT_INPUT, MEL_GATE_INPUT,
		SACUDIR_INPUT, MUTAR_INPUT, NUEVA_INPUT, FABRICA_INPUT, CONGELAR_INPUT, OIDO_INPUT,
		INPUTS_LEN
	};
	enum OutputId { NEURONAS_OUTPUT, DISPAROS_OUTPUT, PULSO_OUTPUT, OIDO_OUTPUT, L_OUTPUT, R_OUTPUT, OUTPUTS_LEN };
	enum LightId {
		SACUDIR_LIGHT, MUTAR_LIGHT, NUEVA_LIGHT, FABRICA_LIGHT, ESCUCHA_LIGHT, BORDON_LIGHT, CONGELAR_LIGHT, PULSO_LIGHT,
		LIGHTS_LEN
	};

	Nucleo nucleo;
	dsp::ClockDivider control;
	dsp::SchmittTrigger trig[4], melGate;
	dsp::BooleanTrigger boton[4];
	dsp::PulseGenerator pulsoLuz[4], pulso, disparo[N];
	bool gateMel = false;

	Ganglio() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> temps;
		for (int i = 0; i < N_TEMPERAMENTOS; ++i) temps.push_back(std::string(TEMPERAMENTOS[i].nombre) + " · " + TEMPERAMENTOS[i].caracter);
		auto* sq = configSwitch(TEMP_PARAM, 0.f, N_TEMPERAMENTOS - 1.f, 0.f, "Temperamento", temps);
		sq->description = "Ocho redes vivas, encontradas por búsqueda. Al cambiar, la red transita 1,5 s hacia la nueva, sin saltos.";
		auto* p = configParam(METAB_PARAM, -3.f, 3.f, 0.f, "Metabolismo", "×", 2.f);
		p->description = "Escala todo el tiempo de la red, de ×0,125 a ×8. TERMITA a ×8 se vuelve un pulso.";
		p = configParam(INTENS_PARAM, 0.25f, 1.5f, 1.f, "Intensidad", "%", 0.f, 100.f);
		p->description = "Cuánto mueve el cerebro a la síntesis.";
		p = configParam(RUIDO_PARAM, 0.f, 3.f, 0.5f, "Ruido sináptico");
		p->description = "Pequeñas fluctuaciones al azar: ni las redes periódicas se repiten exactamente.";
		p = configParam(TONICA_PARAM, std::log2(27.5f), std::log2(220.f), std::log2(55.f), "Tónica del bordón", " Hz", 2.f);
		configParam(NIVEL_PARAM, 0.f, 1.5f, 1.f, "Nivel", "%", 0.f, 100.f);
		std::vector<std::string> acordes(NOMBRE_ACORDE, NOMBRE_ACORDE + N_ACORDES);
		auto* sa = configSwitch(ACORDE_PARAM, 0.f, N_ACORDES - 1.f, 0.f, "Acorde del bordón", acordes);
		sa->description = "Quinta, cuarta, séptima natural (4:5:6:7), armónicos de la tónica o racimo de cuartos de tono.";
		configSwitch(FILTRO_PARAM, 0.f, 2.f, 0.f, "Filtro", {"Paso bajo (LP)", "Paso banda (BP)", "Ambos (LB)"});
		std::vector<std::string> timbres(NOMBRE_TIMBRE, NOMBRE_TIMBRE + N_TIMBRES);
		configSwitch(TIMBRE_PARAM, 0.f, N_TIMBRES - 1.f, 2.f, "Timbre de la melodía", timbres);
		configSwitch(ESCUCHA_PARAM, 0.f, 1.f, 1.f, "Escucha: el sonido vuelve al cerebro", {"No", "Sí"});
		configSwitch(BORDON_PARAM, 0.f, 1.f, 1.f, "Bordón", {"Callado", "Suena"});
		configSwitch(CONGELAR_PARAM, 0.f, 1.f, 0.f, "Congelar el cerebro", {"No", "Sí"});
		configButton(SACUDIR_PARAM, "Sacudir la red");
		configButton(MUTAR_PARAM, "Mutar la red actual");
		configButton(NUEVA_PARAM, "Red nueva al azar (4 pares osciladores)");
		configButton(FABRICA_PARAM, "Volver al temperamento de fábrica");

		configInput(VOCT_INPUT, "V/OCT de la tónica");
		configInput(TEMP_INPUT, "Temperamento · 1 V por paso");
		configInput(METAB_INPUT, "Metabolismo · 1 V por duplicación");
		configInput(INTENS_INPUT, "Intensidad · 10 V = +125 %");
		configInput(RUIDO_INPUT, "Ruido · 10 V = +3");
		auto* e = configInput(ESTIMULO_INPUT, "Estímulo · polifónico, canal i → neurona i (±5 V)");
		e->description = "Positivo estimula, negativo inhibe. Un cable mono estimula a las 8 neuronas por igual.";
		configInput(MEL_VOCT_INPUT, "Melodía · V/OCT");
		configInput(MEL_GATE_INPUT, "Melodía · compuerta (cada nota da un pulso a una neurona)");
		configInput(SACUDIR_INPUT, "Sacudir (disparo)");
		configInput(MUTAR_INPUT, "Mutar (disparo)");
		configInput(NUEVA_INPUT, "Red nueva (disparo)");
		configInput(FABRICA_INPUT, "Fábrica (disparo)");
		configInput(CONGELAR_INPUT, "Congelar (compuerta)");
		auto* o = configInput(OIDO_INPUT, "Oído · audio externo que escucha el cerebro");
		o->description = "Conectado, el cerebro deja de oír su propio bordón y oye esto: otro instrumento, una voz, un sintetizador.";

		configOutput(NEURONAS_OUTPUT, "Neuronas · 8 canales, 0–10 V");
		configOutput(DISPAROS_OUTPUT, "Disparos · 8 canales: compuerta mientras cada neurona está alta");
		configOutput(PULSO_OUTPUT, "Pulso · disparo cada vez que una neurona se enciende");
		configOutput(OIDO_OUTPUT, "Oído · lo que escucha el cerebro (0–10 V)");
		configOutput(L_OUTPUT, "Izquierda");
		configOutput(R_OUTPUT, "Derecha");
		configBypass(OIDO_INPUT, L_OUTPUT);

		control.setDivision(16);
		nucleo.init(48000.f);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { nucleo.frecuencia(e.sampleRate); }
	void onReset(const ResetEvent& e) override { Module::onReset(e); nucleo.restaurar(); }

	float entrada(int id, float escala) { return inputs[id].isConnected() ? inputs[id].getVoltage() * escala : 0.f; }

	void process(const ProcessArgs& args) override {
		if (control.process()) {
			nucleo.temperamento = clamp((int)std::round(params[TEMP_PARAM].getValue() + entrada(TEMP_INPUT, 1.f)), 0, N_TEMPERAMENTOS - 1);
			nucleo.metabolismo = std::pow(2.f, clamp(params[METAB_PARAM].getValue() + entrada(METAB_INPUT, 1.f), -3.f, 3.f));
			nucleo.intensidad = clamp(params[INTENS_PARAM].getValue() + entrada(INTENS_INPUT, 0.125f), 0.25f, 1.5f);
			nucleo.ruido = clamp(params[RUIDO_PARAM].getValue() + entrada(RUIDO_INPUT, 0.3f), 0.f, 3.f);
			nucleo.tonicaHz = clamp(std::pow(2.f, params[TONICA_PARAM].getValue() + entrada(VOCT_INPUT, 1.f)), 8.f, 2000.f);
			nucleo.nivel = params[NIVEL_PARAM].getValue();
			nucleo.acorde = (int)params[ACORDE_PARAM].getValue();
			nucleo.filtro = (int)params[FILTRO_PARAM].getValue();
			nucleo.timbre = (int)params[TIMBRE_PARAM].getValue();
			nucleo.escucha = params[ESCUCHA_PARAM].getValue() > 0.5f;
			nucleo.bordon = params[BORDON_PARAM].getValue() > 0.5f;
			nucleo.congelado = params[CONGELAR_PARAM].getValue() > 0.5f || inputs[CONGELAR_INPUT].getVoltage() >= 1.f;
			nucleo.escuchaExterna = inputs[OIDO_INPUT].isConnected();
			// estímulo: canal i → neurona i; mono → todas
			const int ch = inputs[ESTIMULO_INPUT].getChannels();
			for (int i = 0; i < N; ++i) {
				const float v = ch == 0 ? 0.f : inputs[ESTIMULO_INPUT].getVoltage(ch == 1 ? 0 : std::min(i, ch - 1));
				nucleo.estimulo[i] = (ch > 1 && i >= ch) ? 0.f : clamp(v, -10.f, 10.f) * 0.8f;
			}
			lights[ESCUCHA_LIGHT].setBrightness(nucleo.escucha ? 1.f : 0.f);
			lights[BORDON_LIGHT].setBrightness(nucleo.bordon ? 1.f : 0.f);
			lights[CONGELAR_LIGHT].setBrightness(nucleo.congelado ? 1.f : 0.f);
		}

		// gestos: botón + disparo
		const int ids[4] = {SACUDIR_INPUT, MUTAR_INPUT, NUEVA_INPUT, FABRICA_INPUT};
		for (int i = 0; i < 4; ++i) {
			const bool t = trig[i].process(inputs[ids[i]].getVoltage(), 0.1f, 1.f);
			const bool b = boton[i].process(params[SACUDIR_PARAM + i].getValue() > 0.f);
			if (!(t || b)) continue;
			pulsoLuz[i].trigger(0.12f);
			if (i == 0) nucleo.sacudir();
			else if (i == 1) nucleo.mutar();
			else if (i == 2) nucleo.redNueva();
			else nucleo.restaurar();
		}

		// melodía
		if (inputs[MEL_GATE_INPUT].isConnected()) {
			const float hz = dsp::FREQ_C4 * std::pow(2.f, clamp(inputs[MEL_VOCT_INPUT].getVoltage(), -5.f, 5.f));
			const bool alta = melGate.process(inputs[MEL_GATE_INPUT].getVoltage(), 0.1f, 1.f);
			if (alta) { nucleo.notaOn(hz); gateMel = true; }
			else if (gateMel && inputs[MEL_GATE_INPUT].getVoltage() < 0.1f) { nucleo.notaOff(); gateMel = false; }
			else if (gateMel) nucleo.tonoMelodia(hz);
		} else if (gateMel) { nucleo.notaOff(); gateMel = false; }

		nucleo.entrada = clamp(inputs[OIDO_INPUT].getVoltage() * 0.1f, -1.f, 1.f);
		float l, r;
		nucleo.process(l, r);
		outputs[L_OUTPUT].setVoltage(l * 10.f);
		outputs[R_OUTPUT].setVoltage(r * 10.f);

		// el cerebro como voltaje
		outputs[NEURONAS_OUTPUT].setChannels(N);
		outputs[DISPAROS_OUTPUT].setChannels(N);
		bool alguna = false;
		for (int i = 0; i < N; ++i) {
			outputs[NEURONAS_OUTPUT].setVoltage(nucleo.o[i] * 10.f, i);
			outputs[DISPAROS_OUTPUT].setVoltage((nucleo.activas & (1 << i)) ? 10.f : 0.f, i);
			if (nucleo.cruces & (1 << i)) alguna = true;
		}
		if (alguna && !pulsoArmado) pulso.trigger(0.005f);
		pulsoArmado = alguna;
		const float st = args.sampleTime;
		const bool p = pulso.process(st);
		outputs[PULSO_OUTPUT].setVoltage(p ? 10.f : 0.f);
		outputs[OIDO_OUTPUT].setVoltage(clamp(nucleo.nivelBordon * 10.f, 0.f, 10.f));
		if (p) pulsoVisto.trigger(0.08f);
		lights[PULSO_LIGHT].setBrightnessSmooth(pulsoVisto.process(st) ? 1.f : 0.f, st);
		for (int i = 0; i < 4; ++i) lights[SACUDIR_LIGHT + i].setBrightnessSmooth(pulsoLuz[i].process(st) ? 1.f : 0.f, st);
	}
	bool pulsoArmado = false;
	dsp::PulseGenerator pulsoVisto;

	// la red mutada o nueva se guarda con el patch
	json_t* dataToJson() override {
		json_t* r = json_object();
		if (nucleo.mutado) {
			float red[Nucleo::TAM_RED];
			nucleo.exportarRed(red);
			json_t* a = json_array();
			for (int i = 0; i < Nucleo::TAM_RED; ++i) json_array_append_new(a, json_real(red[i]));
			json_object_set_new(r, "red", a);
		}
		return r;
	}
	void dataFromJson(json_t* r) override {
		json_t* a = json_object_get(r, "red");
		if (!a || json_array_size(a) != (size_t)Nucleo::TAM_RED) return;
		float red[Nucleo::TAM_RED];
		for (int i = 0; i < Nucleo::TAM_RED; ++i) red[i] = (float)json_number_value(json_array_get(a, i));
		nucleo.temperamento = clamp((int)std::round(params[TEMP_PARAM].getValue()), 0, N_TEMPERAMENTOS - 1);
		nucleo.importarRed(red);
	}
};

// ============================================================ estética
// obsidiana con oro y turquesa, como la identidad de Ecotono: oro = excitación, turquesa = inhibición
static const NVGcolor COL_ORO = nvgRGB(0xe8, 0xb0, 0x4b);
static const NVGcolor COL_TUR = nvgRGB(0x2e, 0xc4, 0xb6);
static const NVGcolor COL_NAR = nvgRGB(0xf0, 0xa0, 0x7e);
static const NVGcolor COL_TXT = nvgRGB(0xf2, 0xe6, 0xd3);
static const NVGcolor COL_DIM = nvgRGB(0x9a, 0x8c, 0x78);
// color de cada neurona según lo que mueve: oro el bordón, naranja FM y plegado, turquesa el filtro
static NVGcolor colorNeurona(int i) { return i < 4 ? COL_ORO : (i < 6 ? COL_NAR : COL_TUR); }

static std::shared_ptr<window::Font> fuente() { return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf")); }

struct PantallaGanglio : TransparentWidget {
	Ganglio* module = nullptr;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3.f);
		nvgFillColor(args.vg, nvgRGB(0x07, 0x06, 0x05));
		nvgFill(args.vg);
		if (!module) dibujar(args, nullptr);
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) dibujar(args, &module->nucleo);
		Widget::drawLayer(args, layer);
	}

	void dibujar(const DrawArgs& args, const Nucleo* n) {
		NVGcontext* vg = args.vg;
		const float W = box.size.x, H = box.size.y;
		nvgSave(vg);
		nvgScissor(vg, 0, 0, W, H);
		// valores de muestra para la vista previa del navegador
		float o[N], tau[N], Wm[N][N];
		for (int i = 0; i < N; ++i) {
			o[i] = n ? n->o[i] : 0.5f + 0.45f * std::sin(i * 1.7f);
			tau[i] = n ? n->tau[i] : TEMPERAMENTOS[0].tau[i];
			for (int j = 0; j < N; ++j) Wm[i][j] = n ? n->W[i][j] : TEMPERAMENTOS[0].W[i][j];
		}

		// ---- el ganglio (izquierda): neuronas en círculo, conexiones según el signo del peso
		const float cx = H * 0.56f, cy = H * 0.53f, R = H * 0.36f;
		float px[N], py[N];
		for (int i = 0; i < N; ++i) { const float a = -PI_F / 2 + 2 * PI_F * i / N; px[i] = cx + R * std::cos(a); py[i] = cy + R * std::sin(a); }
		nvgGlobalCompositeOperation(vg, NVG_LIGHTER);
		for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) {
			if (i == j) continue;
			const float w = Wm[i][j];
			if (std::fabs(w) < 0.8f) continue;
			NVGcolor c = w > 0 ? COL_ORO : COL_TUR;
			c.a = clamp(0.08f + 0.5f * o[j] * std::min(std::fabs(w) / 6.f, 1.f), 0.f, 0.7f);
			nvgBeginPath(vg);
			// curva hacia el centro: se distinguen i→j y j→i
			const float mx = (px[i] + px[j]) / 2, my = (py[i] + py[j]) / 2;
			nvgMoveTo(vg, px[j], py[j]);
			nvgQuadTo(vg, mx + (cx - mx) * 0.35f, my + (cy - my) * 0.35f, px[i], py[i]);
			nvgStrokeColor(vg, c); nvgStrokeWidth(vg, 0.5f + std::min(std::fabs(w), 6.f) * 0.18f); nvgStroke(vg);
		}
		for (int i = 0; i < N; ++i) {
			const float rad = 1.8f + 3.2f * std::sqrt((tau[i] - 0.5f) / 14.5f);   // más grande = más lenta
			NVGcolor c = colorNeurona(i);
			NVGcolor h = c; h.a = 0.25f * o[i];
			nvgBeginPath(vg); nvgCircle(vg, px[i], py[i], rad + 3.f * o[i]); nvgFillColor(vg, h); nvgFill(vg);
			c.a = 0.25f + 0.75f * o[i];
			nvgBeginPath(vg); nvgCircle(vg, px[i], py[i], rad); nvgFillColor(vg, c); nvgFill(vg);
		}

		// ---- electroencefalograma (derecha): los últimos ~9 s de cada neurona
		const float gx = H * 1.18f, gw = W - gx - 5.f, gy = 11.f, gh = H - 21.f, lh = gh / N;
		nvgGlobalCompositeOperation(vg, NVG_SOURCE_OVER);
		for (int i = 0; i < N; ++i) {
			const float base = gy + lh * (i + 1) - 1.f;
			NVGcolor c = colorNeurona(i);
			NVGcolor g = COL_DIM; g.a = 0.15f;
			nvgBeginPath(vg); nvgMoveTo(vg, gx, base); nvgLineTo(vg, gx + gw, base); nvgStrokeColor(vg, g); nvgStrokeWidth(vg, 0.4f); nvgStroke(vg);
			nvgBeginPath(vg);
			if (n) {
				const int cnt = n->eegCuenta, cab = n->eegCabeza;
				for (int k = 0; k < cnt; ++k) {
					const float v = n->eeg[(cab - cnt + k + EEG) % EEG][i];
					const float x = gx + gw * (EEG - cnt + k) / (EEG - 1.f), yv = base - v * (lh * 1.5f);
					if (k == 0) nvgMoveTo(vg, x, yv); else nvgLineTo(vg, x, yv);
				}
			} else {
				for (int k = 0; k < 60; ++k) {
					const float x = gx + gw * k / 59.f, v = 0.5f + 0.45f * std::sin(k * (0.12f + 0.05f * i) + i);
					if (k == 0) nvgMoveTo(vg, x, base - v * (lh * 1.5f)); else nvgLineTo(vg, x, base - v * (lh * 1.5f));
				}
			}
			nvgStrokeColor(vg, c); nvgStrokeWidth(vg, 0.9f); nvgStroke(vg);
		}

		// ---- textos
		auto f = fuente();
		if (f && f->handle >= 0) {
			nvgFontFaceId(vg, f->handle);
			nvgFontSize(vg, 8.5f);
			char buf[96];
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			const int t = n ? n->temperamento : 0;
			snprintf(buf, sizeof buf, "%s%s", TEMPERAMENTOS[t].nombre, (n && n->mutado) ? " ~" : "");
			nvgFillColor(vg, COL_ORO); nvgText(vg, gx, 1.f, buf, nullptr);
			nvgFillColor(vg, COL_DIM);
			nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
			nvgText(vg, gx + gw, 1.f, (n && n->mutado) ? "red mutada" : TEMPERAMENTOS[t].caracter, nullptr);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			snprintf(buf, sizeof buf, "MET ×%.2f  %s %4.0f Hz Q%.1f  %s", n ? n->metabolismo : 1.f,
			         NOMBRE_FILTRO[n ? clamp(n->filtro, 0, 2) : 0], n ? n->fcVivo : 900.f, n ? n->qVivo : 2.f,
			         NOMBRE_ACORDE[n ? clamp(n->acorde, 0, N_ACORDES - 1) : 0]);
			nvgText(vg, gx, H - 1.f, buf, nullptr);
			if (n && (n->despierta > 0.f || n->congelado)) {
				nvgFillColor(vg, COL_NAR);
				nvgText(vg, 3.f, H - 1.f, n->congelado ? "CONGELADO" : "despierta", nullptr);
			}
		}
		nvgResetScissor(vg);
		nvgRestore(vg);
	}
};

// rótulos del panel (texto vivo: el SVG de Rack no dibuja texto)
struct RotuloG { float x, y, tam; const char* txt; int alin; int color; };
struct RotulosG : TransparentWidget {
	std::vector<RotuloG> lista;
	void draw(const DrawArgs& args) override {
		auto f = fuente();
		if (!f || f->handle < 0) return;
		nvgFontFaceId(args.vg, f->handle);
		for (const RotuloG& r : lista) {
			nvgFontSize(args.vg, r.tam);
			nvgTextAlign(args.vg, (r.alin < 0 ? NVG_ALIGN_LEFT : r.alin > 0 ? NVG_ALIGN_RIGHT : NVG_ALIGN_CENTER) | NVG_ALIGN_MIDDLE);
			const NVGcolor cs[] = {COL_TXT, COL_DIM, COL_ORO, COL_TUR, COL_NAR};
			nvgFillColor(args.vg, cs[r.color]);
			const Vec p = mm2px(Vec(r.x, r.y));
			nvgText(args.vg, p.x, p.y, r.txt, nullptr);
		}
	}
};

// ============================================================ panel (24 HP)
static const float G6[6] = {11.f, 31.f, 51.f, 71.f, 91.f, 110.9f};
static const float G8[8] = {8.5f, 23.5f, 38.5f, 53.4f, 68.4f, 83.4f, 98.4f, 113.4f};

struct GanglioWidget : ModuleWidget {
	GanglioWidget(Ganglio* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ganglio.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		PantallaGanglio* p = new PantallaGanglio;
		p->module = module;
		p->box.pos = mm2px(Vec(4.f, 9.5f));
		p->box.size = mm2px(Vec(113.92f, 32.f));
		addChild(p);

		typedef Ganglio G;
		// fila 1: el cerebro
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(G6[0], 51.f)), module, G::TEMP_PARAM));
		const int k1[5] = {G::METAB_PARAM, G::INTENS_PARAM, G::RUIDO_PARAM, G::TONICA_PARAM, G::NIVEL_PARAM};
		for (int i = 0; i < 5; ++i) addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(G6[i + 1], 51.f)), module, k1[i]));
		// fila 2: el sonido
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(G6[0], 66.5f)), module, G::ACORDE_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(G6[1], 66.5f)), module, G::FILTRO_PARAM));
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(G6[2], 66.5f)), module, G::TIMBRE_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(G6[3], 66.5f)), module, G::ESCUCHA_PARAM, G::ESCUCHA_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(G6[4], 66.5f)), module, G::BORDON_PARAM, G::BORDON_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(mm2px(Vec(G6[5], 66.5f)), module, G::CONGELAR_PARAM, G::CONGELAR_LIGHT));
		// gestos: botón + disparo
		const int ti[4] = {G::SACUDIR_INPUT, G::MUTAR_INPUT, G::NUEVA_INPUT, G::FABRICA_INPUT};
		for (int i = 0; i < 4; ++i) {
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(G8[2 * i], 80.5f)), module, G::SACUDIR_PARAM + i, G::SACUDIR_LIGHT + i));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(G8[2 * i + 1], 80.5f)), module, ti[i]));
		}
		// entradas de control
		const int ci[8] = {G::VOCT_INPUT, G::TEMP_INPUT, G::METAB_INPUT, G::INTENS_INPUT, G::RUIDO_INPUT, G::ESTIMULO_INPUT, G::MEL_VOCT_INPUT, G::MEL_GATE_INPUT};
		for (int i = 0; i < 8; ++i) addInput(createInputCentered<PJ301MPort>(mm2px(Vec(G8[i], 94.2f)), module, ci[i]));
		// congelar, oído y salidas
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(G8[0], 105.6f)), module, G::CONGELAR_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(G8[1], 105.6f)), module, G::OIDO_INPUT));
		const int o1[6] = {G::NEURONAS_OUTPUT, G::DISPAROS_OUTPUT, G::PULSO_OUTPUT, G::OIDO_OUTPUT, G::L_OUTPUT, G::R_OUTPUT};
		for (int i = 0; i < 6; ++i) addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(G8[i + 2], 105.6f)), module, o1[i]));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(G8[4] + 4.6f, 101.6f)), module, G::PULSO_LIGHT));

		RotulosG* r = new RotulosG;
		r->box.size = box.size;
		const float t = 7.2f, s = 6.4f;
		r->lista = {
			{12.f, 4.6f, 13.f, "GANGLIO", -1, 2},
			{109.5f, 3.2f, 6.2f, "ESPECTROS · CTRNN de 8 neuronas", 1, 1},
			{109.5f, 6.3f, 5.6f, "un cerebro de insecto que late y suena", 1, 1},
			{G6[0], 44.6f, t, "TEMPERAM.", 0, 0}, {G6[1], 44.6f, t, "METABOL.", 0, 0}, {G6[2], 44.6f, t, "INTENSIDAD", 0, 0},
			{G6[3], 44.6f, t, "RUIDO", 0, 0}, {G6[4], 44.6f, t, "TÓNICA", 0, 0}, {G6[5], 44.6f, t, "NIVEL", 0, 0},
			{G6[0], 59.6f, t, "ACORDE", 0, 0}, {G6[1], 59.6f, t, "FILTRO", 0, 0}, {G6[2], 59.6f, t, "TIMBRE", 0, 0},
			{G6[3], 59.6f, t, "ESCUCHA", 0, 0}, {G6[4], 59.6f, t, "BORDÓN", 0, 0}, {G6[5], 59.6f, t, "CONGELAR", 0, 3},
			{G6[1] + 4.f, 62.2f, 5.4f, "LB", -1, 1}, {G6[1] + 4.f, 66.6f, 5.4f, "BP", -1, 1}, {G6[1] + 4.f, 71.f, 5.4f, "LP", -1, 1},
			{(G8[0] + G8[1]) / 2, 75.4f, s, "SACUDIR", 0, 0}, {(G8[2] + G8[3]) / 2, 75.4f, s, "MUTAR", 0, 0},
			{(G8[4] + G8[5]) / 2, 75.4f, s, "RED NUEVA", 0, 0}, {(G8[6] + G8[7]) / 2, 75.4f, s, "FÁBRICA", 0, 0},
			{G8[0], 88.8f, s, "V/OCT", 0, 0}, {G8[1], 88.8f, s, "TEMP", 0, 1}, {G8[2], 88.8f, s, "METAB", 0, 1}, {G8[3], 88.8f, s, "INTENS", 0, 1},
			{G8[4], 88.8f, s, "RUIDO", 0, 1}, {G8[5], 88.8f, s, "ESTÍMULO", 0, 2}, {G8[6], 88.8f, s, "MEL V/O", 0, 4}, {G8[7], 88.8f, s, "MEL GATE", 0, 4},
			{G8[0], 100.2f, s, "CONGEL", 0, 3}, {G8[1], 100.2f, s, "OÍDO", 0, 3}, {G8[2], 100.2f, s, "NEURONAS", 0, 2}, {G8[3], 100.2f, s, "DISPAROS", 0, 2},
			{G8[4], 100.2f, s, "PULSO", 0, 0}, {G8[5], 100.2f, s, "OÍDO", 0, 0}, {G8[6], 100.2f, s, "IZQ", 0, 0}, {G8[7], 100.2f, s, "DER", 0, 0},
			{4.5f, 113.6f, 5.6f, "Ph.D. Mauricio Vásquez Arias", -1, 0},
			{4.5f, 116.7f, 4.8f, "Profesor titular – Investigador asociado", -1, 1},
			{4.5f, 119.5f, 4.8f, "Escuela de Artes y Humanidades · EAFIT", -1, 1},
			{117.5f, 116.7f, 4.8f, "oro: excita · turquesa: inhibe", 1, 1},
			{117.5f, 119.5f, 4.8f, "tamaño = lentitud · brillo = actividad", 1, 1},
		};
		addChild(r);
	}

	void appendContextMenu(Menu* menu) override {
		Ganglio* m = getModule<Ganglio>();
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(std::string("Temperamento: ") + m->nucleo.nombreTemperamento() + (m->nucleo.mutado ? " (red mutada, se guarda con el patch)" : "")));
		menu->addChild(createMenuLabel("Neuronas 1–4: bordón · 5: FM · 6: plegado · 7: corte · 8: resonancia"));
	}
};

Model* modelGanglio = createModel<Ganglio, GanglioWidget>("Ganglio");
