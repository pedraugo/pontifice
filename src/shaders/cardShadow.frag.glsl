#version 450
// Passe de sombra (shadow map): só profundidade, nenhuma saída de cor. Equivalente a fs_shadow
// em card.metal. Compilar com glslc.
layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec4 v_light_pos0;
layout(location = 3) in vec4 v_light_pos1;

void main() {}
