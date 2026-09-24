![Elora](https://i.imgur.com/AUCjyBuh.jpg)

# Elora

Before building, make sure the repo is set up correctly:

Initialize submodules:

```bash
make git-submodule
```

Set up QMK:

```bash
qmk config user.qmk_home=$PWD
qmk config user.overlay_dir=None
```

Build the firmware:

```bash
qmk compile -c -kb splitkb/elora/rev1 -km vial
```

Select the animation for the secondary screen explicitly using `-e SECONDARY_SCREEN_ANIMATION=<animation>`.
Supported values are `matrix` (default), `wave`, and `none`.

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

You can enter the bootloader in 3 ways:

* **Reset button**: Double-tap the reset button on the side of the PCB.
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available.
* **Bootloader reset**: As a last resort, hold down the small "Boot" button near the USB connector while plugging in the keyboard.
