uniform sampler2D uRays;
uniform sampler2D uGradient;
uniform vec2 uSun;
uniform float uClock;
uniform float uLightStrength;
uniform float uSunEnabled;
void main() {
    vec3 scene = texture(uSource, vUV).rgb;
    vec2 axis = (vUV - uSun) * vec2(uAspect, 1.0);
    float reach = max(1.0 - length(axis) / 3.0, 0.0);
    vec3 sunlight = texture(uGradient, vec2(clamp(uClock, 0.0, 1.0), 0.5)).rgb;
    sunlight *= reach * uLightStrength * uSunEnabled;
    vec3 rays = texture(uRays, vUV).rgb;
    fragColor = vec4(clamp(scene + max(sunlight - rays, 0.0), 0.0, 1.0), 1.0);
}
