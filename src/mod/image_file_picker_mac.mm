#ifdef __APPLE__
#include "settings/image_file_picker.h"
#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>
#include <memory>

// Image branch of pinned PLAY's audio_file_picker_mac.mm. Cocoa remains on its
// main queue; the caller receives only an owned UTF-8 path future.
std::future<std::string> OpenImageFilePicker()
{
  auto result = std::make_shared<std::promise<std::string>>();
  auto future = result->get_future();
  dispatch_async(dispatch_get_main_queue(), ^{
    @autoreleasepool {
      NSOpenPanel* panel = [NSOpenPanel openPanel];
      [panel setTitle:@"Choose a loading image"];
      [panel setCanChooseFiles:YES];
      [panel setCanChooseDirectories:NO];
      [panel setAllowsMultipleSelection:NO];
      [panel setAllowedFileTypes:@[@"png", @"jpg", @"jpeg"]];
      [panel beginWithCompletionHandler:^(NSModalResponse response) {
        const char* path = response == NSModalResponseOK ? [[[panel URL] path] UTF8String] : nullptr;
        result->set_value(path ? std::string(path) : std::string{});
      }];
    }
  });
  return future;
}
#endif
