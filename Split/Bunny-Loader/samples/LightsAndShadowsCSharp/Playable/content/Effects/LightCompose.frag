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
    float day = clamp(uDayBlend, 0.0, 1.0);
    float moonShadow = clamp(dot(rays, vec3(0.3333333)) * 0.85, 0.0, 0.42);
    vec3 moonFill = moonlight * (0.055 * uLightStrength * uSunPower * uSunEnabled) * (0.65 + 0.35 * reach);
    vec3 moonLit = sunlight * (1.0 - moonShadow) + moonFill;
    vec3 dayLit = max(sunlight - rays, vec3(0.0));
    vec3 lit = mix(moonLit, dayLit, smoothstep(0.0, 0.18, day));
    fragColor = vec4(clamp(scene + lit, 0.0, 1.0), 1.0);
}
