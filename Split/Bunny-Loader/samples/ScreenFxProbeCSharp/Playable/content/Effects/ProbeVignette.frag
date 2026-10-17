void main() {
    vec3 scene = blScreen(vUV).rgb;
    vec2 d = (vUV - 0.5) * vec2(uAspect, 1.0);
    float corner = smoothstep(0.3, 0.95, length(d));
    fragColor = vec4(scene * (1.0 - 0.3 * corner * uIntensity), 1.0);
}
