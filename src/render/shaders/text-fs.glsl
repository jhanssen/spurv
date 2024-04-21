#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform FragmentBufferObject {
    vec4 color;
    float pixelRange;
} ubo;
layout(set = 0, binding = 2) uniform sampler u_smp0;
layout(set = 0, binding = 3) uniform texture2D u_tex0;

float screenPxRange(vec2 sz) {
    vec2 unitRange = vec2(ubo.pixelRange) / sz;
    vec2 screenTexSize = vec2(1.0) / (fwidth(v_uv) / sz);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    ivec2 sz = textureSize(sampler2D(u_tex0, u_smp0), 0);
    vec2 texCoord = v_uv / vec2(sz);

    vec3 msd = texture(sampler2D(u_tex0, u_smp0), texCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange(vec2(sz)) * (sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

    fragColor = vec4(pow(ubo.color.rgb, vec3(1.0 / 1.8)), opacity);
}
