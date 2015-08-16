
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <serial/serial.h>
#include <numeric>
#include <boost/circular_buffer.hpp>
#include <cstdio>
#include <cmath>
#include <cstring>

#include "Vertex2D.h"
#include "load_shader.h"
#include "Mesh.h"
#include "Protocol.h"

#define PI 3.14159265359
#define SCREEN_WIDTH_GL 0.7
#define SCREEN_WIDTH_DEG 200
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
Protocol g_calibration_protocol; // open-loop calibration for closed loop

// meshes for experiments (sets of vertices, colors and data for drawing shapes.
// constructor arguments indicate which shaders to use with a mesh
Mesh g_background("./rotating_grating.vert", "./boring.frag");
Mesh g_prey("./boring.vert", "./boring.frag");
Mesh g_rotating("./rotating_grating.vert", "./boring.frag");
Mesh g_linear("./linear_grating.vert", "./boring.frag");
//Mesh g_horz("./horzGrating.vert", "./boring.frag");

// serial communication with arduino boards for synchronization and closed loop
serial::Serial g_chan("/dev/ttyACM1", // closed-loop port
                      8 * 115200, // baud rate
                      serial::Timeout::simpleTimeout(1000));
serial::Serial g_sync_chan("/dev/ttyACM0", // synchonization port
                           4 * 115200, // baud rate
                           serial::Timeout::simpleTimeout(1000));
const uint8_t g_msg = 'a';
bool g_serial_up = false;
const uint8_t g_serial_flag = 255;

// Open-loop buffers
// record of power for leftward trials
std::vector<float> g_data0_leftward; // raw data
std::vector<float> g_data1_leftward; // raw data

// record of power for rightward trials
std::vector<float> g_data0_rightward; // raw data
std::vector<float> g_data1_rightward; // raw data

// record of power for forward trials
std::vector<float> g_data0_forward; // raw data
std::vector<float> g_data1_forward; // raw data

// thresholding and scaling coefficients for power
float g_pow0_threshold;
float g_pow1_threshold;
float g_bias;
float g_scale;

// Closed-loop buffers
int g_buffer_length = 200; // approx 10 ms worth of samples
float g_raw_std_0 = 1; // std. dev. of raw data0
float g_raw_mean_0 = 0; // mean of raw data0
float g_raw_std_1 = 1; // std. dev. of raw data1
float g_raw_mean_1 = 0; // mean of raw data1
boost::circular_buffer<float> g_data0_ring(g_buffer_length); // raw data
boost::circular_buffer<float> g_data1_ring(g_buffer_length); // raw data

// closed-loop velocities:
//      g_total_vel = g_stim_vel - g_fish_vel
float g_fish_vel = 0;
float g_stim_vel = 0;
float g_total_vel = 0;
float g_pow0_cl = 0;
float g_pow1_cl = 0;
std::vector<float> g_fish_vel_record; // to save
std::vector<float> g_stim_vel_record; // to save
std::vector<float> g_total_vel_record; // to save
//std::vector<float> g_pow0_cl_record; // to save
//std::vector<float> g_pow1_cl_record; // to save
//std::vector<float> g_raw0_cl_record; // to save
//std::vector<float> g_raw1_cl_record; // to save

// timing and state variables for updating the graphics
double g_dt = 0;
double g_total_elasped = 0;
double g_elapsed_in_trial = 0;
double g_trial_duration = 0;

int g_curr_mode = -1;
float g_curr_frequency = -1;
float g_curr_speed = -1;
float g_curr_size = -1;
float g_curr_gain = -1;
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
    
    float sum = std::accumulate(buffer.begin(), buffer.end(), 0.0);
    return sum / buffer.size();
}

float std_dev_vec(std::vector<float>& buffer) {
    
    // computes std. deviation of float data in a vector
    
    float mean = mean_vec(buffer);
    std::vector<float>::iterator i = buffer.begin();
    float sum = 0;
    for (; i != buffer.end(); ++i) {
        sum += ((*i) - mean) * ((*i) - mean);
    }
    
    return sqrt(sum / (buffer.size() - 1));
}

float mean_ring(boost::circular_buffer<float>& buffer) {
    
    // computes mean value of a float ring buffer
    
    float sum = std::accumulate(buffer.begin(), buffer.end(), 0.0);
    return sum / buffer.size();
}

float std_dev_ring(boost::circular_buffer<float>& buffer) {
    
    // computes std. deviation of data in a ring buffer
    
    float mean = mean_ring(buffer);
    boost::circular_buffer<float>::iterator i = buffer.begin();
    float sum = 0;
    for (; i != buffer.end(); ++i) {
        sum += ((*i) - mean) * ((*i) - mean);
    }
    
    return sqrt(sum / (buffer.size() - 1));
}

void getSerialDataOpenLoop() {
    // grabs and parses data from the arduino, storing
    // data in two vectors (left and right ventral roots)
    // for the appropriate stimulus type
    
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
    
    // put left and right data into vectors
    switch (g_curr_mode) {
        case 0:
            for (; i0 < ba; i0 += 3) {
                g_data0_rightward.push_back(data[i0]);
            }
            for (; i1 < ba; i1 += 3) {
                g_data1_rightward.push_back(data[i1]);
            }
            break;
            
        case 1:
            for (; i0 < ba; i0 += 3) {
                g_data0_leftward.push_back(data[i0]);
            }
            for (; i1 < ba; i1 += 3) {
                g_data1_leftward.push_back(data[i1]);
            }
            break;
            
        case 2:
            for (; i0 < ba; i0 += 3) {
                g_data0_forward.push_back(data[i0]);
            }
            for (; i1 < ba; i1 += 3) {
                g_data1_forward.push_back(data[i1]);
            }
            break;
        
        default:
            break;
    }
}

void getSerialDataClosedLoop() {
    
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
    
    // copy left and right data into ring buffers after scaling
    for (; i0 < ba; i0 += 3) {
        //g_raw0_cl_record.push_back(data[i0]);
        g_data0_ring.push_back(((float)data[i0] - g_raw_mean_0) / g_raw_std_0);
    }
    for (; i1 < ba; i1 += 3) {
        //g_raw1_cl_record.push_back(data[i1]);
        g_data1_ring.push_back(((float)data[i1] - g_raw_mean_1) / g_raw_std_1);
    }
}

void recordVelocity() {
    g_stim_vel_record.push_back(g_stim_vel);
    g_fish_vel_record.push_back(g_fish_vel);
    g_total_vel_record.push_back(g_total_vel);
}

void writeVec(FILE* file, std::vector<float>& x) {
    std::vector<float>::iterator i;
    int j = 0;
    for (i = x.begin(); i != x.end(); ++i) {
        if (j == 0) {
            fprintf(file, "%f", *i);
            j = 1;
        } else {
            fprintf(file, ",%f", *i);
        }
    }
}

void saveVelocity(char* fileid) {
    char path[100];
    strcpy(path, fileid);
    strcat(path, "_closed_loop_velocity.txt");
    FILE* file = fopen(path, "w");
    
    writeVec(file, g_stim_vel_record);
    fprintf(file, "\n");
    writeVec(file, g_fish_vel_record);
    fprintf(file, "\n");
    writeVec(file, g_total_vel_record);
    //fprintf(file, "\n");
    
    //writeVec(file, g_pow0_cl_record);
    //fprintf(file, "\n");
    //writeVec(file, g_pow1_cl_record);
    //fprintf(file, "\n");
    
    //writeVec(file, g_raw0_cl_record);
    //fprintf(file, "\n");
    //writeVec(file, g_raw1_cl_record);
    //fprintf(file, "\n");
    
    //fprintf(file, "%f,%f,%f,%f", g_raw_mean_0, g_raw_std_0, g_raw_mean_1, g_raw_std_1);
    fclose(file);
}

unsigned int mymin(unsigned int a, unsigned int b) {
    return (a < b) ? a : b;
}

void thresholdVector(std::vector<float>& data, float threshold) {
    for (unsigned int i = 0; i < data.size(); ++i) {
        data[i] = (data[i] < threshold) ? 0 : data[i];
    }
}

void normalizeVector(std::vector<float>& x, float& mean, float& std) {
    mean = mean_vec(x);
    std = std_dev_vec(x);
    std::vector<float>::iterator i;
    for (i = x.begin(); i != x.end(); ++i) {
        *i = ((*i) - mean) / std;
    }
}

void openLoopPower(std::vector<float>& x, std::vector<float>& p) {
    boost::circular_buffer<float> win(g_buffer_length);
    std::vector<float>::iterator i;
    int j = 0;
    for (i = x.begin(); i != x.end(); ++i) {
        win.push_back(*i);
        if (j < 200) {
            p.push_back(0.0);
        } else {
            p.push_back(std_dev_ring(win));
        }
        j++;
    }
}

float getScale(std::vector<float>& left, std::vector<float>& right) {
    std::vector<float>::iterator i;
    float time_swimming_r = 0.0;
    for (i = right.begin(); i != right.end(); ++i) {
        float val = fabs(*i);
        if (val > 0) {
            time_swimming_r++;
        }
    }
    float time_swimming_l = 0.0;
    for (i = left.begin(); i != left.end(); ++i) {
        float val = fabs(*i);
        if (val > 0) {
            time_swimming_l++;
        }
    }
    float sr = std::accumulate(right.begin(), right.end(), 0.0);
    float sl = std::accumulate(left.begin(), left.end(), 0.0);
    return 40 * (time_swimming_l + time_swimming_r) / (fabs(sl) + fabs(sr));
}

void prepareForClosedLoop(char* fileid, bool saveit) {
    
    // first scale and de-mean raw data
    float rightward_m_0, rightward_s_0,
          rightward_m_1, rightward_s_1;
    normalizeVector(g_data0_rightward, rightward_m_0, rightward_s_0);
    normalizeVector(g_data1_rightward, rightward_m_1, rightward_s_1);
    
    float leftward_m_0, leftward_s_0,
          leftward_m_1, leftward_s_1;
    normalizeVector(g_data0_leftward, leftward_m_0, leftward_s_0);
    normalizeVector(g_data1_leftward, leftward_m_1, leftward_s_1);
    
    float forward_m_0, forward_s_0,
          forward_m_1, forward_s_1;
    normalizeVector(g_data0_forward, forward_m_0, forward_s_0);
    normalizeVector(g_data1_forward, forward_m_1, forward_s_1);
    
    // set global mean and std for raw data - used in closed loop
    g_raw_mean_0 = (leftward_m_0 + rightward_m_0 + forward_m_0) / 3;
    g_raw_std_0 = (leftward_s_0 + rightward_s_0 + forward_s_0) / 3;
    g_raw_mean_1 = (leftward_m_1 + rightward_m_1 + forward_m_1) / 3;
    g_raw_std_1 = (leftward_s_1 + rightward_s_1 + forward_s_1) / 3;
    
    // now compute power of open-loop raw data
    std::vector<float> pow0_rightward, pow1_rightward,
                       pow0_leftward, pow1_leftward,
                       pow0_forward, pow1_forward;
    openLoopPower(g_data0_rightward, pow0_rightward);
    openLoopPower(g_data1_rightward, pow1_rightward);
    openLoopPower(g_data0_leftward, pow0_leftward);
    openLoopPower(g_data1_leftward, pow1_leftward);
    openLoopPower(g_data0_forward, pow0_forward);
    openLoopPower(g_data1_forward, pow1_forward);
    
    // compute power threshold
    float th_p0_rightward = mean_vec(pow0_rightward) + 2 * std_dev_vec(pow0_rightward);
    float th_p1_rightward = mean_vec(pow1_rightward) + 2 * std_dev_vec(pow1_rightward);
    
    float th_p0_leftward = mean_vec(pow0_leftward) + 2 * std_dev_vec(pow0_leftward);
    float th_p1_leftward = mean_vec(pow1_leftward) + 2 * std_dev_vec(pow1_leftward);
    
    float th_p0_forward = mean_vec(pow0_forward) + 2 * std_dev_vec(pow0_forward);
    float th_p1_forward = mean_vec(pow1_forward) + 2 * std_dev_vec(pow1_forward);
    
    g_pow0_threshold = (th_p0_rightward + th_p0_leftward + th_p0_forward) / 3;
    g_pow1_threshold = (th_p1_rightward + th_p1_leftward + th_p1_forward) / 3;
    
    printf("th_p0_rightward = %f\n", th_p0_rightward);
    printf("th_p1_rightward = %f\n", th_p1_rightward);
    printf("th_p0_leftward = %f\n", th_p0_leftward);
    printf("th_p1_leftward = %f\n", th_p1_leftward);
    printf("th_p0_forward = %f\n", th_p0_forward);
    printf("th_p1_forward = %f\n", th_p1_forward);
    
    // threshold power
    thresholdVector(pow0_rightward, g_pow0_threshold);
    thresholdVector(pow0_leftward, g_pow0_threshold);
    thresholdVector(pow0_forward, g_pow0_threshold);
    thresholdVector(pow1_rightward, g_pow1_threshold);
    thresholdVector(pow1_leftward, g_pow1_threshold);
    thresholdVector(pow1_forward, g_pow1_threshold);
    
    // get left-right bias coeff
    float mp0 = mean_vec(pow0_forward);
    float mp1 = mean_vec(pow1_forward);
    g_bias = mp1 / mp0;
    
    // get power difference with bias correction
    unsigned int i, n;
    std::vector<float> dp_rightward;
    n = mymin(pow0_rightward.size(), pow1_rightward.size());
    for (i = 0; i < n; ++i) {
        dp_rightward.push_back(pow1_rightward[i] - g_bias * pow0_rightward[i]);
    }
    std::vector<float> dp_leftward;
    n = mymin(pow0_leftward.size(), pow1_leftward.size());
    for (i = 0; i < n; ++i) {
        dp_leftward.push_back(pow1_leftward[i] - g_bias * pow0_leftward[i]);
    }
    std::vector<float> dp_forward;
    n = mymin(pow0_forward.size(), pow1_forward.size());
    for (i = 0; i < n; ++i) {
        dp_forward.push_back(pow1_forward[i] - g_bias * pow0_forward[i]);
    }
    
    // find scale to metric
    g_scale = getScale(dp_leftward, dp_rightward);
    
    // save data if desired
    if (saveit) {
        char path[100];
        strcpy(path, fileid);
        strcat(path, "_calibration_data.txt");
        FILE* file = fopen(path, "w");
        
        // raw data
        writeVec(file, g_data0_rightward);
        fprintf(file, "\n");
        writeVec(file, g_data1_rightward);
        fprintf(file, "\n");
        writeVec(file, g_data0_leftward);
        fprintf(file, "\n");
        writeVec(file, g_data1_leftward);
        fprintf(file, "\n");
        writeVec(file, g_data0_forward);
        fprintf(file, "\n");
        writeVec(file, g_data1_forward);
        fprintf(file, "\n");
        
        // power
        writeVec(file, pow0_rightward);
        fprintf(file, "\n");
        writeVec(file, pow1_rightward);
        fprintf(file, "\n");
        writeVec(file, pow0_leftward);
        fprintf(file, "\n");
        writeVec(file, pow1_leftward);
        fprintf(file, "\n");
        writeVec(file, pow0_forward);
        fprintf(file, "\n");
        writeVec(file, pow1_forward);
        fprintf(file, "\n");
        
        // power difference
        writeVec(file, dp_rightward);
        fprintf(file, "\n");
        writeVec(file, dp_leftward);
        fprintf(file, "\n");
        writeVec(file, dp_forward);
        fprintf(file, "\n");
    
        // power thresholds
        fprintf(file, "%f,%f,%f,%f,%f,%f\n", th_p0_rightward, th_p1_rightward,
                                             th_p0_leftward, th_p1_leftward,
                                             th_p0_forward, th_p1_forward);
        fprintf(file, "%f,%f\n", g_pow0_threshold, g_pow1_threshold);
        
        // bias and scale
        fprintf(file, "%f,%f", g_bias, g_scale);
        
        fclose(file);
    }
}

void getFishVel() {
    // compute power of ring buffers
    g_pow0_cl = std_dev_ring(g_data0_ring);
    g_pow1_cl = std_dev_ring(g_data1_ring);
    
    // debug
    //g_pow0_cl_record.push_back(g_pow0_cl);
    //g_pow1_cl_record.push_back(g_pow1_cl);
    
    // threshold power
    g_pow0_cl = (g_pow0_cl > g_pow0_threshold) ? g_pow0_cl : 0;
    g_pow1_cl = (g_pow1_cl > g_pow1_threshold) ? g_pow1_cl : 0;
    
    // correct for forward bias and scale to degrees / s
    float dp = g_pow1_cl - g_bias * g_pow0_cl;
    g_fish_vel = g_scale * dp;
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
float velToGL(float vel) {
    return SCREEN_WIDTH_GL * (vel / SCREEN_WIDTH_DEG);
}

void updateOpenLoopPrey() {
    if (g_elapsed_in_trial <= g_trial_duration) {
        
        // trial is not done yet
        g_prey.translateX(-g_curr_speed * g_dt);
        g_elapsed_in_trial += g_dt;
        
        if (!g_serial_up) {
            g_sync_chan.write(&g_msg, 1);
            g_serial_up = true;
        }
        
    } else if (g_elapsed_in_trial <= g_trial_duration + 10) {
        // inter-trial period (10 s)
        g_elapsed_in_trial += g_dt;
        g_prey.centerXY(2, -0.02); // move mesh off-screen
        
        if (g_serial_up) {
            g_sync_chan.write(&g_msg, 1);
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
                g_sync_chan.write(&g_msg, 1);
                g_serial_up = !g_serial_up;
            }
        }
    }
}

void updateOpenLoopStepOMR() {
    if (g_elapsed_in_trial <= g_trial_duration) {
        
        // trial is not done yet
        float coeff = (g_curr_mode == 0) ? -1 : 1;
        g_linear.translateXmod(coeff * velToGL(g_curr_speed) * g_dt, SCREEN_WIDTH_GL);
        g_rotating.translateXmod(coeff * velToGL(g_curr_speed) * g_dt, SCREEN_WIDTH_GL);
        g_elapsed_in_trial += g_dt;
        
        if (!g_serial_up) {
            g_sync_chan.write(&g_msg, 1);
            g_serial_up = true;
        }
        
    } else if (g_elapsed_in_trial <= g_trial_duration + 10) {
        
        // inter-trial period (10 s)
        g_elapsed_in_trial += g_dt;
        
        if (g_serial_up) {
            g_sync_chan.write(&g_msg, 1);
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
            g_sync_chan.write(&g_msg, 1);
            g_serial_up = true;
        }
    }
}

void updateClosedLoopStepOMR() {
    if (g_elapsed_in_trial <= g_trial_duration) {
        
        // trial is not done yet
        float coeff = (g_curr_mode == 0) ? -1 : 1;
        
        getSerialDataClosedLoop();
        getFishVel();
        
        g_stim_vel = coeff * g_curr_speed;
        g_total_vel = g_stim_vel - (g_curr_gain * g_fish_vel);
        
        recordVelocity();
        
        g_rotating.translateXmod(velToGL(g_total_vel) * g_dt, SCREEN_WIDTH_GL);
        g_elapsed_in_trial += g_dt;
        
        if (!g_serial_up) {
            g_sync_chan.write(&g_msg, 1);
            g_serial_up = true;
        }
        
    } else if (g_elapsed_in_trial <= g_trial_duration + 10) {
        
        // inter-trial period (10 s)
        g_elapsed_in_trial += g_dt;
        
        g_stim_vel = 0;
        g_fish_vel = 0;
        g_total_vel = 0;
        recordVelocity();
        
        if (g_serial_up) {
            g_sync_chan.write(&g_msg, 1);
            g_serial_up = false;
        }
        
    } else {
        
        g_curr_speed = g_protocol.nextSpeed();
        g_curr_gain = g_protocol.nextGain();
        g_curr_mode = g_protocol.nextMode();
        recordVelocity();
        
        if (g_curr_speed < 0 || g_curr_mode < 0) {
            // end of protocol
            g_not_done = false;
            
        } else {
            // start a new trial
            g_elapsed_in_trial = 0;
            g_sync_chan.write(&g_msg, 1);
            g_serial_up = true;
        }
    }
}

void updateCalibrationStepOMR() {
    if (g_elapsed_in_trial <= g_trial_duration) {
        
        // trial is not done yet
        float coeff = (g_curr_mode == 0) ? -1 : 1;
        g_linear.translateXmod(coeff * velToGL(g_curr_speed) * g_dt, SCREEN_WIDTH_GL);
        g_rotating.translateXmod(coeff * velToGL(g_curr_speed) * g_dt, SCREEN_WIDTH_GL);
        g_elapsed_in_trial += g_dt;
        
        if (!g_serial_up) {
            g_sync_chan.write(&g_msg, 1);
            g_serial_up = true;
        }
        
        getSerialDataOpenLoop();
        
    } else if (g_elapsed_in_trial <= g_trial_duration + 10) {
        
        // inter-trial period (10 s)
        g_elapsed_in_trial += g_dt;
        
        if (g_serial_up) {
            g_sync_chan.write(&g_msg, 1);
            g_serial_up = false;
        }
        
    } else {
        
        g_curr_speed = g_calibration_protocol.nextSpeed();
        g_curr_mode = g_calibration_protocol.nextMode();
        
        if (g_curr_speed < 0 || g_curr_mode < 0) {
            // end of protocol
            g_not_done = false;
        } else {
            // start a new trial
            g_elapsed_in_trial = 0;
            g_sync_chan.write(&g_msg, 1);
            g_serial_up = true;
        }
    }
}

void setupExperiment(int type, char* fileid) {
    switch (type) {
        case OPEN_LOOP_OMR:
        {
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
            
            char path[100];
            strcpy(path, fileid);
            strcat(path, "_openloop.txt");
            g_protocol.createOpenLoopStepOMR(true, path);
            
            g_curr_speed = g_protocol.nextSpeed();
            g_curr_mode = g_protocol.nextMode();
            g_trial_duration = 10;
            
            g_updateFunc = &updateOpenLoopStepOMR;
            g_drawFunc = &drawOpenLoopOMR;
            
            break;
        }
        case OPEN_LOOP_PREY:
        {
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
            
            char path[100];
            strcpy(path, fileid);
            strcat(path, "_openloop.txt");
            g_protocol.createOpenLoopPrey(true, path);
            
            g_curr_speed = g_protocol.nextSpeed();
            g_curr_size = g_protocol.nextSize();
            g_prey.scaleXY(g_curr_size);
            g_prey.centerXY(SCREEN_EDGE_GL, -0.02);
            
            g_updateFunc = &updateOpenLoopPrey;
            g_drawFunc = &drawOpenLoopPrey;
            
            break;
        }
        case CLOSED_LOOP_OMR:
        {
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
            
            
            char path1[100];
            strcpy(path1, fileid);
            strcat(path1, "_openloop.txt");
            g_calibration_protocol.createOpenLoopStepOMR(true, path1);
            
            char path2[100];
            strcpy(path2, fileid);
            strcat(path2, "_closedloop.txt");
            g_protocol.createClosedLoopStepOMR(true, path2);
            
            g_curr_speed = g_calibration_protocol.nextSpeed();
            g_curr_mode = g_calibration_protocol.nextMode();
            g_trial_duration = 10;
            
            g_updateFunc = &updateCalibrationStepOMR;
            g_drawFunc = &drawOpenLoopOMR;

            break;
        }
        case CLOSED_LOOP_PREY:
            break;
            
        default:
        {
            printf("Unrecognized experiment type!\n");
            exit(EXIT_FAILURE);
            break;
        }
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
    
    printf("done with open-loop\n");
    
    // second game loop in closed-loop
    if (exp_type == CLOSED_LOOP_OMR || exp_type == CLOSED_LOOP_PREY) {
        
        // set up closed-loop
        prepareForClosedLoop(argv[2], true);
        g_chan.flush();
        g_not_done = true;
        g_total_elasped = 0;
        g_updateFunc = &updateClosedLoopStepOMR;
        g_drawFunc = &drawClosedLoopOMR;
        
        while (g_not_done && !glfwWindowShouldClose(window)) {
            // game loop
            curr_sec = glfwGetTime();
            g_dt = curr_sec - prev_sec;
            prev_sec = curr_sec;
            g_total_elasped += g_dt;
        
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        
            g_drawFunc();
            g_updateFunc();
        
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
    
    printf("saving velocity...\n");
    saveVelocity(argv[2]);
    printf("we're done here!\n");
    
    g_chan.close();
    g_sync_chan.close();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


