#ifndef __INCLUDESHADERS
#define __INCLUDESHADERS
#include <iostream>
#include <string>
#include "Mesh.h"

std::string textFileRead(const char* filename);
void programerrors(const GLint program);
void shadererrors(const GLint shader);
GLuint initshaders(GLenum type, const char* filename);
void initprogram(Mesh* mesh, GLuint vertexshader, GLuint fragmentshader);

#endif
