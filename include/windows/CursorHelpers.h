#pragma once

class CursorHelpers {
  public:
    static void DrawCursor(HDC aHDC, int screenOffsetX, int screenOffsetY)
    {
        CURSORINFO cursorInfo = {0};
        cursorInfo.cbSize = sizeof(CURSORINFO);

        if (GetCursorInfo(&cursorInfo) == TRUE) {
            if (cursorInfo.flags == CURSOR_SHOWING) {

                HICON hicon = CopyIcon(cursorInfo.hCursor);

                if (hicon != NULL) {

                    ICONINFO iconInfo = {0};

                    if (GetIconInfo(hicon, &iconInfo)) {
                        const int x = cursorInfo.ptScreenPos.x - iconInfo.xHotspot - screenOffsetX;
                        const int y = cursorInfo.ptScreenPos.y - iconInfo.yHotspot - screenOffsetY;

                        BITMAP maskBitmap = {0};
                        GetObject(iconInfo.hbmMask, sizeof(maskBitmap), &maskBitmap);

                        // normal "monochrome" icons e.g. i-beam appear to work fine with DrawIconEx
                        // icons with a color mask do not seem to work correctly and we need to manually bitblt
                        // each bitmap mask in correctly (AND the mask, and XOR the color)
                        if (maskBitmap.bmHeight == maskBitmap.bmWidth * 2 && iconInfo.hbmColor == NULL) {
                            DrawIconEx(aHDC, x, y, hicon, maskBitmap.bmWidth, maskBitmap.bmWidth, 0, NULL, DI_NORMAL);
                        }
                        else {
                            BITMAP colorBitmap = {0};
                            GetObject(iconInfo.hbmColor, sizeof(colorBitmap), &colorBitmap);

                            BLENDFUNCTION bf;
                            bf.SourceConstantAlpha = 255;
                            bf.BlendOp = AC_SRC_OVER;
                            bf.BlendFlags = 0;
                            bf.AlphaFormat = AC_SRC_ALPHA;

                            HDC hDC = GetDC(NULL);
                            HDC hMemDC = CreateCompatibleDC(hDC);

                            SelectObject(hMemDC, iconInfo.hbmMask);
                            BitBlt(aHDC, x, y, maskBitmap.bmHeight, maskBitmap.bmWidth, hMemDC, 0, 0, SRCAND);

                            SelectObject(hMemDC, iconInfo.hbmColor);
                            BitBlt(aHDC, x, y, colorBitmap.bmHeight, colorBitmap.bmWidth, hMemDC, 0, 0, SRCINVERT);

                            DeleteDC(hMemDC);
                            ReleaseDC(NULL, hDC);
                        }

                        DeleteObject(iconInfo.hbmColor);
                        DeleteObject(iconInfo.hbmMask);
                    }

                    DestroyIcon(hicon);
                }
            }
        }
    }
};