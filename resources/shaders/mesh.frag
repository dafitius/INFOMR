#version 330 core

in vec3 vViewPos;
out vec4 color;

uniform vec3 fillColor;

void main()
{
    if (!gl_FrontFacing)
    {
        color = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 normal = normalize(cross(dFdx(vViewPos), dFdy(vViewPos)));

    vec3 lightDir = normalize(-vViewPos);

    float diffuse = abs(dot(normal, lightDir));
    float ambient = 0.25;

    vec3 shaded = fillColor * (ambient + diffuse * (1.0 - ambient));

    color = vec4(shaded, 1.0);
}
