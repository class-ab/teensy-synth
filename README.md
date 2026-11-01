# teensy-synth
code and electrical layout for a fully custom synthesiser using a Teensy 4.0 and the inbuilt audio library. please note the following:

i made this as a 15 year old (this repo was made later). it's got some pretty dodgy stuff in it. i'll make another one day, and do it proper, but till then, it's gonna stay pretty dodgy.

almost all of the code was made using Claude. i had to fix everything it messed up, and although i know how to do it myself, full disclaimer that it did damn near everything.

the 'project report' file was the full project report i did on this. it's 20 something thousand words, and i removed some images. a fun read i'm sure, but is more about the build and not the actual synth.

## purpose
created as a fun project by yours truly. designed to copy various famous synths but also to build upon random features i though vaugely applicable, everything here is a bit of a mishmash of various ideas. but, most importantly, the teensy and complementary audio library allow for extreme modularity and customisation of literally everything. a really cool future project would be to turn this into a fully operational professional synth that allows for the plug in-n-out nature of modular rack synths, e.g. eurorack. could be implemented through digtally controlled patches or adjustable plugs, overall it could be a cool small form factor modular synth capable of running off a battery.

## method
the teensy 4.0 runs the main code (`src/teensy/main.cpp`). it's connected to the multiplexer and an arduino UNO and MEGA (i2c).

teensy 4.0 reads mainly potentiometer values and recieves data from the arduinos, namely more potentiometer values and the piano key presses.

the arduino uno isn't currently used in the final version of the project here, but is connected to pots only.

the arduino mega runs `src/mega/main.cpp` and is connected to the keyboard (5 octaves salvaged from a broken keyboard) through digital pins connected to the various PORTA,B,... D. The active 8x8 row/column matrix uses output pins `{10, 11, 12, 13, 51, 50, 53, 52}`. The previously used output pins `{48, 46, 44, 42, 43, 45, 47, 49}` are deprecated and no longer configured or scanned. The Mega is also connected to more pots.

## PlatformIO
The three firmware targets are built separately from this repository root:

- `pio run -e teensy40`
- `pio run -e mega`
- `pio run -e uno`

The Teensy is the only I2C master. It polls the Mega at address `0x08` and the Uno at `0x09` every 5 ms. Each slave replies with four 3-byte packet slots; unused slots contain `0xFF`. Mega note packets use `{command, note, reserved}`; note-on/off commands are `1`/`0`, and the Teensy uses a fixed voice gain. Mega pot IDs remain `100` through `115`, and Uno pot IDs are `0` through `3`. Both slaves queue their initial pot readings at startup so the Teensy gets an initial value without waiting for knob movement.

Uno values are available in the Teensy mapping section as `UNO_0` through `UNO_3`. To assign one to a synth parameter, change that parameter's existing source string, for example `#define VCF_DECAY "UNO_0"`. Uno pots default to linear scaling and are not inverted.

On the Teensy, `MEGA_12` controls the reverb decay time from 0.1 to 5 seconds and `MEGA_14` controls its wet mix. `MEGA_13` is unused because the selected `AudioEffectReverb` does not expose a damping parameter.

The Mega scans the active output pins against `rowPins`, starts each note as soon as the active contact closes, and sends note-off immediately when it opens. The formerly used output pins are retained in a source comment for wiring reference only and are not configured or scanned.

Connect SDA to SDA, SCL to SCL, and share ground across all three boards. The Teensy 4.0 uses SDA 18/SCL 19; the Mega uses SDA 20/SCL 21; the Uno uses SDA A4/SCL A5. The Teensy is 3.3 V tolerant only: use a bidirectional I2C level shifter between the Teensy and the 5 V Arduino bus, with pull-ups appropriate for each side.

## issues
- the teensy runs as 3.3v and the arduinos 5v. important to notice for both interchip communication and reading the pot values (i learnt the hard way for the later. several hours of resoldering ensued).
- DAC and teensy i2s traces were too long. required a tiny Pµ capacitor to fix.
- teensy did NOT like being a i2c slave while running i2s. it didn't play nice at all, and in the end i made it a i2s master, which is a little weird, but it works i guess? this took a LONG time to figure out, i made a post on the PJRC forum too, no-one found anything, but in the end this fixed it... weird interrupt conflict i think.
- update: i found this document with a bunch of known Teensy issues. low and behold, the I2C and I2S conflict is mentioned... oh well. next time. https://gist.github.com/somebox/d969f8a97e5a4362af5049ed554a9e69 

## research
i'd strongly reccommend going though these if you wanna make this yourself. doubtless some you've already seen, the astute engineer i'm sure you are... c o u g h

https://www.youtube.com/watch?v=msIQIWeMnBE

https://www.youtube.com/watch?v=Exk_K2VwGu0

https://www.youtube.com/watch?v=ttWqUQ5hmU8

https://github.com/albnys/TeensyPoly6

https://www.youtube.com/watch?v=Y7TesKMSE74

https://www.youtube.com/watch?v=o-ShEG3CLqM

https://www.youtube.com/watch?v=KbcNqarBTsI

https://www.youtube.com/watch?v=UJcZxyB5rVc&list=PL4_gPbvyebyHi4VRZEOG9RKOYq5Hre3a1

https://thewessens.net/synthbook/#constructive-synthesis

https://www.youtube.com/watch?v=--L100JvWc4

https://docs.arduino.cc/tutorials/communication/guide-to-shift-in/#shftin13

https://www.vttoth.com/CMS/technical-notes/?view=article&id=68

https://blog.tommy.sh/posts/poly555-synth/

https://blog.tommy.sh/posts/chord-synth/

https://blog.tommy.sh/posts/okay-2-synth/
