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

package com.droidlogic.imageplayer.decoder;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.util.Log;
import android.util.Size;
import android.view.Surface;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import com.droidlogic.app.SystemControlManager;

import java.lang.reflect.Method;

public class ImagePlayer {
    public static final String TAG = "ImagePlayer";
    public static int STEP = 100;
    private static final String AXIS = "/sys/class/video/device_resolution";
    private static final int ROTATION_DEGREE = 90;
    private static ImagePlayer mImagePlayerInstance;

    public static final int FIT_DEFAULT = -1;
    public static final int FIT_ORIGINAL = -2;

    //0: osd, 1: auto, 2: video
    private static final String PROP_VIEW_MODE = "debug.imageplayer.view_mode";
    private static final String PROP_SHOW_IMG_INFO = "debug.imageplayer.show_img_info";
    private static final String PROP_OSD_LIMITED_SIZE = "debug.imageplayer.osd_limited_size";
    private final int mOsdMaxImagePixels;

    private int mShowingFit = -1;
    private int mViewMode;
    private final boolean mShowImageInfo;
    private boolean mIsShowToOsd = true;
    private Size mOsdLimitedSize;
    private String mDebugImageInfo = "";

    /**
     * Always playing image to OSD
     */
    public static final int VIEW_MODE_FAST_OSD = 0;
    /**
     * Select the appropriate display target, default mode
     */
    public static final int VIEW_MODE_AUTO_HD = 1;
    /**
     * Always playing image to Video Composer
     */
    public static final int VIEW_MODE_VC_UHD = 2;

    private EmbeddedView mEmbeddedView;
    private OsdImageView mOsdImageView;
    private NetImageLoader mNetImageLoader;

    static {
        System.loadLibrary("image_jni");
    }
    Object lockObject = new Object();
    BmpInfo mBmpInfoHandler;
    BmpInfo mLastBmpInfo;
    boolean bindSurface;
    private int mScreenHeight;
    private int mScreenWidth;
    private int mSurfaceHeight;
    private int mSurfaceWidth;
    private PrepareReadyListener mReadyListener;
    private HandlerThread mWorkThread;
    private Handler mWorkHandler;
    private Handler mUiHandler = new Handler(Looper.getMainLooper());
    private SurfaceControl.Transaction mTransaction;
    private Status mStatus = Status.IDLE;
    private String mImageFilePath;
    private int mDegree;
    private int mBufferWidth;
    private int mBufferHeight;
    private SurfaceView mSurfaceView;
    private boolean mredraw;
    private float mSx;
    private float mSy;
    private final int mOsdWidth = getProperties("ro.surface_flinger.max_graphics_width", 3840);
    private final int mOsdHeight = getProperties("ro.surface_flinger.max_graphics_height", 2160);
    private Runnable preparedDelay = new Runnable() {
        @Override
        public void run() {
            boolean ready = (mReadyListener != null);
            if (ready) {
                mStatus = Status.PREPARED;

                mReadyListener.Prepared((mBmpInfoHandler != null) ?
                        mBmpInfoHandler.filePath : null);
            } else {
                mWorkHandler.postDelayed(preparedDelay, 200);
            }
        }
    };
    private Runnable reshow = new Runnable() {
         @Override
        public void run() {
             if (mSurfaceView == null) {
                 mWorkHandler.postDelayed(reshow, 200);
             } else {
                 show(mShowingFit);
             }
        }
    };
    private Runnable decodeRunnable = new Runnable() {
        @Override
        public void run() {
            synchronized(lockObject) {
                if (mBmpInfoHandler == null) {
                    return;
                }
                long time  = System.currentTimeMillis();
                boolean decodeOk = mBmpInfoHandler.decode();
                boolean ready = (mReadyListener != null);
                Log.d(TAG,"decodeOk"+decodeOk+" ready"+ready);
                if (decodeOk && ready) {
                    mStatus = Status.PREPARED;
                    mReadyListener.Prepared(mBmpInfoHandler.filePath);
                } else if (decodeOk) {
                    mWorkHandler.postDelayed(preparedDelay, 200);
                } else {
                    if (mReadyListener != null ) {
                        mReadyListener.playerr(mBmpInfoHandler.filePath);
                    }
                    Log.d("TAG", "cannot display");
                }
            }
        }
    };

    public boolean CurrentBmpAvailable() {
        if (mIsShowToOsd && mOsdImageView != null) {
            return true;
        }

        if (mBmpInfoHandler != null ) {
            if ((mBmpInfoHandler instanceof GifBmpInfo) &&
                    ((GifBmpInfo)mBmpInfoHandler).mFrameCount >0 ) {
                return true;
            }
            if (!(mBmpInfoHandler instanceof GifBmpInfo) && mBmpInfoHandler.mNativeBmpPtr != 0) {
                return true;
            }
        }
        return false;
    }
    private Runnable setDataSourceWork = new Runnable() {
        @Override
        public void run() {
            boolean ret = true;
            synchronized (lockObject) {
                mBmpInfoHandler = BmpInfoFactory.getBmpInfo(mImageFilePath);
                if (mBmpInfoHandler == null) {
                    Log.e(TAG, "setDataSourceWork, mBmpInfoHandler is null");
                    return;
                }

                mBmpInfoHandler.setImagePlayer(ImagePlayer.this);
                ret = mBmpInfoHandler.setDataSource(mImageFilePath);
                Log.d(TAG,"setDataSource"+mImageFilePath+"@"+mBmpInfoHandler+"----"+ret);
                if (!ret && mBmpInfoHandler instanceof GifBmpInfo) {
                    if (mLastBmpInfo != null) {
                        mLastBmpInfo.release();
                    }
                    mBmpInfoHandler = BmpInfoFactory.getStaticBmpInfo();
                    mBmpInfoHandler.setImagePlayer(ImagePlayer.this);
                    ret = mBmpInfoHandler.setDataSource(mImageFilePath);
                    mLastBmpInfo = mBmpInfoHandler;
                }
            }
            if (ret)
                mWorkHandler.post(decodeRunnable);
        }
    };
    private Runnable releasework = new Runnable() {
         @Override
        public void run() {
           synchronized(lockObject) {
                if (mBmpInfoHandler != null) {
                    Log.d(TAG,"releasework"+mBmpInfoHandler);
                    mBmpInfoHandler.release();
                    mBmpInfoHandler = null;
                }
                nativeRelease();
           }
        }
    };
    private Runnable ShowFrame = new Runnable() {
        @Override
        public void run() {
            synchronized(lockObject) {
                if (bindSurface) {
                    Log.d(TAG,"mStatus= "+mStatus + ", mShowingFit= " + mShowingFit);
                    if (mStatus != Status.PREPARED && mStatus != Status.PLAYING) {
                        return;
                    }

                    if (mBmpInfoHandler == null) {
                        Log.e(TAG, "ShowFrame, mBmpInfoHandler is null.");
                        return;
                    }

                    boolean rendered = mBmpInfoHandler.mRendered;

                    Log.d(TAG, "mBmpInfo" + mBmpInfoHandler + "**" + mBmpInfoHandler.mNativeBmpPtr
                            + "mStatus" + mStatus + ", rendered: " + rendered);
                    if ((mBmpInfoHandler instanceof GifBmpInfo) &&
                            ((GifBmpInfo)mBmpInfoHandler).mFrameCount >0) {
                        Log.d(TAG,"((GifBmpInfo)mBmpInfoHandler).mFrameCount"+((GifBmpInfo)mBmpInfoHandler).mFrameCount);
                        long startMs = SystemClock.uptimeMillis();

                        if (mBmpInfoHandler.renderFrame(mShowingFit)) {
                            mStatus = Status.PLAYING;
                            Log.d(TAG, "GifRender cost: " + (SystemClock.uptimeMillis() - startMs) + "ms");

                            long decodeStartMs = SystemClock.uptimeMillis();
                            mBmpInfoHandler.decodeNext();
                            Log.d(TAG, "GifDecode cost: " + (SystemClock.uptimeMillis() - decodeStartMs) + "ms");
                            int duration = ((GifBmpInfo)mBmpInfoHandler).getDuration();

                            long decodeElapseTime = SystemClock.uptimeMillis() - startMs;
                            long delayMs = duration - decodeElapseTime;
                            if (delayMs < 0) delayMs = 0;

                            Log.d(TAG, "((GifBmpInfo)mBmpInfoHandler).getDuration: " + duration + ", delayMs= " + delayMs);
                            mWorkHandler.postDelayed(ShowFrame, delayMs);
                            if (!rendered && mReadyListener != null ) {
                                mReadyListener.played(mBmpInfoHandler.filePath);
                            }
                        }
                    }else if ((mBmpInfoHandler.mNativeBmpPtr != 0) && mBmpInfoHandler.renderFrame(mShowingFit)){
                        mStatus = Status.PLAYING;
                        mShowingFit = -1;
                        mUiHandler.post(()->updateViewVisibility());
                        if (!rendered && mReadyListener != null ) {
                            mReadyListener.played(mBmpInfoHandler.filePath);
                        }
                    }
                } else {
                    mWorkHandler.postDelayed(ShowFrame, 200);
                }
            }
        }
    };

    private ImagePlayer() {
        mTransaction = new SurfaceControl.Transaction();
        mViewMode = getProperties(PROP_VIEW_MODE, VIEW_MODE_AUTO_HD);
        mShowImageInfo = getProperties(PROP_SHOW_IMG_INFO, false);
        mOsdMaxImagePixels = getPanelFrameSize();

        try {
            mOsdLimitedSize = Size.parseSize(getProperties(PROP_OSD_LIMITED_SIZE, "1920x1080"));
        } catch (NumberFormatException e) {
            Log.e(TAG, PROP_OSD_LIMITED_SIZE + " format error, using default 1920x1080");
            mOsdLimitedSize = Size.parseSize("1920x1080");
        }
    }

    public synchronized static ImagePlayer getInstance() {
        if (mImagePlayerInstance == null) {
            mImagePlayerInstance = new ImagePlayer();
        }
        return mImagePlayerInstance;
    }
    public native static void  nativeReset();
    public native static void  nativeRestore();
    public native static void  nativeRelease();
    public native static int nativeScale(float sx, float sy, boolean redraw);

    public native static int nativeShow(long bmphandler, int fit, boolean isMovie);

    public native static int nativeRotate(int ori, boolean redraw);

    public native static int nativeTranslate(float x, float y, boolean redraw);

    public native static int nativeRotateScaleCrop(int ori, float sx, float sy, boolean redraw);

    public native static int nativeTransform(int ori, float sx, float sy, int left, int right, int top, int bottom, int step);

    public native static int updateWindowSize(int width, int height);

    private void appendDebugInfo(String infoSegment, boolean post) {
        if (mShowImageInfo) {
            if (post) {
                mDebugImageInfo = mDebugImageInfo.concat(infoSegment);
            } else { // pre-concat
                mDebugImageInfo = infoSegment.concat(mDebugImageInfo);
            }
        }
    }

    public void setPrepareListener(PrepareReadyListener listener) {
        this.mReadyListener = listener;
        Log.d(TAG, "setPrepared" + listener);

    }
    public PrepareReadyListener getPrepareListener() {
        return this.mReadyListener;
    }
    public  boolean setDataSource(String filePath) {
        mImageFilePath = filePath;

        if (mSurfaceView == null || mOsdImageView == null) {
            RetryUtil.retryIf(TAG + "_setDataSource", mUiHandler,
                    () -> setDataSourceChecked(mImageFilePath),
                    () -> (mSurfaceView == null || mOsdImageView == null));
        } else {
            setDataSourceChecked(mImageFilePath);
        }
        return true;
    }

    private void setDataSourceChecked(String path) {
        mDebugImageInfo = "";

        boolean netImage = NetImageLoader.isNetImage(path);
        mEmbeddedView.showNetImageStateWindow(netImage);

        if (netImage) {
            loadImageFromInternet(path);
        } else {
            mIsShowToOsd = isDisplayToOsd(path);
            setDataSourceInternal(path, null);
        }
    }

    private void loadImageFromInternet(String path) {
        mNetImageLoader.start();
        mNetImageLoader.load(path, (phase, drawable, info) -> {
            if (phase == NetImageLoader.LoadPhase.SUCCESS) {
                if (info.isAnimated) {
                    mIsShowToOsd = true;
                } else if (drawable instanceof BitmapDrawable){
                    Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
                    if (bitmap != null) {
                        //TODO: isDisplayToOsd(bitmap); considering show large image to video
                        mIsShowToOsd = isDisplayToOsd(bitmap); // Just for fill debug info
                        mIsShowToOsd = true;
                    } else {
                        mIsShowToOsd = true;
                    }
                }

                if (mReadyListener != null) {
                    mReadyListener.netImageLoaded(path);
                }

                setDataSourceInternal(path, drawable);
            } else {
                Log.e(TAG, phase.toString());

                if (phase == NetImageLoader.LoadPhase.FAILED) {
                    if (mReadyListener != null) {
                        mReadyListener.netImageLoaded(path);
                        mReadyListener.playerr(path);
                    }
                } else {
                    if (mReadyListener != null) {
                        mReadyListener.netImageLoading(path);
                    }
                }
            }

            // Update Downloading state
            switch (phase) {
                case SUCCESS:
                    mEmbeddedView.showNetImageStateWindow(false);
                    break;
                case FAILED:
                    mEmbeddedView.setNetImageState(phase.name() + "\n" + phase.phaseDescription);
                    break;
                case DOWNLOADING:
                    mEmbeddedView.setNetImageState(phase.name() + ":" + phase.phaseDescription);
                    break;
                default:
                    mEmbeddedView.setNetImageState(phase.name());
                    break;
            }
        });
    }


    private void setDataSourceInternal(String path, Drawable drawable) {
        Log.d(TAG, "setDataSourceInternal: " + path + ", mIsShowToOsd: " + mIsShowToOsd);

        {
            appendDebugInfo(mIsShowToOsd ? "Target: OSD\n" : "Target: Video\n", false);
            appendDebugInfo(path.substring(path.lastIndexOf("/") + 1) + "\n\n", false);
        }

        if (mIsShowToOsd) {
            boolean ok = (drawable != null) ? mOsdImageView.setDrawable(drawable)
                    : mOsdImageView.setImagePath(path);
            if (ok) {
                mStatus = Status.PREPARED;
                if (mReadyListener != null) {
                    mReadyListener.Prepared(path);
                }
            } else {
                if (mReadyListener != null) {
                    mReadyListener.playerr(path);
                }
            }
        } else {
            mWorkHandler.removeCallbacks(rotateWork);
            mWorkHandler.removeCallbacks(rotateCropWork);
            mWorkHandler.removeCallbacks(ShowFrame);
            mWorkHandler.removeCallbacks(decodeRunnable);
            mWorkHandler.post(setDataSourceWork);
        }
    }

    private void updateViewVisibility() {
        Log.d(TAG, "updateViewVisibility, mIsShowToOsd: " + mIsShowToOsd);
        if (mEmbeddedView != null) {
            mEmbeddedView.flipView(mIsShowToOsd);
            mEmbeddedView.setInfo(mDebugImageInfo);
        }
    }

    /**
     * set display target mode, OSD only, auto, video only
     * @param viewMode :VIEW_MODE_FAST_OSD, VIEW_MODE_AUTO_HD, VIEW_MODE_VC_UHD
     */
    public void setViewMode(int viewMode) {
        mViewMode = viewMode;
    }

    private static int getPanelFrameSize() {
        final int defaultSize = 100 * 1024 * 1024; // 100 MB;
        return Math.max(getProperties("ro.hwui.max_texture_allocation_size", defaultSize),
                defaultSize);
    }

    private boolean isDisplayToOsd(String filePath) {
        // Only for gif display to OSD now
        //return filePath.toLowerCase().endsWith(".gif");

        // Always play gif to osd
        if (filePath.toLowerCase().endsWith(".gif")) {
            return true;
        }

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        Bitmap bitmap = BitmapFactory.decodeFile(filePath);
        if (bitmap == null) {
            appendDebugInfo("Bad Image" + "\n",true);
            Log.e(TAG, "Decode image: " + filePath + " failed");
            return true;
        }

        return isDisplayToOsd(bitmap);
    }

    private boolean isDisplayToOsd(Bitmap bitmap) {
        /* Limiting max image size for displaying to osd */
        int osdLimitedWidth = mOsdLimitedSize.getWidth();
        int osdLimitedHeight = mOsdLimitedSize.getHeight();

        { // Debug scope
            String viewMode = null;
            switch (mViewMode) {
                case VIEW_MODE_FAST_OSD:
                    viewMode = "Osd only";
                    break;
                case VIEW_MODE_VC_UHD:
                    viewMode = "Video only";
                    break;
                case VIEW_MODE_AUTO_HD:
                    viewMode = "Auto";
                    break;
                default:
                    viewMode = "Unknown";
                    break;
            }
            appendDebugInfo("ViewMode: " + viewMode + "\n",true);
            appendDebugInfo("Image size: " + bitmap.getWidth() + "x" + bitmap.getHeight() + "\n",true);
            appendDebugInfo("OSD size: " + mSurfaceView.getWidth() + " x " + mSurfaceView.getHeight() + "\n", true);
            appendDebugInfo("OSD limited size: " + osdLimitedWidth + " x " + osdLimitedHeight + "\n", true);
            appendDebugInfo("Video size: " + mScreenWidth + " x " + mScreenHeight, true);
        }

        if (mViewMode == VIEW_MODE_FAST_OSD) {
            if (bitmap.getByteCount() > mOsdMaxImagePixels) {
                Log.e(TAG, "Bitmap is too big, can not be drawn on OSD, show on video");
                return false;
            }

            return true;
        } else if (mViewMode == VIEW_MODE_VC_UHD) {
            return false;
        } else if (mViewMode == VIEW_MODE_AUTO_HD) {
            if (bitmap.getByteCount() > mOsdMaxImagePixels) {
                Log.e(TAG, "Bitmap is too big, can not be drawn on OSD, show on video");
                return false;
            }

            int osdWidth = mSurfaceView != null ? mSurfaceView.getWidth() : mOsdWidth;
            int osdHeight = mSurfaceView != null ? mSurfaceView.getHeight() : mOsdHeight;
            int videoWidth = mScreenWidth;
            int videoHeight = mScreenHeight;

            Log.d(TAG, "osdSize: [" + osdWidth + "x" + osdHeight + "]," +
                    "imageSize: [" + bitmap.getWidth() + "x" + bitmap.getHeight() + "]," +
                    "videoSize: [" + videoWidth + "x" + videoHeight + "], bitmap.getByteCount: " + bitmap.getByteCount());

            // Max osd allowed limited to 1920x1080
            int limitedOsdW = Math.min(osdWidth, osdLimitedWidth);
            int limitedOsdH = Math.min(osdHeight, osdLimitedHeight);

            Log.d(TAG, "limitedOsdW: " + limitedOsdW + ", limitedOsdH: " + limitedOsdH);

            if (videoWidth <= limitedOsdW || videoHeight <= limitedOsdH) {
                // No need show to video
                return true;
            }

            return bitmap.getWidth() <= limitedOsdW && bitmap.getHeight() <= limitedOsdH;
        }

        // OSD first
        return true;
    }

    public void bindSurface(final SurfaceHolder holder) {
        int surfaceWidth = 0;
        int surfaceHeight = 0;
        if (mScreenWidth > 0 && mScreenHeight > 0) {
            surfaceWidth = mScreenWidth;
            surfaceHeight = mScreenHeight;
        } else {
            surfaceWidth = mSurfaceView.getWidth();
            surfaceHeight = mSurfaceView.getHeight();
            Log.w(TAG, "bindSurface, Can not recognized screen size, using view size");
        }

        final int fSurfaceWidth = surfaceWidth;
        final int fSurfaceHeight = surfaceHeight;
        mWorkHandler.post(new Runnable() {
            @Override
            public void run() {
                Log.i(TAG, "bindSurface, Screen size: [" + fSurfaceWidth + " x " + fSurfaceHeight + "]");
                bindSurface(holder.getSurface(), fSurfaceWidth, fSurfaceHeight);
                bindSurface = true;
            }
        });

    }
    private Point getInitialFrameSize() {
        int srcW = getBmpWidth();
        int srcH = getBmpHeight();
        Log.d(TAG,"getInitalFrameSize"+srcW+"x"+srcH+" - "+mDegree);
        if ((mDegree / ROTATION_DEGREE) % 2 != 0) {
            srcW = getBmpHeight();
            srcH = getBmpWidth();
            if (srcW > mOsdWidth || srcH > mOsdHeight) {
                float scaleDown = 1.0f * mOsdWidth / srcW < 1.0f * mOsdHeight / srcH ?
                        1.0f * mOsdWidth / srcW : 1.0f * mOsdHeight / srcH;
                srcW = (int) Math.ceil(scaleDown * srcW);
                srcH = (int) Math.ceil(scaleDown * srcH);
            }
        }
       if (srcW > BmpInfoFactory.BMP_SMALL_W || srcH > BmpInfoFactory.BMP_SMALL_H ||isNeedFullScreen()) {
            float scaleUp = 1.0f * mOsdWidth /srcW < 1.0f * mOsdHeight /srcH?
                     1.0f * mOsdWidth /srcW : 1.0f * mOsdHeight /srcH;
            srcH = (int) Math.ceil(scaleUp*srcH);
            srcW = (int) Math.ceil(scaleUp*srcW);
        }else {
            srcW = srcW > mOsdWidth ? mOsdWidth : srcW;
            srcH = srcH > mOsdHeight ? mOsdHeight : srcH;
        }
        int frameWidth = ((srcW + 1) & ~1);
        int frameHeight = ((srcH + 1) & ~1);
        return new Point(frameWidth,frameHeight);
    }
    private boolean isNeedFullScreen() {
        int srcW = getBmpWidth();
        int srcH = getBmpHeight();
        if (srcW > BmpInfoFactory.BMP_SMALL_W || srcH > BmpInfoFactory.BMP_SMALL_H) {
             return true;
         }
         return false;
    }

    public boolean show() {
        return show(FIT_DEFAULT);
    }


    /**
     * @param fit Using {@link Matrix.ScaleToFit#ordinal()}.
     *           {@link ImagePlayer#FIT_DEFAULT} for default action.
     *            {@link ImagePlayer#FIT_ORIGINAL} will show as origin picture base on left-top.
     *            Otherwise will be treat as FitCenter.
     * @return
     */
    public boolean show(int fit) {
        Log.d(TAG,"show"+mStatus+" "+mSurfaceView);
        if (mStatus != Status.PREPARED && mStatus != Status.PLAYING) {
            return false;
        }

        mShowingFit = fit;

        if (mIsShowToOsd) {
            if (fit == FIT_DEFAULT || fit == FIT_ORIGINAL) {
                mOsdImageView.setExtendsScaleType(fit);
            } else {
                mOsdImageView.setFit(OsdImageView.intToScaleFit(fit));
            }

            Log.d(TAG, "mOsdImageView: " + mOsdImageView + ", mOsdImageView.isReady: " + mOsdImageView.isReady());

            RetryUtil.retryIf(TAG + "_show", mUiHandler, ()->{
                mOsdImageView.show();
                mStatus = Status.PLAYING;
                updateViewVisibility();
                if (mReadyListener != null) {
                    mReadyListener.played(mImageFilePath);
                }
            }, ()->(mOsdImageView == null || !mOsdImageView.isReady()));
        } else {
            if (mSurfaceView == null) {
                Log.d(TAG, "mSurfaceView invalid, retry after 200ms");
                mWorkHandler.postDelayed(reshow, 200);
                return false;
            }
            mWorkHandler.post(ShowFrame);
            Point p = getInitialFrameSize();
            mSurfaceWidth = p.x;
            mSurfaceHeight = p.y;
            setPaintSize(1,1);
        }

        return true;
    }

    private Point viewSizeToAxisSize(int width, int height) {
        int targetWidth;
        int targetHeight;

        int viewWidth = mOsdWidth;
        int viewHeight = mOsdHeight;
        int screenWidth = mScreenWidth;
        int screenHeight = mScreenHeight;

        int rot = getProperties("persist.sys.builtinrotation", 0);
        if (rot == 1 || rot == 3) {
            int tmp = viewWidth;
            viewWidth = viewHeight;
            viewHeight = tmp;

            tmp = screenWidth;
            screenWidth = screenHeight;
            screenHeight = tmp;
        }

        Log.d(TAG, "viewSizeToAxisSize, width= " + width + ", height= " + height +
                ", viewWidth= " + viewWidth + ", viewHeight= " + viewHeight +
                ", mScreenWidth= " + screenWidth + ", mScreenHeight= " + screenHeight);

        if (viewWidth == screenWidth && viewHeight == screenHeight) {
            targetWidth = width;
            targetHeight = height;
        } else {
            targetWidth = (int) ((float)width / viewWidth * screenWidth);
            targetHeight = (int) ((float)height / viewHeight * screenHeight);
        }

        return new Point(targetWidth, targetHeight);
    }

    public boolean updateWindowDimension(int width, int height) {
        Log.d(TAG, "updateWindowSize: " + width + " x " + height);

        if (width <= 0 || height <= 0) {
            Log.w(TAG, "No need update for invalid or not changed");
            return false;
        }

        RetryUtil.retryIf(TAG + "_updateWindowDimension", mUiHandler,
                () -> {
                    if (mIsShowToOsd) {
                        mEmbeddedView.relayout(width, height);
                    } else {
                        final Point point = viewSizeToAxisSize(width, height);
                        mWorkHandler.post(()-> updateWindowSize(point.x, point.y));
                    }

                    if (mStatus == Status.PLAYING && mImageFilePath != null) {
                        Log.d(TAG, "Replay in updateWindowDimension");
                        setDataSourceChecked(mImageFilePath);
                        show(mShowingFit);
                    }
                },
                () -> (mSurfaceView == null || mOsdImageView == null));

        return true;
    }

    public void setDisplay(SurfaceView surfaceview) {
        Log.d(TAG,"setDisplay");
        if (mEmbeddedView != null) {
            mEmbeddedView.release();
        }

        mEmbeddedView = new EmbeddedView(surfaceview.getContext());
        mEmbeddedView.mountTo(surfaceview, ()->{
            mOsdImageView = mEmbeddedView.getOsdView();
            mEmbeddedView.showInfoWindow(mShowImageInfo);
            mSurfaceView = mEmbeddedView.getSurfaceView();
            Log.d(TAG, "mSurfaceView and mOsdImageView is ready");
            surfaceview.setFocusable(false);
            bindSurface(mSurfaceView.getHolder());
        }, surfaceview.getWidth(), surfaceview.getHeight());

    }
    private void setPaintSize(float sx,float sy) {
        int frameWidth = (int)(mSurfaceWidth*sx);
        int frameHeight = (int)(mSurfaceHeight*sy);
        int top = (mOsdHeight - frameHeight)/2;
        int left = (mOsdWidth - frameWidth)/2;
        Log.d(TAG,"setPaintSize: "+left+"-"+top+"-"+(left+frameWidth)+"-"+(top+frameHeight)+" mSurfaceView"+mSurfaceView);
//        SurfaceControl sc = mSurfaceView.getSurfaceControl();
//        mTransaction.setVisibility(sc, true)
//                        .setGeometry(sc, null, new Rect(left, top, left+frameWidth, top+frameHeight), Surface.ROTATION_0)
//                        .setBufferSize(sc,frameWidth,frameHeight)
//                        .apply();
    }
    private Runnable rotateWork = new Runnable() {
         @Override
        public void run() {
           synchronized(lockObject) {
                nativeRotate(mDegree,mredraw);
           }
        }
    };

    public int setRotate(int degrees) {
        if (mIsShowToOsd) {
            mOsdImageView.rotate(degrees);
        } else {
            mWorkHandler.removeCallbacks(rotateWork);
            boolean redraw = true;
            mDegree = degrees;
            mredraw = redraw;
            mWorkHandler.post(rotateWork);
            Point p = getInitialFrameSize();
            mSurfaceWidth = p.x;
            mSurfaceHeight = p.y;
            setPaintSize(1,1);
        }

        return 0;
    }

    public int setScale(float sx, float sy) {
        if (mIsShowToOsd) {
            mOsdImageView.scale(sx, sy);
        } else {
            boolean redraw = true;
            synchronized (lockObject) {
                mWorkHandler.post(() -> nativeScale(sx, sy, redraw));
            }
        }
        //setPaintSize(sx,sy);
        return 0;
    }

    public int setTranslate(int xpos,int ypos, float scale) {
        if (mIsShowToOsd) {
            mOsdImageView.translate(xpos, ypos);
        } else {
            boolean redraw = true;
            synchronized(lockObject) {
                mWorkHandler.post(()->nativeTranslate(xpos, ypos, redraw));
            }
        }
//        mWorkHandler.removeCallbacks(ShowFrame);
//        int frameWidth = (int)(mSurfaceWidth*scale);
//        int frameHeight = (int)(mSurfaceHeight*scale);
//        int top = (mOsdHeight - frameHeight)/2;
//        int left = (mOsdWidth - frameWidth)/2;
//        Log.d(TAG,"setTranslate("+xpos+" "+ypos+")top "+top+" left "+left+" scale "+scale);
//        if (left > 0) {
//            xpos = 0;
//        }
//        if (top > 0) {
//            ypos = 0;
//        }
//        if (xpos == 0 && ypos == 0) return -1;
//        int step = (int)((scale*10-10)/2);
//        int xStep = Math.abs(left/step);
//        int yStep = Math.abs(top/step);
//        Log.d(TAG,"step"+step+" "+xStep+ "yStep"+yStep);
//        top -= ypos*yStep;
//        left -= xpos*xStep;
//        Log.d(TAG,"top"+top+"left"+left+"step"+step);
//        if (Math.abs(xpos) == step || Math.abs(ypos) == step) {
//            if (xpos == -step) {
//                left = 0;
//            }
//            if (ypos == -step) {
//                top = 0;
//            }
//            if (xpos == step) {
//                left = (mOsdWidth - frameWidth);
//            }
//            if (ypos == step) {
//                top = (mOsdHeight - frameHeight);
//            }
//        }
//
//        Log.d(TAG,"setTranslate, ("+left+" "+top+" "+(left+frameWidth)+" "+(top+frameHeight)+")");
//        SurfaceControl sc = mSurfaceView.getSurfaceControl();
//        mTransaction.setVisibility(sc, true)
//                        .setGeometry(sc, null, new Rect(left, top, left+frameWidth, top+frameHeight), Surface.ROTATION_0)
//                        .setBufferSize(sc,frameWidth,frameHeight)
//                        .apply();
        return 0;
    }

    public void restore() {
        if (mIsShowToOsd) {
            mOsdImageView.restore();
        } else {
            mWorkHandler.post(()->nativeRestore());
        }
    }

    private Runnable rotateCropWork = new Runnable() {
         @Override
        public void run() {
           synchronized(lockObject) {
                nativeRotateScaleCrop(mDegree, mSx, mSy, mredraw);
           }
        }
    };

    /**Instead of setRotate(int degrees)*/
    @Deprecated
    public int setRotateScale(int degrees, float sx, float sy) {
        if (mIsShowToOsd) {
            mOsdImageView.rotate(degrees);
        } else {
            mWorkHandler.removeCallbacks(rotateCropWork);
            boolean redraw = true;
            mDegree = degrees;
            mSx = mSx;
            mSy = mSy;
            mredraw = redraw;
            //mWorkHandler.post(rotateCropWork);
            mWorkHandler.post(rotateWork);
            Point p = getInitialFrameSize();
            mSurfaceWidth = p.x;
            mSurfaceHeight = p.y;
            setPaintSize(sx,sy);
        }
        return 0;
    }

    public boolean isPlaying() {
        return mStatus == Status.PLAYING;
    }

    public void unbindSurface() {
        synchronized(lockObject) {
            bindSurface = false;
            nativeUnbindSurface();
        }
        mSurfaceView = null;
    }
    private boolean checkVideoAxis() {
        SystemControlManager mSystemControlManager = SystemControlManager.getInstance();
        String deviceoutput = mSystemControlManager.readSysFs(AXIS);
        if (deviceoutput != null && !deviceoutput.isEmpty()) {
            Log.d(TAG,"checkVideoAxis"+deviceoutput);
            String[] axisStr = deviceoutput.split("x");
            if (axisStr.length < 2)return false;
            try {
                mScreenWidth = Integer.parseInt(axisStr[0]);
                mScreenHeight = Integer.parseInt(axisStr[1]);
                return true;
            }catch(Exception ex){
                return false;
            }
        }
        return false;
    }
    public void initPlayer(Context context) {
        if (mWorkThread != null && mWorkThread.isAlive()) {
            mWorkThread.quit();
        }
        mWorkThread = new HandlerThread("worker",Process.THREAD_PRIORITY_VIDEO);
        mWorkThread.start();
        mWorkHandler = new Handler(mWorkThread.getLooper());
        mNetImageLoader = new NetImageLoader();

        if (mOsdImageView == null) {
            mOsdImageView = new OsdImageView(context);
        }

        if (!checkVideoAxis()) {
            initParam();
        }else {
            initVideoParam();
        }
    }
    private native void bindSurface(Surface surface, int surfaceWidth, int surfaceHeight);

    private native void nativeUnbindSurface();

    private native int initParam();

    private native int initVideoParam();

    public void stop() {
        if (mStatus == Status.PLAYING) {
            mWorkHandler.removeCallbacks(rotateWork);
            mWorkHandler.removeCallbacks(rotateCropWork);
            mWorkHandler.removeCallbacks(decodeRunnable);
            Log.d(TAG,"stop");
            mNetImageLoader.end();
            mWorkHandler.removeCallbacks(ShowFrame);
            mWorkHandler.post(this::unbindSurface);
            mWorkHandler.getLooper().quitSafely();
            mStatus = Status.STOPPED;
        }
    }

    public void release() {
        Log.d(TAG,"release");
        mWorkHandler.removeCallbacks(ShowFrame);
        mWorkHandler.removeCallbacks(decodeRunnable);
        mWorkHandler.removeCallbacks(setDataSourceWork);
        mWorkHandler.post(releasework);
        mStatus = Status.IDLE;
        mSurfaceHeight = 0;
        mSurfaceWidth = 0;
        mDegree = 0;
    }

    public int getMxW() {
        return mScreenWidth;
    }

    public int getMxH() {
        return mScreenHeight;
    }

    public int getBmpWidth() {
        if (mBmpInfoHandler != null) {
            return mBmpInfoHandler.getBmpWidth();
        }
        return 0;
    }

    public int getBmpHeight() {
        if (mBmpInfoHandler != null)
            return mBmpInfoHandler.getBmpHeight();
        return 0;

    }

    public enum Status {PREPARED, PLAYING, STOPPED, IDLE}

    public interface PrepareReadyListener {
        void netImageLoading(String curUri);
        void netImageLoaded(String curUri);
        void Prepared(String curUri);
        void played(String curUri);
        void playerr(String curUri);
    }

    public static boolean getProperties(String key, boolean def) {
        boolean defVal = def;
        try {
            Class properClass = Class.forName("android.os.SystemProperties");
            Method getMethod = properClass.getMethod("getBoolean", String.class, boolean.class);
            defVal = (boolean) getMethod.invoke(null, key, def);
        } catch (Exception ex) {
            ex.printStackTrace();
        } finally {
            Log.d(TAG, "getProperty:" + key + " defVal:" + defVal);
            return defVal;
        }
    }

    public static int getProperties(String key, int def) {
        int defVal = def;
        try {
            Class properClass = Class.forName("android.os.SystemProperties");
            Method getMethod = properClass.getMethod("getInt", String.class, int.class);
            defVal = (int) getMethod.invoke(null, key, def);
        } catch (Exception ex) {
            ex.printStackTrace();
        } finally {
            Log.d(TAG, "getProperty:" + key + " defVal:" + defVal);
            return defVal;
        }
    }

    public static String getProperties(String key, String def) {
        String defVal = def;
        try {
            Class properClass = Class.forName("android.os.SystemProperties");
            Method getMethod = properClass.getMethod("get", String.class, String.class);
            defVal = (String) getMethod.invoke(null, key, def);
        } catch (Exception ex) {
            ex.printStackTrace();
        } finally {
            Log.d(TAG, "getProperty:" + key + " defVal:" + defVal);
            return defVal;
        }
    }
}
