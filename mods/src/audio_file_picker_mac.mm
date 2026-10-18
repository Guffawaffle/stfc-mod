#ifdef __APPLE__
#include "settings/audio_file_picker.h"
#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>
#include <memory>

namespace
{
std::future<std::string> OpenFilePicker(bool image)
{
  auto result = std::make_shared<std::promise<std::string>>();
  auto future = result->get_future();
  dispatch_async(dispatch_get_main_queue(), ^{
    @autoreleasepool {
      NSOpenPanel* panel = [NSOpenPanel openPanel];
      [panel setTitle:image ? @"Choose a loading image" : @"Choose an alert sound"];
      [panel setCanChooseFiles:YES];
      [panel setCanChooseDirectories:NO];
      [panel setAllowsMultipleSelection:NO];
      [panel setAllowedFileTypes:image ? @[@"png", @"jpg", @"jpeg"] : @[@"wav", @"mp3"]];
      [panel beginWithCompletionHandler:^(NSModalResponse response) {
        const char* path = response == NSModalResponseOK ? [[[panel URL] path] UTF8String] : nullptr;
        result->set_value(path ? std::string(path) : std::string{});
      }];
    }
  });
  return future;
}
} // namespace
std::future<std::string> OpenAudioFilePicker() { return OpenFilePicker(false); }
std::future<std::string> OpenImageFilePicker() { return OpenFilePicker(true); }
#endif
