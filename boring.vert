#version 410

layout (location = 0) in vec2 vPosition;

uniform mat4 transform_matrix;
uniform vec4 color;

flat out vec4 out_color;

void main()
{
    gl_Position = transform_matrix * vec4(vPosition, 0, 1);
    out_color = color;
}
