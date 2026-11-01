#version 450
layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 3, binding = 0) uniform FragmentUniforms {
  vec4 color;
  vec4 params;
} u;

void main() {
  vec4 base = u.color;
  if (u.params.x > 0.5) base = texture(tex, v_uv) * u.color;
  vec3 n = normalize(cross(dFdx(v_world_pos), dFdy(v_world_pos)));
  vec3 l = normalize(vec3(0.25, 0.5, 1.0));
  float diffuse = abs(dot(n, l));
  float light = 0.6 + 0.4 * diffuse;
  out_color = vec4(base.rgb * light, base.a);
}
