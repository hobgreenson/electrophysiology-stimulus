
#ifndef MESH_H
#define MESH_H

#include <GLFW/glfw3.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <ctime>
#include "Vertex2D.h"

class Mesh
{
public:
    Mesh(const char* vs_path, const char* fs_path);
    ~Mesh();
    
    float aspect_ratio_;
    
    // buffer id's for this mesh
    GLuint vertex_buffer_;
    GLuint index_buffer_;
    GLuint vao_;

    // vertex and index data defining the mesh 
    int num_vertices_;
    int num_indices_;
    Vertex2D* vertices_;
    GLushort* indices_;

    // this is uniform data to pass to shaders
    GLfloat transform_matrix_[16];

    // in my experience, each mesh will want it's own GLSL program
    const char* vertex_shader_path_;
    const char* fragment_shader_path_;
    GLuint program_;
    GLint transform_matrix_location_;
    
    // functions create vertex and index data defining the mesh
    void rect(float lower_x, float lower_y,
              float upper_x, float upper_y);
    void makeVerticesRect(float lower_x, float lower_y,
                          float upper_x, float upper_y);
    void makeIndicesRect();
    
    void circle(float radius, float cx, float cy);
    void makeVerticesCircle(float radius, float cx, float cy);
    void makeIndicesCircle();
    
    void rotatingGrating(int periods);
    void makeRotatingGratingVertices(int periods);
    void makeRotatingGratingIndices(int periods);
    
    void linearGrating(int periods);
    void makeLinearGratingVertices(int periods);
    void makeLinearGratingIndices(int periods);
    
    // simple functions to modify color, position, shape of mesh
    void color(float R, float G, float B, float A);
    void randColor(float A);
    void randGrayScaleColor(float A);
    void randBlueScaleColor(float A);
    void translateX(float dx);
    void translateY(float dy);
    void translateZ(float dz);
    void centerXY(float x, float y);
    void scaleX(float da);
    void scaleY(float da);
    void scaleXY(float da);
    void resetScale();
    void rotateZ(float dtheta);
    
};
    
#endif
