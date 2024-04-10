// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:ui';

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import 'floating_window.dart';
import 'lightcard_trackpad.dart';

class FloatingTrackpad extends StatelessWidget {
  const FloatingTrackpad({super.key});

  /// The height of the draggable area at the top of the trackpad.
  static const double _dragBarHeight = 44;

  @override
  Widget build(BuildContext context) {
    final size = FloatingWindow.getDefaultSize(context) + const Offset(0, _dragBarHeight);

    return FloatingWindow(
      settingsPrefix: 'floatingTrackpad',
      icon: Icon(Icons.games_outlined),
      builder: (context) => Stack(
        children: [
          Positioned(
            top: _dragBarHeight - 1,
            bottom: 0,
            left: 0,
            right: 0,
            child: BackdropFilter(
              filter: ImageFilter.blur(sigmaX: 3, sigmaY: 3),
              child: Container(
                color: Colors.black54,
                child: Center(
                  child: Icon(
                    Icons.touch_app_rounded,
                    size: 100,
                    color: Colors.white12,
                  ),
                ),
              ),
            ),
          ),
          Positioned.fill(
            child: LightCardTrackpad(),
          ),
          Container(
            height: _dragBarHeight,
            padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 10),
            alignment: Alignment.centerLeft,
            color: Theme.of(context).colorScheme.surfaceTint,
            child: Row(
              children: [
                Icon(
                  Icons.drag_indicator,
                  color: Colors.white30,
                ),
                const SizedBox(width: 4),
                Expanded(
                  child: Text(
                    AppLocalizations.of(context)!.trackpadTitle.toUpperCase(),
                    style: Theme.of(context).textTheme.titleLarge,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
      draggableInsets: EdgeInsets.only(bottom: size.height - _dragBarHeight),
      size: size,
    );
  }
}
