#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include <algorithm>
#include <string>
#include "TargetConditionals.h"
#include <ApplicationServices/ApplicationServices.h>
#include <iostream>

namespace SL
{
namespace Screen_Capture
{

    std::vector<Window> GetWindows()
    {
        CGDisplayCount displayCount=0;
        CGGetActiveDisplayList(0, 0, &displayCount);
        std::vector<CGDirectDisplayID> displays;
        displays.resize(displayCount);
        CGGetActiveDisplayList(displayCount, displays.data(), &displayCount);

        auto windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
        std::vector<Window> ret;
        CFIndex numWindows = CFArrayGetCount(windowList );

        for( int i = 0; i < (int)numWindows; i++ ) {
            Window w = {};
            uint32_t windowid=0;
            auto xscale=1.0f;
            auto yscale = 1.0f;
            auto dict = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowList, i));
            auto cfwindowname = static_cast<CFStringRef>(CFDictionaryGetValue(dict, kCGWindowName));
            CFStringGetCString(cfwindowname, w.Name, sizeof(w.Name), kCFStringEncodingUTF8);
            w.Name[sizeof(w.Name)-1] = '\n';


            CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(dict, kCGWindowNumber)), kCFNumberIntType, &windowid);
            w.Handle = static_cast<size_t>(windowid);

            auto dims =static_cast<CFDictionaryRef>(CFDictionaryGetValue(dict,kCGWindowBounds));
            CGRect rect;
            CGRectMakeWithDictionaryRepresentation(dims, &rect);

            // find the display this window belongs to in order to apply any scaling
            for(auto  i = 0; i < displayCount; i++) {
                //only include non-mirrored displays
                if(CGDisplayMirrorsDisplay(displays[i]) == kCGNullDirectDisplay){

                    auto dismode =CGDisplayCopyDisplayMode(displays[i]);
                    auto scaledsize = CGDisplayBounds(displays[i]);

                    auto pixelwidth = CGDisplayModeGetPixelWidth(dismode);
                    auto pixelheight = CGDisplayModeGetPixelHeight(dismode);

                    CGDisplayModeRelease(dismode);

                    if (CGRectContainsRect(scaledsize, rect)) {
                        if(scaledsize.size.width != pixelwidth){
                            xscale = static_cast<float>(pixelwidth)/static_cast<float>(scaledsize.size.width);
                        }
                        if(scaledsize.size.height != pixelheight){
                            yscale = static_cast<float>(pixelheight)/static_cast<float>(scaledsize.size.height);
                        }

                        break;
                    }
                }
            }

            w.Position.x = static_cast<int>(rect.origin.x);
            w.Position.y = static_cast<int>(rect.origin.y);


            w.Size.x = static_cast<int>(rect.size.width * xscale);
            w.Size.y = static_cast<int>(rect.size.height* yscale);

            w.Scaling = xscale; // only support a single scaling factor

            std::transform(std::begin(w.Name), std::end(w.Name), std::begin(w.Name), ::tolower);
            ret.push_back(w);
        }
        CFRelease(windowList);
        return ret;
    }
}
}
