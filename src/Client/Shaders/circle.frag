#version 450

layout(push_constant) uniform BodyColor {
    vec4 color;
} bodyColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = bodyColor.color;
}
