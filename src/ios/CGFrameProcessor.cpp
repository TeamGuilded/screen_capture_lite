#include "CGFrameProcessor.h"
#include "TargetConditionals.h"
#include <ApplicationServices/ApplicationServices.h>
#include "NSMouseCapture.h"
#include <iostream>

namespace SL {
namespace Screen_Capture {


    DUPL_RETURN CGFrameProcessor::Init(std::shared_ptr<Thread_Data> data, Window &window)
    {
        auto ret = DUPL_RETURN::DUPL_RETURN_SUCCESS;
        Data = data;
        return ret;
    }
    DUPL_RETURN CGFrameProcessor::ProcessFrame(const Window &window)
    {
        auto mouseev = CGEventCreate(NULL);
        auto loc = CGEventGetLocation(mouseev);
        CFRelease(mouseev);

        auto mouse = SLScreen_Capture_GetCurrentMouseImage(window.Scaling);

        auto mouseWidth = CGImageGetWidth(mouse.Image);
        auto mouseHeight = CGImageGetHeight(mouse.Image);

        auto Ret = DUPL_RETURN_SUCCESS;

        auto winImageRef = CGWindowListCreateImage(
            CGRectNull,
            kCGWindowListOptionIncludingWindow,
            static_cast<uint32_t>(window.Handle),
            kCGWindowImageBoundsIgnoreFraming);

        if (!winImageRef)
            return DUPL_RETURN_ERROR_EXPECTED; // this happens when the monitors change.

        auto width = CGImageGetWidth(winImageRef);
        auto height = CGImageGetHeight(winImageRef);

        if (width != window.Size.x || height != window.Size.y) {
            CGImageRelease(winImageRef);
            return DUPL_RETURN_ERROR_EXPECTED; // this happens when the window sizes change.
        }

        auto mouse_x_adjusted = (loc.x * window.Scaling) - (window.Position.x * 2) - mouse.HotSpotx;
        auto mouse_y_adjusted = height - ((loc.y * window.Scaling) - (window.Position.y * 2) + (mouseHeight - mouse.HotSpoty));

        auto mouseBaseRect = CGRectMake(loc.x, loc.y, mouseWidth, mouseHeight);
        auto mouseAdjustedRect = CGRectMake(mouse_x_adjusted, mouse_y_adjusted, mouseWidth, mouseHeight);
        auto windowRect = CGRectMake(window.Position.x, window.Position.y, window.Size.x, window.Size.y);

        auto winImageRefBytesPerRow = CGImageGetBytesPerRow(winImageRef);

        unsigned int * imgData = (unsigned int*)malloc(height*winImageRefBytesPerRow);

        // have the graphics context now,
        CGRect bgBoundingBox = CGRectMake (0, 0, width,height);

        CGContextRef context =  CGBitmapContextCreate(imgData,
                                                  width,
                                                  height,
                                                  8, // 8 bits per component
                                                  winImageRefBytesPerRow,
                                                  CGImageGetColorSpace(winImageRef),
                                                  CGImageGetBitmapInfo(winImageRef));

        CGContextDrawImage(context,CGRectMake(0, 0, width,height),winImageRef);

        // skip drawing the cursor if no intersection
        if (CGRectIntersectsRect(windowRect, mouseBaseRect)) {
          CGContextDrawImage(context, mouseAdjustedRect, mouse.Image);
        }

        auto imageRef = CGBitmapContextCreateImage(context);

        CGContextRelease(context);

        auto prov = CGImageGetDataProvider(imageRef);
        if (!prov) {
            CGImageRelease(imageRef);
            return DUPL_RETURN_ERROR_EXPECTED;
        }
        auto bytesperrow = CGImageGetBytesPerRow(imageRef);
        auto bitsperpixel = CGImageGetBitsPerPixel(imageRef);
        // right now only support full 32 bit images.. Most desktops should run this as its the most efficent
        assert(bitsperpixel == sizeof(ImageBGRA) * 8);

        auto rawdatas = CGDataProviderCopyData(prov);
        auto buf = CFDataGetBytePtr(rawdatas);

        ProcessCapture(Data->WindowCaptureData, *this, window, buf, bytesperrow);

        CFRelease(rawdatas);
        CGImageRelease(imageRef);
        return Ret;
    }
}
}
