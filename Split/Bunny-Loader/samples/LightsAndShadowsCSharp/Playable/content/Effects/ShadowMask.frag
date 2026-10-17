uniform float uShadowThreshold;
uniform float uDayBlend;
void main() {
    vec4 pixel = texture(uSource, vUV);
    float softness = mix(0.008, 0.012, clamp(uDayBlend, 0.0, 1.0));
    float edge = 1.0 - smoothstep(
        max(uShadowThreshold - softness, 0.0),
        min(uShadowThreshold + softness, 1.0),
        pixel.b);
    float mask = pow(max(1.0 - pixel.b, 0.0), 0.75) * edge;
    fragColor = vec4(1.0, 1.0, 1.0, mask);
}
