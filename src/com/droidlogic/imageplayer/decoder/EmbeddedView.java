package com.droidlogic.imageplayer.decoder;

import android.content.Context;
import android.graphics.Color;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Gravity;
import android.view.SurfaceControlViewHost;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

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
    private TextView mInfoView;
    private TextView mNetLoadingStateView;

    private Handler mUiHandler;

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
        mUiHandler = new Handler(Looper.getMainLooper());
        setBackgroundColor(Color.BLACK);
        setFocusable(false);
    }

    private void runOnUiThread(Runnable r) {
        if (Looper.myLooper() != Looper.getMainLooper()) {
            mUiHandler.post(r);
        } else {
            r.run();
        }
    }

    public void mountTo(SurfaceView parent, Runnable finished, int width, int height) {
        Log.d(TAG, "mountTo: " + parent);
        mHost = new SurfaceControlViewHost(mContext, mContext.getDisplay(), parent.getHostToken());
        parent.setChildSurfacePackage(Objects.requireNonNull(mHost.getSurfacePackage()));

        addSurfaceView();

        mHost.setView(this, width, height);

        mMountFinished = finished;
        notifyFinish();
    }

    private void addSurfaceView() {
        mSurfaceView = new SurfaceView(mContext);
        mSurfaceView.getHolder().addCallback(this);

        addView(mSurfaceView, new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT,
                Gravity.CENTER_HORIZONTAL | Gravity.TOP));
    }

    private void addOsdView() {
        mOsdImageView = new OsdImageView(mContext);
        addView(mOsdImageView, new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT,
                Gravity.CENTER_HORIZONTAL | Gravity.TOP));

        /*Default show SurfaceView*/
        flipView(false);
    }

    private void addNetLoadingStateView() {
        if (mNetLoadingStateView != null) {
            return;
        }

        mNetLoadingStateView = new TextView(mContext);
        mNetLoadingStateView.setBackgroundColor(Color.argb(155, 0, 0, 0));
        mNetLoadingStateView.setTextSize(15);
        mNetLoadingStateView.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        mNetLoadingStateView.setTextColor(Color.WHITE);
        mNetLoadingStateView.setGravity(Gravity.START);
        mNetLoadingStateView.setPadding(20, 20, 20, 20);
        LayoutParams params = new LayoutParams(
                LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT, Gravity.CENTER);
        params.rightMargin = params.topMargin = 10;
        addView(mNetLoadingStateView, params);
        mNetLoadingStateView.setVisibility(View.GONE);
    }

    private void addInfoView() {
        if (mInfoView != null) {
            return;
        }

        mInfoView = new TextView(mContext);
        mInfoView.setBackgroundColor(Color.argb(155, 0, 0, 0));
        mInfoView.setTextSize(15);
        mInfoView.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        mInfoView.setTextColor(Color.WHITE);
        mInfoView.setGravity(Gravity.START);
        mInfoView.setPadding(20, 20, 20, 20);
        LayoutParams params = new LayoutParams(
                LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT,
                Gravity.START | Gravity.TOP);
        params.leftMargin = params.topMargin = 10;
        addView(mInfoView, params);
        mInfoView.setVisibility(View.GONE);
    }

    public void flipView(boolean toOsd) {
        runOnUiThread(()->{
            if (toOsd) {
                mOsdImageView.setVisibility(View.VISIBLE);
            } else {
                mOsdImageView.setVisibility(View.INVISIBLE);
            }
        });
    }

    public void setInfo(String info) {
        runOnUiThread(()->{
            if (mInfoView != null && (mInfoView.getVisibility() == View.VISIBLE)) {
                mInfoView.setText(info);
            }
        });
    }

    public void setNetImageState(String state) {
        runOnUiThread(()->{
            if (mNetLoadingStateView != null && (mNetLoadingStateView.getVisibility() == View.VISIBLE)) {
                mNetLoadingStateView.setText(state);
            }
        });
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
        addOsdView();
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

    public void showInfoWindow(boolean show) {
        runOnUiThread(()->{
            if (show) {
                addInfoView();
            }

            if (mInfoView != null) {
                mInfoView.setVisibility(show ? View.VISIBLE : View.GONE);
            }
        });
    }

    public void showNetImageStateWindow(boolean show) {
        runOnUiThread(()->{
            if (show) {
                addNetLoadingStateView();
            }

            if (mNetLoadingStateView != null) {
                mNetLoadingStateView.setVisibility(show ? View.VISIBLE : View.GONE);
            }
        });
    }
}
