#include "Block.h"
#include "Texture.h"

//Atalho pra bloco com a mesma textura nas 6 faces.
static BlockInfo uniformBlock(int tile, bool solid = true, bool transparent = false)
{
    BlockInfo b;
    b.solid = solid;
    b.transparent = transparent;
    b.translucent = false;
    for (int i = 0; i < 6; i++)
        b.tiles[i] = tile;
    b.tintIndex = TINT_NONE;
    b.tintTopOnly = false;

    return b;
}

//Tabela montada uma vez, no primeiro acesso.
static const BlockInfo* table()
{
    static BlockInfo t[BLOCK_COUNT];
    static bool built = false;

    if (!built)
    {
        t[BLOCK_AIR] = uniformBlock(0, false, true);

        t[BLOCK_GRASS] = uniformBlock(TILE_GRASS_SIDE);
        t[BLOCK_GRASS].tiles[FACE_TOP] = TILE_GRASS_TOP;
        t[BLOCK_GRASS].tiles[FACE_BOTTOM] = TILE_DIRT;
        //O grass_top do Minecraft e cinza de proposito: o verde vem do bioma.
        //Aqui e fixo na cor de planicie (#91BD59). O grass_side ja vem colorido.
        t[BLOCK_GRASS].tintIndex = TINT_GRASS;
        t[BLOCK_GRASS].tintTopOnly = true;

        t[BLOCK_DIRT] = uniformBlock(TILE_DIRT);
        t[BLOCK_STONE] = uniformBlock(TILE_STONE);
        t[BLOCK_COBBLESTONE] = uniformBlock(TILE_COBBLESTONE);
        t[BLOCK_SAND] = uniformBlock(TILE_SAND);
        t[BLOCK_GRAVEL] = uniformBlock(TILE_GRAVEL);
        t[BLOCK_BEDROCK] = uniformBlock(TILE_BEDROCK);

        t[BLOCK_LOG_OAK] = uniformBlock(TILE_LOG_OAK);
        t[BLOCK_LOG_OAK].tiles[FACE_TOP] = TILE_LOG_OAK_TOP;
        t[BLOCK_LOG_OAK].tiles[FACE_BOTTOM] = TILE_LOG_OAK_TOP;

        t[BLOCK_PLANKS_OAK] = uniformBlock(TILE_PLANKS_OAK);

        //Vidro e agua deixam ver atras: o vizinho continua desenhando a face.
        t[BLOCK_GLASS] = uniformBlock(TILE_GLASS, true, true);
        t[BLOCK_WATER] = uniformBlock(TILE_WATER, false, true);
        //Unico bloco com alpha parcial no pack (170 a 223 de 255).
        t[BLOCK_WATER].translucent = true;

        //Folha e solida mas transparente: da pra ver o tronco atras dela,
        //entao o vizinho continua desenhando a face virada pra ca.
        t[BLOCK_LEAVES_OAK] = uniformBlock(TILE_LEAVES_OAK, true, true);
        //Assim como o grass_top, a folha vem cinza no pack e e tingida aqui.
        t[BLOCK_LEAVES_OAK].tintIndex = TINT_LEAVES;

        t[BLOCK_SNOW] = uniformBlock(TILE_SNOW);

        built = true;
    }

    return t;
}

const BlockInfo& blockInfo(BlockID id)
{
    if (id < 0 || id >= BLOCK_COUNT)
        return table()[BLOCK_AIR];

    return table()[id];
}

glm::vec3 tintColor(int index)
{
    switch (index)
    {
    case TINT_GRASS:  return glm::vec3(0.569f, 0.741f, 0.349f);  // #91BD59, planicie
    case TINT_LEAVES: return glm::vec3(0.290f, 0.592f, 0.204f);
    default:          return glm::vec3(1.0f);
    }
}

int blockFaceTintIndex(BlockID id, int face)
{
    const BlockInfo& info = blockInfo(id);

    if (info.tintTopOnly && face != FACE_TOP)
        return TINT_NONE;

    return info.tintIndex;
}
