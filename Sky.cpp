#include "Sky.h"

#include <cmath>

//Paletas dos tres momentos. A transicao entre elas e feita por peso.
static const glm::vec3 DIA_ZENITE(0.35f, 0.55f, 0.90f);
static const glm::vec3 DIA_HORIZONTE(0.70f, 0.82f, 0.95f);

static const glm::vec3 CREPUSCULO_ZENITE(0.24f, 0.28f, 0.52f);
static const glm::vec3 CREPUSCULO_HORIZONTE(0.95f, 0.52f, 0.24f);

static const glm::vec3 NOITE_ZENITE(0.02f, 0.03f, 0.09f);
static const glm::vec3 NOITE_HORIZONTE(0.06f, 0.08f, 0.17f);

//Piso de luz da noite. Abaixo disso o jogo fica injogavel.
static const float LUZ_NOTURNA = 0.22f;

static float smoothstep01(float borda0, float borda1, float x)
{
    float t = (x - borda0) / (borda1 - borda0);

    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    return t * t * (3.0f - 2.0f * t);
}

SkyState skyAt(float dayTime)
{
    //Mantem no intervalo 0..1 mesmo se o tempo acumulado passar disso.
    dayTime = dayTime - std::floor(dayTime);

    SkyState s;

    //Altura do sol: -1 a meia-noite, +1 ao meio-dia.
    const float TAU = 6.2831853f;
    s.sunHeight = -std::cos(dayTime * TAU);

    //Pesos de dia e de noite. O que sobra e crepusculo, que e justamente a
    //faixa estreita em que o sol esta perto do horizonte.
    float dia = smoothstep01(0.05f, 0.35f, s.sunHeight);
    float noite = smoothstep01(-0.05f, -0.35f, s.sunHeight);
    float crepusculo = 1.0f - dia - noite;

    if (crepusculo < 0.0f)
        crepusculo = 0.0f;

    s.zenith = DIA_ZENITE * dia + CREPUSCULO_ZENITE * crepusculo + NOITE_ZENITE * noite;
    s.horizon = DIA_HORIZONTE * dia + CREPUSCULO_HORIZONTE * crepusculo + NOITE_HORIZONTE * noite;

    //A luz acompanha o sol, mas parte de um piso em vez de zero.
    float brilho = smoothstep01(-0.15f, 0.30f, s.sunHeight);
    s.lightLevel = LUZ_NOTURNA + (1.0f - LUZ_NOTURNA) * brilho;

    return s;
}
