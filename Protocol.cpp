#include "Protocol.h"

Protocol::Protocol()
    : size_index_(0), speed_index_(0), mode_index_(0)
{
    
}

Protocol::~Protocol()
{
    free(size_array_);
    free(speed_array_);
    free(mode_array_);
}

void Protocol::createOpenLoopStepOMR(char* path) {
    int speed_set[4] = {4, 10, 40, 80};
    int mode_set[3] = {0, 1, 2};
    length_ = 3 * 4 * 5;
    mode_array_ = (int*) malloc(length_ * sizeof(int));
    speed_array_ = (int*) malloc(length_ * sizeof(int));
    size_array_ = (int*) malloc(length_ * sizeof(int));
    int arr_i = 0;
    for(int i = 0; i < 4; i++)
    {
        int speed = speed_set[i];
        for(int j = 0; j < 3; j++)
        {
            int mode = mode_set[j];
            for(int k = 0; k < 5; k++)
            {
                mode_array_[arr_i + k] = mode;
                speed_array_[arr_i + k] = speed;
            }
            arr_i += 5;
        }
    }
    
    shuffle();
    
    FILE* file = fopen(path, "w");
    for (int i = 0; i < length_; ++i)
        fprintf(file, "%d ", mode_array_[i]);
    fprintf(file, "\n");
    for (int i = 0; i < length_; ++i)
        fprintf(file, "%d ", speed_array_[i]);
    fclose(file);
}

void Protocol::createShortOpenLoopStepOMR(char* path) {
    int speed_set = 10;
    int mode_set[3] = {0, 1, 2};
    length_ = 3 * 5;
    mode_array_ = (int*) malloc(length_ * sizeof(int));
    speed_array_ = (int*) malloc(length_ * sizeof(int));
    size_array_ = (int*) malloc(length_ * sizeof(int));
    
    int arr_i = 0;
    int speed = speed_set;
    for(int j = 0; j < 3; j++)
    {
        int mode = mode_set[j];
        for(int k = 0; k < 5; k++)
        {
            mode_array_[arr_i + k] = mode;
            speed_array_[arr_i + k] = speed;
        }
        arr_i += 5;
    }
    
    shuffle();
    
    FILE* file = fopen(path, "w");
    for (int i = 0; i < length_; ++i)
        fprintf(file, "%d ", mode_array_[i]);
    fprintf(file, "\n");
    for (int i = 0; i < length_; ++i)
        fprintf(file, "%d ", speed_array_[i]);
    fclose(file);
}

void Protocol::createOpenLoopPrey(char* path) {
    int speed_set[5] = {30, 60, 90, 120, 150};
    int size_set[6] = {1, 3, 5, 7, 20, 30};
    length_ = 150; // 5x reps for each speed-size combo
    size_array_ = (int*) malloc(length_ * sizeof(int));
    speed_array_ = (int*) malloc(length_ * sizeof(int));
    mode_array_ = (int*) malloc(length_ * sizeof(int));
    
    int arr_i = 0;
    for(int i = 0; i < 5; i++)
    {
        int speed = speed_set[i];
        for(int j = 0; j < 6; j++)
        {
            int size = size_set[j];
            for(int k = 0; k < 5; k++)
            {
                size_array_[arr_i + k] = size;
                speed_array_[arr_i + k] = speed;
            }
            arr_i += 5;
        }
    }
    
    shuffle();
    
    FILE* file = fopen(path, "w");
    for (int i = 0; i < length_; ++i) {
        fprintf(file, "%d ", size_array_[i]);
    }
    fprintf(file, "\n");
    for (int i = 0; i < length_; ++i) {
        fprintf(file, "%d ", speed_array_[i]);
    }
    fclose(file);
}

void Protocol::reset() {
    size_index_ = 0;
    speed_index_ = 0;
    mode_index_ = 0;
}

float Protocol::nextSize() {
    int size = size_index_ < length_ ? size_array_[size_index_++] : -1;
    return sizeToGL(size);
}

float Protocol::sizeToGL(int size) {
    return SCREEN_WIDTH_GL * ((float)size / SCREEN_WIDTH_DEG);
}

float Protocol::nextSpeed() {
    int speed = speed_index_ < length_ ? speed_array_[speed_index_++] : -1;
    return speedToGL(speed);
}

float Protocol::speedToGL(int speed) {
    return SCREEN_WIDTH_GL * ((float)speed / SCREEN_WIDTH_DEG);
}

int Protocol::nextMode() {
    return mode_index_ < length_ ? mode_array_[mode_index_++] : -1;
}

template <typename T> void Protocol::swap(T* a, T* b) {
    T temp = *a;
    *a = *b;
    *b = temp;
}

void Protocol::shuffle() {
    srand(time(NULL));
    int i, ri;
    for (i = 0; i < length_; ++i) {
        ri = (rand() % (length_ - i)) + i;
        swap(size_array_ + i, size_array_ + ri);
        swap(speed_array_ + i, speed_array_ + ri);
        swap(mode_array_ + i, mode_array_ + ri);
    }
}
