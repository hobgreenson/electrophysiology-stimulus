#include "Protocol.h"

Protocol::Protocol()
    : size_index_(0), speed_index_(0), mode_index_(0), gain_index_(0) {
    srand(time(NULL));
}

Protocol::~Protocol() {
    free(size_array_);
    free(speed_array_);
    free(mode_array_);
    free(gain_array_);
}

void Protocol::createOpenLoopStepOMR(bool saveit, char* path) {
    const int n_speeds = 2, n_modes = 3, n_reps = 10;
    float speed_set[n_speeds] = {2.0, 10.0};
    int mode_set[n_modes] = {0, 1, 2};
    length_ = n_modes * n_speeds * n_reps;
    
    mode_array_ = (int*) malloc(length_ * sizeof(int));
    speed_array_ = (float*) malloc(length_ * sizeof(float));
    size_array_ = (int*) malloc(length_ * sizeof(int));
    gain_array_ = (float*) malloc(length_ * sizeof(float));
    
    int arr_i = 0;
    for (int i = 0; i < n_speeds; i++) {
        for (int j = 0; j < n_modes; j++) {
            for (int k = 0; k < n_reps; k++) {
                mode_array_[arr_i + k] = mode_set[j];
                speed_array_[arr_i + k] = speed_set[i];
            }
            arr_i += n_reps;
        }
    }
    
    shuffle(mode_array_);
    shuffle(speed_array_);
    
    if (saveit) {
        FILE* file = fopen(path, "w");
        for (int i = 0; i < length_; ++i) {
            fprintf(file, "%d ", mode_array_[i]);
        }
        fprintf(file, "\n");
        for (int i = 0; i < length_; ++i) {
            fprintf(file, "%f ", speed_array_[i]);
        }
        fclose(file);
    }
}

void Protocol::createClosedLoopStepOMR(bool saveit, char* path) {
    const int n_speeds = 1, n_modes = 2, n_reps = 5, n_gains = 4;
    float speed_set[n_speeds] = {10.0};
    int mode_set[n_modes] = {0, 1};
    float gain_set[n_gains] = {2.0, 1.0, -1.0, -2.0};
    
    length_ = n_modes * n_speeds * n_reps * n_gains;
    mode_array_ = (int*) malloc(length_ * sizeof(int));
    speed_array_ = (float*) malloc(length_ * sizeof(float));
    size_array_ = (int*) malloc(length_ * sizeof(int));
    gain_array_ = (float*) malloc(length_ * sizeof(float));
    
    for (int i = 0; i < length_; ++i) {
        speed_array_[i] = speed_set[0];
    }
    
    int arr_i = 0;
    for (int i = 0; i < n_gains; i++) {
        for (int j = 0; j < n_modes; j++) {
            for (int k = 0; k < n_reps; k++) {
                mode_array_[arr_i + k] = mode_set[j];
                gain_array_[arr_i + k] = gain_set[i];
            }
            arr_i += n_reps;
        }
    }
    
    shuffle(mode_array_);
    
    if (saveit) {
        FILE* file = fopen(path, "w");
        for (int i = 0; i < length_; ++i) {
            fprintf(file, "%d ", mode_array_[i]);
        }
        fprintf(file, "\n");
        for (int i = 0; i < length_; ++i) {
            fprintf(file, "%f ", speed_array_[i]);
        }
        fprintf(file, "\n");
        for (int i = 0; i < length_; ++i) {
            fprintf(file, "%f ", gain_array_[i]);
        }
        fclose(file);
    }
}

void Protocol::createOpenLoopPrey(bool saveit, char* path) {
    const int n_speeds = 5, n_sizes = 6, n_reps = 5;
    float speed_set[n_speeds] = {30, 60, 90, 120, 150};
    int size_set[n_sizes] = {1, 3, 5, 7, 20, 30};
    length_ = n_speeds * n_sizes * n_reps; // 5x reps for each speed-size combo
    
    size_array_ = (int*) malloc(length_ * sizeof(int));
    speed_array_ = (float*) malloc(length_ * sizeof(float));
    mode_array_ = (int*) malloc(length_ * sizeof(int));
    gain_array_ = (float*) malloc(length_ * sizeof(float));
    
    int arr_i = 0;
    for(int i = 0; i < n_speeds; i++) {
        for(int j = 0; j < n_sizes; j++) {
            for(int k = 0; k < n_reps; k++) {
                size_array_[arr_i + k] = size_set[j];
                speed_array_[arr_i + k] = speed_set[i];
            }
            arr_i += n_reps;
        }
    }
    
    shuffle(size_array_);
    shuffle(speed_array_);
    
    if (saveit) {
        FILE* file = fopen(path, "w");
        for (int i = 0; i < length_; ++i) {
            fprintf(file, "%d ", size_array_[i]);
        }
        fprintf(file, "\n");
        for (int i = 0; i < length_; ++i) {
            fprintf(file, "%f ", speed_array_[i]);
        }
        fclose(file);
    }
}

void Protocol::reset() {
    size_index_ = 0;
    speed_index_ = 0;
    mode_index_ = 0;
    gain_index_ = 0;
}

float Protocol::sizeToGL(int size) {
    return SCREEN_WIDTH_GL * ((float)size / SCREEN_WIDTH_DEG);
}

float Protocol::speedToGL(int speed) {
    return SCREEN_WIDTH_GL * ((float)speed / SCREEN_WIDTH_DEG);
}

float Protocol::nextGain() {
    float gain = (gain_index_ < length_) ? gain_array_[gain_index_++] : -1;
    return gain;
}

float Protocol::nextSize() {
    int size = (size_index_ < length_) ? size_array_[size_index_++] : -1;
    return sizeToGL(size);
}

float Protocol::nextSpeed() {
    float speed = (speed_index_ < length_) ? speed_array_[speed_index_++] : -1;
    return speed;
}

int Protocol::nextMode() {
    return (mode_index_ < length_) ? mode_array_[mode_index_++] : -1;
}

template <typename T> void Protocol::swap(T* a, T* b) {
    T temp = *a;
    *a = *b;
    *b = temp;
}

template <typename T> void Protocol::shuffle(T* x) {
    int i, ri;
    for (i = 0; i < length_; ++i) {
        ri = (rand() % (length_ - i)) + i;
        swap(x + i, x + ri);
    }
}
