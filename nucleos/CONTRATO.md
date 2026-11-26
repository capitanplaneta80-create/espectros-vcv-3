# Contrato de los núcleos ESPECTROS

**Ph.D. Mauricio Vásquez Arias**
Profesor titular – Investigador asociado · Escuela de Artes y Humanidades, Universidad EAFIT

Esta es la constante de diseño de todos los instrumentos. Cada instrumento tiene un **núcleo**: un archivo C++ que hace todo el sonido y la lógica, y que no sabe en qué aparato corre. Cada plataforma tiene un **adaptador delgado** que solo traduce perillas, voltajes y teclas a parámetros del núcleo, y dibuja lo que el núcleo expone.

```
                    nucleos/<instrumento>/Nucleo.hpp
                    nucleos/comun/  (salida, afinaciones, Scala)
                                 │
     ┌─────────────────┬─────────┴────────┬──────────────────┐
  src/ (VCV Rack)   MetaModule       Daisy patch.Init()   Cardputer (ESP32-S3)
  módulo + panel    mismo src/,      perillas + CV         teclado + pantalla
                    SDK de 4ms       → núcleo              → núcleo
```

Un puerto nuevo no reescribe el instrumento: escribe un adaptador.

## Reglas del núcleo

1. **Sin dependencias de plataforma.** Solo `<math.h>`, `<stdint.h>`, `<string.h>` y otros núcleos. Nada de `rack.hpp`, `Arduino.h`, `daisy_*.h`, M5 ni FreeRTOS.
2. **C++11.** Es el mínimo común: VCV compila con C++11, y Daisy, MetaModule y el ESP32 aceptan versiones posteriores.
3. **Sin memoria dinámica en el audio.** Ni `new`, ni `malloc`, ni `std::vector` o `std::string` dentro del núcleo. Todo el estado es de tamaño fijo y vive en la instancia.
4. **Sin excepciones ni RTTI.** Debe compilar con `-fno-exceptions -fno-rtti`.
5. **`float` en el audio.** El ESP32-S3 y el Cortex-M7 de Daisy solo tienen FPU de precisión simple. `double` solo se admite en acumuladores de tiempo a tasa de control.
6. **Azar propio y reproducible.** Generador interno con semilla (xorshift). Nunca `rand()`.
7. **Estado por instancia.** Varias copias del instrumento conviven en un patch, sin variables globales mutables.
8. **Presupuesto de memoria:** el estado cabe en **32 KB**. El Cardputer ADV no tiene PSRAM: tiene unos 320 KB de RAM para todo.
9. **Nunca satura.** Toda salida de audio pasa por `comun/Salida.hpp` (paso alto, compresor suave y limitador con anticipación).

`comun/Scala.hpp` es la única excepción a la regla 3: lee archivos `.scl` fuera del hilo de audio y usa la STL. Solo lo incluyen los adaptadores, que le pasan al núcleo la escala ya convertida a cents.

## Forma del núcleo

```cpp
namespace <instrumento> {
class Nucleo {
 public:
  // parámetros: miembros públicos en unidades físicas o musicales (Hz, s, 0–1), no en voltios
  void init(float sr);                       // una vez, o al cambiar la frecuencia de muestreo
  void process(float& izq, float& der, ...); // una muestra; la tasa de control la lleva el núcleo
  void <evento>();                           // gestos: soltar, estimular, sacudir…
  // lectura para la interfaz: estado que cada adaptador dibuja a su manera
};
}
```

- **Los voltios son del adaptador.** El núcleo recibe Hz, segundos y fracciones. El adaptador de VCV convierte V/OCT y ±5 V, el de Daisy sus 0–5 V, y el del Cardputer sus teclas.
- **La pantalla es del adaptador.** El núcleo expone lo que hay que ver (trazas, neuronas, niveles), y cada plataforma lo dibuja con su resolución.

## Cómo se verifica

`tools/portabilidad.sh` comprueba cada núcleo, y la CI lo corre en cada cambio:

- que compile en el computador con `-fno-exceptions -fno-rtti`;
- que compile para **Cortex-M7** (Daisy) con `arm-none-eabi-g++`;
- que no incluya cabeceras de plataforma;
- su tamaño en memoria frente al presupuesto de 32 KB.

Además, cada núcleo tiene su prueba en `test/`: corre minutos de audio sin el adaptador y verifica que no haya NaN, que no sature y cuánto CPU usa.

| Núcleo | Estado en memoria | CPU en el computador | Cortex-M7 |
|---|---|---|---|
| LYAPUNOV | 20 KB | 0,7 % de un núcleo | compila |
| GANGLIO | 15 KB (con canto, lectura, sentidos y plasticidad) | 1,2 % de un núcleo | compila |
| RYDBERG | 29 KB | 1,1 % de un núcleo | compila |
