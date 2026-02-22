#version 450

layout(push_constant) uniform PushConstBlock {
    vec2 scale;
    vec2 translate;
} pushConst;

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec4 color;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

void main() {
    fragTexCoord = texCoord;
    fragColor = color;
    gl_Position = vec4(pushConst.scale * position + pushConst.translate, 0.0, 1.0);
}
