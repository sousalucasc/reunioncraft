#ifndef CHUNKSTORAGE_H
#define CHUNKSTORAGE_H

#include "World.h"

#include <string>

//Persistencia de chunk em disco, um arquivo por chunk.
//
//So chunk MODIFICADO pelo jogador e salvo. O resto do mundo nao precisa ir
//pro disco: o TerrainGenerator e deterministico, entao a mesma seed e a
//mesma coordenada reproduzem exatamente os mesmos blocos. E por isso que
//um mundo infinito cabe em alguns kilobytes.
namespace ChunkStorage
{
    //Pasta onde os chunks sao gravados. Criada se nao existir.
    void setWorldPath(const std::string& path);

    //Le o chunk do disco. Devolve false se nao existe ou se o arquivo
    //estiver corrompido, e nesse caso cabe ao chamador gerar pelo noise.
    bool load(ChunkPos pos, Chunk& chunk);

    //Grava. Chamado antes de descarregar um chunk e ao sair do jogo.
    bool save(ChunkPos pos, const Chunk& chunk);

    //Estatistica acumulada, so pra diagnostico.
    int chunksSaved();
    int chunksLoaded();
}

#endif
