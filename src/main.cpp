#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/gxm.h>
#include <stdio.h>
#include <stdlib.h>

#include "vita2d.h"
#include "vitashader.h"

#include "debugScreen.h"

#define printf psvDebugScreenPrintf

SceGxmProgram *
load_program(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    printf("%s = %p\n", filename, fp);
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    SceGxmProgram *buf = (SceGxmProgram *)malloc(len);
    fread(buf, len, 1, fp);
    return buf;
}

void
dump_program(SceGxmProgram *program)
{
    printf(" Check = 0x%08x\n", sceGxmProgramCheck(program));
    printf(" Size = %u\n", sceGxmProgramGetSize(program));
    printf(" Type = 0x%08x\n", sceGxmProgramGetType(program));

    unsigned int params = sceGxmProgramGetParameterCount(program);
    printf(" Params = %u\n", params);
    for (unsigned int i=0; i<params; ++i) {
        const SceGxmProgramParameter *parameter = sceGxmProgramGetParameter(program, i);
        printf("  params[%u] = {cat=0x%08x, name=%s, semantic=0x%08x, type=0x%08x, comp=%u, asize=%u}\n",
                i,
                sceGxmProgramParameterGetCategory(parameter),
                sceGxmProgramParameterGetName(parameter),
                sceGxmProgramParameterGetSemantic(parameter),
                sceGxmProgramParameterGetType(parameter),
                sceGxmProgramParameterGetComponentCount(parameter),
                sceGxmProgramParameterGetArraySize(parameter));
    }
}

struct Vertex {
    float x;
    float y;
    float u;
    float v;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

int main(int argc, char *argv[]) {
	psvDebugScreenInit();
	printf("Hello, world!\n");

        SceGxmProgram *sphere_v = load_program("app0:/sphere_v.gxp");
        SceGxmProgram *sphere_f = load_program("app0:/sphere_f.gxp");

        printf("Vertex Shader:\n");
        dump_program(sphere_v);

        printf("Fragment Shader:\n");
        dump_program(sphere_f);

        sceKernelDelayThread(5*1000000);

        printf("===\n");

        SceCtrlData pad;
        sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

        vita2d_init();

        vita2d_texture *texture = vita2d_load_PNG_file("app0:/Tomarchio_256.png");

        SceGxmContext *gxmContext = vita2d_get_context();
        SceGxmShaderPatcher *shader_patcher = vita2d_get_shader_patcher();

        vitashader::ShaderProgram program(gxmContext, shader_patcher, sphere_v, sphere_f);

        program.add_attribute("aPosition", 2, SCE_GXM_ATTRIBUTE_FORMAT_F32);
        program.add_attribute("aTexCoord", 2, SCE_GXM_ATTRIBUTE_FORMAT_F32);
        program.add_attribute("aColor", 4, SCE_GXM_ATTRIBUTE_FORMAT_U8N);

        auto uColorFrg = program.get_fragment_uniform("uColorFrg");
        auto uColorVtx = program.get_vertex_uniform("uColorVtx");
        auto uProjection = program.get_vertex_uniform("uProjection");

        static const SceGxmBlendInfo blend_info = {
            .colorMask = SCE_GXM_COLOR_MASK_ALL,
            .colorFunc = SCE_GXM_BLEND_FUNC_ADD,
            .alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
            .colorSrc  = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
            .colorDst  = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaSrc  = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
            .alphaDst  = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        };

        auto pprogram = program.create(SCE_GXM_MULTISAMPLE_NONE, &blend_info);

        int idx = 0;

        while (1) {
            sceCtrlPeekBufferPositive(0, &pad, 1);

            if (pad.buttons & SCE_CTRL_START) {
                break;
            }

            vita2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));
            vita2d_start_drawing();
            vita2d_clear_screen();

            int n = 4;
            Vertex *vertices = (Vertex *)vita2d_pool_memalign(n * sizeof(Vertex), sizeof(Vertex));

            Vertex *v = vertices;
            v->x = 0.f; v->y = 0.f; ++v;
            v->x = 0.f; v->y = 1.f; ++v;
            v->x = 1.f; v->y = 0.f; ++v;
            v->x = 1.f; v->y = 1.f; ++v;

            for (int i=0; i<n; ++i) {
                vertices[i].r = vertices[i].g = vertices[i].b = vertices[i].a = 255;
                if (i%2 == 0) {
                    vertices[i].a = 0;
                }

                vertices[i].u = vertices[i].x;
                vertices[i].v = vertices[i].y;
                vertices[i].x = pad.lx + vertices[i].x * 100.f;
                vertices[i].y = pad.ly + vertices[i].y * 100.f;
            }

            uint16_t *indices = (uint16_t *)vita2d_pool_memalign(n * sizeof(uint16_t), sizeof(uint16_t));
            indices[0] = 0;
            indices[1] = 1;
            indices[2] = 2;
            indices[3] = 3;

            pprogram.use(gxmContext);

            sceGxmSetBackPolygonMode(gxmContext, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);

            float color[] = { 0.5f, 1.f, 0.f, 0.5f, };
            color[1] = (idx % 100) / 100.f;
            uColorFrg.set_float(color, 4);

            float color2[] = { 0.5f, 0.f, 1.f, 0.5f, };
            color2[2] = (idx % 200) / 200.f;
            uColorVtx.set_float(color2, 4);

            uProjection.set_float(glm::value_ptr(glm::ortho(0.f, 960.f, 544.f, 0.f)), 16);

            sceGxmSetFragmentTexture(gxmContext, 0, &texture->gxm_tex);
            sceGxmSetVertexStream(gxmContext, 0, vertices);
            sceGxmDraw(gxmContext, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, indices, n);

            vita2d_end_drawing();
            vita2d_swap_buffers();

            ++idx;
        }

        free(sphere_f);
        free(sphere_v);

        vita2d_free_texture(texture);
        vita2d_fini();

        sceKernelExitProcess(0);
        return 0;
}
