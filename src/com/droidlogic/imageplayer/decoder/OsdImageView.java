package com.droidlogic.imageplayer.decoder;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ImageDecoder;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.drawable.AnimatedImageDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.util.Log;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import java.io.File;
import java.io.IOException;

public class OsdImageView extends ImageView {
    private static final String TAG = "OsdImageView";

    public static final int FIT_DEFAULT = -1;
    public static final int FIT_ORIGINAL = -2;

    private static final int IMAGE_TYPE_STATIC = 0;
    private static final int IMAGE_TYPE_GIF = 1;
    private int mImageType = IMAGE_TYPE_STATIC;

    private final RectF mImageRect = new RectF();
    private final RectF mScreenRect = new RectF();
    private final RectF mTmpRect = new RectF();
    private Matrix mMatrix;
    private Handler mHandler;
    private int mScaleType = ScaleType.FIT_CENTER.ordinal();

    private final Handler mUIHandler = new Handler(Looper.getMainLooper());
    private Drawable mDrawable;
    private boolean mOsdMarked = false;

    class EffectParams {
        float scaleX = 1;
        float scaleY = 1;
        float rotDegree = 0;
        float transX = 0;
        float transY = 0;

        void reset() {
            scaleX = 1;
            scaleY = 1;
            rotDegree = 0;
            transX = 0;
            transY = 0;
        }
    }

    private final EffectParams mEP = new EffectParams();

    public OsdImageView(Context context) {
        this(context, null);
    }

    public OsdImageView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public OsdImageView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        this(context, attrs, defStyleAttr, 0);
    }

    public OsdImageView(Context context, @Nullable AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);

        init();
    }

    private void init() {
        mHandler = new Handler();
        mMatrix = new Matrix();

        setBackgroundColor(Color.BLACK);
        super.setScaleType(ScaleType.MATRIX);
        setImageMatrix(new Matrix());
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        Log.d(TAG, "fitMatrix: bounds: " + getMeasuredWidth() + "x" + getMeasuredHeight());
        mScreenRect.set(0, 0, getMeasuredWidth(), getMeasuredHeight());
    }

    @Override
    public void setScaleType(ScaleType scaleType) {
        throw new UnsupportedOperationException("ImageView#setScaleType is unsupported");
    }

    public void setFit(Matrix.ScaleToFit scaleType) {
        mScaleType = scaleType.ordinal();
    }

    public void setExtendsScaleType(int extScaleType) {
        mScaleType = extScaleType;
    }

    public boolean isAnimated() {
        return mImageType == IMAGE_TYPE_GIF;
    }

    public void scale(float x, float y) {
        if (mEP.scaleX == x && mEP.scaleY == y)
            return;

        mEP.scaleX = x;
        mEP.scaleY = y;

        refresh();
    }

    public void translate(float x, float y) {
        if (mEP.transX == x && mEP.transY == y)
            return;

        mEP.transX = x;
        mEP.transY = y;
        refresh();
    }


    public void rotate(float degree) {
        if (mEP.rotDegree == degree)
            return;

        mEP.rotDegree = degree;
        refresh();
    }

    public void restore() {
        mEP.reset();
        refresh();
    }

    public boolean setImagePath(String path) {
        if (path == null || path.isEmpty()) {
            Log.e(TAG, "setImagePath: No valid path");
            return false;
        }

        File file = new File(path);
        if (!file.exists() || !file.isFile()) {
            Log.e(TAG, "setImagePath: File " + path + " does not exits");
            return false;
        }

        return setImageUnchecked(file, -1);
    }

    private void refresh() {
        if (mImageRect.isEmpty()) {
            return;
        }

        mUIHandler.post(this::invalidate);
    }

    public void setOsdMarked(boolean marked) {
        mOsdMarked = marked;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        Log.d(TAG, "onDraw: ");
        if (!mImageRect.isEmpty()) {
            canvas.concat(calculateFinalMatrix());
        }
        super.onDraw(canvas);

        if (mOsdMarked) {
            int saveCount = canvas.getSaveCount();
            Paint paint = new Paint();
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(3);
            paint.setColor(Color.RED);
            canvas.drawRect(mImageRect, paint);
            canvas.restoreToCount(saveCount);
        }
    }

    private Matrix calculateFinalMatrix() {
        mMatrix.reset();
        mMatrix.postConcat(fitBounds());

        mTmpRect.setEmpty();
        if (mMatrix.mapRect(mTmpRect, mImageRect)) {
            mMatrix.postRotate(mEP.rotDegree, mTmpRect.centerX(), mTmpRect.centerY());

            boolean isDefaultFit = mScaleType == FIT_DEFAULT;
            boolean isOriginalFit = mScaleType == FIT_ORIGINAL;

            if (!isOriginalFit) {
                mTmpRect.setEmpty();
                mMatrix.mapRect(mTmpRect, mImageRect);

                if (isDefaultFit) {
                    // Get original rect for image with out scale
                    Matrix matrix = new Matrix();
                    float offsetX = (mScreenRect.width() - mImageRect.width()) / 2.f;
                    float offsetY = (mScreenRect.height() - mImageRect.height()) / 2.f;
                    matrix.postTranslate(offsetX, offsetY);
                    matrix.postRotate(mEP.rotDegree, mTmpRect.centerX(), mTmpRect.centerY());

                    RectF originRotRect = new RectF();
                    matrix.mapRect(originRotRect, mImageRect);

                    matrix.reset();
                    if (mScreenRect.contains(originRotRect)) {
                        matrix.setRectToRect(mTmpRect, originRotRect, Matrix.ScaleToFit.FILL);
                    } else {
                        matrix.setRectToRect(mTmpRect, mScreenRect, Matrix.ScaleToFit.CENTER);
                    }
                    mMatrix.postConcat(matrix);
                } else {
                    Matrix matrix = new Matrix();
                    matrix.setRectToRect(mTmpRect, mScreenRect, intToScaleFit(mScaleType));
                    mMatrix.postConcat(matrix);
                }
            }
        }

        mTmpRect.setEmpty();
        if (mMatrix.mapRect(mTmpRect, mImageRect)) {
            mMatrix.postScale(mEP.scaleX, mEP.scaleY, mTmpRect.centerX(), mTmpRect.centerY());
        }

        mMatrix.postTranslate(mEP.transX, mEP.transY);
        return mMatrix;
    }

    @SuppressLint("ResourceType")
    public boolean setImageRes(int resId) {
        if (resId == -1) {
            Log.e(TAG, "setImageResource: Invalid resId: -1");
            return false;
        }

        return setImageUnchecked(null, resId);
    }

    public void release() {
        mDrawable = null;
    }

    private boolean setImageUnchecked(File file, int resId) {
        try {
            ImageDecoder.Source source = null;
            if (file != null) {
                source = ImageDecoder.createSource(file);
            } else if (resId != -1) {
                source = ImageDecoder.createSource(getResources(), resId);
            } else {
                Log.e(TAG, "setImageUnchecked: Can not create source");
                return false;
            }

            mDrawable = ImageDecoder.decodeDrawable(
                    source, (decoder, info, s) -> {
                        Log.d(TAG, "setImageUnchecked, memType:" + info.getMimeType() + ", isAnimated: " + info.isAnimated()
                                + ", size: " + info.getSize());
                        if (info.isAnimated()) {
                            OsdImageView.this.mImageType = IMAGE_TYPE_GIF;
                        }
                        decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);
                    });

            return true;
        } catch (IOException e) {
            Log.e(TAG, "setImageUnchecked: " + e.getMessage());
        }

        return false;
    }

    public boolean isReady() {
        return mDrawable != null;
    }

    public boolean isShowing() {
        return getDrawable() == mDrawable;
    }

    public void show() {
        mUIHandler.post(() -> {
            Log.d(TAG, "show");
            setImageDrawable(mDrawable);
            Drawable drawable = getDrawable();

            mImageRect.set(0, 0,
                    drawable.getIntrinsicWidth(),
                    drawable.getIntrinsicHeight());

            if (isAnimated() && drawable instanceof AnimatedImageDrawable) {
                AnimatedImageDrawable anim = (AnimatedImageDrawable) drawable;
                anim.setRepeatCount(AnimatedImageDrawable.REPEAT_INFINITE);
                anim.start();
            } else {
                refresh();
            }
        });
    }

    private Matrix fitBounds() {
        Matrix matrix = null;
        switch (mScaleType) {
            case FIT_DEFAULT:
                matrix = new Matrix();
                if (!mScreenRect.contains(mImageRect)) {
                    matrix.setRectToRect(mImageRect, mScreenRect, Matrix.ScaleToFit.CENTER);
                } else {
                    matrix.postTranslate((mScreenRect.width() - mImageRect.width()) / 2,
                            (mScreenRect.height() - mImageRect.height()) / 2);
                }
                break;
            case FIT_ORIGINAL:
                // Show original image base on left-top
                matrix = new Matrix();
                matrix.postTranslate(0, 0);
                break;
            default:
                matrix = new Matrix();
                Matrix.ScaleToFit scaleToFit = intToScaleFit(mScaleType);
                if (scaleToFit == null) {
                    scaleToFit = Matrix.ScaleToFit.CENTER;
                }
                matrix.setRectToRect(mImageRect, mScreenRect, scaleToFit);
                break;
        }

        return matrix;
    }

    public static Matrix.ScaleToFit intToScaleFit(int scaleType) {
        if (scaleType == Matrix.ScaleToFit.CENTER.ordinal()) {
            return Matrix.ScaleToFit.CENTER;
        } else if (scaleType == Matrix.ScaleToFit.FILL.ordinal()) {
            return Matrix.ScaleToFit.FILL;
        } else if (scaleType == Matrix.ScaleToFit.START.ordinal()) {
            return Matrix.ScaleToFit.START;
        } else if (scaleType == Matrix.ScaleToFit.END.ordinal()) {
            return Matrix.ScaleToFit.END;
        }

        return null;
    }
}
