uniform float uDistance;
uniform float uBlurGain;
void main() {
    vec2 d = uSourceTexel * uDistance;
    vec3 c = texture(uSource, vUV + vec2( d.x,  d.y)).rgb;
    c += texture(uSource, vUV + vec2(-d.x,  d.y)).rgb;
    c += texture(uSource, vUV + vec2( d.x, -d.y)).rgb;
    c += texture(uSource, vUV + vec2(-d.x, -d.y)).rgb;
    fragColor = vec4(c * (uBlurGain * 0.25), 1.0);
}
