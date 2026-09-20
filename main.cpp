#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <cmath>

#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "Block.h"
#include "Chunk.h"
#include "World.h"
#include "ChunkMesher.h"
#include "TerrainGenerator.h"
#include "Raycast.h"

//Globais porque os callbacks do GLFW sao funcoes livres e nao carregam contexto.
Camera camera(glm::vec3(64.0f, 70.0f, 170.0f));
float lastX = 640.0f;
float lastY = 360.0f;
bool firstMouse = true;

//Quantos chunks de raio ficam carregados em volta do jogador.
const int RENDER_DISTANCE = 4;
//Teto por frame pra geracao e pra remesh, pra nao engasgar ao andar.
const int CHUNKS_PER_FRAME = 1;
const int REMESH_PER_FRAME = 2;
//Alcance do raycast, em blocos.
const float REACH = 6.0f;

//Wireframe liga/desliga no F. Serve pra conferir que face interna nao existe.
bool wireframe = false;
bool wireKeyWasDown = false;
bool digKeyWasDown = false;
bool placeKeyWasDown = false;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    //No primeiro frame o delta seria gigante: so guarda a posicao.
    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    //Invertido: o Y da tela cresce pra baixo.
    float yoffset = lastY - (float)ypos;

    lastX = (float)xpos;
    lastY = (float)ypos;

    camera.processMouse(xoffset, yoffset);
}

void processInput(GLFWwindow* window, float deltaTime, World& world)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::DOWN, deltaTime);

    //So na transicao de solto pra pressionado, senao alterna todo frame.
    bool wireKeyDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (wireKeyDown && !wireKeyWasDown)
    {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    }
    wireKeyWasDown = wireKeyDown;

    //G cava e H empilha na coluna em que a camera esta. Vira clique com
    //raycast na fase 5.
    const int TEST_X = (int)std::floor(camera.position.x);
    const int TEST_Z = (int)std::floor(camera.position.z);

    bool digKeyDown = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    if (digKeyDown && !digKeyWasDown)
    {
        for (int y = CHUNK_HEIGHT - 1; y >= 0; y--)
        {
            if (world.getBlock(TEST_X, y, TEST_Z) != BLOCK_AIR)
            {
                world.setBlock(TEST_X, y, TEST_Z, BLOCK_AIR);
                break;
            }
        }
    }
    digKeyWasDown = digKeyDown;

    bool placeKeyDown = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
    if (placeKeyDown && !placeKeyWasDown)
    {
        for (int y = 0; y < CHUNK_HEIGHT; y++)
        {
            if (world.getBlock(TEST_X, y, TEST_Z) == BLOCK_AIR)
            {
                world.setBlock(TEST_X, y, TEST_Z, BLOCK_COBBLESTONE);
                break;
            }
        }
    }
    placeKeyWasDown = placeKeyDown;
}

//Cria janela + contexto OpenGL e carrega o GLAD. Devolve NULL se falhar.
GLFWwindow* initWindow(int width, int height, const char* title)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return NULL;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return NULL;
    }

    glViewport(0, 0, width, height);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    //Esconde e prende o cursor na janela, estilo FPS.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    //Descarta fragmentos atras de outros.
    glEnable(GL_DEPTH_TEST);
    //Descarta faces viradas pra longe da camera (winding CCW = frente).
    glEnable(GL_CULL_FACE);

    return window;
}

typedef std::unordered_map<ChunkPos, ChunkMesh, ChunkPosHash> MeshMap;

//Remesha apenas os chunks marcados como sujos.
//O scratch e reaproveitado entre chamadas pra nao realocar os vetores toda vez.
//Remesha apenas os chunks sujos, no maximo maxRemesh por chamada,
//e joga fora as meshes dos chunks que o streaming ja descarregou.
//Devolve quantos remeshou.
int updateWorld(World& world, MeshMap& meshes, MeshData& scratch, int maxRemesh)
{
    for (MeshMap::iterator it = meshes.begin(); it != meshes.end(); )
    {
        if (world.getChunk(it->first) == NULL)
        {
            it->second.destroy();
            it = meshes.erase(it);
        }
        else
        {
            ++it;
        }
    }

    int done = 0;

    for (ChunkMap::const_iterator it = world.allChunks().begin(); it != world.allChunks().end(); ++it)
    {
        if (done >= maxRemesh)
            break;

        Chunk* chunk = it->second.get();
        if (!chunk->dirty)
            continue;

        buildChunkMesh(world, it->first, scratch);
        meshes[it->first].upload(scratch);
        chunk->dirty = false;

        done++;
    }

    return done;
}

//12 arestas de um cubo centrado na origem, pra desenhar com GL_LINES.
//Cada aresta sao 2 vertices, entao 24 no total.
unsigned int createWireCube()
{
    const float h = 0.5f;
    float v[] = {
        //base
        -h,-h,-h,  h,-h,-h,   h,-h,-h,  h,-h, h,   h,-h, h, -h,-h, h,  -h,-h, h, -h,-h,-h,
        //topo
        -h, h,-h,  h, h,-h,   h, h,-h,  h, h, h,   h, h, h, -h, h, h,  -h, h, h, -h, h,-h,
        //colunas
        -h,-h,-h, -h, h,-h,   h,-h,-h,  h, h,-h,   h,-h, h,  h, h, h,  -h,-h, h, -h, h, h
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    return VAO;
}

//Contorno do bloco que a mira esta apontando.
void drawSelection(const Shader& lineShader, unsigned int wireVAO, const RaycastHit& hit,
    const glm::mat4& view, const glm::mat4& projection)
{
    if (!hit.hit)
        return;

    //Centro do bloco, levemente inflado pra nao brigar em z com a face.
    glm::mat4 model = glm::translate(glm::mat4(1.0f),
        glm::vec3((float)hit.x + 0.5f, (float)hit.y + 0.5f, (float)hit.z + 0.5f));
    model = glm::scale(model, glm::vec3(1.004f));

    lineShader.use();
    lineShader.setMat4("model", model);
    lineShader.setMat4("view", view);
    lineShader.setMat4("projection", projection);
    lineShader.setVec3("lineColor", glm::vec3(0.05f, 0.05f, 0.05f));

    glBindVertexArray(wireVAO);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);
}

//Desenha um frame.
void render(const Shader& shader, const Shader& lineShader, const Texture& texture,
    const MeshMap& meshes, unsigned int wireVAO, const RaycastHit& hit, float aspect)
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = camera.getView();
    glm::mat4 projection = camera.getProjection(aspect);

    shader.use();
    texture.bind(0);

    //O chunk ja nasce em coordenada de mundo, entao model e identidade.
    shader.setMat4("model", glm::mat4(1.0f));
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);

    //Um draw call por chunk. Nunca um por bloco.
    for (MeshMap::const_iterator it = meshes.begin(); it != meshes.end(); ++it)
        it->second.draw();

    drawSelection(lineShader, wireVAO, hit, view, projection);
}

int main()
{
    GLFWwindow* window = initWindow(1280, 720, "openglEngine");
    if (window == NULL)
        return -1;

    Shader basicShader("shaders/basic.vert", "shaders/basic.frag");
    Shader lineShader("shaders/line.vert", "shaders/line.frag");
    Texture atlas("textures/atlas.png");

    unsigned int wireVAO = createWireCube();

    TerrainGenerator terrain(1337);
    World world;

    MeshData meshScratch;
    MeshMap meshes;

    //Nasce em pe no terreno, nao no vazio.
    camera.position = glm::vec3(8.0f, (float)terrain.heightAt(8, 8) + 3.0f, 8.0f);

    //Primeira carga sem limite, senao o jogador comecaria olhando pro nada.
    //Do segundo frame em diante o limite por frame entra em acao.
    double fillStart = glfwGetTime();
    ChunkPos spawn = World::chunkAt(camera.position.x, camera.position.z);
    world.streamAround(spawn, RENDER_DISTANCE, terrain, 100000);
    updateWorld(world, meshes, meshScratch, 100000);
    double fillTime = glfwGetTime() - fillStart;

    std::cout << "carga inicial: " << world.chunkCount() << " chunks em "
        << (int)(fillTime * 1000.0) << " ms | raio " << RENDER_DISTANCE
        << " chunks (" << RENDER_DISTANCE * CHUNK_SIZE << " blocos)" << std::endl;
    std::cout << "WASD + mouse voa, G cava, H empilha, F wireframe" << std::endl;

    //O sampler le da unidade 0. Precisa ser setado uma vez, com o shader ativo.
    basicShader.use();
    basicShader.setInt("blockTexture", 0);

    //Comeca do relogio atual, nao de zero: a carga inicial levou segundos,
    //e o primeiro deltaTime seria esse tempo todo de uma vez. Com uma tecla
    //pressionada, isso teleporta o jogador dezenas de blocos no frame 1.
    float lastFrame = (float)glfwGetTime();
    float statTimer = 0.0f;
    int frames = 0;

    //Cada volta desse loop e um frame. Roda ate o ESC ou o X da janela.
    while (!glfwWindowShouldClose(window))
    {
        //1. Quanto tempo passou desde o frame anterior, em segundos.
        //   E o que deixa a velocidade igual em qualquer FPS: a 30 fps o deltaTime
        //   e o dobro do de 60 fps, entao a camera anda o dobro por frame e chega junto.
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        //2. Le o teclado e move a camera.
        //   O mouse NAO passa por aqui: ele chega sozinho pelo mouse_callback,
        //   que quem dispara e o glfwPollEvents la no passo 8.
        processInput(window, deltaTime, world);

        //3. Streaming: gera o que entrou no raio, descarta o que saiu,
        //   e remesha os sujos. Tudo com teto por frame.
        ChunkPos center = World::chunkAt(camera.position.x, camera.position.z);
        world.streamAround(center, RENDER_DISTANCE, terrain, CHUNKS_PER_FRAME);
        updateWorld(world, meshes, meshScratch, REMESH_PER_FRAME);


        //4. Raycast da camera pra frente, pra saber qual bloco esta na mira.
        RaycastHit hit = raycast(world, camera.position, camera.front, REACH);

        //Status uma vez por segundo, pra nao inundar o console.
        statTimer += deltaTime;
        frames++;
        if (statTimer >= 1.0f)
        {
            std::cout << "fps " << frames
                << " | pos " << (int)camera.position.x << "," << (int)camera.position.y << "," << (int)camera.position.z
                << " | yaw " << (int)camera.yaw << " pitch " << (int)camera.pitch
                << " | chunk (" << center.x << ", " << center.z << ")"
                << " | carregados " << world.chunkCount();
            if (hit.hit)
            {
                std::cout << " | mira (" << hit.x << "," << hit.y << "," << hit.z
                    << ") bloco " << (int)world.getBlock(hit.x, hit.y, hit.z)
                    << " normal (" << hit.nx << "," << hit.ny << "," << hit.nz << ")";
            }
            else
            {
                std::cout << " | mira: nada";
            }
            std::cout << std::endl;
            statTimer = 0.0f;
            frames = 0;
        }

        //5. Pega o tamanho atual do framebuffer pra calcular o aspecto (largura/altura).
        //   Se le todo frame porque a janela pode ter sido redimensionada.
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        //6. Desenha o frame no buffer de tras (limpa a tela, ativa o shader,
        //   manda as matrizes e desenha os chunks).
        //   Com a janela minimizada a altura vira 0: nao da pra dividir e nao ha o que desenhar.
        if (fbHeight > 0)
            render(basicShader, lineShader, atlas, meshes, wireVAO, hit, (float)fbWidth / (float)fbHeight);

        //7. Troca o buffer de tras com o da frente. So agora o frame aparece na tela.
        glfwSwapBuffers(window);

        //8. Processa a fila de eventos do sistema: teclado, mouse, resize, botao de fechar.
        //   E aqui que o mouse_callback e o framebuffer_size_callback sao chamados.
        //   Sem isso a janela congela e o Windows marca como "nao responde".
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
