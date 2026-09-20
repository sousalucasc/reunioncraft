#include "ChunkStorage.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{
    std::string worldPath = "saves/world";

    //Workers leem em paralelo, entao os contadores precisam ser atomicos.
    std::atomic<int> savedCount(0);
    std::atomic<int> loadedCount(0);

    //Identifica o formato. Se mudar o layout, muda o numero e os arquivos
    //velhos passam a ser ignorados em vez de lidos errado.
    const char MAGIC[4] = { 'R', 'C', 'K', '1' };

    std::string chunkFile(ChunkPos pos)
    {
        char nome[64];
        std::snprintf(nome, sizeof(nome), "/c.%d.%d.bin", pos.x, pos.z);

        return worldPath + nome;
    }
}

void ChunkStorage::setWorldPath(const std::string& path)
{
    worldPath = path;

    std::error_code ec;
    std::filesystem::create_directories(worldPath, ec);
}

bool ChunkStorage::save(ChunkPos pos, const Chunk& chunk)
{
    std::vector<uint8_t> rle;
    chunk.encodeRLE(rle);

    //Grava num temporario e so depois renomeia. Assim um chunk nunca e lido
    //pela metade se o jogo fechar no meio da escrita.
    std::string finalPath = chunkFile(pos);
    std::string tempPath = finalPath + ".tmp";

    {
        std::ofstream f(tempPath, std::ios::binary | std::ios::trunc);
        if (!f)
            return false;

        f.write(MAGIC, 4);

        uint32_t size = (uint32_t)rle.size();
        f.write((const char*)&size, sizeof(size));
        f.write((const char*)rle.data(), (std::streamsize)rle.size());

        if (!f)
            return false;
    }

    std::error_code ec;
    std::filesystem::rename(tempPath, finalPath, ec);
    if (ec)
    {
        std::filesystem::remove(tempPath, ec);
        return false;
    }

    savedCount++;

    return true;
}

bool ChunkStorage::load(ChunkPos pos, Chunk& chunk)
{
    std::ifstream f(chunkFile(pos), std::ios::binary);
    if (!f)
        return false;

    char magic[4];
    f.read(magic, 4);
    if (!f || magic[0] != MAGIC[0] || magic[1] != MAGIC[1]
           || magic[2] != MAGIC[2] || magic[3] != MAGIC[3])
        return false;

    uint32_t size = 0;
    f.read((char*)&size, sizeof(size));

    //Um chunk inteiro sem nenhuma faixa repetida daria 3 bytes por bloco.
    //Acima disso o arquivo so pode estar corrompido.
    if (!f || size == 0 || size > (uint32_t)CHUNK_VOLUME * 3)
        return false;

    std::vector<uint8_t> rle(size);
    f.read((char*)rle.data(), (std::streamsize)size);
    if (!f)
        return false;

    if (!chunk.decodeRLE(rle.data(), rle.size()))
        return false;

    loadedCount++;

    return true;
}

int ChunkStorage::chunksSaved()
{
    return savedCount.load();
}

int ChunkStorage::chunksLoaded()
{
    return loadedCount.load();
}
