#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec4 fragLightSpacePos;
layout(location = 4) in float fragMaterialType;

layout(binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightSpace;
    vec3 lightDir;
    float shadowMapSize;
    vec3 lightColor;
    float fogDensity;
    vec3 cameraPos;
    float pad1;
} ubo;

layout(binding = 1) uniform sampler2D shadowMap;

layout(location = 0) out vec4 outColor;

float hash2D(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float hash3D(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float valueNoise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash2D(i);
    float b = hash2D(i + vec2(1.0, 0.0));
    float c = hash2D(i + vec2(0.0, 1.0));
    float d = hash2D(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float valueNoise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    float a = hash3D(i);
    float b = hash3D(i + vec3(1, 0, 0));
    float c = hash3D(i + vec3(0, 1, 0));
    float d = hash3D(i + vec3(1, 1, 0));
    float e = hash3D(i + vec3(0, 0, 1));
    float ff = hash3D(i + vec3(1, 0, 1));
    float g = hash3D(i + vec3(0, 1, 1));
    float h = hash3D(i + vec3(1, 1, 1));
    return mix(
        mix(mix(a, b, u.x), mix(c, d, u.x), u.y),
        mix(mix(e, ff, u.x), mix(g, h, u.x), u.y),
        u.z);
}

float fbm2D(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * valueNoise2D(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

float fbm3D(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 3; i++) {
        v += a * valueNoise3D(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

float calculateShadow(vec4 lightSpacePos) {
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0) return 0.0;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    float bias = max(0.002 * (1.0 - dot(normalize(fragNormal), normalize(ubo.lightDir))), 0.001);

    vec2 texelSize = vec2(1.0) / vec2(ubo.shadowMapSize);
    float shadow = 0.0;
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 25.0;

    return shadow;
}

vec3 terrainMaterial(vec3 pos, vec3 baseColor) {
    float broad = fbm2D(pos.xz * 0.055);
    float terrainPatch = fbm2D(pos.xz * 0.22 + 7.3);
    float fine = fbm2D(pos.xz * 1.4 + 21.0);
    float slope = 1.0 - abs(normalize(fragNormal).y);
    float height = clamp(pos.y / 6.0, 0.0, 1.0);

    vec3 lowGrass = vec3(0.24, 0.42, 0.13);
    vec3 sunGrass = vec3(0.38, 0.56, 0.20);
    vec3 dryGrass = vec3(0.50, 0.45, 0.23);
    vec3 exposedSoil = vec3(0.42, 0.31, 0.19);
    vec3 stone = vec3(0.50, 0.48, 0.40);

    vec3 grass = mix(lowGrass, sunGrass, broad);
    grass = mix(grass, dryGrass, smoothstep(0.44, 0.78, terrainPatch) * 0.55);
    vec3 color = mix(grass, exposedSoil, smoothstep(0.42, 0.72, terrainPatch + slope * 0.65));
    color = mix(color, stone, smoothstep(0.58, 0.88, slope + height * 0.24));
    color *= 0.92 + fine * 0.16;
    color += (baseColor - vec3(0.35, 0.45, 0.22)) * 0.14;
    return color;
}

vec3 barkMaterial(vec3 pos, vec3 baseColor) {
    float angle = atan(pos.z, pos.x);
    float vertical = fbm2D(vec2(angle * 1.6, pos.y * 0.85));
    float ridges = pow(abs(sin(angle * 10.0 + pos.y * 2.5 + vertical * 5.0)), 2.4);
    float cracks = smoothstep(0.58, 0.78, fbm2D(vec2(angle * 4.0, pos.y * 3.2)));
    vec3 dark = baseColor * vec3(0.48, 0.42, 0.34);
    vec3 warm = baseColor * vec3(1.16, 1.02, 0.82);
    vec3 color = mix(dark, warm, ridges * 0.55 + vertical * 0.35);
    color = mix(color, dark * 0.72, cracks * 0.45);
    return color;
}

vec3 leavesMaterial(vec3 pos, vec3 baseColor) {
    float canopy = fbm3D(pos * 1.25);
    float cluster = fbm3D(pos * 3.8 + 3.7);
    float small = fbm3D(pos * 9.0 + 11.0);
    vec3 shadowLeaf = baseColor * vec3(0.46, 0.62, 0.42);
    vec3 midLeaf = baseColor * vec3(0.86, 1.02, 0.72);
    vec3 yellowTip = vec3(0.48, 0.60, 0.24);
    vec3 color = mix(shadowLeaf, midLeaf, canopy);
    color = mix(color, yellowTip, smoothstep(0.62, 0.90, cluster) * 0.28);
    color *= 0.86 + small * 0.22;
    return color;
}

vec3 wallMaterial(vec3 pos, vec3 baseColor) {
    vec3 nrm = abs(normalize(fragNormal));
    vec2 brickUv = pos.xy;
    if (nrm.x > nrm.z && nrm.x > nrm.y) {
        brickUv = pos.zy;
    } else if (nrm.y > nrm.x && nrm.y > nrm.z) {
        brickUv = pos.xz;
    }

    float n = fbm2D(brickUv * 4.0);
    float stain = fbm2D(brickUv * 0.55 + vec2(13.0, 5.0));
    float mortar = step(0.15, fract(brickUv.y * 3.0));
    mortar *= step(0.1, fract(brickUv.x * 1.5 + floor(brickUv.y * 3.0) * 0.5));
    vec3 brick = baseColor * (0.78 + n * 0.34);
    brick = mix(brick, brick * vec3(0.82, 0.86, 0.88), smoothstep(0.55, 0.86, stain) * 0.35);
    vec3 mortarColor = baseColor * vec3(0.6, 0.58, 0.55);
    vec3 color = mix(mortarColor, brick, mortar);
    color += (n - 0.5) * 0.045;
    return color;
}

vec3 roofMaterial(vec3 pos, vec3 baseColor) {
    vec2 uv = vec2(pos.x * 1.8 + pos.z * 0.18, pos.z * 2.8 + pos.y * 0.35);
    float n = fbm2D(uv * 2.0);
    float rows = smoothstep(0.08, 0.16, fract(uv.y));
    float tiles = smoothstep(0.18, 0.34, abs(fract(uv.x + floor(uv.y) * 0.5) - 0.5));
    vec3 dark = baseColor * vec3(0.52, 0.48, 0.44);
    vec3 light = baseColor * vec3(1.25, 1.06, 0.88);
    vec3 color = mix(dark, light, tiles * rows);
    color *= 0.86 + n * 0.20;
    return color;
}

vec3 doorMaterial(vec3 pos, vec3 baseColor) {
    float grain = fbm2D(vec2(pos.x * 1.5, pos.y * 8.0));
    float fine = fbm2D(vec2(pos.x * 8.0, pos.y * 18.0));
    float plankLine = 1.0 - smoothstep(0.035, 0.075, abs(fract(pos.x * 2.6) - 0.5));
    vec3 color = baseColor * (0.70 + grain * 0.34 + fine * 0.08);
    color = mix(color, color * 0.55, plankLine * 0.45);
    return color;
}

vec3 windowMaterial(vec3 pos, vec3 baseColor) {
    float n = fbm2D(pos.xy * 3.0);
    float glint = smoothstep(0.72, 0.96, fract(pos.x * 2.2 + pos.y * 0.9));
    vec3 color = mix(vec3(0.26, 0.38, 0.48), baseColor, 0.48 + n * 0.18);
    color = mix(color, vec3(0.86, 0.96, 1.0), glint * 0.28);
    return color;
}

vec3 chimneyMaterial(vec3 pos, vec3 baseColor) {
    vec3 nrm = abs(normalize(fragNormal));
    vec2 brickUv = pos.xy;
    if (nrm.x > nrm.z && nrm.x > nrm.y) {
        brickUv = pos.zy;
    } else if (nrm.y > nrm.x && nrm.y > nrm.z) {
        brickUv = pos.xz;
    }

    float n = fbm2D(brickUv * 5.0);
    float soot = smoothstep(0.58, 0.90, fbm2D(brickUv * 0.8 + vec2(4.0, 19.0)));
    float mortar = step(0.12, fract(brickUv.y * 4.0));
    vec3 brick = baseColor * (0.8 + n * 0.3);
    vec3 mortarColor = baseColor * vec3(0.5, 0.48, 0.45);
    vec3 color = mix(mortarColor, brick, mortar);
    color = mix(color, color * vec3(0.45, 0.43, 0.40), soot * 0.28);
    return color;
}

vec3 floorMaterial(vec3 pos, vec3 baseColor) {
    float grain = fbm2D(pos.xz * vec2(1.2, 7.0));
    float boards = 1.0 - smoothstep(0.025, 0.055, abs(fract(pos.x * 1.2) - 0.5));
    vec3 color = baseColor * (0.62 + grain * 0.28);
    return mix(color, color * 0.58, boards * 0.35);
}

vec3 rockMaterial(vec3 pos, vec3 baseColor) {
    vec3 normal = normalize(fragNormal);
    float strata = sin(pos.y * 9.0 + fbm3D(pos * 1.2) * 5.0) * 0.5 + 0.5;
    float flecks = fbm3D(pos * 7.0 + 10.0);
    float broad = fbm3D(pos * 1.3);
    vec3 dark = baseColor * vec3(0.54, 0.53, 0.50);
    vec3 light = baseColor * vec3(1.20, 1.16, 1.08);
    vec3 color = mix(dark, light, broad * 0.65 + strata * 0.22);
    float moss = smoothstep(0.52, 0.78, fbm2D(pos.xz * 1.6 + pos.y * 0.25)) * smoothstep(0.15, 0.75, normal.y);
    color = mix(color, vec3(0.22, 0.31, 0.16), moss * 0.30);
    float cracks = smoothstep(0.62, 0.76, flecks);
    color = mix(color, dark * 0.62, cracks * 0.32);
    return color;
}

vec3 applyMaterial(vec3 pos, vec3 baseColor, float matType) {
    if (matType < 0.5) return terrainMaterial(pos, baseColor);
    else if (matType < 1.5) return barkMaterial(pos, baseColor);
    else if (matType < 2.5) return leavesMaterial(pos, baseColor);
    else if (matType < 3.5) return wallMaterial(pos, baseColor);
    else if (matType < 4.5) return roofMaterial(pos, baseColor);
    else if (matType < 5.5) return doorMaterial(pos, baseColor);
    else if (matType < 6.5) return windowMaterial(pos, baseColor);
    else if (matType < 7.5) return chimneyMaterial(pos, baseColor);
    else if (matType < 8.5) return floorMaterial(pos, baseColor);
    else return rockMaterial(pos, baseColor);
}

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(ubo.lightDir);

    vec3 surfaceColor = applyMaterial(fragPos, fragColor, fragMaterialType);

    float ndl = max(dot(normal, lightDir), 0.0);
    float diffuse = pow(ndl, 0.82);
    float shadow = calculateShadow(fragLightSpacePos);
    float shadowVisibility = mix(1.0, 0.48, shadow);

    float skyFacing = normal.y * 0.5 + 0.5;
    vec3 groundAmbient = vec3(0.16, 0.14, 0.12);
    vec3 skyAmbient = vec3(0.36, 0.43, 0.48);
    vec3 ambient = mix(groundAmbient, skyAmbient, skyFacing) * 0.62;
    vec3 sun = ubo.lightColor * diffuse * shadowVisibility * 1.18;

    vec3 color = surfaceColor * (ambient + sun);
    color += surfaceColor * pow(max(dot(normal, normalize(vec3(-0.45, 0.5, -0.7))), 0.0), 2.0) * 0.055;

    float dist = length(fragPos - ubo.cameraPos);
    float fog = 1.0 - exp(-dist * ubo.fogDensity);
    vec3 fogColor = vec3(0.60, 0.76, 0.86);
    color = mix(color, fogColor, clamp(fog, 0.0, 0.34));

    color = pow(clamp(color, 0.0, 1.0), vec3(0.92));
    outColor = vec4(color, 1.0);
}
