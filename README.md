# EFRT

Compresor FET de carácter clásico, hecho en JUCE/C++, pensado para producir y
mezclar rock / post-hardcore (baterías, guitarras, voces, buses). Está
**inspirado** en los limitadores FET de estudio: umbral interno fijo (el Input
empuja la señal contra él), ataque ultrarrápido, release program-dependent,
saturación FET y un modo "All Buttons In". Es una implementación propia, no una
copia de código de nadie.

> El comportamiento del compresor está
> *medido* por tests (ver "Verificación" más abajo): los ratios son los que dice
> cada botón, los tiempos son los que dice cada knob, y no depende del sample
> rate. Lo que **no** es: un modelo de circuito calibrado contra un aparato
> real. "All Buttons In" y el saturador son interpretaciones de diseño propias.
> Para decir "suena igual a X" hace falta medir contra hardware y hacer ABX
> ciegos; eso todavía no está hecho.

## Controles

**Knobs de madera**

- **Input** (−20 a +30 dB): cuánto empujás la señal contra el compresor. Más
  Input = más compresión (no hay knob de threshold, como en el hardware).
- **Attack** (20 µs – 800 µs): constante de tiempo real del ataque. Rápido muerde
  el transitorio; lento lo deja pasar.
- **Release** (50 ms – 1.1 s): *program dependent*: cuanta más reducción hay, más
  rápido se recupera (la constante rápida es el 15% de la lenta).
- **Output** (−30 a +20 dB): ganancia de salida.

**Botones de ratio** (mármol): 4:1, 8:1, 12:1, 20:1 y **All Buttons In**.

- 4/8/12/20 comprimen *exactamente* eso por encima de la rodilla (medido: 4.0 /
  8.0 / 12.0 / 19.9 : 1). La rodilla es más suave en los ratios bajos.
- **All Buttons In** es el modo agresivo de diseño propio: ratio 30:1, umbral 4 dB
  más bajo, rodilla casi dura, ataque y release más rápidos y más saturación.

**Arriba**

- **Medidor**: barra de reducción de ganancia (con marca de pico) + picos de
  entrada/salida en dBFS.
- **PRESET**: presets de fábrica. Son *puntos de partida* (Drum Bus, Vocal,
  Guitars Parallel, Mix Bus Glue, Fast & Nasty), no el sonido de ningún disco.
- **MIX** (arrastrar el texto arriba/abajo): compresión paralela interna.

**Tira inferior y esquina superior derecha** (arrastrar arriba/abajo = valor,
Shift = fino, doble click = default; los que alternan, un click). SC, HPF, LINK,
L/R·M/S y COLOR están abajo; **OS y AUTO** están arriba a la derecha, debajo de
MIX (la esquina inferior derecha queda libre porque ahí JUCE dibuja su cartel
"Made with JUCE" con la licencia gratuita/GPL):

| Control | Qué hace |
|---|---|
| **SC INT/EXT** | Detección interna o por el bus de sidechain del host. |
| **HPF** | Filtro pasa-altos de 12 dB/oct en el sidechain (Off = 20 Hz). Evita que los graves "muevan" la ganancia. |
| **LINK** | Link estéreo 0–100%. 100% = misma reducción en L y R (la imagen no se corre). |
| **L/R · M/S** | Procesa en izquierda/derecha o en mid/side. |
| **COLOR** | Saturación FET/transformador (asimétrica: 2º y 3er armónico). 0% = bypass exacto. No cambia el nivel en señal chica. |
| **OS Off/2x/4x** | Oversampling del saturador. Baja el aliasing y agrega latencia (se informa al host). |
| **AUTO** | Auto gain: iguala la sonoridad de salida con la de entrada (ventana lenta, se congela con silencio). |

**Formatos de pista**: mono y estéreo (con o sin sidechain). La ventana se puede
redimensionar (arrastrar la esquina; el tamaño se guarda con el proyecto).

## Cambios importantes respecto a la versión anterior

- **Los ratios ahora son reales.** Antes, la fórmula feed-forward dentro de un
  lazo feedback daba ratios efectivos de 1.6 / 1.9 / 1.94 / 1.92 : 1 para los
  cuatro botones. Los proyectos viejos van a sonar **más comprimidos** en 8, 12
  y 20 (los estados guardados cargan bien; los parámetros nuevos quedan en su
  default).
- La detección pasó a feed-forward (sobre la entrada). El ratio nominal es el
  efectivo, y attack/release son los del knob. Se perdió la dinámica "propia"
  del lazo feedback; a cambio es estable a cualquier ataque y predecible.
- El saturador ya no está siempre activo ni suma +4.8 dB: ahora es ganancia
  unitaria y se controla con COLOR (default 25%).
- Un NaN/Inf en la entrada ya no deja el compresor muerto para siempre.
- Input, Output, Mix y Color tienen smoothing (sin "zipper" al automatizar).
- **El texto "MIX: xx" por fin se ve.** En la versión anterior el control
  respondía al arrastre pero era invisible: JUCE le daba al slider vertical de
  16 px de alto un rectángulo de dibujo de altura 0 y el texto se dibujaba ahí.
- El Mix con oversampling queda alineado en fase con el wet (el dry pasa por un
  oversampler idéntico), así que la compresión paralela no hace peine en agudos.

## Requisitos (Windows)

1. **Visual Studio 2022** (Community alcanza) con el workload
   "Desktop development with C++".
2. **CMake** ≥ 3.22 → https://cmake.org/download/ (marcar "Add to PATH" al instalar).
3. **Git** → https://git-scm.com/download/win
4. Conexión a internet la primera vez (CMake descarga JUCE desde GitHub, tag 7.0.12).

## Compilar desde VSCode

1. Instalá "Build Tools for Visual Studio 2022" (solo el workload "Desktop
   development with C++", no el IDE completo), CMake y Git.
2. En VSCode, instalá las extensiones **C/C++** y **CMake Tools** (Microsoft).
3. File > Open Folder → la carpeta `ElFratacho` (la que tiene `CMakeLists.txt`).
4. Elegí el kit "Visual Studio Build Tools 2022 Release - amd64" (si no aparece:
   `Ctrl+Shift+P` → "CMake: Scan for Kits").
5. `Ctrl+Shift+P` → "CMake: Configure" (la primera vez descarga JUCE; modo
   **Release**, no Debug) y después "CMake: Build".

> Si venís de la versión anterior, corré **"CMake: Configure"** de nuevo antes de
> compilar: hay opciones y targets nuevos (tests).

## Compilar desde línea de comandos

```powershell
cd ruta\donde\descomprimiste\ElFratacho
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Dónde queda el plugin

En Windows el VST3 (y el AU) se copian solos a la carpeta de plugins del
sistema (`C:\Program Files\Common Files\VST3\EFRT.vst3`). También se
genera un **Standalone** en `build\ElFratacho_artefacts\Release\Standalone\` para
probar sin DAW. Después tu DAW tiene que reescanear plugins VST3.

## Verificación

```bash
cd build
ctest -C Release --output-on-failure
```

- **`EfrtDspTests`** (50 chequeos, sin JUCE): ratio efectivo de cada botón,
  transparencia bajo el umbral, saturador (ganancia unitaria, armónicos, DC,
  bypass exacto), NaN/Inf, tiempos de attack/release en 44.1/48/96/192 kHz,
  link estéreo, rizado en graves, HPF, auto gain, reset.
- **`EfrtProcessorTests`** (40 chequeos): layouts mono/estéreo/sidechain,
  latencia y alineación wet/dry con oversampling, aliasing medido con FFT,
  robustez (NaN, bloques de 1 muestra, bloques más grandes que el preparado),
  sidechain externo, link y M/S, auto gain, ausencia de zipper, estado viejo y
  nuevo, presets, CPU.

Algunos números medidos (48 kHz, saturador al 100%, Input +12 dB, seno de 9 kHz):

| | Antes | Ahora |
|---|---|---|
| Ratio efectivo 4/8/12/20 | 1.6 / 1.9 / 1.94 / 1.92 : 1 | 4.0 / 8.0 / 12.0 / 19.9 : 1 |
| Alias (pico no armónico) | −34.6 dBc, sin forma de reducirlo | −36.6 dBc con OS Off · **−117.6 dBc con 2x** · −122.2 dBc con 4x |
| Rizado de GR a 40 Hz (release 50 ms) | 1.84 dB p-p | 1.06 dB p-p (0.00 con HPF) |
| Compresión tras un NaN | muerta (0 dB) | 19.2 dB |
| CPU, estéreo con 4x | — | ~2% de un núcleo |

El VST3 compilado pasa [pluginval](https://github.com/Tracktion/pluginval) en
strictness 10 (Linux): apertura en frío/tibio, programas, audio, estado y
restauración, automatización, thread-safety de parámetros, `vst3 validator`,
buses (incluido el sidechain) y fuzz de parámetros, sin fallas ni advertencias.
Para correrlo vos:

```bash
pluginval --strictness-level 10 --validate-in-process path/a/EFRT.vst3
```

Para revisar la UI sin abrir un DAW: `cmake -DEFRT_BUILD_SNAPSHOT=ON ...` y
`EfrtEditorSnapshot salida.png 960 1` (ancho y preset opcionales).

## Punto de partida para un sonido tipo "To Be Everywhere Is To Be Nowhere"

Ese disco tiene batería muy pegada/comprimida y guitarras con mucho ataque
controlado pero con punch. Como punto de partida (después ajustá al oído); los
presets de fábrica siguen estas ideas:

- **Batería (bus completo)**: Ratio 4:1 o 8:1, Attack rápido (~0.05–0.1 ms),
  Release medio (~150–300 ms), Input alto para marcar 5–10 dB de GR, Mix 60–80%.
  HPF del sidechain en ~60 Hz para que el bombo no mueva todo.
- **Voz**: Ratio 4:1, Attack ~0.2 ms, Release ~200 ms, GR de 4–6 dB, Mix 100%.
- **Bus de guitarras / paralelo agresivo**: 20:1 o All Buttons In, Attack muy
  rápido, Release rápido, Mix bajo (20–40%) contra la señal seca. Con AUTO
  prendido comparás a igual nivel.

## Limitaciones conocidas

- El fondo de la UI es un bitmap de 480×320: al agrandar la ventana se escala
  con filtro de alta calidad pero se va a ver blando. Para pantallas HiDPI hace
  falta arte @2x (o vectorial).
- No hay AAX (Pro Tools) ni CLAP todavía.
- Verificado compilando y corriendo los tests en Linux.
- No hay modo bypass propio con crossfade (se usa el bypass del host).
- El detector mide picos de las muestras (no inter-sample peaks).

## Instalador de Windows (.exe)

`installer/EFRT.iss` es el script de [Inno Setup 6](https://jrsoftware.org/isinfo.php).
Muestra el EULA (hay que aceptarlo), instala el VST3 en
`C:\Program Files\Common Files\VST3\` y, opcionalmente, el Standalone.
