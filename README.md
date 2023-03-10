# sys-usb-botbase
A Nintendo Switch (CFW) sys-module that allows users to remote control their Switch via sockets or USB via a config file, as well as read and write to a game's memory. This can be used to create bots for games and other fun automation projects.

## Features:
### Remote Control:
- Set controller state
- Simulate button press, hold, and release
- Simulate touch screen drawing

### Memory Reading and Writing:
- Read/write x amount bytes of consecutive memory from RAM based on:
    1. Absolute memory address
    2. Address relative to main nso base
    3. Address relative to heap base

### Screen Capture:
- Capture current screen and return as JPG

## Disclaimer:
This project was created for the purpose of development for bot automation. The creators and maintainers of this project are not liable for any damages caused or bans received. Use at your own risk.

## Installation
1. Download [latest release](https://github.com/Koi-3088/sys-usb-botbase/releases/latest) and extract into your Nintendo Switch SD card.
2. Open the `config.cfg` located in `atmosphere/contents/43000000000B` using your favorite text editor.
3. Change text to `wifi` if you want to connect wirelessly using sockets, or `usb` if you want to connect using a USB cable.
4. Restart your Switch.

## Credits
- Big thank you to [jakibaki](https://github.com/jakibaki/sys-netcheat) for a great sysmodule base to learn and work with, as well as being helpful on the ReSwitched Discord!
- Thanks to RTNX on Discord for bringing to my attention a nasty little bug that would very randomly cause RAM poking to go bad and the switch (sometimes) crashing as a result.
- Thanks to Anubis for stress testing!
- Thanks to FishGuy for the initial USB-Botbase implementation.
