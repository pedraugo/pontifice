#version 450
// Equivalente GLSL de card.metal (fs_main) para SPIR-V (Vulkan). Compilar com glslc.
layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 3, binding = 0) uniform FragmentUniforms {
  vec4 color;
  vec4 params;
  vec4 light_dir;
  vec4 light_color;
  vec4 ambient;
  vec4 point_light;
  vec4 fog_color;
  vec4 fog_height;
  vec4 camera;
  vec4 resolution;
} u;

float Hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float Noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = Hash(i);
  float b = Hash(i + vec2(1.0, 0.0));
  float c = Hash(i + vec2(0.0, 1.0));
  float d = Hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float Fbm(vec2 p) {
  float v = 0.0;
  float amp = 0.5;
  for (int i = 0; i < 4; ++i) {
    v += amp * Noise(p);
    p = p * 2.03 + 17.3;
    amp *= 0.5;
  }
  return v;
}

void main() {
  float t = u.params.z;
  float flicker = 1.0 + 0.035 * sin(t * 11.0) + 0.025 * sin(t * 6.3 + 1.7);

  vec3 rgb;
  float alpha;
  if (u.params.y > 0.5) {
    vec2 p = v_world_pos.xz + v_world_pos.y * 7.0;
    float n1 = Fbm(p * 0.35 + vec2(t * 0.06, t * 0.025));
    float n2 = Fbm(p * 0.7 - vec2(t * 0.04, -t * 0.05) + 31.0);
    float mist = smoothstep(0.32, 0.82, n1 * 0.65 + n2 * 0.35);
    float r = length(v_world_pos.xz);
    float falloff = 1.0 - smoothstep(8.0, 28.0, r);
    vec3 to_point = u.point_light.xyz - v_world_pos;
    float glow = u.point_light.w / (1.0 + dot(to_point, to_point) * 0.03);
    rgb = u.color.rgb * (u.ambient.rgb * 2.5 + u.light_color.rgb * glow * 0.45 * flicker);
    alpha = mist * falloff * u.color.a;
  } else {
    vec4 base = u.color;
    if (u.params.x > 0.5) base = texture(tex, v_uv) * u.color;
    vec3 n = normalize(cross(dFdx(v_world_pos), dFdy(v_world_pos)));
    float key = abs(dot(n, u.light_dir.xyz)) * 0.6 + 0.4;
    vec3 light = u.ambient.rgb + u.light_color.rgb * (u.light_dir.w * key * flicker);
    if (u.point_light.w > 0.0) {
      vec3 to_point = u.point_light.xyz - v_world_pos;
      float d2 = dot(to_point, to_point);
      float pl = abs(dot(n, to_point * inversesqrt(d2))) * 0.7 + 0.3;
      float atten = u.point_light.w / (1.0 + d2 * 0.035);
      light += u.light_color.rgb * (pl * atten * flicker);
    }
    rgb = base.rgb * light;
    alpha = base.a;
  }

  float dist = length(v_world_pos - u.camera.xyz);
  float df = 1.0 - exp(-max(dist - u.fog_height.w, 0.0) * u.fog_color.w);
  float hf = (1.0 - smoothstep(u.fog_height.x, u.fog_height.y, v_world_pos.y)) * u.fog_height.z;
  float fog = clamp(1.0 - (1.0 - hf) * (1.0 - df), 0.0, 1.0);
  rgb = mix(rgb, u.fog_color.rgb, fog);

  vec2 uv = gl_FragCoord.xy / u.resolution.xy - 0.5;
  float v = length(uv * vec2(1.0, 0.85));
  rgb *= 1.0 - u.params.w * smoothstep(0.3, 0.8, v);

  out_color = vec4(rgb, alpha);
}
