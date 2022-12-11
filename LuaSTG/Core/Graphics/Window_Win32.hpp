﻿#pragma once
#include "Core/Object.hpp"
#include "Core/Graphics/Window.hpp"
#include "platform/Monitor.hpp"
#include "platform/WindowSizeMoveController.hpp"

namespace Core::Graphics
{
	class Window_Win32 : public Object<IWindow>
	{
	private:
		WCHAR win32_window_class_name[64]{};
		WNDCLASSEXW win32_window_class{ sizeof(WNDCLASSEXW) };
		ATOM win32_window_class_atom{ 0 };
		static LRESULT CALLBACK win32_window_callback(HWND window, UINT message, WPARAM arg1, LPARAM arg2);

		HWND win32_window{ NULL };

		HIMC win32_window_imc{ NULL };
		BOOL win32_window_ime_enable{ FALSE };

		UINT win32_window_width{ 640 };
		UINT win32_window_height{ 480 };
		UINT win32_window_dpi{ USER_DEFAULT_SCREEN_DPI };

		INT_PTR win32_window_icon_id{ 0 };

		std::string win32_window_text{ "Window" };
		std::array<wchar_t, 512> win32_window_text_w;

		WindowCursor m_cursor{ WindowCursor::Arrow };
		HCURSOR win32_window_cursor{ NULL };

		WindowFrameStyle m_framestyle{ WindowFrameStyle::Fixed };
		DWORD win32_window_style{ WS_OVERLAPPEDWINDOW ^ (WS_THICKFRAME | WS_MAXIMIZEBOX) };
		DWORD win32_window_style_ex{ 0 };
		BOOL m_hidewindow{ TRUE };
		BOOL m_redirect_bitmap{ TRUE };
		WINDOWPLACEMENT m_last_window_placement{};
		BOOL m_alt_down{ FALSE };
		BOOL m_fullscreen_mode{ FALSE };

		BOOL win32_window_is_sizemove{ FALSE };
		BOOL win32_window_want_track_focus{ FALSE };

		platform::WindowSizeMoveController m_sizemove;
		platform::MonitorList m_monitors;

		LRESULT onMessage(HWND window, UINT message, WPARAM arg1, LPARAM arg2);

		bool createWindowClass();
		void destroyWindowClass();
		bool createWindow();
		void destroyWindow();

	public:
		// 内部方法

		HWND GetWindow() { return win32_window; }

		void convertTitleText();

		RectI getRect();
		bool setRect(RectI v);
		RectI getClientRect();
		bool setClientRect(RectI v);
		uint32_t getDPI();
		void setRedirectBitmapEnable(bool enable);
		bool getRedirectBitmapEnable();
		bool recreateWindow();
		
	private:
		enum class EventType
		{
			WindowCreate,
			WindowDestroy,

			WindowActive,
			WindowInactive,

			WindowClose,

			WindowSize,
			WindowSizeMovePaint,
			WindowDpiChanged,

			NativeWindowMessage,

			DeviceChange,
		};
		union EventData
		{
			Vector2I window_size;
		};
		bool m_is_dispatch_event{ false };
		std::vector<IWindowEventListener*> m_eventobj;
		std::vector<IWindowEventListener*> m_eventobj_late;
		void dispatchEvent(EventType t, EventData d = {});

	public:
		void addEventListener(IWindowEventListener* e);
		void removeEventListener(IWindowEventListener* e);

		void* getNativeHandle();
		void setNativeIcon(void* id);

		void setIMEState(bool enable);
		bool getIMEState();

		void setTitleText(StringView str);
		StringView getTitleText();

		bool setFrameStyle(WindowFrameStyle style);
		WindowFrameStyle getFrameStyle();

		Vector2I getSize();
		bool setSize(Vector2I v);

		WindowLayer getLayer();
		bool setLayer(WindowLayer layer);

		float getDPIScaling();

		Vector2I getMonitorSize();
		void setCentered();
		void setFullScreen();

		uint32_t getMonitorCount();
		RectI getMonitorRect(uint32_t index);
		void setMonitorCentered(uint32_t index);
		void setMonitorFullScreen(uint32_t index);

		void setCustomSizeMoveEnable(bool v);
		void setCustomMinimizeButtonRect(RectI v);
		void setCustomCloseButtonRect(RectI v);
		void setCustomMoveButtonRect(RectI v);

		bool setCursor(WindowCursor type);
		WindowCursor getCursor();
		void setCursorToRightBottom();

	public:
		Window_Win32();
		~Window_Win32();

	public:
		static bool create(Window_Win32** pp_window);
		static bool create(Vector2I size, StringView title_text, WindowFrameStyle style, bool show, Window_Win32** pp_window);
	};
}
