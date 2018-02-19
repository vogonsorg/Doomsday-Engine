#ifndef GLOOM_GBUFFER_H
#define GLOOM_GBUFFER_H

layout (location = 1) out vec4 out_Normal;

uniform mat3 uWorldToViewMatrix;

void GBuffer_SetFragmentNormal(vec3 worldNormal)
{
    out_Normal = vec4(normalize(uWorldToViewMatrix * worldNormal) * 0.5 + vec3(0.5), 1.0);
}

#endif // GLOOM_GBUFFER_H
