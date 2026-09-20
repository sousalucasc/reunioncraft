#include "Raycast.h"

#include <cmath>
#include <cfloat>

//Quanto falta andar, na direcao ds, pra cruzar o proximo limite de voxel.
static float intBound(float s, float ds)
{
    if (ds == 0.0f)
        return FLT_MAX;

    if (ds < 0.0f)
        return intBound(-s, -ds);

    //Parte fracionaria sempre positiva.
    float frac = s - std::floor(s);

    return (1.0f - frac) / ds;
}

static int signOf(float v)
{
    if (v > 0.0f)
        return 1;
    if (v < 0.0f)
        return -1;

    return 0;
}

RaycastHit raycast(const World& world, const glm::vec3& origin, const glm::vec3& dir, float maxDistance)
{
    RaycastHit result;
    result.hit = false;
    result.x = 0;
    result.y = 0;
    result.z = 0;
    result.nx = 0;
    result.ny = 0;
    result.nz = 0;

    //Voxel em que o raio comeca.
    int x = (int)std::floor(origin.x);
    int y = (int)std::floor(origin.y);
    int z = (int)std::floor(origin.z);

    int stepX = signOf(dir.x);
    int stepY = signOf(dir.y);
    int stepZ = signOf(dir.z);

    //Distancia ate a primeira fronteira de voxel em cada eixo...
    float tMaxX = intBound(origin.x, dir.x);
    float tMaxY = intBound(origin.y, dir.y);
    float tMaxZ = intBound(origin.z, dir.z);

    //...e distancia entre fronteiras consecutivas do mesmo eixo.
    float tDeltaX = (stepX != 0) ? (float)stepX / dir.x : FLT_MAX;
    float tDeltaY = (stepY != 0) ? (float)stepY / dir.y : FLT_MAX;
    float tDeltaZ = (stepZ != 0) ? (float)stepZ / dir.z : FLT_MAX;

    if (stepX == 0 && stepY == 0 && stepZ == 0)
        return result;

    //Se a camera esta dentro de um bloco solido nao existe face de entrada,
    //e portanto nao existe normal valida. Melhor nao mirar em nada.
    BlockID startId = world.getBlock(x, y, z);
    if (startId != BLOCK_AIR && startId != BLOCK_WATER && blockInfo(startId).solid)
        return result;

    while (true)
    {
        //Avanca primeiro, testa depois. Se testasse o voxel de origem antes
        //do primeiro passo, um acerto imediato sairia com normal (0,0,0) e
        //colocar bloco em hit + normal cairia dentro do proprio bloco.
        //A normal aponta pra tras do passo: entramos pela face oposta.
        if (tMaxX < tMaxY && tMaxX < tMaxZ)
        {
            if (tMaxX > maxDistance)
                break;

            x += stepX;
            tMaxX += tDeltaX;

            result.nx = -stepX;
            result.ny = 0;
            result.nz = 0;
        }
        else if (tMaxY < tMaxZ)
        {
            if (tMaxY > maxDistance)
                break;

            y += stepY;
            tMaxY += tDeltaY;

            result.nx = 0;
            result.ny = -stepY;
            result.nz = 0;
        }
        else
        {
            if (tMaxZ > maxDistance)
                break;

            z += stepZ;
            tMaxZ += tDeltaZ;

            result.nx = 0;
            result.ny = 0;
            result.nz = -stepZ;
        }

        BlockID id = world.getBlock(x, y, z);

        //Agua nao e alvo: da pra atravessar.
        if (id != BLOCK_AIR && id != BLOCK_WATER && blockInfo(id).solid)
        {
            result.hit = true;
            result.x = x;
            result.y = y;
            result.z = z;

            return result;
        }
    }

    //Nao acertou nada: zera a normal que ficou do ultimo passo.
    result.nx = 0;
    result.ny = 0;
    result.nz = 0;

    return result;
}
