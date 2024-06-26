#version 450

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 0) out vec2 v_uv;

layout(set = 0, binding = 0) uniform VertexBufferObject {
    vec4 geom;
} ubo;

layout(push_constant) uniform VertexPushConstant {
    float yoff;
} push;

void main() {
    v_uv = a_uv;
    gl_Position = vec4((((a_pos.x + ubo.geom.x) / ubo.geom.z) * 2.0) - 1.0, (((a_pos.y + ubo.geom.y + push.yoff) / ubo.geom.w) * 2.0) - 1.0, 0.0, 1.0);
}
