# Programmable Load Common Code
This is a sort of "standard library" for the programmable load software. It contains some common stuff that's useful to basically every software component on the platform. This includes, but not limited to:

- Event loop: A small wrapper around libevent2
    - Watchdog support: If the process is running under systemd watchdog supervision, the primary event loop will automatically periodically kick the watchdog.
- Logging: Implement support for logging, based around the [plog](https://github.com/SergiusTheBest/plog) library.
