#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 3) in vec2 uv;

uniform mat4 u_mvp;
uniform float u_time;
uniform float u_yWaterPlane;

out vec2 vTexCoord;
out vec3 vPosition;

const float pi = 3.14159265358979323846;

float waveAmplitude = 0.25;
float waveFreq = 128.0 * 2 * pi;
float waveSpeed = 0.005;

float distance_from(float x, float y) 
{
    return sqrt(x * x + y * y);
}

float add_wave(float xSource, float ySource, float speed, float amplitude, float frequency, float waveLength)
{
    return amplitude * cos((u_time * speed - waveLength * distance_from(xSource, ySource)) * frequency);
}

void main() 
{
    float a0 = add_wave(uv.x * 0.5, uv.x * 0.5, waveSpeed * 10.0, waveAmplitude * 2.0, 0.1 * waveFreq, 2.0);
    float a1 = add_wave(uv.x, uv.y, waveSpeed, waveAmplitude, waveFreq, 3.0);
    float a2 = add_wave(0.0, uv.y, waveSpeed, waveAmplitude, 0.5 * waveFreq, 1.0);
    float a3 = add_wave(uv.x, 0.0, waveSpeed, waveAmplitude, 0.5 * waveFreq, 2.0);
    float a4 = add_wave(uv.x * 0.5, uv.y * 0.5, waveSpeed, waveAmplitude, 0.5 * waveFreq, 2.0);
    float a5 = add_wave(0.0, 0.0, waveSpeed, waveAmplitude, .5 * waveFreq, 2.0);

    vec3 pos = vec3(position.xy, u_yWaterPlane + 0.3 * (a0 + a1 + a2 + a3 + a4 + a5));
    gl_Position = u_mvp * vec4(pos, 1.0);

    vTexCoord = uv;
    vPosition = pos;
}
