#version 410
#define PI 3.14159265359

layout(location = 0) in vec2 vPosition;
layout(location = 1) in vec4 vColor;

uniform mat4 transform_matrix;

flat out vec4 color;

void main() {
    vec2 w = vPosition;
    w.y -= 1;
    gl_Position = transform_matrix * vec4(w, 0, 1);
    color = vColor / 255;
}

