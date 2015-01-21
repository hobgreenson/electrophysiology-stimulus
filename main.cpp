#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#include <OpenGL/glext.h>
#include <GLFW/glfw3.h>
#else
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#endif

#include "Vertex2D.h"
#include "load_shader.h"
#include "Mesh.h"
#include <cstdio>

/************ callbacks ************************/
static void error_callback(int error, const char* description)
{
    fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

/************ rendering ************************/
enum ATTRIBUTE_ID
{
    VERTEX_POSITION,
    VERTEX_COLOR,
    VERTEX_NORMAL,
    VERTEX_TEXTURE
};

void checkMyGL()
{
    GLenum gl_err = glGetError();
    if(gl_err != GL_NO_ERROR)
    {
        printf("fuck! an error! %d\n", gl_err);
        exit(EXIT_FAILURE);
    }
}

void drawMesh(Mesh* mesh)
{
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(mesh->vao_);
    glUseProgram(*(mesh->program_));
    glUniformMatrix4fv(*(mesh->transform_matrix_location_), 1, GL_FALSE,
                       mesh->transform_matrix_);
    glUniform4fv(*(mesh->color_location_), 1, mesh->color_);
    glDrawElements(GL_TRIANGLES, mesh->num_indices_,
                   GL_UNSIGNED_SHORT, (const GLvoid*) 0);
    glBindVertexArray(0);
}

void initMeshShaders(Mesh* mesh)
{
    // set up GLSL program for the mesh
    GLuint vs = initshaders(GL_VERTEX_SHADER, mesh->vertex_shader_path_);
    GLuint fs = initshaders(GL_FRAGMENT_SHADER, mesh->fragment_shader_path_);
    GLuint program = initprogram(vs, fs);
    mesh->program_ = &program;
    
    GLint mat_loc = glGetUniformLocation(program, "transform_matrix");
    mesh->transform_matrix_location_ = &mat_loc;
    
    GLint color_loc = glGetUniformLocation(program, "color");
    mesh->color_location_ = &color_loc;
}

void bufferMesh(Mesh* mesh)
{
    // generate buffers and vertex array object for the mesh
    glGenBuffers(1, &(mesh->vertex_buffer_));
    glGenBuffers(1, &(mesh->index_buffer_));
    glGenVertexArrays(1, &(mesh->vao_));
    
    glBindVertexArray(mesh->vao_);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mesh->num_indices_ * sizeof(GLushort),
                 mesh->indices_,
                 GL_STATIC_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER,
                 mesh->num_vertices_ * sizeof(Vertex2D),
                 mesh->vertices_,
                 GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(VERTEX_POSITION);
    glVertexAttribPointer(VERTEX_POSITION, 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex2D),
                          (const GLvoid*) offsetof(Vertex2D, position));
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

/************ main ************************/
int main(int argc, char** argv)
{
    GLFWwindow* window;
    glfwSetErrorCallback(error_callback);
    
    if (!glfwInit())
        exit(EXIT_FAILURE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    window = glfwCreateWindow(mode->width, mode->height,
                              "Hello game!", primary, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    
    glfwMakeContextCurrent(window);
    
#ifdef __APPLE__
#else
    // GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err)
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
#endif
    
    glfwSetKeyCallback(window, key_callback);
    
    float screen_aspect_ratio = (float)mode->width / (float)mode->height;
    
    Mesh prey("./boring.vert", "./boring.frag", screen_aspect_ratio);
    prey.rect(-0.2, -0.2, 0.2, 0.2);
    prey.randBlueScaleColor(1.0);
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
     
    bufferMesh(&prey);
    initMeshShaders(&prey);
    
    double prev_sec = glfwGetTime();
    double curr_sec;
    double dt;
    
    while(!glfwWindowShouldClose(window))
    {
        curr_sec = glfwGetTime();
        dt = curr_sec - prev_sec;
        prev_sec = curr_sec;
        
        drawMesh(&prey);
        prey.randBlueScaleColor(1.0);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


