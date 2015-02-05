#version 410
#define PI 3.14159265359

layout(location = 0) in vec2 vPosition;
layout(location = 1) in vec4 vColor;

uniform mat4 transform_matrix;

flat out vec4 color;

void main()
{
    int sign = 1;
    vec2 w = vPosition;
    if (abs(w.y) > 1)
        w.y /= 2;
    else
        sign *= -1;
    
    vec4 p = transform_matrix * vec4(w, 0, 1);
    
    if (p.x > 0 && p.x < r)
        p.x = sign * sqrt(r * r - p.x * p.x);
    else if (p.x >= r)
        p.x = 0;
    else
        p.x = sign * r;
    
    gl_Position = p;
    color = vColor / 255;
}
