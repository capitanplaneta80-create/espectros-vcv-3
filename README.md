# ESPECTROS para VCV Rack · LYAPUNOV, GANGLIO y RYDBERG

**Ph.D. Mauricio Vásquez Arias**
Profesor titular – Investigador asociado · Escuela de Artes y Humanidades, Universidad EAFIT

LYAPUNOV suelta dos péndulos dobles casi idénticos. Lo que suena, lo que se ve y lo que sale como voltaje es cómo se separan:

- los gemelos son el estéreo;
- cada uno compone sus propias notas en una afinación no temperada;
- el instante en que dejan de parecerse, el **horizonte de predictibilidad**, es un evento que se puede patchear.

## Qué lo distingue

Otros módulos de VCV Rack ya usan el péndulo doble: Loco CHAOS, Vult Caudal y SignalFunctionSet Gravity. Lo usan como fuente de voltajes caóticos. LYAPUNOV se construye sobre otra pregunta: **¿cuándo deja de ser predecible un sistema determinista?** De ahí sale lo que lo hace propio:

- **Dos gemelos, no uno.** A y B se integran a la vez, separados por δ0.
- **La separación se mide.** δ, el exponente λ y el horizonte salen como voltaje.
- **Estéreo que nace del caos.** A a la izquierda, B a la derecha: empiezan en mono y se abren solos.
- **Dos compositores.** Cada gemelo produce sus notas en 38 afinaciones no occidentales, no temperadas o espectrales, más archivos Scala. Coinciden hasta el horizonte y luego se separan.
- **Escuchar solo el caos.** En A−B no suena nada mientras los gemelos coinciden.
- **Instrumento de audio.** Audificación o FM a frecuencia audible, con limitador que nunca satura.
- **Pantalla que explica.** Los dos péndulos con sus estelas, y log δ desde SOLTAR: la pendiente de esa recta es λ.

## Principios de diseño aplicados

- **Sonoridad, matemática, expresividad.**
  - La fórmula no es una metáfora: es la ecuación de Euler-Lagrange, integrada con Runge-Kutta 4, con dos péndulos a la vez.
  - Lo que se mide (δ, λ, horizonte) sale como voltaje.
  - Los gestos (soltar, perturbar, sincronizar, golpear, congelar) son el instrumento.
- **Microtonalidad y espectralismo como principios compositivos.**
  - La afinación no es un filtro final: las notas de los vuelcos y las portadoras de FM nacen en los grados de la escala.
  - La perilla ATRACCIÓN va de las alturas que da la física (0 %) a la escala fija (100 %).
  - BASE es la raíz, y V/OCT transpone la escala entera.
- **Comportamiento generativo.** El sistema toca solo. El intérprete no dicta notas: perturba, empuja y detiene un sistema que compone.
- **Ver lo que se escucha.** La pantalla es una lección de física: dos trayectorias que se superponen (se ven blancas) hasta que se abren en cian y magenta. Al lado, la recta de log δ cuya pendiente es el exponente de Lyapunov, y el horizonte marcado en ámbar.
- **Conservar la dinámica sin saturar.** Cada salida pasa por un paso alto, un compresor suave y un limitador con anticipación, como en la suite ESPECTROS.
- **Ligero.** Usa entre 0,4 % y 0,9 % de un núcleo. El integrador se subdivide solo cuando la frecuencia sube, para no perder la física.

## Instalación

1. Instala o actualiza **VCV Rack 2.6** o posterior (gratuito, en vcvrack.com) y ábrelo una vez.
2. Abre la biblioteca ESPECTROS en **Chrome**. El enlace está en «About», en la columna derecha de este repositorio.
3. Pulsa **INSTALAR EN VCV RACK**. La primera vez, elige la carpeta `Rack2`; la página explica dónde está.
4. Abre VCV Rack y busca **LYAPUNOV** (marca ESPECTROS) en el navegador de módulos.

En otros navegadores, usa «Descargar el archivo» y copia el `.vcvplugin` en la carpeta de plugins de Rack. En Rack, esa carpeta se abre desde Ayuda → Abrir carpeta de usuario.

Cada cambio que llega a este repositorio se compila solo para Mac, Linux y Windows con GitHub Actions, y la biblioteca se actualiza. Para compilar a mano con el Rack SDK: `make RACK_DIR=/ruta/al/Rack-SDK install`.

## El panel (24 HP)

### Fila 1 · la física

| Control | Qué hace |
|---|---|
| **f0** | Frecuencia natural del péndulo (0,05–220 Hz). Es la gravedad: el tempo del caos. Baja, es un gesto lento para FM y voltajes; alta, la oscilación se oye. |
| **BASE** | Portadora de FM, raíz de la afinación y registro de las notas (27,5–880 Hz). |
| **ENERGÍA** | Altura desde la que se sueltan. Actúa en el próximo SOLTAR. |
| **IMPULSO** | Negativo: fricción, el caos se apaga. Cero: conservativo. Positivo: bombea energía, y el sistema nunca pasa de 4,5 g. |
| **δ0** | Separación inicial de los gemelos (10⁻⁵ a 10°). Más pequeña, más tarda el horizonte. |
| **TIEMPO** | Escala del tiempo físico (0–2×). En 0 la física se detiene. |

### Fila 2 · la escucha

| Control | Qué hace |
|---|---|
| **MEZCLA** | 0 = A · 1 = A+B · 2 = A−B. En A−B solo se oye la diferencia entre los gemelos. |
| **AFINACIÓN** | Selector de 38 afinaciones, más un archivo Scala propio. El nombre aparece en la pantalla y en el menú de clic derecho. |
| **ATRACCIÓN** | 0 = las alturas de la física · 100 % = los grados de la escala. |
| **RÉGIMEN** | **peq**: oscilaciones pequeñas, sin caos; los gemelos no se separan. **grand**: caos. **div**: caos que se vuelve a soltar solo al cruzar el horizonte; es un ciclo de predicción y pérdida. |
| **SONIDO** | **audif**: la segunda masa ES la forma de onda. **FM**: los ángulos modulan dos osciladores. Portadora ← θ2, moduladora ← θ1, índice ← \|sin(θ1−θ2)\|. |
| **NIVEL** | Nivel de salida. |

### Gestos (botón + entrada de disparo)

- **SOLTAR:** suelta los dos péndulos desde ENERGÍA.
- **PERTURBAR:** empuja a B desde donde está; la divergencia recomienza.
- **SINCRO:** hace B = A + δ0, como volver a empezar sin soltar.
- **GOLPE:** patea a los dos. El voltaje del disparo es la fuerza (10 V = 1).
- **CONGELAR** (entrada de compuerta, abajo a la izquierda): mientras está alta, el tiempo se detiene con un glissando breve.

### Entradas de control

- **V/OCT:** transpone BASE y la afinación. En audificación también transpone f0, así que el teclado toca el caos.
- **f0:** 1 V/oct.
- **ENERG:** 0–10 V.
- **IMPUL:** ±5 V.
- **δ0:** 2 V por década.
- **TIEMPO:** 5 V = ×1.
- **MEZCLA:** 5 V por paso.
- **ATRAC:** 0–10 V.

### Salidas (placa iluminada)

| Salida | |
|---|---|
| **A**, **B** | Cada gemelo por separado: el par estéreo. |
| **MEZCLA** | Según la perilla MEZCLA. |
| **δ** | Separación de los gemelos, logarítmica (0–10 V). Sube en rampa recta mientras son predecibles. |
| **λ** | Exponente de Lyapunov de tiempo finito (1 V = 0,1). |
| **HORIZ** | Compuerta alta desde que los gemelos dejan de parecerse (δ > 0,5). Su flanco es el instante de la pérdida de predicción. |
| **ESTADO** | Polifónica de 8 canales: x/y de las cuatro masas (±5 V). Útil para paneo, filtros o un osciloscopio XY. |
| **NOTA A / GATE A** | Cada vuelco de A (la segunda masa pasa por arriba) produce una nota en la afinación, en V/OCT con 0 V = C4. |
| **NOTA B / GATE B** | Lo mismo para B. Al principio coinciden con las de A; tras el horizonte, cada gemelo sigue su propia melodía. |

Clic derecho en el módulo:
- cargar una afinación Scala (.scl);
- ver la nota cultural de la afinación elegida;
- decidir si el régimen de divergencia se suelta solo.

La afinación Scala queda guardada en el patch.

## Afinaciones incluidas

- **Nativa:** sin atracción; la física decide.
- **Indonesia:** slendro, pelog, pelog bem, selisir.
- **India:** 22 shrutis, ragas Bhairav, Yaman, Todi y Marwa.
- **Mundo árabe:** maqam Rast, Bayati, Saba, Hijaz, Sikah; Safi al-Din (17).
- **Persia:** dastgah Shur y Chahargah.
- **Turquía:** makam Rast (53 comas), Uşşak, Hicaz.
- **Etiopía:** tezeta, bati, ambassel, anchihoye.
- **África:** mbira nyamaropa, amadinda, timbila.
- **Asia:** thai pi phat, modo gong, doce lü, hirajōshi, Ryūkyū.
- **Espectrales:** serie armónica 8–16, serie subarmónica, Bohlen-Pierce (tritava).
- **Referencia temperada:** 12-TET y 19-EDO.

## Ideas de patch

1. **Canon que se desgarra.** NOTA A y GATE A a una voz, NOTA B y GATE B a otra. Empiezan en unísono y, tras el horizonte, se vuelven dos melodías independientes. HORIZ dispara un cambio de sección.
2. **Estéreo de Lyapunov.** A y B a la mezcla, paneados a los extremos. Con δ0 muy pequeño el sonido nace en el centro y se abre lentamente; SINCRO lo vuelve a cerrar.
3. **Respiración.** δ → corte de un filtro: el timbre se abre a medida que se pierde la predicción. RÉGIMEN en div para que el ciclo se repita solo.
4. **Biofeedback.** Una señal lenta (por ejemplo, el Scíon) a IMPULSO o a TIEMPO: el cuerpo empuja o frena el caos. CONGELAR con un pedal para suspenderlo.
5. **Solo el caos.** MEZCLA en 2 (A−B) con FM y una afinación espectral. Mientras los gemelos coinciden hay silencio; el sonido aparece exactamente cuando dejan de ser iguales.

# GANGLIO

En los insectos, ritmos como el canto del grillo los producen **ganglios**: pequeños grupos de neuronas acopladas. GANGLIO es una red neuronal recurrente de tiempo continuo (CTRNN) de 8 neuronas que late sola y mueve un bordón. Es el mismo cerebro del firmware Ganglio del Cardputer, con sus ocho temperamentos.

## Qué lo distingue

En la biblioteca de VCV hay pocas redes neuronales, y ninguna es un sistema dinámico que se toque, se escuche y se vea a la vez. GANGLIO:

- **Late solo.** Cada red tiene 4 pares de neuronas que oscilan, acoplados entre sí, con sesgos *center-crossing*. Ninguna se apaga y un vigía la despierta si se queda quieta más de 10 s.
- **Se toca estimulando.** Cada canal de ESTÍMULO inyecta corriente en una neurona; un voltaje negativo la inhibe. Cada nota de la melodía da un pulso a una neurona.
- **Escucha.** El sonido vuelve al cerebro: oye los cambios de su propio bordón o, con OÍDO conectado, cualquier otro instrumento.
- **Se ve pensar.** A la izquierda, el ganglio: tamaño = lentitud (τ), brillo = actividad, conexiones en oro (excitan) y turquesa (inhiben). A la derecha, el electroencefalograma de los últimos 9 segundos.
- **Su cerebro sale como voltaje.** 8 canales continuos, 8 compuertas y un pulso para mover cualquier otro módulo.
- **Canta, lee, escucha y aprende (2.3).** Cada neurona que cruza el umbral canta una nota microtonal; la síntesis puede leer combinaciones de neuronas; el oído distingue nivel, brillo y ataques; y la red aprende por plasticidad hebbiana.

## Qué mueve cada neurona

| Neurona | Controla |
|---|---|
| 1–4 | Volumen de los 4 osciladores del bordón. Su desafinación sale de la diferencia entre vecinas: el batido aparece cuando dos neuronas no están de acuerdo |
| 5 | Índice de FM |
| 6 | Plegador de onda: ganancia y realimentación |
| 7 | Corte del filtro, 150 Hz a 7,3 kHz |
| 8 | Resonancia del filtro |

## Temperamentos

| Temperamento | Carácter |
|---|---|
| GRILLO | Pulso corto y regular |
| LIBÉLULA | Rápido e irregular |
| ENJAMBRE | El más sensible, casi caótico |
| POLILLA | Irregular y muy activo |
| MANTIS | Quieto, con sobresaltos |
| LUCIÉRNAGA | Destellos lentos (~19 s) |
| CIGARRA | Ciclo largo y amplio (~50 s) |
| TERMITA | Lento y obstinado |

Al cambiar de temperamento, la red transita 1,5 s hacia el nuevo. MUTAR altera la red actual y RED NUEVA crea otra con la misma arquitectura. Una red mutada se guarda con el patch; FÁBRICA vuelve al temperamento original.

## Cuatro capas sobre la red (versión 2.3)

### Canto: las neuronas como eventos

Cuando una neurona pasa del **UMBRAL** (50–90 %) canta una nota, y se calla 20 puntos más abajo.

- **Altura:** sale del acorde del bordón, con su desafinación viva. Las neuronas 1–4 cantan una octava arriba de su oscilador y las 5–8, dos octavas arriba. Las notas no están en temperamento igual.
- **Velocidad:** depende de lo rápido que subió la neurona. Un despertar brusco suena fuerte y uno lento, suave.
- **CANTA:** la voz interna toca esas notas con el TIMBRE elegido. CAMPANA es la más cercana a un grillo.
- **Salidas:** CANTO V/O y VEL (8 canales), ÚLTIMA (V/OCT monofónico, para un sintetizador de una voz o el CM1A). DISPAROS siguen el mismo umbral.

El núcleo también entrega las notas como una cola de eventos con Hz y velocidad, más una función que convierte Hz en nota MIDI con desvío en cents. Es lo que usará el Cardputer para tocar por MIDI, con pitch bend para los microtonos.

### Lectura: del mapeo uno a uno al poblacional

Con **MEZCLA** en 0, cada neurona mueve su parámetro, como siempre. En 100 %, cada parámetro lee una combinación de las 8 neuronas mediante una matriz de lectura: filas al azar de norma 1, con algo de peso en la diagonal. Así el timbre depende de *patrones* de actividad y no de neuronas sueltas.

- **NUEVA LECTURA** (botón y disparo) sortea otra matriz.
- La lectura se guarda con el patch, y el menú contextual la devuelve a la de fábrica.
- **CANALES** saca lo que realmente lee la síntesis (8 canales).

### Sentidos: un oído con tres órganos

Con ESCUCHA encendida, el cerebro oye su bordón o, si OÍDO está conectado, otro instrumento.

- **Nivel:** los cambios de volumen excitan la neurona 1 e inhiben la 8 (como antes).
- **Brillo:** es la fracción de energía sobre ~2 kHz. Cuando el sonido es más brillante que su promedio de 4 s, el cerebro cierra el filtro (neurona 7) y abre la FM (neurona 5). Con su propio bordón, este contrapunto estabiliza; con otro instrumento, le contesta en el registro opuesto.
- **Ataques:** solo cuentan los de OÍDO. Cada golpe despierta a la neurona más dormida, y si esa neurona cruza el umbral, canta. Es un diálogo de llamada y respuesta.
- **SENTIDOS** escala los tres de 0 a 200 %. Hay salidas de ATAQUES (disparo) y BRILLO (0–10 V).

### Plasticidad: la red aprende

**PLASTICIDAD** activa una regla de Hebb de covarianza con olvido. Las neuronas que se mueven juntas refuerzan su conexión y las que se oponen la vuelven inhibitoria:

    dD_ij/dt = η · ( k · (o_i − ō_i)(o_j − ō_j) − D_ij / 60 s )

- La red efectiva es **W + D**. Los sesgos se corrigen para seguir siendo *center-crossing*, el régimen más vivo de una CTRNN.
- D se acota a ±8. En 100 %, la red cambia en alrededor de un minuto. En 0, no aprende, pero conserva lo aprendido, así que puedes dejar que aprenda y luego fijar el hábito.
- El **hábito** se guarda con el patch. **OLVIDAR** (botón y disparo) lo borra. FÁBRICA no lo borra: devuelve el temperamento, no la memoria.
- La segunda pantalla dibuja D como una matriz 8×8 (oro refuerza, turquesa inhibe) junto a las barras de la lectura y la última nota cantada. **HÁBITO** sale como voltaje.

Algunos temperamentos se transforman al aprender. Tras 5 minutos de plasticidad máxima, LIBÉLULA a veces se aquieta en un ciclo más simple. Si pasa, OLVIDAR la devuelve.

## El panel (34 HP)

- **Fila 1 · el cerebro:** TEMPERAMENTO, METABOLISMO (×0,125 a ×8: escala todo el tiempo de la red), INTENSIDAD (cuánto mueve el cerebro al sonido), RUIDO sináptico, TÓNICA del bordón, NIVEL.
- **Fila 2 · el sonido:** ACORDE del bordón (quinta, cuarta, séptima natural 4:5:6:7, armónicos, racimo de cuartos de tono), FILTRO (LP, BP o ambos), TIMBRE de la melodía (puro, FM2, campana, sub, metal), ESCUCHA, BORDÓN y CONGELAR.
- **Gestos (botón + disparo):** SACUDIR, MUTAR, RED NUEVA, FÁBRICA.
- **Entradas:** V/OCT de la tónica, CV de temperamento, metabolismo, intensidad y ruido, ESTÍMULO (polifónico, canal i → neurona i), melodía (V/OCT y compuerta), CONGELAR y OÍDO (audio externo).
- **Salidas:** NEURONAS (8 canales, 0–10 V), DISPAROS (8 compuertas), PULSO, OÍDO (lo que escucha el cerebro), IZQ y DER.
- **Sección derecha (2.3):** perillas UMBRAL, MEZCLA, SENTIDOS y PLASTICIDAD; NUEVA LECTURA y OLVIDAR (botón y disparo); CANTA; CV de lectura, sentidos, plasticidad y umbral; salidas CANTO V/O, VEL, ÚLTIMA, CANALES, ATAQUES, BRILLO y HÁBITO.
- **MEL GATE** ahora es polifónico: un acorde desde Ecotono o desde un MIDI-CV da un pulso por nota.

## Ideas de patch

1. **El cerebro de otro.** NEURONAS a un MERGE/SPLIT y cada canal a los parámetros de otro módulo: LYAPUNOV con la ENERGÍA y el TIEMPO que decide GANGLIO.
2. **Oír a un sintetizador.** La salida de tu Hydrasynth a OÍDO: cada frase cambia el ritmo de la red, y la red responde con el bordón.
3. **Ritmo de insecto.** DISPAROS a un módulo de percusión, canal por canal. GRILLO da un pulso regular; ENJAMBRE, uno que nunca se repite.
4. **Tocar un cerebro.** Un teclado MIDI a MEL V/O y MEL GATE con el timbre CAMPANA: cada nota empuja a una neurona y la red responde.
5. **Grillo que canta a otro.** CANTO V/O y DISPAROS a Plaits o Rings (polifónicos), con CANTA apagado: la red compone y otro módulo suena.
6. **Conversación.** Un loop de percusión a OÍDO con SENTIDOS al 150 %: cada golpe despierta a una neurona dormida y la red contesta con una nota.
7. **Costumbre.** PLASTICIDAD al 100 % durante unos minutos, luego en 0: guarda el patch y la red recordará el hábito que aprendió esa noche.
8. **Al modular, por el CM1A.** ÚLTIMA y PULSO al módulo CV-MIDI de VCV, y de ahí al Behringer CM1A, que vuelve las notas voltaje para el rack físico. El MIDI llega a semitonos: los microtonos se pierden en este camino, pero no en el Cardputer, que usará pitch bend.

# RYDBERG

El átomo de hidrógeno como instrumento espectral. Es el motor del firmware RYDBERG de la suite ESPECTROS del Cardputer (versión sin saturación), llevado a VCV Rack con estéreo.

Cada átomo se excita y cae en cascada por sus niveles, E_n = −R/n². Cada salto n → n' emite un fotón de frecuencia proporcional a 1/n'² − 1/n², y ese fotón es una nota. La afinación es el espectro del hidrógeno: la razón entre Lyman-alfa y H-alfa es 5,4, como en la naturaleza.

## Qué lo distingue

- **Física real.** Los saltos respetan la regla de selección Δl = ±1 y se eligen al azar con peso ν³. El nivel 2s es metaestable y decae con dos fotones, que se oyen como un continuo de ruido.
- **Tres modos.** CASCADA: cada fotón suena. ESPECTRO: las líneas más emitidas quedan sonando como bordones. ZEEMAN: el campo magnético abre cada línea en un triplete (σ−, π, σ+).
- **Estructura fina.** Con FINA, cada línea es un doblete muy cercano que bate.
- **Estéreo físico.** Cada átomo tiene su lugar en el campo estéreo, y el triplete Zeeman se abre de izquierda a derecha.
- **Se ve.** La nube |ψnlm|² del primer átomo cambia con cada salto; la escalera de niveles muestra los fotones con su color real (magenta el ultravioleta, ámbar el infrarrojo); y el espectro acumulado crece con la banda visible debajo.
- **El átomo como voltaje.** Cada fotón sale como disparo y como V/OCT; también su serie (1 V Lyman, 2 V Balmer…), su energía, el nivel de cada átomo, el continuo de dos fotones y hasta 8 líneas del espectro como acorde polifónico.

## El panel (24 HP)

- **Fila 1 · el átomo:** n MÁX (2 a 12: hasta dónde se excitan), TEMPERATURA (excitaciones por segundo), CAMPO B (Zeeman), H-ALFA (55 a 1760 Hz: transpone todo el espectro), ÁTOMOS (1 a 4), ESPACIO (reverberación).
- **Fila 2:** MODO (cascada, espectro, Zeeman), FINA (dobletes), NIVEL y las luces de FOTÓN y 2 FOTONES.
- **Gestos (botón + disparo):** EXCITAR (los átomos en el fundamental absorben ya) y OLVIDAR (borra el espectro acumulado: los bordones se apagan).
- **Entradas:** V/OCT de H-alfa y CV de n máx, temperatura, campo, átomos, espacio, modo y nivel (este último como VCA).
- **Salidas:** FOTÓN V/OCT, FOTÓN (disparo), SERIE, ν, NIVELES (un canal por átomo), CONTINUO, LÍNEAS (hasta 8 canales V/OCT), IZQ y DER.

## Ideas de patch

1. **Acordes de hidrógeno.** LÍNEAS a un oscilador polifónico en modo ESPECTRO: el acorde se arma con las líneas que más emite el átomo y cambia a medida que el espectro evoluciona.
2. **Cada fotón, otro instrumento.** FOTÓN V/OCT y FOTÓN a una voz percutida (Plaits, Rings): la melodía sale de las cascadas. Con SERIE a un secuenciador conmutado, Lyman, Balmer y Paschen pueden ir a voces distintas.
3. **Un reloj que excita.** Un reloj a EXCITAR convierte las cascadas en frases con pulso; con TEMPERATURA muy baja, el átomo solo cae cuando el reloj lo empuja.
4. **Campo que respira.** Un LFO lento a CAMPO en modo ZEEMAN abre y cierra los tripletes en el estéreo.

## Notas técnicas

- La física es la de la suite ESPECTROS: péndulo doble m = L = 1, g = w0², integración RK4 a ~8 kHz, con subpasos automáticos a frecuencias altas.
- Todo el estado es de cada instancia, así que varios LYAPUNOV pueden convivir en un patch.
- Los relojes internos van en doble precisión, para no detenerse en sesiones largas.
- **Núcleo y adaptadores.** Cada instrumento tiene su sonido en un núcleo C++ sin dependencias de plataforma, y un adaptador delgado por plataforma. Las reglas están en `nucleos/CONTRATO.md`.
  - `nucleos/lyapunov/Nucleo.hpp`, `nucleos/ganglio/Nucleo.hpp` y `nucleos/rydberg/Nucleo.hpp`: los instrumentos;
  - `nucleos/comun/`: la etapa de salida, el catálogo de afinaciones y el lector de archivos Scala;
  - `src/`: los adaptadores de VCV Rack (módulos, paneles y pantallas).
- El mismo núcleo sirve para MetaModule, Daisy y el Cardputer: `tools/portabilidad.sh` comprueba en cada cambio que compile sin excepciones, para Cortex-M7, y dentro de 32 KB.
- Pruebas de los núcleos en `test/`. Ninguna configuración satura ni produce valores inválidos.
- Licencia: GPL-3.0-or-later. Es la que exige la biblioteca de VCV para plugins de código abierto. Puede cambiarse si se prefiere otra distribución.
