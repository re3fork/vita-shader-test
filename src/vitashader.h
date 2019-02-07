#pragma once

#include <psp2/gxm.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

namespace vitashader {

struct PatchedProgram {
    PatchedProgram(SceGxmShaderPatcher *patcher,
            SceGxmVertexProgram *vertexProgram,
            SceGxmFragmentProgram *fragmentProgram)
        : shaderPatcher(shaderPatcher)
        , vertexProgram(vertexProgram)
        , fragmentProgram(fragmentProgram)
    {
    }

    ~PatchedProgram()
    {
        if (fragmentProgram) {
            sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, fragmentProgram);
        }
        if (vertexProgram) {
            sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, vertexProgram);
        }
    }

    PatchedProgram(const PatchedProgram &) = delete;

    PatchedProgram(PatchedProgram &&other)
        : vertexProgram(other.vertexProgram)
        , fragmentProgram(other.fragmentProgram)
    {
        other.vertexProgram = nullptr;
        other.fragmentProgram = nullptr;
    }

    void use(SceGxmContext *context)
    {
        sceGxmSetVertexProgram(context, vertexProgram);
        sceGxmSetFragmentProgram(context, fragmentProgram);
    }

    SceGxmShaderPatcher *shaderPatcher;
    SceGxmVertexProgram *vertexProgram;
    SceGxmFragmentProgram *fragmentProgram;
};

struct VertexAttribute {
    VertexAttribute(const char *name, uint8_t components, SceGxmAttributeFormat format)
        : name(name)
        , components(components)
        , format(format)
    {
    }

    const char *name;
    uint8_t components;
    SceGxmAttributeFormat format;
};

static size_t
attribute_format_to_size(SceGxmAttributeFormat format)
{
    switch (format) {
        case SCE_GXM_ATTRIBUTE_FORMAT_F32:
            return 4;
        case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
            return 1;
        default:
            return -1;
    }
}

struct UniformVariable {
    UniformVariable(SceGxmContext *context, const SceGxmProgramParameter *parameter, bool isVertex)
        : context(context)
        , parameter(parameter)
        , isVertex(isVertex)
    {
    }

    void set_matrix(const glm::mat4 &matrix) {
        set_float(glm::value_ptr(matrix), 16);
    }

    void set_vector(const glm::vec4 &vector) {
        set_float(glm::value_ptr(vector), 4);
    }

    void set_float(const float *value, size_t components) {
        void *buffer;
        if (isVertex) {
            sceGxmReserveVertexDefaultUniformBuffer(context, &buffer);
        } else {
            sceGxmReserveFragmentDefaultUniformBuffer(context, &buffer);
        }
        sceGxmSetUniformDataF(buffer, parameter, 0, components, value);
    }

    SceGxmContext *context;
    const SceGxmProgramParameter *parameter;
    bool isVertex;
};

struct ShaderProgram {
    ShaderProgram(SceGxmContext *context, SceGxmShaderPatcher *shaderPatcher,
            const SceGxmProgram *vertexProgram, const SceGxmProgram *fragmentProgram)
        : context(context)
        , shaderPatcher(shaderPatcher)
        , vertexProgram(vertexProgram)
        , fragmentProgram(fragmentProgram)
    {
        int err;

        err = sceGxmProgramCheck(vertexProgram);
        if (err != 0) { printf("Failure when checking vertex program\n"); }
        err = sceGxmProgramCheck(fragmentProgram);
        if (err != 0) { printf("Failure when checking fragment program\n"); }

        err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, vertexProgram, &vertexProgramId);
        if (err != 0) { printf("Failure when registering vertex program\n"); }
        err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, fragmentProgram, &fragmentProgramId);
        if (err != 0) { printf("Failure when registering fragment program\n"); }
    }

    ~ShaderProgram()
    {
        sceGxmShaderPatcherUnregisterProgram(shaderPatcher, fragmentProgramId);
        sceGxmShaderPatcherUnregisterProgram(shaderPatcher, vertexProgramId);
    }

    void add_attribute(const char *name, uint8_t components, SceGxmAttributeFormat format)
    {
        attributes.emplace_back(name, components, format);
    }

    UniformVariable get_vertex_uniform(const char *name)
    {
        return UniformVariable(context, sceGxmProgramFindParameterByName(vertexProgram, name), true);
    }

    UniformVariable get_fragment_uniform(const char *name)
    {
        return UniformVariable(context, sceGxmProgramFindParameterByName(fragmentProgram, name), false);
    }

    PatchedProgram create(SceGxmMultisampleMode msaa, const SceGxmBlendInfo *blend_info)
    {
        SceGxmVertexProgram *outVertexProgram;
        SceGxmFragmentProgram *outFragmentProgram;

        uint16_t offset = 0;
        std::vector<SceGxmVertexAttribute> gxmAttributes;

        for (auto &attribute: attributes) {
            const SceGxmProgramParameter *param =
                sceGxmProgramFindParameterByName(vertexProgram, attribute.name);
            uint16_t index = sceGxmProgramParameterGetResourceIndex(param);

            gxmAttributes.emplace_back(SceGxmVertexAttribute{0, offset, attribute.format,
                    attribute.components, index});

            offset += attribute.components * attribute_format_to_size(attribute.format);
        }

        uint16_t stride = offset;
        std::vector<SceGxmVertexStream> gxmStreams = {
            { stride, SCE_GXM_INDEX_SOURCE_INDEX_16BIT, },
        };

        int err;

        err = sceGxmShaderPatcherCreateVertexProgram(shaderPatcher,
                vertexProgramId,
                gxmAttributes.data(),
                gxmAttributes.size(),
                gxmStreams.data(),
                gxmStreams.size(),
                &outVertexProgram);

        if (err != 0) { printf("Could not create vertex program\n"); }

        err = sceGxmShaderPatcherCreateFragmentProgram(shaderPatcher,
                fragmentProgramId,
                SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
                msaa,
                blend_info,
                vertexProgram,
                &outFragmentProgram);

        if (err != 0) { printf("Could not create fragment program\n"); }

        return PatchedProgram(shaderPatcher, outVertexProgram, outFragmentProgram);
    }

    SceGxmContext *context;
    SceGxmShaderPatcher *shaderPatcher;
    const SceGxmProgram *vertexProgram;
    const SceGxmProgram *fragmentProgram;
    SceGxmShaderPatcherId vertexProgramId;
    SceGxmShaderPatcherId fragmentProgramId;

    std::vector<VertexAttribute> attributes;
};

} // end namespace vitashader
