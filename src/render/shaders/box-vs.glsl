#version 450

vec2 positions[4] = vec2[](
    vec2(-1.0, +1.0),
    vec2(+1.0, +1.0),
    vec2(-1.0, -1.0),
    vec2(+1.0, -1.0)
);

vec2 uvs[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);

layout(binding = 0) uniform VertexUniformBufferObject {
    vec4 geom;
} vubo;

layout(location = 0) out vec2 fragCoord;

void main() {
    vec2 position = positions[gl_VertexIndex];
    gl_Position = vec4(position, 0.0, 1.0);
    fragCoord = (uvs[gl_VertexIndex] * vubo.geom.zw) + vubo.geom.xy;
}
