#include <metal_stdlib>
using namespace metal;

struct VertexIn {
  float3 position [[attribute(0)]];
  float2 uv [[attribute(1)]];
};

struct VertexOut {
  float4 position [[position]];
  float3 world_pos;
  float2 uv;
};

struct VertexUniforms {
  float4x4 mvp;
  float4x4 model;
};

// Mesma ordem e tamanho de Gpu::FragmentUniforms (só float4, sem padding).
struct FragmentUniforms {
  float4 color;        // tint; multiplica a textura ou é a cor sólida
  float4 params;       // x = 1 usa textura; y = modo (0 malha, 1 névoa); z = tempo; w = vinheta
  float4 light_dir;    // xyz direção para a luz principal (normalizada), w = intensidade
  float4 light_color;  // rgb cor da luz principal (quente, tocha)
  float4 ambient;      // rgb luz ambiente (fria)
  float4 point_light;  // xyz posição da luz pontual sobre a arena, w = intensidade (0 = off)
  float4 fog_color;    // rgb cor da névoa, w = densidade da névoa por distância
  float4 fog_height;   // x = y névoa total, y = y sem névoa, z = força máx., w = dist. inicial
  float4 camera;       // xyz posição da câmera no mundo
  float4 resolution;   // xy tamanho do alvo em pixels
  float4 mist;         // x, y = raios onde a névoa de chão começa a sumir e some; z = escala do
                       // ruído; w = densidade (desloca o limiar do ruído para baixo)
};

vertex VertexOut vs_main(VertexIn in [[stage_in]], constant VertexUniforms& u [[buffer(0)]]) {
  VertexOut out;
  out.position = u.mvp * float4(in.position, 1.0);
  out.world_pos = (u.model * float4(in.position, 1.0)).xyz;
  out.uv = in.uv;
  return out;
}

// Ruído de valor barato; suficiente para névoa rolando ao fundo.
static float Hash(float2 p) { return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }

static float Noise(float2 p) {
  const float2 i = floor(p);
  float2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  const float a = Hash(i);
  const float b = Hash(i + float2(1.0, 0.0));
  const float c = Hash(i + float2(0.0, 1.0));
  const float d = Hash(i + float2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

static float Fbm(float2 p) {
  float v = 0.0;
  float amp = 0.5;
  for (int i = 0; i < 4; ++i) {
    v += amp * Noise(p);
    p = p * 2.03 + 17.3;
    amp *= 0.5;
  }
  return v;
}

fragment float4 fs_main(VertexOut in [[stage_in]], texture2d<float> tex [[texture(0)]],
                        sampler smp [[sampler(0)]], constant FragmentUniforms& u [[buffer(0)]]) {
  const float t = u.params.z;
  // Tremular de chama: modula as luzes quentes (principal e pontual).
  const float flicker = 1.0 + 0.035 * sin(t * 11.0) + 0.025 * sin(t * 6.3 + 1.7);

  float3 rgb;
  float alpha;
  if (u.params.y > 0.5) {
    // Névoa: quad horizontal na base da arena. Alpha vem de ruído animado, com camadas em
    // alturas diferentes deslocadas (world_pos.y) para não repetirem o padrão.
    const float2 p = (in.world_pos.xz + in.world_pos.y * 7.0) * u.mist.z;
    const float n1 = Fbm(p * 0.35 + float2(t * 0.06, t * 0.025));
    const float n2 = Fbm(p * 0.7 - float2(t * 0.04, -t * 0.05) + 31.0);
    const float mist = smoothstep(0.32 - u.mist.w, 0.82 - u.mist.w, n1 * 0.65 + n2 * 0.35);
    const float r = length(in.world_pos.xz);
    const float falloff = 1.0 - smoothstep(u.mist.x, u.mist.y, r);
    // Brilho da luz pontual chega na névoa perto do centro.
    const float3 to_point = u.point_light.xyz - in.world_pos;
    const float glow = u.point_light.w / (1.0 + dot(to_point, to_point) * 0.03);
    rgb = u.color.rgb * (u.ambient.rgb * 2.5 + u.light_color.rgb * glow * 0.45 * flicker);
    alpha = mist * falloff * u.color.a;
  } else {
    float4 base = u.color;
    if (u.params.x > 0.5) base = tex.sample(smp, in.uv) * u.color;
    // Normal da face via derivadas: funciona para malhas sem normais e é dupla face.
    const float3 n = normalize(cross(dfdx(in.world_pos), dfdy(in.world_pos)));
    // Luz principal: half-lambert suave para não ficar chapado nem preto.
    const float key = abs(dot(n, u.light_dir.xyz)) * 0.6 + 0.4;
    float3 light = u.ambient.rgb + u.light_color.rgb * (u.light_dir.w * key * flicker);
    // Luz pontual quente sobre a arena: centro aceso, bordas afundando no escuro.
    if (u.point_light.w > 0.0) {
      const float3 to_point = u.point_light.xyz - in.world_pos;
      const float d2 = dot(to_point, to_point);
      const float pl = abs(dot(n, to_point * rsqrt(d2))) * 0.7 + 0.3;
      const float atten = u.point_light.w / (1.0 + d2 * 0.035);
      light += u.light_color.rgb * (pl * atten * flicker);
    }
    rgb = base.rgb * light;
    alpha = base.a;
  }

  // Névoa de altura (base da arena) combinada com névoa por distância (fundo).
  const float dist = length(in.world_pos - u.camera.xyz);
  const float df = 1.0 - exp(-max(dist - u.fog_height.w, 0.0) * u.fog_color.w);
  const float hf =
      (1.0 - smoothstep(u.fog_height.x, u.fog_height.y, in.world_pos.y)) * u.fog_height.z;
  const float fog = clamp(1.0 - (1.0 - hf) * (1.0 - df), 0.0, 1.0);
  rgb = mix(rgb, u.fog_color.rgb, fog);

  // Vinheta: escurece as bordas da tela.
  const float2 uv = in.position.xy / u.resolution.xy - 0.5;
  const float v = length(uv * float2(1.0, 0.85));
  rgb *= 1.0 - u.params.w * smoothstep(0.3, 0.8, v);

  return float4(rgb, alpha);
}
