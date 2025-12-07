#version 330 core
in vec2 UV;
out vec4 fragColor;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCameraPos;
uniform int uShape; //Which scene to render: One of 0 = Mandelbulb, 1 = Menger Sponge, 2 = Julia Quaternion, 3 = Terrain, 4 = Sphere/Torus combo

struct Light {
    vec3 position;
    vec3 color; // can exceed 1.0 for HDR
};

const int MAX_LIGHTS = 4;
uniform Light lights[MAX_LIGHTS];
uniform int numLights;

// Material coefficients
uniform float ka; // ambient
uniform float kd; // diffuse
uniform float ks; // specular
uniform float shininess;

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

// Phong lighting
vec3 phongLighting(vec3 p, vec3 N, vec3 viewDir)
{
    vec3 color = vec3(0.0);

    for (int i = 0; i < numLights; i++) {
        vec3 L = normalize(lights[i].position - p);
        vec3 R = reflect(-L, N);

        float diff = max(dot(N, L), 0.0);
        float spec = pow(max(dot(viewDir, R), 0.0), shininess);

        color += lights[i].color * (kd * diff + ks * spec);
    }

    // Ambient
    color += ka * vec3(0.1);

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
        vec3 viewDir = normalize(ro - hitPos);
        vec3 color = phongLighting(hitPos, N, viewDir);
        fragColor = vec4(color, 1.0);

    } else {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
