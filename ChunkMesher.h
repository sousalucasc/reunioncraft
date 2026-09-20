#ifndef CHUNKMESHER_H
#define CHUNKMESHER_H

#include "World.h"

#include <vector>
#include <cstdint>

//Largura do instantaneo: o chunk mais 1 bloco de casca de cada lado.
constexpr int SNAP_SIZE = CHUNK_SIZE + 2;

//Copia dos blocos de que o mesher precisa: o chunk inteiro mais a casca de
//1 bloco em X e Z, que e ate onde a checagem de vizinho e o AO alcancam.
//
//Existe pra que o meshing possa rodar em outra thread sem tocar no World.
//A alternativa seria proteger o World com mutex, mas ai o worker seguraria
//o mapa por quase um milissegundo a cada chunk, justamente enquanto a
//thread principal quer inserir e remover chunks.
struct ChunkSnapshot
{
    ChunkPos pos;
    int highestBlock;

    //Indexado por [lx + 1][y][lz + 1], com lx e lz de -1 a CHUNK_SIZE.
    std::vector<uint8_t> blocks;

    ChunkSnapshot() : highestBlock(-1) {}

    BlockID get(int lx, int y, int lz) const
    {
        if (lx < -1 || lx > CHUNK_SIZE || lz < -1 || lz > CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT)
            return BLOCK_AIR;

        return (BlockID)blocks[(size_t)(lx + 1) + SNAP_SIZE * ((size_t)(lz + 1) + SNAP_SIZE * (size_t)y)];
    }
};

//Monta o instantaneo a partir do World. Roda na thread principal, que e a
//unica que mexe no World, entao nao precisa de sincronizacao.
void captureSnapshot(const World& world, ChunkPos pos, ChunkSnapshot& out);

//Um conjunto de vertices e indices.
//
//Cada vertice sao DOIS uint32 em vez de 8 floats: 8 bytes contra 32.
//O shader desempacota e reconstrui posicao, uv e cor.
//
//  palavra 0:  bits  0-4   x local   (0..16)
//              bits  5-13  y         (0..256)
//              bits 14-18  z local   (0..16)
//              bits 19-21  face      (0..5)
//              bits 22-23  nivel de AO (0..3)
//              bits 24-25  indice do tint
//
//  palavra 1:  bits  0-7   tile do atlas (0..255)
//              bit   8     canto em U
//              bit   9     canto em V
struct MeshBuffer
{
    std::vector<uint32_t> vertices;
    std::vector<uint32_t> indices;

    //2 uint32 por vertice.
    int vertexCount() const { return (int)vertices.size() / 2; }
    int faceCount() const { return (int)indices.size() / 6; }

    void clear()
    {
        vertices.clear();
        indices.clear();
    }
};

//Dados crus da mesh, sem nada de OpenGL.
//Separado de proposito: na fase 6.4 isso vai ser gerado fora da thread
//principal, e contexto OpenGL nao atravessa thread.
struct MeshData
{
    //Opaco e recorte juntos: o discard no shader resolve vidro e folha,
    //e nenhum dos dois depende de ordem de desenho.
    MeshBuffer solid;
    //Agua. Vai num segundo passe, com blending e ordenada.
    MeshBuffer water;

    int faceCount() const { return solid.faceCount() + water.faceCount(); }
};

//Emite apenas as faces que dao pro ar ou pra um bloco transparente.
//Face entre dois blocos solidos nao chega a existir.
//Le do instantaneo, que ja inclui a casca do vizinho: e isso que evita
//parede na costura entre chunks.
//As posicoes ja saem em coordenada de mundo, entao o model fica identidade.
void buildChunkMesh(const ChunkSnapshot& snap, MeshData& out);

//Os buffers na GPU. Guarda dois conjuntos: um por passe.
class ChunkMesh
{
public:
    ChunkMesh();

    //Substitui o conteudo dos buffers. Cria na primeira chamada.
    void upload(const MeshData& data);

    void drawSolid() const;
    void drawWater() const;

    bool hasWater() const { return water.indexCount > 0; }

    //Libera os buffers na GPU. Chamado quando o chunk e descarregado;
    //sem isso o streaming vaza um VAO e dois VBOs por chunk.
    void destroy();

private:
    struct Buffers
    {
        unsigned int VAO;
        unsigned int VBO;
        unsigned int EBO;
        int indexCount;
        bool created;
    };

    static void uploadBuffer(Buffers& b, const MeshBuffer& data);
    static void drawBuffer(const Buffers& b);
    static void destroyBuffer(Buffers& b);

    Buffers solid;
    Buffers water;
};

#endif
