# cAvatar
Identicon styled batch avatar generator in C lang



This program reads a text seed from `seed.txt` and generates unique 
identicon-style avatars based on that seed and a user-provided name.
 

Usage:

  `./a.out`
  
  - interactive cli mode 
  
  `./a.out Name`
  
  - generates Name.ppm
  
  `./a.out Name type(int>3 && int< <=1024) size(int>0)`
  
  - generates Name.ppm with type*type cells and type*size pixels large image 

CLI mode:

  when ran as just `./a.out`, it will ask for the following things:
  
  - avatar count or filename (text file with names separated by newlines
  - grid size - how many unique pixels per side (originally the Identicon is 5)
  - size multiplier - how many pixels per cell in the output image
 

The generated images are in PPM format, which can be easily converted to PNG/JPEG without loss of quality.

<p align="center">Below are the generated images using the same seed.txt as in the repo with type: 5 and size: 75.</p>
<table align="center">
  <tr>
    <td><img src="https://github.com/user-attachments/assets/904de98f-e61a-4c0c-b702-9b30c4899b02" width="250" alt="kutya"></td>
    <td><img src="https://github.com/user-attachments/assets/4e7f3857-db72-4436-a4ee-c85e7d47d2e1" width="250" alt="cica"></td>
    <td><img src="https://github.com/user-attachments/assets/3ce90392-b7d9-4035-9667-632a7e672c76" width="250" alt="alma"></td>
  </tr>
  <tr align="center">
    <td><b>kutya</b></td>
    <td><b>cica</b></td>
    <td><b>alma</b></td>
  </tr>
</table>
