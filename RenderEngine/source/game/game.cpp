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
	if (!g_renderer->initialise((HWND)hwnd))
	{
		LOG_ERROR(L"renderer init failed! aborting");
		delete g_renderer;
		return;
	}

	main_init();

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
    /*
	// load cube model
    vertex cube_vertices[] =
    {
        // top
        { { { -1.0f,  1.0f,  1.0f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f } } }, // 3 // 0
        { { {  1.0f,  1.0f, -1.0f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f } } }, // 1 // 1
        { { { -1.0f,  1.0f, -1.0f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f } } }, // 0 // 2
                                                      
        { { {  1.0f,  1.0f,  1.0f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f } } }, // 2 // 3
        { { {  1.0f,  1.0f, -1.0f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f } } }, // 1 // 4
        { { { -1.0f,  1.0f,  1.0f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f } } }, // 3 // 5
                                                      
        // bottom                                     
        { { {  1.0f, -1.0f,  1.0f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f } } }, // 6 // 6
        { { { -1.0f, -1.0f, -1.0f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f } } }, // 4 // 7
        { { {  1.0f, -1.0f, -1.0f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f } } }, // 5 // 8
                                                      
        { { { -1.0f, -1.0f,  1.0f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f } } }, // 7 // 9
        { { { -1.0f, -1.0f, -1.0f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f } } }, // 4 // 10 
        { { {  1.0f, -1.0f,  1.0f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f } } }, // 6 // 11
                                                      
        // left                                       
        { { { -1.0f,  1.0f,  1.0f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } } }, // 11 // 12
        { { { -1.0f, -1.0f, -1.0f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } } }, // 9 // 13
        { { { -1.0f, -1.0f,  1.0f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } } }, // 8 // 14
                                                      
        { { { -1.0f,  1.0f, -1.0f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } } }, // 10 // 15
        { { { -1.0f, -1.0f, -1.0f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } } }, // 9 // 16
        { { { -1.0f,  1.0f,  1.0f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } } }, // 11 // 17
                                                      
        // right                                      
        { { {  1.0f,  1.0f, -1.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } } }, // 14 // 18
        { { {  1.0f, -1.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } } }, // 12 // 19
        { { {  1.0f, -1.0f, -1.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } } }, // 13 // 20
                                                      
        { { {  1.0f,  1.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } } }, // 15 // 21
        { { {  1.0f, -1.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } } }, // 12 // 22
        { { {  1.0f,  1.0f, -1.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } } }, // 14 // 23

        // front
        { { { -1.0f,  1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f } } }, // 19 // 24
        { { {  1.0f, -1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f} , { 1.0f, 1.0f } } }, // 17 // 25
        { { { -1.0f, -1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f } } }, // 16 // 26 
                                               
        { { {  1.0f,  1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f } } }, // 18 // 27
        { { {  1.0f, -1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f} , { 1.0f, 1.0f } } }, // 17 // 28
        { { { -1.0f,  1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f } } }, // 19 // 29
                                               
        // back                                
        { { {  1.0f,  1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f } } }, // 22
        { { { -1.0f, -1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f } } }, // 20s
        { { {  1.0f, -1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f } } }, // 21
                                                      
        { { { -1.0f,  1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f } } }, // 23
        { { { -1.0f, -1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f } } }, // 20
        { { {  1.0f,  1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f } } }, // 22
    };
    dword cube_indices[] =
    {
        0,1,2,
        3,4,5,

        6,7,8,
        9,10,11,

        12,13,14,
        15,16,17,

        18,19,20,
        21,22,23,

        24,25,26,
        27,28,29,

        30,31,32,
        33,34,35
    };
    */
    // scene lights
    g_scene->m_lights[0].m_enabled = static_cast<dword>(true);
    //point3d look_direction = g_camera->get_look_direction();
    //XMVECTOR light_direction = XMVectorSet(look_direction.x, look_direction.y, look_direction.z, 1.0f);
    //light_direction = XMVector3Normalize(light_direction);
    //XMStoreFloat4((XMFLOAT4*)&g_scene->m_lights[0].m_direction, light_direction);
    g_scene->m_lights[0].m_light_type = _light_point;
    g_scene->m_lights[0].m_spot_angle = XMConvertToRadians(45.0f);
    g_scene->m_lights[0].m_constant_attenuation = 1.0f;
    g_scene->m_lights[0].m_linear_attenuation = 0.0f;
    g_scene->m_lights[0].m_quadratic_attenuation = 0.235f;

    g_scene->m_lights[1] = g_scene->m_lights[0];
    g_scene->m_lights[2] = g_scene->m_lights[0];
    g_scene->m_lights[3] = g_scene->m_lights[0];

    g_scene->m_lights[0].m_position = vector4d{ 6.5f, 0.0f, -4.0f, 1.0f };
    g_scene->m_lights[0].m_colour = colour_rgba{ 1.0f, 1.0f, 1.0f, 1.0f };
    g_scene->m_lights[1].m_position = vector4d{ -6.5f, 0.0f, -4.0f, 1.0f };
    g_scene->m_lights[1].m_colour = colour_rgba{ 1.0f, 0.0f, 0.0f, 1.0f };
    g_scene->m_lights[2].m_position = vector4d{ 6.5f, 0.0f, 4.0f, 1.0f };
    g_scene->m_lights[2].m_colour = colour_rgba{ 0.0f, 1.0f, 0.0f, 1.0f };
    g_scene->m_lights[3].m_position = vector4d{ -6.5f, 0.0f, 4.0f, 1.0f };
    g_scene->m_lights[3].m_colour = colour_rgba{ 0.0f, 0.0f, 1.0f, 1.0f };

    g_scene->m_ambient_light = colour_rgba{ 0.1f, 0.1f, 0.1f, 1.0f };

    // TODO: Move these to object initialisation! Load data from external files
    // need to get asset manager working again to handle shared resources
	//c_mesh* cube_model = new c_mesh(g_renderer, cube_vertices, sizeof(cube_vertices), cube_indices, sizeof(cube_indices));
	c_mesh* cube_model = new c_mesh(g_renderer, L"assets\\models\\cube.vbo");

    c_material* cube_material = new c_material(g_renderer, 3);
    cube_material->m_properties.m_diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    cube_material->m_properties.m_specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    cube_material->m_properties.m_specular_power = 128.0f;
    //cube_material->m_properties.m_ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
    cube_material->m_properties.m_use_diffuse_texture = true;
    cube_material->m_properties.m_use_specular_texture = true;
    cube_material->m_properties.m_use_normal_texture = true;
    cube_material->m_properties.m_render_texture = true;

    // https://opengameart.org/content/free-materials-pack-34-redux
    c_render_texture* cube_diffuse_texture = new c_render_texture(g_renderer, L"assets\\textures\\crate\\Crate_COLOR.dds", _texture_diffuse);
    c_render_texture* cube_specular_texture = new c_render_texture(g_renderer, L"assets\\textures\\crate\\Crate_SPEC.dds", _texture_specular);
    c_render_texture* cube_normal_texture = new c_render_texture(g_renderer, L"assets\\textures\\crate\\Crate_NRM.dds", _texture_normal);

    cube_material->assign_texture(cube_diffuse_texture);
    cube_material->assign_texture(cube_specular_texture);
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
    g_scene->add_object(cube_object);

    // 25 diff, 19 norm
    constexpr const char* sponza_mesh_material_names[25] =
    {
        "arch",
        "background",
        "bricks",
        "ceiling",
        "chain",
        "column_a",
        "column_b",
        "column_c",
        "details",
        "fabric_a",
        "fabric_c",
        "fabric_d",
        "fabric_e",
        "fabric_f",
        "fabric_g",
        "flagpole",
        "floor",
        "leaf",
        "lion",
        "plaque",
        "roof",
        "vase", // textures arent applying correctly
        "vase_hanging",
        "vase_plant",
        "vase_round"
    };
    constexpr const char* sponza_material_texture_names[25][2] =
    {
        { "sponza_arch_diff",          "sponza_arch_ddn"      },
        { "background",                "background_ddn"       },
        { "spnza_bricks_a_diff",       "spnza_bricks_a_ddn"   },
        { "sponza_ceiling_a_diff",     "sponza_ceiling_a_ddn" },
        { "chain_texture",             "chain_texture_ddn"    },
        { "sponza_column_a_diff",      "sponza_column_a_ddn"  },
        { "sponza_column_b_diff",      "sponza_column_b_ddn"  },
        { "sponza_column_c_diff",      "sponza_column_c_ddn"  },
        { "sponza_details_diff",       "sponza_details_ddn"   },
        { "sponza_fabric_diff",        "sponza_fabric_ddn"    }, // shares normal w/ fabric
        { "sponza_curtain_diff",       "sponza_curtain_ddn"   }, // shares normal w/ curtain
        { "sponza_fabric_blue_diff",   "sponza_fabric_ddn"    }, // shares normal w/ fabric
        { "sponza_fabric_green_diff",  "sponza_fabric_ddn"    }, // shares normal w/ fabric
        { "sponza_curtain_green_diff", "sponza_curtain_ddn"   }, // shares normal w/ curtain
        { "sponza_curtain_blue_diff",  "sponza_curtain_ddn"   }, // shares normal w/ curtain
        { "sponza_flagpole_diff",      "sponza_flagpole_ddn"  },
        { "sponza_floor_a_diff",       "sponza_floor_a_ddn"   },
        { "sponza_thorn_diff",         "sponza_thorn_ddn"     },
        { "lion",                      "lion_ddn"             },
        { "",                          ""                     }, // no diffuse, no normal
        { "sponza_roof_diff",          "sponza_roof_ddn"      },
        { "vase_dif",                  "vase_ddn"             },
        { "vase_hanging",              "vase_hanging_ddn"     },
        { "vase_plant",                ""                     }, // no normal
        { "vase_round",                "vase_round_ddn"       },
    };
    constexpr dword sponza_mesh_material_count = _countof(sponza_mesh_material_names);

    c_mesh* sponza_meshes[sponza_mesh_material_count];
    c_material* sponza_materials[sponza_mesh_material_count];
    for (dword i = 0; i < sponza_mesh_material_count; i++)
    {
        wchar_t mesh_path[MAX_PATH] = {};
        swprintf_s(mesh_path, MAX_PATH, L"assets\\models\\sponza\\%hs.vbo", sponza_mesh_material_names[i]);
        sponza_meshes[i] = new c_mesh(g_renderer, mesh_path);

        dword texture_count = 0;
        c_render_texture* diffuse_texture = nullptr;
        c_render_texture* normal_texture = nullptr;

        if (sponza_material_texture_names[i][0] != "")
        {
            wchar_t diff_texture_path[MAX_PATH] = {};
            swprintf_s(diff_texture_path, MAX_PATH, L"assets\\textures\\sponza\\%hs.dds", sponza_material_texture_names[i][0]);
            diffuse_texture = new c_render_texture(g_renderer, diff_texture_path, _texture_diffuse);
            texture_count++;
        }
        if (sponza_material_texture_names[i][1] != "")
        {
            wchar_t norm_texture_path[MAX_PATH] = {};
            swprintf_s(norm_texture_path, MAX_PATH, L"assets\\textures\\sponza\\%hs.dds", sponza_material_texture_names[i][1]);
            normal_texture = new c_render_texture(g_renderer, norm_texture_path, _texture_normal);
            texture_count++;
        }

        sponza_materials[i] = new c_material(g_renderer, texture_count);
        sponza_materials[i]->m_properties.m_specular_power = 128.0f;
        sponza_materials[i]->m_properties.m_specular = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);

        if (diffuse_texture != nullptr)
        {
            sponza_materials[i]->assign_texture(diffuse_texture);
            sponza_materials[i]->m_properties.m_use_diffuse_texture = true;
        }
        if (normal_texture != nullptr)
        {
            sponza_materials[i]->assign_texture(normal_texture);
            sponza_materials[i]->m_properties.m_use_normal_texture = true;
        }
    }

    // plaque
    sponza_materials[19]->m_properties.m_render_texture = true;

    // bricks
    sponza_materials[2]->m_properties.m_specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    sponza_materials[2]->m_properties.m_specular_power = 40.0f;

    // fabric_a (red curved cloth)
    sponza_materials[9]->m_properties.m_ambient = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
    // fabric_c (red flat cloth)
    sponza_materials[10]->m_properties.m_ambient = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
    // fabric_d (blue curved cloth)
    sponza_materials[11]->m_properties.m_ambient = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
    // fabric_g (blue flat cloth)
    sponza_materials[14]->m_properties.m_ambient = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
    // fabric_e (green curved cloth)
    sponza_materials[12]->m_properties.m_ambient = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
    // fabric_f (green flat cloth)
    sponza_materials[13]->m_properties.m_ambient = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

    // lion background
    sponza_materials[1]->m_properties.m_diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
    // lion head
    sponza_materials[18]->m_properties.m_diffuse = XMFLOAT4(1.0f, 1.0f, 0.5f, 1.0f);

    // chain
    sponza_materials[4]->m_properties.m_emissive = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);

    // vase hanging
    sponza_materials[22]->m_properties.m_emissive = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);

    for (dword i = 0; i < sponza_mesh_material_count; i++)
    {
        c_scene_object* scene_object = new c_scene_object(sponza_mesh_material_names[i], sponza_meshes[i], sponza_materials[i], { 0.0f, -1.1f, 0.0f }, {}, { 0.01f, 0.01f, 0.01f });
        g_scene->add_object(scene_object);
    }
}

void main_loop_body()
{
	static c_time_manager time_manager = c_time_manager();
	const float delta_time = time_manager.delta_time();
    if (delta_time == 0.0) { return; }

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
	g_scene->m_camera->update_look(delta_x, delta_y);
	//LOG_MESSAGE(L"%d, %d", delta_x, delta_y);
}

void update_input(const float delta_time)
{
#ifdef PLATFORM_WINDOWS
	const float camera_move_amount = 4.0f * delta_time;

    // Only accept input if window has focus
    if (GetActiveWindow() != NULL)
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
