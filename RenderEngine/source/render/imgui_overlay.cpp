#include "imgui_overlay.h"
#include <imgui.h>
#include <scene/scene.h>
#include <reporting/report.h>
#include <render/render.h>
#include <ImGuizmo.h>
#include <time/time.h>

void imgui_overlay(c_scene* const scene, const c_renderer* const renderer, dword fps_counter)
{
    //ImGui::ShowDemoWindow();

    bool open = true;
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoNav;
    // Use work area to avoid menu-bar/task-bar
    ImVec2 debug_overlay_pos = { RENDER_GLOBALS.window_pos.x + 10.0f, RENDER_GLOBALS.window_pos.y + 10.0f };
    ImGui::SetNextWindowPos(debug_overlay_pos, ImGuiCond_Always, { 0.0f, 0.0f });
    ImGui::SetNextWindowSize({ 500.0f, 900.0f });
    ImGui::SetNextWindowBgAlpha(0.35f);

    dword selected_object_index = 0;
    dword selected_light_index = 0;
    bool lights_open = false;
    if (ImGui::Begin("Debug Overlay", &open, window_flags))
    {
        char debug_overlay_text[32];
        sprintf_s(debug_overlay_text, "DEBUG OVERLAY (%d/%dFPS)\n", fps_counter, TICK_RATE);
        ImGui::SeparatorText(debug_overlay_text);

        // m_raster
        ImGui::Checkbox("Raster", (bool*)&renderer->m_raster); // TODO: make renderer not a const, const casting is horrid

        if (ImGui::BeginTabBar("Debug Overlay Tab Bar"))
        {
            if (renderer->m_raster)
            {
                if (ImGui::BeginTabItem("G-Buffers"))
                {
                    for (dword i = 0; i < k_gbuffer_count + k_light_buffer_count + 1; i++) // + depth
                    {
                        ImGui::SeparatorText(get_gbuffer_name((e_gbuffers)i));
                        const float aspect_ratio = static_cast<float>(RENDER_GLOBALS.render_bounds.height) / static_cast<float>(RENDER_GLOBALS.render_bounds.width);
                        ImVec2 imvec = ImVec2(256.0f, 256.0f * aspect_ratio);
                        ImGui::Image((ImTextureID)renderer->get_gbuffer_textureid((e_gbuffers)i), imvec); // width * aspect ratio corrected height
                    }
                    ImGui::EndTabItem();
                }
            }
            if (ImGui::BeginTabItem("Scene Objects"))
            {
                dword object_index = 0;
                for (c_scene_object* const object : *scene->get_objects())
                {
                    ImGui::PushID(object_index);
                    if (ImGui::TreeNode("", "(%d) %hs", object_index, object->m_name))
                    {
                        selected_object_index = object_index;
                        ImGui::SeparatorText("MATERIAL\n");
                        ImGui::Checkbox("Use Albedo Texture", (bool*)&object->get_material()->m_properties.m_use_albedo_texture);
                        ImGui::Checkbox("Use Roughness Texture", (bool*)&object->get_material()->m_properties.m_use_roughness_texture);
                        ImGui::Checkbox("Use Metallic Texture", (bool*)&object->get_material()->m_properties.m_use_metallic_texture);
                        ImGui::Checkbox("Use Normal Texture", (bool*)&object->get_material()->m_properties.m_use_normal_texture);
                        ImGui::Checkbox("Render Target As Texture", (bool*)&object->get_material()->m_properties.m_render_texture);
                        ImGui::SliderFloat3("Albedo Material", object->get_material()->m_properties.m_albedo.values, 0.0f, 1.0f);
                        ImGui::SliderFloat("Roughness Material", &object->get_material()->m_properties.m_roughness, 0.0f, 1.0f);
                        ImGui::SliderFloat("Metallic Material", &object->get_material()->m_properties.m_metallic, 0.0f, 1.0f);

                        ImGui::SeparatorText("TRANSFORM\n");
                        point3d position = object->m_transform.get_position();
                        point3d rotation = object->m_transform.get_rotation_euler();
                        point3d scale = object->m_transform.get_scale();
                        if (ImGui::SliderFloat3("Position", position.values, -10.0f, 10.0f))
                        {
                            object->m_transform.set_position(position);
                        }
                        if (ImGui::SliderFloat3("Rotation", rotation.values, DEGREES_RANGE.min, DEGREES_RANGE.max))
                        {
                            // TODO: rotation conversions freak out when converting between eulers
                            //object->m_transform.set_rotation_euler(rotation);
                        }
                        if (ImGui::SliderFloat3("Scale", scale.values, 0.0f, 1.0f))
                        {
                            object->m_transform.set_scale(scale);
                        }
                        //point3d rotation_new = object->m_transform.get_rotation_euler();
                        //LOG_MESSAGE(L"SETTING: %f %f %f\nRETRIEVED: %f %f %f", rotation.x, rotation.y, rotation.z, rotation_new.x, rotation_new.y, rotation_new.z);

                        ImGui::TreePop();
                    }
                    object_index++;
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Scene Lights"))
            {
                lights_open = true;
                ImGui::SliderFloat3("Ambient Light", scene->m_ambient_light.values, 0.0f, 1.0f);
                for (dword i = 0; i < MAXIMUM_SCENE_LIGHTS; i++)
                {
                    s_light* light = &scene->m_lights[i];
                    ImGui::PushID(i);
                    if (ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_DefaultOpen, "Light %d", i))
                    {
                        ImGui::Checkbox("Enabled", (bool*)&light->m_enabled);
                        if (light->m_enabled)
                        {
                            selected_light_index = i;

                            if (ImGui::BeginCombo("Light Type", get_light_name((e_light_type)light->m_light_type)))
                            {
                                for (dword light_type = 0; light_type < k_light_type_count; light_type++)
                                {
                                    const bool is_selected = (light->m_light_type == light_type);
                                    if (ImGui::Selectable(get_light_name((e_light_type)light_type), is_selected))
                                    {
                                        light->m_light_type = light_type;
                                    }

                                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                                    if (is_selected)
                                    {
                                        ImGui::SetItemDefaultFocus();
                                    }
                                }
                                ImGui::EndCombo();
                            }

                            if (light->m_light_type != _light_direcitonal)
                            {
                                ImGui::SliderFloat3("Position", light->m_position.values, -10.0f, 10.0f);
                            }
                            if (light->m_light_type == _light_spot || light->m_light_type == _light_direcitonal)
                            {
                                ImGui::SliderFloat3("Direction", light->m_direction.values, -1.0f, 1.0f);
                                if (light->m_light_type == _light_spot)
                                {
                                    ImGui::SliderFloat("Spot Angle (Radians)", &light->m_spot_angle, 0.0f, MAX_RADIANS);
                                }
                            }
                            ImGui::SliderFloat3("Colour", light->m_colour.values, 0.0f, 1.0f);
                            ImGui::SliderFloat("Constant Attenuation", &light->m_constant_attenuation, 0.0f, 1.0f);
                            ImGui::SliderFloat("Linear Attenuation", &light->m_linear_attenuation, 0.0f, 1.0f);
                            ImGui::SliderFloat("Quadratic Attenuation", &light->m_quadratic_attenuation, 0.0f, 1.0f);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Post Processing Parameters"))
            {
                ImGui::SeparatorText("BLUR PARAMETERS\n");
                ImGui::Checkbox("Enable Blur", (bool*)&scene->m_post_parameters.enable_blur);
                ImGui::SliderFloat("Blur Strength", &scene->m_post_parameters.blur_strength, 0.0f, 10.0f);
                ImGui::SliderFloat("Blur Sharpness", &scene->m_post_parameters.blur_sharpness, 0.5f, 10.0f);
                ImGui::SliderFloat("Blur X Coverage", &scene->m_post_parameters.blur_x_coverage, 1.0f, 0.0f);
                ImGui::Checkbox("Enable Depth of Field", (bool*)&scene->m_post_parameters.enable_depth_of_field);
                ImGui::SliderFloat("Depth of Field Scale", &scene->m_post_parameters.depth_of_field_scale, 0.0f, 1.0f);

                ImGui::SeparatorText("SCREEN TINT PARAMETERS\n");
                ImGui::Checkbox("Enable Greyscale", (bool*)&scene->m_post_parameters.enable_greyscale);

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        // TODO: Cameras in scene

        // TODO: console output
    }

    ImGui::End();

    // Gizmos
    static ImGuizmo::OPERATION gizmo_operation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_T))
        gizmo_operation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_E))
        gizmo_operation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_Q) && !lights_open) // no scale for lights
        gizmo_operation = ImGuizmo::SCALE;
    ImGuiWindowFlags gizmo_window_flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs;

    //RECT client_rect;
    //GetClientRect((HWND)RENDER_GLOBALS.hwnd, &client_rect);
    //dword client_width = client_rect.right - client_rect.left;
    //dword client_height = client_rect.bottom - client_rect.top;

    ImGui::SetNextWindowPos(ImVec2(RENDER_GLOBALS.window_pos.x, RENDER_GLOBALS.window_pos.y), ImGuiCond_Always, { 0.0f, 0.0f });
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(RENDER_GLOBALS.render_bounds.width), static_cast<float>(RENDER_GLOBALS.render_bounds.height)));
    ImGui::Begin("Gizmo", 0, gizmo_window_flags);
    ImGuizmo::SetDrawlist();
    matrix4x4 view = scene->m_camera->get_view();
    matrix4x4 projection = scene->m_camera->get_projection();
    ImGuizmo::SetRect(RENDER_GLOBALS.window_pos.x, RENDER_GLOBALS.window_pos.y, static_cast<float>(RENDER_GLOBALS.render_bounds.width), static_cast<float>(RENDER_GLOBALS.render_bounds.height));
    if (!lights_open)
    {
        std::vector<c_scene_object*> objects = *scene->get_objects();
        if (objects.size() > 0)
        {
            c_scene_object* selected_object = objects[selected_object_index];
            if (selected_object != nullptr)
            {
                matrix4x4 matrix = selected_object->m_transform.build_matrix();
                ImGuizmo::Manipulate((float*)&view, (float*)&projection, gizmo_operation, ImGuizmo::WORLD, (float*)&matrix, NULL, NULL);
                XMVECTOR scale_vec, rot_vec, pos_vec;
                XMMatrixDecompose(&scale_vec, &rot_vec, &pos_vec, XMLoadFloat4x4((XMFLOAT4X4*)&matrix));
                point3d position, scale;
                quaternion rotation;
                XMStoreFloat3((XMFLOAT3*)&position, pos_vec);
                XMStoreFloat4((XMFLOAT4*)&rotation, rot_vec);
                XMStoreFloat3((XMFLOAT3*)&scale, scale_vec);
                selected_object->m_transform.set_position(position);
                selected_object->m_transform.set_rotation(rotation);
                selected_object->m_transform.set_scale(scale);
            }
        }
    }
    else
    {
        // No scale for lights
        if (gizmo_operation == ImGuizmo::SCALE)
        {
            gizmo_operation = ImGuizmo::TRANSLATE;
        }
        s_light* light = &scene->m_lights[selected_light_index];

        c_transform light_transform(light->m_position.v3);
        matrix4x4 matrix = light_transform.build_matrix();
        ImGuizmo::Manipulate((float*)&view, (float*)&projection, gizmo_operation, ImGuizmo::WORLD, (float*)&matrix, NULL, NULL);
        XMVECTOR scale_vec, rot_vec, pos_vec;
        XMMatrixDecompose(&scale_vec, &rot_vec, &pos_vec, XMLoadFloat4x4((XMFLOAT4X4*)&matrix));
        point3d position;
        XMStoreFloat3((XMFLOAT3*)&position, pos_vec);
        quaternion quat;
        XMStoreFloat4((XMFLOAT4*)&quat, rot_vec);

        vector3d direction = quat.rotate_vector(light->m_direction.v3);

        //LOG_MESSAGE(L"direction: %f %f %f", direction.i, direction.j, direction.k);

        scene->m_lights[selected_light_index].m_position.v3 = position;
        scene->m_lights[selected_light_index].m_direction.v3 = direction;
    }
    ImGui::End();
}
