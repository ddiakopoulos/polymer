#version 450 core

uniform vec4 u_color = vec4(1, 1, 1, 0.5);

in vec3 triangleDist;
out vec4 f_color;

// http://codeflow.org/entries/2012/aug/02/easy-wireframe-display-with-barycentric-coordinates/
float edge_factor(float lineWidth) 
{
    vec3 d = fwidth(triangleDist);
    vec3 a3 = smoothstep(vec3(0.0), d * lineWidth, triangleDist);
    return min(min(a3.x, a3.y), a3.z);
}

// returns a wireframed color from a fill, stroke and linewidth
vec4 wireframe(vec4 fill, vec4 stroke, float lineWidth) 
{
    return mix(stroke, fill, edge_factor(lineWidth));
}

void main()
{
    f_color = wireframe(vec4(0, 0, 0, 0), u_color, 1.25); 
}
