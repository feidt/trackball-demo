#include <GL/gl.h>
#include <GL/glut.h>
#include "GLQuaternion4f.h"
#include <iostream>
#include <math.h>
#include <fstream>
#include <tiffio.h>
#include <string.h>

#define GL_PI 3.141592654f



/** ======================================================================
 * 									ENUMS
 *  ======================================================================
 */
enum ResolutionMode
{
	HIGH_RES,
	LOW_RES
};

enum RenderMode
{
	RENDER_ALL,
	RENDER_SINGLE
};

enum ColorMode
{
	COLOR,
	MONO
};

enum DataMode
{
	DATA_XY,
	DATA_EX,
	DATA_EY
};


/** ======================================================================
 * 									 VARIABLES
 *  ======================================================================
 */

// defined operation modi
RenderMode render_mode;
ResolutionMode resolution_mode;
ColorMode color_mode;
DataMode data_mode;


// virtual trackball variables for rotation
GLVector3f* start_vector;
GLVector3f* end_vector;
GLVector3f* rotation_vector;
GLQuaternion4f* current_quaternion;
GLQuaternion4f* previous_quaternion;
GLfloat* rotation_matrix;
GLfloat rotation_angle;


// Global adjustable parameters
GLfloat	viewport_scale = 1;
GLint active_slice;
GLubyte voxel_alpha = 255;
GLint x_linecut_y_position;
GLint y_linecut_x_position;
GLint y_linecut_x_position_high_res;
GLint x_linecut_y_position_high_res;
GLint show_x_linecut;
GLint show_y_linecut;

// color palette (MATLAB jet color)
GLubyte* palette;

// opengl screen setting parameters
GLfloat viewport_height = 512;
GLfloat viewport_width = 512;
GLfloat viewport_fovy = 90;
GLfloat viewport_near = 0.01;
GLfloat viewport_far = 512;
GLfloat viewport_aspect = viewport_width / viewport_height;


// program state variables
GLboolean data_downsampled;
GLboolean data_downscaled;

/**
 * DATA STORAGE AND FILE HANDLING
 */

// data storage
GLushort* data_DLD_raw;		// storage with same resolution as imported TIFF images (will be downsampled from 16Bit(actually 12Bit-> DLD/Camera sampling) to 8Bit)
GLubyte* data_DLD;			//downscaled/downsampled data storage, 128x128 px, 8Bit

GLint data_DLD_width;
GLint data_DLD_height;

const GLchar* path_root = "data/";
GLchar* filename = (GLchar *) "DLD50.tif";
GLchar path[80];
GLchar* filename_root =(GLchar *) "DLD";
GLint no_slices = 100;


/** ======================================================================
 * 									 FUNCTIONS 								|
 *  ======================================================================
 */

/**
 * GLUT FUNCTIONS
 */

GLvoid keyHandler(GLubyte _key, GLint _x, GLint _y);
GLvoid specialKeyHandler(GLint _key, GLint _x, GLint _y);
GLvoid processMouse(GLint button, GLint state, GLint x, GLint y);
GLvoid processMouseActiveMotion(GLint x, GLint y);
// main render function
GLvoid render();


// init functions
GLvoid init();
GLvoid initOpenGL();
GLvoid initTrackball();
GLvoid initAdjustableParameters();
GLvoid initStateVariables();

GLvoid renderXLineCut();
GLvoid renderYLineCut();
GLvoid renderFrame();


GLvoid loadTiff(GLchar* _path, GLint _time_slice);
GLvoid loadDataStack();
GLvoid setPalette();
GLvoid resetRotationMatrix();
GLvoid setSideView();
GLvoid freeMemory();

// data processing
GLvoid downsample();
GLvoid downscale();

// debugging
GLvoid debugMsg(GLchar* _arg_desc, GLfloat _arg_val);
GLvoid debugMsg(GLchar* _msg);


GLint main(int _argc, char** _argv){

	glutInit(&_argc, _argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(viewport_width,viewport_height);
	glutCreateWindow("3D");
	glutDisplayFunc(render);
	glutMouseFunc(processMouse);
	glutMotionFunc(processMouseActiveMotion);
	glutKeyboardFunc(keyHandler);
	glutSpecialFunc(specialKeyHandler);

	init();

	glutMainLoop();
	return 0;
}


GLvoid freeMemory()
{
 delete start_vector;
 delete end_vector;
 delete rotation_vector;
 delete current_quaternion;
 delete previous_quaternion;
 delete rotation_matrix;
 //delete data_DLD_downsampled;

 delete data_DLD_raw;
 delete data_DLD;
 delete palette;

}


GLvoid setPalette()
{
	// start with these colors and interpolate between them
	GLubyte palette_colors[] = {0, 0, 127,
							 0, 0, 255,
							 0, 127, 255,
							 0, 255, 255,
							 127, 255, 127,
							 255, 255, 0,
							 255, 127, 0,
							 255, 0, 0,
							 127, 0, 0,
							 255, 255, 255};

	palette = new GLubyte[3*255];

	// initialise with white
	for(int i=0; i< 3*255; i++)
	{
		palette[i] = 255;
	}

	// we have 10 colors stored in palette_colors
	// we run only from 0 to 8 because the interpolation
	// function accesses always the current color + the next color
	for(GLint c=0; c<9; c++)
	{
		for(GLint i=0; i<25; i++)
		{
			GLfloat weight_c0 =  (25.0 - i)/25.0; // if i=0, the first color gives the total rgb values, the next does not contribute
			GLfloat weight_c1 = i/25.0; // if i=25, only the next color contributes

			// linear interpolation between two neighboring colors
			// red
			palette[c * 3 * 25 + 3 * i] = (GLubyte) (palette_colors[c*3] * weight_c0  + palette_colors[(c+1)*3] * weight_c1);
			// green
			palette[c * 3 * 25 + 3 * i + 1] = (GLubyte) (palette_colors[c*3 + 1] * weight_c0 + palette_colors[(c+1)*3+1] * weight_c1);
			// blue
			palette[c * 3 * 25 + 3 * i + 2] = (GLubyte) (palette_colors[c*3 + 2] * weight_c0 + palette_colors[(c+1)*3 +2] * weight_c1);
		}
	}
}


GLvoid loadDataStack()
{
	for(int n = 0; n <= no_slices; n+=1)
	{
		sprintf(path, "%s%s%d.tif", path_root, filename_root, n);
		loadTiff(path, n);
	}
}


GLvoid render()
{
	if(resolution_mode == LOW_RES)
	{
		glViewport(0,0,viewport_width,viewport_height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-128*viewport_scale, 128*viewport_scale, -128*viewport_scale , 128*viewport_scale,-viewport_far, viewport_far);
		glMatrixMode(GL_MODELVIEW);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		GLubyte voxel_i;
		GLubyte voxel_r;
		GLubyte voxel_g;
		GLubyte voxel_b;


		// ----------------------------------------------------
		// 						DRAW DATA POINTS
		// ----------------------------------------------------
		if(render_mode == RENDER_ALL)
		{
			glLoadIdentity();
			glPushMatrix();
			glMultMatrixf(rotation_matrix);
			glTranslatef(-128/2.0, -128/2.0, 0);
			glBegin(GL_POINTS);
            for(int t=0; t<no_slices; t+=2)
			{
				for(int y=0; y<128; y++)
				{
					for(int x=0; x<128; x++)
					{
						voxel_i = data_DLD[y * 128 + x + t * 128 * 128];

						switch(color_mode)
						{
							case COLOR:
								voxel_r = palette[voxel_i*3];
								voxel_g = palette[voxel_i*3 + 1];
								voxel_b = palette[voxel_i*3 + 2];
								glColor4ub(voxel_r, voxel_g, voxel_b, voxel_alpha);
								break;
							case MONO:
								glColor4ub(voxel_i, voxel_i, voxel_i, voxel_alpha);
								break;
						}

						glLoadIdentity();
						if(t == active_slice && voxel_i > 20)
						{
							voxel_r = palette[voxel_i*3];
							voxel_g = palette[voxel_i*3 + 1];
							glColor3ub(voxel_r,voxel_g, 0);
						}
						glVertex3f(x, y,  -no_slices/2.0 + t);
					}
				}
			}
			glEnd();
			glPopMatrix();

			renderFrame();

			// draw rect around the active slice
			glColor4f(1.0,1.0,0.0,0.8);
			glPushMatrix();
			glMultMatrixf(rotation_matrix);
			glTranslatef(-128/2.0, -128/2.0, 0);
			glBegin(GL_LINE_LOOP);
				glVertex3f(0, 0, -no_slices/2.0 + active_slice);
				glVertex3f(128, 0, -no_slices/2.0 + active_slice);
				glVertex3f(128, 128, -no_slices/2.0 + active_slice);
				glVertex3f(0, 128, -no_slices/2.0 + active_slice);
			glEnd();
			glPopMatrix();
		}

		else if(render_mode == RENDER_SINGLE)
		{
			glLoadIdentity();
			glPushMatrix();
			glMultMatrixf(rotation_matrix);
			glTranslatef(-128/2.0, -128/2.0, 0);
			if(data_mode == DATA_XY)
			{
				glBegin(GL_POINTS);
					for(int y=0; y<128; y++)
					{
						for(int x=0; x<128; x++)
						{
							voxel_i = data_DLD[y * 128 + x + active_slice * 128 * 128];
							switch(color_mode)
							{
								case COLOR:
									voxel_r = palette[voxel_i*3];
									voxel_g = palette[voxel_i*3 + 1];
									voxel_b = palette[voxel_i*3 + 2];
									glColor4ub(voxel_r, voxel_g, voxel_b, voxel_alpha);
									break;
								case MONO:
									glColor4ub(voxel_i, voxel_i, voxel_i, voxel_alpha);
									break;
							}

							glLoadIdentity();
							glVertex3f(x, y, -no_slices/2.0 + active_slice);
						}
					}
				glEnd();
				glPopMatrix();

				renderFrame();

				// draw rect around the active slice
				glColor4f(1.0,1.0,0.0,0.8);
				glPushMatrix();
				glMultMatrixf(rotation_matrix);
				glTranslatef(-128/2.0, -128/2.0, 0);
				glBegin(GL_LINE_LOOP);
					glVertex3f(0, 0, -no_slices/2.0 + active_slice);
					glVertex3f(128, 0, -no_slices/2.0 + active_slice);
					glVertex3f(128, 128, -no_slices/2.0 + active_slice);
					glVertex3f(0, 128, -no_slices/2.0 + active_slice);
				glEnd();

				glPopMatrix();
			} // XY

			else if(data_mode == DATA_EY)
			{
				glLoadIdentity();
				glPushMatrix();
				glMultMatrixf(rotation_matrix);
				glTranslatef(-128/2.0, -128/2.0, -no_slices/2.0);
				glBegin(GL_POINTS);
				for(int y=0; y<128; y++)
				{
					for(int e=0; e<no_slices; e++)
					{
						voxel_i = data_DLD[y * 128 + y_linecut_x_position + e * 128 * 128];
						switch(color_mode)
						{
							case COLOR:
								voxel_r = palette[voxel_i*3];
								voxel_g = palette[voxel_i*3 + 1];
								voxel_b = palette[voxel_i*3 + 2];
								glColor4ub(voxel_r, voxel_g, voxel_b, voxel_alpha);
								break;
							case MONO:
								glColor4ub(voxel_i, voxel_i, voxel_i, voxel_alpha);
								break;
						}

						glLoadIdentity();
						glVertex3f(y_linecut_x_position, y, e);
					}
				}
				glEnd();
				glPopMatrix();

				renderFrame();

				// draw rect around the active slice
				glColor4f(1.0,1.0,0.0,0.8);
				glPushMatrix();
				glMultMatrixf(rotation_matrix);
				glTranslatef(-128/2.0, -128/2.0, -no_slices/2.0);
				glBegin(GL_LINE_LOOP);
					glVertex3f(y_linecut_x_position, 128, 0);
					glVertex3f(y_linecut_x_position, 128, no_slices);
					glVertex3f(y_linecut_x_position, 0, no_slices);
					glVertex3f(y_linecut_x_position, 0, 0);
				glEnd();

				glPopMatrix();
			} //EY

			if(show_y_linecut)
				renderYLineCut();


		}// RENDER_SINGLE


	}// check render_mode

	else if(resolution_mode == HIGH_RES)
	{
        data_mode = DATA_XY;

		glViewport(0,0,viewport_width,viewport_height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-data_DLD_width * viewport_scale, data_DLD_width * viewport_scale, -data_DLD_height * viewport_scale , data_DLD_height * viewport_scale,-viewport_far, viewport_far);
		glMatrixMode(GL_MODELVIEW);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		GLuint voxel_i;
		GLubyte voxel_r;
		GLubyte voxel_g;
		GLubyte voxel_b;

		if(render_mode == RENDER_SINGLE)
		{
			glLoadIdentity();
			glPushMatrix();
			glMultMatrixf(rotation_matrix);
			glTranslatef(-data_DLD_width/2.0, -data_DLD_height/2.0, 0);

			if(data_mode == DATA_XY)
			{
				// only RENDER_SINGLE in HIGH_RES mode (for performance)
				glBegin(GL_POINTS);
				for(int y=0; y<data_DLD_height; y++)
				{
					for(int x=0; x<data_DLD_width; x++)
					{

						voxel_i = data_DLD_raw[y * data_DLD_width + x + active_slice * data_DLD_width * data_DLD_height];

						switch(color_mode)
						{
							case COLOR:
								voxel_r = palette[(GLint) voxel_i*3];
								voxel_g = palette[(GLint) voxel_i*3 + 1];
								voxel_b = palette[(GLint) voxel_i*3 + 2];
								glColor4ub(voxel_r, voxel_g, voxel_b, voxel_alpha);
								break;
							case MONO:
								glColor4ub(voxel_i, voxel_i, voxel_i, voxel_alpha);
								break;
						}

						glLoadIdentity();
						glVertex3f(x, y, -no_slices/2.0 + active_slice);
					}
				}
				glEnd();
				glPopMatrix();

				renderFrame();

				// draw rect around the active slice
				glColor4f(1.0,1.0,0.0,0.8);
				glPushMatrix();
				glMultMatrixf(rotation_matrix);
				glTranslatef(-data_DLD_width/2.0, -data_DLD_height/2.0, 0);
				glBegin(GL_LINE_LOOP);
					glVertex3f(0, 0, -no_slices/2.0 + active_slice);
					glVertex3f(data_DLD_width, 0, -no_slices/2.0 + active_slice);
					glVertex3f(data_DLD_width, data_DLD_height, -no_slices/2.0 + active_slice);
					glVertex3f(0, data_DLD_height, -no_slices/2.0 + active_slice);
				glEnd();

				glPopMatrix();
			} //DATA XY

			else if(data_mode == DATA_EY)
			{
				glLoadIdentity();
				glPushMatrix();
				glMultMatrixf(rotation_matrix);
				glTranslatef(-data_DLD_width/2.0, -data_DLD_height/2.0, -no_slices/2.0);
				glBegin(GL_POINTS);
				for(int y=0; y<data_DLD_height; y++)
				{
					for(int e=0; e<no_slices; e++)
					{
						voxel_i = data_DLD_raw[y * data_DLD_width + y_linecut_x_position_high_res + e * data_DLD_width * data_DLD_height];
						switch(color_mode)
						{
							case COLOR:
								voxel_r = palette[voxel_i*3];
								voxel_g = palette[voxel_i*3 + 1];
								voxel_b = palette[voxel_i*3 + 2];
								glColor4ub(voxel_r, voxel_g, voxel_b, voxel_alpha);
								break;
							case MONO:
								glColor4ub(voxel_i, voxel_i, voxel_i, voxel_alpha);
								break;
						}

						glLoadIdentity();
						glVertex3f(y_linecut_x_position_high_res, y, e);
					}
				}
				glEnd();
				glPopMatrix();

				renderFrame();

				// draw rect around the active slice
				glColor4f(1.0,1.0,0.0,0.8);
				//glLoadIdentity();
				glPushMatrix();
				glMultMatrixf(rotation_matrix);
				glTranslatef(-data_DLD_width/2.0, -data_DLD_height/2.0, -no_slices/2.0);
				glBegin(GL_LINE_LOOP);
					glVertex3f(y_linecut_x_position_high_res, data_DLD_height, 0);
					glVertex3f(y_linecut_x_position_high_res, data_DLD_height, no_slices);
					glVertex3f(y_linecut_x_position_high_res, 0, no_slices);
					glVertex3f(y_linecut_x_position_high_res, 0, 0);
				glEnd();

				glPopMatrix();
			} //EY
		} // SINGLE
		else
		{
			render_mode == RENDER_SINGLE;
		}
	}

	glutSwapBuffers();
}

GLvoid renderYLineCut()
{

	glLoadIdentity();
	glPushMatrix();
	glMultMatrixf(rotation_matrix);
	glTranslatef(-128/2.0, -128/2.0, -no_slices/2.0);
	/*
	glColor4f(1,0,0,0.2);
	glBegin(GL_QUADS);
        glVertex3f(y_linecut_x_position, plane_height, -plane_length);
        glVertex3f(y_linecut_x_position, plane_height, plane_length);
        glVertex3f(y_linecut_x_position, -plane_height, plane_length);
        glVertex3f(y_linecut_x_position, -plane_height, -plane_length);
    glEnd();*/
	glColor4f(1,1,0,1);
	glBegin(GL_LINES);
	    glVertex3f(y_linecut_x_position, 0, active_slice);
        glVertex3f(y_linecut_x_position, 128,  active_slice);
	glEnd();
	glPopMatrix();

}

GLvoid renderXLineCut()
{

}

GLvoid resetRotationMatrix()
{
	data_mode = DATA_XY; // cause i use this function to set the top view (which only makes sense with DATA_XY)

	for(int i=0; i<16; i++)
	{
		rotation_matrix[i]=0.0;
	}
	rotation_matrix[0] = 1.0;
	rotation_matrix[5] = 1.0;
	rotation_matrix[10]= 1.0;
	rotation_matrix[15] = 1.0;

	current_quaternion->reset();
	previous_quaternion->reset();

	glutPostRedisplay();

}


GLvoid initTrackball()
{
	rotation_matrix = new GLfloat[16];

	// start with unit matrix
	for(int i=0; i<16; i++)
	{
		rotation_matrix[i]=0.0;
	}
	rotation_matrix[0] = 1.0;
	rotation_matrix[5] = 1.0;
	rotation_matrix[10]= 1.0;
	rotation_matrix[15] = 1.0;

	// init as unit quaternions
	current_quaternion = new GLQuaternion4f(1,0,0,0);
	previous_quaternion = new GLQuaternion4f(1,0,0,0);
	start_vector = new GLVector3f();
	end_vector = new GLVector3f();
	rotation_vector = new GLVector3f();
}

GLvoid initOpenGL()
{
	//glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE);
	glClearColor(0.0,0.0,0.0,1.0);
}

GLvoid initAdjustableParameters()
{
	resolution_mode = LOW_RES;
	render_mode = RENDER_SINGLE;
	color_mode = COLOR;
	data_mode = DATA_XY;

	active_slice = 0;
	show_x_linecut = false;
	show_y_linecut = false;
	y_linecut_x_position = 64;
	x_linecut_y_position = 64;

	y_linecut_x_position_high_res = 256;
	x_linecut_y_position_high_res = 256;
}

GLvoid initStateVariables()
{
	data_downsampled = false;
	data_downscaled = false;
}

GLvoid init(){

	initOpenGL();
	initTrackball();
	initStateVariables();
	initAdjustableParameters();

	data_DLD_raw = new GLushort[512*512*no_slices];


	loadDataStack();
	downsample();
	downscale();
	setPalette();

}


GLvoid specialKeyHandler(GLint _key, GLint _x, GLint _y)
{
	switch(_key)
	{
		case GLUT_KEY_F1:
			data_mode = DATA_XY;
			glutPostRedisplay();
			break;
		case GLUT_KEY_F2:
			data_mode = DATA_EY;
			glutPostRedisplay();
			break;
		case GLUT_KEY_F3:
			data_mode = DATA_EX;
			glutPostRedisplay();
			break;
		case GLUT_KEY_F4:
            /*if(resolution_mode == HIGH_RES)
			{
				resolution_mode = LOW_RES;
			}
			else
				resolution_mode = HIGH_RES;
			glutPostRedisplay();
            */
            break;
		case GLUT_KEY_F5:
			if(color_mode == COLOR)
			{
				color_mode = MONO;
			}
			else
				color_mode = COLOR;
			glutPostRedisplay();
			break;
		case GLUT_KEY_F6:
			if(render_mode == RENDER_ALL)
			{
				render_mode = RENDER_SINGLE;
			}
			else
				render_mode = RENDER_ALL;
			glutPostRedisplay();
			break;
		case GLUT_KEY_HOME:
			break;
		case GLUT_KEY_END:
			break;
		case GLUT_KEY_INSERT:
			break;
		case GLUT_KEY_RIGHT:
			break;
		case GLUT_KEY_LEFT:
			break;
		case GLUT_KEY_UP:
			break;
		case GLUT_KEY_DOWN:
			break;
		case GLUT_KEY_PAGE_UP:
			break;
		case GLUT_KEY_PAGE_DOWN:
			break;

	}
}

GLvoid keyHandler(GLubyte _key, GLint _x, GLint _y)
{
	switch(_key){
		case 27:
			freeMemory();
			exit(EXIT_SUCCESS);
			break;
		case 'm':
			if(viewport_scale < 2)
				viewport_scale+=0.1;
			glutPostRedisplay();
			break;
		case 'n':
			if(viewport_scale>0.2)
				viewport_scale-=0.1;
			glutPostRedisplay();
			break;
		case '+':
			if(voxel_alpha < 245)
			{
				voxel_alpha+=10;
				glutPostRedisplay();
			}
			break;
		case '-':
			if(voxel_alpha > 10)
			{
				voxel_alpha-=10;
				glutPostRedisplay();
			}
			break;
		case 'r':
			resetRotationMatrix();
			break;
		case 'y':
			if(show_y_linecut)
				show_y_linecut = false;
			else
				show_y_linecut = true;
			glutPostRedisplay();
			break;
		case 't':
			setSideView();
			break;

			if(glutGetModifiers() == GLUT_ACTIVE_SHIFT)
			{}
			if(glutGetModifiers() == GLUT_ACTIVE_CTRL)
			{}
			if(glutGetModifiers() == GLUT_ACTIVE_ALT)
			{}
	}
}

GLvoid processMouse(GLint button, GLint state, GLint x, GLint y)
{
	// down without modifiers
	if(state == GLUT_DOWN && !glutGetModifiers())
	{
		if(button == GLUT_LEFT_BUTTON)
		{

		start_vector->x = (GLfloat) x;
		start_vector->y = (GLfloat) y;

		start_vector->x /= viewport_width/2.0;
		start_vector->y /= viewport_height/2.0;

		start_vector->x = start_vector->x - 1.0;
		start_vector->y = 1.0 - start_vector->y;

		GLfloat z = 1.0 - start_vector->x * start_vector->x + start_vector->y * start_vector->y ;

		if(z > 0.0f)
			start_vector->z = (GLfloat) sqrt(z);
		else
			start_vector->z = 0.0;

		start_vector->normalize();
		}
		else if(button == 3 && data_mode == DATA_XY && !show_y_linecut)
		{
			if(active_slice < no_slices){
				active_slice++;
				glutPostRedisplay();
			}
		}
		else if (button == 4 && data_mode == DATA_XY && !show_y_linecut)
		{
			if(active_slice > 0)
			{
				active_slice--;
				glutPostRedisplay();
			}
		}


		else if(button == 3 && data_mode == DATA_XY && show_y_linecut)
		{
			if(y_linecut_x_position < 128){
				//debugMsg("x_pos = ",(GLfloat) y_linecut_x_position);
				y_linecut_x_position++;
				glutPostRedisplay();
			}
		}
		else if (button == 4 && data_mode == DATA_XY && show_y_linecut)
		{
			if(y_linecut_x_position > 0)
			{
				y_linecut_x_position--;
				glutPostRedisplay();
			}
		}


		else if(button == 3 && data_mode == DATA_EY && resolution_mode == LOW_RES && !show_y_linecut)
		{
			if(y_linecut_x_position < 128){
				y_linecut_x_position++;
				glutPostRedisplay();
			}
		}
		else if (button == 4 && data_mode == DATA_EY && resolution_mode == LOW_RES && !show_y_linecut)
		{
			if(y_linecut_x_position > 0)
			{
				y_linecut_x_position--;
				glutPostRedisplay();
			}
		}


		else if(button == 3 && data_mode == DATA_EY && resolution_mode == LOW_RES && show_y_linecut)
		{
			//debugMsg("x_pos = ",(GLfloat) y_linecut_x_position);
			if(active_slice < no_slices){
				//debugMsg("x_pos = ",(GLfloat) y_linecut_x_position);
				active_slice++;
				glutPostRedisplay();
			}
		}
		else if (button == 4 && data_mode == DATA_EY && resolution_mode == LOW_RES && show_y_linecut)
		{
			if(active_slice > 0)
			{
				active_slice--;
				glutPostRedisplay();
			}
		}



		else if(button == 3 && data_mode == DATA_EY && resolution_mode == HIGH_RES)
		{
			if(y_linecut_x_position_high_res < data_DLD_width){
				y_linecut_x_position_high_res++;
				glutPostRedisplay();
			}
		}
		else if (button == 4 && data_mode == DATA_EY && resolution_mode == HIGH_RES)
		{
			if(y_linecut_x_position_high_res > 0)
			{
				y_linecut_x_position_high_res--;
				glutPostRedisplay();
			}
		}


	}


	// down with modifiers
	else if(state == GLUT_DOWN && glutGetModifiers())
	{
				// ZOOM IN
		if(glutGetModifiers() == GLUT_ACTIVE_CTRL)
		{
			if(button == 3)
			{
				if(viewport_scale < 2)
				{
					viewport_scale+=0.05;
					glutPostRedisplay();
				}
			}

			else if(button == 4)
			{
				if(viewport_scale > 0.05)
				{
					viewport_scale-=0.05;
					glutPostRedisplay();
				}
			}

		}

		else if(button == GLUT_RIGHT_BUTTON)
		{}

		else if(glutGetModifiers() == GLUT_ACTIVE_SHIFT)
		{}

		else if(glutGetModifiers() == GLUT_ACTIVE_ALT)
		{}
	}



	else if(state == GLUT_UP)
	{
		previous_quaternion->imag->x = current_quaternion->imag->x;
		previous_quaternion->imag->y = current_quaternion->imag->y;
		previous_quaternion->imag->z = current_quaternion->imag->z;
		previous_quaternion->real = current_quaternion->real;
	}

}

GLvoid setSideView()
{
	resetRotationMatrix();

	data_mode = DATA_EY;

	current_quaternion->polar(GL_PI/2.0, 0.0, 1.0, 0.0);
	current_quaternion->multiply(previous_quaternion);
	current_quaternion->normalize();
	rotation_matrix = current_quaternion->getRotationMatrix();

	previous_quaternion->imag->x = current_quaternion->imag->x;
	previous_quaternion->imag->y = current_quaternion->imag->y;
	previous_quaternion->imag->z = current_quaternion->imag->z;
	previous_quaternion->real = current_quaternion->real;


	current_quaternion->polar(GL_PI/2.0, 0.0, 0.0, -1.0);
	current_quaternion->multiply(previous_quaternion);
	current_quaternion->normalize();
	rotation_matrix = current_quaternion->getRotationMatrix();

	previous_quaternion->imag->x = current_quaternion->imag->x;
	previous_quaternion->imag->y = current_quaternion->imag->y;
	previous_quaternion->imag->z = current_quaternion->imag->z;
	previous_quaternion->real = current_quaternion->real;

	glutPostRedisplay();
}

GLvoid processMouseActiveMotion(GLint x, GLint y)
{
		end_vector->x = (GLfloat) x;
		end_vector->y = (GLfloat) y;

		end_vector->x /= viewport_width/2.0;
		end_vector->y /= viewport_height/2.0;

		end_vector->x = end_vector->x - 1.0;
		end_vector->y = 1.0 - end_vector->y;

		GLfloat z = 1.0 - end_vector->x * end_vector->x + end_vector->y * end_vector->y ;

		if(z > 0.0f)
			end_vector->z = (GLfloat) sqrt(z);
		else
			end_vector->z = 0.0;

		end_vector->normalize();

		rotation_angle = (GLfloat) acos(start_vector->dotProduct(end_vector));

		// set end_vector as result of the cross product = rotation vector
		end_vector->crossProduct(start_vector);
		current_quaternion->polar(rotation_angle, end_vector);
		current_quaternion->multiply(previous_quaternion);
		current_quaternion->normalize();
		rotation_matrix = current_quaternion->getRotationMatrix();
		glutPostRedisplay();

}

GLvoid debugMsg(GLchar* _arg_desc, GLfloat _arg_val)
{
  std::cout << _arg_desc << ":\t" << _arg_val << std::endl;
}

GLvoid debugMsg(GLchar* _msg)
{
	std::cout << _msg << std::endl;
}

GLvoid renderFrame()
{
	if(resolution_mode == HIGH_RES)
	{

		glPopMatrix();
		glLoadIdentity();
		glPushMatrix();
		glMultMatrixf(rotation_matrix);

		glTranslatef(-data_DLD_width/2.0, -data_DLD_height/2.0, -no_slices/2.0);
		glColor4f(0.2,0.2,0.2,0.8);

			glBegin(GL_LINE_LOOP);
				glVertex3f(0, data_DLD_height, 0);
				glVertex3f(0, data_DLD_height, no_slices);
				glVertex3f(0, 0, no_slices);
				glVertex3f(0, 0, 0);
			glEnd();
			glBegin(GL_LINE_LOOP);
				glVertex3f(data_DLD_width, data_DLD_height, 0);
				glVertex3f(data_DLD_width, data_DLD_height, no_slices);
				glVertex3f(data_DLD_width, 0, no_slices);
				glVertex3f(data_DLD_width, 0, 0);
			glEnd();
			glBegin(GL_LINES);
				//bottom lines
				glVertex3f(data_DLD_width,0,0);
				glVertex3f(0,0,0);
				glVertex3f(data_DLD_width,0,no_slices);
				glVertex3f(0,0,no_slices);
				//top lines
				glVertex3f(data_DLD_width,data_DLD_height,no_slices);
				glVertex3f(0,data_DLD_height,no_slices);
				glVertex3f(data_DLD_width,data_DLD_height,0);
				glVertex3f(0,data_DLD_height,0);
			glEnd();

		glPopMatrix();
	}
	else if(resolution_mode == LOW_RES)
	{
		glPopMatrix();
		glLoadIdentity();
		glPushMatrix();
		glMultMatrixf(rotation_matrix);
		glTranslatef(-128/2.0, -128/2.0, -no_slices/2.0);
		glColor4f(0.2,0.2,0.2,0.8);

			glBegin(GL_LINE_LOOP);
				//left ring
				glVertex3f(0, 128, 0);
				glVertex3f(0, 128, no_slices);
				glVertex3f(0, 0, no_slices);
				glVertex3f(0, 0, 0);
			glEnd();
			glBegin(GL_LINE_LOOP);
				//right ring
				glVertex3f(128, 128, 0);
				glVertex3f(128, 128, no_slices);
				glVertex3f(128, 0, no_slices);
				glVertex3f(128, 0, 0);
			glEnd();
			glBegin(GL_LINES);
				//bottom lines
				glVertex3f(128,0,0);
				glVertex3f(0,0,0);
				glVertex3f(128,0,no_slices);
				glVertex3f(0,0,no_slices);
				//top lines
				glVertex3f(128,128,no_slices);
				glVertex3f(0,128,no_slices);
				glVertex3f(128,128,0);
				glVertex3f(0,128,0);
			glEnd();

		glPopMatrix();
	}
}

GLvoid downsample()
{

	/**
	 *  TO DO: maybe downsampling should be adjusted to an max_average rather than using a single max
	 * 			-> false color scale will be much more efficient in resolution in HIGH_RES mode
	 */

	GLushort max_count_rate = 1; //>= 1 because of the devision
	GLushort count_rate;

	//two runs through data_DLD_raw are necessary, 1. to get the global max count rate, 2. to downsample the data
	for(int t = 0; t < no_slices; t++)
	{
		for (int y = 0; y < data_DLD_height; y++)
		{
			for(int x = 0; x < data_DLD_width; x++)
			{
				count_rate = data_DLD_raw[y * data_DLD_width + x + t * data_DLD_width * data_DLD_height];
				if(count_rate > max_count_rate)
				{
					max_count_rate = count_rate;
				}

			}
		}
	}


	for(GLint i=0; i< data_DLD_width * data_DLD_height * no_slices; i++)
	{
		data_DLD_raw[i] = (GLushort) (255 * (data_DLD_raw[i]/((GLfloat) max_count_rate)));
	}

	data_downsampled = true;
}

GLvoid downscale()
{

	if(data_downsampled)
	{
		if(data_DLD_width != data_DLD_height)
		{
			std::cout << "info: data_DLD_width != data_DLD_height" << std::endl;
			std::cout << "downscaling not possilbe!" << std::endl;
			return;
		}
		// downscaled and downsampled final data array
		//data_DLD = (GLubyte*) malloc(128 * 128 * no_slices * sizeof(GLubyte));
		data_DLD = new GLubyte[128*128*no_slices];

		//initialise with 0
		for(GLint i=0; i<128 * 128 * no_slices; i++)
			data_DLD[i] = 0;

		if(data_DLD_width == 512)
		{

			// write the averaged data in a downscaled buffer of type GLushort,
			// because averaging will produce values bigger than 255 while summing up
			// if averaging is done, copy the buffer to data_DLD
			GLushort* data_buffer = new GLushort[128*128*no_slices];

			// init with 0
			for(GLint i=0; i<128*128*no_slices; i++)
			{
				data_buffer[i] = 0;
			}

			GLint sampling_counter_x = 0;
			GLint sampling_counter_y = 0;
			GLushort count_rate = 0;
			GLint data_DLD_index_x = 0;
			GLint data_DLD_index_y = 0;

			for(int t = 0; t < no_slices; t++)
			{

			sampling_counter_x = 0;
			sampling_counter_y = 0;
			count_rate = 0;
			data_DLD_index_y = 0;
			data_DLD_index_x = 0;

				for (int y = 0; y < data_DLD_height; y++)
				{

					for(int x = 0; x < data_DLD_width; x++)
					{
						count_rate += data_DLD_raw[y * data_DLD_width + x + t * data_DLD_width * data_DLD_height];
						sampling_counter_x++;

						// sum up 4 pixels over 4 rows
						if((sampling_counter_x % 4 == 0) && (sampling_counter_x != 0))
						{
							data_buffer[data_DLD_index_x + data_DLD_index_y * 128 + t * 128 * 128] += count_rate;
							count_rate = 0;
							data_DLD_index_x++;
						}

					}
					data_DLD_index_x = 0;
					sampling_counter_x = 0;
					sampling_counter_y++;
					if(sampling_counter_y % 4 == 0 && sampling_counter_y != 0)
					{

						data_DLD_index_y++;
					}

				}
			}

			/*
			 *  DOWNSAMPLE AGAIN
			 */

			GLushort max_count_rate = 1; //>= 1 because of the devision

			// get the max count rate
			for(GLint i=0; i< 128 * 128 * no_slices; i++)
			{
				count_rate = data_buffer[i];
				if(count_rate > max_count_rate)
				{
					max_count_rate = count_rate;
				}
			}

			// downsample to 8Bit
			for(GLint i=0; i< 128 * 128 * no_slices; i++)
			{
				//data_buffer[i] = (GLushort) (255 * (data_buffer[i]/((GLfloat) max_count_rate)));
				data_DLD[i] = (GLubyte) (255 * (data_buffer[i]/((GLfloat) max_count_rate)));
			}


			// release memory
			delete data_buffer;

		}

	}

	data_downscaled = true;
}


GLvoid loadTiff(GLchar* _path, GLint _time_slice)
{
    TIFF* tif = TIFFOpen(_path, "r");
    if (tif) {

    uint32 image_height;
	uint32 image_width;
	uint16 config;
	uint16* data_buffer;
	GLint intensity;

    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &image_height);
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &image_width);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &config);

	data_DLD_height = (GLint) image_height;
	data_DLD_width = (GLint) image_width;

	data_buffer = (uint16*)_TIFFmalloc(image_width * sizeof(uint16));

    if (config == PLANARCONFIG_CONTIG)
	{

		for (GLint y = 0; y < image_height; y++)
		{
		    TIFFReadScanline(tif, data_buffer, y,0);

			for(GLint x = 0; x < image_width; x++)
			{
			  data_DLD_raw[y * image_width + x + _time_slice * image_width * image_height] = (GLushort) data_buffer[x];
			}
		}
    }
    _TIFFfree(data_buffer);
    TIFFClose(tif);
    }
}
