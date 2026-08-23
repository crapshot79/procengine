#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in float inMaterialType;

layout(binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightSpace;
    vec3 lightDir;
    float shadowMapSize;
    vec3 lightColor;
    float pad1;
} ubo;

layout(push_constant) uniform PushConsts {
    mat4 model;
} pcs;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec4 fragLightSpacePos;
layout(location = 4) out float fragMaterialType;

void main() {
    gl_Position = ubo.proj * ubo.view * pcs.model * vec4(inPos, 1.0);
    fragColor = inColor;
    fragNormal = mat3(transpose(inverse(pcs.model))) * inNormal;
    fragPos = vec3(pcs.model * vec4(inPos, 1.0));
    fragLightSpacePos = ubo.lightSpace * pcs.model * vec4(inPos, 1.0);
    fragMaterialType = inMaterialType;
}
