import 'dart:async';

import 'package:flutter/services.dart';

typedef _Listen = Future Function(MethodCall call);

class ClashWindowDll {
  static final MethodChannel _channel = const MethodChannel('clash_window_dll')
    ..setMethodCallHandler(_listen);

  static setListen(_Listen? listen) {
    _all = listen;
  }

  static _Listen? _all;
  static _Listen? _getHide;
  static void Function()? onShowWindow;
  static void setGetHide(_Listen hide) {
    _getHide = hide;
  }

  static Future _listen(MethodCall call) async {
    if (call.method == 'getHideOnClose') {
      if (_getHide != null) {
        return _getHide!(call);
      }
      return true;
    }
    if (call.method == 'showWindow') {
      onShowWindow?.call();
      return;
    }

    if (call.method == 'close') {
      if (_all != null) {
        await _all!(call);
      }
    }
  }

  static Future<void> setHideOnClose(bool hideOnClose) async {
    await _channel.invokeMethod('hideOnClose', hideOnClose);
  }

  static Future<bool> get hideOnClose async {
    try {
      final result = await _channel.invokeMethod('getHideOnClose') as bool;
      return result;
    } catch (e) {
      return false;
    }
  }
}
