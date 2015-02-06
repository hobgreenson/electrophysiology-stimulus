
#include "Experiment.h"

Experiment::Experiment(int type, char* path, Mesh* mesh)
    : type_(type), not_done_(true), mesh_(mesh), mesh1_(NULL), mesh2_(NULL), serial_up_(false)
{
    /* this constructor works for the prey experiment type */
    p_ = new Protocol(path, type_);
    
    curr_speed_ = p_->nextSpeed();
    float scale_r = p_->nextSize();
    trial_duration_ = SCREEN_WIDTH_GL / curr_speed_;
    elapsed_in_trial_ = 0;
    
    mesh_->resetScale();
    mesh_->scaleXY(scale_r);
    mesh_->centerXY(SCREEN_EDGE_GL, -0.02);
        
    serial_up_ = false;
}


Experiment::Experiment(int type, char* path, Mesh* mesh1, Mesh* mesh2)
: type_(type), not_done_(true), mesh_(NULL), mesh1_(mesh1), mesh2_(mesh2), serial_up_(false)
{
    /* this constructor works for the difting grating experiment type */
    p_ = new Protocol(path, type_);
    
    curr_speed_ = p_->nextSpeed();
    curr_mode_ = p_->nextMode();
    
    trial_duration_ = 1;
    elapsed_in_trial_ = 0;
    
    serial_up_ = false;
}


Experiment::~Experiment()
{
    delete p_;    
}

void Experiment::triggerSerial(serial::Serial* chan, const uint8_t* msg)
{
    chan->write(msg, 1);
}

void Experiment::update(double dt, serial::Serial* chan, const uint8_t* msg)
{
    switch (type_)
    {
        case PREY:
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
                    triggerSerial(chan, msg);
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
                    if (!serial_up_)
                    {
                        triggerSerial(chan, msg);
                        serial_up_ = !serial_up_;
                    }
                }
            }
            break;
        }
        case DRIFTING_GRATING:
        {
            if (elapsed_in_trial_ <= trial_duration_) // trial is not done yet
            {
                float coeff = curr_mode_ == 0 ? -1 : 1;
                mesh1_->translateX(coeff * curr_speed_ * dt);
                mesh2_->translateX(coeff * curr_speed_ * dt);
                elapsed_in_trial_ += dt;
                if (!serial_up_)
                {
                    triggerSerial(chan, msg);
                    serial_up_ = true;
                }
            }
            else if (elapsed_in_trial_ <= 2 * trial_duration_) // inter-trial period
            {
                elapsed_in_trial_ += dt;
                if (serial_up_)
                {    
                    triggerSerial(chan, msg);
                    serial_up_ = false;
                }
            }
            else // start a new trial
            {
                curr_speed_ = p_->nextSpeed();
                curr_mode_ = p_->nextMode();
                if (curr_speed_ < 0 || curr_mode_ < 0) // end of protocol
                    not_done_ = false;
                else
                {
                    elapsed_in_trial_ = 0;
                    triggerSerial(chan, msg);
                    serial_up_ = true;
                }
            }
            break;
        }
        default:
            break;
    }
}

