# Continuar OTG desde AU11Z6

## Estrategia

Desarrollar OTG como capa externa antes de integrarlo al core LGPT.

## Hipótesis validada parcialmente

- Windows puede detectar `R36SX USB Audio` en ramas AU11Z7/AU11Z10.
- Hubo pruebas donde reproducción y preescucha funcionaron, pero con regresiones del port.
- La integración debe separar UDC/configfs/UAC2/daemon/FIFO del audio callback y del flujo de samples.

## Orden recomendado

1. Mantener AU11Z6 intacta.
2. Crear rama `otg-sidecar-from-au11z6`.
3. Validar configfs/UDC/UAC2 con scripts externos.
4. Validar daemon ALSA/PCM/FIFO fuera del core.
5. Integrar activación explícita al port solo cuando el sidecar sea estable.

## Restricciones iniciales

No tocar al comienzo:

- InstrumentView
- ImportSampleDialog
- SampleManager
- Audio callback de LGPT
- navegación de menús

