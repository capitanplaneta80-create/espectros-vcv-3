# Compilación estándar de VCV Rack (requiere el Rack SDK): make RACK_DIR=/ruta/al/Rack-SDK install
RACK_DIR ?= ../..
FLAGS += -Inucleos
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
include $(RACK_DIR)/plugin.mk
