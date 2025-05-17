#include "render.h"
#include <render/camera.h>
#include <reporting/report.h>

c_renderer::c_renderer()
	: m_object_cb()
	, m_material_properties_cb()
	, m_light_properties_cb()
{
}

const char* const get_light_name(const e_light_type light_type)
{
	constexpr const char* k_light_names[] =
	{
		"Point Light",
		"Spot Light",
		"Directional Light"
	};
	static_assert(_countof(k_light_names) == k_light_type_count);
	
	if (!IN_RANGE_COUNT(light_type, _light_point, k_light_type_count))
	{
		LOG_WARNING(L"tried to get name for invalid target type [%d]!", light_type);
		return "Invalid Light Type";
	}
	
	return k_light_names[light_type];
}
