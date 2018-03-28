#version 330

layout(location = 0) in vec3 position;

void main()
{
    gl_Position = vec4(position, 1.0);
    gl_Position.z = 0.9999; // Clamp depth right before far clip
}
