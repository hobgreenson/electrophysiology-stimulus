
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <serial/serial.h>
#include <numeric>
#include <boost/circular_buffer.hpp>
#include <cstdio>
#include <cmath>

#include "Vertex2D.h"
#include "load_shader.h"
#include "Mesh.h"
#include "Protocol.h"

#define SCREEN_WIDTH_GL 0.7 //0.68
#define SCREEN_EDGE_GL 0.35

#define OPEN_LOOP_OMR 0
#define OPEN_LOOP_PREY 1
#define CLOSED_LOOP_OMR 2
#define CLOSED_LOOP_PREY 3

/************* globals ***********************/

void (*g_drawFunc)(); // points to the appropriate draw function
void (*g_updateFunc)(); // points to the appropriate update function

// sequences of speeds, sizes and directions for prey and omr experiments
Protocol g_protocol;

// meshes for experiments (sets of vertices, colors and data for drawing shapes.
// constructor arguments indicate which shaders to use with a mesh
Mesh g_background("./rotating_grating.vert", "./boring.frag");
Mesh g_prey("./boring.vert", "./boring.frag");
Mesh g_rotating("./rotating_grating.vert", "./boring.frag");
Mesh g_linear("./linear_grating.vert", "./boring.frag");
//Mesh g_horz("./horzGrating.vert", "./boring.frag");

// serial communication with arduino boards for synchronization and closed loop
serial::Serial g_chan("/dev/ttyACM0", // port ID
                      8 * 115200, // baud rate
                      serial::Timeout::simpleTimeout(1000));
const uint8_t g_msg = 'a';
bool g_serial_up = false;
const uint8_t g_serial_flag = 255;

// closed-loop buffers
int g_buffer_length = 200;
boost::circular_buffer<int> g_data0(g_buffer_length);
boost::circular_buffer<int> g_data1(g_buffer_length);

std::vector<float> g_pow0;
std::vector<float> g_pow1;
float g_pow0_threshold = 0;
float g_pow1_threshold = 0;
float g_pow0_coeff = 0;
float g_pow1_coeff = 0;

// closed-loop velocities:
//      g_total_vel = g_stim_vel - g_fish_vel
float g_fish_vel = 0;
float g_stim_vel = 0;
float g_total_vel = 0;

std::vector<float> g_fish_vel_record;
std::vector<float> g_stim_vel_record;

// timing and state variables for updating the graphics
double g_dt = 0;
double g_elapsed_in_trial = 0;
double g_trial_duration = 0;

int g_curr_mode = -1;
float g_curr_speed = -1;
float g_curr_size = -1;
bool g_not_done = true;

/************ GLFW callbacks ************************/

static void error_callback(int error, const char* description) {
    fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}

/************* closed-loop functions ***************************************/

float mean_vec(std::vector<float>& buffer) {
    
    // computes mean value of a float vector
    
    float sum = std::accumulate(buffer.begin(), buffer.end(), 0);
    return sum / buffer.size();
}

float std_dev_ring(boost::circular_buffer<int>& buffer) {
    
    // computes std. deviation of integer data in a ring buffer
    
    float M_next, M_prev, S_next = 0, S_prev = 0, R_prev, R_next;
    int k = 1;
    boost::circular_buffer<int>::iterator i = buffer.begin();
    
    M_prev = M_next = *i;
    i++;
    for (; i != buffer.end(); ++i) {
        k++;
        R_prev = (float)(*i) - M_prev;
        R_next = (float)(*i) - M_next;
        
        M_next = M_prev + R_prev / k;
        S_next = S_prev + R_prev * R_next;
        M_prev = M_next;
        S_prev = S_next;
    }
    
    return sqrt(S_next / k);
}

float std_dev_vec(std::vector<float>& buffer) {
    
    // computes std. deviation of float data in a vector
    
    float M_next, M_prev, S_next = 0, S_prev = 0, R_prev, R_next;
    int k = 1;
    std::vector<float>::iterator i = buffer.begin();
    
    M_prev = M_next = *i;
    i++;
    for (; i != buffer.end(); ++i) {
        k++;
        R_prev = (*i) - M_prev;
        R_next = (*i) - M_next;
        
        M_next = M_prev + R_prev / k;
        S_next = S_prev + R_prev * R_next;
        M_prev = M_next;
        S_prev = S_next;
    }
    
    return sqrt(S_next / k);
}

void getSerialData() {
    
    // grabs and parses data from the arduino, storing
    // data in two ring buffers (left and right ventral roots)
    
    int ba = g_chan.available();
    uint8_t data[ba];
    g_chan.read(data, ba);
    
    // split data into two channels
    int i = 0, i0 = 0, i1 = 0;
    for (; data[i] != g_serial_flag; ++i) {} // find the first 0xFF flag in the data
    switch (i) {
        case 0:
            // data = [0xFF, l, r, ...]
            i0 = 1;
            i1 = 2;
            break;
        case 1:
            // data = [r, 0xFF, l, ...]
            i0 = 2;
            i1 = 0;
            break;
        case 2:
            // data = [l, r, 0xFF, ...]
            i0 = 0;
            i1 = 1;
            break;
        default:
            break;
    }
    
    // copy left and right data into buffers
    for (; i0 < ba; i0 += 3) {
        g_data0.push_back(data[i0]);
    }
    for (; i1 < ba; i1 += 3) {
        g_data1.push_back(data[i1]);
    }
    
}

void recordPower() {
    g_pow0.push_back(std_dev_ring(g_data0));
    g_pow1.push_back(std_dev_ring(g_data1));
}

void getPowerThreshold() {
    g_pow0_threshold = mean_vec(g_pow0) + 2 * std_dev_vec(g_pow0);
    g_pow1_threshold = mean_vec(g_pow1) + 2 * std_dev_vec(g_pow1);
}

void getPowerCoeffs() {
    // want mean(left) = mean(right) for forward swimming
    
}

void getFishVel() {
    // compute power of ring buffers
    float p0 = std_dev_ring(g_data0);
    float p1 = std_dev_ring(g_data1);
    
    // compute fish velocity from power
    p0 = (p0 > g_pow0_threshold) ? p0 : 0;
    p1 = (p1 > g_pow1_threshold) ? p1 : 0;
    
    g_fish_vel = g_pow0_coeff * p0 - g_pow1_coeff * p1;
}

/************ rendering ************************/

enum ATTRIBUTE_ID {
    VERTEX_POSITION,
    VERTEX_COLOR,
    VERTEX_NORMAL,
    VERTEX_TEXTURE
};

void checkMyGL() {
    GLenum gl_err = glGetError();
    if(gl_err != GL_NO_ERROR) {
        printf("fuck! an error! %d\n", gl_err);
        exit(EXIT_FAILURE);
    }
}

void drawMesh(Mesh* mesh) {
    glBindVertexArray(mesh->vao_);
    glUseProgram(mesh->program_);
    glUniformMatrix4fv(mesh->transform_matrix_location_, 1, GL_FALSE,
                       mesh->transform_matrix_);
    glDrawElements(GL_TRIANGLES, mesh->num_indices_,
                   GL_UNSIGNED_SHORT, (const GLvoid*) 0);
}

void initMeshShaders(Mesh* mesh) {
    GLuint vs = initshaders(GL_VERTEX_SHADER, mesh->vertex_shader_path_);
    GLuint fs = initshaders(GL_FRAGMENT_SHADER, mesh->fragment_shader_path_);
    initprogram(mesh, vs, fs);
    mesh->transform_matrix_location_ = glGetUniformLocation(mesh->program_, "transform_matrix");
}

void bufferMesh(Mesh* mesh) {
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

/***************** Draw functions for specific experiments *****************/

void drawOpenLoopOMR() {
    if (g_curr_mode == 2) {
        drawMesh(&g_linear);
    } else {
        drawMesh(&g_rotating);
    }
}

void drawOpenLoopPrey() {
    drawMesh(&g_background);
    drawMesh(&g_prey);
}

void drawClosedLoopOMR() {
    drawMesh(&g_rotating);
}

/*********** Experiment set-up and update ************************/

void updateOpenLoopPrey() {
    if (g_elapsed_in_trial <= g_trial_duration) {
        // trial is not done yet
        g_prey.translateX(-g_curr_speed * g_dt);
        g_elapsed_in_trial += g_dt;
        
        if (!g_serial_up) {
            g_chan.write(&g_msg, 1);
            g_serial_up = true;
        }
        
    } else if (g_elapsed_in_trial <= g_trial_duration + 10) {
        // inter-trial period (10 s)
        g_elapsed_in_trial += g_dt;
        g_prey.centerXY(2, -0.02); // move mesh off-screen
        
        if (g_serial_up) {
            g_chan.write(&g_msg, 1);
            g_serial_up = !g_serial_up;
        }
        
    } else {
        // start a new trial
        g_curr_speed = g_protocol.nextSpeed();
        g_curr_size = g_protocol.nextSize();
        
        if (g_curr_speed < 0 || g_curr_size < 0) { // end of protocol
            g_not_done = false;
        } else {
            g_trial_duration = SCREEN_WIDTH_GL / g_curr_speed;
            g_elapsed_in_trial = 0;
            g_prey.resetScale();
            g_prey.scaleXY(g_curr_size);
            g_prey.centerXY(SCREEN_EDGE_GL, -0.05);
            
            if (!g_serial_up) {
                g_chan.write(&g_msg, 1);
                g_serial_up = !g_serial_up;
            }
        }
    }
}

void updateOpenLoopOMR() {
    if (g_elapsed_in_trial <= g_trial_duration) {
        // trial is not done yet
        float coeff = (g_curr_mode == 0) ? -1 : 1;
        g_linear.translateXmod(coeff * g_curr_speed * g_dt, SCREEN_WIDTH_GL);
        g_rotating.translateXmod(coeff * g_curr_speed * g_dt, SCREEN_WIDTH_GL);
        
        g_elapsed_in_trial += g_dt;
        
        if (!g_serial_up) {
            g_chan.write(&g_msg, 1);
            g_serial_up = true;
        }
        
    } else if (g_elapsed_in_trial <= g_trial_duration + 10) {
        // inter-trial period (10 s)
        g_elapsed_in_trial += g_dt;
        
        if (g_serial_up) {
            g_chan.write(&g_msg, 1);
            g_serial_up = false;
        }
    } else {
        g_curr_speed = g_protocol.nextSpeed();
        g_curr_mode = g_protocol.nextMode();
        
        if (g_curr_speed < 0 || g_curr_mode < 0) {
            // end of protocol
            g_not_done = false;
        } else {
            // start a new trial
            g_elapsed_in_trial = 0;
            g_chan.write(&g_msg, 1);
            g_serial_up = true;
        }
    }
}

void updateClosedLoopOMR() {
    //g_horz.translateYmod(0.1 * g_dt, 1);
}

void setupExperiment(int type, char* path) {
    switch (type) {
        case OPEN_LOOP_OMR:
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
            
            g_protocol.createOpenLoopStepOMR(path);
            g_curr_speed = g_protocol.nextSpeed();
            g_curr_mode = g_protocol.nextMode();
            g_trial_duration = 10;
            
            g_updateFunc = &updateOpenLoopOMR;
            g_drawFunc = &drawOpenLoopOMR;
            
            break;
            
        case OPEN_LOOP_PREY:
            g_background.rotatingGrating(8);
            g_background.scaleX(SCREEN_EDGE_GL);
            g_background.scaleY(0.3);
            g_background.translateZ(0.001);
            bufferMesh(&g_background);
            initMeshShaders(&g_background);
            
            g_prey.circle(1, 0, 0);
            g_prey.color(0, 0, 0, 255);
            bufferMesh(&g_prey);
            initMeshShaders(&g_prey);
            
            g_protocol.createOpenLoopPrey(path);
            g_curr_speed = g_protocol.nextSpeed();
            g_curr_size = g_protocol.nextSize();
            g_prey.scaleXY(g_curr_size);
            g_prey.centerXY(SCREEN_EDGE_GL, -0.02);
            
            g_updateFunc = &updateOpenLoopPrey;
            g_drawFunc = &drawOpenLoopPrey;
            
            break;
            
        case CLOSED_LOOP_OMR:
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
            
            g_protocol.createShortOpenLoopStepOMR(path);
            g_curr_speed = g_protocol.nextSpeed();
            g_curr_mode = g_protocol.nextMode();
            g_trial_duration = 10;
            
            g_updateFunc = &updateOpenLoopOMR;
            g_drawFunc = &drawOpenLoopOMR;

            break;
            
        case CLOSED_LOOP_PREY:
            break;
            
        default:
            printf("Unrecognized experiment type!\n");
            exit(EXIT_FAILURE);
            break;
    }
}

/************ main ************************/

int main(int argc, char** argv) {
    
    // GLFW set up
    GLFWwindow* window;
    glfwSetErrorCallback(error_callback);
    
    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }
    
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    window = glfwCreateWindow(mode->width, mode->height,
                              "Hello game!", primary, NULL);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    
    glfwMakeContextCurrent(window);

    // GLEW set up
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
    }
    
    glfwSetKeyCallback(window, key_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    
    // start an experiment
    int exp_type = atoi(argv[1]);
    setupExperiment(exp_type, argv[2]);
    
    double total_elasped = 0;
    double prev_sec = glfwGetTime();
    double curr_sec;
    
    // first game-loop in open-loop
    while(g_not_done && !glfwWindowShouldClose(window)) {
        // game loop
        curr_sec = glfwGetTime();
        g_dt = curr_sec - prev_sec;
        prev_sec = curr_sec;
        total_elasped += g_dt;
        
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        
        g_drawFunc();
        getSerialData();
        
        if (total_elasped > 10) {
            recordPower();
            g_updateFunc();
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // second game loop in closed-loop
    if (exp_type == CLOSED_LOOP_OMR || exp_type == CLOSED_LOOP_PREY) {
        
        g_not_done = true;
        getPowerThreshold();
        getPowerCoeffs();
        g_updateFunc = &updateClosedLoopOMR;
        g_drawFunc = &drawClosedLoopOMR;
    
        while(g_not_done && !glfwWindowShouldClose(window)) {
            // game loop
            curr_sec = glfwGetTime();
            g_dt = curr_sec - prev_sec;
            prev_sec = curr_sec;
            total_elasped += g_dt;
        
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        
            g_drawFunc();
            getSerialData();
            getFishVel();
            g_updateFunc();
        
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
    
    printf("experiment took %f seconds\n", total_elasped);
    
    g_chan.close();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


