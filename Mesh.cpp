#include "Mesh.h"

Mesh::Mesh(const char* vs_path, const char* fs_path, float aspect_ratio)
    : vertex_shader_path_(vs_path), fragment_shader_path_(fs_path)
{
    srand(time(NULL));
    
    // set transform matrix to identity
    GLfloat matrix[16] = 
    {
        1.0, 0.0, 0.0, 0.0,
        0.0, aspect_ratio, 0.0, 0.0,
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
    color_[0] = R;
    color_[1] = G;
    color_[2] = B;
    color_[3] = A;
}

void Mesh::randColor(float A)
{
    int r = rand() % 10;
    float R = r / 10.0;
    color_[0] = R;
    
    int g = rand() % 10;
    float G = g / 10.0;
    color_[1] = G;
    
    int b = rand() % 10;
    float B = b / 10.0;
    color_[2] = B;
    
    color_[3] = A;
}

void Mesh::randGrayScaleColor(float A)
{
    int i = rand() % 10;
    float L = i / 10.0;
    
    color_[0] = L;
    color_[1] = L;
    color_[2] = L;
    color_[3] = A;
}

void Mesh::randBlueScaleColor(float A)
{
    int i = rand() % 10;
    float B = i / 10.0;
    
    color_[0] = 0;
    color_[1] = 0;
    color_[2] = B;
    color_[3] = A;
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
void Mesh::circle(float radius)
{
    makeVerticesCircle(radius);
    makeIndicesCircle();
}

void Mesh::makeVerticesCircle(float radius)
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
        x = radius * cos(i * M_PI * da / 180.0);
        y = radius * sin(i * M_PI * da / 180.0);
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





