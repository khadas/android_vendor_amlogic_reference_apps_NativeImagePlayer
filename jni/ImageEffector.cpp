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
#define LOG_TAG "ImageEffector"

#include "ImageEffector.h"
#include <utils/Log.h>

#define FUNC_E() ALOGD("%s EEE", __FUNCTION__ )
#define FUNC_X() ALOGD("%s XXX", __FUNCTION__ )

namespace android {

ImageEffector::ImageEffector(int32_t width, int32_t height) {
    FUNC_E();
    mScreenWidth = width;
    mScreenHeight = height;

    ALOGD("Surface size: [%d, %d]", mScreenWidth, mScreenHeight);

    mScreenRect.setWH(width, height);
}

ImageEffector::~ImageEffector() {
    FUNC_E();
}

void ImageEffector::onFirstRef() {
    FUNC_E();
    init();
    FUNC_X();
}

void ImageEffector::onLastStrongRef(const void *id) {
    release();
}

bool ImageEffector::setImage(SkBitmap *bitmap, int fit, bool show, bool movie) {
    if (bitmap == nullptr || bitmap->isNull()) {
        ALOGE("Invalid bitmap ref");
        return false;
    }

    ALOGD("setImage, movie= %d, mFrontImageInfo.isValid(): %d", movie, mFrontImageInfo.isValid());

    if (mFrontImageInfo.isValid()) {
        if (movie && (mIsLastTypeMovie == movie)) {
            // Reset data only for movie
            ALOGD("Reset data only for movie");
            mFrontImageInfo.resetData();
        } else {
            ALOGD("Reset all for image changed");
            mFrontImageInfo.reset();
        }
    }

    mIsLastTypeMovie = movie;
    mFit = fit;
    mFrontImageInfo.image = std::make_shared<SkBitmap>(*bitmap);
    mFrontImageInfo.width = bitmap->width();
    mFrontImageInfo.height = bitmap->height();

    ALOGD("setImage, show: %d, fit= %d", show, fit);

    if (show) {
        render();
    }

    return true;
}

bool ImageEffector::scaleUpdated(float sX, float sY) {
    if (sX == mFrontImageInfo.scaleX && sY == mFrontImageInfo.scaleY) {
        return false;
    }

    mFrontImageInfo.scaleX = sX;
    mFrontImageInfo.scaleY = sY;
    return true;
}

bool ImageEffector::rotationUpdated(float rotation) {
    rotation = (float) ((int) rotation % 360);
    if (mFrontImageInfo.rotation == rotation) {
        return false;
    }
    mFrontImageInfo.rotation = rotation;
    return true;
}

bool ImageEffector::scale(float sX, float sY, bool show) {
    if (!mFrontImageInfo.isValid()) {
        ALOGE("scale, mFrontImageInfo invalid");
        return false;
    }

    if (!scaleUpdated(sX, sY)) {
        ALOGE("scale, scaleUpdated failed");
        return false;
    }

    ALOGD("scale, sX: %f, sY: %f, show: %d", mFrontImageInfo.scaleX, mFrontImageInfo.scaleY, show);

    if (show) {
        render();
    }
    return true;
}

bool ImageEffector::translate(float x, float y, bool show) {
    if (!mFrontImageInfo.isValid())
        return false;

    if (!translateUpdated(x, y))
        return false;

    ALOGD("translate: [%f, %f], show: %d", mFrontImageInfo.translateX, mFrontImageInfo.translateY, show);

    if (show) {
        render();
    }
    return true;
}

bool ImageEffector::translateUpdated(float x, float y) {
    if (x == mFrontImageInfo.translateX && y == mFrontImageInfo.translateY) {
        return false;
    }

    mFrontImageInfo.translateX = x;
    mFrontImageInfo.translateY = y;
    return true;
}

bool ImageEffector::rotate(float rotation, bool keepScale, bool show) {
    if (!mFrontImageInfo.isValid()) {
        ALOGE("rotate, mFrontImageInfo invalid");
        return false;
    }

    if (!rotationUpdated(rotation)) {
        ALOGE("rotate, rotationUpdated failed");
        return false;
    }

    if (!keepScale) {
        mFrontImageInfo.scaleX = 1;
        mFrontImageInfo.scaleY = 1;
        mFrontImageInfo.translateX = 0;
        mFrontImageInfo.translateY = 0;
    }

    ALOGD("rotate, %f, show: %d", mFrontImageInfo.rotation, show);

    if (show) {
        render();
    }
    return true;
}

void ImageEffector::render() {
    SkCanvas canvas(mScreenBitmap);
    SkRect imageRect{0, 0,
                     (float) mFrontImageInfo.width, (float) mFrontImageInfo.height};
    SkRect screenRect = mScreenRect;

    SkMatrix finalMatrix;
    if (mFit != -1 || !screenRect.contains(imageRect)) {
        finalMatrix.setRectToRect(imageRect, screenRect,iToFit(mFit));
    } else {
        finalMatrix.postTranslate((screenRect.width() - imageRect.width()) / 2,
                                  (screenRect.height() - imageRect.height()) / 2);
    }

    SkScalar rotation = mFrontImageInfo.rotation;
    SkScalar scaleX = mFrontImageInfo.scaleX;
    SkScalar scaleY = mFrontImageInfo.scaleY;

    if (rotation != -1) {
        SkRect mappedRect = finalMatrix.mapRect(imageRect);
        finalMatrix.postRotate(rotation, (float) mappedRect.centerX(),
                               (float) mappedRect.centerY());
        mappedRect = finalMatrix.mapRect(imageRect);

        if (mFit != -1 || (scaleX == 1 && !screenRect.contains(mappedRect))) {
            SkMatrix m;
            m.setRectToRect(mappedRect, screenRect, iToFit(mFit));
            finalMatrix.postConcat(m);
        }
    }

    if (scaleX != 0 && scaleY != 0) {
        finalMatrix.postScale(scaleX, scaleY,
                              (float) screenRect.width() / 2, (float) screenRect.height() / 2);
    }

    finalMatrix.postTranslate(mFrontImageInfo.translateX, mFrontImageInfo.translateY);

    //canvas.drawColor(SkColorSetARGB(255, 0, 0, 0));

    SkRect currentRect = finalMatrix.mapRect(imageRect);
    if (!currentRect.isEmpty()) {
        clearDirtyRegion(canvas, currentRect);

        canvas.save();
        canvas.clipRect(currentRect);
        canvas.setMatrix(finalMatrix);

        canvas.drawImage(mFrontImageInfo.image->asImage(), 0, 0);

        canvas.restore();
    }
}

void ImageEffector::traverseRegion(const SkRegion &region,
                                   const std::function<void(const SkIRect &rect)> f) {
    SkRegion::Iterator it(region);
    while (!it.done()) {
        f(it.rect());
        it.next();
    }
}

void ImageEffector::clearDirtyRegion(SkCanvas &canvas, const SkRect &currentRect) {
    SkRegion currentDirty{SkIRect::MakeLTRB(round(currentRect.left()),
                                            round(currentRect.top()),
                                            round(currentRect.right()),
                                            round(currentRect.bottom()))};
    if (!mLastDirtyRegion.isEmpty()) {
        SkRegion clearRegion{SkIRect::MakeWH(mScreenWidth, mScreenHeight)};
        if (clearRegion.op(mLastDirtyRegion, SkRegion::Op::kIntersect_Op)) {

            if (clearRegion.op(currentDirty, SkRegion::Op::kDifference_Op)) {
                traverseRegion(clearRegion, [&](const SkIRect &rect) {
                    canvas.save();
                    canvas.clipRect(SkRect::MakeLTRB(rect.left(), rect.top(), rect.right(),
                                                     rect.bottom()));
                    canvas.drawColor(SkColorSetARGB(255, 0, 0, 0));
                    canvas.restore();
                });
            }
        }
    }

    mLastDirtyRegion.set(currentDirty);
}

void ImageEffector::init() {
    if (!mScreenBitmap.isNull()) {
        mScreenBitmap.reset();
    }

    SkImageInfo k32Info = SkImageInfo::Make(mScreenWidth, mScreenHeight,
                                            SkColorType::kRGBA_8888_SkColorType,
                                            SkAlphaType::kPremul_SkAlphaType,
                                            SkColorSpace::MakeSRGB());
    mScreenBitmap.allocPixels(k32Info);
    mScreenBitmap.eraseARGB(255, 0, 0, 0);
    mLastDirtyRegion.setRect(SkIRect::MakeWH(mScreenWidth, mScreenHeight));
}

void ImageEffector::release() {
    if (mFrontImageInfo.isValid()) {
        mFrontImageInfo.reset();
    }

    if (!mScreenBitmap.isNull()) {
        mScreenBitmap.reset();
    }
}

void ImageEffector::reset() {
    if (mFrontImageInfo.isValid()) {
        mFrontImageInfo.translateX = mFrontImageInfo.translateY = 0;
        mFrontImageInfo.rotation = -1;
        mFrontImageInfo.scaleX = 1;
        mFrontImageInfo.scaleY = 1;

        render();
    }
}

SkMatrix::ScaleToFit ImageEffector::iToFit(int iFit) {
    return static_cast<SkMatrix::ScaleToFit>(iFit);
}

}
