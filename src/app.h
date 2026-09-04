#ifndef CRUX_APP_H
#define CRUX_APP_H

// Shared game glue: owns level/player/camera/score and runs one fixed-timestep
// tick + one draw per frame. Platform mains (main_ps3.cpp / main_pc.cpp) just
// call App::Init once and App::Update/Draw every frame inside whatever loop
// shape that platform's SDK requires.
namespace App
{
    bool Init();
    void Shutdown();
    void Update(float dt);
    void Draw();
}

#endif
