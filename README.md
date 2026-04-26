# cAvatar
Identicon styled batch avatar generator in C lang


cAvatar - a simple avatar generator in C inspired by Identicon.
 *

This program reads a text seed from `seed.txt` and generates unique 
identicon-style avatars based on that seed and a user-provided name.
 

Usage:
  `./a.out`
  ^ - interactive cli mode 
  `./a.out Name`
  ^ - generates Name.ppm
  `./a.out Name type(int>3 && int< <=1024) size(int>0)`
  ^ - generates Name.ppm with type*type cells and type*size pixels large image 

CLI mode:
  when ran as just `./a.out`, it will ask for the following things:
      - avatar count or filename (text file with names separated by newlines
      - grid size - how many unique pixels per side (originally the Identicon is 5)
      - size multiplier - how many pixels per cell in the output image
 

The generated images are in PPM format, which can be easily converted to PNG/JPEG without loss of quality.
 
