
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

#define PI 3.14159265359
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

// record of power for leftward trials
std::vector<float> g_pow0_leftward;
std::vector<float> g_pow1_leftward;

// record of power for rightward trials
std::vector<float> g_pow0_rightward;
std::vector<float> g_pow1_rightward;

// record of power for forward trials
std::vector<float> g_pow0_forward;
std::vector<float> g_pow1_forward;

float g_pow0_threshold;
float g_pow1_threshold;
float g_bias;
float g_scale;

// closed-loop velocities:
//      g_total_vel = g_stim_vel - g_fish_vel
float g_fish_vel = 0;
float g_stim_vel = 0;
float g_total_vel = 0;

std::vector<float> g_fish_vel_record;
std::vector<float> g_stim_vel_record;

// timing and state variables for updating the graphics
double g_dt = 0;
double g_total_elasped = 0;
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

/************* closed-loop functions *********************/

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
    float p0 = std_dev_ring(g_data0);
    float p1 = std_dev_ring(g_data1);
    
    // power is recorded in separate buffers for each type
    // of stimulus
    switch (g_curr_mode) {
        case 0: // rightward stim
            g_pow0_rightward.push_back(p0);
            g_pow1_rightward.push_back(p1);
            break;
        case 1: // leftward stim
            g_pow0_leftward.push_back(p0);
            g_pow1_leftward.push_back(p1);
            break;
        case 2: // forward stim
            g_pow0_forward.push_back(p0);
            g_pow1_forward.push_back(p1);
            break;
        default:
            break;
    }
}

void getPowerThreshold() {
    float th_p0_rightward = mean_vec(g_pow0_rightward) + 2 * std_dev_vec(g_pow0_rightward);
    float th_p1_rightward = mean_vec(g_pow1_rightward) + 2 * std_dev_vec(g_pow1_rightward);
    
    float th_p0_leftward = mean_vec(g_pow0_leftward) + 2 * std_dev_vec(g_pow0_leftward);
    float th_p1_leftward = mean_vec(g_pow1_leftward) + 2 * std_dev_vec(g_pow1_leftward);

    float th_p0_forward = mean_vec(g_pow0_forward) + 2 * std_dev_vec(g_pow0_forward);
    float th_p1_forward = mean_vec(g_pow1_forward) + 2 * std_dev_vec(g_pow1_forward);
    
    g_pow0_threshold = (th_p0_rightward + th_p0_leftward + th_p0_forward) / 3;
    g_pow1_threshold = (th_p1_rightward + th_p1_leftward + th_p1_forward) / 3;
}

unsigned int mymin(unsigned int a, unsigned int b) {
    return (a < b) ? a : b;
}

void thresholdVector(std::vector<float>& data, float threshold) {
    for (unsigned int i = 0; i < data.size(); ++i) {
        data[i] = (data[i] < threshold) ? 0 : data[i];
    }
}

void smoothVector(std::vector<float>& data, std::vector<float>& target, int win) {
    // applies boxcar smoothing to data, storing results in target.
    // the target vector is assumed to be empty (smoothed values are appended to it)
    float smoothing_sum;
    target.push_back(0);
    for (int i = 1; i < (int)data.size(); ++i) {
        smoothing_sum = 0;
        if (i < win) {
            for (int j = 0; j < i; ++j) {
                smoothing_sum += data[j];
            }
            target.push_back(smoothing_sum / (float)win);
        } else {
            for (int j = i - win; j < i; ++j) {
                smoothing_sum += data[j];
            }
            target.push_back(smoothing_sum / (float)win);
        }
    }
}

void getPowerCoeffs(bool saveit) {

    // threshold power
    thresholdVector(g_pow0_rightward, g_pow0_threshold);
    thresholdVector(g_pow0_leftward, g_pow0_threshold);
    thresholdVector(g_pow0_forward, g_pow0_threshold);
    thresholdVector(g_pow1_rightward, g_pow1_threshold);
    thresholdVector(g_pow1_leftward, g_pow1_threshold);
    thresholdVector(g_pow1_forward, g_pow1_threshold);
    
    // get power difference and power sum
    unsigned int i, n;
    std::vector<float> dp_rightward;
    std::vector<float> sp_rightward;
    n = mymin(g_pow0_rightward.size(), g_pow1_rightward.size());
    for (i = 0; i < n; ++i) {
        dp_rightward.push_back(g_pow1_rightward[i] - g_pow0_rightward[i]);
        sp_rightward.push_back(g_pow1_rightward[i] + g_pow0_rightward[i]);
    }
    
    std::vector<float> dp_leftward;
    std::vector<float> sp_leftward;
    n = mymin(g_pow0_leftward.size(), g_pow1_leftward.size());
    for (i = 0; i < n; ++i) {
        dp_leftward.push_back(g_pow1_leftward[i] - g_pow0_leftward[i]);
        sp_leftward.push_back(g_pow1_leftward[i] + g_pow0_leftward[i]);
    }
    
    std::vector<float> dp_forward;
    std::vector<float> sp_forward;
    n = mymin(g_pow0_forward.size(), g_pow1_forward.size());
    for (i = 0; i < n; ++i) {
        dp_forward.push_back(g_pow1_forward[i] - g_pow0_forward[i]);
        sp_forward.push_back(g_pow1_forward[i] + g_pow0_forward[i]);
    }

    // smooth the power sum
    int win = 3; // assuming frame rate of 60 Hz, this is a 50 ms window
    
    std::vector<float> smooth_sp_rightward;
    smoothVector(sp_rightward, smooth_sp_rightward, win);
    
    std::vector<float> smooth_sp_leftward;
    smoothVector(sp_leftward, smooth_sp_leftward, win);
    
    std::vector<float> smooth_sp_forward;
    smoothVector(sp_forward, smooth_sp_forward, win);
    
    // count swim bouts
    int num_bouts_rightward = 0, num_bouts_leftward = 0, num_bouts_forward = 0;
    float th;
    
    th = 0.5 * std_dev_vec(smooth_sp_rightward);
    for (i = 1; i < smooth_sp_rightward.size(); ++i) {
        if (smooth_sp_rightward[i-1] <= th && smooth_sp_rightward[i] > th) {
            num_bouts_rightward++;
        }
    }
    th = 0.5 * std_dev_vec(smooth_sp_leftward);
    for (i = 1; i < smooth_sp_leftward.size(); ++i) {
        if (smooth_sp_leftward[i-1] <= th && smooth_sp_leftward[i] > th) {
            num_bouts_leftward++;
        }
    }
    th = 0.5 * std_dev_vec(smooth_sp_forward);
    for (i = 1; i < smooth_sp_forward.size(); ++i) {
        if (smooth_sp_forward[i-1] <= th && smooth_sp_forward[i] > th) {
            num_bouts_forward++;
        }
    }
    
    // finally find coefficients
    g_bias = std::accumulate(dp_forward.begin(), dp_forward.end(), 0) / num_bouts_forward;
    float c_right = std::accumulate(dp_rightward.begin(), dp_rightward.end(), 0) / num_bouts_rightward;
    float c_left = std::accumulate(dp_leftward.begin(), dp_leftward.end(), 0) / num_bouts_leftward;
    g_scale = 39.0 * 2 / (c_right + c_left);
    
    // save computations if desired
    if (saveit) {
        FILE* file = fopen("POWERrecord.txt", "w");
        
        std::vector<float>::iterator i;
        
        // thresholded power for each stimulus direction
        for (i = g_pow0_rightward.begin(); i != g_pow0_rightward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = g_pow1_rightward.begin(); i != g_pow1_rightward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = g_pow0_leftward.begin(); i != g_pow0_leftward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = g_pow1_leftward.begin(); i != g_pow1_leftward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = g_pow0_forward.begin(); i != g_pow0_forward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = g_pow1_forward.begin(); i != g_pow1_forward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        
        // power sum and power difference for each stimulus direction
        for (i = dp_rightward.begin(); i != dp_rightward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = sp_rightward.begin(); i != sp_rightward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = dp_leftward.begin(); i != dp_leftward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = sp_leftward.begin(); i != sp_leftward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = dp_forward.begin(); i != dp_forward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = sp_forward.begin(); i != sp_forward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        
        // smoothed power sum
        for (i = smooth_sp_rightward.begin(); i != smooth_sp_rightward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = smooth_sp_leftward.begin(); i != smooth_sp_leftward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        for (i = smooth_sp_forward.begin(); i != smooth_sp_forward.end(); ++i) {
            fprintf(file, "%f,", *i);
        }
        fprintf(file, "\n");
        
        // power thresholds
        fprintf(file, "%f,%f\n", g_pow0_threshold, g_pow1_threshold);
        
        // bout counts
        fprintf(file, "%d,%d,%d\n", num_bouts_rightward, num_bouts_leftward, num_bouts_forward);
        
        // bias and scale
        fprintf(file, "%f,%f", g_bias, g_scale);
        
        fclose(file);
    }
}

void getFishVel() {
    // compute power of ring buffers
    float p0 = std_dev_ring(g_data0);
    float p1 = std_dev_ring(g_data1);
    
    // compute fish velocity from power
    p0 = (p0 > g_pow0_threshold) ? p0 : 0;
    p1 = (p1 > g_pow1_threshold) ? p1 : 0;
    
    // correct for forward bias and scale data to degrees / s
    float dp = p1 - p0;
    g_fish_vel = g_scale * (dp - g_bias);
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
        
        getSerialData();
        recordPower();
        
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
    
    double prev_sec = glfwGetTime();
    double curr_sec;
    
    // first game-loop in open-loop
    while (g_not_done && !glfwWindowShouldClose(window)) {
        // game loop
        curr_sec = glfwGetTime();
        g_dt = curr_sec - prev_sec;
        prev_sec = curr_sec;
        g_total_elasped += g_dt;
        
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        
        g_drawFunc();
        
        if (g_total_elasped > 10) {
            g_updateFunc();
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // second game loop in closed-loop
    if (exp_type == CLOSED_LOOP_OMR || exp_type == CLOSED_LOOP_PREY) {
        
        // for the purposes of closed-loop, analyze power
        getPowerThreshold();
        getPowerCoeffs(true); // true to save the analysis, false otherwise
        
        g_not_done = true;
        g_total_elasped = 0;
        g_updateFunc = &updateClosedLoopOMR;
        g_drawFunc = &drawClosedLoopOMR;
        
        printf("beginning closed-loop stimulus\n");
        while (g_not_done && !glfwWindowShouldClose(window)) {
            // game loop
            curr_sec = glfwGetTime();
            g_dt = curr_sec - prev_sec;
            prev_sec = curr_sec;
            g_total_elasped += g_dt;
        
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        
            g_drawFunc();
            getSerialData();
            getFishVel();
            g_updateFunc();
        
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
    
    g_chan.close();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


