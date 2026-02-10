# my_kilo
DIY text editor, made entirely in a single C file from-scratch.
I am following along with this tutorial: https://viewsourcecode.org/snaptoken/kilo/

My intention is to learn about raw terminal input, so that I may implement a cleaner Terminal User Interface into my pre-existing meal tracker program while only relying on libc. (not using ncurses, termbox, etc.)

What i've learned:
- ASCII control codes, e.g the binary representation of the number 10 being the 'Enter' key.
- 'Enabling raw input',  Disabling many flags to effectively remove the 'input' line: All keypresses are immediately sent into STDIN.
- Remapping CTRL keys, e.g setting CTRL+q to quit instead of the terminal emulator CTRL+c defualt.
