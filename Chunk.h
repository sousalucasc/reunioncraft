#ifndef CHUNK_H
#define CHUNK_H

#include "Block.h"

#include <cstdint>

constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_HEIGHT = 256;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE;

//Uma coluna 16x256x16 de blocos. 64 KB, entao sempre alocar na heap.
class Chunk
{
public:
    //Liga quando o conteudo muda: a mesh precisa ser regerada.
    bool dirty;

    //Y do bloco nao-ar mais alto, ou -1 se o chunk esta vazio.
    //O mesher para aqui em vez de varrer os 256 niveis: acima disso so tem ar.
    //Conservador de proposito: depois de remover blocos pode ficar alto demais,
    //nunca baixo demais, entao nunca esconde geometria.
    int highestBlock;

    Chunk();

    //Fora dos limites devolve BLOCK_AIR em vez de estourar o array.
    BlockID getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockID id);

    //Chao plano de teste: bedrock, pedra, terra e uma camada de grama no topo.
    void fillFlat(int groundHeight);

private:
    //Layout x + CHUNK_SIZE * (z + CHUNK_SIZE * y): varrer em X e o mais rapido,
    //que e a ordem que o mesher usa.
    static int index(int x, int y, int z);
    static bool inBounds(int x, int y, int z);

    uint8_t blocks[CHUNK_VOLUME];
};

#endif
