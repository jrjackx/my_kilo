# my_kilo
DIY text editor, made entirely in a single C file from-scratch.
I am following along with this tutorial: https://viewsourcecode.org/snaptoken/kilo/
The project is still very much WIP, doesn't do much currently.

My intention is to learn about raw terminal input, so that I may implement a cleaner Terminal User Interface into my pre-existing meal tracker program while only relying on libc. (not using ncurses, termbox, etc.).
The code is **extremely heavily commented**, which i wish to stress is _not my usual style_. I am doing it specifically to be able to read back the reasons why things really are in the tutorial, instead of just blindly copying. My own work, as you can see in the meal tracker program, is not structured like this.

What i've learned:
- ASCII control codes, e.g the binary representation of the number 10 being the 'Enter' key.
- 'Enabling raw input',  Disabling many flags to effectively remove the 'input' line: All keypresses are immediately sent into STDIN.
- Remapping CTRL keys, e.g setting CTRL+q to quit instead of the terminal emulator CTRL+c defualt.
- Character buffers, reading special codes spat out from the terminal back into the program.
- the nature of the terminal emulator being a separate program from the actual text editor program I am writing, which merely *interfaces* with the terminal emulator rather than being an inseparable part of it.
- assembling an append buffer instead of doing a bunch of little write calls, writing the whole thing on a draw pass.
- processing escape sequences, like what the arrow keys send to stdin.
- inline arguments when starting the program
- loading in external file information into append buffer

Build instsructions:

  IF USING WINDOWS:
    - use Windows Subsystem for Linux to run the program: https://learn.microsoft.com/en-us/windows/wsl/install
  
  - clone repo
  - open terminal in repo directory
  - run the command 'make'
  - program is run with the command ./kilo
