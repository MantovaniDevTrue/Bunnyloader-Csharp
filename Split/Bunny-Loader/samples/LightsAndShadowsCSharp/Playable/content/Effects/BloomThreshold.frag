uniform float uThreshold;
void main() {
    vec3 c = texture(uSource, vUV).rgb;
    float high = max(c.r, max(c.g, c.b));
    fragColor = vec4(high > uThreshold ? c : vec3(0.0), 1.0);
}
