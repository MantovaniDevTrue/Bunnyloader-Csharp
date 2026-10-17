uniform float uShadowThreshold;
void main() {
    vec4 pixel = texture(uSource, vUV);
    float mask = pixel.b < uShadowThreshold
        ? pow(max(1.0 - pixel.b, 0.0), 0.75) : 0.0;
    fragColor = vec4(1.0, 1.0, 1.0, mask);
}
