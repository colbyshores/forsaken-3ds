#version 450
layout(location = 0) in vec4 tlpos;
layout(location = 1) in vec4 vcolor;
layout(location = 2) in vec2 vtexc;
layout(push_constant) uniform Push { mat4 ortho; } push;
layout(location = 0) out vec4 color;
layout(location = 1) out vec2 texc;
void main()
{
    gl_Position = push.ortho * tlpos;
    gl_Position.y = -gl_Position.y;
    color = vcolor.bgra;
    texc = vtexc;
}
