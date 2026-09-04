/* PS3 entry point. Delegates all real work to the shared App:: module
 * (src/app.cpp) via the gcmutil SampleBasic callback template -- see
 * D:/PS3/samples/common/gcmutil/template/sampleBasic.cpp for what drives
 * onInit/onUpdate/onFrame/onDraw/onDbgfont and at what point in the loop. */

#include <stdio.h>
#include <sys/process.h>

#include "gcmutil.h"
using namespace CellGcmUtil;

#include "template/sampleBasic.h"
using namespace CellGcmUtil::SampleBasic;

#include "app.h"

namespace
{
    const float kFixedDt = 1.0f / 60.0f;
}

bool onInit(void)
{
    return App::Init();
}

void onFinish(void)
{
    App::Shutdown();
}

void onUpdate(void)
{
    if (!gSampleApp.isPause && !gSampleApp.isSysMenu)
    {
        App::Update(kFixedDt);
    }
}

void onFrame(void)
{
}

void onDraw(void)
{
    App::Draw();
}

void onDbgfont(void)
{
    cellGcmUtilSetPrintSize(0.75f);
    cellGcmUtilSetPrintPos(0.04f, 0.04f);
    cellGcmUtilSetPrintColor(0xffffffff);

    if (gSampleApp.isSysMenu)
        cellGcmUtilPrintf("Crux: In Game XMB\n");
    else if (gSampleApp.isPause)
        cellGcmUtilPrintf("Crux: PAUSE\n");
    else
        cellGcmUtilPrintf("Crux\n");

    if (!gSampleApp.isSysMenu) sampleDrawSimplePerf();
}

SYS_PROCESS_PARAM(1001, 0x10000);

int main(int argc, char *argv[])
{
    return sampleMain(argc, argv);
}
