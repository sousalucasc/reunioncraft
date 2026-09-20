#ifndef PLAYER_H
#define PLAYER_H

#include "World.h"

#include <glm/glm.hpp>

//Caixa de colisao do jogador, nas mesmas medidas do Minecraft.
constexpr float PLAYER_WIDTH = 0.6f;
constexpr float PLAYER_HEIGHT = 1.8f;
constexpr float PLAYER_EYE = 1.62f;

class Player
{
public:
    //Centro da base da caixa, ou seja, entre os pes.
    glm::vec3 position;
    glm::vec3 velocity;

    bool onGround;
    //Modo criativo: atravessa tudo e ignora gravidade.
    bool flying;

    Player(const glm::vec3& spawn);

    //De onde a camera enxerga.
    glm::vec3 eyePosition() const;

    //wishDir e a direcao desejada, ja normalizada. No modo andando so o XZ
    //importa; voando o Y tambem e usado.
    void update(const World& world, const glm::vec3& wishDir, bool jump, float dt);

private:
    //Tenta mover num unico eixo e resolve a colisao encostando na face.
    //Resolver um eixo por vez e o que permite deslizar na parede em vez de
    //travar: bater em X nao impede o movimento em Z.
    void moveAxis(const World& world, int axis, float amount);

    //true se a caixa nesta posicao encosta em algum bloco solido.
    bool collides(const World& world, const glm::vec3& pos) const;
};

#endif
