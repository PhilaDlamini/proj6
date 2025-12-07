#version 330 core
in vec2 UV;
out vec4 fragColor;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCameraPos;
uniform int uShape; //Which shape to render: One of 0 = Mandelbulb, 1 = Menger Sponge, 2 = Julia Quaternion, 3 = Terrain, 4 = Sphere/Torus combo

struct SceneLightData {
    int type;          // 0 = Directional, 1 = Point, 2 = Spot
    vec3 color;        // Light color (RG). Can exceed 1.0 for HDR
    vec3 function;     // Attenuation: constant, linear, quadratic
    vec4 pos;          // World-space position (point/spot)
    vec4 dir;          // World-space direction (directional/spot)
    float penumbra;    // Spot penumbra (radians)
    float angle;       // Spot angle (radians)
};

const int MAX_LIGHTS = 8;
uniform SceneLightData lights[MAX_LIGHTS];
uniform int numLights;

// Material coefficients
uniform float ka; // ambient
uniform float kd; // diffuse
uniform float ks; // specular

// Material properties
uniform vec3 mat_ambient;
uniform vec3 mat_diffuse;
uniform vec3 mat_specular;
uniform float mat_shininess;

//////////// Fractal SDF definitions ////////////////

// Mandel buld fractral
float mandelbulbDE(vec3 pos)
{
    vec3 z = pos;
    float dr = 1.0;
    float r  = 0.0;

    const int Iterations = 12;
    const float Power   = 8.0;

    for (int i = 0; i < Iterations; i++) {
        r = length(z);
        if (r > 2.0) break;

        float theta = acos(z.z / r);
        float phi   = atan(z.y, z.x);

        float rPow = pow(r, Power - 1.0);
        dr = rPow * dr * Power + 1.0;

        float zr = pow(r, Power);
        theta *= Power;
        phi   *= Power;

        z = zr * vec3(
            sin(theta) * cos(phi),
            sin(theta) * sin(phi),
            cos(theta)
        );

        z += pos;
    }

    return 0.5 * log(r) * r / dr;
}


//Menger Sponge
float mengerSDF(vec3 p)
{
    float scale = 3.0;
    float d = 1.0;

    for (int i = 0; i < 4; i++)
    {
        p = abs(p);                 // fold into first octant
        if (p.x < p.y) p.xy = p.yx;
        if (p.x < p.z) p.xz = p.zx;

        p = p * scale - vec3(scale-1.0); // scale and translate
    }

    // distance to unit cube
    d = (length(max(p - vec3(1.0), 0.0))) / pow(scale, 4.0);
    return d;
}


// Julian quaterion
float juliaDE(vec3 p)
{
    vec4 z = vec4(p, 0.0);
    const vec4 c = vec4(-0.2, 0.7, 0.3, -0.1);

    float dr = 1.0;
    float r;

    for (int i = 0; i < 10; i++) {
        r = length(z);
        if (r > 2.0) break;

        // quaternion square
        vec4 z2 = z;
        z = vec4(
            z2.x*z2.x - dot(z2.yzw, z2.yzw),
            2.0*z2.x * z2.yzw
        );
        z += c;

        dr = dr * 2.0 * r + 1.0;
    }

    return 0.5 * log(r) * r / dr;
}

//Terrain
float terrainSDF(vec3 p)
{
    float h = sin(p.x * 0.3) * cos(p.z * 0.3) * 2.0; // height oscillates 0 → ±2
    return p.y - h; // distance from point to terrain surface
}

// Smooth sphere + torus
float sphereTorusSDF(vec3 p)
{
    float sphere = length(p) - 1.0;
    float t = length(vec2(length(p.xz) - 2.0, p.y)) - 0.4;
    return min(sphere, t);
}

float sceneSDF(vec3 p)
{
    switch (uShape) {
        case 0: return mandelbulbDE(p);
        case 1: return mengerSDF(p);
        case 2: return juliaDE(p);
        case 3: return terrainSDF(p);
        case 4: return sphereTorusSDF(p);
    }
    return mandelbulbDE(p); // fallback
}

//Estimates the normals for lighting calculation
vec3 estimateNormal(vec3 p)
{
    float e = 0.0005;
    return normalize(vec3(
        sceneSDF(p + vec3(e,0,0)) - sceneSDF(p - vec3(e,0,0)),
        sceneSDF(p + vec3(0,e,0)) - sceneSDF(p - vec3(0,e,0)),
        sceneSDF(p + vec3(0,0,e)) - sceneSDF(p - vec3(0,0,e))
    ));
}

//Ray marching
float marchRay(vec3 ro, vec3 rd, out vec3 hitPos, out int hit)
{
    const float MAX_DIST = 100.0;
    const float EPS = 0.0005;
    const int MAX_STEPS = 350;

    float t = 0.0;
    for (int i = 0; i < MAX_STEPS; i++) {
        vec3 p = ro + rd * t;
        float d = sceneSDF(p);

        if (d < EPS) {
            hit = 1;
            hitPos = p;
            return t;
        }

        if (t > MAX_DIST) break;

        float step = d * 0.7;
        step = max(step, 0.0005);
        t += step;
    }

    hit = 0;
    return t;
}

vec3 computeLight(SceneLightData light, vec3 pos, vec3 normal) {
    vec3 N = normalize(normal);
    vec3 V = normalize(uCameraPos - pos);
    vec3 L;
    float attenuation = 1.0;

    if (light.type == 0) {
        L = normalize(-light.dir.xyz);
    } else {
        L = normalize(light.pos.xyz - pos);
        float dist = length(light.pos.xyz - pos);

        attenuation = 1.0 / (light.function.x +
                             light.function.y * dist +
                             light.function.z * dist * dist);

        if (light.type == 2) {
            float theta = dot(normalize(-L), normalize(light.dir.xyz));
            float cosInner = cos(light.angle - light.penumbra);
            float cosOuter = cos(light.angle);
            float intensity = clamp((theta - cosOuter) / (cosInner - cosOuter),
                                    0.0, 1.0);
            attenuation *= intensity;
        }
    }

    // Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = kd * diff * mat_diffuse;

    // Specular
    vec3 specular = vec3(0.0);
    if(mat_shininess > 0.0) {
        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(R, V), 0.0), mat_shininess);
        specular = ks * spec * mat_specular;
    }

    return attenuation * light.color * (diffuse + specular);
}

// Phong lighting
vec3 phongLighting(vec3 p, vec3 N)
{
    // Global ambient (independent of lights)
    vec3 color = ka * mat_ambient;

    // Add diffuse + spec per light
    for (int i = 0; i < numLights; ++i) {
        color += computeLight(lights[i], p, N);
    }

    return color;
}


void main() {
    vec2 uv = UV * 2.0 - 1.0;
    mat4 invProj = inverse(uProj); //inverse matrices
    mat4 invView = inverse(uView);

    /** Shoot a ray starting from each pixel position on the screen **/
    vec4 rayClip = vec4(uv, -1.0, 1.0); // Build a point on the near plane in clip space
    vec4 rayEye  = invProj * rayClip; //Get the same point in camera space
    rayEye = vec4(rayEye.xy, -1.0, 0.0);  // Turn that point into a direction pointing forward (-Z)
    vec3 rd = normalize((invView * rayEye).xyz); // Convert direction to world space
    vec3 ro = uCameraPos;  // Ray origin is the camera position

    // Ray march
    vec3 hitPos;
    int hit;
    marchRay(ro, rd, hitPos, hit);

    if (hit == 1) {
        vec3 N = estimateNormal(hitPos);
        vec3 color = phongLighting(hitPos, N);
        fragColor = vec4(color, 1.0);

    } else {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
