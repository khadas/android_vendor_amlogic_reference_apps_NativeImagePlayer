package com.droidlogic.imageplayer.decoder;

import android.os.Handler;
import android.util.Log;

import java.util.function.Consumer;

public class RetryUtil {
    public static interface ConditionOk {
        boolean get();
    }

    public static void retryIf(final String name, final Handler handler, final Runnable r, final ConditionOk cond) {
        retryIf(name, handler, r, cond, -1);
    }

    public static void retryIf(final String name, final Handler handler, final Runnable r, final ConditionOk cond, final long maxDelayed) {
        final long delayOnceMs = 20;
        handler.post(new Runnable() {
            private long mTotalDelayedMs = 0;

            @Override
            public void run() {
                mTotalDelayedMs += delayOnceMs;

                if (cond.get()) {
                    if (maxDelayed != -1 && mTotalDelayedMs > maxDelayed) {
                        Log.e(name, "Waiting for ready" +
                                "too long time: " + maxDelayed + ", top it");
                    } else {
                        Log.d(name,
                                "retry after " + delayOnceMs + "ms");
                        handler.postDelayed(this, delayOnceMs);
                    }
                    return;
                }

                r.run();
            }
        });
    }
}
