#version 330 core

// Task 5: declare "in" variables for the world-space position and normal,
//         received post-interpolation from the vertex shader
in vec3 world_space_pos;
in vec3 world_space_normal;

// Task 10: declare an out vec4 for your output color
out vec4 fragColor;

// Task 12: declare relevant uniform(s) here, for ambient lighting
uniform float k_a;

// Task 13: declare relevant uniform(s) here, for diffuse lighting
uniform float k_d;
uniform vec4 light_world_pos;

// Task 14: declare relevant uniform(s) here, for specular lighting
uniform float k_s;
uniform float shininess;
uniform vec4 world_space_cam_pos;


void main() {
    // Remember that you need to renormalize vectors here if you want them to be normalized

    // Task 10: set your output color to white (i.e. vec4(1.0)). Make sure you get a white circle!
    // fragColor = vec4(1.0);

    // Task 11: set your output color to the absolute value of your world-space normals,
    //          to make sure your normals are correct.
    // fragColor = vec4(world_space_normal, 1.0);

    // Task 12: add ambient component to output color
    vec3 ambient = vec3(k_a);
    // fragColor = vec4(ambient, 1.0);

    // Task 13: add diffuse component to output color
    vec3 N = normalize(world_space_normal);
    vec3 L = normalize(light_world_pos.xyz - world_space_pos);
    vec3 diffuse = k_d * max(0.0, dot(N, L)) * vec3(1.0);
    // fragColor = vec4(ambient + diffuse, 1.0);

    // Task 14: add specular component to output color
    vec3 E = normalize(world_space_cam_pos.xyz - world_space_pos);
    vec3 R = reflect(-L, N);
    vec3 specular = k_s * pow(max(0.0, dot(R, E)), shininess) * vec3(1.0);
    fragColor = vec4(ambient + diffuse + specular, 1.0);

}

