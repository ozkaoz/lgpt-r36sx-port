# U2.46 TEST - Phrase navigation mapping correction

Base: U2.45 TEST.

## Cambios

En `Phrase` se ajusta el mapeo de navegación con `L2 + flechas`:

- `L2 + LEFT`: previous used phrase.
- `L2 + RIGHT`: next used phrase.
- `L2 + UP`: previous phrase assignment in current Song channel.
- `L2 + DOWN`: next phrase assignment in current Song channel.
- `L2 + DOWN` crea y enlaza una nueva phrase si no existe una asignación posterior en el canal actual del Song.

Se mantiene lo validado de U2.45:

- `Y`: preview current row.
- `L1 + X`: start selection.
- `X`: cut active selection.
- double `A`: open Pitch/Envelope for assigned chop.
- `R1 + B` inside that Pitch/Envelope view: back to Phrase.

## Build

```bash
cd ~
rm -rf lgpt_u246_test_build
mkdir -p lgpt_u246_test_build
cd lgpt_u246_test_build
unzip -q "/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_46_PHRASE_NAV_SWAP_TEST_SOURCE.zip"
cd LGPT_PORT_U2_46_PHRASE_NAV_SWAP_TEST_SOURCE
bash VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh
bash BUILD_U2_36_STABLE_WSL.sh
```

## Install

```bash
cp -f dist/lgpt_libretro.so "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_46_TEST.so"
bash INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh F "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_46_TEST.so"
```

Change `F` if your SD card uses another drive letter.
