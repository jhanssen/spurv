#include "Easing.h"
#include <array>
#include <cmath>

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunsequenced"
#else
// gcc?
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wsequence-point"
#endif

#ifndef PI
#define PI 3.1415926545f
#endif

namespace spurv {

float easeInSine( float t ) {
    return sinf( 1.5707963f * t );
}

float easeOutSine( float t ) {
    return 1 + sinf( 1.5707963f * (--t) );
}

float easeInOutSine( float t ) {
    return 0.5f * (1 + sin( 3.1415926f * (t - 0.5f) ) );
}

float easeInQuad( float t ) {
    return t * t;
}

float easeOutQuad( float t ) {
    return t * (2 - t);
}

float easeInOutQuad( float t ) {
    return t < 0.5f ? 2 * t * t : t * (4 - 2 * t) - 1;
}

float easeInCubic( float t ) {
    return t * t * t;
}

float easeOutCubic( float t ) {
    return 1 + (--t) * t * t;
}

float easeInOutCubic( float t ) {
    return t < 0.5f ? 4 * t * t * t : 1 + (--t) * (2 * (--t)) * (2 * t);
}

float easeInQuart( float t ) {
    t *= t;
    return t * t;
}

float easeOutQuart( float t ) {
    t = (--t) * t;
    return 1 - t * t;
}

float easeInOutQuart( float t ) {
    if( t < 0.5f ) {
        t *= t;
        return 8 * t * t;
    } else {
        t = (--t) * t;
        return 1 - 8 * t * t;
    }
}

float easeInQuint( float t ) {
    float t2 = t * t;
    return t * t2 * t2;
}

float easeOutQuint( float t ) {
    float t2 = (--t) * t;
    return 1 + t * t2 * t2;
}

float easeInOutQuint( float t ) {
    float t2;
    if( t < 0.5f ) {
        t2 = t * t;
        return 16 * t * t2 * t2;
    } else {
        t2 = (--t) * t;
        return 1 + 16 * t * t2 * t2;
    }
}

float easeInExpo( float t ) {
    return (powf( 2, 8 * t ) - 1) / 255;
}

float easeOutExpo( float t ) {
    return 1 - powf( 2, -8 * t );
}

float easeInOutExpo( float t ) {
    if( t < 0.5f ) {
        return (powf( 2, 16 * t ) - 1) / 510;
    } else {
        return 1 - 0.5f * powf( 2, -16 * (t - 0.5f) );
    }
}

float easeInCirc( float t ) {
    return 1 - sqrtf( 1 - t );
}

float easeOutCirc( float t ) {
    return sqrtf( t );
}

float easeInOutCirc( float t ) {
    if( t < 0.5f ) {
        return (1 - sqrtf( 1 - 2 * t )) * 0.5f;
    } else {
        return (1 + sqrtf( 2 * t - 1 )) * 0.5f;
    }
}

float easeInBack( float t ) {
    return t * t * (2.70158f * t - 1.70158f);
}

float easeOutBack( float t ) {
    return 1 + (--t) * t * (2.70158f * t + 1.70158f);
}

float easeInOutBack( float t ) {
    if( t < 0.5f ) {
        return t * t * (7 * t - 2.5f) * 2;
    } else {
        return 1 + (--t) * t * 2 * (7 * t + 2.5f);
    }
}

float easeInElastic( float t ) {
    float t2 = t * t;
    return t2 * t2 * sinf( t * PI * 4.5f );
}

float easeOutElastic( float t ) {
    float t2 = (t - 1) * (t - 1);
    return 1 - t2 * t2 * cosf( t * PI * 4.5f );
}

float easeInOutElastic( float t ) {
    float t2;
    if( t < 0.45f ) {
        t2 = t * t;
        return 8 * t2 * t2 * sinf( t * PI * 9 );
    } else if( t < 0.55f ) {
        return 0.5f + 0.75f * sinf( t * PI * 4 );
    } else {
        t2 = (t - 1) * (t - 1);
        return 1 - 8 * t2 * t2 * sinf( t * PI * 9 );
    }
}

float easeInBounce( float t ) {
    return pow( 2, 6 * (t - 1) ) * fabsf( sinf( t * PI * 3.5f ) );
}

float easeOutBounce( float t ) {
    return 1 - pow( 2, -6 * t ) * fabsf( cosf( t * PI * 3.5f ) );
}

float easeInOutBounce( float t ) {
    if( t < 0.5f ) {
        return 8 * pow( 2, 8 * (t - 1) ) * fabsf( sinf( t * PI * 7 ) );
    } else {
        return 1 - 8 * pow( 2, -8 * t ) * fabsf( sinf( t * PI * 7 ) );
    }
}

static std::array<EasingFunction, 30> easingFunctions = {
    easeInSine,
    easeOutSine,
    easeInOutSine,
    easeInQuad,
    easeOutQuad,
    easeInOutQuad,
    easeInCubic,
    easeOutCubic,
    easeInOutCubic,
    easeInQuart,
    easeOutQuart,
    easeInOutQuart,
    easeInQuint,
    easeOutQuint,
    easeInOutQuint,
    easeInExpo,
    easeOutExpo,
    easeInOutExpo,
    easeInCirc,
    easeOutCirc,
    easeInOutCirc,
    easeInBack,
    easeOutBack,
    easeInOutBack,
    easeInElastic,
    easeOutElastic,
    easeInOutElastic,
    easeInBounce,
    easeOutBounce,
    easeInOutBounce,
};

EasingFunction getEasingFunction(Ease ease)
{
    return easingFunctions[static_cast<std::underlying_type_t<Ease>>(ease)];
}

} // namespace spurv

#if defined(__clang__)
# pragma clang diagnostic pop
#else
// gcc?
# pragma GCC diagnostic pop
#endif
