#pragma once
#include <types.h>


struct s_shader_resources
{
	// TODO: forward declare these
	void* vertex_shader;  // ID3DBlob*
	void* pixel_shader;   // ID3DBlob*
	void* pipeline_state; // ID3D12PipelineState*
};

class c_renderer;
enum e_shader_input;
class c_shader
{
public:
	c_shader(c_renderer* const renderer, const wchar_t* vs_path, const char* vs_name, const wchar_t* ps_path, const char* ps_name, const e_shader_input input_type);
	~c_shader();

	const s_shader_resources* const get_resources() const { return &m_resources; };

private:
	s_shader_resources m_resources;
};