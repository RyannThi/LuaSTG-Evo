﻿#include "Core/Graphics/Device_D3D11.hpp"
#include "Core/i18n.hpp"
#include "utility/encoding.hpp"
#include "platform/WindowsVersion.hpp"

namespace LuaSTG::Core::Graphics
{
	static std::string bytes_count_to_string(DWORDLONG size)
	{
		int count = 0;
		char buffer[64] = {};
		if (size < 1024llu) // B
		{
			count = std::snprintf(buffer, 64, "%u B", (unsigned int)size);
		}
		else if (size < (1024llu * 1024llu)) // KB
		{
			count = std::snprintf(buffer, 64, "%.2f KB", (double)size / 1024.0);
		}
		else if (size < (1024llu * 1024llu * 1024llu)) // MB
		{
			count = std::snprintf(buffer, 64, "%.2f MB", (double)size / 1048576.0);
		}
		else // GB
		{
			count = std::snprintf(buffer, 64, "%.2f GB", (double)size / 1073741824.0);
		}
		return std::string(buffer, count);
	}
	inline std::string_view adapter_flags_to_string(UINT const flags)
	{
		if ((flags & DXGI_ADAPTER_FLAG_REMOTE))
		{
			if (flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				return i18n("DXGI_adapter_type_software_remote");
			}
			else
			{
				return i18n("DXGI_adapter_type_hardware_remote");
			}
		}
		else
		{
			if (flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				return i18n("DXGI_adapter_type_software");
			}
			else
			{
				return i18n("DXGI_adapter_type_hardware");
			}
		}
	}
	inline std::string_view d3d_feature_level_to_string(D3D_FEATURE_LEVEL level)
	{
		switch (level)
		{
		case D3D_FEATURE_LEVEL_12_2: return "12.2";
		case D3D_FEATURE_LEVEL_12_1: return "12.1";
		case D3D_FEATURE_LEVEL_12_0: return "12.0";
		case D3D_FEATURE_LEVEL_11_1: return "11.1";
		case D3D_FEATURE_LEVEL_11_0: return "11.0";
		case D3D_FEATURE_LEVEL_10_1: return "10.1";
		case D3D_FEATURE_LEVEL_10_0: return "10.0";
		case D3D_FEATURE_LEVEL_9_3: return "9.3";
		case D3D_FEATURE_LEVEL_9_2: return "9.2";
		case D3D_FEATURE_LEVEL_9_1: return "9.1";
		default: return i18n("unknown");
		}
	}
	inline std::string_view hardware_composition_flags_to_string(UINT const flags)
	{
		if (flags & DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN)
		{
			if (flags & DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_WINDOWED)
			{
				if (flags & DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_CURSOR_STRETCHED)
				{
					return "全屏、窗口、鼠标指针缩放";
				}
				else
				{
					return "全屏、窗口";
				}
			}
			else
			{
				if (flags & DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_CURSOR_STRETCHED)
				{
					return "全屏、鼠标指针缩放";
				}
				else
				{
					return "全屏";
				}
			}
		}
		else
		{
			if (flags & DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_WINDOWED)
			{
				if (flags & DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_CURSOR_STRETCHED)
				{
					return "窗口、鼠标指针缩放";
				}
				else
				{
					return "窗口";
				}
			}
			else
			{
				if (flags & DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_CURSOR_STRETCHED)
				{
					return "鼠标指针缩放";
				}
				else
				{
					return "不支持";
				}
			}
		}
	};
	inline std::string_view rotation_to_string(DXGI_MODE_ROTATION const rot)
	{
		switch (rot)
		{
		default:
		case DXGI_MODE_ROTATION_UNSPECIFIED: return "未知";
		case DXGI_MODE_ROTATION_IDENTITY: return "无";
		case DXGI_MODE_ROTATION_ROTATE90: return "90 度";
		case DXGI_MODE_ROTATION_ROTATE180: return "180 度";
		case DXGI_MODE_ROTATION_ROTATE270: return "270 度";
		}
	};
	inline std::string_view threading_feature_to_string(D3D11_FEATURE_DATA_THREADING const v)
	{
		if (v.DriverConcurrentCreates)
		{
			if (v.DriverCommandLists)
			{
				return "异步资源创建、多线程命令队列";
			}
			else
			{
				return "异步资源创建";
			}
		}
		else
		{
			if (v.DriverCommandLists)
			{
				return "多线程命令队列";
			}
			else
			{
				return "不支持";
			}
		}
	};
	inline std::string_view d3d_feature_level_to_maximum_texture2d_size_string(D3D_FEATURE_LEVEL const level)
	{
		switch (level)
		{
		case D3D_FEATURE_LEVEL_12_2:
		case D3D_FEATURE_LEVEL_12_1:
		case D3D_FEATURE_LEVEL_12_0:
		case D3D_FEATURE_LEVEL_11_1:
		case D3D_FEATURE_LEVEL_11_0:
			return "16384x16384";
		case D3D_FEATURE_LEVEL_10_1:
		case D3D_FEATURE_LEVEL_10_0:
			return "8192x8192";
		case D3D_FEATURE_LEVEL_9_3:
			return "4096x4096";
		case D3D_FEATURE_LEVEL_9_2:
		case D3D_FEATURE_LEVEL_9_1:
		default:
			return "2048x2048";
		}
	}
	inline std::string_view renderer_architecture_to_string(BOOL const TileBasedDeferredRenderer)
	{
		if (TileBasedDeferredRenderer)
			return "Tile Based Deferred Renderer (TBDR)";
		else
			return "Immediate Mode Rendering (IMR)";
	}

	Device_D3D11::Device_D3D11(std::string_view const& prefered_gpu)
		: preferred_adapter_name(prefered_gpu)
	{
		HRESULT hr = S_OK;

		// 加载 DXGI 模块

		dxgi_dll = LoadLibraryW(L"dxgi.dll");
		assert(dxgi_dll);
		if (dxgi_dll == NULL)
		{
			// 不应该出现这种情况
			hr = gHRLastError;
			i18n_log_error_fmt("[core].system_dll_load_failed_f", "dxgi.dll");
			throw std::runtime_error("dxgi.dll not found");
		}
		dxgi_api_CreateDXGIFactory1 = (decltype(dxgi_api_CreateDXGIFactory1))GetProcAddress(dxgi_dll, "CreateDXGIFactory1");
		assert(dxgi_api_CreateDXGIFactory1);
		if (dxgi_api_CreateDXGIFactory1 == NULL)
		{
			i18n_log_error_fmt("[core].system_dll_load_func_failed_f", "dxgi.dll", "CreateDXGIFactory1");
		}
		dxgi_api_CreateDXGIFactory2 = (decltype(dxgi_api_CreateDXGIFactory2))GetProcAddress(dxgi_dll, "CreateDXGIFactory2");
		if (dxgi_api_CreateDXGIFactory2 == NULL)
		{
			i18n_log_error_fmt("[core].system_dll_load_func_failed_f", "dxgi.dll", "CreateDXGIFactory2");
		}

		// 加载 Direct3D 11 模块

		d3d11_dll = LoadLibraryW(L"d3d11.dll");
		assert(d3d11_dll);
		if (d3d11_dll == NULL)
		{
			// 不应该出现这种情况
			hr = gHRLastError;
			i18n_log_error_fmt("[core].system_dll_load_failed_f", "d3d11.dll");
			throw std::runtime_error("d3d11.dll not found");
		}
		d3d11_api_D3D11CreateDevice = (decltype(d3d11_api_D3D11CreateDevice))GetProcAddress(d3d11_dll, "D3D11CreateDevice");
		assert(d3d11_api_D3D11CreateDevice);
		if (d3d11_api_D3D11CreateDevice == NULL)
		{
			i18n_log_error_fmt("[core].system_dll_load_func_failed_f", "d3d11.dll", "D3D11CreateDevice");
		}

		// 创建图形组件

		i18n_log_info("[core].Device_D3D11.start_creating_graphic_components");

		if (!createDXGI())
			throw std::runtime_error("create basic DXGI components failed");
		if (!createD3D11())
			throw std::runtime_error("create basic D3D11 components failed");

		i18n_log_info("[core].Device_D3D11.created_graphic_components");
	}
	Device_D3D11::~Device_D3D11()
	{
		// 清理对象

		destroyD3D11();
		destroyDXGI();

		// 卸载 DXGI 模块

		if (dxgi_dll) FreeLibrary(dxgi_dll); dxgi_dll = NULL;
		dxgi_api_CreateDXGIFactory1 = NULL;
		dxgi_api_CreateDXGIFactory2 = NULL;

		// 卸载 Direct3D 11 模块

		if (d3d11_dll) FreeLibrary(d3d11_dll); d3d11_dll = NULL;
		d3d11_api_D3D11CreateDevice = NULL;
	}

	bool Device_D3D11::selectAdapter()
	{
		HRESULT hr = S_OK;

		// 公共参数

	#ifdef _DEBUG
		UINT const d3d11_creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;
	#else
		UINT const d3d11_creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	#endif
		D3D_FEATURE_LEVEL const target_levels[7] = {
			D3D_FEATURE_LEVEL_12_2, // Windows 7, 8, 8.1 没有这个
			D3D_FEATURE_LEVEL_12_1, // Windows 7, 8, 8.1 没有这个
			D3D_FEATURE_LEVEL_12_0, // Windows 7, 8, 8.1 没有这个
			D3D_FEATURE_LEVEL_11_1, // Windows 7 没有这个
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};

		// 枚举所有图形设备

		i18n_log_info("[core].Device_D3D11.enum_all_adapters");

		struct AdapterCandidate
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			std::string adapter_name;
			DXGI_ADAPTER_DESC1 adapter_info;
			D3D_FEATURE_LEVEL feature_level;
			BOOL link_to_output;
		};
		std::vector<AdapterCandidate> adapter_candidate;

		Microsoft::WRL::ComPtr<IDXGIAdapter1> dxgi_adapter_temp;
		for (UINT idx = 0; bHR = dxgi_factory->EnumAdapters1(idx, &dxgi_adapter_temp); idx += 1)
		{
			// 检查此设备是否支持 Direct3D 11 并获取特性级别
			bool supported_d3d11 = false;
			D3D_FEATURE_LEVEL level_info = D3D_FEATURE_LEVEL_10_0;
			for (UINT offset = 0; offset < 5; offset += 1)
			{
				hr = gHR = D3D11CreateDevice(
					dxgi_adapter_temp.Get(),
					D3D_DRIVER_TYPE_UNKNOWN,
					NULL,
					d3d11_creation_flags,
					target_levels + offset,
					(UINT)std::size(target_levels) - offset,
					D3D11_SDK_VERSION,
					NULL,
					&level_info,
					NULL);
				if (SUCCEEDED(hr))
				{
					supported_d3d11 = true;
					break;
				}
			}

			// 获取图形设备信息
			std::string dev_name = "<NULL>";
			DXGI_ADAPTER_DESC1 desc_ = {};
			if (bHR = gHR = dxgi_adapter_temp->GetDesc1(&desc_))
			{
				bool soft_dev_type = (desc_.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) || (desc_.Flags & DXGI_ADAPTER_FLAG_REMOTE);
				dev_name = std::move(utility::encoding::to_utf8(desc_.Description));
				i18n_log_info_fmt("[core].Device_D3D11.DXGI_adapter_detail_fmt"
					, idx
					, dev_name
					, d3d_feature_level_to_string(level_info)
					, adapter_flags_to_string(desc_.Flags)
					, soft_dev_type ? i18n("DXGI_adapter_type_software_warning") : ""
					, bytes_count_to_string(desc_.DedicatedVideoMemory)
					, bytes_count_to_string(desc_.DedicatedSystemMemory)
					, bytes_count_to_string(desc_.SharedSystemMemory)
					, desc_.VendorId
					, desc_.DeviceId
					, desc_.SubSysId
					, desc_.Revision
					, static_cast<DWORD>(desc_.AdapterLuid.HighPart), desc_.AdapterLuid.LowPart
				);
				if (soft_dev_type)
				{
					supported_d3d11 = false; // 排除软件或远程设备
				}
			}
			else
			{
				i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIAdapter1::GetDesc1");
				i18n_log_error_fmt("[core].Device_D3D11.DXGI_adapter_detail_error_fmt", idx);
				supported_d3d11 = false; // 排除未知错误
			}

			// 枚举显示输出
			bool has_linked_output = false;
			Microsoft::WRL::ComPtr<IDXGIOutput> dxgi_output_temp;
			for (UINT odx = 0; bHR = dxgi_adapter_temp->EnumOutputs(odx, &dxgi_output_temp); odx += 1)
			{
				Microsoft::WRL::ComPtr<IDXGIOutput6> dxgi_output_temp6;
				hr = gHR = dxgi_output_temp.As(&dxgi_output_temp6);
				if (FAILED(hr))
				{
					i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIOutput::QueryInterface -> IDXGIOutput6");
					// 不是严重错误
				}

				DXGI_OUTPUT_DESC1 o_desc = {};
				bool read_o_desc = false;
				UINT comp_sp_flags = 0;

				if (dxgi_output_temp6)
				{
					if (!(bHR = gHR = dxgi_output_temp6->CheckHardwareCompositionSupport(&comp_sp_flags)))
					{
						comp_sp_flags = 0;
						i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIOutput6::CheckHardwareCompositionSupport");
					}
					if (bHR = gHR = dxgi_output_temp6->GetDesc1(&o_desc))
					{
						read_o_desc = true;
					}
					else
					{
						i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIOutput6::GetDesc1");
					}
				}
				if (!read_o_desc)
				{
					DXGI_OUTPUT_DESC desc_0_ = {};
					if (bHR = gHR = dxgi_output_temp->GetDesc(&desc_0_))
					{
						std::memcpy(o_desc.DeviceName, desc_0_.DeviceName, sizeof(o_desc.DeviceName));
						o_desc.DesktopCoordinates = desc_0_.DesktopCoordinates;
						o_desc.AttachedToDesktop = desc_0_.AttachedToDesktop;
						o_desc.Rotation = desc_0_.Rotation;
						o_desc.Monitor = desc_0_.Monitor;
						read_o_desc = true;
					}
					else
					{
						i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIOutput::GetDesc");
					}
				}

				if (read_o_desc)
				{
					i18n_log_info_fmt("[core].Device_D3D11.DXGI_output_detail_fmt"
						, idx, odx
						, o_desc.AttachedToDesktop ? i18n("DXGI_output_connected") : i18n("DXGI_output_not_connect")
						, o_desc.DesktopCoordinates.left
						, o_desc.DesktopCoordinates.top
						, o_desc.DesktopCoordinates.right - o_desc.DesktopCoordinates.left
						, o_desc.DesktopCoordinates.bottom - o_desc.DesktopCoordinates.top
						, rotation_to_string(o_desc.Rotation)
						, hardware_composition_flags_to_string(comp_sp_flags)
					);
					has_linked_output = true;
				}
				else
				{
					i18n_log_error_fmt("[core].Device_D3D11.DXGI_output_detail_error_fmt", idx, odx);
				}
			}
			dxgi_output_temp.Reset();

			// 加入候选列表
			if (supported_d3d11)
			{
				adapter_candidate.emplace_back(AdapterCandidate{
					.adapter = dxgi_adapter_temp,
					.adapter_name = std::move(dev_name),
					.adapter_info = desc_,
					.feature_level = level_info,
					.link_to_output = has_linked_output,
					});
			}
		}
		dxgi_adapter_temp.Reset();

		// 选择图形设备

		BOOL link_to_output = false;
		for (auto& v : adapter_candidate)
		{
			if (v.adapter_name == preferred_adapter_name)
			{
				dxgi_adapter = v.adapter;
				dxgi_adapter_name = v.adapter_name;
				link_to_output = v.link_to_output;
				break;
			}
		}
		if (!dxgi_adapter && !adapter_candidate.empty())
		{
			auto& v = adapter_candidate[0];
			dxgi_adapter = v.adapter;
			dxgi_adapter_name = v.adapter_name;
			link_to_output = v.link_to_output;
		}
		dxgi_adapter_names.clear();
		for (auto& v : adapter_candidate)
		{
			dxgi_adapter_names.emplace_back(std::move(v.adapter_name));
		}
		adapter_candidate.clear();

		// 获取图形设备

		if (dxgi_adapter)
		{
			i18n_log_info_fmt("[core].Device_D3D11.select_DXGI_adapter_fmt", dxgi_adapter_name);
			if (!link_to_output)
			{
				i18n_log_warn_fmt("[core].Device_D3D11.DXGI_adapter_no_output_warning_fmt", dxgi_adapter_name);
			}
		}
		else
		{
			i18n_log_critical("[core].Device_D3D11.no_available_DXGI_adapter");
			return false;
		}

		return true;
	}
	bool Device_D3D11::createDXGI()
	{
		HRESULT hr = S_OK;

		i18n_log_info("[core].Device_D3D11.start_creating_basic_DXGI_components");

		// 创建工厂

		if (dxgi_api_CreateDXGIFactory2)
		{
			// 创建 1.2 的组件
			UINT dxgi_flags = 0;
		#ifdef _DEBUG
			dxgi_flags |= DXGI_CREATE_FACTORY_DEBUG;
		#endif
			hr = gHR = dxgi_api_CreateDXGIFactory2(dxgi_flags, IID_PPV_ARGS(&dxgi_factory2));
			if (FAILED(hr))
			{
				i18n_log_error_fmt("[core].system_call_failed_f", "CreateDXGIFactory2 -> IDXGIFactory2");
				return false;
			}
			// 获得 1.1 的组件
			hr = gHR = dxgi_factory2.As(&dxgi_factory);
			assert(SUCCEEDED(hr)); // 不应该出现这种情况
			if (FAILED(hr))
			{
				i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIFactory2::QueryInterface -> IDXGIFactory1");
				return false;
			}
		}
		else if (dxgi_api_CreateDXGIFactory1)
		{
			// 创建 1.1 的组件
			hr = gHR = dxgi_api_CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
			if (FAILED(hr))
			{
				i18n_log_error_fmt("[core].system_call_failed_f", "CreateDXGIFactory1 -> IDXGIFactory1");
				return false;
			}
			// 获得 1.2 的组件（Windows 7 平台更新）
			hr = gHR = dxgi_factory.As(&dxgi_factory2);
			if (FAILED(hr))
			{
				i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIFactory1::QueryInterface -> IDXGIFactory2");
				// 不是严重错误
			}
		}
		else
		{
			// 不应该出现这种情况
			i18n_log_error_fmt("[core].system_call_failed_f", "CreateDXGIFactory");
			return false;
		}
		
		// 检测特性支持情况

		Microsoft::WRL::ComPtr<IDXGIFactory3> dxgi_factory3;
		hr = gHR = dxgi_factory.As(&dxgi_factory3);
		if (FAILED(hr))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIFactory1::QueryInterface -> IDXGIFactory3");
			// 不是严重错误
		}

		Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory4;
		hr = gHR = dxgi_factory.As(&dxgi_factory4);
		if (FAILED(hr))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIFactory1::QueryInterface -> IDXGIFactory4");
			// 不是严重错误
		}

		Microsoft::WRL::ComPtr<IDXGIFactory5> dxgi_factory5;
		hr = gHR = dxgi_factory.As(&dxgi_factory5);
		if (FAILED(hr))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIFactory1::QueryInterface -> IDXGIFactory5");
			// 不是严重错误
		}

		Microsoft::WRL::ComPtr<IDXGIFactory6> dxgi_factory6;
		hr = gHR = dxgi_factory.As(&dxgi_factory6);
		if (FAILED(hr))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIFactory1::QueryInterface -> IDXGIFactory6");
			// 不是严重错误
		}

		Microsoft::WRL::ComPtr<IDXGIFactory7> dxgi_factory7;
		hr = gHR = dxgi_factory.As(&dxgi_factory7);
		if (FAILED(hr))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIFactory1::QueryInterface -> IDXGIFactory7");
			// 不是严重错误
		}

		if (platform::WindowsVersion::Is8())
		{
			dxgi_support_flip_model = TRUE;
		}
		if (platform::WindowsVersion::Is8Point1())
		{
			dxgi_support_low_latency = TRUE;
		}
		if (platform::WindowsVersion::Is10())
		{
			dxgi_support_flip_model2 = TRUE;
		}
		if (dxgi_factory5)
		{
			BOOL value = FALSE;
			hr = gHR = dxgi_factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &value, sizeof(value));
			if (SUCCEEDED(hr))
			{
				dxgi_support_tearing = value;
			}
			else
			{
				i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIFactory5::CheckFeatureSupport -> DXGI_FEATURE_PRESENT_ALLOW_TEARING");
				// 不是严重错误
			}
		}
		dwm_acceleration_level = 0;
		if (dxgi_support_flip_model) dwm_acceleration_level = 1;
		if (dxgi_support_low_latency) dwm_acceleration_level = 2;
		if (dxgi_support_flip_model2) dwm_acceleration_level = 3;
		if (dxgi_support_tearing) dwm_acceleration_level = 4;

		// 打印特性支持情况

		i18n_log_info_fmt("[core].Device_D3D11.DXGI_detail_fmt"
			, dwm_acceleration_level
			, dxgi_support_flip_model  ? i18n("support") : i18n("not_support.requires_Windows_8")
			, dxgi_support_flip_model2 ? i18n("support") : i18n("not_support.requires_Windows_10")
			, dxgi_support_low_latency ? i18n("support") : i18n("not_support.requires_Windows_8_point_1")
			, dxgi_support_tearing     ? i18n("support") : i18n("not_support.requires_Windows_10_and_hardware")
		);

		// 获取适配器

		if (!selectAdapter()) return false;

		// 检查适配器支持

		Microsoft::WRL::ComPtr<IDXGIAdapter2> dxgi_adapter2;
		hr = gHR = dxgi_adapter.As(&dxgi_adapter2);
		if (FAILED(hr))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIAdapter1::QueryInterface -> IDXGIAdapter2");
			// 不是严重错误
		}

		Microsoft::WRL::ComPtr<IDXGIAdapter3> dxgi_adapter3;
		hr = gHR = dxgi_adapter.As(&dxgi_adapter3);
		if (FAILED(hr))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIAdapter1::QueryInterface -> IDXGIAdapter3");
			// 不是严重错误
		}

		Microsoft::WRL::ComPtr<IDXGIAdapter2> dxgi_adapter4;
		hr = gHR = dxgi_adapter.As(&dxgi_adapter4);
		if (FAILED(hr))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "IDXGIAdapter1::QueryInterface -> IDXGIAdapter4");
			// 不是严重错误
		}

		i18n_log_info("[core].Device_D3D11.created_basic_DXGI_components");

		return true;
	}
	void Device_D3D11::destroyDXGI()
	{
		dxgi_factory.Reset();
		dxgi_factory2.Reset();
		dxgi_adapter.Reset();

		dxgi_adapter_name.clear();
		dxgi_adapter_names.clear();

		dwm_acceleration_level = 0;
		dxgi_support_flip_model = FALSE;
		dxgi_support_low_latency = FALSE;
		dxgi_support_flip_model2 = FALSE;
		dxgi_support_tearing = FALSE;
	}
	bool Device_D3D11::createD3D11()
	{
		HRESULT hr = S_OK;

		// 公共参数

	#ifdef _DEBUG
		UINT const d3d11_creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;
	#else
		UINT const d3d11_creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	#endif
		D3D_FEATURE_LEVEL const target_levels[7] = {
			D3D_FEATURE_LEVEL_12_2, // Windows 7, 8, 8.1 没有这个
			D3D_FEATURE_LEVEL_12_1, // Windows 7, 8, 8.1 没有这个
			D3D_FEATURE_LEVEL_12_0, // Windows 7, 8, 8.1 没有这个
			D3D_FEATURE_LEVEL_11_1, // Windows 7 没有这个
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};

		// 创建

		i18n_log_info("[core].Device_D3D11.start_creating_basic_D3D11_components");

		for (UINT offset = 0; offset < 5; offset += 1)
		{
			hr = gHR = D3D11CreateDevice(
				dxgi_adapter.Get(),
				D3D_DRIVER_TYPE_UNKNOWN,
				NULL,
				d3d11_creation_flags,
				target_levels + offset,
				(UINT)std::size(target_levels) - offset,
				D3D11_SDK_VERSION,
				&d3d11_device,
				&d3d_feature_level,
				&d3d11_devctx);
			if (SUCCEEDED(hr))
			{
				break;
			}
		}
		if (!d3d11_device)
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "D3D11CreateDevice");
			return false;
		}

		// 特性检查

		hr = gHR = d3d11_device.As(&d3d11_device1);
		if (FAILED(hr))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "ID3D11Device::QueryInterface -> ID3D11Device1");
			// 不是严重错误
		}
		hr = gHR = d3d11_devctx.As(&d3d11_devctx1);
		if (FAILED(hr))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "ID3D11DeviceContext::QueryInterface -> ID3D11DeviceContext1");
			// 不是严重错误
		}
		
		struct D3DX11_FEATURE_DATA_FORMAT_SUPPORT
		{
			DXGI_FORMAT InFormat;
			UINT OutFormatSupport;
			UINT OutFormatSupport2;
		};
		auto check_format_support = [&](DXGI_FORMAT const format, std::string_view const& name) ->D3DX11_FEATURE_DATA_FORMAT_SUPPORT
		{
			std::string name1("ID3D11Device::CheckFeatureSupport -> D3D11_FEATURE_FORMAT_SUPPORT ("); name1.append(name); name1.append(")");
			std::string name2("ID3D11Device::CheckFeatureSupport -> D3D11_FEATURE_FORMAT_SUPPORT2 ("); name2.append(name); name2.append(")");

			D3D11_FEATURE_DATA_FORMAT_SUPPORT data1 = { .InFormat = format };
			HRESULT const hr1 = gHR = d3d11_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT, &data1, sizeof(D3D11_FEATURE_DATA_FORMAT_SUPPORT));
			if (FAILED(hr1)) i18n_log_error_fmt("[core].system_call_failed_f", name1);

			D3D11_FEATURE_DATA_FORMAT_SUPPORT2 data2 = { .InFormat = format };
			HRESULT const hr2 = gHR = d3d11_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &data2, sizeof(D3D11_FEATURE_DATA_FORMAT_SUPPORT2));
			if (FAILED(hr2)) i18n_log_error_fmt("[core].system_call_failed_f", name2);

			return D3DX11_FEATURE_DATA_FORMAT_SUPPORT{ .InFormat = format, .OutFormatSupport = data1.OutFormatSupport, .OutFormatSupport2 = data2.OutFormatSupport2 };
		};

	#define _CHECK_FORMAT_SUPPORT(_NAME, _FORMAT) \
		D3DX11_FEATURE_DATA_FORMAT_SUPPORT d3d11_feature_format_##_NAME = check_format_support(_FORMAT, #_FORMAT);

		_CHECK_FORMAT_SUPPORT(rgba32     , DXGI_FORMAT_R8G8B8A8_UNORM     );
		_CHECK_FORMAT_SUPPORT(rgba32_srgb, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		_CHECK_FORMAT_SUPPORT(bgra32     , DXGI_FORMAT_B8G8R8A8_UNORM     );
		_CHECK_FORMAT_SUPPORT(bgra32_srgb, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
		_CHECK_FORMAT_SUPPORT(d24_s8     , DXGI_FORMAT_D24_UNORM_S8_UINT  );

	#undef _CHECK_FORMAT_SUPPORT

		D3D11_FEATURE_DATA_THREADING d3d11_feature_mt = {};
		HRESULT hr_mt = d3d11_device->CheckFeatureSupport(D3D11_FEATURE_THREADING, &d3d11_feature_mt, sizeof(d3d11_feature_mt));
		if (FAILED(hr_mt))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "ID3D11Device::CheckFeatureSupport -> D3D11_FEATURE_THREADING");
			// 不是严重错误
		}

		D3D11_FEATURE_DATA_ARCHITECTURE_INFO d3d11_feature_arch = {};
		HRESULT hr_arch = d3d11_device->CheckFeatureSupport(D3D11_FEATURE_ARCHITECTURE_INFO, &d3d11_feature_arch, sizeof(d3d11_feature_arch));
		if (FAILED(hr_arch))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "ID3D11Device::CheckFeatureSupport -> D3D11_FEATURE_ARCHITECTURE_INFO");
			// 不是严重错误
		}

		D3D11_FEATURE_DATA_D3D11_OPTIONS2 d3d11_feature_o2 = {};
		HRESULT hr_o2 = d3d11_device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &d3d11_feature_o2, sizeof(d3d11_feature_o2));
		if (FAILED(hr_o2))
		{
			i18n_log_error_fmt("[core].system_call_failed_f", "ID3D11Device::CheckFeatureSupport -> D3D11_FEATURE_D3D11_OPTIONS2");
			// 不是严重错误
		}

	#define _FORMAT_INFO_STRING_FMT3 \
		"        用于顶点缓冲区：{}\n"\
		"        创建二维纹理：{}\n"\
		"        创建立方体纹理：{}\n"\
		"        着色器采样：{}\n"\
		"        创建多级渐进纹理：{}\n"\
		"        自动生成多级渐进纹理：{}\n"\
		"        绑定为渲染目标：{}\n"\
		"        像素颜色混合操作：{}\n"\
		"        绑定为深度、模板缓冲区：{}\n"\
		"        被 CPU 锁定、读取：{}\n"\
		"        解析多重采样：{}\n"\
		"        用于显示输出：{}\n"\
		"        创建多重采样渲染目标：{}\n"\
		"        像素颜色逻辑混合操作：{}\n"\
		"        资源可分块：{}\n"\
		"        资源可共享：{}\n"\
		"        多平面叠加：{}\n"

	#define _FORMAT_MAKE_SUPPORT i18n("support") : i18n("not_support")

	#define _FORMAT_INFO_STRING_ARG3(_NAME) \
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER        ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D               ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_TEXTURECUBE             ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE           ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_MIP                     ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN             ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_RENDER_TARGET           ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_BLENDABLE               ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL           ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_CPU_LOCKABLE            ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE     ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_DISPLAY                 ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_TILED                 ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_SHAREABLE             ) ? _FORMAT_MAKE_SUPPORT\
		, (d3d11_feature_format_##_NAME.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_MULTIPLANE_OVERLAY    ) ? _FORMAT_MAKE_SUPPORT

		spdlog::info("[fancy2d] Direct3D 11 设备功能支持：\n"
			"    Direct3D 功能级别：{}\n"
			"    R8G8B8A8 格式：\n"
			_FORMAT_INFO_STRING_FMT3
			"    R8G8B8A8 sRGB 格式：\n"
			_FORMAT_INFO_STRING_FMT3
			"    B8G8R8A8 格式：\n"
			_FORMAT_INFO_STRING_FMT3
			"    B8G8R8A8 sRGB 格式：\n"
			_FORMAT_INFO_STRING_FMT3
			"    D24 S8 格式：\n"
			_FORMAT_INFO_STRING_FMT3
			"    最大二维纹理尺寸：{}\n"
			"    多线程架构：{}\n"
			"    渲染架构：{}\n"
			"    统一内存架构（UMA）：{}"
			, d3d_feature_level_to_string(d3d_feature_level)
			_FORMAT_INFO_STRING_ARG3(rgba32)
			_FORMAT_INFO_STRING_ARG3(rgba32_srgb)
			_FORMAT_INFO_STRING_ARG3(bgra32)
			_FORMAT_INFO_STRING_ARG3(bgra32_srgb)
			_FORMAT_INFO_STRING_ARG3(d24_s8)
			, d3d_feature_level_to_maximum_texture2d_size_string(d3d_feature_level)
			, threading_feature_to_string(d3d11_feature_mt)
			, renderer_architecture_to_string(d3d11_feature_arch.TileBasedDeferredRenderer)
			, d3d11_feature_o2.UnifiedMemoryArchitecture ? _FORMAT_MAKE_SUPPORT
		);

	#undef _FORMAT_INFO_STRING_ARG3
	#undef _FORMAT_MAKE_SUPPORT
	#undef _FORMAT_INFO_STRING_FMT3

		if (
			(d3d11_feature_format_bgra32.OutFormatSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D)
			&& (d3d11_feature_format_bgra32.OutFormatSupport & D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER)
			&& (d3d11_feature_format_bgra32.OutFormatSupport & D3D11_FORMAT_SUPPORT_MIP)
			&& (d3d11_feature_format_bgra32.OutFormatSupport & D3D11_FORMAT_SUPPORT_RENDER_TARGET)
			&& (d3d11_feature_format_bgra32.OutFormatSupport & D3D11_FORMAT_SUPPORT_BLENDABLE)
			&& (d3d11_feature_format_bgra32.OutFormatSupport & D3D11_FORMAT_SUPPORT_DISPLAY)
			)
		{
			// 确实支持
		}
		else
		{
			spdlog::warn("[fancy2d] 此设备没有完整的 B8G8R8A8 格式支持，程序可能无法正常运行");
		}

		i18n_log_info("[core].Device_D3D11.created_basic_D3D11_components");

		return true;
	}
	void Device_D3D11::destroyD3D11()
	{
		d3d_feature_level = D3D_FEATURE_LEVEL_10_0;

		d3d11_device.Reset();
		d3d11_device1.Reset();
		d3d11_devctx.Reset();
		d3d11_devctx1.Reset();
	}

	bool IDevice::create(StringView prefered_gpu, IDevice** p_device)
	{
		try
		{
			*p_device = new Device_D3D11(prefered_gpu);
			return true;
		}
		catch (...)
		{
			*p_device = nullptr;
			return false;
		}
	}
}
