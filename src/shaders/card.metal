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
  float4 params;       // x = 1 usa textura; y = modo (0 malha, 1 névoa, 2 emissivo, 3 chama,
                       // 4 halo da chama, 5 malha com foil); z = tempo; w = vinheta
  float4 light_dir;    // xyz direção para a luz principal (normalizada), w = intensidade
  float4 light_color;  // rgb cor da luz principal (quente, tocha)
  float4 ambient;      // rgb luz ambiente (fria)
  float4 point_light;  // xyz posição da luz pontual (tocha), w = intensidade (0 = off)
  float4 point_light2; // segunda luz pontual, mesma cor
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

// Chama procedural num quad: u = largura (0..1), v = 0 no topo e 1 na base. Perfil em gota
// deformado por três camadas de ruído que sobem em velocidades diferentes; línguas se soltam
// no topo; núcleo claro perto da base. Fase varia com a posição no mundo para duas tochas não
// tremularem iguais.
static float4 Flame(float2 uv, float3 world_pos, float t, bool glow) {
  t += world_pos.x * 3.7 + world_pos.z * 1.3;
  const float h = 1.0 - uv.y;  // 0 na base, 1 no topo
  const float n1 = Fbm(float2(uv.x * 2.5, uv.y * 2.0 + t * 1.5));
  const float n2 = Fbm(float2(uv.x * 5.0 + 3.1, uv.y * 3.5 + t * 2.4));
  const float n3 = Fbm(float2(uv.x * 9.0 + 7.7, uv.y * 6.0 + t * 3.6));
  const float dx_raw = (uv.x - 0.5) * 2.0;
  const float dx = dx_raw + (n1 - 0.5) * 1.1 * h * h + (n2 - 0.5) * 0.35 * h;
  const float w = 0.9 * sqrt(max(1.0 - h, 0.0)) * (0.65 + 0.35 * saturate(h * 3.0));
  const float r = abs(dx) / max(w, 1e-3);
  // O ruído só mexe perto da silhueta (r < 1,5): longe do eixo a chama não pode renascer.
  const float near = saturate(1.5 - r) ;
  float body = 1.0 - r - ((n2 - 0.5) * (0.3 + 0.7 * h) + (n3 - 0.5) * 0.25 * h) * near;
  const float core = saturate(1.0 - length(float2(dx_raw * 2.2, (h - 0.15) * 3.0)));
  const float i = saturate(saturate(body) + core * 0.6 * (1.0 - h));
  // Envelope oval no quad: nada de resíduo (ruído ou núcleo) chega às bordas.
  float envelope = smoothstep(1.0, 0.7, length(float2(dx_raw, (h - 0.3) * 1.1)));
  if (glow) {
    // Halo: mancha oval difusa, sem borda reta na base nem nas laterais.
    envelope *= smoothstep(0.0, 0.35, h) *
                smoothstep(1.0, 0.3, length(float2(dx_raw * 1.1, (h - 0.35) * 1.5)));
  }
  const float alpha = smoothstep(0.02, 0.4, i) * (1.0 - smoothstep(0.75, 1.0, h)) *
                      (0.75 + 0.25 * n1) * envelope;
  float3 col = mix(float3(0.75, 0.08, 0.02), float3(1.0, 0.42, 0.04), smoothstep(0.0, 0.3, i));
  col = mix(col, float3(1.0, 0.82, 0.25), smoothstep(0.3, 0.65, i));
  col = mix(col, float3(1.0, 0.97, 0.88), smoothstep(0.65, 1.0, i) * (1.0 - h * 0.8));
  return float4(col, alpha);
}

fragment float4 fs_main(VertexOut in [[stage_in]], texture2d<float> tex [[texture(0)]],
                        sampler smp [[sampler(0)]], constant FragmentUniforms& u [[buffer(0)]]) {
  const float t = u.params.z;
  // Tremular de chama: modula as luzes quentes (principal e pontual).
  const float flicker = 1.0 + 0.035 * sin(t * 11.0) + 0.025 * sin(t * 6.3 + 1.7);

  float3 rgb;
  float alpha;
  const bool foil = u.params.y > 4.5;
  if (!foil && u.params.y > 2.5) return Flame(in.uv, in.world_pos, t, u.params.y > 3.5) * u.color;
  if (!foil && u.params.y > 1.5) {
    // Emissivo: cor da textura direto, sem luz, névoa nem vinheta.
    float4 base = u.color;
    if (u.params.x > 0.5) base = tex.sample(smp, in.uv) * u.color;
    return base;
  }
  if (!foil && u.params.y > 0.5) {
    // Névoa: quad horizontal na base da arena. Alpha vem de ruído animado, com camadas em
    // alturas diferentes deslocadas (world_pos.y) para não repetirem o padrão.
    const float2 p = (in.world_pos.xz + in.world_pos.y * 7.0) * u.mist.z;
    const float n1 = Fbm(p * 0.35 + float2(t * 0.06, t * 0.025));
    const float n2 = Fbm(p * 0.7 - float2(t * 0.04, -t * 0.05) + 31.0);
    const float mist = smoothstep(0.32 - u.mist.w, 0.82 - u.mist.w, n1 * 0.65 + n2 * 0.35);
    const float r = length(in.world_pos.xz);
    const float falloff = 1.0 - smoothstep(u.mist.x, u.mist.y, r);
    // Brilho das luzes pontuais chega na névoa perto delas.
    const float3 to_point = u.point_light.xyz - in.world_pos;
    const float3 to_point2 = u.point_light2.xyz - in.world_pos;
    const float glow = u.point_light.w / (1.0 + dot(to_point, to_point) * 0.03) +
                       u.point_light2.w / (1.0 + dot(to_point2, to_point2) * 0.03);
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
    // Luzes pontuais quentes (tochas): perto aceso, longe afundando no escuro.
    for (int k = 0; k < 2; ++k) {
      const float4 pt = k == 0 ? u.point_light : u.point_light2;
      if (pt.w <= 0.0) continue;
      const float3 to_point = pt.xyz - in.world_pos;
      const float d2 = dot(to_point, to_point);
      const float pl = abs(dot(n, to_point * rsqrt(d2))) * 0.7 + 0.3;
      const float atten = pt.w / (1.0 + d2 * 0.035);
      light += u.light_color.rgb * (pl * atten * flicker);
    }
    rgb = base.rgb * light;
    alpha = base.a;
    if (foil) {
      // Foil: faixas de arco-íris em diagonal que deslizam com o ângulo de visão (mais fortes
      // de raspão) e com o tempo, com um granulado fino de brilhos.
      const float3 v = normalize(u.camera.xyz - in.world_pos);
      const float fres = pow(1.0 - abs(dot(n, v)), 1.5);
      const float band = dot(in.uv, float2(2.5, 4.0)) + fres * 3.0 + t * 0.12;
      const float3 rainbow = 0.5 + 0.5 * cos(6.2831853 * (band + float3(0.0, 0.33, 0.67)));
      const float sparkle = 0.55 + 0.45 * Noise(in.uv * 60.0 + float2(t * 0.3, -t * 0.2));
      rgb += rainbow * (0.06 + 0.38 * fres) * sparkle;
    }
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
