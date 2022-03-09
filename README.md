# DOOM-ASCII

![LOGO](screenshots/logo.png)

**Text-based DOOM in your terminal!**

Source-port of [doomgeneric](https://github.com/ozkl/doomgeneric). Does not have sound.

You will need a WAD file (game data). If you don't own the game, the shareware version is freely available (e.g. [here](http://www.doomworld.com/3ddownloads/ports/shareware_doom_iwad.zip)).

## Build

Binaries for Windows and Linux are provided as github releases.

### Linux
Creates ```doom_ascii/doom_ascii```
```
cd src
make
```

### Windows
Compile on linux. Creates ```doom_ascii/doom_ascii.exe```
```
cd src
make windows
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

Keybinds can be remapped in ```.default.cfg```

## Performance

### Display

Most terminals aren't designed for massive throughput, so the game cannot be played at full 320x200 resolution and the display should be scaled to minimize the strobing effect.

Pass the command-line argument ```-scaling n``` to determine the level of scaling. Smaller numbers denote a larger display.

Below you can find a table of recommended scales, based on subjective observations.

|Terminal      |No Strobing|Some Strobing|
|--------------|-----------|-------------|
|Alacritty     |3          |2            |
|Yakuake       |4          |2            |
|Konsole       |4          |3            |
|Xfce4-terminal|5          |3            |
|Windows CMD   |-          |4 (severe)   |

By default, a scale of 4 is used.

### Input
The following terminal input options are not mandatory but may improve input handling.
- Disable character repeat delay to prevent delayed repeated inputs.
- Increase polling rate to ensure input on every frame.
