# U2.47 TEST - Mixer pan, master stereo meter y commands/FX

Base: U2.46 FINAL Phrase workflow.

Estado: paquete de prueba. No marcar como estable hasta validar playback, export WAV y proyectos antiguos en SD.

## Mixer / Master

- Se agrega pan por track al modelo de Mixer.
- El pan se guarda/restaura en el bloque `MIXER` de `lgptsav.dat` como atributo `PAN`.
- Compatibilidad: proyectos anteriores cargan con pan centrado porque el atributo no existe y el valor por defecto es `0`.
- En Mixer:
  - `R1 + LEFT` mueve el pan del track seleccionado hacia la izquierda.
  - `R1 + RIGHT` mueve el pan del track seleccionado hacia la derecha.
  - `R1 + DOWN` centra el pan del track seleccionado.
- La vista Mixer muestra una línea `PN` por canal:
  - `C` = centro.
  - `Lxx` = izquierda.
  - `Rxx` = derecha.
- Se agrega medidor stereo L/R del master con barras horizontales usando `/`.
- También se dibuja un medidor stereo master en Project/Master.

## Commands/FX

La lista visible del selector de commands queda depurada para performance y uso común. Los commands legacy siguen existiendo para reproducción de proyectos antiguos, pero no todos aparecen en el selector principal.

Commands visibles principales:

- `ARPG` arpegio.
- `CRSH` drive/crush.
- `ECHO` delay por feedback.
- `FBYP` bypass inmediato de filtro.
- `FCUT` rampa de cutoff.
- `FRES` rampa de resonancia.
- `FLTR` filtro low pass legacy cutoff/resonance.
- `HPF ` filtro high pass explícito.
- `KILL` corta nota.
- `LPF ` filtro low pass explícito.
- `PAN ` paneo por command.
- `PFIN` fine pitch.
- `PTCH` pitch.
- `RTRG` retrigger.
- `RVRB` ambiente/reverb simple por feedback corto.
- `STOP` stop song.
- `TABL` dispara table.
- `TMPO` tempo.

## Formatos nuevos

`LPF :aabb`

- `aa` = cutoff.
- `bb` = resonance.
- Fuerza mezcla low pass.

`HPF :aabb`

- `aa` = cutoff.
- `bb` = resonance.
- Fuerza mezcla high pass aproximada usando el motor de filtro existente.

`FBYP:----`

- Bypassa el filtro del voice actual.
- Cutoff full, resonance cero, sin rampa activa.

`ECHO:aabb`

- `aa` = tiempo/offset de feedback.
- `bb` = wet/mix.
- Wet limitado internamente para evitar runaway feedback.

`RVRB:aabb`

- `aa` = tamaño/offset corto.
- `bb` = wet/mix.
- Reverb simple tipo smear/ambience, no convolución.

## Pendiente para prueba

1. Verificar que proyectos U2.46 carguen con pan centrado.
2. Verificar que el pan se escuche y se guarde al guardar proyecto.
3. Verificar que Song WAV y Multitrack respeten el pan en export stereo.
4. Verificar que stems sigan exportando sin rutas incorrectas.
5. Probar commands `LPF`, `HPF`, `FBYP`, `ECHO`, `RVRB` en Phrase y Table.
6. Confirmar que la lista depurada de commands no oculta algo indispensable para tu flujo.

## Validación local

- `VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh`: OK.
- Revisión de sintaxis local con `g++ -fsyntax-only` sobre archivos modificados: OK.
- Build MIPS real no ejecutado en este entorno. El intento de build se detuvo antes de compilar por dependencia local ausente de Python/Pillow (`PIL`) en el generador de fuente. En tu WSL habitual debería compilar si ya venías compilando U2.46.
