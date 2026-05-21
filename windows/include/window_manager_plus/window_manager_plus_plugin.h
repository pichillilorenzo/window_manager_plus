#ifndef FLUTTER_PLUGIN_WINDOW_MANAGER_PLUS_PLUGIN_H_
#define FLUTTER_PLUGIN_WINDOW_MANAGER_PLUS_PLUGIN_H_

#include <windows.h>
#include <any>
#include <map>
#include <string>
#include <vector>

#include <flutter_plugin_registrar.h>

#include <memory>

#ifdef FLUTTER_PLUGIN_IMPL
#define FLUTTER_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FLUTTER_PLUGIN_EXPORT __declspec(dllimport)
#endif

#if defined(__cplusplus)
extern "C" {
#endif

	FLUTTER_PLUGIN_EXPORT void WindowManagerPlusPluginRegisterWithRegistrar(FlutterDesktopPluginRegistrarRef registrar);

#if defined(__cplusplus)
}  // extern "C"
#endif

typedef void (*WindowManagerPlusPluginWindowCreatedCallback)(int64_t windowId, std::vector<std::string> command_line_arguments);
FLUTTER_PLUGIN_EXPORT void WindowManagerPlusPluginSetWindowCreatedCallback(WindowManagerPlusPluginWindowCreatedCallback callback);

#endif  // FLUTTER_PLUGIN_WINDOW_MANAGER_PLUS_PLUGIN_H_
