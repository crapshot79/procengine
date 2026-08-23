#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in float inMaterialType;

layout(binding = 0) uniform LightSpace {
    mat4 lightSpace;
} ls;

layout(push_constant) uniform PushConsts {
    mat4 model;
} pcs;

void main() {
    gl_Position = ls.lightSpace * pcs.model * vec4(inPos, 1.0);
}