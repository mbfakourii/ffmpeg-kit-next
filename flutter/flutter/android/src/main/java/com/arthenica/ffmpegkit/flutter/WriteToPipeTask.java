/*
 * Copyright (c) 2021, 2026 Taner Sener
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

import static com.arthenica.ffmpegkit.flutter.FFmpegKitFlutterPlugin.LIBRARY_NAME;

import android.util.Log;

import androidx.annotation.NonNull;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import io.flutter.plugin.common.MethodChannel;

public class WriteToPipeTask implements Runnable {
    private final String inputPath;
    private final String namedPipePath;
    private final FFmpegKitFlutterMethodResultHandler resultHandler;
    private final MethodChannel.Result result;
    private final AtomicBoolean cancelled = new AtomicBoolean();
    private final AtomicReference<Process> activeProcess = new AtomicReference<>();

    public WriteToPipeTask(@NonNull final String inputPath, @NonNull final String namedPipePath, @NonNull final FFmpegKitFlutterMethodResultHandler resultHandler, @NonNull final MethodChannel.Result result) {
        this.inputPath = inputPath;
        this.namedPipePath = namedPipePath;
        this.resultHandler = resultHandler;
        this.result = result;
    }

    @Override
    public void run() {
        final int rc;

        try {
            if (cancelled.get()) {
                resultHandler.errorAsync(result, "PLUGIN_SHUTDOWN", "The Flutter plugin has been detached.");
                return;
            }
            Log.d(LIBRARY_NAME, String.format("Starting copy %s to pipe %s operation.", inputPath, namedPipePath));

            final long startTime = System.currentTimeMillis();

            // exec keeps the shell and cat in one process, including while the
            // shell waits for a FIFO reader. Pass paths as arguments.
            final Process process = Runtime.getRuntime().exec(new String[] {"sh", "-c", "exec cat \"$1\" > \"$2\"", "ffmpeg-kit-flutter", inputPath, namedPipePath});
            activeProcess.set(process);
            if (cancelled.get())
                process.destroy();
            rc = process.waitFor();

            final long endTime = System.currentTimeMillis();

            Log.d(LIBRARY_NAME, String.format("Copying %s to pipe %s operation completed with rc %d in %d seconds.", inputPath, namedPipePath, rc, (endTime - startTime) / 1000));

            resultHandler.successAsync(result, rc);

        } catch (final IOException | InterruptedException e) {
            if (e instanceof InterruptedException)
                Thread.currentThread().interrupt();
            Log.e(LIBRARY_NAME, String.format("Copy %s to pipe %s failed with error.", inputPath, namedPipePath), e);
            resultHandler.errorAsync(result, "WRITE_TO_PIPE_FAILED", e.getMessage());
        } finally {
            final Process process = activeProcess.getAndSet(null);
            if (process != null)
                process.destroy();
        }
    }

    void cancel() {
        cancelled.set(true);
        final Process process = activeProcess.get();
        if (process != null)
            process.destroy();
    }
}
