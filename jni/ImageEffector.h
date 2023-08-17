/******************************************************************
 *
 *Copyright (C) 2012  Amlogic, Inc.
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 ******************************************************************/

#ifndef _IMAGEEFFECTOR_H
#define _IMAGEEFFECTOR_H

#include <SkImage.h>
#include <SkRefCnt.h>
#include <SkBitmap.h>
#include <SkRect.h>
#include <SkRegion.h>
#include <SkCanvas.h>
#include <SkMatrix.h>
#include <SkColorSpace.h>
#include <utils/RefBase.h>

#include <android-base/properties.h>

#include <functional>

namespace android {
class SkiaGpuDevice;
class AsyncContext;

using UniqueAsyncResult = std::unique_ptr<const SkImage::AsyncReadResult>;
using OnRenderFinished = std::function<void(UniqueAsyncResult, int, int)>;

class ImageEffector : public RefBase {
public:
    const static int FIT_DEFAULT = -1;
    const static int FIT_ORIGINAL = -2;

    ImageEffector(int32_t width, int32_t height, bool gpuRender = false,
            OnRenderFinished callback = nullptr);

    virtual ~ImageEffector();

    const SkBitmap &bitmap() {
        return mScreenBitmap;
    }

    bool setImage(SkBitmap *bitmap, int fit = -1, bool show = true, bool movie = false);
    bool scale(float sX, float sY, bool show = true);
    bool rotate(float rotation, bool keepScale = false, bool show = true);
    bool translate(float x, float y, bool show = true);
    bool updateWindowSize(int width, int height);
    void reset();
    void render();
    bool isAsyncRender() { return mGpuRender && mRenderFinishedCallback != nullptr; }
private:
    class FrontImageInfo {
    public:
        std::unique_ptr<SkBitmap> image = nullptr;
        int32_t width;
        int32_t height;
        float rotation = -1;
        float scaleX = 1;
        float scaleY = 1;
        float translateX = 0;
        float translateY = 0;

        bool isValid() {
            return width > 0 && height > 0 && image != nullptr && !image->isNull();
        }

        void resetData() {
            if (image != nullptr && !image->isNull()) {
                image->reset();
                image = nullptr;
            }
            width = 0;
            height = 0;
        }

        void resetEffect() {
            rotation = -1;
            scaleX = 1;
            scaleY = 1;
            translateX = 0;
            translateY = 0;
        }

        void reset() {
            resetData();
            resetEffect();
        }
    };

    class GlobalOrientation {
    public:
        static const int ROT_0 = 0;
        static const int ROT_90 = 1;
        static const int ROT_180 = 2;
        static const int ROT_270 = 3;

        int getOrientation() {
            const std::string propStr = android::base::GetProperty(std::string(PROP_BUILTIN_ROTATION),
                                                                   "0");
            int rot = atoi(propStr.c_str());
            if (rot < ROT_0) rot = ROT_0;
            if (rot > ROT_270) rot = rot % 4;
            return rot;
        }

        bool isPortrait() {
            int rot = getOrientation();
            return rot == ROT_90 || rot == ROT_270;
        }

        bool isLandscape() {
            int rot = getOrientation();
            return rot == ROT_0 || rot == ROT_180;
        }

        bool isOrientationChanged(bool update = false) {
            int currentOrientation = getOrientation();
            bool changed = currentOrientation != mLastOrientation;
            if (changed && update) {
                mLastOrientation = currentOrientation;
            }

            return changed;
        }

        std::string toString() {
            std::string outStr;
            outStr += isPortrait() ? "Portrait" : "Landscape";
            outStr = outStr + "(" + std::to_string(getOrientation()) + ")";
            return outStr;
        }

    private:
        int mLastOrientation{0};
        const char* PROP_BUILTIN_ROTATION = "persist.sys.builtinrotation";
        // const char* PROP_HDMI_ROTATION = "persist.sys.hdmirotation";
    };

    virtual void onFirstRef() override;
    virtual void onLastStrongRef(const void *id) override;

    void init();
    void initScreenCanvas(int width, int height);
    void release();
    bool scaleUpdated(float sX, float sY);
    bool rotationUpdated(float rotation);
    bool translateUpdated(float x, float y);
    void clearDirtyRegion(SkCanvas *canvas, const SkRect &currentRect);
    void traverseRegion(const SkRegion &region, std::function<void(const SkIRect &rect)> f);
    void readBackPixels(bool requestSync = false);
    inline static SkMatrix::ScaleToFit iToFit(int iFit);

private:
    GlobalOrientation mGlobalOrientation;

    FrontImageInfo mFrontImageInfo;
    int32_t mFit;
    bool mIsLastTypeMovie{false};
    bool mGpuRender{false};

    sp<SkiaGpuDevice> mGpuDevice;
    SkPaint mAntiPaint;

    SkBitmap mScreenBitmap;
    SkRect mScreenRect;
    SkRegion mLastDirtyRegion;

    SkCanvas* mTargetCanvas{nullptr};

    OnRenderFinished mRenderFinishedCallback{nullptr};
};

class AsyncContext {
public:
    std::unique_ptr<const SkImage::AsyncReadResult> fResult;
    bool fCalled;
};
}


#endif //_IMAGEEFFECTOR_H
