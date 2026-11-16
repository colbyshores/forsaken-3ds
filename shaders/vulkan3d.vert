#version 450
layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 vcolor;
layout(location = 2) in vec2 vtexc;
layout(push_constant) uniform Push { mat4 mvp; } push;
layout(location = 0) out vec4 color;
layout(location = 1) out vec2 texc;
void main()
{
    gl_Position = push.mvp * vec4(pos, 1.0);
    gl_Position.y = -gl_Position.y; /* GL-style bottom-up NDC to Vulkan top-down */
    color = vcolor.bgra;
    texc = vtexc;
}
