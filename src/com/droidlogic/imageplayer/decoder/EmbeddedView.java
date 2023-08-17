package com.droidlogic.imageplayer.decoder;

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Gravity;
import android.view.Surface;
import android.view.SurfaceControlViewHost;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.Objects;

public class EmbeddedView extends FrameLayout implements SurfaceHolder.Callback {
    private static final String TAG = "EmbeddedView";
    private SurfaceView mSurfaceView;
    private OsdImageView mOsdImageView;
    private Context mContext;
    private SurfaceControlViewHost mHost;
    private SurfaceHolder mHolder = null;
    private Runnable mMountFinished;

    public EmbeddedView(@NonNull Context context) {
        this(context, null);
    }

    public EmbeddedView(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public EmbeddedView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        this(context, attrs, defStyleAttr, 0);
    }

    public EmbeddedView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);

        init(context);
    }

    private void init(Context context) {
        mContext = context;
        setBackgroundColor(Color.BLACK);
        setFocusable(false);
    }

    public void mountTo(SurfaceView parent, Runnable finished, int width, int height) {
        Log.d(TAG, "mountTo: " + parent);
        mHost = new SurfaceControlViewHost(mContext, mContext.getDisplay(), parent.getHostToken());
        parent.setChildSurfacePackage(Objects.requireNonNull(mHost.getSurfacePackage()));

        mOsdImageView = new OsdImageView(mContext);
        mSurfaceView = new SurfaceView(mContext);
        mSurfaceView.getHolder().addCallback(this);

        addView(mSurfaceView, new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT,
                Gravity.CENTER_HORIZONTAL | Gravity.TOP));

        addView(mOsdImageView, new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT,
                Gravity.CENTER_HORIZONTAL | Gravity.TOP));

        /*Default show SurfaceView*/
        flipView(false);

        mHost.setView(this, width, height);

        mMountFinished = finished;
        notifyFinish();
    }

    public void flipView(boolean toOsd) {
        if (toOsd) {
            mOsdImageView.setVisibility(View.VISIBLE);
        } else {
            mOsdImageView.setVisibility(View.INVISIBLE);
        }
    }

    public OsdImageView getOsdView() {
        return mOsdImageView;
    }

    public SurfaceView getSurfaceView() {
        return mSurfaceView;
    }

    @Override
    protected void onDetachedFromWindow() {
        Log.d(TAG, "onDetachedFromWindow");
        release();
        super.onDetachedFromWindow();
    }

    public void relayout(int width, int height) {
        if (mHost != null) {
            mHost.relayout(width, height);
        }
    }

    public void release() {
        Log.d(TAG, "release");
        if (mHost != null) {
            mHost.release();
            mHost = null;
        }
    }

    private void notifyFinish() {
        if (mHolder != null && mMountFinished != null) {
            mMountFinished.run();
            mMountFinished = null;
        }
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {
        mHolder = surfaceHolder;
        Log.d(TAG, "surfaceCreated, mHolder= " + mHolder);
        notifyFinish();
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int i, int i1, int i2) {
        mHolder = surfaceHolder;
        Log.d(TAG, "surfaceChanged, mHolder= " + mHolder);
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {
        mHolder = null;
        Log.d(TAG, "surfaceDestroyed, mHolder= " + mHolder);
    }
}
