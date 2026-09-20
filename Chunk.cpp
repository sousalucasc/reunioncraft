#include "Chunk.h"

#include <cstring>

Chunk::Chunk()
    : dirty(true), modified(false), highestBlock(-1)
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

void Chunk::encodeRLE(std::vector<uint8_t>& out) const
{
    out.clear();
    out.reserve(4096);

    size_t i = 0;
    while (i < CHUNK_VOLUME)
    {
        uint8_t value = blocks[i];

        //A contagem cabe em 16 bits, entao quebra faixas muito longas.
        size_t run = 1;
        while (i + run < CHUNK_VOLUME && blocks[i + run] == value && run < 65535)
            run++;

        out.push_back((uint8_t)(run & 0xFF));
        out.push_back((uint8_t)((run >> 8) & 0xFF));
        out.push_back(value);

        i += run;
    }
}

bool Chunk::decodeRLE(const uint8_t* data, size_t size)
{
    size_t written = 0;
    size_t i = 0;

    highestBlock = -1;

    while (i + 2 < size && written < CHUNK_VOLUME)
    {
        size_t run = (size_t)data[i] | ((size_t)data[i + 1] << 8);
        uint8_t value = data[i + 2];
        i += 3;

        if (run == 0 || written + run > CHUNK_VOLUME)
            return false;

        for (size_t k = 0; k < run; k++)
            blocks[written + k] = value;

        if (value != BLOCK_AIR)
        {
            //Indice linear e x + 16*(z + 16*y), entao o y do ultimo bloco
            //da faixa sai da divisao pelo tamanho de uma camada.
            int lastY = (int)((written + run - 1) / (CHUNK_SIZE * CHUNK_SIZE));
            if (lastY > highestBlock)
                highestBlock = lastY;
        }

        written += run;
    }

    if (written != CHUNK_VOLUME)
        return false;

    dirty = true;
    modified = true;

    return true;
}
