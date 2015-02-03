
#include "Experiment.h"

Experiment::Experiment(int type, char* path, Mesh* mesh)
    : type_(type), not_done_(true), mesh_(mesh), serial_up_(false)
{
    p_ = new Protocol(path, type_);
    //serial_chan_ = new serial::Serial("/dev/tty.usbmodem1411", // port ID 
    //                                  4 * 115200, // baud rate 
    //                                  serial::Timeout::simpleTimeout(1000));
    switch (type_)
    {
        case DRIFTING_GRATING:
        {
            break;
        }
        case PREY:
        {
            curr_speed_ = p_->nextSpeed();
            float scale_r = p_->nextSize();
            trial_duration_ = SCREEN_WIDTH_GL / curr_speed_;
            elapsed_in_trial_ = 0;
            
            mesh_->resetScale();
            mesh_->scaleXY(scale_r);
            mesh_->centerXY(SCREEN_EDGE_GL, -0.02);
            
            if (!serial_up_)
            {
                triggerSerial();
                serial_up_ = !serial_up_;
            }
            break;
        }
        default:
            break;
    }
}

Experiment::~Experiment() 
{
    //serial_chan_.close();
    //delete serial_chan_;
    delete p_;    
}

void Experiment::triggerSerial()
{
    //serial_chan_.write('a', 1);
}

void Experiment::update(double dt)
{
    if (elapsed_in_trial_ <= trial_duration_) // trial is not done yet
    {
        mesh_->translateX(curr_speed_ * dt);
        elapsed_in_trial_ += dt;
    }
    else if (elapsed_in_trial_ <= 2 * trial_duration_) // inter-trial period
    {
        elapsed_in_trial_ += dt;
        mesh_->centerXY(2, -0.02); // move mesh off-screen
        if (serial_up_)
        {
            triggerSerial();
            serial_up_ = !serial_up_;
        }
    }
    else // start a new trial
    {
        curr_speed_ = p_->nextSpeed();
        float scale_r = p_->nextSize();

        if (curr_speed_ < 0 || scale_r < 0) // end of protocol
            not_done_ = false;
        else
        {
            trial_duration_ = SCREEN_WIDTH_GL / curr_speed_;
            elapsed_in_trial_ = 0;
            mesh_->resetScale();
            mesh_->scaleXY(scale_r);
            mesh_->centerXY(SCREEN_EDGE_GL, -0.02);
        }
        if (!serial_up_)
        {
            triggerSerial();
            serial_up_ = !serial_up_;
        }
    }
}

