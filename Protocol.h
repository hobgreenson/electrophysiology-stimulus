#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstdio>

#define SCREEN_WIDTH_GL 0.7 //0.68
#define SCREEN_WIDTH_DEG 203

#define OPEN_LOOP_OMR 0
#define OPEN_LOOP_PREY 1
#define CLOSED_LOOP_OMR 2
#define CLOSED_LOOP_PREY 3

class Protocol
{
public:
    Protocol();
    ~Protocol();
    
    int experiment_type_;
    
    void createOpenLoopStepOMR(char* path);
    void createShortOpenLoopStepOMR(char* path);
    void createOpenLoopPrey(char* path);
    
    float nextSize();
    float nextSpeed();
    int nextMode();
    void reset();
    
    float sizeToGL(int size);
    float speedToGL(int speed);
    
private:
    int* size_array_;
    int* speed_array_;
    int* mode_array_;
    int size_index_;
    int speed_index_;
    int mode_index_;
    int length_;
    void shuffle();
    template <typename T> void swap(T* a, T* b);
};

#endif
