#version 410

flat in vec4 color;
out vec4 fcolor;

void main()
{
   if (color[3] == 0)
       discard;
   fcolor = color;
}
