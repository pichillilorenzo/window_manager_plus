#include "include/window_manager_plus/window_manager_plus_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

#include <flutter/method_channel.h>
#include <flutter/method_result_functions.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <codecvt>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

#include "window_manager_plus.h"

namespace window_manager_plus {

bool IsWindows11OrGreater() {
  DWORD dwVersion = 0;
  DWORD dwBuild = 0;

#pragma warning(push)
#pragma warning(disable : 4996)
  dwVersion = GetVersion();
  // Get the build number.
  if (dwVersion < 0x80000000)
    dwBuild = (DWORD)(HIWORD(dwVersion));
#pragma warning(pop)

  return dwBuild < 22000;
}

std::mutex threadMtx;

class WindowManagerPlusPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

  WindowManagerPlusPlugin(flutter::PluginRegistrarWindows* registrar);

  virtual ~WindowManagerPlusPlugin();

 private:
  std::shared_ptr<WindowManagerPlus> window_manager;
  flutter::PluginRegistrarWindows* registrar;

  // The ID of the WindowProc delegate registration.
  int window_proc_id = -1;

  void WindowManagerPlusPlugin::_EmitEvent(std::string eventName);
  void WindowManagerPlusPlugin::_EmitGlobalEvent(std::string eventName);
  // Called for top-level WindowProc delegation.
  std::optional<LRESULT> WindowManagerPlusPlugin::HandleWindowProc(
      HWND hWnd,
      UINT message,
      WPARAM wParam,
      LPARAM lParam);
  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  static void HandleStaticMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  void adjustNCCALCSIZE(HWND hwnd, NCCALCSIZE_PARAMS* sz) {
    LONG l = 8;
    LONG t = 8;

    // HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    // Don't use `MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)` above.
    // Because if the window is restored from minimized state, the window is not
    // in the correct monitor. The monitor is always the left-most monitor.
    // https://github.com/leanflutter/window_manager/issues/489
    HMONITOR monitor = MonitorFromRect(&sz->rgrc[0], MONITOR_DEFAULTTONEAREST);
    if (monitor != NULL) {
      MONITORINFO monitorInfo;
      monitorInfo.cbSize = sizeof(MONITORINFO);
      if (TRUE == GetMonitorInfo(monitor, &monitorInfo)) {
        l = sz->rgrc[0].left - monitorInfo.rcWork.left;
        t = sz->rgrc[0].top - monitorInfo.rcWork.top;
      } else {
        // GetMonitorInfo failed, use (8, 8) as default value
      }
    } else {
      // unreachable code
    }

    sz->rgrc[0].left -= l;
    sz->rgrc[0].top -= t;
    sz->rgrc[0].right += l;
    sz->rgrc[0].bottom += t;
  }
};

// static
void WindowManagerPlusPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto plugin = std::make_unique<WindowManagerPlusPlugin>(registrar);

  registrar->AddPlugin(std::move(plugin));
}

WindowManagerPlusPlugin::WindowManagerPlusPlugin(
    flutter::PluginRegistrarWindows* registrar)
    : registrar(registrar) {
  window_manager = std::make_shared<WindowManagerPlus>();
  window_manager->static_channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "window_manager_plus_static",
          &flutter::StandardMethodCodec::GetInstance());
  window_manager->static_channel->SetMethodCallHandler(
      [](const auto& call, auto result) {
        HandleStaticMethodCall(call, std::move(result));
      });
  window_manager->channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "window_manager_plus",
          &flutter::StandardMethodCodec::GetInstance());
  window_manager->channel->SetMethodCallHandler(
      [this](const auto& call, auto result) {
        HandleMethodCall(call, std::move(result));
      });

  window_proc_id = registrar->RegisterTopLevelWindowProcDelegate(
      [this](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        return HandleWindowProc(hWnd, message, wParam, lParam);
      });
}

WindowManagerPlusPlugin::~WindowManagerPlusPlugin() {
#ifndef NDEBUG
  std::cout << "WindowManagerPlugin dealloc" << std::endl;
#endif
  registrar->UnregisterTopLevelWindowProcDelegate(window_proc_id);
  window_manager->channel = nullptr;

  auto id = window_manager->id;
  if (WindowManagerPlus::windowManagers_.find(id) !=
      WindowManagerPlus::windowManagers_.end()) {
    WindowManagerPlus::windowManagers_.erase(id);
  }
  if (WindowManagerPlus::windows_.find(id) !=
      WindowManagerPlus::windows_.end()) {
    WindowManagerPlus::windows_[id]->Destroy();
    // calling WindowManager::windows_.erase(id); will cause a crash
    std::thread([&]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      threadMtx.lock();
      if (WindowManagerPlus::windows_.find(id) !=
          WindowManagerPlus::windows_.end()) {
        WindowManagerPlus::windows_.erase(id);
      }
      threadMtx.unlock();
    }).detach();
  }
}

void WindowManagerPlusPlugin::_EmitEvent(std::string eventName) {
  if (window_manager == nullptr || window_manager->channel == nullptr)
    return;
  flutter::EncodableMap args = flutter::EncodableMap();
  args[flutter::EncodableValue("eventName")] =
      flutter::EncodableValue(eventName);
  window_manager->channel->InvokeMethod(
      "onEvent", std::make_unique<flutter::EncodableValue>(args));

  _EmitGlobalEvent(eventName);
}

void WindowManagerPlusPlugin::_EmitGlobalEvent(std::string eventName) {
  for (auto wManagerPair : WindowManagerPlus::windowManagers_) {
    if (wManagerPair.second->channel) {
      wManagerPair.second->channel->InvokeMethod(
          "onEvent",
          std::make_unique<flutter::EncodableValue>(flutter::EncodableMap{
              {flutter::EncodableValue("eventName"),
               flutter::EncodableValue(eventName)},
              {flutter::EncodableValue("windowId"),
               flutter::EncodableValue(window_manager->id)}}));
    }
  }
}

std::optional<LRESULT> WindowManagerPlusPlugin::HandleWindowProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
  std::optional<LRESULT> result = std::nullopt;

  if (message == WM_DPICHANGED) {
    window_manager->pixel_ratio_ =
        (float)LOWORD(wParam) / USER_DEFAULT_SCREEN_DPI;
    window_manager->ForceChildRefresh();
  }

  if (wParam && message == WM_NCCALCSIZE) {
    if (window_manager->IsFullScreen() &&
        window_manager->title_bar_style_ != "normal") {
      if (window_manager->is_frameless_) {
        adjustNCCALCSIZE(hWnd, reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam));
      }
      return 0;
    }
    // This must always be before handling title_bar_style_ == "hidden" so
    // the `if TitleBarStyle.hidden` doesn't get executed.
    if (window_manager->is_frameless_) {
      if (window_manager->IsMaximized()) {
        adjustNCCALCSIZE(hWnd, reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam));
      }
      return 0;
    }

    // This must always be last.
    if (wParam && window_manager->title_bar_style_ == "hidden") {
      if (window_manager->IsMaximized()) {
        // Adjust the borders when maximized so the app isn't cut off
        adjustNCCALCSIZE(hWnd, reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam));
      } else {
        NCCALCSIZE_PARAMS* sz = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
        // on windows 10, if set to 0, there's a white line at the top
        // of the app and I've yet to find a way to remove that.
        sz->rgrc[0].top += IsWindows11OrGreater() ? 0 : 1;
        // The following lines are required for resizing the window.
        // https://github.com/leanflutter/window_manager/issues/483
        sz->rgrc[0].right -= 8;
        sz->rgrc[0].bottom -= 8;
        sz->rgrc[0].left -= -8;
      }

      // Previously (WVR_HREDRAW | WVR_VREDRAW), but returning 0 or 1 doesn't
      // actually break anything so I've set it to 0. Unless someone pointed a
      // problem in the future.
      return 0;
    }
  } else if (message == WM_NCHITTEST) {
    if (!window_manager->is_resizable_) {
      return HTNOWHERE;
    }
  } else if (message == WM_GETMINMAXINFO) {
    MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
    // For the special "unconstrained" values, leave the defaults.
    if (window_manager->minimum_size_.x != 0)
      info->ptMinTrackSize.x = static_cast<LONG>(
          window_manager->minimum_size_.x * window_manager->pixel_ratio_);
    if (window_manager->minimum_size_.y != 0)
      info->ptMinTrackSize.y = static_cast<LONG>(
          window_manager->minimum_size_.y * window_manager->pixel_ratio_);
    if (window_manager->maximum_size_.x != -1)
      info->ptMaxTrackSize.x = static_cast<LONG>(
          window_manager->maximum_size_.x * window_manager->pixel_ratio_);
    if (window_manager->maximum_size_.y != -1)
      info->ptMaxTrackSize.y = static_cast<LONG>(
          window_manager->maximum_size_.y * window_manager->pixel_ratio_);
    result = 0;
  } else if (message == WM_NCACTIVATE) {
    if (wParam != 0) {
      _EmitEvent("focus");
    } else {
      _EmitEvent("blur");
    }

    if (window_manager->title_bar_style_ == "hidden" ||
        window_manager->is_frameless_)
      return 1;
  } else if (message == WM_EXITSIZEMOVE) {
    if (window_manager->is_resizing_) {
      _EmitEvent("resized");
      window_manager->is_resizing_ = false;
    }
    if (window_manager->is_moving_) {
      _EmitEvent("moved");
      window_manager->is_moving_ = false;
    }
    return false;
  } else if (message == WM_MOVING) {
    window_manager->is_moving_ = true;
    _EmitEvent("move");
    return false;
  } else if (message == WM_SIZING) {
    window_manager->is_resizing_ = true;
    _EmitEvent("resize");

    if (window_manager->aspect_ratio_ > 0) {
      RECT* rect = (LPRECT)lParam;

      double aspect_ratio = window_manager->aspect_ratio_;

      int new_width = static_cast<int>(rect->right - rect->left);
      int new_height = static_cast<int>(rect->bottom - rect->top);

      bool is_resizing_horizontally =
          wParam == WMSZ_LEFT || wParam == WMSZ_RIGHT ||
          wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT;

      if (is_resizing_horizontally) {
        new_height = static_cast<int>(new_width / aspect_ratio);
      } else {
        new_width = static_cast<int>(new_height * aspect_ratio);
      }

      int left = rect->left;
      int top = rect->top;
      int right = rect->right;
      int bottom = rect->bottom;

      switch (wParam) {
        case WMSZ_RIGHT:
        case WMSZ_BOTTOM:
          right = new_width + left;
          bottom = top + new_height;
          break;
        case WMSZ_TOP:
          right = new_width + left;
          top = bottom - new_height;
          break;
        case WMSZ_LEFT:
        case WMSZ_TOPLEFT:
          left = right - new_width;
          top = bottom - new_height;
          break;
        case WMSZ_TOPRIGHT:
          right = left + new_width;
          top = bottom - new_height;
          break;
        case WMSZ_BOTTOMLEFT:
          left = right - new_width;
          bottom = top + new_height;
          break;
        case WMSZ_BOTTOMRIGHT:
          right = left + new_width;
          bottom = top + new_height;
          break;
      }

      rect->left = left;
      rect->top = top;
      rect->right = right;
      rect->bottom = bottom;
    }
  } else if (message == WM_SIZE) {
    if (window_manager->IsFullScreen() && wParam == SIZE_MAXIMIZED &&
        window_manager->last_state != STATE_FULLSCREEN_ENTERED) {
      _EmitEvent("enter-full-screen");
      window_manager->last_state = STATE_FULLSCREEN_ENTERED;
    } else if (!window_manager->IsFullScreen() && wParam == SIZE_RESTORED &&
               window_manager->last_state == STATE_FULLSCREEN_ENTERED) {
      window_manager->ForceChildRefresh();
      _EmitEvent("leave-full-screen");
      window_manager->last_state = STATE_NORMAL;
    } else if (window_manager->last_state != STATE_FULLSCREEN_ENTERED) {
      if (wParam == SIZE_MAXIMIZED) {
        _EmitEvent("maximize");
        window_manager->last_state = STATE_MAXIMIZED;
      } else if (wParam == SIZE_MINIMIZED) {
        _EmitEvent("minimize");
        window_manager->last_state = STATE_MINIMIZED;
        return 0;
      } else if (wParam == SIZE_RESTORED) {
        if (window_manager->last_state == STATE_MAXIMIZED) {
          _EmitEvent("unmaximize");
          window_manager->last_state = STATE_NORMAL;
        } else if (window_manager->last_state == STATE_MINIMIZED) {
          _EmitEvent("restore");
          window_manager->last_state = STATE_NORMAL;
        }
      }
    }
  } else if (message == WM_CLOSE) {
    _EmitEvent("close");
    if (window_manager->IsPreventClose()) {
      return -1;
    }
  } else if (message == WM_SHOWWINDOW) {
    if (wParam == TRUE) {
      _EmitEvent("show");
    } else {
      _EmitEvent("hide");
    }
  } else if (message == WM_WINDOWPOSCHANGED) {
    if (window_manager->IsAlwaysOnBottom()) {
      const flutter::EncodableMap& args = {
          {flutter::EncodableValue("isAlwaysOnBottom"),
           flutter::EncodableValue(true)}};
      window_manager->SetAlwaysOnBottom(args);
    }
  }

  return result;
}

void WindowManagerPlusPlugin::HandleStaticMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  std::string method_name = method_call.method_name();

  const flutter::EncodableMap& args =
      method_call.arguments()->IsNull()
          ? flutter::EncodableMap()
          : std::get<flutter::EncodableMap>(*method_call.arguments());
  /*auto windowId =
      args.find(flutter::EncodableValue("windowId")) != args.end()
          ? std::get<int>(args.at(flutter::EncodableValue("windowId")))
          : -1;*/

  if (method_name.compare("createWindow") == 0) {
    auto encodedArgs = args.at(flutter::EncodableValue("args")).IsNull()
                           ? flutter::EncodableList()
                           : std::get<flutter::EncodableList>(
                                 args.at(flutter::EncodableValue("args")));
    std::vector<std::string> windowArgs;
    for (const auto& arg : encodedArgs) {
      if (std::holds_alternative<std::string>(arg)) {
        windowArgs.push_back(std::get<std::string>(arg));
      }
    }
    auto newWindowId = WindowManagerPlus::createWindow(windowArgs);
    result->Success(newWindowId >= 0 ? flutter ::EncodableValue(newWindowId)
                                     : flutter ::EncodableValue());
  } else if (method_name.compare("getAllWindowManagerIds") == 0) {
    std::vector<int64_t> windowIds;
    for (auto& window : WindowManagerPlus::windowManagers_) {
      windowIds.push_back(window.first);
    }
    result->Success(flutter::EncodableValue(windowIds));
  } else {
    result->NotImplemented();
  }
}

void WindowManagerPlusPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  std::string method_name = method_call.method_name();

  const flutter::EncodableMap& args =
      method_call.arguments()->IsNull()
          ? flutter::EncodableMap()
          : std::get<flutter::EncodableMap>(*method_call.arguments());
  auto windowId =
      args.find(flutter::EncodableValue("windowId")) != args.end()
          ? std::get<int>(args.at(flutter::EncodableValue("windowId")))
          : -1;
  auto wManager = window_manager;
  if (windowId >= 0 && WindowManagerPlus::windowManagers_.find(windowId) !=
                           WindowManagerPlus::windowManagers_.end()) {
    wManager = WindowManagerPlus::windowManagers_[windowId];
  }

  if (method_name.compare("ensureInitialized") == 0) {
    if (windowId >= 0) {
      // if this method is called when window manager already exists, this may be due to a hot restart in flutter.
      // Clear the old channel if applicable.
      auto it = WindowManagerPlus::windowManagers_.find(windowId);
      if (it != WindowManagerPlus::windowManagers_.end()) {
        auto existing_manager = it->second;
        if (existing_manager->channel) {
          existing_manager->channel->SetMethodCallHandler(nullptr);
          existing_manager->channel.reset();  // clear old channel
        }
      }

      window_manager->id = windowId;
      window_manager->native_window =
          ::GetAncestor(registrar->GetView()->GetNativeWindow(), GA_ROOT);

      // create new channel
      window_manager->channel =
          std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
              registrar->messenger(),
              "window_manager_plus_" + std::to_string(windowId),
              &flutter::StandardMethodCodec::GetInstance());
      window_manager->channel->SetMethodCallHandler(
          [this](const auto& call, auto result) {
            HandleMethodCall(call, std::move(result));
          });

      WindowManagerPlus::windowManagers_[windowId] = window_manager;
      result->Success(flutter::EncodableValue(true));
      _EmitGlobalEvent("initialized");
    } else {
      result->Error("0", "Cannot ensureInitialized! windowId >= 0 is required");
    }
  } else if (method_name.compare("invokeMethodToWindow") == 0) {
    auto targetWindowId =
        std::get<int>(args.at(flutter::EncodableValue("targetWindowId")));
    if (WindowManagerPlus::windowManagers_.find(targetWindowId) !=
        WindowManagerPlus::windowManagers_.end()) {
      auto result_ =
          std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>(
              std::move(result));
      WindowManagerPlus::windowManagers_[targetWindowId]->channel->InvokeMethod(
          "onEvent",
          std::make_unique<flutter::EncodableValue>(
              args.at(flutter::EncodableValue("args"))),
          std::make_unique<
              flutter::MethodResultFunctions<flutter::EncodableValue>>(
              [result_](const flutter::EncodableValue* val) {
                // Success
                result_->Success(*val);
              },
              [result_](const std::string& error_code,
                        const std::string& error_message,
                        const flutter::EncodableValue* error_details) {
                // Error
                result_->Error(error_code, error_message);
              },
              [result_]() {
                // Not implemented
                result_->Error("0", "Method not implemented");
              }));
    } else {
      result->Error("0",
                    "Cannot invokeMethodToWindow! targetWindowId not found");
    }
  } else if (method_name.compare("waitUntilReadyToShow") == 0) {
    wManager->WaitUntilReadyToShow();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("setAsFrameless") == 0) {
    wManager->SetAsFrameless();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("destroy") == 0) {
    wManager->Destroy();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("close") == 0) {
    wManager->Close();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isPreventClose") == 0) {
    auto value = wManager->IsPreventClose();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setPreventClose") == 0) {
    wManager->SetPreventClose(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("focus") == 0) {
    wManager->Focus();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("blur") == 0) {
    wManager->Blur();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isFocused") == 0) {
    bool value = wManager->IsFocused();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("show") == 0) {
    wManager->Show();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("hide") == 0) {
    wManager->Hide();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isVisible") == 0) {
    bool value = wManager->IsVisible();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("isMaximized") == 0) {
    bool value = wManager->IsMaximized();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("maximize") == 0) {
    wManager->Maximize(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("unmaximize") == 0) {
    wManager->Unmaximize();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isMinimized") == 0) {
    bool value = wManager->IsMinimized();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("minimize") == 0) {
    wManager->Minimize();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("restore") == 0) {
    wManager->Restore();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isDockable") == 0) {
    bool value = wManager->IsDockable();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("isDocked") == 0) {
    int value = wManager->IsDocked();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("dock") == 0) {
    wManager->Dock(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("undock") == 0) {
    bool value = wManager->Undock();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("isFullScreen") == 0) {
    bool value = wManager->IsFullScreen();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setFullScreen") == 0) {
    wManager->SetFullScreen(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("setAspectRatio") == 0) {
    wManager->SetAspectRatio(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("setBackgroundColor") == 0) {
    wManager->SetBackgroundColor(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("getBounds") == 0) {
    flutter::EncodableMap value = wManager->GetBounds(args);
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setBounds") == 0) {
    wManager->SetBounds(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("setMinimumSize") == 0) {
    wManager->SetMinimumSize(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("setMaximumSize") == 0) {
    wManager->SetMaximumSize(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isResizable") == 0) {
    bool value = wManager->IsResizable();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setResizable") == 0) {
    wManager->SetResizable(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isMinimizable") == 0) {
    bool value = wManager->IsMinimizable();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setMinimizable") == 0) {
    wManager->SetMinimizable(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isMaximizable") == 0) {
    bool value = wManager->IsMaximizable();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setMaximizable") == 0) {
    wManager->SetMaximizable(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isClosable") == 0) {
    bool value = wManager->IsClosable();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setClosable") == 0) {
    wManager->SetClosable(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isAlwaysOnTop") == 0) {
    bool value = wManager->IsAlwaysOnTop();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setAlwaysOnTop") == 0) {
    wManager->SetAlwaysOnTop(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("isAlwaysOnBottom") == 0) {
    bool value = wManager->IsAlwaysOnBottom();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setAlwaysOnBottom") == 0) {
    wManager->SetAlwaysOnBottom(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("getTitle") == 0) {
    std::string value = wManager->GetTitle();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setTitle") == 0) {
    wManager->SetTitle(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("setTitleBarStyle") == 0) {
    wManager->SetTitleBarStyle(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("getTitleBarHeight") == 0) {
    int value = wManager->GetTitleBarHeight();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("isSkipTaskbar") == 0) {
    bool value = wManager->IsSkipTaskbar();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setSkipTaskbar") == 0) {
    wManager->SetSkipTaskbar(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("setProgressBar") == 0) {
    wManager->SetProgressBar(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("setIcon") == 0) {
    wManager->SetIcon(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("hasShadow") == 0) {
    bool value = wManager->HasShadow();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setHasShadow") == 0) {
    wManager->SetHasShadow(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("getOpacity") == 0) {
    double value = wManager->GetOpacity();
    result->Success(flutter::EncodableValue(value));
  } else if (method_name.compare("setOpacity") == 0) {
    wManager->SetOpacity(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("setBrightness") == 0) {
    wManager->SetBrightness(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("setIgnoreMouseEvents") == 0) {
    wManager->SetIgnoreMouseEvents(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("popUpWindowMenu") == 0) {
    wManager->PopUpWindowMenu(args);
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("startDragging") == 0) {
    wManager->StartDragging();
    result->Success(flutter::EncodableValue(true));
  } else if (method_name.compare("startResizing") == 0) {
    wManager->StartResizing(args);
    result->Success(flutter::EncodableValue(true));
  } else {
    result->NotImplemented();
  }
}

}  // namespace window_manager_plus

void WindowManagerPlusPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  window_manager_plus::WindowManagerPlusPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
