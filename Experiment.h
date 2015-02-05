
#ifndef EXPERIMENT_H
#define EXPERIMENT_H
#include "Protocol.h"
#include "Mesh.h"
#include <serial/serial.h>

#define SCREEN_WIDTH_GL 0.68
#define SCREEN_EDGE_GL -0.34
#define DRIFTING_GRATING 0
#define PREY 1

class Experiment 
{
public:
    Experiment(int type, char* path, Mesh* mesh);
    Experiment(int type, char* path, Mesh* mesh1, Mesh* mesh2);
    ~Experiment();
    void update(double dt);
    void triggerSerial();

    int type_;
    bool not_done_;
    
    Protocol* p_;
    Mesh* mesh_;
    Mesh* mesh1_;
    Mesh* mesh2_;
    serial::Serial* serial_chan_;
    
    bool serial_up_;
    int curr_mode_;
    float curr_speed_;
    float curr_size_;
    double elapsed_in_trial_;
    double trial_duration_;

};

#endif

