#version 450
// Equivalente GLSL de card.metal para SPIR-V (Vulkan). Compilar com glslc.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec3 v_world_pos;
layout(location = 1) out vec2 v_uv;

layout(set = 1, binding = 0) uniform VertexUniforms {
  mat4 mvp;
  mat4 model;
} u;

void main() {
  gl_Position = u.mvp * vec4(in_position, 1.0);
  v_world_pos = (u.model * vec4(in_position, 1.0)).xyz;
  v_uv = in_uv;
}
