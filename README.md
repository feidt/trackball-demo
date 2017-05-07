# trackball-demo
A simple trackball demo in C++ for visualizing 3D voxel data sets with OpenGL

## Data set
the dataset consists of a 3D data cube of simulated ARPES (angle resolved photoemission spectroscopy) data.
The file format is that of a conventional photoemission delay line detector of Surface-Concept, 16Bit-TIFF. The demo
is limited to a 512x512 pixel format.

### Requirements

+ LibTiff
+ OpenGL

install (Ubuntu): sudo apt-get install libtiff5-dev freeglut3-dev

### Installation

compile: open a terminal and run
         `scons`
         
### Run
`./trackball`
