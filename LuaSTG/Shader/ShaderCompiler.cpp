﻿#include <cassert>
#include <string_view>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <array>
#include <span>
#include <format>
#include <type_traits>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winrt/base.h>
#include <d3dcompiler.h>

static const std::string_view source_root(HREADER_INPUT_DIRECTORY);
static const std::string_view generated_root(HREADER_OUTPUT_DIRECTORY);

namespace std
{
	inline wstring to_wstring(string_view str)
	{
		wstring wstr;
		int const count = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
		assert(count >= 0);
		wstr.resize(static_cast<size_t>(count));
		[[maybe_unused]]
		int const result = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstr.data(), count);
		assert(count == result);
		return wstr;
	}

	namespace files
	{
		template<typename T>
			requires same_as<typename T::value_type, char>
		bool read_file(string_view path, T& storage)
		{
			ifstream file(to_wstring(path), ios::in | ios::binary);
			if (!file.is_open())
			{
				assert(false);
				return false;
			}
			file.seekg(0, ios::end);
			streampos const file_end = file.tellg();
			file.seekg(0, ios::beg);
			streampos const file_begin = file.tellg();
			assert(file_end >= file_begin);
			streamoff const file_size = file_end - file_begin;

			array<typename T::value_type, 64> buffer{};
			size_t size_to_read = static_cast<size_t>(file_size);
			while (size_to_read > 0)
			{
				if (size_to_read > buffer.size())
				{
					file.read(static_cast<char*>(buffer.data()), 64);
					for (size_t i = 0; i < 64; i += 1)
					{
						storage.push_back(buffer[i]);
					}
					size_to_read -= 64;
				}
				else
				{
					file.read(static_cast<char*>(buffer.data()), static_cast<streamsize>(size_to_read));
					for (size_t i = 0; i < size_to_read; i += 1)
					{
						storage.push_back(buffer[i]);
					}
					size_to_read -= size_to_read;
				}
			}

			return true;
		}
	}
}

namespace std
{
	template<typename OutputStream>
		requires derived_from<OutputStream, basic_ostream<char, char_traits<char>>>
	class basic_ostream_back_insert_iterator
	{
	public:
		using iterator_category = output_iterator_tag;
		using value_type = void;
		using pointer = void;
		using reference = void;

		using container_type = OutputStream;
		using difference_type = ptrdiff_t;

		constexpr explicit basic_ostream_back_insert_iterator(OutputStream& output_stream) noexcept
			: container(addressof(output_stream)) {}

		constexpr basic_ostream_back_insert_iterator& operator=(char value)
		{
			container->put(value);
			return *this;
		}

		[[nodiscard]] constexpr basic_ostream_back_insert_iterator& operator*() noexcept
		{
			return *this;
		}

		constexpr basic_ostream_back_insert_iterator& operator++() noexcept
		{
			return *this;
		}

		constexpr basic_ostream_back_insert_iterator operator++(int) noexcept
		{
			return *this;
		}

	protected:
		OutputStream* container;
	};

	template<typename OutputStream>
		requires derived_from<OutputStream, basic_ostream<char, char_traits<char>>>
	[[nodiscard]] constexpr basic_ostream_back_insert_iterator<OutputStream> back_inserter(OutputStream& output_stream) noexcept
	{
		return basic_ostream_back_insert_iterator<OutputStream>(output_stream);
	}
}

struct output_config_t
{
	std::string_view namespace_name;
	std::string_view value_name;
	std::string file_path;
};

bool write_blob_to_file(winrt::com_ptr<ID3DBlob> blob, output_config_t& config, std::ofstream& file)
{
	assert(blob);
	assert(blob->GetBufferSize() % 4 == 0);

	file << "// This is a file generated by the compiler, DO NOT directly modify this file\n";
	file << "#pragma once\n";
	file << "namespace " << config.namespace_name << "\n";
	file << "{\n";
	file << "    static unsigned char const " << config.value_name << "[] = {\n";
	
	std::span<uint8_t> const bytes(static_cast<uint8_t*>(blob->GetBufferPointer()), blob->GetBufferSize());

	for (size_t index = 0; index < bytes.size(); index += 4)
	{
		std::span<uint8_t> const view = bytes.subspan(index, 4);
		std::format_to(std::back_inserter(file),
			"        {:#04x}, {:#04x}, {:#04x}, {:#04x},\n",
			view[0], view[1], view[2], view[3]);
	}

	file << "    };\n";
	file << "}\n";

	return true;
}

bool write_blob_to_file(winrt::com_ptr<ID3DBlob> blob, output_config_t& config)
{
	std::filesystem::path path(std::to_wstring(std::string(generated_root) + "/" + config.file_path));
	std::filesystem::create_directories(path.parent_path());
	std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!file.is_open())
	{
		assert(false);
		return false;
	}
	return write_blob_to_file(blob, config, file);
}

enum class optimization_level : uint8_t
{
	unspecified = 255u,
	level0 = 0u,
	level1 = 1u,
	level2 = 2u,
	level3 = 3u,
};

enum class shader_type : uint8_t
{
	invalid,
	vertex_shader,
	pixel_shader,
};

struct compile_profile_t
{
	// Directs the compiler to insert debug file/line/type/symbol information into the output code.
	uint8_t debug : 1;

	// Directs the compiler not to validate the generated code against known capabilities and constraints. We recommend that you use this constant only with shaders that have been successfully compiled in the past. DirectX always validates shaders before it sets them to a device.
	uint8_t skip_validation : 1;

	// Directs the compiler to skip optimization steps during code generation. We recommend that you set this constant for debug purposes only.
	uint8_t skip_optimization : 1;

	// Directs the compiler to pack matrices in row-major order on input and output from the shader.
	uint8_t pack_matrix_row_major : 1;

	// Directs the compiler to pack matrices in column-major order on input and output from the shader. This type of packing is generally more efficient because a series of dot-products can then perform vector-matrix multiplication.
	uint8_t pack_matrix_column_major : 1;

	// Directs the compiler to perform all computations with partial precision. If you set this constant, the compiled code might run faster on some hardware.
	uint8_t partial_precision : 1;

	// Directs the compiler to not use flow-control constructs where possible.
	uint8_t avoid_control_flow : 1;

	// Forces strict compile, which might not allow for legacy syntax. By default, the compiler disables strictness on deprecated syntax.
	uint8_t strictness : 1;

	// Forces the IEEE strict compile which avoids optimizations that may break IEEE rules.
	uint8_t ieee_strictness : 1;

	// Directs the compiler to treat all warnings as errors when it compiles the shader code. We recommend that you use this constant for new shader code, so that you can resolve all warnings and lower the number of hard-to-find code defects.
	uint8_t warnings_are_errors : 1;

	// Directs the compiler to use different optimization level.
	optimization_level optimization_level;

	inline uint32_t to_flags()
	{
		uint32_t flags = 0;
		if (debug) flags |= D3DCOMPILE_DEBUG;
		if (skip_validation) flags |= D3DCOMPILE_SKIP_VALIDATION;
		if (skip_optimization) flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		if (pack_matrix_row_major) flags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
		if (pack_matrix_column_major) flags |= D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
		if (partial_precision) flags |= D3DCOMPILE_PARTIAL_PRECISION;
		if (avoid_control_flow) flags |= D3DCOMPILE_AVOID_FLOW_CONTROL;
		if (strictness) flags |= D3DCOMPILE_ENABLE_STRICTNESS;
		if (ieee_strictness) flags |= D3DCOMPILE_IEEE_STRICTNESS;
		if (warnings_are_errors) flags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
		switch (optimization_level)
		{
		case optimization_level::level0: flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0; break;
		case optimization_level::level1: flags |= D3DCOMPILE_OPTIMIZATION_LEVEL1; break;
		case optimization_level::level2: flags |= D3DCOMPILE_OPTIMIZATION_LEVEL2; break;
		case optimization_level::level3: flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3; break;
		default: break;
		}
		return flags;
	}

	static inline compile_profile_t standard_debug()
	{
		return compile_profile_t{
			.debug = true,
			.skip_validation = false,
			.skip_optimization = true,
			.pack_matrix_row_major = false,
			.pack_matrix_column_major = false, // 我们也禁用该选项，保留默认行为
			.partial_precision = false,
			.avoid_control_flow = false,
			.strictness = true, // 避免使用 Direct3D9 时期的语法
			.ieee_strictness = false, // 不需要严格 IEEE 浮点，确保调试模式和发布模式下表现一致
			.warnings_are_errors = true, // 不润许一切警告
			.optimization_level = optimization_level::unspecified,
		};
	}

	static inline compile_profile_t standard_release()
	{
		return compile_profile_t{
			.debug = false,
			.skip_validation = false,
			.skip_optimization = false,
			.pack_matrix_row_major = false,
			.pack_matrix_column_major = false, // 我们也禁用该选项，保留默认行为
			.partial_precision = false,
			.avoid_control_flow = true,
			.strictness = true, // 避免使用 Direct3D9 时期的语法
			.ieee_strictness = false, // 不需要严格 IEEE 浮点，确保调试模式和发布模式下表现一致
			.warnings_are_errors = true, // 不润许一切警告
			.optimization_level = optimization_level::level3,
		};
	}
};

struct compile_config_t
{
	compile_profile_t compile_profile;
	shader_type shader_type;
	std::string file_path;
};

bool compile_shader(compile_config_t& config, winrt::com_ptr<ID3DBlob>& blob)
{
	std::vector<char> source;
	if (!std::files::read_file(std::string(source_root) + "/" + config.file_path, source))
	{
		return false;
	}
	winrt::com_ptr<ID3DBlob> error_message;
	std::string_view target_type = "";
	switch (config.shader_type)
	{
	case shader_type::vertex_shader: target_type = "vs_4_0"; break;
	case shader_type::pixel_shader: target_type = "ps_4_0"; break;
	default: assert(false); return false;
	}
	winrt::hresult hr = D3DCompile(
		source.data(),
		source.size(),
		config.file_path.data(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		target_type.data(),
		config.compile_profile.to_flags(),
		0,
		blob.put(),
		error_message.put());
	if (error_message && error_message->GetBufferSize() > 0)
	{
		std::cout << std::string_view(static_cast<char*>(error_message->GetBufferPointer()), error_message->GetBufferSize()) << std::endl;
	}
	if (FAILED(hr))
	{
		return false;
	}
	return true;
}

bool process_shader(compile_config_t& compile_config, output_config_t& output_config)
{
	winrt::com_ptr<ID3DBlob> blob;
	if (!compile_shader(compile_config, blob))
	{
		return false;
	}
	return write_blob_to_file(blob, output_config);
}

int main(int, char**)
{
	// imgui::backend::d3d11

	/* vertex shader */ {
		compile_config_t iconfig = {
			.compile_profile = compile_profile_t::standard_release(),
			.shader_type = shader_type::vertex_shader,
			.file_path = "imgui/backend/d3d11/vertex_shader.hlsl",
		};
		output_config_t oconfig = {
			.namespace_name = "imgui::backend::d3d11",
			.value_name = "vertex_shader",
			.file_path = "imgui/backend/d3d11/vertex_shader.hpp",
		};
		if (!process_shader(iconfig, oconfig))
		{
			return 1;
		}
	}
	/* pixel shader */ {
		compile_config_t iconfig = {
			.compile_profile = compile_profile_t::standard_release(),
			.shader_type = shader_type::pixel_shader,
			.file_path = "imgui/backend/d3d11/pixel_shader.hlsl",
		};
		output_config_t oconfig = {
			.namespace_name = "imgui::backend::d3d11",
			.value_name = "pixel_shader",
			.file_path = "imgui/backend/d3d11/pixel_shader.hpp",
		};
		if (!process_shader(iconfig, oconfig))
		{
			return 1;
		}
	}

	return 0;
}
