#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 0) uniform FragmentBufferObject {
layout(set = 2, binding = 0) uniform sampler u_smp0;
layout(set = 2, binding = 1) uniform texture2D u_tex0;
    vec4 color;
} ubo;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 smp = texture(sampler2D(u_tex0, u_smp0), v_uv).rgb;
    ivec2 sz = textureSize(sampler2D(u_tex0, u_smp0), 0);

    float dx = dFdx(v_uv.x) * sz.x;
    float dy = dFdy(v_uv.y) * sz.y;
    float toPixels = 16.0 * inversesqrt(dx * dx + dy * dy);
    float sigDist = median(smp.r, smp.g, smp.b) - 0.5;
    float alpha = clamp(sigDist * toPixels + 0.5, 0.0, 1.0);

    fragColor = vec4(pow(ubo.color.rgb, vec3(1.0 / 1.8)), alpha);
}
