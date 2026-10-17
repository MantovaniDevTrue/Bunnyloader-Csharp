uniform sampler2D uRays;
uniform sampler2D uGradientDay;
uniform sampler2D uGradientNight;
uniform float uDayBlend;
uniform vec2 uSun;
uniform float uClock;
uniform float uLightStrength;
uniform float uSunPower;
uniform float uSunEnabled;
void main() {
    vec3 scene = texture(uSource, vUV).rgb;
    vec2 axis = (vUV - uSun) * vec2(uAspect, 1.0);
    float reach = max(1.0 - length(axis) / 3.0, 0.0);
    vec2 gradientUV = vec2(clamp(uClock, 0.0, 1.0), 0.5);
    vec3 daylight = texture(uGradientDay, gradientUV).rgb;
    vec3 moonlight = texture(uGradientNight, gradientUV).rgb;
    vec3 sunlight = mix(moonlight, daylight, clamp(uDayBlend, 0.0, 1.0));
    sunlight *= reach * uLightStrength * uSunPower * uSunEnabled;
    vec3 rays = texture(uRays, vUV).rgb;
    float moonAmbient = (1.0 - clamp(uDayBlend, 0.0, 1.0)) * 0.22;
    vec3 lit = max(sunlight - rays, sunlight * moonAmbient);
    fragColor = vec4(clamp(scene + lit, 0.0, 1.0), 1.0);
}
