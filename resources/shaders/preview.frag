#version 330 core

in vec3 vViewPos;
out vec4 color;

uniform vec3 fillColor = vec3(0.25, 0.65, 0.95);

void main()
{
    vec3 normal = normalize(cross(dFdx(vViewPos), dFdy(vViewPos)));

    // Simple fixed "headlight" style light, coming from the camera.
    vec3 lightDir = normalize(-vViewPos);

    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.25;

    vec3 shaded = fillColor * (ambient + diffuse * (1.0 - ambient));

    color = vec4(shaded, 1.0);
}