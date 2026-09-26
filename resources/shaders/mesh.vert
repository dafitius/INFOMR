#version 330 core

layout(location = 0) in vec3 position;

uniform mat4 mvp;
uniform mat4 modelView;

out vec3 vViewPos;

void main()
{
    vViewPos = vec3(modelView * vec4(position, 1.0));
    gl_Position = mvp * vec4(position, 1.0);
}
