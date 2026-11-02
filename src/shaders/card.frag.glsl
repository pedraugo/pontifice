#version 450
// Equivalente GLSL de card.metal (fs_main) para SPIR-V (Vulkan). Compilar com glslc.
layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec4 v_light_pos0;
layout(location = 3) in vec4 v_light_pos1;
layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 2, binding = 1) uniform sampler2D nmap;
layout(set = 2, binding = 2) uniform sampler2DShadow shadow0;
layout(set = 2, binding = 3) uniform sampler2DShadow shadow1;
layout(set = 3, binding = 0) uniform FragmentUniforms {
  vec4 color;
  vec4 params;
  vec4 light_dir;
  vec4 light_color;
  vec4 ambient;
  vec4 point_light;
  vec4 point_light2;
  vec4 fog_color;
  vec4 fog_height;
  vec4 camera;
  vec4 resolution;
  vec4 mist;
  vec4 shadow;
} u;

float ShadowFactor(sampler2DShadow map, vec4 lp, float bias, float texel, float ndl) {
  if (lp.w <= 0.0) return 1.0;
  vec3 p = lp.xyz / lp.w;
  vec2 uv = vec2(p.x * 0.5 + 0.5, 1.0 - (p.y * 0.5 + 0.5));
  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || p.z > 1.0) return 1.0;
  float ref = p.z - bias * (1.0 + 4.0 * (1.0 - ndl));
  float sum = 0.0;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      sum += texture(map, vec3(uv + vec2(dx, dy) * texel, ref));
    }
  }
  return sum / 9.0;
}

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

vec4 Flame(vec2 uv, vec3 world_pos, float t, bool glow) {
  t += world_pos.x * 3.7 + world_pos.z * 1.3;
  float h = 1.0 - uv.y;
  float n1 = Fbm(vec2(uv.x * 2.5, uv.y * 2.0 + t * 1.5));
  float n2 = Fbm(vec2(uv.x * 5.0 + 3.1, uv.y * 3.5 + t * 2.4));
  float n3 = Fbm(vec2(uv.x * 9.0 + 7.7, uv.y * 6.0 + t * 3.6));
  float dx_raw = (uv.x - 0.5) * 2.0;
  float dx = dx_raw + (n1 - 0.5) * 1.1 * h * h + (n2 - 0.5) * 0.35 * h;
  float w = 0.9 * sqrt(max(1.0 - h, 0.0)) * (0.65 + 0.35 * clamp(h * 3.0, 0.0, 1.0));
  float r = abs(dx) / max(w, 1e-3);
  float near = clamp(1.5 - r, 0.0, 1.0);
  float body = 1.0 - r - ((n2 - 0.5) * (0.3 + 0.7 * h) + (n3 - 0.5) * 0.25 * h) * near;
  float core = clamp(1.0 - length(vec2(dx_raw * 2.2, (h - 0.15) * 3.0)), 0.0, 1.0);
  float i = clamp(clamp(body, 0.0, 1.0) + core * 0.6 * (1.0 - h), 0.0, 1.0);
  float envelope = smoothstep(1.0, 0.7, length(vec2(dx_raw, (h - 0.3) * 1.1)));
  if (glow) {
    envelope *= smoothstep(0.0, 0.35, h) *
                smoothstep(1.0, 0.3, length(vec2(dx_raw * 1.1, (h - 0.35) * 1.5)));
  }
  float alpha = smoothstep(0.02, 0.4, i) * (1.0 - smoothstep(0.75, 1.0, h)) *
                (0.75 + 0.25 * n1) * envelope;
  vec3 col = mix(vec3(0.75, 0.08, 0.02), vec3(1.0, 0.42, 0.04), smoothstep(0.0, 0.3, i));
  col = mix(col, vec3(1.0, 0.82, 0.25), smoothstep(0.3, 0.65, i));
  col = mix(col, vec3(1.0, 0.97, 0.88), smoothstep(0.65, 1.0, i) * (1.0 - h * 0.8));
  return vec4(col, alpha);
}

void main() {
  float t = u.params.z;
  float flicker = 1.0 + 0.035 * sin(t * 11.0) + 0.025 * sin(t * 6.3 + 1.7);

  vec3 rgb;
  float alpha;
  bool foil = u.params.y > 4.5;
  if (!foil && u.params.y > 2.5) {
    out_color = Flame(v_uv, v_world_pos, t, u.params.y > 3.5) * u.color;
    return;
  }
  if (!foil && u.params.y > 1.5) {
    vec4 base = u.color;
    if (u.params.x > 0.5) base = texture(tex, v_uv) * u.color;
    out_color = base;
    return;
  }
  if (!foil && u.params.y > 0.5) {
    vec2 p = (v_world_pos.xz + v_world_pos.y * 7.0) * u.mist.z;
    float n1 = Fbm(p * 0.35 + vec2(t * 0.06, t * 0.025));
    float n2 = Fbm(p * 0.7 - vec2(t * 0.04, -t * 0.05) + 31.0);
    float mist = smoothstep(0.32 - u.mist.w, 0.82 - u.mist.w, n1 * 0.65 + n2 * 0.35);
    float r = length(v_world_pos.xz);
    float falloff = 1.0 - smoothstep(u.mist.x, u.mist.y, r);
    vec3 to_point = u.point_light.xyz - v_world_pos;
    vec3 to_point2 = u.point_light2.xyz - v_world_pos;
    float s0 = 1.0, s1 = 1.0;
    if (u.shadow.x > 0.0) {
      s0 = mix(1.0, ShadowFactor(shadow0, v_light_pos0, u.shadow.y, u.shadow.z, 1.0), u.shadow.x * 0.5);
      s1 = mix(1.0, ShadowFactor(shadow1, v_light_pos1, u.shadow.y, u.shadow.z, 1.0), u.shadow.x * 0.5);
    }
    float glow = u.point_light.w * s0 / (1.0 + dot(to_point, to_point) * 0.03) +
                 u.point_light2.w * s1 / (1.0 + dot(to_point2, to_point2) * 0.03);
    rgb = u.color.rgb * (u.ambient.rgb * 2.5 + u.light_color.rgb * glow * 0.45 * flicker);
    alpha = mist * falloff * u.color.a;
  } else {
    vec4 base = u.color;
    if (u.params.x > 0.5) base = texture(tex, v_uv) * u.color;
    vec3 n = normalize(cross(dFdx(v_world_pos), dFdy(v_world_pos)));
    if (u.params.x > 1.5) {
      vec3 dp1 = dFdx(v_world_pos), dp2 = dFdy(v_world_pos);
      vec2 duv1 = dFdx(v_uv), duv2 = dFdy(v_uv);
      vec3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
      vec3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
      vec3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
      float inv_max = inversesqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
      vec3 tn = texture(nmap, v_uv).xyz * 2.0 - 1.0;
      tn.y = -tn.y;
      tn.xy *= 2.5;
      n = normalize(tangent * inv_max * tn.x + bitangent * inv_max * tn.y + n * tn.z);
    }
    float key = abs(dot(n, u.light_dir.xyz)) * 0.6 + 0.4;
    vec3 light = u.ambient.rgb + u.light_color.rgb * (u.light_dir.w * key * flicker);
    for (int k = 0; k < 2; ++k) {
      vec4 pt = k == 0 ? u.point_light : u.point_light2;
      if (pt.w <= 0.0) continue;
      vec3 to_point = pt.xyz - v_world_pos;
      float d2 = dot(to_point, to_point);
      float ndl = abs(dot(n, to_point * inversesqrt(d2)));
      float pl = ndl * 0.7 + 0.3;
      float atten = pt.w / (1.0 + d2 * 0.035);
      float shade = 1.0;
      if (u.shadow.x > 0.0) {
        shade = k == 0 ? ShadowFactor(shadow0, v_light_pos0, u.shadow.y, u.shadow.z, ndl)
                       : ShadowFactor(shadow1, v_light_pos1, u.shadow.y, u.shadow.z, ndl);
        shade = mix(1.0, shade, u.shadow.x);
      }
      light += u.light_color.rgb * (pl * atten * flicker * shade);
    }
    rgb = base.rgb * light;
    alpha = base.a;
    if (foil) {
      vec3 v = normalize(u.camera.xyz - v_world_pos);
      float fres = pow(1.0 - abs(dot(n, v)), 1.5);
      float band = dot(v_uv, vec2(2.5, 4.0)) + fres * 3.0 + t * 0.12;
      vec3 rainbow = 0.5 + 0.5 * cos(6.2831853 * (band + vec3(0.0, 0.33, 0.67)));
      float sparkle = 0.55 + 0.45 * Noise(v_uv * 60.0 + vec2(t * 0.3, -t * 0.2));
      rgb += rainbow * (0.06 + 0.38 * fres) * sparkle;
    }
  }

  // Dessaturação (ex.: carta que já atacou no turno): mistura com a luminância.
  if (u.resolution.z > 0.0) {
    float luma = dot(rgb, vec3(0.299, 0.587, 0.114));
    rgb = mix(rgb, vec3(luma), u.resolution.z);
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
