# DOOM-ASCII

![LOGO](screenshots/logo.png)

**Text-based DOOM in your terminal!**

Source-port of [doomgeneric](https://github.com/ozkl/doomgeneric). Does not have sound.

You will need a WAD file (game data). If you don't own the game, the shareware version is freely available.

**Try it out over telnet!**
```
telnet doom.w-graj.net 666
```

## Build
Binaries for Windows and Linux are provided as github releases.

### Linux / Mac
Requires Make and a C compiler. Creates ```doom_ascii/doom_ascii```
```
cd src
make
```

### Windows
Compile on linux. Creates ```doom_ascii/doom_ascii.exe```
```
cd src
make windows-cross
```

## Controls
Default keybindings are listed below.

|Action         |Default Keybind|
|---------------|---------------|
|UP             |ARROW UP		|
|DOWN			|ARROW DOWN		|
|LEFT			|ARROW LEFT		|
|RIGHT			|ARROW RIGHT	|
|STRAFE LEFT	|,				|
|STRAFE RIGHT	|.				|
|FIRE			|SPACE			|
|USE			|E				|
|SPEED			|]				|
|WEAPON SELECT  |1-7            |

Keybinds can be remapped in ```.default.cfg```, which should be placed in the same directory as the game executable.

## Performance tips
### Display
Most terminals aren't designed for massive throughput, so the game cannot be played at full 320x200 resolution at high frames per second.

Pass the command-line argument ```-scaling n``` to determine the level of scaling. Smaller numbers denote a larger display.

A scale of 4 is used by default, and should work flawlessly on all terminals. Most terminals (excluding Windows CMD) should manage with scales up to and including 2.

### Input
For a better playing experience, increase the keyboard repeat rate, and reduce the keyboard repeat delay.

## Troubleshooting
### Colours are displayed incorrectly
If the displayed image looks something like [this](https://github.com/wojciech-graj/doom-ascii/issues/8), you are likely using a terminal that does not support 24 bit RGB. See [this](https://gist.github.com/sindresorhus/bed863fb8bedf023b833c88c322e44f9) for more details, troubleshooting information, and a list of supported terminals.

### Running make throws an error
Run `make --version` and `cc --version` to verify that you have Make and a C compiler installed. If you do, and you're still getting an error, file a github issue.
