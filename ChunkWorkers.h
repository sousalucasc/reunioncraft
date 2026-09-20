#ifndef CHUNKWORKERS_H
#define CHUNKWORKERS_H

#include "ChunkMesher.h"
#include "TerrainGenerator.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct GenResult
{
    ChunkPos pos;
    std::unique_ptr<Chunk> chunk;
};

struct MeshResult
{
    ChunkPos pos;
    MeshData data;
};

//Pool de threads que faz o trabalho pesado fora da thread principal.
//
//Por que isso e seguro sem mutex no World: nenhum worker toca no World.
//Gerar terreno so depende do TerrainGenerator, que e somente leitura.
//Fazer mesh so depende do ChunkSnapshot, que e uma copia feita pela thread
//principal antes de submeter o trabalho.
//
//Contexto OpenGL nao atravessa thread, entao o upload pra GPU continua
//sendo da thread principal. Os workers so preenchem vetores.
class ChunkWorkers
{
public:
    ChunkWorkers(const TerrainGenerator& generator, int threadCount);
    ~ChunkWorkers();

    //Submissao e coleta: so pela thread principal.
    void submitGenerate(ChunkPos pos);
    void submitMesh(ChunkSnapshot&& snap);

    //Devolvem false quando nao ha resultado pronto.
    bool popGenerated(GenResult& out);
    bool popMesh(MeshResult& out);

    //Trabalhos submetidos que ainda nao viraram resultado.
    int pending() const { return pendingCount.load(); }

    int threadCount() const { return (int)threads.size(); }

private:
    void workerLoop();

    const TerrainGenerator& generator;

    std::vector<std::thread> threads;

    //Protege as filas de entrada.
    std::mutex inMutex;
    std::condition_variable inCv;
    std::deque<ChunkPos> genQueue;
    std::deque<ChunkSnapshot> meshQueue;
    bool running;

    //Protege as filas de saida. Separado da entrada pra que um worker
    //entregando resultado nao trave outro que esta pegando trabalho.
    std::mutex outMutex;
    std::deque<GenResult> genDone;
    std::deque<MeshResult> meshDone;

    std::atomic<int> pendingCount;
};

#endif
