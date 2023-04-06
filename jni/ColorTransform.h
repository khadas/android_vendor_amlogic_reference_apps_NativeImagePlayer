//
// Created by jiajia.tian on 2023/4/4.
//

#ifndef _COLORTRANSFORM_H
#define _COLORTRANSFORM_H

#include <effects/SkColorMatrix.h>
#include <utils/RefBase.h>

using RefBase = android::RefBase;

class ColorTransform : public RefBase {
public:
    ColorTransform(SkYUVColorSpace colorSpace, bool rgbToYuv = true, bool needOffset = true)
    :mNeedOffset(needOffset) {
        SkColorMatrix colorMatrix = rgbToYuv ? SkColorMatrix::RGBtoYUV(colorSpace) :
                SkColorMatrix::YUVtoRGB(colorSpace);

        mIsLimited = isLimitedColorSpace(colorSpace);

        mMajor.reserve(20);
        colorMatrix.getRowMajor(mMajor.data());
    };
    virtual ~ColorTransform() {
        mMajor.clear();
    }

public:
    inline float y(float r, float g, float b) {
        return (mMajor[0] * r + mMajor[1] * g + mMajor[2] * b) + (mNeedOffset ? mMajor[4] : 0) + (mIsLimited ? 16 : 0);
    }

    inline float u(float r, float g, float b) {
        return (mMajor[5] * r + mMajor[6] * g + mMajor[7] * b)  + (mNeedOffset ? mMajor[9] : 0) + 128;
    }

    inline float v(float r, float g, float b) {
        return (mMajor[10] * r + mMajor[11] *g + mMajor[12] * b)  + (mNeedOffset ? mMajor[14] : 0) + 128;
    }

    inline float r(float y, float u, float v) {
        float _y = mIsLimited ? (y - 16) : y;
        float _u = u - 128;
        float _v = v - 128;
        return (mMajor[0] * _y + mMajor[1] * _u + mMajor[2] * _v) + (mNeedOffset ? mMajor[4] : 0);
    }

    inline float g(float y, float u, float v) {
        float _y = mIsLimited ? (y - 16) : y;
        float _u = u - 128;
        float _v = v - 128;
        return (mMajor[5] * _y + mMajor[6] * _u + mMajor[7] * _v) + (mNeedOffset ? mMajor[9] : 0);
    }

    inline float b(float y, float u, float v) {
        float _y = mIsLimited ? (y - 16) : y;
        float _u = u - 128;
        float _v = v - 128;
        return (mMajor[10] * _y + mMajor[11] * _u + mMajor[12] * _v) + (mNeedOffset ? mMajor[14] : 0);
    }

private:
    static bool isLimitedColorSpace(SkYUVColorSpace colorSpace) {
        switch (colorSpace) {
            case kRec601_Limited_SkYUVColorSpace:
            case kRec709_Limited_SkYUVColorSpace:
            case kBT2020_8bit_Limited_SkYUVColorSpace:
            case kBT2020_10bit_Limited_SkYUVColorSpace:
            case kBT2020_12bit_Limited_SkYUVColorSpace:
                return true;
            case kRec709_Full_SkYUVColorSpace:
            case kBT2020_8bit_Full_SkYUVColorSpace:
            case kBT2020_10bit_Full_SkYUVColorSpace:
            case kBT2020_12bit_Full_SkYUVColorSpace:
            case kJPEG_Full_SkYUVColorSpace:
                return false;
            default:
                return false;
        }
    }

private:
    std::vector<float> mMajor;
    bool mNeedOffset{true};
    bool mIsLimited{true};
};

#endif //_COLORTRANSFORM_H
