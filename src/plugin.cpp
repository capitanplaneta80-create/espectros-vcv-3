#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelLyapunov);
	p->addModel(modelGanglio);
	p->addModel(modelRydberg);
}
