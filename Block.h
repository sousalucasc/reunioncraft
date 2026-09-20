#ifndef BLOCK_H
#define BLOCK_H

#include <glm/glm.hpp>

//Ordem das faces. A mesma do createCube e, depois, do mesher do chunk.
enum BlockFace
{
    FACE_FRONT = 0,  // +Z
    FACE_BACK,       // -Z
    FACE_LEFT,       // -X
    FACE_RIGHT,      // +X
    FACE_TOP,        // +Y
    FACE_BOTTOM      // -Y
};

enum BlockID
{
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_COBBLESTONE,
    BLOCK_SAND,
    BLOCK_GRAVEL,
    BLOCK_BEDROCK,
    BLOCK_LOG_OAK,
    BLOCK_PLANKS_OAK,
    BLOCK_GLASS,
    BLOCK_WATER,
    BLOCK_LEAVES_OAK,
    BLOCK_SNOW,
    BLOCK_COUNT
};

struct BlockInfo
{
    //Bloqueia passagem e esconde a face do vizinho.
    bool solid;
    //Deixa ver o que esta atras, entao o vizinho ainda precisa desenhar a face.
    bool transparent;
    //Alpha parcial: precisa de blending e de ser desenhado por ultimo, em
    //ordem. Vidro e folha NAO entram aqui: o alpha deles e 0 ou 255, entao
    //um discard no shader resolve, sem custo de ordenacao.
    bool translucent;
    //Tile do atlas por face, na ordem do BlockFace.
    int tiles[6];
    //Multiplica a textura. Branco = textura original.
    glm::vec3 tint;
    //true = so a face de cima recebe o tint (caso da grama).
    bool tintTopOnly;
};

const BlockInfo& blockInfo(BlockID id);

//Tint que a face recebe, ja resolvendo o tintTopOnly.
glm::vec3 blockFaceTint(BlockID id, int face);

#endif
