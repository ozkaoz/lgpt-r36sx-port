# Protocolo de prueba U2.33

## Verificación visual

1. Arrancar LGPT.
2. Cargar proyecto.
3. Entrar a Chopper.
4. Debe verse `Graphical Chopper U2.33`.
5. Entrar a `L1+R1`.
6. Debe verse `PITCH/ENV U2.33`.

## Regresión Pitch/Envelope

1. En Pitch/Env, cambiar Pitch a `+2`.
2. Pulsar `B`: debe sonar preview.
3. Pulsar `L2+B`: debe detener.
4. Pulsar `A`: debe aplicar.
5. El overlay `OK / Press A to continue` debe seguir centrado.
6. Salir con `L1+R1`.

## Instrument / Listen correcto

1. Volver a Instrument.
2. Pulsar `B` sobre el instrumento/sample: no debe sonar nada.
3. Abrir `Listen / Import` desde el campo `sample`.
4. Seleccionar `Listen`.
5. Pulsar `A`.

Resultado esperado: debe sonar el sample y no debe aparecer mensaje `Listen preview`.

6. Pulsar `L2+B`.

Resultado esperado: la preescucha debe detenerse.

7. Seleccionar `Import`.
8. Pulsar `A`.

Resultado esperado: la importación debe seguir funcionando.
