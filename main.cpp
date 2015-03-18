ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#include <OpenGL/glext.h>
#include <GLFW/glfw3.h>
#else
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#endif

#include <serial/serial.h>
#include "Vertex2D.h"
#include "load_shader.h"
#include "Mesh.h"
#include "Protocol.h"
#include <cstdio>

#define SCREEN_WIDTH_GL 0.68
#define SCREEN_EDGE_GL 0.35
#define DRIFTING_GRATING 0
#define PREY 1

/************* globals ***********************/
int g_exp_type;
Protocol g_prey_protocol(PREY);
Protocol g_drifting_protocol(DRIFTING_GRATING);

Mesh g_background("./boring.vert", "./boring.frag");
Mesh g_prey("./boring.vert", "./boring.frag");
Mesh g_rotating("./rotating_grating.vert", "./boring.frag");
Mesh g_linear("./linear_grating.vert", "./boring.frag");

serial::Serial g_chan("/dev/tty.usbmodem1421", // port ID
                      4 * 115200, // baud rate
                      serial::Timeout::simpleTimeout(1000));

const uint8_t g_msg = 'a';
bool g_serial_up = false;

double g_dt = 0;
double g_elapsed_in_trial = 0;
int g_curr_mode = -1;
float g_curr_speed = -1;
float g_curr_size = -1;
double g_trial_duration = 0;
bool g_not_done = true;

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
    glBindVertexArray(mesh->vao_);
    glUseProgram(mesh->program_);
    glUniformMatrix4fv(mesh->transform_matrix_location_, 1, GL_FALSE,
                       mesh->transform_matrix_);
    glDrawElements(GL_TRIANGLES, mesh->num_indices_,
                   GL_UNSIGNED_SHORT, (const GLvoid*) 0);
}

void initMeshShaders(Mesh* mesh)
{
    GLuint vs = initshaders(GL_VERTEX_SHADER, mesh->vertex_shader_path_);
    GLuint fs = initshaders(GL_FRAGMENT_SHADER, mesh->fragment_shader_path_);
    initprogram(mesh, vs, fs);
    mesh->transform_matrix_location_ = glGetUniformLocation(mesh->program_, "transform_matrix");
}

void bufferMesh(Mesh* mesh)
{
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
    
    glEnableVertexAttribArray(VERTEX_COLOR);
    glVertexAttribPointer(VERTEX_COLOR, 4, GL_UNSIGNED_BYTE, GL_FALSE,
                          sizeof(Vertex2D),
                          (const GLvoid*) offsetof(Vertex2D, color));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

/*********** logic ************************/

void update()
{
    switch (g_exp_type)
    {
        case PREY:
        {
            if (g_elapsed_in_trial <= g_trial_duration) // trial is not done yet
            {
                g_prey.translateX(g_curr_speed * g_dt);
                g_elapsed_in_trial += g_dt;
                if (!g_serial_up)
                {
                    g_chan.write(&g_msg, 1);
                    g_serial_up = true;
                }
            }
            else if (g_elapsed_in_trial <= 2 * g_trial_duration) // inter-trial period
            {
                g_elapsed_in_trial += g_dt;
                g_prey.centerXY(2, -0.02); // move mesh off-screen
                if (g_serial_up)
                {
                    g_chan.write(&g_msg, 1);
                    g_serial_up = !g_serial_up;
                }
            }
            else // start a new trial
            {
                g_curr_speed = g_prey_protocol.nextSpeed();
                g_curr_size = g_prey_protocol.nextSize();
                if (g_curr_speed < 0 || g_curr_size < 0) // end of protocol
                    g_not_done = false;
                else
                {
                    g_trial_duration = SCREEN_WIDTH_GL / g_curr_speed;
                    g_elapsed_in_trial = 0;
                    g_prey.resetScale();
                    g_prey.scaleXY(g_curr_size);
                    g_prey.centerXY(-SCREEN_EDGE_GL, -0.02);
                    if (!g_serial_up)
                    {
                        g_chan.write(&g_msg, 1);
                        g_serial_up = !g_serial_up;
                    }
                }
            }
            break;
        }
        case DRIFTING_GRATING:
        {
            if (g_elapsed_in_trial <= g_trial_duration) // trial is not done yet
            {
                float coeff = g_curr_mode == 0 ? -1 : 1;
                g_linear.translateXmod(coeff * g_curr_speed * g_dt, SCREEN_WIDTH_GL);
                g_rotating.translateXmod(coeff * g_curr_speed * g_dt, SCREEN_WIDTH_GL);
                g_elapsed_in_trial += g_dt;
                
                if (!g_serial_up)
                {
                    g_chan.write(&g_msg, 1);
                    g_serial_up = true;
                }
            }
            else if (g_elapsed_in_trial <= 2 * g_trial_duration) // inter-trial period
            {
                g_elapsed_in_trial += g_dt;
                if (g_serial_up)
                {
                    g_chan.write(&g_msg, 1);
                    g_serial_up = false;
                }
            }
            else // start a new trial
            {
                g_curr_speed = g_drifting_protocol.nextSpeed();
                g_curr_mode = g_drifting_protocol.nextMode();
                if (g_curr_speed < 0 || g_curr_mode < 0) // end of protocol
                    g_not_done = false;
                else
                {
                    g_elapsed_in_trial = 0;
                    g_chan.write(&g_msg, 1);
                    g_serial_up = true;
                }
            }
            break;
        }
        default:
            break;
    }
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
    //float screen_aspect_ratio = (float)mode->width / (float)mode->height;
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
    
    /* This sets up the prey experiment */
    g_background.rect(-SCREEN_EDGE_GL, -0.16, SCREEN_EDGE_GL, 0.09);
    g_background.color(0, 0, 100, 255);
    g_background.translateZ(0.001); // so it is behind the prey
    bufferMesh(&g_background);
    initMeshShaders(&g_background);
    
    g_prey.circle(1, 0, 0);
    g_prey.color(0.0, 0.0, 255, 255);
    bufferMesh(&g_prey);
    initMeshShaders(&g_prey);
    
    /* This sets up the drifting grating experiment */
    g_rotating.rotatingGrating(8);
    g_rotating.scaleX(SCREEN_EDGE_GL);
    g_rotating.scaleY(0.3);
    bufferMesh(&g_rotating);
    initMeshShaders(&g_rotating);
    
    g_linear.linearGrating(8);
    g_linear.scaleX(SCREEN_EDGE_GL);
    g_linear.scaleY(0.3);
    bufferMesh(&g_linear);
    initMeshShaders(&g_linear);
   
    /* choose an experiment */
    g_exp_type = atoi(argv[1]);
    if (g_exp_type == PREY)
    {
        g_prey_protocol.save(argv[2]);
        g_curr_speed = g_prey_protocol.nextSpeed();
        g_curr_size = g_prey_protocol.nextSize();
        g_prey.scaleXY(g_curr_size);
        g_prey.centerXY(SCREEN_EDGE_GL, -0.02);
    }
    else
    {
        g_drifting_protocol.save(argv[2]);
        g_curr_speed = g_drifting_protocol.nextSpeed();
        g_curr_mode = g_drifting_protocol.nextMode();
        g_trial_duration = 10;
    }
    
    /* game loop */
    double total_elasped = 0;
    double prev_sec = glfwGetTime();
    double curr_sec;
    while(g_not_done && !glfwWindowShouldClose(window))
    {
        curr_sec = glfwGetTime();
        g_dt = curr_sec - prev_sec;
        prev_sec = curr_sec;
        total_elasped += g_dt;
        
        //printf("g_dt = %f\n", g_dt);
        
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        
        if (g_exp_type == PREY)
        {
            /* draws prey experiment */
            drawMesh(&g_background);
            drawMesh(&g_prey);
        }
        else
        {
            /* draws dirfting grating experiment */
            if (g_curr_mode == 2)
                drawMesh(&g_linear);
            else
                drawMesh(&g_rotating);
        }

        if (total_elasped > 10) 
            update();
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    printf("experiment took %f seconds\n", total_elasped);
    
    g_chan.close();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


