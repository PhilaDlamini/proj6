#version 330 core
in vec2 UV;
out vec4 fragColor;

uniform sampler2D uHDRTexture;

vec3 tonemap(vec3 x) {
    return x / (1.0 + x); //Reinhard operator
}

void main() {
    vec3 hdr = texture(uHDRTexture, UV).rgb;
    vec3 ldr = tonemap(hdr);
    fragColor = vec4(ldr, 1.0);
}
