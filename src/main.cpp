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
    float z;
    float u;
    float v;
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

        vita2d_texture *texture = vita2d_load_PNG_file("app0:/stb_font_SourceSansProSemiBold.png");
        vita2d_texture_set_filters(texture, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);

        SceGxmContext *gxmContext = vita2d_get_context();
        SceGxmShaderPatcher *shader_patcher = vita2d_get_shader_patcher();

        {
            vitashader::ShaderProgram program(gxmContext, shader_patcher, sphere_v, sphere_f);

            program.add_attribute("aPosition", 3, SCE_GXM_ATTRIBUTE_FORMAT_F32);
            program.add_attribute("aTexCoord", 2, SCE_GXM_ATTRIBUTE_FORMAT_F32);

            auto uColor = program.get_vertex_uniform("uColor");
            auto uTransform = program.get_vertex_uniform("uTransform");
            auto uProjection = program.get_vertex_uniform("uProjection");
            auto uShadowOffset = program.get_fragment_uniform("uShadowOffset");

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

            float dx = 0.f, dy = 0.f;
            float sx = 1.f, sy = 1.f;

            while (1) {
                sceCtrlPeekBufferPositive(0, &pad, 1);

                if (pad.buttons & SCE_CTRL_START) {
                    break;
                }

                if (pad.buttons & SCE_CTRL_SELECT) {
                    dx = dy = 0.f;
                    sx = sy = 1.f;
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

                float lxf = ((int)pad.lx - 127) / 127.f;
                float lyf = ((int)pad.ly - 127) / 127.f;
                float rxf = ((int)pad.rx - 127) / 127.f;
                float ryf = ((int)pad.ry - 127) / 127.f;

                if (fabsf(lxf) > 0.2f) {
                    dx += 2.f * lxf;
                }
                if (fabsf(lyf) > 0.2f) {
                    dy += 2.f * lyf;
                }
                if (fabsf(rxf) > 0.2f) {
                    sx *= 1.f + 0.2f * rxf;
                }
                if (fabsf(ryf) > 0.2f) {
                    sy *= 1.f + 0.2f * ryf;
                }

                for (int i=0; i<n; ++i) {
                    vertices[i].u = vertices[i].x;
                    vertices[i].v = vertices[i].y;
                    vertices[i].x = vertices[i].x * 900.f;
                    vertices[i].y = vertices[i].y * 900.f;
                    vertices[i].z = (sx + sy) / 2.f;
                }

                uint16_t *indices = (uint16_t *)vita2d_pool_memalign(n * sizeof(uint16_t), sizeof(uint16_t));
                indices[0] = 0;
                indices[1] = 1;
                indices[2] = 2;
                indices[3] = 3;

                pprogram.use(gxmContext);

                sceGxmSetBackPolygonMode(gxmContext, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);

                float color[] = { 0.5f, 1.f, 1.f, 1.0f, };
                color[1] = (idx % 100) / 100.f;
                uColor.set_float(color, 4);

                float transform[] = {
                    dx, dy,
                    sx, sy,
                };
                uTransform.set_float(transform, 4);

                float shadow_offset[] = { 1.f / 512.f, 1.f / 512.f };
                uShadowOffset.set_float(shadow_offset, 2);

                uProjection.set_float(glm::value_ptr(glm::ortho(0.f, 960.f, 544.f, 0.f)), 16);

                sceGxmSetFragmentTexture(gxmContext, 0, &texture->gxm_tex);
                sceGxmSetVertexStream(gxmContext, 0, vertices);
                sceGxmDraw(gxmContext, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, indices, n);

                vita2d_wait_rendering_done();
                vita2d_end_drawing();
                vita2d_swap_buffers();

                ++idx;
            }
        }

        vita2d_wait_rendering_done();
        vita2d_free_texture(texture);
        vita2d_fini();

        free(sphere_f);
        free(sphere_v);

        sceKernelExitProcess(0);
        return 0;
}
