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
#include "Protocol.h"
#include <serial/serial.h>
#include <cstdio>

#define SCREEN_WIDTH_GL 0.68
#define SCREEN_EDGE_GL -0.34

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

/******** serial port ******************/

//const uint64_t g_kBaud = 4 * 115200; // baud rate
//const uint8_t g_kMsg = 'a'; // this message triggers the camera/ephys
//serial::Serial g_serial("/dev/tty.usbmodem1411", g_kBaud, serial::Timeout::simpleTimeout(1000));
bool g_serial_up = false;

void triggerSerial()
{
    //g_serial.write(&g_kMsg, 1);
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

/************ logic *****************************/

void initExperiment(bool* not_done, Mesh* mesh, Protocol* protocol, float* vel_x,
                    double* elapsed_in_trial, double* trial_duration)
{
    *not_done = true;
    *vel_x = protocol->nextSpeed();
    float scale_r = protocol->nextSize();
    *trial_duration = SCREEN_WIDTH_GL / (*vel_x);
    *elapsed_in_trial = 0;
    mesh->resetScale();
    mesh->scaleXY(scale_r);
    mesh->centerXY(SCREEN_EDGE_GL, -0.02);
    if (!g_serial_up)
    {
        triggerSerial();
        g_serial_up = !g_serial_up;
    }
}

void updateExperiment(bool* not_done, Mesh* mesh, Protocol* protocol, float* vel_x,
                      double* elapsed_in_trial, double* trial_duration, double dt)
{
    double T = *trial_duration;
    double t = *elapsed_in_trial;
    
    if (t <= T)
    {
        mesh->translateX((*vel_x) * dt);
        *elapsed_in_trial += dt;
    }
    else if (t <= T + 1)
    {
        *elapsed_in_trial += dt;
        mesh->centerXY(2, -0.02);
        if (g_serial_up)
        {
            triggerSerial();
            g_serial_up = !g_serial_up;
        }
    }
    else
    {
        *vel_x = protocol->nextSpeed();
        float scale_r = protocol->nextSize();
        if ((*vel_x) < 0 || scale_r < 0)
        {
            *not_done = false;
        }
        else
        {
            *trial_duration = SCREEN_WIDTH_GL / (*vel_x);
            *elapsed_in_trial = 0;
            mesh->resetScale();
            mesh->scaleXY(scale_r);
            mesh->centerXY(SCREEN_EDGE_GL, -0.02);
        }
        if (!g_serial_up)
        {
            triggerSerial();
            g_serial_up = !g_serial_up;
        }
    }
    
}

/************ main ************************/

int main(int argc, char** argv)
{
    Protocol* protocol = new Protocol(argv[1]);
    
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
    float screen_aspect_ratio = (float)mode->width / (float)mode->height;
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
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    
    /* NOTE TO SELF: 
     1. rect(-0.34, -0.16, 0.34, 0.09); precisely covers the screen 
     2. when the fish 10 mm away from the screen, the screen covers 203 
        degrees of visual space
     */
    Mesh* background = new Mesh("./boring.vert", "./boring.frag", screen_aspect_ratio);
    background->rect(-0.34, -0.16, 0.34, 0.09);
    background->color(0.0, 0.0, 0.5, 1.0);
    background->translateZ(0.001); // so it is behind the prey
    bufferMesh(background);
    initMeshShaders(background);
    
    Mesh* prey = new Mesh("./boring.vert", "./boring.frag", screen_aspect_ratio);
    prey->circle(1, 0, 0);
    prey->color(0.0, 0.0, 1.0, 1.0);
    bufferMesh(prey);
    initMeshShaders(prey);
    
    bool not_done;
    float vel_x;
    double elapsed_in_trial;
    double trial_duration;
    double total_elasped = 0;
    
    initExperiment(&not_done, prey, protocol, &vel_x,
                   &elapsed_in_trial, &trial_duration);
    
    double prev_sec = glfwGetTime();
    double curr_sec;
    double dt;
    while(not_done && !glfwWindowShouldClose(window))
    {
        curr_sec = glfwGetTime();
        dt = curr_sec - prev_sec;
        prev_sec = curr_sec;
        total_elasped += dt;
        
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        
        drawMesh(background);
        
        if (total_elasped > 0)
        {
            drawMesh(prey);
            updateExperiment(&not_done, prey, protocol, &vel_x,
                             &elapsed_in_trial, &trial_duration, dt);
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    printf("experiment took %f seconds\n", total_elasped);
    
    delete prey;
    delete background;
    delete protocol;
    glfwDestroyWindow(window);
    glfwTerminate();
    //g_serial.close();
    return 0;
}


