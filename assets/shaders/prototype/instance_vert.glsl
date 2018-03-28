    #version 330

    layout(location = 0) in vec3 inPosition;
    layout(location = 1) in vec3 inNormal;
    layout(location = 2) in vec3 inColor;
    layout(location = 3) in vec2 inTexcoord;

    layout(location = 4) in vec3 instanceColor;
    layout(location = 5) in vec3 instanceLocation;

    uniform mat4 u_modelMatrix;
    uniform mat4 u_modelMatrixIT;
    uniform mat4 u_viewProj;

    out vec3 color;
    out vec3 normal;

    void main()
    {
        vec4 worldPos = u_modelMatrix * vec4(instanceLocation + inPosition, 1);
        gl_Position = u_viewProj * worldPos;
        color = instanceColor;
        normal = normalize((u_modelMatrixIT * vec4(inNormal,0)).xyz);
    }