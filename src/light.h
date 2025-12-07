#ifndef LIGHT_H
#define LIGHT_H
#include "glm/glm.hpp"

struct SceneLightData {
    int type;          // 0 = Directional, 1 = Point, 2 = Spot
    glm::vec3 color;        // Light color (RG). Can exceed 1.0 for HDR
    glm::vec3 function;     // Attenuation: constant, linear, quadratic
    glm::vec4 pos;          // World-space position (point/spot)
    glm::vec4 dir;          // World-space direction (directional/spot)
    float penumbra;    // Spot penumbra (radians)
    float angle;       // Spot angle (radians)
};


#endif // LIGHT_H
