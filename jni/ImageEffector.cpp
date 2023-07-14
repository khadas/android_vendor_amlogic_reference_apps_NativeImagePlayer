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
#include <SkPixelRef.h>
#include <SkRefCnt.h>

#include "SkiaGpuDevice.h"

#include <time.h>

#define FUNC_E() ALOGD("%s EEE", __FUNCTION__ )
#define FUNC_X() ALOGD("%s XXX", __FUNCTION__ )

#define GRALLOC_ALIGN(value, base) ((((value) + (base) -1) / (base)) * (base))

namespace android {
ImageEffector::ImageEffector(int32_t width, int32_t height, bool gpuRender, OnRenderFinished callback) {
    FUNC_E();
    mScreenRect.setWH(width, height);
    mGpuRender = gpuRender;
    mRenderFinishedCallback = std::move(callback);
    ALOGD("Surface size: [%f, %f], gpuRender: %d", mScreenRect.width(), mScreenRect.height(), mGpuRender);
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

    auto uniqueBmp = std::make_unique<SkBitmap>();
    uniqueBmp->setInfo(bitmap->info(), bitmap->rowBytes());
    uniqueBmp->setPixelRef(sk_ref_sp(bitmap->pixelRef()), 0, 0);
    mFrontImageInfo.image = std::move(uniqueBmp);

    mFrontImageInfo.width = bitmap->width();
    mFrontImageInfo.height = bitmap->height();

    ALOGD("setImage, show: %d, fit= %d", show, fit);

    if (mGlobalOrientation.isOrientationChanged(true)) {
        ALOGI("setImage, currentOrientation: %s,, isOrientationChanged: %d",
                mGlobalOrientation.toString().c_str(), mGlobalOrientation.isOrientationChanged());
        initScreenCanvas(mScreenRect.width(), mScreenRect.height());
    }

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

bool ImageEffector::updateWindowSize(int width, int height) {
    if (SkRect::MakeWH(width, height) == mScreenRect) {
        // No need update
        return false;
    }

    width = GRALLOC_ALIGN(width, 32);
    int w = mScreenRect.width();
    int h = mScreenRect.height();
    ALOGD("updateWindowSize, width= %d, height= %d, mScreenWidth= %d, mScreenHeight= %d",
          width, height, w, h);

    initScreenCanvas(width, height);
    return true;
}

void ImageEffector::render() {
    SkRect imageRect{0, 0,
                     (float) mFrontImageInfo.width, (float) mFrontImageInfo.height};
    SkRect screenRect = mScreenRect;

    SkMatrix finalMatrix;

    SkMatrix::ScaleToFit requestFit = iToFit(mFit);
    bool isDefaultFit = static_cast<int>(requestFit) == FIT_DEFAULT;
    bool isOriginalFit = static_cast<int>(requestFit) == FIT_ORIGINAL;

    if (isDefaultFit) {
        // Default scale to fit center if image is bigger than display frame.
        if (!screenRect.contains(imageRect)) {
            finalMatrix.setRectToRect(imageRect, screenRect, SkMatrix::ScaleToFit::kCenter_ScaleToFit);
        } else {
            finalMatrix.postTranslate((screenRect.width() - imageRect.width()) / 2,
                          (screenRect.height() - imageRect.height()) / 2);
        }
    } else if (isOriginalFit) {
        // Show original image base on left-top
        finalMatrix.postTranslate(0, 0);
    } else {
        finalMatrix.setRectToRect(imageRect, screenRect, requestFit);
    }

    SkScalar rotation = mFrontImageInfo.rotation;
    SkScalar scaleX = mFrontImageInfo.scaleX;
    SkScalar scaleY = mFrontImageInfo.scaleY;

    if (rotation != -1) {
        SkRect mappedRect = finalMatrix.mapRect(imageRect);
        finalMatrix.postRotate(rotation, (float) mappedRect.centerX(),
                               (float) mappedRect.centerY());

        if (!isOriginalFit) {
            mappedRect = finalMatrix.mapRect(imageRect);

            if (isDefaultFit) {
                // Get original rect for image with out scale
                SkMatrix originForImage;
                SkScalar offsetX = (screenRect.width() - imageRect.width()) / 2.f;
                SkScalar offsetY = (screenRect.height() - imageRect.height()) / 2.f;
                originForImage.postTranslate(offsetX, offsetY);
                originForImage.postRotate(rotation, screenRect.centerX(), screenRect.centerY());
                SkRect originRotRect = originForImage.mapRect(imageRect);

                SkMatrix m;
                if (screenRect.contains(originRotRect)) {
                    m.setRectToRect(mappedRect, originRotRect, SkMatrix::ScaleToFit::kFill_ScaleToFit);
                } else {
                    m.setRectToRect(mappedRect, screenRect, SkMatrix::ScaleToFit::kCenter_ScaleToFit);
                }
                finalMatrix.postConcat(m);
            } else {
                SkMatrix m;
                m.setRectToRect(mappedRect, screenRect, requestFit);
                finalMatrix.postConcat(m);
            }
        }
    }

    if (scaleX != 0 && scaleY != 0) {
        finalMatrix.postScale(scaleX, scaleY,
                              (float) screenRect.width() / 2, (float) screenRect.height() / 2);
    }

    finalMatrix.postTranslate(mFrontImageInfo.translateX, mFrontImageInfo.translateY);

    //canvas.drawColor(SkColorSetARGB(255, 100, 100, 100));

    SkRect currentRect = finalMatrix.mapRect(imageRect);
    if (!currentRect.isEmpty() && mFrontImageInfo.isValid()
        && mFrontImageInfo.image->readyToDraw()) {

        SkCanvas* canvas = mGpuRender ?
                (mGpuDevice->getSurface()->getCanvas()) : (new SkCanvas(mScreenBitmap));

        if (((int)rotation % 90) == 0) {
            clearDirtyRegion(canvas, currentRect);
        } else {
            // Clear all if not integer multiples of 90,
            // Can not calculate the dirty region
            canvas->drawColor(SkColorSetARGB(255, 0, 0, 0));
        }

        //canvas->drawColor(SkColorSetARGB(255, 0, 0, 0));

        canvas->save();
        canvas->clipRect(currentRect);
        canvas->setMatrix(finalMatrix);

        bool filter = finalMatrix.getScaleX() != SK_Scalar1 || finalMatrix.getScaleY() != SK_Scalar1;
        // Do not filter for movie(eg: git), or frame will jank.
        filter &= !mIsLastTypeMovie;

        ALOGD("render, scaleX= %f, scaleY= %f, filter= %d", finalMatrix.getScaleX(), finalMatrix.getScaleY(), filter);

        canvas->drawImage(mFrontImageInfo.image->asImage(), 0, 0,
                SkSamplingOptions(filter ? SkFilterMode::kLinear : SkFilterMode::kNearest),
                &mAntiPaint);

        canvas->restore();

        if (mGpuRender) {
            readBackPixels(mRenderFinishedCallback != nullptr);
        } else {
            // release for gpu render
            delete canvas;
        }
    }
}

static void async_callback(void* c, std::unique_ptr<const SkImage::AsyncReadResult> result) {
    AsyncContext* asyncContext =  static_cast<AsyncContext*>(c);
    asyncContext->fResult = std::move(result);
    asyncContext->fCalled = true;
}

void ImageEffector::readBackPixels(bool requestSync) {
    if (!mGpuDevice) return;

    mGpuDevice->flush();

    auto readSync = [&](){
        if (!mGpuDevice->getSurface()->readPixels(mScreenBitmap, 0, 0)) {
            ALOGE("render failed for Gpu render");
            return;
        }
        mScreenBitmap.notifyPixelsChanged();
    };

//    auto saveYuv = [](const char *buf, const char* url,int size) {
//        int fd = -1;
//        int write_num = 0;
//
//        fd = open(url,O_RDWR | O_CREAT,0660);
//        if (fd < 0) {
//            ALOGE("write file:%s open error\n",url);
//            return;
//        }
//
//        write_num = write(fd, buf, size);
//        if (write_num <= 0) {
//            ALOGE("write file write_num=%d error\n",write_num);
//        }
//        close(fd);
//    };

    auto readAsync = [&](){
        AsyncContext ac;
        mGpuDevice->getSurface()->asyncRescaleAndReadPixelsYUV420(
                SkYUVColorSpace::kRec709_Limited_SkYUVColorSpace,
                SkColorSpace::MakeSRGB(),mScreenBitmap.info().bounds(),
                mScreenBitmap.info().dimensions(),SkImage::RescaleGamma::kSrc,
                SkImage::RescaleMode::kNearest, async_callback, &ac);

        mGpuDevice->submit();
        while (!ac.fCalled) {
            mGpuDevice->checkAsyncCompletion();
        }

        if (!ac.fResult) {
            ALOGE("asyncRescaleAndReadPixelsYUV420 failed, read sync");
            readSync();
            return;
        }

        if (mRenderFinishedCallback != nullptr) {
            mRenderFinishedCallback(std::move(ac.fResult),
                    mScreenBitmap.info().width(), mScreenBitmap.info().height());
        }
//
//        auto &result = ac.fResult;
//        int width = mScreenBitmap.info().width();
//        int height = mScreenBitmap.info().height();
//        ALOGD("Save YUV data, count: %d", result->count());
//        std::vector<uint8_t> yuvMem(width * height * 3 / 2);
//        uint8_t *dataPtr = yuvMem.data();
//        for (int i = 0; i < result->count(); ++i) {
//            ALOGD("data(%d), rowBytes= %d", i, result->rowBytes(i));
//        }
//
//        int yLen = height * (int)result->rowBytes(0);
//        memcpy(dataPtr, result->data(0), yLen);
//        dataPtr += yLen;
//        int uvLen = height * (int)result->rowBytes(1) / 2;
//        memcpy(dataPtr, result->data(1), uvLen);
//        dataPtr += uvLen;
//        memcpy(dataPtr, result->data(2), uvLen);
//
//        saveYuv((const char *)yuvMem.data(), "/sdcard/Pictures/test.yuv", yuvMem.size());
    };

    if (requestSync) {
        readAsync();
    } else {
        readSync();
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

void ImageEffector::clearDirtyRegion(SkCanvas *canvas, const SkRect &currentRect) {
    SkRegion currentDirty{SkIRect::MakeLTRB(round(currentRect.left()),
                                            round(currentRect.top()),
                                            round(currentRect.right()),
                                            round(currentRect.bottom()))};
    if (!mLastDirtyRegion.isEmpty()) {
        SkRegion clearRegion{SkIRect::MakeWH(mScreenRect.width(), mScreenRect.height())};
        if (clearRegion.op(mLastDirtyRegion, SkRegion::Op::kIntersect_Op)) {

            if (clearRegion.op(currentDirty, SkRegion::Op::kDifference_Op)) {
                traverseRegion(clearRegion, [&](const SkIRect &rect) {
                    canvas->save();
                    canvas->clipRect(SkRect::MakeLTRB(rect.left(), rect.top(),rect.right(),
                                                     rect.bottom()).makeOutset(1, 1));
                    canvas->drawColor(SkColorSetARGB(255, 0, 0, 0));
                    canvas->restore();
                });
            }
        }
    } else {
        ALOGI("Empty dirty, no need clear");
    }

    mLastDirtyRegion.set(currentDirty);
}

void ImageEffector::init() {
    ALOGD("init, mGlobalOrientation changed: %s", mGlobalOrientation.toString().c_str());
    bool isLand = mGlobalOrientation.isLandscape();
    int width = mScreenRect.width();
    int height = mScreenRect.height();
    if ((!isLand && (width > height)) || (isLand && (width < height))) {
        std::swap(width, height);
    }

    initScreenCanvas(width, height);

    mAntiPaint.setAntiAlias(true);
    mAntiPaint.setDither(true);
    mAntiPaint.setBlendMode(SkBlendMode::kSrc);

    // Init orientation
    mGlobalOrientation.isOrientationChanged(true);
}

void ImageEffector::initScreenCanvas(int width, int height) {
    if (!mScreenBitmap.isNull()) {
        mScreenBitmap.reset();
        mScreenBitmap.setPixelRef(nullptr, 0, 0);
        mScreenBitmap.setPixels(nullptr);
    }

    mScreenRect.setWH(width, height);

    ALOGD("initScreenCanvas, mScreenWidth= %f, mScreenHeight= %f", mScreenRect.width(), mScreenRect.height());
    SkImageInfo k32Info = SkImageInfo::Make(width, height,
                                            SkColorType::kN32_SkColorType,
                                            SkAlphaType::kPremul_SkAlphaType);
    mScreenBitmap.setInfo(k32Info);
    if (!isAsyncRender()) {
        // Pixel will not copy into this when async render
        mScreenBitmap.allocPixels();
        mScreenBitmap.eraseARGB(255, 0, 0, 0);
    }
    mLastDirtyRegion.setRect(SkIRect::MakeWH(width, height));

    if (mGpuRender) {
        ALOGD("Using Gpu rendering");
        if (mGpuDevice == nullptr) {
            mGpuDevice = new SkiaGpuDevice(mScreenBitmap.width(), mScreenBitmap.height());
        }

        if (!mGpuDevice->initWithTarget(mScreenBitmap)) {
            ALOGE("Init GpuDevice failed, return to using cpu");
            mGpuRender = false;
            mGpuDevice.clear();
            mGpuDevice = nullptr;
        } else {
            ALOGD("SkiaGpuDevice init OK!");
        }
    }
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
    if (iFit == FIT_DEFAULT || iFit == FIT_ORIGINAL || (iFit >= 0 && iFit <= 3)) {
        return static_cast<SkMatrix::ScaleToFit>(iFit);
    }

    return SkMatrix::ScaleToFit::kCenter_ScaleToFit;
}

}
