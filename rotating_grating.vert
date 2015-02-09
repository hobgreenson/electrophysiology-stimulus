#version 410
#define PI 3.14159265359

layout(location = 0) in vec2 vPosition;
layout(location = 1) in vec4 vColor;

uniform mat4 transform_matrix;

flat out vec4 color;

void main()
{
    vec4 p = transform_matrix * vec4(vPosition, 0, 1);
    
    float r = transform_matrix[0][0];
    if(abs(p.x) <= r)
    {
        float frac = (p.x + r) / (2 * r);
        p.x = r * cos(-PI * (1 + frac));
        color = vColor / 255;
    }
    else
    {
        color = vec4(0,0,0,0);
    }

    gl_Position = p;
}
