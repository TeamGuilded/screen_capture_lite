//
//  NSMouseCapture.m
//  Screen_Capture
//
//  Created by scott lee on 1/11/17.
//
//

#import <Foundation/Foundation.h>
#import <appkit/appkit.h>
#import "NSMouseCapture.h"

CGImageRef CreateScaledCGImage(CGImageRef image, int width, int height) {
  // Create context, keeping original image properties.
  CGContextRef context = CGBitmapContextCreate(nil,
                                               width,
                                               height,
                                               CGImageGetBitsPerComponent(image),
                                               CGImageGetBytesPerRow(image),
                                               CGImageGetColorSpace(image),
                                               CGImageGetBitmapInfo(image));
  if (!context) return nil;
  // Draw image to context, resizing it.
  CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
  // Extract resulting image from context.
  CGImageRef imgRef = CGBitmapContextCreateImage(context);
  CGContextRelease(context);
  return imgRef;
}

void SLScreen_Capture_InitMouseCapture(){
    [NSApplication sharedApplication];
}
struct SL_MouseCur SLScreen_Capture_GetCurrentMouseImage(float scalingFactor){
    struct SL_MouseCur ret= {};

    @autoreleasepool {
        NSCursor *cur = [NSCursor currentSystemCursor];
        if(cur==nil) return ret;
        NSImage *overlay =  [cur image];
        NSSize nssize    = [overlay size];  // DIP size

        int scaledWidth = round(nssize.width * scalingFactor);
        int scaledHeight = round(nssize.height * scalingFactor);

        CGImageSourceRef source = CGImageSourceCreateWithData((CFDataRef)[overlay TIFFRepresentation], NULL);
        ret.Image = CGImageSourceCreateImageAtIndex(source, 0, NULL);

        CGImageRef scaledCursorImage = nil;
        if (CGImageGetWidth(ret.Image) != scaledWidth) {
            scaledCursorImage = CreateScaledCGImage(ret.Image, scaledWidth, scaledHeight);
            if (scaledCursorImage != nil) {
              ret.Image = scaledCursorImage;
            }
        }

        NSPoint p = [cur hotSpot];
        ret.HotSpotx = MAX(0, MIN(scaledWidth, (int) p.x * scalingFactor));
        ret.HotSpoty = MAX(0, MIN(scaledHeight, (int) p.y * scalingFactor));
        CFRelease(source);
    }

    return ret;
}
