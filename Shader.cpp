#include "Shader.h"

#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

//Le um arquivo inteiro pra uma string. Devolve false se nao conseguir abrir.
static bool readFile(const char* path, std::string& out)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cout << "ERROR::SHADER::FILE_NOT_READ: " << path << std::endl;
        return false;
    }

    std::stringstream stream;
    stream << file.rdbuf();
    out = stream.str();

    return true;
}

bool Shader::checkErrors(unsigned int object, const char* label, bool isProgram)
{
    int success;
    char infoLog[1024];

    if (isProgram)
    {
        glGetProgramiv(object, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(object, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER::LINK_FAILED (" << label << ")\n" << infoLog << std::endl;
        }
    }
    else
    {
        glGetShaderiv(object, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(object, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER::COMPILE_FAILED (" << label << ")\n" << infoLog << std::endl;
        }
    }

    return success != 0;
}

unsigned int Shader::compile(GLenum type, const char* source, const char* label)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    checkErrors(shader, label, false);

    return shader;
}

Shader::Shader(const char* vertexPath, const char* fragmentPath)
    : ID(0)
{
    std::string vertexCode;
    std::string fragmentCode;

    if (!readFile(vertexPath, vertexCode) || !readFile(fragmentPath, fragmentCode))
        return;

    unsigned int vertex = compile(GL_VERTEX_SHADER, vertexCode.c_str(), vertexPath);
    unsigned int fragment = compile(GL_FRAGMENT_SHADER, fragmentCode.c_str(), fragmentPath);

    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkErrors(ID, "program", true);

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void Shader::use() const
{
    glUseProgram(ID);
}

void Shader::setBool(const std::string& name, bool value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}
