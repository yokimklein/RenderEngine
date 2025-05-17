#pragma once
#include <types.h>

void main_win(qword hwnd);

void main_init();
void main_loop_body();
void update_mouse(const int32 delta_x, const int32 delta_y);
void update_input(const float delta_time);

#ifdef API_DX12
class c_renderer_dx12;
extern c_renderer_dx12* g_renderer;
#else
#error RENDER API MISSING FROM CURRENT PLATFORM
#endif
class c_scene;
extern c_scene* g_scene;