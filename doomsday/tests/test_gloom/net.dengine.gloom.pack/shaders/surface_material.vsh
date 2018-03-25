#version 330 core

#include "common/defs.glsl"
#include "common/surface.glsl"
#include "common/tangentspace.glsl"

uniform mat4          uCameraMvpMatrix;
uniform mat4          uModelViewMatrix;
uniform vec4          uCameraPos; // world space
uniform samplerBuffer uTexOffsets;
uniform float         uCurrentTime;

in float aTexture0; // front material
in float aTexture1; // back material
in vec2  aIndex1; // tex offset (front, back)

     out vec2  vUV;
     out vec4  vVSPos;
     out vec3  vTSViewPos;
     out vec3  vTSFragPos;
     out vec3  vWSTangent;
     out vec3  vWSBitangent;
     out vec3  vWSNormal;
flat out uint  vMaterial;
flat out uint  vFlags;

vec4 fetchTexOffset(uint offsetIndex) {
    return texelFetch(uTexOffsets, int(offsetIndex));
}

void main(void) {
    Surface surface = Gloom_LoadVertexSurface();

    gl_Position  = uCameraMvpMatrix * surface.vertex;
    vVSPos       = uModelViewMatrix * surface.vertex;
    vUV          = aUV.xy;
    vFlags       = surface.flags;
    vWSTangent   = surface.tangent;
    vWSBitangent = surface.bitangent;
    vWSNormal    = surface.normal;
    vMaterial    = floatBitsToUint(surface.isFrontSide? aTexture0 : aTexture1);

    // Tangent space.
    {
        mat3 tbn = transpose(Gloom_TangentMatrix(
            TangentSpace(vWSTangent, vWSBitangent, vWSNormal)
        ));
        vTSViewPos = tbn * uCameraPos.xyz;
        vTSFragPos = tbn * surface.vertex.xyz;
    }

    // Generate texture coordinates.
    if (testFlag(surface.flags, Surface_WorldSpaceYToTexCoord)) {
        vUV.t = surface.vertex.y -
            (surface.isFrontSide ^^ testFlag(surface.flags, Surface_AnchorTopPlane)?
            surface.botPlaneY : surface.topPlaneY);
    }
    if (testFlag(surface.flags, Surface_WorldSpaceXZToTexCoords)) {
        vUV += aVertex.xz;
    }

    // Texture scrolling.
    if (testFlag(surface.flags, Surface_TextureOffset)) {
        vec4 texOffset = fetchTexOffset(
            floatBitsToUint(surface.isFrontSide? aIndex1.x : aIndex1.y));
        vUV += texOffset.xy + uCurrentTime * texOffset.zw;
    }

    // Texture rotation.
    if (aUV.w != 0.0) {
        float angle = radians(aUV.w);
        vUV = mat2(cos(angle), sin(angle), -sin(angle), cos(angle)) * vUV;
    }

    // Align with the left edge.
    if (!surface.isFrontSide) {
        vUV.s = surface.wallLength - vUV.s;
    }

    if (!testFlag(surface.flags, Surface_FlipTexCoordY)) {
        vUV.t = -vUV.t;
    }
}
