#version 330 core

const mat4 projection = mat4(
    2, 0, 0, 0,
    0, -2, 0, 0,
    0, 0, -1, 0,
    -1, 1, 0, 1);

layout (points) in;
layout (triangle_strip, max_vertices = 5) out;

uniform vec2 pos;
uniform vec2 size;

out vec2 uv;

void setPos(vec2 offset) {
    gl_Position = projection * vec4(pos + offset, 0, 1);
}

void main() {
    setPos(vec2(0, 0));
    uv = vec2(0, 0);
    EmitVertex();

    setPos(vec2(0, size.y));
    uv = vec2(0, 1);
    EmitVertex();

    setPos(size);
    uv = vec2(1, 1);
    EmitVertex();

    setPos(vec2(0, 0));
    uv = vec2(0, 0);
    EmitVertex();

    setPos(vec2(size.x, 0));
    uv = vec2(1, 0);
    EmitVertex();

    EndPrimitive();
}
