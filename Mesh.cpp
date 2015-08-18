#include "Mesh.h"

Mesh::Mesh(const char* vs_path, const char* fs_path)
    : vertex_shader_path_(vs_path),
      fragment_shader_path_(fs_path) {
          
    // set transform matrix to identity
    GLfloat matrix[16] = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    for (int i = 0; i < 16; ++i) {
        transform_matrix_[i] = matrix[i];
    }
}

Mesh::~Mesh() {
    free(vertices_);
    free(indices_);
}

/*********** Color ********************************/
void Mesh::color(float R, float G, float B, float A) {
    
    GLubyte color[4];
    color[0] = R;
    color[1] = G;
    color[2] = B;
    color[3] = A;
    
    for (int i = 0; i < num_vertices_; ++i) {
        vertices_[i].color[0] = color[0];
        vertices_[i].color[1] = color[1];
        vertices_[i].color[2] = color[2];
        vertices_[i].color[3] = color[3];
    }
}

/******** Simple spatial transforms *******************************/
void Mesh::translateX(double dx) {
    transform_matrix_[12] += dx;
}

void Mesh::translateXmod(double dx, double n) {
    transform_matrix_[12] += dx;
    
    if(transform_matrix_[12] > n) {
        transform_matrix_[12] -= n;
    }
    if(transform_matrix_[12] < -n) {
        transform_matrix_[12] += n;
    }
}

void Mesh::translateY(double dy) {
    transform_matrix_[13] += dy;
}

void Mesh::translateYmod(double dy, double n) {
    transform_matrix_[13] += dy;
    
    if(transform_matrix_[13] > n) {
        transform_matrix_[13] -= n;
    }
    if(transform_matrix_[13] < -n) {
        transform_matrix_[13] += n;
    }
}

void Mesh::translateZ(double dz) {
    transform_matrix_[14] += dz;
}

void Mesh::centerXY(double x, double y) {
    transform_matrix_[12] = x;
    transform_matrix_[13] = y;
}

void Mesh::scaleX(double da) {
    transform_matrix_[0] *= da;
}

void Mesh::scaleY(double da) {
    transform_matrix_[5] *= da;
}

void Mesh::scaleXY(double da) {
    scaleX(da);
    scaleY(da);
}

void Mesh::resetScale() {
    transform_matrix_[0] = 1.0;
    transform_matrix_[5] = 1.0;
}

/********* RECTANGLE ********************************/
void Mesh::rect(float lower_x, float lower_y,
                float upper_x, float upper_y) {
    makeVerticesRect(lower_x, lower_y,
                     upper_x, upper_y);
    makeIndicesRect();
}

void Mesh::makeVerticesRect(float lower_x, float lower_y,
                            float upper_x, float upper_y) {
    num_vertices_ = 4;
    vertices_ = (Vertex2D*) malloc(num_vertices_ * sizeof(Vertex2D));
    
    vertices_[0].position[0] = lower_x;
    vertices_[0].position[1] = lower_y;

    vertices_[1].position[0] = upper_x;
    vertices_[1].position[1] = lower_y;

    vertices_[2].position[0] = upper_x;
    vertices_[2].position[1] = upper_y;

    vertices_[3].position[0] = lower_x;
    vertices_[3].position[1] = upper_y;
}

void Mesh::makeIndicesRect() {
    num_indices_ = 6;
    indices_ = (GLushort*) malloc(num_indices_ * sizeof(GLushort));
    
    // first triangle
    indices_[0] = 0;
    indices_[1] = 1;
    indices_[2] = 2;
    
    // second triangle
    indices_[3] = 0;
    indices_[4] = 2;
    indices_[5] = 3;
}

/********* CIRCLE ********************************/
void Mesh::circle(float radius, float cx, float cy) {
    makeVerticesCircle(radius, cx, cy);
    makeIndicesCircle();
}

void Mesh::makeVerticesCircle(float radius, float cx, float cy) {
    /* by default we make 10 degree steps and
     this makes a fairly hi-res circle.
     */
    GLfloat da = 10.0;
    num_vertices_ = 2 + (int)(360 / da);
    vertices_ = (Vertex2D*) malloc(num_vertices_ * sizeof(Vertex2D));
   
    // first vertex is at the origin
    vertices_[0].position[0] = 0.0;
    vertices_[0].position[1] = 0.0;
     
    GLfloat x, y;
    for (int i = 0; i < num_vertices_; ++i) {
        x = cx + radius * cos(i * M_PI * da / 180.0);
        y = cy + radius * sin(i * M_PI * da / 180.0);
        vertices_[i].position[0] = x;
        vertices_[i].position[1] = y;
    }
}

void Mesh::makeIndicesCircle() {
    num_indices_ = 3 * (num_vertices_ - 1);
    indices_ = (GLushort*) malloc(num_indices_ * sizeof(GLushort));

    int j = 1;
    for (int i = 0; i < num_indices_ - 1; i += 3) {
        indices_[i] = 0;
        indices_[i + 1] = j;
        indices_[i + 2] = (j + 1) % num_vertices_;
        j++;
    }
}

/******************** VERTICAL GRATING ****************************/

void Mesh::rotatingGrating(int periods) {
    makeRotatingGratingVertices(periods);
    makeRotatingGratingIndices(periods);
}

void Mesh::linearGrating(int periods) {
    makeLinearGratingVertices(periods);
    makeLinearGratingIndices(periods);
}

void Mesh::makeLinearGratingVertices(int periods) {
    /* this is a variation on the function "makeVertices()"
     that allows for simulation of a linear grating presented
     below the fish. The real magic happens in the vertex shader.
     */
    GLubyte white[4] = {0, 0, 150, 1};
    GLubyte black[4] = {0, 0, 0, 1};
    GLubyte* color;
    
    int num_squares = 3 * 2 * periods;
    num_vertices_ = 2 * 4 * num_squares;
    
    vertices_ = (Vertex2D*) malloc(num_vertices_ * sizeof(Vertex2D));
    
    float w_step;
    int N;
    
    if (periods == 0) {
        w_step = 6;
        N = 1;
    } else {
        w_step = 1.0 / (float)periods;
        N = 3 * 2 * periods;
    }
    
    int vi = 0;
    
    for (int i = 0; i <= N; ++i) {
        
        if (i == 0 || i == N) {
            
            vertices_[vi + 0].position[0] = -3 + i * w_step;
            vertices_[vi + 0].position[1] = -1;
            vertices_[vi + 1].position[0] = -3 + i * w_step;
            vertices_[vi + 1].position[1] = 1;
            
            if (periods == 0) {
                color = white;
            } else {
                color = (i == 0) ? white : black;
            }
            
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 0].color[ii] = color[ii];
            }
            
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 1].color[ii] = color[ii];
            }
            
            vi += 2;
            
        } else {
            
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + ii].position[0] = -3 + i * w_step;
            }
            
            vertices_[vi + 0].position[1] = -1;
            vertices_[vi + 1].position[1] = 1;
            vertices_[vi + 2].position[1] = -1;
            vertices_[vi + 3].position[1] = 1;
            
            color = i % 2 ? white : black;
            
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 0].color[ii] = color[ii];
            }
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 1].color[ii] = color[ii];
            }
            
            color = i % 2 ? black : white;
            
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 2].color[ii] = color[ii];
            }
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 3].color[ii] = color[ii];
            }
            
            vi += 4;
        }
    }
    
    int ii = vi - 1, jj = vi;
    Vertex2D my_v;
    
    while (ii >= 0) {
        my_v = vertices_[ii--];
        my_v.position[1] *= 2;
        vertices_[jj++] = my_v;
    }
    
}

void Mesh::makeRotatingGratingVertices(int periods) {
    
    // each stripe of the grating will have color black or white
    GLubyte white[4] = {0, 0, 150, 1};
    GLubyte black[4] = {0, 0, 0/*100*/, 1};
    GLubyte* color;
    
    int num_squares = 3 * 2 * periods;
    num_vertices_ =  4 * num_squares;
    
    // allocate an array of vertex structs
    vertices_ = (Vertex2D*) malloc(num_vertices_ * sizeof(Vertex2D));
    
    // set step size along x-axis
    float x_step;
    int N;
    if (periods == 0) {
        x_step = 6;
        N = 1;
    } else {
        x_step = 1.0 / (float)periods;
        N = 3 * 2 * periods;
    }
    
    int vi = 0;
    
    for (int i = 0; i <= N; ++i) {
        
        if (i == 0 || i == N) {
            
            vertices_[vi + 0].position[0] = -3 + i * x_step;
            vertices_[vi + 0].position[1] = -1;
            vertices_[vi + 1].position[0] = -3 + i * x_step;
            vertices_[vi + 1].position[1] = 1;
            
            if(periods == 0) {
                color = white;
            } else {
                color = (i == 0) ? white : black;
            }
            
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 0].color[ii] = color[ii];
            }
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 0].color[ii] = color[ii];
            }
            
            vi += 2;
            
        } else {
            
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + ii].position[0] = -3 + i * x_step;
            }
            
            vertices_[vi + 0].position[1] = -1;
            vertices_[vi + 1].position[1] = 1;
            vertices_[vi + 2].position[1] = -1;
            vertices_[vi + 3].position[1] = 1;
            
            color = (i % 2) ? white : black;
            
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 0].color[ii] = color[ii];
            }
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 1].color[ii] = color[ii];
            }
            
            color = (i % 2) ? black : white;
            
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 2].color[ii] = color[ii];
            }
            for (int ii = 0; ii < 4; ++ii) {
                vertices_[vi + 3].color[ii] = color[ii];
            }
            
            vi += 4;
        }
    }
}

void Mesh::makeRotatingGratingIndices(int periods) {
    
    const int vertices_per_rectangle = 4;
    const int indices_per_rectangle = 6;
    int num_rects = indices_per_rectangle * periods;
    num_indices_ = indices_per_rectangle * num_rects;
    
    indices_ = (GLushort*) malloc(num_indices_ * sizeof(GLushort));
    
    int vi = 0, ii = 0, N;
    
    N = (periods > 0) ? indices_per_rectangle * periods : 1;
    
    for (int i = 0; i < N; ++i) {
        indices_[ii + 0] = vi;
        indices_[ii + 1] = vi + 2;
        indices_[ii + 2] = vi + 3;
        indices_[ii + 3] = vi;
        indices_[ii + 4] = vi + 3;
        indices_[ii + 5] = vi + 1;
        vi += vertices_per_rectangle;
        ii += indices_per_rectangle;
    }
}

void Mesh::makeLinearGratingIndices(int periods) {
    
    const int vertices_per_rectangle = 4;
    const int indices_per_rectangle = 6;
    int num_squares = indices_per_rectangle * periods;
    num_indices_ = 2 * indices_per_rectangle * num_squares;
    
    indices_ = (GLushort*) malloc(num_indices_ * sizeof(GLushort));
    
    int vi = 0, ii = 0, N;
    
    N = (periods > 0) ? indices_per_rectangle * periods : 1;
    
    for (int i = 0; i < N; ++i) {
        indices_[ii + 0] = vi;
        indices_[ii + 1] = vi + 2;
        indices_[ii + 2] = vi + 3;
        indices_[ii + 3] = vi;
        indices_[ii + 4] = vi + 3;
        indices_[ii + 5] = vi + 1;
        vi += vertices_per_rectangle;
        ii += indices_per_rectangle;
    }
    
    for (int i = N; i < 2 * N; ++i) {
        indices_[ii + 0] = vi;
        indices_[ii + 1] = vi + 3;
        indices_[ii + 2] = vi + 2;
        indices_[ii + 3] = vi;
        indices_[ii + 4] = vi + 1;
        indices_[ii + 5] = vi + 3;
        vi += vertices_per_rectangle;
        ii += indices_per_rectangle;
    }
}



