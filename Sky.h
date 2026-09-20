#ifndef SKY_H
#define SKY_H

#include <glm/glm.hpp>

//Quanto tempo real dura um dia inteiro no jogo, em segundos.
//O Minecraft usa 20 minutos; aqui e mais curto pra dar pra ver o ciclo.
constexpr float DAY_LENGTH = 240.0f;

struct SkyState
{
    //Cor no alto e na linha do horizonte. O shader interpola entre as duas
    //conforme a direcao pra onde o fragmento olha.
    glm::vec3 zenith;
    glm::vec3 horizon;

    //Multiplicador de luz aplicado ao mundo. Nunca chega a zero, senao a
    //noite vira uma tela preta em que nao da pra jogar.
    float lightLevel;

    //-1 a 1. Negativo e noite. So pra diagnostico.
    float sunHeight;

    //Direcao normalizada da camera PRO sol. E o que a sombra usa pra saber
    //de onde a luz vem.
    glm::vec3 sunDir;
};

//dayTime vai de 0 a 1: 0 e meia-noite, 0.25 amanhecer, 0.5 meio-dia.
SkyState skyAt(float dayTime);

#endif
