#version 410 core

// SSAO (screen-space ambient occlusion, after Crytek / J. Chapman), from the depth buffer only:
//   mode 0 - for each pixel, sample points in the hemisphere of its normal; the more of them are behind the geometry
//            in the depth buffer, the less ambient light reaches the pixel (1 = open, 0 = fully occluded)
//   mode 1 - 4x4 blur: removes the pattern of the random per-pixel rotations
in vec2 texCoords;
out vec4 fColor;

uniform sampler2D depthTexture;
uniform sampler2D image;          // blur: the raw occlusion
uniform int mode;
uniform mat4 projection;
uniform mat4 invProjection;
uniform vec3 kernel[16];          // the sample points (hemisphere, z = along the normal)
uniform float radius;             // meters
uniform vec2 texelSize;           // of the half-resolution target

// eye-space position of the pixel at uv, rebuilt from its depth
vec3 viewPosition(vec2 uv)
{
    float d = texture(depthTexture, uv).r;
    vec4 p = invProjection * vec4(vec3(uv, d) * 2.0f - 1.0f, 1.0f);
    return p.xyz / p.w;
}

void main()
{
    if (mode == 1) {
        float sum = 0.0f;
        for (int x = -2; x < 2; x++) {
            for (int y = -2; y < 2; y++) {
                sum += texture(image, texCoords + (vec2(x, y) + 0.5f) * texelSize).r;
            }
        }
        fColor = vec4(sum / 16.0f);
        return;
    }

    if (texture(depthTexture, texCoords).r >= 0.99999f) {   // the sky
        fColor = vec4(1.0f);
        return;
    }
    vec3 p = viewPosition(texCoords);
    // the normal from the neighbouring pixels: on each axis the side with the smaller depth difference, so the
    // silhouettes (where the neighbour is on another object) don't produce false normals
    vec3 right = viewPosition(texCoords + vec2(texelSize.x, 0.0f)) - p;
    vec3 left = p - viewPosition(texCoords - vec2(texelSize.x, 0.0f));
    vec3 up = viewPosition(texCoords + vec2(0.0f, texelSize.y)) - p;
    vec3 down = p - viewPosition(texCoords - vec2(0.0f, texelSize.y));
    vec3 dx = abs(right.z) < abs(left.z) ? right : left;
    vec3 dy = abs(up.z) < abs(down.z) ? up : down;
    vec3 n = normalize(cross(dx, dy));

    // the hemisphere turned randomly around the normal for every pixel (the blur averages the noise away)
    float angle = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898f, 78.233f))) * 43758.5453f) * 6.2831853f;
    vec3 random = vec3(cos(angle), sin(angle), 0.0f);
    vec3 t = random - n * dot(random, n);
    t = dot(t, t) > 1e-4f ? normalize(t) : normalize(cross(n, vec3(0.0f, 0.0f, 1.0f)));
    mat3 tbn = mat3(t, cross(n, t), n);

    float occlusion = 0.0f;
    for (int i = 0; i < 16; i++) {
        vec3 s = p + tbn * kernel[i] * radius;
        vec4 o = projection * vec4(s, 1.0f);
        vec2 sampleUV = o.xy / o.w * 0.5f + 0.5f;
        float sceneZ = viewPosition(sampleUV).z;
        // only geometry close to the point counts (not a wall far behind it on the screen)
        float range = smoothstep(0.0f, 1.0f, radius / abs(p.z - sceneZ));
        occlusion += (sceneZ >= s.z + 0.05f ? 1.0f : 0.0f) * range;
    }
    float ao = 1.0f - occlusion / 16.0f;
    // far away the hemisphere is smaller than a pixel: no occlusion there
    ao = mix(ao, 1.0f, smoothstep(60.0f, 150.0f, -p.z));
    fColor = vec4(ao);
}
