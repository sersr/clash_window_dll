#include "include/clash_window_dll/clash_window_dll_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <flutter/standard_message_codec.h>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#define WM_TRAY_EVENT (WM_APP + 1)
#define WM_CLOSE_EVENT (WM_APP + 2)
#define WM_SEND_DESTROY (WM_APP + 3)

namespace {
using _CLOSE = std::function<void(const flutter::EncodableValue *)>;

class CloseMethodResult
    : public flutter::MethodResult<flutter::EncodableValue> {
public:
  CloseMethodResult(_CLOSE close_) : close(close_){};
  _CLOSE close;

  void SuccessInternal(const flutter::EncodableValue *result) { close(result); }

  void ErrorInternal(const std::string &error_code,
                     const std::string &error_message,
                     const flutter::EncodableValue *error_details) {
    close(nullptr);
  }

  void NotImplementedInternal() { close(nullptr); }
};

class ClashWindowDllPlugin : public flutter::Plugin {
public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  ClashWindowDllPlugin(
      std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_)
      : channel(std::move(channel_)) {}

  virtual ~ClashWindowDllPlugin();
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel;
  bool hideOnClose_ = false;

private:
  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

// static
void ClashWindowDllPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {

  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "clash_window_dll",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<ClashWindowDllPlugin>(std::move(channel));

  plugin->channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });
  registrar->RegisterTopLevelWindowProcDelegate(
      [plugin = plugin.get()](HWND hwnd, UINT message, WPARAM wparam,
                              LPARAM lparam) -> std::optional<LRESULT> {
        static bool _create = false;
        static bool _destroy = false;
        static bool _destroyReal = false;
        if (!_create) {
          _CLOSE a = [&](const flutter::EncodableValue *result) -> void {
            if (!result->IsNull()) {
              try {
                auto data = std::get<bool>(*result);
                plugin->hideOnClose_ = data;
              } catch (const std::exception &e) {
                std::cerr << e.what() << '\n';
              }
            }
            std::cout << "geHideOnClose.... " << plugin->hideOnClose_
                      << std::endl;
          };
          auto s = std::make_unique<CloseMethodResult>(a);
          plugin->channel->InvokeMethod("getHideOnClose", nullptr,
                                        std::move(s));

          _create = true;
          NOTIFYICONDATA nid;
          nid.cbSize = (DWORD)sizeof(NOTIFYICONDATA);
          nid.hWnd = hwnd;
          nid.uID = 0;
          nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
          nid.uCallbackMessage = WM_TRAY_EVENT;
          nid.uVersion = 4;
          nid.hIcon = LoadIcon(GetModuleHandle(0), (LPWSTR)100);
          wcscpy_s(nid.szTip, TEXT("clash_y"));
          Shell_NotifyIcon(NIM_ADD, &nid);
        }
        switch (message) {

          // ShowWindow(hwnd, SW_HIDE);
        case WM_DESTROY: {
          if (!_destroyReal) {
            return std::optional(0);
          }
          break;
        }
        case WM_CLOSE: {
          if (plugin->hideOnClose_) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
          }
          PostMessage(hwnd, WM_CLOSE_EVENT, 0, 0);
          return std::optional(0);
        }
        case WM_SEND_DESTROY: {
          _destroyReal = true;
          std::cout << "close WM_APP" << std::endl;
          PostMessage(hwnd, WM_DESTROY, 0, 0);
          return std::optional(0);
        }
        case WM_CLOSE_EVENT: {
          if (_destroy)
            return std::optional(0);
          _destroy = true;
          NOTIFYICONDATA nid;
          nid.cbSize = (DWORD)sizeof(NOTIFYICONDATA);
          nid.hWnd = hwnd;
          nid.uID = 0;
          nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
          nid.uCallbackMessage = WM_TRAY_EVENT;
          nid.uVersion = 4;
          nid.hIcon = LoadIcon(GetModuleHandle(0), (LPWSTR)100);
          wcscpy_s(nid.szTip, TEXT("clash_y"));
          Shell_NotifyIcon(NIM_DELETE, &nid);

          auto s = std::make_unique<CloseMethodResult>(
              [hwnd](const flutter::EncodableValue *result) {
                std::cout << "close ....CloseMethodResult...." << std::endl;
                PostMessage(hwnd, WM_SEND_DESTROY, 0, 0);
              });

          plugin->channel->InvokeMethod(
              "close", std::make_unique<flutter::EncodableValue>(hwnd),
              std::move(s));

          return std::optional(0);
        }
        case WM_COMMAND: {
          switch (wparam) {
          case WM_CLOSE_EVENT:
            PostMessage(hwnd, WM_CLOSE_EVENT, 0, 0);
            return std::optional(0);

          default:
            break;
          }
          break;
        }
        case WM_TRAY_EVENT: {
          switch (LOWORD(lparam)) {
          case NIN_SELECT:
          case WM_LBUTTONUP: {
            auto show = IsWindowVisible(hwnd) == 1;
            ShowWindow(hwnd, show ? SW_HIDE : SW_SHOW);
            SetForegroundWindow(hwnd);
            if (!show) {
              plugin->channel->InvokeMethod("showWindow", nullptr);
            }
            return std::optional(0);
          }
          case WM_CONTEXTMENU:
          case WM_RBUTTONUP: {
            POINT point;
            GetCursorPos(&point);
            auto hMenu = CreateMenu();
            AppendMenu(hMenu, MF_STRING, WM_CLOSE_EVENT, TEXT("&Quit"));
            auto hMuenubar = CreateMenu();
            AppendMenu(hMuenubar, MF_POPUP, (UINT_PTR)hMenu, TEXT("_Parent"));
            auto uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0) {
              uFlags |= TPM_RIGHTALIGN;
            } else {
              uFlags |= TPM_LEFTALIGN;
            }
            auto hm = GetSubMenu(hMuenubar, 0);
            SetForegroundWindow(hwnd);
            TrackPopupMenuEx(hm, uFlags, point.x, point.y, hwnd, nullptr);
            DestroyMenu(hm);
            return std::optional(0);
          }
          default:
            break;
          }
          break;
        }
        default:
          break;
        }
        return std::nullopt;
      });
  registrar->AddPlugin(std::move(plugin));
}

ClashWindowDllPlugin::~ClashWindowDllPlugin() {}

void ClashWindowDllPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  auto name = method_call.method_name();
  if (name.compare("hideOnClose") == 0) {
    auto args = method_call.arguments();
    if (!args->IsNull()) {
      try {
        auto value = std::get<bool>(*args);
        hideOnClose_ = value;
        std::cout << "curentValue " << hideOnClose_ << std::endl;
      } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
      }
    }
    result->Success();
  } else if (name.compare("getHideOnClose") == 0) {
    return result->Success(flutter::EncodableValue(hideOnClose_));
  } else {
    result->NotImplemented();
  }
}

} // namespace

void ClashWindowDllPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  ClashWindowDllPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
