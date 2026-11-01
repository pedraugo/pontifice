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

struct FragmentUniforms {
  float4 color;   // tint; multiplica a textura ou é a cor sólida
  float4 params;  // x = 1 usa textura, 0 usa color
};

vertex VertexOut vs_main(VertexIn in [[stage_in]], constant VertexUniforms& u [[buffer(0)]]) {
  VertexOut out;
  out.position = u.mvp * float4(in.position, 1.0);
  out.world_pos = (u.model * float4(in.position, 1.0)).xyz;
  out.uv = in.uv;
  return out;
}

fragment float4 fs_main(VertexOut in [[stage_in]], texture2d<float> tex [[texture(0)]],
                        sampler smp [[sampler(0)]], constant FragmentUniforms& u [[buffer(0)]]) {
  float4 base = u.color;
  if (u.params.x > 0.5) {
    base = tex.sample(smp, in.uv) * u.color;
  }
  // Normal da face via derivadas: funciona para malhas sem normais e é dupla face.
  float3 n = normalize(cross(dfdx(in.world_pos), dfdy(in.world_pos)));
  float3 l = normalize(float3(0.25, 0.5, 1.0));
  float diffuse = abs(dot(n, l));
  float light = 0.6 + 0.4 * diffuse;
  return float4(base.rgb * light, base.a);
}
