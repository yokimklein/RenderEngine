#pragma once
#include <types.h>

class c_scene;
class c_renderer;
void imgui_overlay(c_scene* const scene, const c_renderer* const renderer, dword fps_counter);