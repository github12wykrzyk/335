#include "../src/PlayerESP/player_esp_camera.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    Esp335CameraAxes axes;
    Esp335Camera camera;
    Esp335Vec3 test;
    float x,y,depth;
    memset(&axes,0,sizeof(axes));
    axes.forward.y=1.f;
    axes.right.x=1.f;
    axes.up.z=1.f;
    axes.eye.x=8.f;
    axes.eye.y=12.f;
    axes.eye.z=4.f;
    axes.fov_y=1.f;
    axes.near_clip=0.1f;
    axes.far_clip=1000.f;
    axes.aspect=16.f/9.f;
    axes.viewport_width=1280.f;
    axes.viewport_height=720.f;
    assert(esp335_camera_build(&axes,&camera));
    test=(Esp335Vec3){8.f,22.f,4.f};
    assert(esp335_project(&camera,test,&x,&y,&depth));
    assert(x>639.f && x<641.f && y>359.f && y<361.f);
    test=(Esp335Vec3){9.f,22.f,4.f};
    assert(esp335_project(&camera,test,&x,&y,&depth));
    assert(x>640.f);
    test=(Esp335Vec3){8.f,22.f,5.f};
    assert(esp335_project(&camera,test,&x,&y,&depth));
    assert(y<360.f);
    test=(Esp335Vec3){8.f,0.f,4.f};
    assert(!esp335_project(&camera,test,&x,&y,&depth));
    axes.up=axes.forward;
    assert(!esp335_camera_build(&axes,&camera));
    /* Real 12340 in-game report: clip right=0.4903, clip top=0.8716,
     * but output render rectangle is [0,0,1,1]. Normalizing by the
     * clip rectangle had changed x=0.0908 to x=0.1852: WRONG. */
    {
        const float render[4]={0.f,0.f,1.f,1.f};
        const float invalid[4]={0.f,0.f,0.f,1.f};
        float u=0.f,v=0.f;
        assert(esp335_native_screen_to_ui(0.0908f,0.1965f,render,&u,&v));
        assert(u>0.0907f && u<0.0909f);
        assert(v>0.8034f && v<0.8036f);
        assert(esp335_native_screen_to_ui(0.0324f,0.1722f,render,&u,&v));
        assert(u>0.0323f && u<0.0325f);
        assert(v>0.8277f && v<0.8279f);
        assert(!esp335_native_screen_to_ui(0.5f,0.5f,invalid,&u,&v));
        assert(!esp335_native_screen_to_ui(1.1f,0.5f,render,&u,&v));
    }
    puts("PLAYER_ESP_CAMERA: PASS");
    return 0;
}
