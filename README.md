# Tanmatsu Typewriter

This is a program designed to provide a nano-like editor for the Tanmatsu. 

# To compile:
- Run in a Linux environment for best compatibility. Either WSL or an actual install should work
- install: git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
- run "make prepare" and then "make build"
# Controls
- F2 opens the menu (Orange Triangle)
- Volume up and down scroll pages
- Arrow keys navigate the file

# Currently it supports:
- Encryption
(XOR Encryption, with many more algorithms in the works)
- Saving and loading of just about any file
(Yes, even .bin file can be opened, just like any text editor)
# In Progress:
Encryption Algorithms:
- AES 256
- RSA
Optimizations:
- Improve text input and handling

## License

The contents of this repository may be considered in the public domain or [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0) licensed at your disposal.

At Nicolai Electronics we love open source so we recommend licensing your work based on this template under terms of the [MIT license](https://opensource.org/license/mit). The MIT license allows others to build upon your work without restrictions while also making sure you retain your attribution.
