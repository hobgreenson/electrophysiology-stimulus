#include "Mesh.h"

Mesh::Mesh(const char* vs_path, const char* fs_path, float aspect_ratio)
    : aspect_ratio_(aspect_ratio),
      vertex_shader_path_(vs_path),
      fragment_shader_path_(fs_path)
{
    srand(time(NULL));
    
    // set transform matrix to identity
    GLfloat matrix[16] = 
    {
        1.0, 0.0, 0.0, 0.0,
        0.0, aspect_ratio_, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    for (int i = 0; i < 16; ++i)
        transform_matrix_[i] = matrix[i];
}

Mesh::~Mesh()
{
    free(vertices_);
    free(indices_);
}

/*********** Color ********************************/
void Mesh::color(float R, float G, float B, float A)
{
    GLubyte color[4];
    color[0] = R;
    color[1] = G;
    color[2] = B;
    color[3] = A;
    
    for (int i = 0; i < num_vertices_; ++i)
    {
        vertices_[i].color[0] = color[0];
        vertices_[i].color[1] = color[1];
        vertices_[i].color[2] = color[2];
        vertices_[i].color[3] = color[3];
    }
}

void Mesh::randColor(float A)
{
    GLubyte color[4];
    
    int r = rand() % 10;
    float R = r / 10.0;
    color[0] = R;
    
    int g = rand() % 10;
    float G = g / 10.0;
    color[1] = G;
    
    int b = rand() % 10;
    float B = b / 10.0;
    color[2] = B;
    
    color[3] = A;
    
    for (int i = 0; i < num_vertices_; ++i)
    {
        vertices_[i].color[0] = color[0];
        vertices_[i].color[1] = color[1];
        vertices_[i].color[2] = color[2];
        vertices_[i].color[3] = color[3];
    }
}

void Mesh::randGrayScaleColor(float A)
{
    int i = rand() % 10;
    float L = i / 10.0;
    GLubyte color[4];
    color[0] = L;
    color[1] = L;
    color[2] = L;
    color[3] = A;
    
    for (int i = 0; i < num_vertices_; ++i)
    {
        vertices_[i].color[0] = color[0];
        vertices_[i].color[1] = color[1];
        vertices_[i].color[2] = color[2];
        vertices_[i].color[3] = color[3];
    }
}

void Mesh::randBlueScaleColor(float A)
{
    int i = rand() % 10;
    float B = i / 10.0;
    GLubyte color[4];
    color[0] = 0;
    color[1] = 0;
    color[2] = B;
    color[3] = A;
    
    for (int i = 0; i < num_vertices_; ++i)
    {
        vertices_[i].color[0] = color[0];
        vertices_[i].color[1] = color[1];
        vertices_[i].color[2] = color[2];
        vertices_[i].color[3] = color[3];
    }
}

/******** Simple spatial transforms *******************************/
void Mesh::translateX(float dx)
{
    transform_matrix_[12] += dx;
}

void Mesh::translateY(float dy)
{
    transform_matrix_[13] += dy;
}

void Mesh::translateZ(float dz)
{
    transform_matrix_[14] += dz;
}

void Mesh::centerXY(float x, float y)
{
    transform_matrix_[12] = x;
    transform_matrix_[13] = y;
}

void Mesh::scaleX(float da)
{
    transform_matrix_[0] *= da;
}

void Mesh::scaleY(float da)
{
    transform_matrix_[5] *= da;
}

void Mesh::scaleXY(float da)
{
    scaleX(da);
    scaleY(da);
}

void Mesh::resetScale()
{
    transform_matrix_[0] = 1.0;
    transform_matrix_[5] = aspect_ratio_;
}

void Mesh::rotateZ(float dtheta)
{
    
}

/********* RECTANGLE ********************************/
void Mesh::rect(float lower_x, float lower_y,
                float upper_x, float upper_y)
{
    makeVerticesRect(lower_x, lower_y,
                     upper_x, upper_y);
    makeIndicesRect();
}

void Mesh::makeVerticesRect(float lower_x, float lower_y,
                            float upper_x, float upper_y)
{
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

void Mesh::makeIndicesRect()
{
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
void Mesh::circle(float radius, float cx, float cy)
{
    makeVerticesCircle(radius, cx, cy);
    makeIndicesCircle();
}

void Mesh::makeVerticesCircle(float radius, float cx, float cy)
{
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
    for (int i = 0; i < num_vertices_; ++i)
    {
        x = cx + radius * cos(i * M_PI * da / 180.0);
        y = cy + radius * sin(i * M_PI * da / 180.0);
        vertices_[i].position[0] = x;
        vertices_[i].position[1] = y;
    }
}

void Mesh::makeIndicesCircle()
{
    num_indices_ = 3 * (num_vertices_ - 1);
    indices_ = (GLushort*) malloc(num_indices_ * sizeof(GLushort));

    int j = 1;
    for (int i = 0; i < num_indices_ - 1; i += 3)
    {
        indices_[i] = 0;
        indices_[i + 1] = j;
        indices_[i + 2] = (j + 1) % num_vertices_;
        j++;
    }
}

/******************** grating ****************************/

void Mesh::linearGrating(int periods)
{
    makeLinearGratingVertices(periods);
    makeLinearGratingIndices(periods);
}

void Mesh::makeLinearGratingVertices(int periods)
{
    /* this is a variation on the function "makeVertices()"
     that allows for simulation of a linear grating presented
     below the fish. The real magic happens in the vertex shader.
     */
    GLubyte white[4] = {150, 150, 150, 1};
    GLubyte black[4] = {0, 0, 0, 1};
    GLubyte* color;
    
    int num_squares = 3 * 2 * periods;
    num_vertices_ = 2 * 4 * num_squares;
    
    vertices_ = (Vertex2D*) malloc(num_vertices_ * sizeof(Vertex2D));
    
    float w_step;
    int N;
    if (periods == 0)
    {
        w_step = 6;
        N = 1;
    }
    else
    {
        w_step = 1.0 / (float)periods;
        N = 3 * 2 * periods;
    }
    
    int vi = 0;
    
    for (int i = 0; i <= N; ++i)
    {
        if (i == 0 || i == N)
        {
            vertices_[vi + 0].position[0] = -3 + i * w_step;
            vertices_[vi + 0].position[1] = -1;
            vertices_[vi + 1].position[0] = -3 + i * w_step;
            vertices_[vi + 1].position[1] = 1;
            
            if (periods == 0)
                color = white;
            else
                color = (i == 0) ? white : black;
            
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 0].color[ii] = color[ii];
            
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 1].color[ii] = color[ii];
            
            vi += 2;
        }
        else
        {
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + ii].position[0] = -3 + i * w_step;
            
            vertices_[vi + 0].position[1] = -1;
            vertices_[vi + 1].position[1] = 1;
            vertices_[vi + 2].position[1] = -1;
            vertices_[vi + 3].position[1] = 1;
            
            color = i % 2 ? white : black;
            
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 0].color[ii] = color[ii];
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 1].color[ii] = color[ii];
            
            color = i % 2 ? black : white;
            
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 2].color[ii] = color[ii];
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 3].color[ii] = color[ii];
            
            vi += 4;
        }
    }
    
    int ii = vi - 1, jj = vi;
    Vertex2D my_v;
    while (ii >= 0)
    {
        my_v = vertices_[ii--];
        my_v.position[1] *= 2;
        vertices_[jj++] = my_v;
    }
}

void Mesh::makeLinearGratingIndices(int periods)
{
    int num_squares = 3 * 2 * periods;
    num_indices_ = 2 * 6 * num_squares;
    
    indices_ = (GLushort*) malloc(num_indices_ * sizeof(GLushort));
    
    int vi = 0, ii = 0, N;
    
    N = (periods > 0) ? 3 * 2 * periods : 1;
    
    for (int i = 0; i < N; ++i)
    {
        indices_[ii + 0] = vi;
        indices_[ii + 1] = vi + 2;
        indices_[ii + 2] = vi + 3;
        indices_[ii + 3] = vi;
        indices_[ii + 4] = vi + 3;
        indices_[ii + 5] = vi + 1;
        vi += 4;
        ii += 6;
    }
    
    for (int i = N; i < 2 * N; ++i)
    {
        indices_[ii + 0] = vi;
        indices_[ii + 1] = vi + 3;
        indices_[ii + 2] = vi + 2;
        indices_[ii + 3] = vi;
        indices_[ii + 4] = vi + 1;
        indices_[ii + 5] = vi + 3;
        vi += 4;
        ii += 6;
    }
}

void Mesh::rotatingGrating(int periods)
{
    makeLinearGratingVertices(periods);
    makeLinearGratingIndices(periods);
}

void Mesh::makeRotatingGratingVertices(int periods)
{
    // each stripe of the grating will have color black or white
    GLubyte white[4] = {150, 150, 150, 1};
    GLubyte black[4] = {0, 0, 0, 1};
    GLubyte* color;
    
    int num_squares = 3 * 2 * periods;
    num_vertices_ =  4 * num_squares;
    
    // allocate an array of vertex structs
    vertices_ = (Vertex2D*) malloc(num_vertices_ * sizeof(Vertex2D));
    
    // set step size along x-axis
    float x_step;
    int N;
    if (periods == 0)
    {
        x_step = 6;
        N = 1;
    }
    else
    {
        x_step = 1.0 / (float)periods;
        N = 3 * 2 * periods;
    }
    
    int vi = 0;
    
    for (int i = 0; i <= N; ++i)
    {
        if (i == 0 || i == N)
        {
            vertices_[vi + 0].position[0] = -3 + i * x_step;
            vertices_[vi + 0].position[1] = -1;
            vertices_[vi + 1].position[0] = -3 + i * x_step;
            vertices_[vi + 1].position[1] = 1;
            
            if(periods == 0)
                color = white;
            else
                color = (i == 0) ? white : black;
            
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 0].color[ii] = color[ii];
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 0].color[ii] = color[ii];
            
            vi += 2;
        }
        else
        {
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + ii].position[0] = -3 + i * x_step;
            
            vertices_[vi + 0].position[1] = -1;
            vertices_[vi + 1].position[1] = 1;
            vertices_[vi + 2].position[1] = -1;
            vertices_[vi + 3].position[1] = 1;
            
            color = (i % 2) ? white : black;
            
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 0].color[ii] = color[ii];
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 1].color[ii] = color[ii];
            
            color = (i % 2) ? black : white;
            
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 2].color[ii] = color[ii];
            for (int ii = 0; ii < 4; ++ii)
                vertices_[vi + 3].color[ii] = color[ii];
            
            vi += 4;
        }
    }
}

void Mesh::makeRotatingGratingIndices(int periods)
{
    int num_squares = 3 * 2 * periods;
    num_indices_ = 6 * num_squares;
    
    indices_ = (GLushort*) malloc(num_indices_ * sizeof(GLushort));
    
    int vi = 0, ii = 0, N;
    
    N = (periods > 0) ? 3 * 2 * periods : 1;
    
    for(int i = 0; i < N; i++)
    {
        indices_[ii + 0] = vi;
        indices_[ii + 1] = vi + 2;
        indices_[ii + 2] = vi + 3;
        indices_[ii + 3] = vi;
        indices_[ii + 4] = vi + 3;
        indices_[ii + 5] = vi + 1;
        vi += 4;
        ii += 6;
    }
}




