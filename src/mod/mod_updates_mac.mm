#include <string_view>

#import <AppKit/AppKit.h>

bool OpenModReleaseMac(std::string_view url)
{
  @autoreleasepool {
    NSString* text = [[NSString alloc] initWithBytes:url.data() length:url.size() encoding:NSUTF8StringEncoding];
    NSURL* address = text ? [NSURL URLWithString:text] : nil;
    const bool opened = address && [[NSWorkspace sharedWorkspace] openURL:address];
#if !__has_feature(objc_arc)
    [text release];
#endif
    return opened;
  }
}
