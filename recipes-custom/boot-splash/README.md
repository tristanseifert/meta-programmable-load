# Boot-up Splash
This is a simple daemon that renders the boot-up splash screen onto the display framebuffer. It supports basic remote control (to update the boot progress percentage, and change the messages shown on-screen) via a local network socket, both by a utility and an embeddable library.

It is relatively limited, in that only 24bpp/32bpp framebuffers are supported, and all configuration is fixed.

## Dependencies
The following libraries are needed:

- cairo
- freetype2
- pango
- harfbuzz
- libpng
