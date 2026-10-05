//
//  LauncherMain.swift
//  STFC Community Mod Launcher
//
//  Created by tashcan on 3/22/24.
//

import AppKit
import Foundation
import SwiftUI

extension Scene {
  func windowResizabilityContentSize() -> some Scene {
    if #available(macOS 13.0, *) {
      return windowResizability(.contentSize)
    } else {
      return self
    }
  }
}

class AppDelegate: NSObject, NSApplicationDelegate {
  func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
    NSApplication.shared.terminate(self)
    return true
  }
}

@main
struct STFC_Community_Patch_LauncherApp: App {
  @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

  var body: some Scene {
    WindowGroup {
      ContentView()
        .frame(width: 600, height: 400)
        .fixedSize()
    }
    .windowResizabilityContentSize()
    .commands {
      CommandGroup(after: .sidebar) {
        Divider()
        Button("View Mod Folder") {
          openModFolder()
        }
      }
    }
  }

  private func openModFolder() {
    guard let directoryURL = ModFiles.directoryURL else {
      showFolderError("Could not locate your Library folder.")
      return
    }
    var isDirectory: ObjCBool = false
    guard FileManager.default.fileExists(atPath: directoryURL.path, isDirectory: &isDirectory),
      isDirectory.boolValue
    else {
      showFolderError(
        "The mod folder is not available at \(directoryURL.path). Launch the game with the mod once to create it.")
      return
    }
    if !NSWorkspace.shared.open(directoryURL) {
      showFolderError("Could not open the mod folder in Finder.")
    }
  }

  private func showFolderError(_ message: String) {
    let alert = NSAlert()
    alert.messageText = "Could not open mod folder"
    alert.informativeText = message
    alert.addButton(withTitle: "OK")
    alert.runModal()
  }
}
