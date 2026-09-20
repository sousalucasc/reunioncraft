#ifndef RAYCAST_H
#define RAYCAST_H

#include "World.h"

#include <glm/glm.hpp>

struct RaycastHit
{
    bool hit;

    //Bloco atingido, em coordenada global.
    int x;
    int y;
    int z;

    //Normal da face atingida. Somada ao bloco, da o espaco vazio na frente
    //dele, que e onde um bloco novo seria colocado.
    int nx;
    int ny;
    int nz;
};

//Percorre voxel a voxel (algoritmo de Amanatides e Woo) a partir de origin
//na direcao dir, ate maxDistance. Para no primeiro bloco solido.
//Agua nao conta como alvo.
RaycastHit raycast(const World& world, const glm::vec3& origin, const glm::vec3& dir, float maxDistance);

#endif
