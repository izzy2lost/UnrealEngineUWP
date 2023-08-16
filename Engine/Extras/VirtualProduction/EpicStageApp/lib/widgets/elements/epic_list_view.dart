// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import '../../utilities/unreal_colors.dart';

typedef IndexedWidgetBuilder(BuildContext context, int index);

/// Core ListView for the app, wraps a [EpicScrollBar] + [ListView.builder].
class EpicListView extends StatefulWidget {
  /// defines extent for the children of the ListView.
  final double? itemExtent;

  /// Number of items to be rendered on the list.
  final int itemCount;

  /// Additional padding around the main scrollable area.
  final EdgeInsets padding;

  /// Builders callback for rendering each item in the current [context] and for the given [index]
  final IndexedWidgetBuilder itemBuilder;

  /// whether to show scrollbar or not.
  final bool? bShowScrollbar;

  /// Color of the scrollbar thumb.
  final Color? scrollbarThumbColor;

  EpicListView({
    Key? key,
    required this.itemBuilder,
    required this.itemCount,
    this.itemExtent,
    this.padding = EdgeInsets.zero,
    this.bShowScrollbar,
    this.scrollbarThumbColor,
  }) : super(key: key);

  @override
  _EpicListViewState createState() => _EpicListViewState();
}

class _EpicListViewState extends State<EpicListView> {
  late _ScrollControllerWithMixin scrollController;

  @override
  void initState() {
    super.initState();
    scrollController = _ScrollControllerWithMixin();
  }

  @override
  void didUpdateWidget(EpicListView oldWidget) {
    super.didUpdateWidget(oldWidget);
  }

  @override
  Widget build(BuildContext context) {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      setState(() {});
    });
    return EpicScrollBar(
      controller: scrollController,
      padding: widget.padding.add(
        EdgeInsets.only(
          right: (scrollController.bIsScrollable ?? false) ? 8 : 0,
        ),
      ) as EdgeInsets,
      bEnabled: widget.bShowScrollbar ?? true,
      thumbColor: widget.scrollbarThumbColor,
      child: ScrollConfiguration(
        behavior: ScrollConfiguration.of(context).copyWith(scrollbars: false),
        child: ListView.builder(
          controller: scrollController,
          itemExtent: widget.itemExtent,
          itemCount: widget.itemCount,
          padding: EdgeInsets.zero,
          itemBuilder: (BuildContext context, int index) {
            return Padding(
              padding: EdgeInsets.only(
                right: (scrollController.bIsScrollable ?? false) ? 15.0 : 0.0,
                top: (index > 0) ? 2 : 0,
              ),
              child: widget.itemBuilder(context, index),
            );
          },
        ),
      ),
    );
  }
}

/// Mixin for reusable scroll controller with easy access to internal methods.
mixin ScrollControllerMixin on ScrollController {
  /// whether scrollview the controller is attached to is scrollable or not.
  bool? get bIsScrollable {
    if (this.hasClients && this.position.hasContentDimensions) {
      return this.hasClients && (this.position.minScrollExtent < this.position.maxScrollExtent);
    }
    return false;
  }
}

/// ScrollController for [EpicListView] with [ScrollControllerMixin] for easy usage.
class _ScrollControllerWithMixin extends ScrollController with ScrollControllerMixin {}

/// Styled theme scroll bar.
class EpicScrollBar extends StatelessWidget {
  const EpicScrollBar({
    Key? key,
    required this.child,
    required this.controller,
    this.bEnabled = true,
    this.notification = defaultScrollNotificationPredicate,
    this.thumbColor,
    this.padding = EdgeInsets.zero,
  }) : super(key: key);

  /// Whether styled scrollbar is enabled or not.
  final bool bEnabled;

  /// Child widget to be wrapped with scrollbar possibly be a list.
  final Widget child;

  /// Creating a binding with the [child] scroll view.
  final ScrollController controller;

  /// Color for the scrollbar handle or thumb region.
  final Color? thumbColor;

  /// Scrollbar padding.
  final EdgeInsets padding;

  /// Tell the scrollbar which ScrollView to control and respond to as [child] could have multiple scroll views.
  final ScrollNotificationPredicate notification;

  @override
  Widget build(BuildContext context) {
    return bEnabled
        ? Padding(
            padding: padding,
            child: RawScrollbar(
              padding: EdgeInsets.only(top: 4, bottom: 4),
              controller: controller,
              radius: Theme.of(context).scrollbarTheme.radius,
              thickness: 8,
              trackVisibility: false,
              thumbVisibility: true,
              thumbColor: thumbColor ?? UnrealColors.gray22,
              notificationPredicate: notification,
              child: child,
            ),
          )
        : child;
  }
}
