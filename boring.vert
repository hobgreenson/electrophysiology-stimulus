#version 410

layout (location = 0) in vec2 vPosition;
layout (location = 1) in vec4 vColor;

uniform mat4 transform_matrix;

flat out vec4 color;

void main()
{
    gl_Position = transform_matrix * vec4(vPosition, 0, 1);
    color = vColor / 255;
}
