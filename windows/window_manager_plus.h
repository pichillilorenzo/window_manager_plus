#ifndef WINDOW_MANAGER_PLUS_PLUGIN_WINDOW_MANAGER_PLUS_H_
#define WINDOW_MANAGER_PLUS_PLUGIN_WINDOW_MANAGER_PLUS_H_

#include <shobjidl_core.h>

#include "include/window_manager_plus/window_manager_plus_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <dwmapi.h>
#include <codecvt>
#include <map>
#include <memory>
#include <sstream>

#define STATE_NORMAL 0
#define STATE_MAXIMIZED 1
#define STATE_MINIMIZED 2
#define STATE_FULLSCREEN_ENTERED 3
#define STATE_DOCKED 4

namespace window_manager_plus {

	class WindowManagerPlus {
	public:
		WindowManagerPlus();

		virtual ~WindowManagerPlus();

		inline static int64_t autoincrementId_ = 0;
		inline static std::map<int64_t, std::shared_ptr<WindowManagerPlus>> windowManagers_ = {};

		std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>, std::default_delete<flutter::MethodChannel<flutter::EncodableValue>>> static_channel = nullptr;
		std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>, std::default_delete<flutter::MethodChannel<flutter::EncodableValue>>> channel = nullptr;

		int64_t id = -1;
		HWND native_window;
		int last_state = STATE_NORMAL;
		bool has_shadow_ = false;
		bool is_always_on_bottom_ = false;
		bool is_frameless_ = false;
		bool is_prevent_close_ = false;
		double aspect_ratio_ = 0;
		POINT minimum_size_ = { 0, 0 };
		POINT maximum_size_ = { -1, -1 };
		double pixel_ratio_ = 1;
		bool is_resizable_ = true;
		int is_docked_ = 0;
		bool is_registered_for_docking_ = false;
		bool is_skip_taskbar_ = true;
		std::string title_bar_style_ = "normal";
		double opacity_ = 1;

		bool is_resizing_ = false;
		bool is_moving_ = false;

		HWND GetMainWindow();
		void WindowManagerPlus::ForceRefresh();
		void WindowManagerPlus::ForceChildRefresh();
		void WindowManagerPlus::SetAsFrameless();
		void WindowManagerPlus::WaitUntilReadyToShow();
		void WindowManagerPlus::Destroy();
		void WindowManagerPlus::Close();
		bool WindowManagerPlus::IsPreventClose();
		void WindowManagerPlus::SetPreventClose(const flutter::EncodableMap& args);
		void WindowManagerPlus::Focus();
		void WindowManagerPlus::Blur();
		bool WindowManagerPlus::IsFocused();
		void WindowManagerPlus::Show();
		void WindowManagerPlus::Hide();
		bool WindowManagerPlus::IsVisible();
		bool WindowManagerPlus::IsMaximized();
		void WindowManagerPlus::Maximize(const flutter::EncodableMap& args);
		void WindowManagerPlus::Unmaximize();
		bool WindowManagerPlus::IsMinimized();
		void WindowManagerPlus::Minimize();
		void WindowManagerPlus::Restore();
		bool WindowManagerPlus::IsDockable();
		int WindowManagerPlus::IsDocked();
		void WindowManagerPlus::Dock(const flutter::EncodableMap& args);
		bool WindowManagerPlus::Undock();
		bool WindowManagerPlus::IsFullScreen();
		void WindowManagerPlus::SetFullScreen(const flutter::EncodableMap& args);
		void WindowManagerPlus::SetAspectRatio(const flutter::EncodableMap& args);
		void WindowManagerPlus::SetBackgroundColor(const flutter::EncodableMap& args);
		flutter::EncodableMap WindowManagerPlus::GetBounds(const flutter::EncodableMap& args);
		void WindowManagerPlus::SetBounds(const flutter::EncodableMap& args);
		void WindowManagerPlus::SetMinimumSize(const flutter::EncodableMap& args);
		void WindowManagerPlus::SetMaximumSize(const flutter::EncodableMap& args);
		bool WindowManagerPlus::IsResizable();
		void WindowManagerPlus::SetResizable(const flutter::EncodableMap& args);
		bool WindowManagerPlus::IsMinimizable();
		void WindowManagerPlus::SetMinimizable(const flutter::EncodableMap& args);
		bool WindowManagerPlus::IsMaximizable();
		void WindowManagerPlus::SetMaximizable(const flutter::EncodableMap& args);
		bool WindowManagerPlus::IsClosable();
		void WindowManagerPlus::SetClosable(const flutter::EncodableMap& args);
		bool WindowManagerPlus::IsAlwaysOnTop();
		void WindowManagerPlus::SetAlwaysOnTop(const flutter::EncodableMap& args);
		bool WindowManagerPlus::IsAlwaysOnBottom();
		void WindowManagerPlus::SetAlwaysOnBottom(const flutter::EncodableMap& args);
		std::string WindowManagerPlus::GetTitle();
		void WindowManagerPlus::SetTitle(const flutter::EncodableMap& args);
		void WindowManagerPlus::SetTitleBarStyle(const flutter::EncodableMap& args);
		int WindowManagerPlus::GetTitleBarHeight();
		bool WindowManagerPlus::IsSkipTaskbar();
		void WindowManagerPlus::SetSkipTaskbar(const flutter::EncodableMap& args);
		void WindowManagerPlus::SetProgressBar(const flutter::EncodableMap& args);
		void WindowManagerPlus::SetIcon(const flutter::EncodableMap& args);
		bool WindowManagerPlus::HasShadow();
		void WindowManagerPlus::SetHasShadow(const flutter::EncodableMap& args);
		double WindowManagerPlus::GetOpacity();
		void WindowManagerPlus::SetOpacity(const flutter::EncodableMap& args);
		void WindowManagerPlus::SetBrightness(const flutter::EncodableMap& args);
		void WindowManagerPlus::SetIgnoreMouseEvents(const flutter::EncodableMap& args);
		void WindowManagerPlus::PopUpWindowMenu(const flutter::EncodableMap& args);
		void WindowManagerPlus::StartDragging();
		void WindowManagerPlus::StartResizing(const flutter::EncodableMap& args);

		static int64_t WindowManagerPlus::createWindow(const std::vector<std::string>& args);

	private:
		static constexpr auto kFlutterViewWindowClassName = L"FLUTTERVIEW";
		bool g_is_window_fullscreen = false;
		std::string g_title_bar_style_before_fullscreen;
		RECT g_frame_before_fullscreen;
		bool g_maximized_before_fullscreen;
		LONG g_style_before_fullscreen;
		ITaskbarList3* taskbar_ = nullptr;
		double GetDpiForHwnd(HWND hWnd);
		BOOL WindowManagerPlus::RegisterAccessBar(HWND hwnd, BOOL fRegister);
		void PASCAL WindowManagerPlus::AppBarQuerySetPos(HWND hwnd, UINT uEdge, LPRECT lprc, PAPPBARDATA pabd);
		void WindowManagerPlus::DockAccessBar(HWND hwnd, UINT edge, UINT windowWidth);
	};
}  // namespace window_manager_plus

#endif  // WINDOW_MANAGER_PLUS_PLUGIN_WINDOW_MANAGER_PLUS_H_