#version 330 core

uniform sampler2D tex;

in vec2 uv;
out vec4 color;

void main() {
    vec4 texColor = texture(tex, uv);
    color = texColor;
}
