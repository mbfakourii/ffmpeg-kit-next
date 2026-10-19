/*
 * Copyright (c) 2018-2021, 2026 Taner Sener
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

package com.arthenica.ffmpegkit

import com.arthenica.smartexception.java.Exceptions

/**
 * <p>Executes an FFmpeg session asynchronously.
 *
 * <p>The task claims its session when its executor runs it. Claimed before shutdown, the session
 * runs; claimed during shutdown grace, it fails with the reason "FFmpegKit has been shut down"
 * without native execution and its completion callbacks may still run; claimed later, it fails
 * and its completion callbacks are dropped. The task releases its session before the execution
 * stops being counted, so an executor that keeps the task does not keep the session.
 */
open class AsyncFFmpegExecuteTask(ffmpegSession: FFmpegSession) : Runnable {

    /** Moved out when the task runs, so an executor that keeps the task does not keep the session. */
    private var ffmpegSession: FFmpegSession? = ffmpegSession

    /** Set when FFmpegKitConfig counted this task as queued when it submitted it. */
    internal var queued = false

    override fun run() {
        val state = LibraryState.claim(queued)
        try {
            runClaimed(state)
        } finally {
            if (state != LibraryState.TERMINATED) {
                LibraryState.endExecution()
            }
        }
    }

    /** Runs or rejects the claimed session; the session is released when this returns. */
    private fun runClaimed(state: Int) {
        val session = ffmpegSession ?: return
        ffmpegSession = null

        if (state == LibraryState.RUNNING) {
            FFmpegKitConfig.runFFmpegSession(session)
        } else {
            LibraryState.reject(session)
        }
        if (state != LibraryState.TERMINATED) {
            notifyComplete(session)
        }
    }

    /** Calls the session's complete callback, then the global one, each admitted separately. */
    private fun notifyComplete(session: FFmpegSession) {
        val completeCallback = session.getCompleteCallback()
        if (completeCallback != null && LibraryState.beginCallback(true)) {
            try {
                // NOTIFY SESSION CALLBACK DEFINED
                completeCallback.apply(session)
            } catch (e: Exception) {
                android.util.Log.e(FFmpegKitConfig.TAG, String.format("Exception thrown inside session complete callback.%s", Exceptions.getStackTraceString(e)))
            } finally {
                LibraryState.endCallback()
            }
        }

        val globalFFmpegSessionCompleteCallback = FFmpegKitConfig.getFFmpegSessionCompleteCallback()
        if (globalFFmpegSessionCompleteCallback != null && LibraryState.beginCallback(true)) {
            try {
                // NOTIFY GLOBAL CALLBACK DEFINED
                globalFFmpegSessionCompleteCallback.apply(session)
            } catch (e: Exception) {
                android.util.Log.e(FFmpegKitConfig.TAG, String.format("Exception thrown inside global complete callback.%s", Exceptions.getStackTraceString(e)))
            } finally {
                LibraryState.endCallback()
            }
        }
    }
}
