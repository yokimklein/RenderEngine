#include "game.h"
#include <render/camera.h>
#include <reporting/report.h>
#include <scene/scene.h>
#include <time/time.h>
#include <render/texture.h>

#if API_DIRECTX
#include <render/api/directx12/renderer.h>
#include <DirectXMath.h>
using namespace DirectX;
#endif

/* ASSIGNMENT TODO LIST
NAVIGABLE FRAMEWORK (70-84)
- (Extra) Object picking for gizmos - save gbuffer of objects indices? How would I access this on the CPU?
- (Extra) Camera animation using splines?
- "Evidence of an advanced navigation technique developed, such as animation of camera using splines, for example."

NORMAL MAPPING (70-84)
- Evidence of further research into the fidelity or technique as demonstrated in the code (backed up in the forums).

POST PROCESSING (70-84)
- Evidence of further research into the fidelity or technique as demonstrated in the code (backed up in the forums).

DEFERRED SHADING AND LIGHTING (70-84)
- Evidence of further research into the fidelity or technique as demonstrated in the code (backed up in the forums).

PROGRESS AND CRITICAL REFLECTION
- Forum documentation, clearly researched, critical reflection, further reading (Citations), documenting progress
*/

// WOULD BE NICE TODOs:
// - Multiple cameras
// - Smart pointers for assets and references to fix crashes on shutdown
// - Limit camera up/down to prevent clipping
// - reinit swapchain on window resize

// TODO: make this singleton?
#ifdef API_DX12
static c_renderer_dx12* g_renderer;
#else
#error RENDER API MISSING FROM CURRENT PLATFORM
#endif
static c_scene* g_scene;

#ifdef PLATFORM_WINDOWS
void main_win(qword hwnd)
{
	//reporting_init();
    init_render_globals(hwnd);

    g_renderer = new c_renderer_dx12();
    g_scene = new c_scene();

	// Init D3D & return on fail
	if (!g_renderer->initialise((HWND)hwnd, g_scene))
	{
		LOG_ERROR(L"renderer init failed! aborting");
		delete g_renderer;
		return;
	}

	main_init();

    // $TODO: need to be able to access scene object assets after initialisation to create AS, but they need the renderer initialised to upload assets to the GPU first
    g_renderer->create_acceleration_structures(g_scene);
    g_renderer->create_shader_binding_table(g_scene, false);

	MSG msg = {};
	bool running = true;
	while (running)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		main_loop_body();
	}

	// Wait for GPU to finish executing the command list before we close
	g_renderer->wait_for_previous_frame(); // TODO: this does not seem to be waiting properly, we're getting crashes for GPU objects in use on scene destruction
    delete g_scene;                        // Likely due to asset sharing between objects not being cleaned up properly
	delete g_renderer;
}
#endif

void main_init()
{
    // sun spot light
    g_scene->m_lights[0].m_enabled = static_cast<dword>(true);
    g_scene->m_lights[0].m_light_type = _light_spot;
    g_scene->m_lights[0].m_spot_angle = 0.785f;
    g_scene->m_lights[0].m_position = vector4d{ 9.088f, 28.454f, 0.0f, 1.0f };
    g_scene->m_lights[0].m_direction = vector4d{ -0.563f, -0.826f, 0.043f, 1.0f };
    g_scene->m_lights[0].m_colour = colour_rgba{ 1.0f, 0.667f, 0.189f, 1.0f };
    g_scene->m_lights[0].m_constant_attenuation = 1.0f;
    g_scene->m_lights[0].m_linear_attenuation = 0.0f;
    g_scene->m_lights[0].m_quadratic_attenuation = 0.028f;

    g_scene->m_lights[1].m_enabled = static_cast<dword>(true);
    g_scene->m_lights[1].m_light_type = _light_point;
    g_scene->m_lights[1].m_position = vector4d{ 10.157f, 3.049f, -4.737f, 1.0f };
    g_scene->m_lights[1].m_colour = colour_rgba{ 1.0f, 0.867f, 0.444f, 1.0f };
    g_scene->m_lights[1].m_constant_attenuation = 1.0f;
    g_scene->m_lights[1].m_linear_attenuation = 0.0f;
    g_scene->m_lights[1].m_quadratic_attenuation = 0.641f;

    g_scene->m_lights[2] = g_scene->m_lights[1];
    g_scene->m_lights[2].m_position = vector4d{ -9.822f, 3.047f, -4.741f, 1.0f };

    g_scene->m_lights[3] = g_scene->m_lights[1];
    g_scene->m_lights[3].m_position = vector4d{ 15.616f, 2.072f, 0.933f, 1.0f };

    g_scene->m_lights[4] = g_scene->m_lights[1];
    g_scene->m_lights[4].m_position = vector4d{ -13.596f, 2.693f, -0.061f, 1.0f };

    g_scene->m_lights[5] = g_scene->m_lights[1];
    g_scene->m_lights[5].m_position.v3.z = 4.801f;
    g_scene->m_lights[5].m_position.v3.x = 10.03f;

    g_scene->m_lights[6] = g_scene->m_lights[2];
    g_scene->m_lights[6].m_position.v3.z = 4.798f;

    g_scene->m_ambient_light = colour_rgba{ 0.056f, 0.056f, 0.056f, 1.0f };

    // TODO: Move these to object initialisation! Load data from external files
    // need to get asset manager working again to handle shared resources
	//c_mesh* cube_model = new c_mesh(g_renderer, cube_vertices, sizeof(cube_vertices), cube_indices, sizeof(cube_indices));
	c_mesh* cube_model = new c_mesh(g_renderer, L"assets\\models\\cube.vbo_i32");

    c_material* cube_material = new c_material(g_renderer, 2);
    cube_material->m_properties.m_albedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    //cube_material->m_properties.m_ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
    cube_material->m_properties.m_use_albedo_texture = true;
    cube_material->m_properties.m_use_normal_texture = true;
    cube_material->m_properties.m_render_texture = false;

    // https://opengameart.org/content/free-materials-pack-34-redux
    c_render_texture* cube_diffuse_texture = new c_render_texture(g_renderer, L"assets\\textures\\crate\\Crate_COLOR.dds", _texture_albedo);
    c_render_texture* cube_normal_texture = new c_render_texture(g_renderer, L"assets\\textures\\crate\\Crate_NRM.dds", _texture_normal);

    cube_material->assign_texture(cube_diffuse_texture);
    //cube_material->assign_texture(cube_specular_texture);
    cube_material->assign_texture(cube_normal_texture);

    // scene objects - will be cleaned up by scene destruction
    c_scene_object* cube_object = new c_scene_object("crate", cube_model, cube_material);
    cube_object->add_update_function
    (
        [cube_object]
        {
            XMVECTOR vec = XMQuaternionRotationRollPitchYaw(0.0f, 0.01f, 0.0f);
            quaternion quat;
            XMStoreFloat4((XMFLOAT4*)&quat, vec);
            cube_object->m_transform.set_rotation(cube_object->m_transform.get_rotation() * quat);
        }
    );
    //g_scene->add_object(cube_object);

    constexpr const char* sponza_mesh_material_names[] =
    {
        "arch_stone_wall_01",
        "brickwall_01",
        "brickwall_02",
        "ceiling_plaster_01",
        "ceiling_plaster_02",
        "column_1stfloor",
        "column_brickwall_01",
        "column_head_1stfloor",
        "column_head_2ndfloor_02",
        "column_head_2ndfloor_03",
        "door_stoneframe_01",
        "door_stoneframe_02",
        "floor_01",
        "metal_door_laatste",
        "ornament_01",
        "ornament_lion",
        "roof_tiles_01",
        "stones_01_tile",
        "stones_2ndfloor",
        "stone_trims_01",
        "stone_trims_02",
        "window_frame_01",
        "wood_01",
        "wood_door_01",

        "curtain_01_metal_door",
        "curtain_01",
        "curtain_02",
        "curtain_03",
        
        //"dirt_decal",
        //"glass",
        //"lamp_glass_01",
        //"light_bulb",
    };
    constexpr dword sponza_mesh_material_count = _countof(sponza_mesh_material_names);
    constexpr const char* sponza_material_texture_names[][4] =
    {
        { "arch_stone_wall_01_BaseColor",   "arch_stone_wall_01_Normal",    "arch_stone_wall_01_Roughness",     "arch_stone_wall_01_Metalness"	 	},
        { "brickwall_01_BaseColor",         "brickwall_01_Normal",          "brickwall_01_Roughness",     		"brickwall_01_Metalness" 	 	    },
        { "brickwall_02_BaseColor",         "brickwall_02_Normal",          "brickwall_02_Roughness",     		"brickwall_02_Metalness" 			},
        { "ceiling_plaster_01_BaseColor",   "ceiling_plaster_01_Normal",    "ceiling_plaster_01_Roughness",     "ceiling_plaster_01_Metalness" 		},
        { "ceiling_plaster_02_BaseColor",   "ceiling_plaster_02_Normal",    "ceiling_plaster_02_Roughness",     "ceiling_plaster_02_Metalness" 		},
        { "col_1stfloor_BaseColor",         "col_1stfloor_Normal",          "col_1stfloor_Roughness",     		"col_1stfloor_Metalness" 			},
        { "col_brickwall_01_BaseColor",     "col_brickwall_01_Normal",      "col_brickwall_01_Roughness",     	"col_brickwall_01_Metalness" 		},
        { "col_head_1stfloor_BaseColor",    "col_head_1stfloor_Normal",     "col_head_1stfloor_Roughness",     	"col_head_1stfloor_Metalness" 		},
        { "col_head_2ndfloor_02_BaseColor", "col_head_2ndfloor_02_Normal",  "col_head_2ndfloor_02_Roughness",	"col_head_2ndfloor_02_Metalness"	},
        { "col_head_2ndfloor_03_BaseColor", "col_head_2ndfloor_03_Normal",  "col_head_2ndfloor_03_Roughness",	"col_head_2ndfloor_03_Metalness"	},
        { "door_stoneframe_01_BaseColor",   "door_stoneframe_01_Normal",    "door_stoneframe_01_Roughness",     "door_stoneframe_01_Metalness" 		},
        { "door_stoneframe_02_BaseColor",   "door_stoneframe_02_Normal",    "door_stoneframe_02_Roughness",     "door_stoneframe_02_Metalness" 		},
        { "floor_tiles_01_BaseColor",       "floor_tiles_01_Normal",        "floor_tiles_01_Roughness",     	"floor_tiles_01_Metalness" 			},
        { "metal_door_01_BaseColor",        "metal_door_01_Normal",         "metal_door_01_Roughness",     		"metal_door_01_Metalness" 			},
        { "ornament_01_BaseColor",          "ornament_01_Normal",           "ornament_01_Roughness",     		"ornament_01_Metalness" 			},
        { "lionhead_01_BaseColor",          "lionhead_01_Normal",           "lionhead_01_Roughness",     		"lionhead_01_Metalness" 			},
        { "roof_tiles_01_BaseColor",        "roof_tiles_01_Normal",         "roof_tiles_01_Roughness",     		"roof_tiles_01_Metalness" 			},
        { "stone_01_tile_BaseColor",        "stone_01_tile_Normal",         "stone_01_tile_Roughness",     		"stone_01_tile_Metalness" 			},
        { "stones_2ndfloor_01_BaseColor",   "stones_2ndfloor_01_Normal",    "stones_2ndfloor_01_Roughness",     "stones_2ndfloor_01_Metalness" 		},
        { "stone_trims_01_BaseColor",       "stone_trims_01_Normal",        "stone_trims_01_Roughness",     	"stone_trims_01_Metalness" 			},
        { "stone_trims_02_BaseColor",       "stone_trims_02_Normal",        "stone_trims_02_Roughness",     	"stone_trims_02_Metalness" 			},
        { "window_frame_01_BaseColor",      "window_frame_01_Normal",       "window_frame_01_Roughness",     	"window_frame_01_Metalness" 		},
        { "wood_tile_01_BaseColor",         "wood_tile_01_Normal",          "wood_tile_01_Roughness",     		"wood_tile_01_Metalness" 			},
        { "wood_door_01_BaseColor",         "wood_door_01_Normal",          "wood_door_01_Roughness",     		"wood_door_01_Metalness" 			},

        { "metal_door_01_BaseColor",        "metal_door_01_Normal",         "metal_door_01_Roughness",     		"metal_door_01_Metalness" 			},
        { "curtain_fabric_red_BaseColor",   "curtain_fabric_Normal",        "curtain_fabric_Roughness",     	"curtain_fabric_Metalness" 			},
        { "curtain_fabric_green_BaseColor", "curtain_fabric_Normal",        "curtain_fabric_Roughness",     	"curtain_fabric_Metalness" 			},
        { "curtain_fabric_blue_BaseColor",  "curtain_fabric_Normal",        "curtain_fabric_Roughness",     	"curtain_fabric_Metalness" 			},
        
        //{ "dirt_decal_01_alpha", "", "", "" },
        //{ "", "", "", "" }, // glass
        //{ "", "", "", "" }, // lamp glass
        //{ "", "", "", "" }, // light bulb
    };

    c_mesh* sponza_meshes[sponza_mesh_material_count];
    c_material* sponza_materials[sponza_mesh_material_count];
    for (dword i = 0; i < sponza_mesh_material_count; i++)
    {
        wchar_t mesh_path[MAX_PATH] = {};
        swprintf_s(mesh_path, MAX_PATH, L"assets\\models\\sponza_pbr\\%hs.vbo_i32", sponza_mesh_material_names[i]);
        sponza_meshes[i] = new c_mesh(g_renderer, mesh_path);

        dword texture_count = 0;
        c_render_texture* albedo_texture = nullptr;
        c_render_texture* normal_texture = nullptr;
        c_render_texture* roughness_texture = nullptr;
        c_render_texture* metallic_texture = nullptr;

        if (sponza_material_texture_names[i][0] != "")
        {
            wchar_t diff_texture_path[MAX_PATH] = {};
            swprintf_s(diff_texture_path, MAX_PATH, L"assets\\textures\\sponza_pbr\\%hs.dds", sponza_material_texture_names[i][0]);
            albedo_texture = new c_render_texture(g_renderer, diff_texture_path, _texture_albedo);
            texture_count++;
        }
        if (sponza_material_texture_names[i][2] != "")
        {
            wchar_t roughness_texture_path[MAX_PATH] = {};
            swprintf_s(roughness_texture_path, MAX_PATH, L"assets\\textures\\sponza_pbr\\%hs.dds", sponza_material_texture_names[i][2]);
            roughness_texture = new c_render_texture(g_renderer, roughness_texture_path, _texture_roughness);
            texture_count++;
        }
        if (sponza_material_texture_names[i][3] != "")
        {
            wchar_t metallic_texture_path[MAX_PATH] = {};
            swprintf_s(metallic_texture_path, MAX_PATH, L"assets\\textures\\sponza_pbr\\%hs.dds", sponza_material_texture_names[i][3]);
            metallic_texture = new c_render_texture(g_renderer, metallic_texture_path, _texture_metallic);
            texture_count++;
        }
        if (sponza_material_texture_names[i][1] != "")
        {
            wchar_t norm_texture_path[MAX_PATH] = {};
            swprintf_s(norm_texture_path, MAX_PATH, L"assets\\textures\\sponza_pbr\\%hs.dds", sponza_material_texture_names[i][1]);
            normal_texture = new c_render_texture(g_renderer, norm_texture_path, _texture_normal);
            texture_count++;
        }

        sponza_materials[i] = new c_material(g_renderer, texture_count);

        if (albedo_texture != nullptr)
        {
            sponza_materials[i]->assign_texture(albedo_texture);
            sponza_materials[i]->m_properties.m_use_albedo_texture = true;
        }
        if (roughness_texture != nullptr)
        {
            sponza_materials[i]->assign_texture(roughness_texture);
            sponza_materials[i]->m_properties.m_use_roughness_texture = true;
        }
        if (metallic_texture != nullptr)
        {
            sponza_materials[i]->assign_texture(metallic_texture);
            sponza_materials[i]->m_properties.m_use_metallic_texture = true;
        }
        if (normal_texture != nullptr)
        {
            sponza_materials[i]->assign_texture(normal_texture);
            sponza_materials[i]->m_properties.m_use_normal_texture = true;
        }
    }

    for (dword i = 0; i < sponza_mesh_material_count; i++)
    {
        c_scene_object* scene_object = new c_scene_object(sponza_mesh_material_names[i], sponza_meshes[i], sponza_materials[i], { 0.0f, -1.1f, 0.0f }, {}, { 1.0f, 1.0f, 1.0f });
        g_scene->add_object(scene_object);
    }
}

static bool g_camera_transitions_enabled = true;
static float camera_transition_timer = 0.0f;
constexpr float camera_sit_time = 10.0f;
static dword camera_position_index = 0;
constexpr dword camera_position_count = 6;
void main_loop_body()
{
	static c_time_manager time_manager = c_time_manager();
	const float delta_time = time_manager.delta_time();
    if (delta_time == 0.0) { return; }

    if (GetAsyncKeyState(VK_TAB) & 0x0001)
    {
        g_camera_transitions_enabled = !g_camera_transitions_enabled;
    }

    if (g_camera_transitions_enabled)
    {
        camera_transition_timer += delta_time;
        if (camera_transition_timer >= (camera_sit_time * (camera_position_index + 1)))
        {
            camera_position_index++;
            if (camera_position_index == camera_position_count)
            {
                camera_position_index = 0;
                camera_transition_timer = 0.0f;
            }
        }
        switch (camera_position_index)
        {
        case 0:
            g_scene->m_camera->set_position({ 7.689f, -0.157f, -1.912f });
            g_scene->m_camera->set_direction({ -0.846f, 0.307f, 0.436f });
            break;
        case 1:
            g_scene->m_camera->set_position({ 9.704f, 0.975f, -0.449f });
            g_scene->m_camera->set_direction({ 0.875f, -0.173f, 0.453f });
            break;
        case 2:
            g_scene->m_camera->set_position({ -10.707f, 0.404f, -3.495f });
            g_scene->m_camera->set_direction({ 0.697f, -0.015f, -0.717f });
            break;
        case 3:
            g_scene->m_camera->set_position({ -4.701f, 1.808f, 1.529f });
            g_scene->m_camera->set_direction({ -0.892f, 0.230f, -0.390f });
            break;
        case 4:
            g_scene->m_camera->set_position({ 8.784f, 6.301f, -2.019f });
            g_scene->m_camera->set_direction({ -0.887f, -0.394f, 0.242f });
            break;
        case 5:
            g_scene->m_camera->set_position({ 7.839f, 5.555f, 5.362f });
            g_scene->m_camera->set_direction({ -0.69f, 0.108f, -0.716f });
            break;
        default:
            break;
        }
    }
    // Uncomment to fake poor performance
    //Sleep(25);

	// input
	update_input(delta_time);

	// scene
	g_scene->update(delta_time);

	// render
    // TODO: can I better decouple the renderer and scene classes?
    g_scene->setup_for_render(g_renderer);
	g_renderer->render_frame(g_scene, time_manager.get_frames_per_second()); // execute the command queue (rendering the scene is the result of the gpu executing the command lists)
    time_manager.increment_frame_count();
}

void update_mouse(const int32 delta_x, const int32 delta_y)
{
    if (!g_camera_transitions_enabled)
    {
	    g_scene->m_camera->update_look(delta_x, delta_y);
    }
	//LOG_MESSAGE(L"%d, %d", delta_x, delta_y);
}

void update_input(const float delta_time)
{
#ifdef PLATFORM_WINDOWS
	const float camera_move_amount = 4.0f * delta_time;

    // Only accept input if window has focus
    if (GetActiveWindow() != NULL && !g_camera_transitions_enabled)
    {
        if (GetAsyncKeyState('W') & 0x8000)
            g_scene->m_camera->move_forward(camera_move_amount);
        if (GetAsyncKeyState('A') & 0x8000)
            g_scene->m_camera->strafe_left(camera_move_amount);
        if (GetAsyncKeyState('S') & 0x8000)
            g_scene->m_camera->move_backward(camera_move_amount);
        if (GetAsyncKeyState('D') & 0x8000)
            g_scene->m_camera->strafe_right(camera_move_amount);
        if (GetAsyncKeyState('F') & 0x8000)
        	g_scene->m_camera->move_down(camera_move_amount);
        if (GetAsyncKeyState('R') & 0x8000)
        	g_scene->m_camera->move_up(camera_move_amount);
    }
#endif
}
