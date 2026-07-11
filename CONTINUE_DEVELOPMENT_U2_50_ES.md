# Continuar desarrollo desde U2.50 FINAL

Punto de partida recomendado: `LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE`.

U2.50 debe tratarse como checkpoint estable. Cualquier cambio posterior debe hacerse en una rama nueva y como versión TEST antes de marcarse como estable.

## Rama sugerida para el siguiente bloque

```bash
git checkout -b u2.51-next-dev
```

## Áreas estabilizadas hasta U2.50

No tocar salvo necesidad clara:

- Instalador TreeFrogUI para R36SX/R36S.
- Entrada única `roms/lgpt/start.lgpt`.
- Exportación WAV Song/Multitrack.
- Chopper estable con edición de chops.
- Phrase workflow U2.46.
- Mixer layout U2.50.

## Áreas candidatas para U2.51

1. Refinar commands/FX sin romper compatibilidad con proyectos existentes.
2. Mejorar documentación de commands visibles vs commands legacy.
3. Evaluar si delay/reverb deben ser comandos por step, parámetros de instrumento o canal FX.
4. Añadir pruebas específicas de exportación WAV con paneo y FX.
5. Crear un banco de proyectos de regresión pequeños para probar Phrase, Song, Mixer, Export y Chopper.

## Regla de desarrollo

Para cada versión TEST:

1. Crear rama nueva.
2. Aplicar cambios mínimos.
3. Compilar en WSL.
4. Probar en SD.
5. Solo si el usuario aprueba, generar versión FINAL.

## Build limpio

```bash
cd ~
rm -rf lgpt_u251_test_build
mkdir -p lgpt_u251_test_build
cd lgpt_u251_test_build
unzip -q "/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE.zip"
cd LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE
bash VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh
bash BUILD_U2_36_STABLE_WSL.sh
```

## Instalación de prueba

```bash
cp -f dist/lgpt_libretro.so "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_51_TEST.so"
bash INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh F "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_51_TEST.so"
```

Cambiar `F` si la SD tiene otra letra.
