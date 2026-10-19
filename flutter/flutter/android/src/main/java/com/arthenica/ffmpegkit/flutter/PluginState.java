/*
 * Copyright (c) 2026 Taner Sener
 *
 * This file is part of FFmpegKitNext.
 *
 * FFmpegKitNext is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FFmpegKitNext is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General License for more details.
 *
 * You should have received a copy of the GNU Lesser General License
 * along with FFmpegKitNext. If not, see <http://www.gnu.org/licenses/>.
 */

package com.arthenica.ffmpegkit.flutter;

import android.util.Log;
import com.arthenica.ffmpegkit.Session;
import com.arthenica.ffmpegkit.SessionState;
import java.util.HashMap;
import java.util.Map;

import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Executor;

import io.flutter.plugin.common.MethodChannel;

/** Admission and ownership for one attachment to a Flutter engine. */
final class PluginState {
    private final Set<Work> work = Collections.newSetFromMap(new IdentityHashMap<>());
    private final Set<PendingResult> replies = Collections.newSetFromMap(new IdentityHashMap<>());
    private boolean stopping;
    private final Map<Long, Session> asyncSessions = new HashMap<>();

    synchronized boolean isAttached() { return !stopping; }

    synchronized MethodChannel.Result track(MethodChannel.Result result) {
        if (stopping)
            return null;
        final PendingResult pending = new PendingResult(result);
        replies.add(pending);
        return pending;
    }

    boolean submit(Executor executor, Runnable action, Runnable cancel) {
        final Work admitted;
        synchronized (this) {
            if (stopping)
                return false;
            admitted = new Work(action, cancel);
            work.add(admitted);
        }
        try {
            executor.execute(admitted);
            return true;
        } catch (RuntimeException error) {
            admitted.finish();
            throw error;
        }
    }

    synchronized boolean trackAsync(Session session) {
        if (stopping)
            return false;
        pruneAsync();
        asyncSessions.put(session.getSessionId(), session);
        return true;
    }

    synchronized void finishAsync(long id) { asyncSessions.remove(id); }

    synchronized void pruneAsync() { asyncSessions.values().removeIf(session -> session.getState() == SessionState.COMPLETED || session.getState() == SessionState.FAILED); }

    /** Called on the platform thread before removing channel handlers. */
    void beginShutdown() {
        final List<PendingResult> pending;
        final List<Session> ownedAsync;
        synchronized (this) {
            if (stopping)
                return;
            stopping = true;
            pruneAsync();
            ownedAsync = new ArrayList<>(asyncSessions.values());
            asyncSessions.clear();
            pending = new ArrayList<>(replies);
            for (Work admitted : new ArrayList<>(work)) {
                if (!admitted.running)
                    admitted.finish();
            }
        }
        for (Session session : ownedAsync) {
            try {
                session.cancel();
            } catch (RuntimeException error) {
                Log.w(FFmpegKitFlutterPlugin.LIBRARY_NAME, "Failed to cancel plugin session.", error);
            }
        }
        for (PendingResult result : pending) {
            try {
                result.error("PLUGIN_SHUTDOWN", "The Flutter plugin has been detached.", null);
            } catch (RuntimeException error) {
                Log.w(FFmpegKitFlutterPlugin.LIBRARY_NAME, "Failed to deliver plugin detachment reply.", error);
            }
        }
    }

    /** Drain without blocking the platform queue that workers may need. */
    void cancelAndDrain() {
        final Thread drain = new Thread(() -> {
            boolean interrupted = false;
            for (;;) {
                final List<Runnable> cancellation = new ArrayList<>();
                synchronized (PluginState.this) {
                    if (work.isEmpty())
                        break;
                    for (Work admitted : work) {
                        if (admitted.cancel != null)
                            cancellation.add(admitted.cancel);
                    }
                }
                // Native registration can overwrite an early cancellation.
                // Retry against strong active handles, never historical IDs.
                for (Runnable cancel : cancellation) {
                    try {
                        cancel.run();
                    } catch (RuntimeException error) {
                        Log.w(FFmpegKitFlutterPlugin.LIBRARY_NAME, "Failed to cancel plugin execution.", error);
                    }
                }
                synchronized (PluginState.this) {
                    if (work.isEmpty())
                        break;
                    try {
                        PluginState.this.wait(10);
                    } catch (InterruptedException ignored) {
                        interrupted = true;
                    }
                }
            }
            if (interrupted)
                Thread.currentThread().interrupt();
        }, "ffmpeg-kit-flutter-detach");
        drain.setDaemon(true);
        drain.start();
    }

    private final class Work implements Runnable {
        private Runnable action;
        private Runnable cancel;
        private boolean running;

        Work(Runnable action, Runnable cancel) {
            this.action = action;
            this.cancel = cancel;
        }

        @Override
        public void run() {
            final Runnable claimed;
            synchronized (PluginState.this) {
                if (stopping || action == null) {
                    finish();
                    return;
                }
                running = true;
                claimed = action;
                action = null;
            }
            try {
                claimed.run();
            } finally {
                finish();
            }
        }

        void finish() {
            synchronized (PluginState.this) {
                action = null;
                cancel = null;
                work.remove(this);
                PluginState.this.notifyAll();
            }
        }
    }

    private final class PendingResult implements MethodChannel.Result {
        private MethodChannel.Result result;

        PendingResult(MethodChannel.Result result) { this.result = result; }

        private MethodChannel.Result take() {
            synchronized (PluginState.this) {
                final MethodChannel.Result delivery = result;
                result = null;
                replies.remove(this);
                return delivery;
            }
        }

        @Override
        public void success(Object value) {
            final MethodChannel.Result delivery = take();
            if (delivery != null)
                delivery.success(value);
        }

        @Override
        public void error(String code, String message, Object details) {
            final MethodChannel.Result delivery = take();
            if (delivery != null)
                delivery.error(code, message, details);
        }

        @Override
        public void notImplemented() {
            final MethodChannel.Result delivery = take();
            if (delivery != null)
                delivery.notImplemented();
        }
    }
}
