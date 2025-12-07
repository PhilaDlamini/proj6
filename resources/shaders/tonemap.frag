#version 330 core
in vec2 UV;
out vec4 fragColor;

uniform sampler2D uHDRTexture;

vec3 tonemap(vec3 x) {
    float exposure = 1.2;
    x *= exposure; //Brighten up exposure
    x = x / (1.0 + x); // Reinhard
    x = pow(x, vec3(0.95));      // subtle gamma
    return x;
}

void main() {
    vec3 hdr = texture(uHDRTexture, UV).rgb;
    vec3 ldr = tonemap(hdr);
    fragColor = vec4(hdr, 1.0);
}
