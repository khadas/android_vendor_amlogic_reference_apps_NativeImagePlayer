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
#include <utils/RefBase.h>

#include <functional>

namespace android {
class ImageEffector : public RefBase {
public:
    ImageEffector(int32_t width, int32_t height);

    virtual ~ImageEffector();

    const SkBitmap &bitmap() {
        return mScreenBitmap;
    }

    bool setImage(SkBitmap *bitmap, int fit = -1, bool show = true, bool movie = false);
    bool scale(float sX, float sY, bool show = true);
    bool rotate(float rotation, bool keepScale = false, bool show = true);
    bool translate(float x, float y, bool show = true);
    void reset();
    void render();

private:
    class FrontImageInfo {
    public:
        std::shared_ptr<SkBitmap> image = nullptr;
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

    virtual void onFirstRef() override;
    virtual void onLastStrongRef(const void *id) override;

    void init();
    void release();
    bool scaleUpdated(float sX, float sY);
    bool rotationUpdated(float rotation);
    bool translateUpdated(float x, float y);
    void clearDirtyRegion(SkCanvas &canvas, const SkRect &currentRect);
    void traverseRegion(const SkRegion &region, std::function<void(const SkIRect &rect)> f);
    inline static SkMatrix::ScaleToFit iToFit(int iFit);

private:
    FrontImageInfo mFrontImageInfo;
    int32_t mScreenWidth;
    int32_t mScreenHeight;
    int32_t mFit;
    bool mIsLastTypeMovie{false};

    SkBitmap mScreenBitmap;
    SkRect mScreenRect;
    SkRegion mLastDirtyRegion;
};
}


#endif //_IMAGEEFFECTOR_H
