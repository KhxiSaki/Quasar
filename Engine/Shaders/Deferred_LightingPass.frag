#version 450

layout(binding = 0) uniform sampler2D gPosition;
layout(binding = 1) uniform sampler2D gNormal;
layout(binding = 2) uniform sampler2D gAlbedo;
layout(binding = 3) uniform sampler2D gPBR;

layout(binding = 4) uniform LightData {
    vec3 lightPos;
    float lightRadius;
    vec3 lightColor;
    vec3 viewPos;
    float exposure;
} light;

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // DEBUG: Check individual G-Buffer channels
    // Uncomment ONE of these to see what's in each buffer:
    
    // Check albedo only (should show the texture colors)
    vec3 albedo = texture(gAlbedo, inTexCoord).rgb;
    outColor = vec4(albedo, 1.0);
    return;
    
    // Check normals (convert from [-1,1] to [0,1] for visualization)
    // vec3 normal = texture(gNormal, inTexCoord).xyz;
    // outColor = vec4(normal * 0.5 + 0.5, 1.0);
    // return;
    
    // Check position (visualize as color)
    // vec3 pos = texture(gPosition, inTexCoord).xyz;
    // outColor = vec4(pos * 0.1, 1.0);  // Scale down for visualization
    // return;

    // Full PBR lighting (uncomment when debugging is done)
    /*
    // Sample G-Buffer
    vec3 worldPosition = texture(gPosition, inTexCoord).xyz;
    vec3 worldNormal = texture(gNormal, inTexCoord).xyz;
    vec3 albedo = texture(gAlbedo, inTexCoord).rgb;
    vec4 pbrData = texture(gPBR, inTexCoord);
    float roughness = pbrData.r;
    float metallic = pbrData.g;
    float ao = pbrData.b;

    // PBR lighting calculation
    vec3 N = normalize(worldNormal);
    vec3 L = normalize(light.lightPos - worldPosition);
    vec3 V = normalize(light.viewPos - worldPosition);
    vec3 R = reflect(-L, N);

    // Diffuse (Lambert)
    vec3 diffuse = albedo / 3.14159;

    // Specular (Cook-Torrance)
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotR = max(dot(N, R), 0.0);

    // Fresnel (Schlick approximation)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(L, V), 0.0), 5.0);

    // Normal Distribution Function (GGX)
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotR * NdotR * (alpha2 - 1.0) + 1.0;
    float NDF = alpha2 / (3.14159 * denom * denom);

    // Geometry Function (Schlick-GGX)
    float k = (alpha + 1.0) * (alpha + 1.0) / 8.0;
    float G_V = NdotV / (NdotV * (1.0 - k) + k);
    float G_L = NdotL / (NdotL * (1.0 - k) + k);
    float G = G_V * G_L;

    // Cook-Torrance BRDF
    vec3 specular = (NDF * G * F) / max(4.0 * NdotV * NdotL, 0.001);

    // Combine diffuse and specular
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 color = (kD * diffuse + specular) * NdotL * light.lightColor;

    // Apply ambient occlusion
    color *= ao;

    // Simple distance-based light attenuation
    float distanceToLight = length(light.lightPos - worldPosition);
    float attenuation = 1.0 / (1.0 + 0.09 * distanceToLight * distanceToLight);
    color *= attenuation;

    // Add ambient light so shadowed areas aren't completely black
    vec3 ambient = vec3(0.05) * albedo;
    color += ambient;

    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
    */
}
