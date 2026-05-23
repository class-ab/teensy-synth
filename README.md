# teensy-synth
code and electrical layout for a fully custom synthesiser using a Teensy 4.0 and the inbuilt audio library. please note the following:

i made this as a 15 year old (this repo was made later). it's got some pretty dodgy stuff in it. i'll make another one day, and do it proper, but till then, it's gonna stay pretty dodgy.

almost all of the code was made using Claude. i had to fix everything it messed up, and although i know how to do it myself, full disclaimer that it did damn near everything.

the 'project report' file was the full project report i did on this. it's 20 something thousand words, and i removed some images. a fun read i'm sure, but is more about the build and not the actual synth.

## purpose
created as a fun project by yours truly. designed to copy various famous synths but also to build upon random features i though vaugely applicable, everything here is a bit of a mishmash of various ideas. but, most importantly, the teensy and complementary audio library allow for extreme modularity and customisation of literally everything. a really cool future project would be to turn this into a fully operational professional synth that allows for the plug in-n-out nature of modular rack synths, e.g. eurorack. could be implemented through digtally controlled patches or adjustable plugs, overall it could be a cool small form factor modular synth capable of running off a battery.

## method
the teensy 4.0 runs the main code (main.ino). it's connected to the multiplexer and an arduino UNO and MEGA (i2c). 

teensy 4.0 reads mainly potentiometer values and recieves data from the arduinos, namely more potentiometer values and the piano key presses.

the arduino uno isn't currently used in the final version of the project here, but is connected to pots only.

the arduino mega is connected to the keyboard (5 octaves salvaged from a broken keyboard) through digital pins connected to the various PORTA,B,... D. this allows for faster reading of the keyboard. the keyboard has 2 buttons per key, so thus could be used to detect velocity or hardness of press, etc, but i can't be bothered adding a switch for it so it's currently just unused. the keyboard connected through a 8x8 matrix, where each key is described by a row and a column of the matrix (e.g. 2x3). the velocity sensing effectively creates a 16x8 matrix (note, 8x8 = 64 possible keys. 5 octave keyboard = 61 keys. see?). the mega is also connected to more pots.

## issues
- the teensy runs as 3.3v and the arduinos 5v. important to notice for both interchip communication and reading the pot values (i learnt the hard way for the later. several hours of resoldering ensued).
- DAC and teensy i2s traces were too long. required a tiny Pµ capacitor to fix.
- teensy did NOT like being a i2c slave while running i2s. it didn't play nice at all, and in the end i made it a i2s master, which is a little weird, but it works i guess? this took a LONG time to figure out, i made a post on the PJRC forum too, no-one found anything, but in the end this fixed it... weird interrupt conflict i think.

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
