#include "Chunk.h"

#include <cstring>

Chunk::Chunk()
    : dirty(true), highestBlock(-1)
{
    //BLOCK_AIR e 0, entao memset zera tudo pra ar.
    std::memset(blocks, BLOCK_AIR, sizeof(blocks));
}

int Chunk::index(int x, int y, int z)
{
    return x + CHUNK_SIZE * (z + CHUNK_SIZE * y);
}

bool Chunk::inBounds(int x, int y, int z)
{
    return x >= 0 && x < CHUNK_SIZE
        && y >= 0 && y < CHUNK_HEIGHT
        && z >= 0 && z < CHUNK_SIZE;
}

BlockID Chunk::getBlock(int x, int y, int z) const
{
    if (!inBounds(x, y, z))
        return BLOCK_AIR;

    return (BlockID)blocks[index(x, y, z)];
}

void Chunk::setBlock(int x, int y, int z, BlockID id)
{
    if (!inBounds(x, y, z))
        return;

    blocks[index(x, y, z)] = (uint8_t)id;

    if (id != BLOCK_AIR && y > highestBlock)
        highestBlock = y;

    dirty = true;
}

void Chunk::fillFlat(int groundHeight)
{
    for (int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            for (int x = 0; x < CHUNK_SIZE; x++)
            {
                BlockID id = BLOCK_AIR;

                if (y == 0)
                    id = BLOCK_BEDROCK;
                else if (y < groundHeight - 4)
                    id = BLOCK_STONE;
                else if (y < groundHeight - 1)
                    id = BLOCK_DIRT;
                else if (y == groundHeight - 1)
                    id = BLOCK_GRASS;

                blocks[index(x, y, z)] = (uint8_t)id;
            }
        }
    }

    if (groundHeight - 1 > highestBlock)
        highestBlock = groundHeight - 1;

    dirty = true;
}
