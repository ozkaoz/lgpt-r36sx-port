# U2.50 TEST — Mixer layout y meters legibles

Base: U2.49 TEST.

## Cambios

- Mixer reorganizado: ayudas de teclas arriba; faders y medidores abajo.
- Barras de canal más altas.
- Barras de canal estabilizadas: ahora representan el volumen configurado del canal para evitar parpadeo excesivo en sonidos rápidos como hi-hats.
- La señal dinámica queda concentrada en ML/MR, que siguen moviéndose con el master stereo real.
- ML/MR en Mixer son barras verticales más grandes.
- Indicador `!` cuando un canal o master sobrepasa 0 dBFS / clip.
- Pan más visible: los canales paneados muestran `Lxx` / `Rxx` y un marcador `<` / `>` en la fila inferior.
- La selección de canal mantiene una variación de color/inversión para distinguir el canal activo.
- Tempo con `Y+flechas` conserva el highlight visual de U2.49.
- Project: barras ML/MR verticales más largas, reubicadas hacia la zona inferior izquierda pero separadas del borde.

## Prueba recomendada

1. Abrir Mixer.
2. Reproducir un patrón con hi-hats rápidos.
3. Verificar que los faders de canal ya no parpadeen de forma confusa.
4. Verificar que ML/MR sí se muevan dinámicamente con la mezcla.
5. Probar paneo con `R1+LEFT/RIGHT` y confirmar que se ve `Lxx/Rxx` más marcador `<` o `>`.
6. Verificar clip: subir volumen/master hasta ver `!`, luego bajar para confirmar que desaparece.
7. Abrir Project y confirmar que ML/MR no interfieren con Export ni Exit.
