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

package com.arthenica.ffmpegkit

import java.util.concurrent.locks.Condition
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

/**
 * Library state across shutdown: running, terminating, terminated, one-way. Admissions, counters and
 * transitions share one lock, so the execute methods, sessions and asynchronous tasks see
 * every transition at the same point.
 */
internal object LibraryState {

    const val RUNNING = 0
    const val TERMINATING = 1
    const val TERMINATED = 2

    const val SHUTDOWN_REASON = "FFmpegKit has been shut down"

    @Volatile
    var state = RUNNING

    val lock = ReentrantLock()
    val changed: Condition = lock.newCondition()

    /** Running executions, counted from their claim until their completion callbacks return. */
    var pendingExecutions = 0

    /** Tasks submitted by the library that have not claimed their sessions; counted during grace only. */
    var queuedExecutionCount = 0

    /** Callbacks already admitted, which may finish after the cutoff. */
    var activeCallbacks = 0

    var cleanupPending = false

    /** System.nanoTime() at which completion grace ends, fixed by the first call. */
    var graceDeadline = 0L

    /** Prevents self-wait in executions, callbacks and history cleanup. */
    private val workDepth = object : ThreadLocal<IntArray>() {
        override fun initialValue() = IntArray(1)
    }

    fun isLibraryWork(): Boolean = workDepth.get()!![0] > 0

    fun enterWork() {
        workDepth.get()!![0]++
    }

    fun leaveWork() {
        workDepth.get()!![0]--
    }

    /** Counts an execution unless shutdown began. */
    fun beginExecution(): Boolean = lock.withLock {
        val admitted = state == RUNNING
        if (admitted) {
            pendingExecutions++
            enterWork()
        }
        admitted
    }

    /** Ends a counted execution, after its session was released. */
    fun endExecution() = lock.withLock {
        pendingExecutions--
        changed.signalAll()
        leaveWork()
    }

    /** Admits each callback invocation separately, atomically with the shutdown transition. */
    fun beginCallback(completion: Boolean): Boolean = lock.withLock {
        val current = state
        val admitted = current == RUNNING ||
            (completion && current == TERMINATING && System.nanoTime() - graceDeadline < 0)
        if (admitted) {
            activeCallbacks++
            enterWork()
        }
        admitted
    }

    fun endCallback() = lock.withLock {
        activeCallbacks--
        changed.signalAll()
        leaveWork()
    }

    /** Counts a task the library is about to submit, unless shutdown began. */
    fun acceptSubmission(): Boolean = lock.withLock {
        val accepted = state == RUNNING
        if (accepted) {
            queuedExecutionCount++
        }
        accepted
    }

    /** Uncounts a task whose submission failed. */
    fun abandonSubmission() = lock.withLock {
        queuedExecutionCount--
        changed.signalAll()
    }

    /**
     * Claims a task's session when its executor runs it. The execution is counted unless the
     * session is claimed after termination.
     *
     * @param queued whether the library counted the task as queued when it submitted it
     * @return the state the session was claimed in
     */
    fun claim(queued: Boolean): Int = lock.withLock {
        if (queued) {
            queuedExecutionCount--
        }
        val current = state
        if (current != TERMINATED) {
            pendingExecutions++
            enterWork()
        }
        changed.signalAll()
        current
    }

    /** Fails a session that shutdown does not let run. */
    fun reject(session: AbstractSession) {
        session.fail(IllegalStateException(SHUTDOWN_REASON))
    }
}
