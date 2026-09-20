#include "ChunkWorkers.h"
#include "ChunkStorage.h"

ChunkWorkers::ChunkWorkers(const TerrainGenerator& gen, int threadCount)
    : generator(gen), running(true), pendingCount(0)
{
    if (threadCount < 1)
        threadCount = 1;

    for (int i = 0; i < threadCount; i++)
        threads.push_back(std::thread(&ChunkWorkers::workerLoop, this));
}

ChunkWorkers::~ChunkWorkers()
{
    {
        std::lock_guard<std::mutex> lock(inMutex);
        running = false;
    }

    //Acorda todo mundo pra que saiam do wait e terminem.
    inCv.notify_all();

    for (size_t i = 0; i < threads.size(); i++)
    {
        if (threads[i].joinable())
            threads[i].join();
    }
}

void ChunkWorkers::submitGenerate(ChunkPos pos)
{
    {
        std::lock_guard<std::mutex> lock(inMutex);
        genQueue.push_back(pos);
    }

    pendingCount++;
    inCv.notify_one();
}

void ChunkWorkers::submitMesh(ChunkSnapshot&& snap)
{
    {
        std::lock_guard<std::mutex> lock(inMutex);
        meshQueue.push_back(std::move(snap));
    }

    pendingCount++;
    inCv.notify_one();
}

bool ChunkWorkers::popGenerated(GenResult& out)
{
    std::lock_guard<std::mutex> lock(outMutex);

    if (genDone.empty())
        return false;

    out = std::move(genDone.front());
    genDone.pop_front();

    return true;
}

bool ChunkWorkers::popMesh(MeshResult& out)
{
    std::lock_guard<std::mutex> lock(outMutex);

    if (meshDone.empty())
        return false;

    out = std::move(meshDone.front());
    meshDone.pop_front();

    return true;
}

void ChunkWorkers::workerLoop()
{
    while (true)
    {
        ChunkPos genPos{ 0, 0 };
        ChunkSnapshot snap;
        bool haveGen = false;
        bool haveMesh = false;

        {
            std::unique_lock<std::mutex> lock(inMutex);

            inCv.wait(lock, [this] {
                return !running || !genQueue.empty() || !meshQueue.empty();
            });

            //No encerramento, larga o que sobrou: o processo vai fechar mesmo.
            if (!running)
                return;

            //Geracao tem prioridade: sem o chunk gerado nao ha o que meshar,
            //e um chunk que falta deixa buraco visivel no mundo.
            if (!genQueue.empty())
            {
                genPos = genQueue.front();
                genQueue.pop_front();
                haveGen = true;
            }
            else
            {
                snap = std::move(meshQueue.front());
                meshQueue.pop_front();
                haveMesh = true;
            }
        }

        if (haveGen)
        {
            GenResult r;
            r.pos = genPos;
            r.chunk.reset(new Chunk());

            //Disco primeiro: se o jogador ja mexeu neste chunk, o arquivo
            //manda. Senao o gerador reproduz ele do zero.
            //Cada chunk tem o proprio arquivo, entao workers em paralelo
            //nunca leem o mesmo.
            if (!ChunkStorage::load(genPos, *r.chunk))
                generator.generate(*r.chunk, genPos.x, genPos.z);

            std::lock_guard<std::mutex> lock(outMutex);
            genDone.push_back(std::move(r));
        }
        else if (haveMesh)
        {
            MeshResult r;
            r.pos = snap.pos;

            //So le o instantaneo, que e copia exclusiva deste trabalho.
            buildChunkMesh(snap, r.data);

            std::lock_guard<std::mutex> lock(outMutex);
            meshDone.push_back(std::move(r));
        }

        pendingCount--;
    }
}
