#version 450
layout(binding = 0) uniform sampler2D tex;
layout(push_constant) uniform PushF { layout(offset = 64) int colorkeying; int texturing; } pushf;
layout(location = 0) in vec4 color;
layout(location = 1) in vec2 texc;
layout(location = 0) out vec4 fcolor;
void main()
{
    vec2 dx = dFdx(texc);
    vec2 dy = dFdy(texc);
    if (pushf.texturing != 0)
        fcolor = textureGrad(tex, texc, dx, dy) * color;
    else
        fcolor = color;
    if (pushf.colorkeying != 0 && fcolor.a <= (100.0 / 255.0))
        discard;
}
