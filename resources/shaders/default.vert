#version 330 core

// Task 4: declare a vec3 object-space position variable, using
//         the `layout` and `in` keywords.
layout(location = 0) in vec3 object_space_pos;

// Task 5: declare `out` variables for the world-space position and normal,
//         to be passed to the fragment shader
out vec3 world_space_pos;
out vec3 world_space_normal;

// Task 6: declare a uniform mat4 to store model matrix
uniform mat4 m_model;

// Task 7: declare uniform mat4's for the view and projection matrix
uniform mat4 m_proj;
uniform mat4 m_view;

void main() {
    // Task 8: compute the world-space position and normal, then pass them to
    //         the fragment shader using the variables created in task 5
    world_space_pos = vec3(m_model * vec4(object_space_pos, 1.0));

    // Recall that transforming normals requires obtaining the inverse-transpose of the model matrix!
    // In projects 5 and 6, consider the performance implications of performing this here.
    mat3 normal_matrix = transpose(inverse(mat3(m_model)));
    world_space_normal = normalize(normal_matrix * normalize(object_space_pos));

    // Task 9: set gl_Position to the object space position transformed to clip space
    gl_Position = m_proj * m_view * vec4(world_space_pos, 1.0);
}
