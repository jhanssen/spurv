#version 450

// adapted from https://www.shadertoy.com/view/fsdyzB

layout(location = 0) in vec2 fragCoord;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform FragmentBufferObject {
    vec4 colorBg; // background color
    vec4 colorRect; // rect color
    vec4 colorBorder; // border color
    vec4 colorShadow; // shadow color
    vec4 cornerRadiuses; //rediuses of corners in pixels (top right, bottom right, top left, bottom left)
    vec2 resolution; // viewport resolution
    vec2 rectSize; // pixel space of rectangle
    vec2 rectCenter; // center location of rectangle
    vec2 shadowOffset; // pixel-space shadow offset from rect center
    float edgeSoftness; // softness of edges in pixels
    float borderThickness; // border size in pixels
    float borderSoftness; // softness of internal border in pixels
    float shadowSoftness; // half shadow radius in pixels
} ubo;

float roundedBoxSDF(vec2 CenterPosition, vec2 Size, vec4 Radius)
{
    Radius.xy = (CenterPosition.x > 0.0) ? Radius.xy : Radius.zw;
    Radius.x  = (CenterPosition.y > 0.0) ? Radius.x  : Radius.y;

    vec2 q = abs(CenterPosition)-Size+Radius.x;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - Radius.x;
}

void main()
{
    vec2 halfSize = (ubo.rectSize / 2.0); // Rectangle extents (half of the size)

    // vec4 radius = vec4((sin(ubo.time) + 1.0)) * ubo.cornerRadiuses; // Animated corners radiuses

    // -------------------------------------------------------------------------

    // Calculate distance to edge.
    float distance = roundedBoxSDF(fragCoord.xy - ubo.rectCenter, halfSize, ubo.cornerRadiuses);

    // Smooth the result (free antialiasing).
    float smoothedAlpha = 1.0-smoothstep(0.0, ubo.edgeSoftness, distance);

    // -------------------------------------------------------------------------
    // Border.

    float borderAlpha   = 1.0-smoothstep(ubo.borderThickness - ubo.borderSoftness, ubo.borderThickness, abs(distance));

    // -------------------------------------------------------------------------
    // Apply a drop shadow effect.

    float shadowDistance  = roundedBoxSDF(fragCoord.xy - ubo.rectCenter + ubo.shadowOffset, halfSize, ubo.cornerRadiuses);
    float shadowAlpha 	  = 1.0-smoothstep(-ubo.shadowSoftness, ubo.shadowSoftness, shadowDistance);


    // -------------------------------------------------------------------------
    // Debug output

        // vec4 debug_sdf = vec4(distance, 0.0, 0.0, 1.0);

        // Notice, that instead simple 'alpha' here is used 'min(ubo.colorRect.a, alpha)' to enable transparency
        // vec4 debug_rect_color   = mix(ubo.colorBg, ubo.colorRect, min(ubo.colorRect.a, smoothedAlpha));

        // Notice, that instead simple 'alpha' here is used 'min(ubo.colorBorder.a, alpha)' to enable transparency
        // vec4 debug_border_color = mix(ubo.colorBg, ubo.colorBorder, min(ubo.colorBorder.a, min(borderAlpha, smoothedAlpha)) );

    // -------------------------------------------------------------------------
    // Apply colors layer-by-layer: background <- shadow <- rect <- border.

    // Blend background with shadow
    vec4 res_shadow_color = mix(ubo.colorBg, vec4(ubo.colorShadow.rgb, shadowAlpha), shadowAlpha);

    // Blend (background+shadow) with rect
    //   Note:
    //     - Used 'min(ubo.colorRect.a, smoothedAlpha)' instead of 'smoothedAlpha'
    //       to enable rectangle color transparency
    vec4 res_shadow_with_rect_color =
        mix(
            res_shadow_color,
            ubo.colorRect,
            min(ubo.colorRect.a, smoothedAlpha)
        );

    // Blend (background+shadow+rect) with border
    //   Note:
    //     - Used 'min(borderAlpha, smoothedAlpha)' instead of 'borderAlpha'
    //       to make border 'internal'
    //     - Used 'min(ubo.colorBorder.a, alpha)' instead of 'alpha' to enable
    //       border color transparency
    vec4 res_shadow_with_rect_with_border =
        mix(
            res_shadow_with_rect_color,
            ubo.colorBorder,
            min(ubo.colorBorder.a, min(borderAlpha, smoothedAlpha))
        );

    // -------------------------------------------------------------------------

    fragColor = res_shadow_with_rect_with_border;
}
