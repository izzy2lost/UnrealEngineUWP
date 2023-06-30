//
//  SwiftUIView.swift
//  IOSOnVision
//
//  Created by Josh Adams on 6/28/23.
//

import SwiftUI

struct SwiftUIView: View {
  var onClick: () -> Void = {}
  
  var body: some View {
	VStack(spacing: 8) {
	  Text("SwiftUI in Unreal!")
		.font(.title)
		.bold()
	  Button(action: {
		onClick()
	  }, label: {
		Text("Test Button")
	  })
	}
  }
}


class HostingViewFactory: NSObject
{
  @objc static func MakeSwiftUIView(OnClick: @escaping (() -> Void)) -> UIViewController
  {
	return UIHostingController(rootView: SwiftUIView(onClick: OnClick))
  }
}


//#Preview {
//    SwiftUIView()
//}
