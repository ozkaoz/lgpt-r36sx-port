# U2.48 TEST - Mixer dynamic meters, Project master meter and command cleanup

Base: U2.47 TEST, itself based on U2.46 FINAL.

## Cambios de Mixer

- Las barras verticales de los canales del Mixer ahora muestran señal dinámica por canal, no solo el volumen configurado.
- El valor `VL` sigue mostrando el volumen configurado del canal.
- El valor `PN` sigue mostrando paneo: `C`, `Lxx` o `Rxx`.
- Se añadió edición de tempo directamente desde Mixer:
  - `Y + UP/DOWN` cambia tempo en pasos de 1.
  - `Y + LEFT/RIGHT` cambia tempo en pasos de 10.
- El layout del Mixer fue reducido para que quepan canales, volumen, paneo, master meter y ayuda sin taparse.

## Cambios de Master meter

- El meter Master L/R del Mixer ahora es horizontal compacto y se actualiza durante reproducción.
- El meter Master L/R del Project/Master ahora está en la esquina inferior izquierda como una línea horizontal compacta:
  - `L //////-- R ////----`
- El objetivo es visualizar la pulsación real de la onda sin tapar `Exit` ni campos del proyecto.

## Cambios de Commands

- `FBYP` se retiró del selector visible de commands.
- El FourCC `FBYP` permanece definido y soportado internamente para compatibilidad con proyectos antiguos.
- `LPF`, `HPF`, `ECHO` y `RVRB` permanecen como comandos visibles de prueba.

## Cambios de Phrase UI

- La grilla de Phrase se movió 3 columnas hacia la izquierda para centrar mejor el layout ampliado:
  - `Note | Vol | Inst | Cmd1 | Param1 | Cmd2 | Param2`
- También se ajustó la posición del editor de parámetros para que coincida con la nueva grilla.

## Pendiente de probar en SD

1. Mixer: reproducir un proyecto con varios canales y confirmar que las barras se mueven con la señal.
2. Mixer: verificar `VL`, `PN`, mute/solo y paneo.
3. Mixer: probar `Y+UP/DOWN` y `Y+LEFT/RIGHT` para tempo.
4. Project: verificar que el master meter queda abajo a la izquierda y no tapa `Exit`.
5. Phrase: confirmar que la grilla vuelve a quedar centrada.
6. Commands: confirmar que `FBYP` ya no aparece en el selector.

## Nota técnica

Los meters por canal se calculan en `PlayerChannel::Render()` después de aplicar volumen y paneo del Mixer. El master meter sigue usando los picos L/R del `AudioMixer` principal.
