uniform float uDay;
uniform float uShadowStrength;
uniform float uSunStrength;
uniform float uQuality;
uniform vec2 uSun;
void main() {
    vec2 offset = vUV - uSun;
    float dayFactor = clamp(uDay, 0.0, 1.0);
    float ray = 0.0;
    for (int i = 0; i < 30; ++i) {
        if (dayFactor < 0.015 && i >= 20) break;
        float j = float(i);
        float dayScale = 1.0 + j / max(uQuality, 1.0) * (0.014 + uQuality * 0.01);
        float nightScale = 1.0 + j * 0.01;
        float scale = mix(nightScale, dayScale, dayFactor);
        float mask = texture(uSource, clamp(uSun + offset / scale, 0.0, 1.0)).a;
        float dayWeight = 0.55 * (30.0 - j) / 120.0
            * (uShadowStrength + 0.1) * (uSunStrength * 0.5 - 0.1);
        float nightWeight = i < 20 ? (20.0 - j) / 190.0 : 0.0;
        ray += mask * max(mix(nightWeight, dayWeight, dayFactor), 0.0);
    }
    fragColor = vec4(vec3(max(ray, 0.0)), 1.0);
}
