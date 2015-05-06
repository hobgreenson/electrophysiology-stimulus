#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstdio>

#define SCREEN_WIDTH_GL 0.7 //0.68
#define SCREEN_WIDTH_DEG 203
#define DRIFTING_GRATING 0
#define PREY 1

class Protocol
{
public:
    Protocol(int experiment_type);
    ~Protocol();
    float nextSize();
    float nextSpeed();
    int nextMode();
    void reset();
    int experiment_type_;
    void save(char* path);
    
private:
    int* size_array_;
    int* speed_array_;
    int* mode_array_;
    int size_index_;
    int speed_index_;
    int mode_index_;
    int length_;
    
    float sizeToGL(int size); 
    float speedToGL(int speed); 
    
    void shuffle();
    template <typename T> void swap(T* a, T* b);
};

#endif
