#include "render.h"

#include <cell/gcm.h>
using namespace cell::Gcm;
#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;
#include <sys/paths.h>

#include "gcmutil.h"
using namespace CellGcmUtil;

#include "template/sampleBasic.h"
using namespace CellGcmUtil::SampleBasic;

namespace Render
{
    namespace
    {
        const int32_t COLOR_BUF_NUM = 2;
        CellGcmSurface sSurface[COLOR_BUF_NUM];
        Memory_t sFrameBuffer;
        Memory_t sDepthBuffer;
        CellGcmTexture sFrameTexture[COLOR_BUF_NUM];
        CellGcmTexture sDepthTexture;
        float sDisplayAspectRatio = 1.0f;

        Shader_t sVShader;
        Shader_t sFShader;
        Memory_t sFShaderUCode;
        const uint32_t VS_UF_MVP_MATRIX = 0;

        #define VERTEX_SHADER   SYS_APP_HOME "/vs_quad.vpo"
        #define FRAGMENT_SHADER SYS_APP_HOME "/fs_quad.fpo"

        // Packed color is read as CELL_GCM_VERTEX_UB,4 (unsigned byte, normalized) --
        // not float, unlike the gcmutil "basic2" sample's vertex color setup, which
        // reads a 3-component float span over what is actually a 4-byte packed ARGB
        // (reading past the field into the next vertex). Byte order for the fixed
        // COLOR attribute is GPU-convention BGRA; this hasn't been verified on real
        // hardware yet -- if colors come out channel-swapped on first hardware test,
        // swap the byte order written in DrawQuad below.
        struct QuadVertex { float x, y, z; uint32_t argb; };
        const uint32_t MAX_QUADS = 4096;
        Memory_t sVB;
        QuadVertex *sVBPtr = 0;
        uint32_t sQuadCount = 0;
    }

    bool Init()
    {
        const int32_t CB_SIZE   = 0x00800000; //   8MB
        const int32_t MAIN_SIZE = 0x08000000; // 128MB
        if (!cellGcmUtilInit(CB_SIZE, MAIN_SIZE)) return false;

        CellVideoOutResolution reso = cellGcmUtilGetResolution();
        const uint8_t color_format = CELL_GCM_TEXTURE_A8R8G8B8;
        const uint8_t depth_format = CELL_GCM_TEXTURE_DEPTH24_D8;

        if (!cellGcmUtilCreateTiledTexture(reso.width, reso.height, color_format, CELL_GCM_LOCATION_LOCAL,
                                            CELL_GCM_COMPMODE_C32_2X1, COLOR_BUF_NUM, sFrameTexture, &sFrameBuffer))
            return false;

        if (!cellGcmUtilCreateDepthTexture(reso.width, reso.height, depth_format, CELL_GCM_LOCATION_LOCAL,
                                            CELL_GCM_SURFACE_CENTER_1, true, true, &sDepthTexture, &sDepthBuffer))
            return false;

        for (int32_t i = 0; i < COLOR_BUF_NUM; ++i)
            sSurface[i] = cellGcmUtilTextureToSurface(&sFrameTexture[i], &sDepthTexture);

        if (!cellGcmUtilSetDisplayBuffer(COLOR_BUF_NUM, sFrameTexture)) return false;

        // Wire into the SampleBasic template's flip()/swap-chain bookkeeping.
        gSampleApp.nFrameNumber = COLOR_BUF_NUM;
        gSampleApp.nFrameIndex = 0;
        gSampleApp.p_vSurface = sSurface;

        sDisplayAspectRatio = cellGcmUtilGetAspectRatio();

        if (!cellGcmUtilLoadShader(VERTEX_SHADER, &sVShader)) return false;
        if (!cellGcmUtilLoadShader(FRAGMENT_SHADER, &sFShader)) return false;
        if (!cellGcmUtilGetFragmentUCode(&sFShader, CELL_GCM_LOCATION_LOCAL, &sFShaderUCode)) return false;

        // Dynamic, CPU-written-every-frame quad buffer -- kept in MAIN memory
        // (not LOCAL/VRAM) since the CPU rewrites it every DrawQuad call.
        if (!cellGcmUtilAllocateMain(MAX_QUADS * 4 * sizeof(QuadVertex), 128, &sVB)) return false;
        sVBPtr = reinterpret_cast<QuadVertex *>(sVB.addr);

        return true;
    }

    void Shutdown()
    {
        cellGcmUtilFree(&sVB);
        cellGcmUtilDestroyShader(&sVShader);
        cellGcmUtilDestroyShader(&sFShader);
        cellGcmUtilFree(&sFShaderUCode);
        cellGcmUtilFree(&sFrameBuffer);
        cellGcmUtilFree(&sDepthBuffer);
    }

    void BeginFrame(Vec2 camPos, float orthoHalfHeight)
    {
        sQuadCount = 0;

        cellGcmSetDepthTestEnable(CELL_GCM_FALSE);
        cellGcmSetBlendEnable(CELL_GCM_TRUE);
        cellGcmSetBlendEquation(CELL_GCM_FUNC_ADD, CELL_GCM_FUNC_ADD);
        cellGcmSetBlendFunc(CELL_GCM_SRC_ALPHA, CELL_GCM_ONE_MINUS_SRC_ALPHA, CELL_GCM_ONE, CELL_GCM_ONE_MINUS_SRC_ALPHA);
        cellGcmSetClearColor(0xFF202028);
        cellGcmSetFrontFace(CELL_GCM_CCW);
        cellGcmSetCullFaceEnable(CELL_GCM_FALSE);

        Viewport_t vp = cellGcmUtilGetViewportGL(sSurface[0].height, 0, 0, sSurface[0].width, sSurface[0].height, 0.0f, 1.0f);
        cellGcmSetViewport(vp.x, vp.y, vp.width, vp.height, vp.min, vp.max, vp.scale, vp.offset);

        cellGcmSetClearSurface(CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_A | CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B);

        float halfW = orthoHalfHeight * sDisplayAspectRatio;
        float l = camPos.x - halfW, r = camPos.x + halfW;
        float b = camPos.y - orthoHalfHeight, t = camPos.y + orthoHalfHeight;
        float n = -1.0f, f = 1.0f;

        float sx = 2.0f / (r - l), sy = 2.0f / (t - b), sz = -2.0f / (f - n);
        float tx = -(r + l) / (r - l), ty = -(t + b) / (t - b), tz = -(f + n) / (f - n);

        Matrix4 ortho(Vector4(sx, 0.0f, 0.0f, 0.0f),
                      Vector4(0.0f, sy, 0.0f, 0.0f),
                      Vector4(0.0f, 0.0f, sz, 0.0f),
                      Vector4(tx, ty, tz, 1.0f));
        Matrix4 mvp = transpose(ortho);

        cellGcmSetVertexProgram(sVShader.program, sVShader.ucode);
        cellGcmSetFragmentProgram(sFShader.program, sFShaderUCode.offset);
        cellGcmSetVertexProgramConstants(VS_UF_MVP_MATRIX, 16, reinterpret_cast<const float *>(&mvp));
    }

    void DrawQuad(Vec2 center, Vec2 size, float rotationDeg, unsigned int argb)
    {
        if (sQuadCount >= MAX_QUADS) return;

        float rad = rotationDeg * (3.14159265f / 180.0f);
        float cs = cosf(rad), sn = sinf(rad);
        float hx = size.x * 0.5f, hy = size.y * 0.5f;

        float lx[4] = { -hx,  hx,  hx, -hx };
        float ly[4] = { -hy, -hy,  hy,  hy };

        QuadVertex *v = sVBPtr + sQuadCount * 4;
        for (int i = 0; i < 4; i++)
        {
            v[i].x = center.x + (lx[i] * cs - ly[i] * sn);
            v[i].y = center.y + (lx[i] * sn + ly[i] * cs);
            v[i].z = 0.0f;
            v[i].argb = argb;
        }

        uint32_t byteOffset = sVB.offset + sQuadCount * 4 * sizeof(QuadVertex);

        cellGcmSetInvalidateVertexCache();
        cellGcmSetVertexDataArray(CELL_GCMUTIL_ATTR_POSITION, 0, sizeof(QuadVertex), 3, CELL_GCM_VERTEX_F,
                                   sVB.location, byteOffset);
        cellGcmSetVertexDataArray(CELL_GCMUTIL_ATTR_COLOR, 0, sizeof(QuadVertex), 4, CELL_GCM_VERTEX_UB,
                                   sVB.location, byteOffset + sizeof(float) * 3);

        cellGcmSetDrawArrays(CELL_GCM_PRIMITIVE_TRIANGLE_FAN, 0, 4);

        cellGcmSetVertexDataArray(CELL_GCMUTIL_ATTR_POSITION, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
        cellGcmSetVertexDataArray(CELL_GCMUTIL_ATTR_COLOR, 0, 0, 0, CELL_GCM_VERTEX_UB, CELL_GCM_LOCATION_LOCAL, 0);

        sQuadCount++;
    }

    TextureHandle LoadTexture(const char * /*path*/)
    {
        // TODO(TODO.md: PS3 texture pipeline): real sprites need PNG->DDS->GTF
        // conversion (dds2gtf, D:/PS3/host-win32/bin) plus cellGcmUtilLoadTexture
        // and a textured variant of vs_quad.cg/fs_quad.cg (UV attribute + sampler).
        // Until that's wired up, every "textured" quad below just draws flat-
        // colored so the PS3 build keeps compiling and running.
        return 0;
    }

    void DrawTexturedQuad(TextureHandle /*tex*/, Vec2 center, Vec2 size, float rotationDeg, unsigned int tintArgb)
    {
        DrawQuad(center, size, rotationDeg, tintArgb);
    }

    void EndFrame()
    {
        // Flip is driven by the SampleBasic template's main loop (main_ps3.cpp),
        // not here -- nothing to do per-DrawQuad-batch beyond what BeginFrame set up.
    }

    bool ShouldQuit()
    {
        // Exit is owned by gSampleApp.isKeepRunning via the sysutil
        // CELL_SYSUTIL_REQUEST_EXITGAME callback; main_ps3.cpp doesn't need to
        // poll this itself.
        return false;
    }
}
