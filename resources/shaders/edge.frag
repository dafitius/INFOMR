#version 330 core

out vec4 color;

uniform vec3 edgeColor;

void main()
{
    color = vec4(edgeColor, 1.0);
}
