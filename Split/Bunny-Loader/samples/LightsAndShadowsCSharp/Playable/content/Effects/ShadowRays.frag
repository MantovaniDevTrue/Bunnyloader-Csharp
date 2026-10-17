uniform float uDay;
uniform float uShadowThreshold;
uniform float uShadowStrength;
uniform float uSunStrength;
uniform float uQuality;
uniform vec2 uSun;
void main() {
    vec2 offset = vUV - uSun;
    float ray = 0.0;
    for (int i = 0; i < 30; ++i) {
        if (uDay < 0.5 && i >= 20) break;
        float j = float(i);
        float scale = uDay >= 0.5
            ? 1.0 + j / max(uQuality, 1.0) * (0.014 + uQuality * 0.01)
            : 1.0 + j * 0.01;
        float blue = texture(uSource, clamp(uSun + offset / scale, 0.0, 1.0)).b;
        float mask = blue < uShadowThreshold ? pow(max(1.0 - blue, 0.0), 0.75) : 0.0;
        float weight = uDay >= 0.5
            ? 0.55 * (30.0 - j) / 120.0 * (uShadowStrength + 0.1) * (uSunStrength * 0.5 - 0.1)
            : (20.0 - j) / 190.0;
        ray += mask * max(weight, 0.0);
    }
    fragColor = vec4(vec3(max(ray, 0.0)), 1.0);
}
