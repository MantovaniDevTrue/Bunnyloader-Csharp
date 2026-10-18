uniform sampler2D uBloom;
uniform float uBloomStrength;
uniform float uBloomPower;
void main() {
    vec3 base = texture(uSource, vUV).rgb;
    vec3 bloom = texture(uBloom, vUV).rgb;
    float luma = dot(base, vec3(0.3, 0.6, 0.1));
    float attenuation = pow(clamp(1.0 - luma, 0.0, 1.0), uBloomPower);
    fragColor = vec4(clamp(base + bloom * uBloomStrength * attenuation, 0.0, 1.0), 1.0);
}
