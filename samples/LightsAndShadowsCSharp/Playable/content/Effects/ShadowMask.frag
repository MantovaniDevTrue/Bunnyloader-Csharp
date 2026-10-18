uniform float uDayShadowThreshold;
uniform float uNightShadowThreshold;
uniform float uDayShadowSoftness;
uniform float uNightShadowSoftness;
uniform float uDayBlend;
uniform float uSceneShadowThreshold;
uniform float uSunEnabled;
void main() {
    if (uSunEnabled <= 0.0) {
        fragColor = vec4(1.0, 1.0, 1.0, 0.0);
        return;
    }
    vec4 pixel = texture(uSource, vUV);
    float day = clamp(uDayBlend, 0.0, 1.0);
    float softness = mix(uNightShadowSoftness, uDayShadowSoftness, day);
    float baseline = mix(0.02, 0.98, day);
    float uShadowThreshold = clamp(
        mix(uNightShadowThreshold, uDayShadowThreshold, day)
        + uSceneShadowThreshold - baseline, 0.0, 1.0);
    float edge = 1.0 - smoothstep(
        max(uShadowThreshold - softness, 0.0),
        min(uShadowThreshold + softness, 1.0),
        pixel.b);
    float mask = pow(max(1.0 - pixel.b, 0.0), 0.75) * edge;
    fragColor = vec4(1.0, 1.0, 1.0, mask);
}
