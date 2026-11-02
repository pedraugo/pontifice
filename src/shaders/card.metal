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
  float4 light_pos0;  // posição em clip da luz 0 (shadow map)
  float4 light_pos1;
};

struct VertexUniforms {
  float4x4 mvp;
  float4x4 model;
  float4x4 light_vp0;
  float4x4 light_vp1;
};

// Mesma ordem e tamanho de Gpu::FragmentUniforms (só float4, sem padding).
struct FragmentUniforms {
  float4 color;        // tint; multiplica a textura ou é a cor sólida
  float4 params;       // x = textura (0 não, 1 sim, 2 sim + normal map); y = modo (0 malha,
                       // 1 névoa, 2 emissivo, 3 chama, 4 halo da chama, 5 malha com foil,
                       // 6 aura da carta foil);
                       // z = tempo; w = vinheta
  float4 light_dir;    // xyz direção para a luz principal (normalizada), w = intensidade
  float4 light_color;  // rgb cor da luz principal (quente, tocha)
  float4 ambient;      // rgb luz ambiente (fria)
  float4 point_light;  // xyz posição da luz pontual (tocha), w = intensidade (0 = off)
  float4 point_light2; // segunda luz pontual, mesma cor
  float4 fog_color;    // rgb cor da névoa, w = densidade da névoa por distância
  float4 fog_height;   // x = y névoa total, y = y sem névoa, z = força máx., w = dist. inicial
  float4 camera;       // xyz posição da câmera no mundo
  float4 resolution;   // xy tamanho do alvo em pixels; z dessaturação (0 cor, 1 preto e branco)
  float4 mist;         // x, y = raios onde a névoa de chão começa a sumir e some; z = escala do
                       // ruído; w = densidade (desloca o limiar do ruído para baixo)
  float4 shadow;       // x = força (0 = sem sombra), y = bias, z = 1 / tamanho do mapa
};

vertex VertexOut vs_main(VertexIn in [[stage_in]], constant VertexUniforms& u [[buffer(0)]]) {
  VertexOut out;
  out.position = u.mvp * float4(in.position, 1.0);
  out.world_pos = (u.model * float4(in.position, 1.0)).xyz;
  out.uv = in.uv;
  out.light_pos0 = u.light_vp0 * float4(out.world_pos, 1.0);
  out.light_pos1 = u.light_vp1 * float4(out.world_pos, 1.0);
  return out;
}

// Passe de sombra: só profundidade.
fragment void fs_shadow(VertexOut in [[stage_in]]) {}

// Fração iluminada (0 = sombra) pelo shadow map de uma luz: PCF 3x3 com sampler de comparação.
// ndl = |n . direção da luz|, para o bias crescer onde a luz chega de raspão.
static float ShadowFactor(depth2d<float> map, sampler smp, float4 lp, float bias, float texel,
                          float ndl) {
  if (lp.w <= 0.0) return 1.0;
  const float3 p = lp.xyz / lp.w;
  const float2 uv = float2(p.x * 0.5 + 0.5, 1.0 - (p.y * 0.5 + 0.5));
  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || p.z > 1.0) return 1.0;
  const float ref = p.z - bias * (1.0 + 4.0 * (1.0 - ndl));
  float sum = 0.0;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      sum += map.sample_compare(smp, uv + float2(dx, dy) * texel, ref);
    }
  }
  return sum / 9.0;
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

static float3 Rainbow(float x) {
  return 0.5 + 0.5 * cos(6.2831853 * (x + float3(0.0, 0.33, 0.67)));
}

// Aura em volta da carta foil (CardAura): uv = distância além da borda da carta por eixo,
// 0 junto à carta e 1 na ponta. Energia que flui (dois ruídos que sobem em sentidos
// diferentes, no espaço do mundo) e pulsa; branco-dourada junto à borda, arco-íris que gira
// com o tempo para fora; borda externa irregular, como chamas suaves.
static float4 Aura(float2 uv, float3 world_pos, float t) {
  const float d = saturate(length(uv));
  const float2 p = float2(world_pos.x + world_pos.z * 0.6, world_pos.y + world_pos.z * 0.4);
  const float flow = Fbm(p * 4.0 + float2(0.0, -t * 0.8));
  const float flow2 = Fbm(p * 7.5 + float2(t * 0.45, t * 0.3) + 17.0);
  const float pulse = 0.82 + 0.18 * sin(t * 2.6);
  const float core = pow(1.0 - d, 2.4);
  const float tongues = 1.0 - smoothstep(0.25 + 0.55 * flow, 0.95, d);
  const float alpha = (core * 0.95 + tongues * 0.35) * (0.55 + 0.7 * flow2) * pulse;
  const float3 rainbow = mix(Rainbow(t * 0.07 + flow * 0.8 + d * 0.6), float3(1.0), 0.25);
  const float3 col = mix(float3(1.0, 0.95, 0.8), rainbow * 1.1, smoothstep(0.2, 0.85, d));
  return float4(col, saturate(alpha));
}

fragment float4 fs_main(VertexOut in [[stage_in]], texture2d<float> tex [[texture(0)]],
                        sampler smp [[sampler(0)]], texture2d<float> nmap [[texture(1)]],
                        sampler nsmp [[sampler(1)]], depth2d<float> shadow0 [[texture(2)]],
                        sampler ssmp0 [[sampler(2)]], depth2d<float> shadow1 [[texture(3)]],
                        sampler ssmp1 [[sampler(3)]], constant FragmentUniforms& u [[buffer(0)]]) {
  const float t = u.params.z;
  // Tremular de chama: modula as luzes quentes (principal e pontual).
  const float flicker = 1.0 + 0.035 * sin(t * 11.0) + 0.025 * sin(t * 6.3 + 1.7);

  float3 rgb;
  float alpha;
  if (u.params.y > 5.5) {
    const float4 aura = Aura(in.uv, in.world_pos, t) * u.color;
    // Vinheta também na aura, para não brilhar no escuro das bordas da tela.
    const float2 suv = in.position.xy / u.resolution.xy - 0.5;
    const float vig = 1.0 - u.params.w * smoothstep(0.3, 0.8, length(suv * float2(1.0, 0.85)));
    return float4(aura.rgb * vig, aura.a * (1.0 - u.resolution.z));
  }
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
    // Brilho das luzes pontuais chega na névoa perto delas (menos onde há sombra).
    const float3 to_point = u.point_light.xyz - in.world_pos;
    const float3 to_point2 = u.point_light2.xyz - in.world_pos;
    float s0 = 1.0, s1 = 1.0;
    if (u.shadow.x > 0.0) {
      // Na névoa a sombra é mais suave (metade da força): ela é volume, não superfície.
      s0 = mix(1.0, ShadowFactor(shadow0, ssmp0, in.light_pos0, u.shadow.y, u.shadow.z, 1.0), u.shadow.x * 0.5);
      s1 = mix(1.0, ShadowFactor(shadow1, ssmp1, in.light_pos1, u.shadow.y, u.shadow.z, 1.0), u.shadow.x * 0.5);
    }
    const float glow = u.point_light.w * s0 / (1.0 + dot(to_point, to_point) * 0.03) +
                       u.point_light2.w * s1 / (1.0 + dot(to_point2, to_point2) * 0.03);
    rgb = u.color.rgb * (u.ambient.rgb * 2.5 + u.light_color.rgb * glow * 0.45 * flicker);
    alpha = mist * falloff * u.color.a;
  } else {
    float4 base = u.color;
    if (u.params.x > 0.5) base = tex.sample(smp, in.uv) * u.color;
    // Normal da face via derivadas: funciona para malhas sem normais e é dupla face.
    float3 n = normalize(cross(dfdx(in.world_pos), dfdy(in.world_pos)));
    if (u.params.x > 1.5) {
      // Normal map sem tangentes nos vértices: referencial tangente pelas derivadas de posição
      // e UV (cotangent frame). O mapa é OpenGL (verde = +V para cima na imagem), e o V da
      // textura cresce para baixo, então o verde é invertido.
      const float3 dp1 = dfdx(in.world_pos), dp2 = dfdy(in.world_pos);
      const float2 duv1 = dfdx(in.uv), duv2 = dfdy(in.uv);
      const float3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
      const float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
      const float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
      const float inv_max = rsqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
      float3 tn = nmap.sample(nsmp, in.uv).xyz * 2.0 - 1.0;
      tn.y = -tn.y;
      tn.xy *= 2.5;  // intensidade do relevo (os mapas são suaves)
      n = normalize(tangent * inv_max * tn.x + bitangent * inv_max * tn.y + n * tn.z);
    }
    // Luz principal: half-lambert suave para não ficar chapado nem preto.
    const float key = abs(dot(n, u.light_dir.xyz)) * 0.6 + 0.4;
    float3 light = u.ambient.rgb + u.light_color.rgb * (u.light_dir.w * key * flicker);
    // Luzes pontuais quentes (tochas): perto aceso, longe afundando no escuro; cada uma com
    // o próprio shadow map.
    for (int k = 0; k < 2; ++k) {
      const float4 pt = k == 0 ? u.point_light : u.point_light2;
      if (pt.w <= 0.0) continue;
      const float3 to_point = pt.xyz - in.world_pos;
      const float d2 = dot(to_point, to_point);
      const float ndl = abs(dot(n, to_point * rsqrt(d2)));
      const float pl = ndl * 0.7 + 0.3;
      const float atten = pt.w / (1.0 + d2 * 0.035);
      float shade = 1.0;
      if (u.shadow.x > 0.0) {
        shade = k == 0 ? ShadowFactor(shadow0, ssmp0, in.light_pos0, u.shadow.y, u.shadow.z, ndl)
                       : ShadowFactor(shadow1, ssmp1, in.light_pos1, u.shadow.y, u.shadow.z, ndl);
        shade = mix(1.0, shade, u.shadow.x);
      }
      light += u.light_color.rgb * (pl * atten * flicker * shade);
    }
    rgb = base.rgb * light;
    alpha = base.a;
    if (foil) {
      // Foil holográfico. Referencial da carta pela geometria (sem o relevo do normal map):
      // normal e eixos tangentes (U, V) pelas derivadas, para saber para que lado e quanto a
      // carta está inclinada em relação ao olho.
      const float3 view = normalize(u.camera.xyz - in.world_pos);
      const float3 dp1 = dfdx(in.world_pos), dp2 = dfdy(in.world_pos);
      const float2 duv1 = dfdx(in.uv), duv2 = dfdy(in.uv);
      const float3 ng = normalize(cross(dp1, dp2));
      const float3 dp2perp = cross(dp2, ng), dp1perp = cross(ng, dp1);
      const float3 axis_u = normalize(dp2perp * duv1.x + dp1perp * duv2.x);
      const float3 axis_v = normalize(dp2perp * duv1.y + dp1perp * duv2.y);
      // Inclinação: o olhar projetado no plano da carta (0 de frente, 1 de raspão).
      const float2 slant = float2(dot(view, axis_u), dot(view, axis_v));
      const float tilt = saturate(length(slant));
      // De frente o arco-íris é discreto; inclinando ele domina a carta.
      const float strength = 0.1 + 0.8 * smoothstep(0.08, 0.55, tilt);
      // Faixas: o arco-íris corre pela carta para o lado em que ela inclina, com o tempo e
      // com o relevo (moldura e entalhes mudam a cor localmente).
      const float3 bump = n - ng;
      const float band = dot(in.uv, float2(1.4, 2.2)) + dot(slant, float2(2.6, 1.8)) +
                         2.0 * (dot(bump, axis_u) + dot(bump, axis_v)) + t * 0.05;
      const float3 rainbow = Rainbow(band);
      // Textura holográfica: riscas bem finas e granulado, discretos.
      const float lines = 0.88 + 0.12 * sin((in.uv.x + in.uv.y) * 420.0 + slant.x * 30.0);
      const float grain = 0.75 + 0.25 * Noise(in.uv * 70.0 + slant * 9.0);
      // O arco-íris tinge a cor (multiplica) mais um pouco de brilho próprio: o branco vira
      // cor viva e o texto escuro continua escuro e legível.
      const float3 holo = rgb * (0.35 + 1.3 * rainbow) + rainbow * 0.22;
      rgb = mix(rgb, holo, saturate(strength * lines * grain));
      // Reflexo que varre a carta em diagonal de tempos em tempos (e acompanha a inclinação).
      const float sweep = fract(t * 0.16) * 2.8 - 0.9;
      const float gd = dot(in.uv, float2(0.55, 0.45)) + slant.x * 0.35 - sweep;
      rgb += float3(1.0, 0.97, 0.9) * exp(-gd * gd * 160.0) * 0.45;
      // Estrelas que piscam: uma por célula sorteada, em cruz, mais fortes inclinando.
      const float2 grid = float2(12.0, 17.0);
      const float2 cell = floor(in.uv * grid);
      const float h = Hash(cell);
      const float2 offset = float2(Hash(cell + 3.1), Hash(cell + 7.7)) * 0.6 + 0.2;
      const float2 sp = fract(in.uv * grid) - offset;
      const float twinkle = pow(saturate(sin(t * 2.2 + h * 37.0)), 14.0) * step(0.7, h);
      const float star = saturate(1.0 - length(sp) * 10.0) +
                         0.6 * max(saturate(1.0 - abs(sp.x) * 45.0) * saturate(1.0 - abs(sp.y) * 5.0),
                                   saturate(1.0 - abs(sp.y) * 45.0) * saturate(1.0 - abs(sp.x) * 5.0));
      rgb += float3(1.0, 0.98, 0.92) * star * twinkle * (0.5 + 0.8 * tilt);
    }
  }

  // Dessaturação (ex.: carta que já atacou no turno): mistura com a luminância.
  if (u.resolution.z > 0.0) {
    const float luma = dot(rgb, float3(0.299, 0.587, 0.114));
    rgb = mix(rgb, float3(luma), u.resolution.z);
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
