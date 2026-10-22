void main() {
    vec3 original = blScreen(vUV).rgb;
    vec3 shift = vec3(original.b * 0.82, original.r * 0.86, original.g * 1.02);
    fragColor = vec4(mix(original, shift, 0.35 * uIntensity), 1.0);
}
