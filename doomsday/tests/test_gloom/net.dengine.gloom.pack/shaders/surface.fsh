#version 330 core

#include "common/gbuffer_out.glsl"
#include "common/miplevel.glsl"

uniform sampler2D uTex;
uniform sampler2D uTextureMetrics;
uniform float     uTexelsPerMeter;

     DENG_VAR vec2  vUV;
     DENG_VAR vec3  vNormal;
flat DENG_VAR float vTexture;
flat DENG_VAR uint  vFlags;

void main(void) {
    uint texIndex = uint(vTexture + 0.5);

    vec4 uvRect    = texelFetch(uTextureMetrics, ivec2(0, texIndex), 0);
    vec4 texelSize = texelFetch(uTextureMetrics, ivec2(1, texIndex), 0);
    vec2 texScale  = vec2(uTexelsPerMeter) / texelSize.xy;

    vec2 normUV = vUV * texScale;
    vec2 uv = uvRect.xy + fract(normUV) * uvRect.zw;

    vec4 color = textureLod(uTex, uv, mipLevel(normUV, texelSize.xy) - 0.5);
    if (color.a < 0.005)
    {
        discard;
    }

    out_FragColor = color;
    GBuffer_SetFragNormal(vNormal);
}
