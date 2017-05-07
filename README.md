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

### Keyboard Controls
+ F2: momentum map
+ F3: energy momentum map

+ F5: switch between color and bw mode
+ F6: switch between plane and pointcloud mode
+ ESC: exit

+ m: zoom in
+ n: zoom out
+ r = top view
+ t = side view

