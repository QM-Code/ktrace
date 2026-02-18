#include "internal.hpp"

#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

namespace karma::renderer_backend {
namespace {

constexpr uint32_t kDiligentMaterialMaxAnisotropy = 8u;

} // namespace

bool CreateDiligentMainPipelineShadersAndConstants(
    Diligent::IRenderDevice* device,
    uint32_t constants_buffer_size,
    Diligent::RefCntAutoPtr<Diligent::IBuffer>& constant_buffer,
    Diligent::RefCntAutoPtr<Diligent::IShader>& out_vs,
    Diligent::RefCntAutoPtr<Diligent::IShader>& out_ps) {
    if (!device) {
        return false;
    }

    Diligent::ShaderCreateInfo shader_ci{};
    shader_ci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

    const char* vs_source = R"(
struct VSInput {
    float3 Pos : ATTRIB0;
    float3 Nor : ATTRIB1;
    float2 UV  : ATTRIB2;
};
struct PSInput {
    float4 Pos : SV_POSITION;
    float3 Nor : NORMAL;
    float2 UV  : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4x4 u_model;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
    float4 u_shadowParams0;
    float4 u_shadowParams1;
    float4 u_shadowParams2; // inv_depth_range, pcf_radius, clip_depth_scale, clip_depth_bias
    float4 u_shadowBiasParams; // receiver_scale, normal_scale, raster_depth_bias, raster_slope_bias
    float4 u_shadowAxisRight;
    float4 u_shadowAxisUp;
    float4 u_shadowAxisForward;
    float4 u_shadowCascadeSplits;
    float4 u_shadowCascadeWorldTexel;
    float4 u_shadowCascadeParams;
    float4 u_shadowCameraPosition;
    float4 u_shadowCameraForward;
    float4x4 u_shadowCascadeUvProj[4];
    float4 u_localLightCount;
    float4 u_localLightParams;
    float4 u_localLightPosRange[4];
    float4 u_localLightColorIntensity[4];
    float4 u_localLightShadowSlot[4];
    float4 u_pointShadowParams;
    float4 u_pointShadowAtlasTexel;
    float4 u_pointShadowTuning;
    float4x4 u_pointShadowUvProj[24];
};
PSInput main(VSInput input) {
    PSInput outp;
    outp.Pos = mul(u_modelViewProj, float4(input.Pos, 1.0));
    float4 worldPos = mul(u_model, float4(input.Pos, 1.0));
    outp.WorldPos = worldPos.xyz;
    outp.Nor = mul((float3x3)u_model, input.Nor);
    outp.UV = input.UV;
    return outp;
}
)";

    const char* ps_source = R"(
struct PSInput {
    float4 Pos : SV_POSITION;
    float3 Nor : NORMAL;
    float2 UV  : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4x4 u_model;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
    float4 u_shadowParams0;
    float4 u_shadowParams1;
    float4 u_shadowParams2;
    float4 u_shadowBiasParams;
    float4 u_shadowAxisRight;
    float4 u_shadowAxisUp;
    float4 u_shadowAxisForward;
    float4 u_shadowCascadeSplits;
    float4 u_shadowCascadeWorldTexel;
    float4 u_shadowCascadeParams;
    float4 u_shadowCameraPosition;
    float4 u_shadowCameraForward;
    float4x4 u_shadowCascadeUvProj[4];
    float4 u_localLightCount;
    float4 u_localLightParams;
    float4 u_localLightPosRange[4];
    float4 u_localLightColorIntensity[4];
    float4 u_localLightShadowSlot[4];
    float4 u_pointShadowParams;
    float4 u_pointShadowAtlasTexel;
    float4 u_pointShadowTuning;
    float4x4 u_pointShadowUvProj[24];
};
Texture2D s_tex;
SamplerState s_tex_sampler;
Texture2D s_normal;
SamplerState s_normal_sampler;
Texture2D s_occlusion;
SamplerState s_occlusion_sampler;
Texture2D<float> s_shadow;
SamplerComparisonState s_shadow_sampler;
Texture2D<float> s_pointShadow;
SamplerComparisonState s_pointShadow_sampler;

float shadowCascadeSplit(int cascadeIdx) {
    if (cascadeIdx == 0) return u_shadowCascadeSplits.x;
    if (cascadeIdx == 1) return u_shadowCascadeSplits.y;
    if (cascadeIdx == 2) return u_shadowCascadeSplits.z;
    return u_shadowCascadeSplits.w;
}

float shadowCascadeWorldTexel(int cascadeIdx) {
    if (cascadeIdx == 0) return u_shadowCascadeWorldTexel.x;
    if (cascadeIdx == 1) return u_shadowCascadeWorldTexel.y;
    if (cascadeIdx == 2) return u_shadowCascadeWorldTexel.z;
    return u_shadowCascadeWorldTexel.w;
}

float4x4 shadowCascadeUvProjection(int cascadeIdx) {
    if (cascadeIdx == 1) return u_shadowCascadeUvProj[1];
    if (cascadeIdx == 2) return u_shadowCascadeUvProj[2];
    if (cascadeIdx == 3) return u_shadowCascadeUvProj[3];
    return u_shadowCascadeUvProj[0];
}

float sampleCascadeShadowVisibility(int cascadeIdx, float3 worldPos, float ndotl) {
    if (cascadeIdx < 0 || cascadeIdx > 3) {
        return 1.0;
    }

    float4x4 uvProj = shadowCascadeUvProjection(cascadeIdx);
    float4 shadowUvDepth = mul(uvProj, float4(worldPos, 1.0));
    shadowUvDepth.xyz /= max(shadowUvDepth.w, 1e-7);
    float2 shadowUv = shadowUvDepth.xy;
    float shadowDepth = shadowUvDepth.z;
    if (shadowUv.x < 0.0 || shadowUv.x > 1.0 ||
        shadowUv.y < 0.0 || shadowUv.y > 1.0 ||
        shadowDepth < 0.0 || shadowDepth > 1.0) {
        return 1.0;
    }

    float radius = clamp(u_shadowParams2.y, 0.0, 4.0);
    float atlasTexel = max(u_shadowCascadeParams.z, 0.0);
    float worldTexel = max(shadowCascadeWorldTexel(cascadeIdx), 0.0);
    float slope = clamp(1.0 - ndotl, 0.0, 1.0);
    float bias = clamp(
        u_shadowParams0.z +
            (worldTexel * (u_shadowBiasParams.x + (u_shadowBiasParams.y * slope))),
        0.0,
        0.02);

    int iradius = int(clamp(floor(radius + 0.5), 0.0, 4.0));
    if (iradius <= 0) {
        return s_shadow.SampleCmpLevelZero(s_shadow_sampler, shadowUv, shadowDepth - bias);
    }

    float lit = 0.0;
    float count = 0.0;
    if (iradius == 1) {
        for (int oy = -1; oy <= 1; ++oy) {
            for (int ox = -1; ox <= 1; ++ox) {
                float2 uv = shadowUv + float2(float(ox), float(oy)) * atlasTexel;
                float w = 1.0 / (1.0 + float((ox * ox) + (oy * oy)));
                lit += s_shadow.SampleCmpLevelZero(s_shadow_sampler, uv, shadowDepth - bias) * w;
                count += w;
            }
        }
    } else if (iradius == 2) {
        for (int oy = -2; oy <= 2; ++oy) {
            for (int ox = -2; ox <= 2; ++ox) {
                float2 uv = shadowUv + float2(float(ox), float(oy)) * atlasTexel;
                float w = 1.0 / (1.0 + float((ox * ox) + (oy * oy)));
                lit += s_shadow.SampleCmpLevelZero(s_shadow_sampler, uv, shadowDepth - bias) * w;
                count += w;
            }
        }
    } else if (iradius == 3) {
        for (int oy = -3; oy <= 3; ++oy) {
            for (int ox = -3; ox <= 3; ++ox) {
                float2 uv = shadowUv + float2(float(ox), float(oy)) * atlasTexel;
                float w = 1.0 / (1.0 + float((ox * ox) + (oy * oy)));
                lit += s_shadow.SampleCmpLevelZero(s_shadow_sampler, uv, shadowDepth - bias) * w;
                count += w;
            }
        }
    } else {
        for (int oy = -4; oy <= 4; ++oy) {
            for (int ox = -4; ox <= 4; ++ox) {
                float2 uv = shadowUv + float2(float(ox), float(oy)) * atlasTexel;
                float w = 1.0 / (1.0 + float((ox * ox) + (oy * oy)));
                lit += s_shadow.SampleCmpLevelZero(s_shadow_sampler, uv, shadowDepth - bias) * w;
                count += w;
            }
        }
    }
    if (count < 0.5) {
        return 1.0;
    }
    return lit / count;
}

float sampleShadowVisibility(float3 worldPos, float ndotl) {
    if (u_shadowParams0.x < 0.5) {
        return 1.0;
    }
    if (ndotl <= 0.0001) {
        return 1.0;
    }

    int cascadeCount = int(clamp(floor(u_shadowCascadeParams.y + 0.5), 1.0, 4.0));
    float3 cameraForward = u_shadowCameraForward.xyz;
    float cameraForwardLen = length(cameraForward);
    if (cameraForwardLen <= 1e-6) {
        cameraForward = float3(0.0, 0.0, -1.0);
    } else {
        cameraForward /= cameraForwardLen;
    }
    float viewDepth = max(dot(worldPos - u_shadowCameraPosition.xyz, cameraForward), 0.0);

    int cascadeIdx = 0;
    if (cascadeCount > 1 && viewDepth > shadowCascadeSplit(0)) cascadeIdx = 1;
    if (cascadeCount > 2 && viewDepth > shadowCascadeSplit(1)) cascadeIdx = 2;
    if (cascadeCount > 3 && viewDepth > shadowCascadeSplit(2)) cascadeIdx = 3;

    float shadow = sampleCascadeShadowVisibility(cascadeIdx, worldPos, ndotl);
    if (cascadeIdx + 1 < cascadeCount) {
        float splitDepth = shadowCascadeSplit(cascadeIdx);
        float transitionFraction = max(u_shadowCascadeParams.x, 0.0);
        float transitionRange = max(splitDepth * transitionFraction, 0.25);
        float blend = saturate((viewDepth - (splitDepth - transitionRange)) /
                               max(transitionRange, 1e-4));
        if (blend > 0.0) {
            float shadowNext = sampleCascadeShadowVisibility(cascadeIdx + 1, worldPos, ndotl);
            shadow = lerp(shadow, shadowNext, blend);
        }
    }
    return shadow;
}

int selectPointShadowFace(float3 dirWs) {
    float3 a = abs(dirWs);
    if (a.x >= a.y && a.x >= a.z) {
        return dirWs.x >= 0.0 ? 0 : 1;
    }
    if (a.y >= a.x && a.y >= a.z) {
        return dirWs.y >= 0.0 ? 2 : 3;
    }
    return dirWs.z >= 0.0 ? 4 : 5;
}

float samplePointShadowVisibility(int localIndex, float3 worldPos, float3 geomNormal, float3 localDir) {
    if (u_pointShadowParams.x < 0.5 || localIndex < 0 || localIndex >= 4) {
        return 1.0;
    }
    int shadowSlot = int(floor(u_localLightShadowSlot[localIndex].x + 0.5));
    int activeSlots = int(clamp(floor(u_pointShadowParams.w + 0.5), 0.0, 4.0));
    if (shadowSlot < 0 || shadowSlot >= activeSlots) {
        return 1.0;
    }

    float3 toSample = worldPos - u_localLightPosRange[localIndex].xyz;
    int face = selectPointShadowFace(toSample);
    int matrixIndex = shadowSlot * 6 + face;
    if (matrixIndex < 0 || matrixIndex >= 24) {
        return 1.0;
    }

    float texelSize = max(u_pointShadowParams.y, 0.0);
    float slope = 1.0 - saturate(dot(geomNormal, localDir));
    float normalWs = texelSize * max(u_pointShadowTuning.z, 0.0) * (0.5 + slope);
    float3 shadowWorldPos = worldPos + geomNormal * normalWs;

    float4 shadowUvDepth = mul(u_pointShadowUvProj[matrixIndex], float4(shadowWorldPos, 1.0));
    shadowUvDepth.xyz /= max(shadowUvDepth.w, 1e-7);
    float2 shadowUv = shadowUvDepth.xy;
    float shadowDepth = max(shadowUvDepth.z, 1e-7);
    if (shadowUv.x < 0.0 || shadowUv.x > 1.0 ||
        shadowUv.y < 0.0 || shadowUv.y > 1.0 ||
        shadowDepth < 0.0 || shadowDepth > 1.0) {
        return 1.0;
    }

    float constBias = max(u_pointShadowTuning.x, 0.0);
    float slopeBias = texelSize * max(u_pointShadowTuning.y, 0.0) * (0.4 + slope);
    float receiverBias =
        (abs(ddx(shadowDepth)) + abs(ddy(shadowDepth))) * max(u_pointShadowTuning.w, 0.0);
    float bias = min(constBias + slopeBias + receiverBias, 0.04);

    int radius = int(clamp(floor(u_pointShadowParams.z + 0.5), 0.0, 1.0));
    if (radius <= 0) {
        return s_pointShadow.SampleCmpLevelZero(
            s_pointShadow_sampler,
            shadowUv,
            shadowDepth - bias);
    }

    float2 atlasTexel = max(u_pointShadowAtlasTexel.xy, float2(0.0, 0.0));
    float sum = 0.0;
    int count = 0;
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            if (abs(ox) > radius || abs(oy) > radius) {
                continue;
            }
            float2 uv = shadowUv + float2(float(ox), float(oy)) * atlasTexel;
            sum += s_pointShadow.SampleCmpLevelZero(
                s_pointShadow_sampler,
                uv,
                shadowDepth - bias);
            count += 1;
        }
    }
    return count > 0 ? (sum / float(count)) : 1.0;
}

float4 main(PSInput input) : SV_TARGET {
    float3 n = normalize(input.Nor);
    float4 texColor = s_tex.Sample(s_tex_sampler, input.UV);
    float normalModulation = 1.0;
    if (u_textureMode.x > 0.5) {
        float2 encodedNormal = s_normal.Sample(s_normal_sampler, input.UV).rg * 2.0 - float2(1.0, 1.0);
        float normalResponse = clamp(length(encodedNormal), 0.0, 1.0);
        normalModulation = clamp(1.0 + (0.16 * max(0.0, normalResponse - 0.18)), 1.0, 1.18);
    }
    float ao = 1.0;
    float occlusionModulation = 1.0;
    if (u_textureMode.y > 0.5) {
        ao = clamp(s_occlusion.Sample(s_occlusion_sampler, input.UV).r, 0.0, 1.0);
        occlusionModulation = clamp(0.25 + (0.75 * ao), 0.25, 1.0);
    }
    float combinedModulation = clamp(normalModulation * occlusionModulation, 0.20, 1.20);
    float4 shadedTexColor = float4(clamp(texColor.rgb * combinedModulation, 0.0, 1.0), clamp(texColor.a, 0.0, 1.0));
    float4 baseColor = u_color * shadedTexColor;
    if (u_unlit.x > 0.5) {
        return baseColor;
    }

    float ndotl = max(dot(n, normalize(u_lightDir.xyz)), 0.0);
    float shadowVis = ndotl > 0.0001 ? sampleShadowVisibility(input.WorldPos, ndotl) : 1.0;
    float shadowFactor = clamp(1.0 - (u_shadowParams0.y * (1.0 - shadowVis)), 0.0, 1.0);
    int localCount = int(clamp(floor(u_localLightCount.x + 0.5), 0.0, 4.0));
    float localDamping = max(u_localLightParams.x, 0.0);
    float localRangeExponent = max(u_localLightParams.y, 0.1);
    bool aoAffectsLocal = u_localLightParams.z > 0.5;
    float localShadowLiftStrength = max(u_localLightParams.w, 0.0);
    float3 localLightAccum = float3(0.0, 0.0, 0.0);
    float localShadowLiftEnergy = 0.0;
    for (int i = 0; i < 4; ++i) {
        if (i >= localCount) {
            continue;
        }
        float3 toLight = u_localLightPosRange[i].xyz - input.WorldPos;
        float lightDistance = length(toLight);
        float lightRange = max(u_localLightPosRange[i].w, 0.001);
        if (lightDistance >= lightRange || lightDistance <= 1e-5) {
            continue;
        }
        float3 localDir = toLight / lightDistance;
        float localNdotL = max(dot(n, localDir), 0.0);
        if (localNdotL <= 0.0) {
            continue;
        }
        float rangeAttenuation = pow(saturate(1.0 - (lightDistance / lightRange)), localRangeExponent);
        float distanceAttenuation = 1.0 / (1.0 + (localDamping * lightDistance * lightDistance));
        float attenuation = rangeAttenuation * distanceAttenuation;
        float aoMod = aoAffectsLocal ? ao : 1.0;
        float pointShadow = samplePointShadowVisibility(i, input.WorldPos, n, localDir);
        float localScalar = u_localLightColorIntensity[i].a * attenuation * localNdotL * aoMod * pointShadow;
        localLightAccum += baseColor.rgb * u_localLightColorIntensity[i].rgb * localScalar;
        float localLuminance = dot(u_localLightColorIntensity[i].rgb, float3(0.2126, 0.7152, 0.0722));
        localShadowLiftEnergy += localLuminance * localScalar;
    }

    float shadowLift = 1.0 - exp(-localShadowLiftEnergy * localShadowLiftStrength);
    float liftedShadowFactor = lerp(shadowFactor, 1.0, saturate(shadowLift));
    float3 lit = baseColor.rgb * (u_ambientColor.rgb + u_lightColor.rgb * ndotl * liftedShadowFactor);
    lit += localLightAccum;
    return float4(lit, baseColor.a);
}
)";

    shader_ci.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shader_ci.Desc.Name = "simple_vs";
    shader_ci.EntryPoint = "main";
    shader_ci.Source = vs_source;
    device->CreateShader(shader_ci, &out_vs);

    shader_ci.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    shader_ci.Desc.Name = "simple_ps";
    shader_ci.EntryPoint = "main";
    shader_ci.Source = ps_source;
    device->CreateShader(shader_ci, &out_ps);

    Diligent::BufferDesc cb_desc{};
    cb_desc.Name = "constants";
    cb_desc.Size = constants_buffer_size;
    cb_desc.Usage = Diligent::USAGE_DYNAMIC;
    cb_desc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    cb_desc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    device->CreateBuffer(cb_desc, nullptr, &constant_buffer);
    if (!constant_buffer || !out_vs || !out_ps) {
        spdlog::error("Diligent: failed to initialize renderer pipeline prerequisites");
        return false;
    }

    return true;
}

void CreateDiligentPipelineVariant(
    Diligent::IRenderDevice* device,
    Diligent::ISwapChain* swapchain,
    Diligent::IBuffer* constant_buffer,
    const char* name,
    Diligent::IShader* vs,
    Diligent::IShader* ps,
    bool alpha_blend,
    bool double_sided,
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>& out_pso,
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& out_srb) {
    if (!device || !swapchain || !vs || !ps || !constant_buffer) {
        return;
    }

    Diligent::GraphicsPipelineStateCreateInfo pso_ci{};
    pso_ci.PSODesc.Name = name;
    pso_ci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    pso_ci.GraphicsPipeline.NumRenderTargets = 1;
    pso_ci.GraphicsPipeline.RTVFormats[0] = swapchain->GetDesc().ColorBufferFormat;
    pso_ci.GraphicsPipeline.DSVFormat = swapchain->GetDesc().DepthBufferFormat;
    pso_ci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pso_ci.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
    auto& rt0 = pso_ci.GraphicsPipeline.BlendDesc.RenderTargets[0];
    rt0.BlendEnable = alpha_blend;
    rt0.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
    rt0.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    rt0.BlendOp = Diligent::BLEND_OPERATION_ADD;
    rt0.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
    rt0.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    rt0.BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;
    pso_ci.GraphicsPipeline.RasterizerDesc.CullMode =
        double_sided ? Diligent::CULL_MODE_NONE : Diligent::CULL_MODE_BACK;
    pso_ci.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;

    Diligent::LayoutElement layout[] = {
        {0, 0, 3, Diligent::VT_FLOAT32, false},
        {1, 0, 3, Diligent::VT_FLOAT32, false},
        {2, 0, 2, Diligent::VT_FLOAT32, false}
    };
    pso_ci.GraphicsPipeline.InputLayout.LayoutElements = layout;
    pso_ci.GraphicsPipeline.InputLayout.NumElements = 3;

    pso_ci.pVS = vs;
    pso_ci.pPS = ps;

    Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_PIXEL, "s_tex", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_PIXEL, "s_normal", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_PIXEL, "s_occlusion", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_PIXEL, "s_shadow", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_PIXEL, "s_pointShadow", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };
    Diligent::SamplerDesc sampler_desc{};
    sampler_desc.MinFilter = Diligent::FILTER_TYPE_ANISOTROPIC;
    sampler_desc.MagFilter = Diligent::FILTER_TYPE_ANISOTROPIC;
    sampler_desc.MipFilter = Diligent::FILTER_TYPE_ANISOTROPIC;
    sampler_desc.MaxAnisotropy = kDiligentMaterialMaxAnisotropy;
    sampler_desc.AddressU = Diligent::TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressV = Diligent::TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressW = Diligent::TEXTURE_ADDRESS_WRAP;
    Diligent::SamplerDesc shadow_sampler_desc{};
    shadow_sampler_desc.MinFilter = Diligent::FILTER_TYPE_COMPARISON_LINEAR;
    shadow_sampler_desc.MagFilter = Diligent::FILTER_TYPE_COMPARISON_LINEAR;
    shadow_sampler_desc.MipFilter = Diligent::FILTER_TYPE_COMPARISON_LINEAR;
    shadow_sampler_desc.ComparisonFunc = Diligent::COMPARISON_FUNC_LESS_EQUAL;
    shadow_sampler_desc.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
    shadow_sampler_desc.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
    shadow_sampler_desc.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;
    Diligent::ImmutableSamplerDesc samplers[] = {
        {Diligent::SHADER_TYPE_PIXEL, "s_tex_sampler", sampler_desc},
        {Diligent::SHADER_TYPE_PIXEL, "s_normal_sampler", sampler_desc},
        {Diligent::SHADER_TYPE_PIXEL, "s_occlusion_sampler", sampler_desc},
        {Diligent::SHADER_TYPE_PIXEL, "s_shadow_sampler", shadow_sampler_desc},
        {Diligent::SHADER_TYPE_PIXEL, "s_pointShadow_sampler", shadow_sampler_desc},
    };
    pso_ci.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    pso_ci.PSODesc.ResourceLayout.Variables = vars;
    pso_ci.PSODesc.ResourceLayout.NumVariables = 5;
    pso_ci.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
    pso_ci.PSODesc.ResourceLayout.NumImmutableSamplers = 5;

    device->CreateGraphicsPipelineState(pso_ci, &out_pso);
    if (!out_pso) {
        spdlog::error("Diligent: failed to create pipeline '{}'", name ? name : "<unnamed>");
        return;
    }

    if (auto* vs_var = out_pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")) {
        vs_var->Set(constant_buffer);
    } else {
        KARMA_TRACE("render.diligent", "Diligent: missing VS static var 'Constants' for {}", name ? name : "<unnamed>");
    }
    if (auto* ps_var = out_pso->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")) {
        ps_var->Set(constant_buffer);
    } else {
        KARMA_TRACE("render.diligent", "Diligent: missing PS static var 'Constants' for {}", name ? name : "<unnamed>");
    }

    out_pso->CreateShaderResourceBinding(&out_srb, true);
    if (!out_srb) {
        spdlog::error("Diligent: failed to create shader resource binding for '{}'", name ? name : "<unnamed>");
    }
}

void CreateDiligentShadowDepthPipeline(
    Diligent::IRenderDevice* device,
    Diligent::IBuffer* constant_buffer,
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>& out_pso,
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& out_srb) {
    if (!device || !constant_buffer) {
        return;
    }

    Diligent::ShaderCreateInfo shader_ci{};
    shader_ci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

    const char* vs_source = R"(
struct VSInput {
    float3 Pos : ATTRIB0;
    float3 Nor : ATTRIB1;
    float2 UV  : ATTRIB2;
};
struct VSOutput {
    float4 Pos : SV_POSITION;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4x4 u_model;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
    float4 u_shadowParams0;
    float4 u_shadowParams1;
    float4 u_shadowParams2;
    float4 u_shadowBiasParams;
    float4 u_shadowAxisRight;
    float4 u_shadowAxisUp;
    float4 u_shadowAxisForward;
};
VSOutput main(VSInput input) {
    VSOutput outp;
    float4 worldPos = mul(u_model, float4(input.Pos, 1.0));
    float3 worldNormal = normalize(mul((float3x3)u_model, input.Nor));
    float extent = max(u_shadowParams1.x, 0.001);
    float invDepthRange = max(u_shadowParams2.x, 0.0001);
    float invMapSize = max(u_shadowParams0.w, 0.0);
    float lx = dot(worldPos.xyz, u_shadowAxisRight.xyz);
    float ly = dot(worldPos.xyz, u_shadowAxisUp.xyz);
    float lz = dot(worldPos.xyz, u_shadowAxisForward.xyz);
    float depth = clamp((lz - u_shadowParams1.w) * invDepthRange, 0.0, 1.0);
    float slope = clamp(1.0 - abs(dot(worldNormal, -u_shadowAxisForward.xyz)), 0.0, 1.0);
    depth = clamp(
        depth + u_shadowBiasParams.z + (invMapSize * u_shadowBiasParams.w * slope),
        0.0,
        1.0);
    float ndcX = (lx - u_shadowParams1.y) / extent;
    float ndcY = ((ly - u_shadowParams1.z) / extent) * u_shadowAxisUp.w;
    outp.Pos = float4(ndcX, ndcY, depth * u_shadowParams2.z + u_shadowParams2.w, 1.0);
    return outp;
}
)";

    Diligent::RefCntAutoPtr<Diligent::IShader> vs;
    shader_ci.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shader_ci.Desc.Name = "shadow_depth_vs";
    shader_ci.EntryPoint = "main";
    shader_ci.Source = vs_source;
    device->CreateShader(shader_ci, &vs);

    if (!vs) {
        return;
    }

    Diligent::GraphicsPipelineStateCreateInfo pso_ci{};
    pso_ci.PSODesc.Name = "shadow_depth_pso";
    pso_ci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    pso_ci.GraphicsPipeline.NumRenderTargets = 0;
    pso_ci.GraphicsPipeline.DSVFormat = Diligent::TEX_FORMAT_D32_FLOAT;
    pso_ci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pso_ci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    pso_ci.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
    pso_ci.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
    pso_ci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
    pso_ci.GraphicsPipeline.DepthStencilDesc.DepthFunc = Diligent::COMPARISON_FUNC_LESS;

    Diligent::LayoutElement layout[] = {
        {0, 0, 3, Diligent::VT_FLOAT32, false},
        {1, 0, 3, Diligent::VT_FLOAT32, false},
        {2, 0, 2, Diligent::VT_FLOAT32, false}
    };
    pso_ci.GraphicsPipeline.InputLayout.LayoutElements = layout;
    pso_ci.GraphicsPipeline.InputLayout.NumElements = 3;
    pso_ci.pVS = vs;
    pso_ci.pPS = nullptr;

    Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_VERTEX, "Constants", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    };
    pso_ci.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    pso_ci.PSODesc.ResourceLayout.Variables = vars;
    pso_ci.PSODesc.ResourceLayout.NumVariables = 1;

    device->CreateGraphicsPipelineState(pso_ci, &out_pso);
    if (!out_pso) {
        spdlog::error("Diligent: failed to create shadow depth pipeline");
        return;
    }
    if (auto* vs_var = out_pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")) {
        vs_var->Set(constant_buffer);
    }
    out_pso->CreateShaderResourceBinding(&out_srb, true);
}

void CreateDiligentLinePipeline(
    Diligent::IRenderDevice* device,
    Diligent::ISwapChain* swapchain,
    Diligent::IBuffer* constant_buffer,
    uint32_t line_vertex_stride,
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>& out_pso,
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& out_srb,
    Diligent::RefCntAutoPtr<Diligent::IBuffer>& out_line_vertex_buffer) {
    if (!device || !swapchain || !constant_buffer) {
        return;
    }

    Diligent::ShaderCreateInfo shader_ci{};
    shader_ci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

    const char* vs_source = R"(
struct VSInput {
    float3 Pos : ATTRIB0;
    float3 Nor : ATTRIB1;
    float2 UV  : ATTRIB2;
};
struct PSInput {
    float4 Pos : SV_POSITION;
    float3 Nor : NORMAL;
    float2 UV  : TEXCOORD0;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
};
PSInput main(VSInput input) {
    PSInput outp;
    outp.Pos = mul(u_modelViewProj, float4(input.Pos, 1.0));
    outp.Nor = input.Nor;
    outp.UV = input.UV;
    return outp;
}
)";

    const char* ps_source = R"(
struct PSInput {
    float4 Pos : SV_POSITION;
    float3 Nor : NORMAL;
    float2 UV  : TEXCOORD0;
};
cbuffer Constants {
    float4x4 u_modelViewProj;
    float4 u_color;
    float4 u_lightDir;
    float4 u_lightColor;
    float4 u_ambientColor;
    float4 u_unlit;
    float4 u_textureMode;
};
Texture2D s_tex;
SamplerState s_tex_sampler;
Texture2D s_normal;
SamplerState s_normal_sampler;
Texture2D s_occlusion;
SamplerState s_occlusion_sampler;
float4 main(PSInput input) : SV_TARGET {
    float4 texColor = s_tex.Sample(s_tex_sampler, input.UV);
    return u_color * texColor;
}
)";

    Diligent::RefCntAutoPtr<Diligent::IShader> vs;
    shader_ci.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shader_ci.Desc.Name = "line_vs";
    shader_ci.EntryPoint = "main";
    shader_ci.Source = vs_source;
    device->CreateShader(shader_ci, &vs);

    Diligent::RefCntAutoPtr<Diligent::IShader> ps;
    shader_ci.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    shader_ci.Desc.Name = "line_ps";
    shader_ci.EntryPoint = "main";
    shader_ci.Source = ps_source;
    device->CreateShader(shader_ci, &ps);
    if (!vs || !ps) {
        return;
    }

    Diligent::GraphicsPipelineStateCreateInfo pso_ci{};
    pso_ci.PSODesc.Name = "line_pso";
    pso_ci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    pso_ci.GraphicsPipeline.NumRenderTargets = 1;
    pso_ci.GraphicsPipeline.RTVFormats[0] = swapchain->GetDesc().ColorBufferFormat;
    pso_ci.GraphicsPipeline.DSVFormat = swapchain->GetDesc().DepthBufferFormat;
    pso_ci.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_LINE_LIST;
    pso_ci.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
    pso_ci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
    pso_ci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;

    auto& rt0 = pso_ci.GraphicsPipeline.BlendDesc.RenderTargets[0];
    rt0.BlendEnable = true;
    rt0.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
    rt0.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    rt0.BlendOp = Diligent::BLEND_OPERATION_ADD;
    rt0.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
    rt0.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    rt0.BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;

    Diligent::LayoutElement layout[] = {
        {0, 0, 3, Diligent::VT_FLOAT32, false},
        {1, 0, 3, Diligent::VT_FLOAT32, false},
        {2, 0, 2, Diligent::VT_FLOAT32, false}
    };
    pso_ci.GraphicsPipeline.InputLayout.LayoutElements = layout;
    pso_ci.GraphicsPipeline.InputLayout.NumElements = 3;

    pso_ci.pVS = vs;
    pso_ci.pPS = ps;

    Diligent::ShaderResourceVariableDesc vars[] = {
        {Diligent::SHADER_TYPE_PIXEL, "s_tex", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_PIXEL, "s_normal", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_PIXEL, "s_occlusion", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
    };
    Diligent::SamplerDesc sampler_desc{};
    sampler_desc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
    sampler_desc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
    sampler_desc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
    sampler_desc.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;
    Diligent::ImmutableSamplerDesc samplers[] = {
        {Diligent::SHADER_TYPE_PIXEL, "s_tex_sampler", sampler_desc},
        {Diligent::SHADER_TYPE_PIXEL, "s_normal_sampler", sampler_desc},
        {Diligent::SHADER_TYPE_PIXEL, "s_occlusion_sampler", sampler_desc},
    };
    pso_ci.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    pso_ci.PSODesc.ResourceLayout.Variables = vars;
    pso_ci.PSODesc.ResourceLayout.NumVariables = 3;
    pso_ci.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
    pso_ci.PSODesc.ResourceLayout.NumImmutableSamplers = 3;

    device->CreateGraphicsPipelineState(pso_ci, &out_pso);
    if (!out_pso) {
        return;
    }
    if (auto* vs_var = out_pso->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")) {
        vs_var->Set(constant_buffer);
    }
    if (auto* ps_var = out_pso->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")) {
        ps_var->Set(constant_buffer);
    }
    out_pso->CreateShaderResourceBinding(&out_srb, true);

    Diligent::BufferDesc line_vb_desc{};
    line_vb_desc.Name = "debug_line_vb";
    line_vb_desc.Size = line_vertex_stride * 2u;
    line_vb_desc.Usage = Diligent::USAGE_DYNAMIC;
    line_vb_desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    line_vb_desc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    device->CreateBuffer(line_vb_desc, nullptr, &out_line_vertex_buffer);
}

} // namespace karma::renderer_backend
