#!/bin/bash
# Verifica que cada núcleo cumpla el contrato (nucleos/CONTRATO.md):
# sin cabeceras de plataforma, compila sin excepciones ni RTTI, compila para Cortex-M7 (Daisy)
# y su estado cabe en el presupuesto de memoria.
# uso: tools/portabilidad.sh            (desde la raíz del repositorio)
# Ph.D. Mauricio Vásquez Arias · Profesor titular – Investigador asociado · Escuela de Artes y Humanidades, Universidad EAFIT
set -e
PRESUPUESTO=32768
FALLA=0
T=$(mktemp -d)

# nombre del directorio : espacio de nombres
NUCLEOS="lyapunov:lyap"
[ -f nucleos/ganglio/Nucleo.hpp ] && NUCLEOS="$NUCLEOS ganglio:ganglio"
[ -f nucleos/rydberg/Nucleo.hpp ] && NUCLEOS="$NUCLEOS rydberg:rydberg"

if grep -rnE '#include *[<"](rack|Arduino|M5|daisy|freertos|esp_)' nucleos/; then
  echo "✗ un núcleo incluye una cabecera de plataforma"; FALLA=1
fi

for par in $NUCLEOS; do
  dir=${par%%:*}; ns=${par##*:}
  cat > "$T/$dir.cpp" <<EOF
#include "$dir/Nucleo.hpp"
#include <stdio.h>
int main() { static $ns::Nucleo n; n.init(48000.f); printf("%u\n", (unsigned)sizeof(n)); return 0; }
EOF
  g++ -std=c++11 -O2 -fno-exceptions -fno-rtti -Wall -Inucleos "$T/$dir.cpp" -o "$T/$dir" || { echo "✗ $dir no compila sin excepciones/RTTI"; FALLA=1; continue; }
  BYTES=$("$T/$dir")
  if [ "$BYTES" -gt "$PRESUPUESTO" ]; then echo "✗ $dir ocupa $BYTES bytes (presupuesto $PRESUPUESTO)"; FALLA=1; else echo "✓ $dir: $BYTES bytes de estado"; fi
  if command -v arm-none-eabi-g++ >/dev/null; then
    arm-none-eabi-g++ -std=c++11 -O2 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -fno-exceptions -fno-rtti \
      -Inucleos -c "$T/$dir.cpp" -o "$T/$dir.o" && echo "✓ $dir compila para Cortex-M7 (Daisy)" || { echo "✗ $dir no compila para Cortex-M7"; FALLA=1; }
  else
    echo "· arm-none-eabi-g++ no está instalado: se omite la prueba de Daisy"
  fi
done
rm -rf "$T"
exit $FALLA
