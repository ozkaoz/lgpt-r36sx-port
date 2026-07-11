# U2.45 TEST - Phrase navigation and double-A workflow fixes

Estado: TEST. No marcar como estable hasta validarlo en SD.

## Cambios incluidos

### Phrase navigation

- `L2+UP`: navega al phrase usado anterior.
- `L2+DOWN`: navega al siguiente phrase usado. Si no existe un phrase posterior, crea el siguiente phrase libre automáticamente.
- Al crear un phrase con `L2+DOWN`, el port intenta enlazarlo al siguiente slot vacío del chain actual. Si no hay slot libre, igualmente crea y abre el phrase.
- `L2+LEFT`: navega a la asignación anterior dentro del Song actual, en el canal actual.
- `L2+RIGHT`: navega a la asignación siguiente dentro del Song actual, en el canal actual.

La navegación `L2+LEFT/RIGHT` recorre el arreglo real `Song -> Chain -> Phrase`, de modo que se puede saltar de un phrase asignado a otro sin volver manualmente a Song y Chain.

### Phrase double-A

- Se elimina la restricción de detener playback para abrir el editor del chop asignado.
- `double A` sobre una fila con chop asignado abre el editor Pitch/Envelope del chop aunque el proyecto esté reproduciéndose.
- El editor Pitch/Envelope abierto desde Phrase mantiene `R1+B` como regreso directo a Phrase.

### Se mantiene desde U2.44

- `Y`: preescucha de la fila actual en Phrase.
- `L1+X`: inicia selección.
- `X`: corta selección activa.
- `R1+A`: solo / unmute all si ya hay solo activo.
- `R1+B`: mute.

## No incluido en este test

La revisión de commands (`ARPG`, `FCUT`, `KILL`, `PTCH`, filtros, reverb, delay) y el medidor stereo del Master quedan fuera de U2.45 TEST. Eso debe entrar en una rama posterior porque toca playback, mezcla, exportación y DSP.

## Compilación

```bash
cd ~

rm -rf lgpt_u245_test_build
mkdir -p lgpt_u245_test_build
cd lgpt_u245_test_build

unzip -q "/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_45_PHRASE_NAV_DOUBLEA_TEST_SOURCE.zip"

cd LGPT_PORT_U2_45_PHRASE_NAV_DOUBLEA_TEST_SOURCE

bash VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh
bash BUILD_U2_36_STABLE_WSL.sh
```

## Instalación en SD

Ajusta `F` si la SD tiene otra letra.

```bash
cp -f dist/lgpt_libretro.so "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_45_TEST.so"

bash INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh F "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_45_TEST.so"
```

## Protocolo de prueba rápido

1. En Phrase, probar `Y` para preview.
2. Probar `L1+X`, luego `X` para cortar selección.
3. Probar `L2+UP/DOWN` para navegar phrases.
4. En el último phrase usado, probar `L2+DOWN`; debe crear un phrase nuevo.
5. En un Song con varias asignaciones, entrar a Phrase y probar `L2+LEFT/RIGHT`; debe saltar entre asignaciones del Song en el canal actual.
6. Reproducir el proyecto y hacer `double A` sobre una fila con chop asignado; debe abrir Pitch/Envelope sin pedir detener playback.
7. Desde Pitch/Envelope abierto por `double A`, probar `R1+B` para volver a Phrase.
