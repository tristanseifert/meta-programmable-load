# User Interface Daemon (pinballd)
A small daemon that's responsible for dealing with all hardware user interface devices (that is, the front panel buttons and knobs, beepers, and the auxiliary control functions for the display) and exposing an RPC interface to the GUI process to receive events, and change the state of indicators as well.

In essence, this is an userspace driver for all the I²C peripherals on the front panel board, and also initializes the display controller via SPI.
