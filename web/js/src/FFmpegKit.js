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

import {parseArguments} from './Arguments.js';
import {SessionType} from './Constants.js';
import {FFmpegKitConfig} from './FFmpegKitConfig.js';
import {getFactory} from './FFmpegKitFactory.js';
import {FFmpegSession} from './FFmpegSession.js';

/**
 * Main entry point for running FFmpeg commands in the web worker.
 *
 * Commands execute inside the FFmpegKit worker. Methods that accept a command
 * string parse it into arguments using FFmpegKitConfig.parseArguments(); use
 * the argument-array methods when arguments must be passed losslessly.
 *
 * Every method here is async, as on the native platforms, so a bad argument rejects
 * the returned promise instead of throwing out of the call and slipping past a
 * caller's .catch().
 *
 * After FFmpegKitConfig.shutdown() executions fail with "FFmpegKit has been shut down"
 * without running or invoking completion callbacks. Sessions created on the page for
 * rejection have no native session id and are not added to history.
 */
export class FFmpegKit {
    /**
     * Runs an FFmpeg command to completion.
     *
     * On web the public promise waits for completion, while native FFmpeg runs
     * asynchronously inside the worker so cancel and live-progress messages can
     * still be processed.
     *
     * @param {string} command Command string to parse and execute.
     * @returns {Promise<FFmpegSession>} a populated session after the command reaches COMPLETED or FAILED.
     */
    static async execute(command) {
        return FFmpegKit.executeWithArguments(parseArguments(command));
    }

    /**
     * Runs an FFmpeg command to completion using pre-split arguments.
     *
     * On web the public promise waits for completion, while native FFmpeg runs
     * asynchronously inside the worker so cancel and live-progress messages can
     * still be processed.
     *
     * @param {string[]} commandArguments Command arguments passed without string parsing.
     * @returns {Promise<FFmpegSession>} a populated session after the command reaches COMPLETED or FAILED.
     */
    static async executeWithArguments(commandArguments) {
        if (FFmpegKitConfig.isShutdown()) {
            return getFactory().rejectedSession(SessionType.FFMPEG, commandArguments);
        }
        const session = await FFmpegSession.create(commandArguments);
        await FFmpegKitConfig.ffmpegExecute(session);
        return session;
    }

    /**
     * Starts an asynchronous FFmpeg command.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion. Use completeCallback to observe the final
     * populated session. logCallback and statisticsCallback receive live events
     * while the command runs.
     *
     * @param {string} command Command string to parse and execute.
     * @param {?FFmpegSessionCompleteCallback} completeCallback Optional completion callback.
     * @param {?LogCallback} logCallback Optional per-session log callback.
     * @param {?StatisticsCallback} statisticsCallback Optional per-session statistics callback.
     * @returns {Promise<FFmpegSession>} the session after the request is posted to the worker.
     */
    static async executeAsync(
        command,
        completeCallback = null,
        logCallback = null,
        statisticsCallback = null
    ) {
        return FFmpegKit.executeWithArgumentsAsync(
            parseArguments(command),
            completeCallback,
            logCallback,
            statisticsCallback
        );
    }

    /**
     * Starts an asynchronous FFmpeg command using pre-split arguments.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion.
     *
     * @param {string[]} commandArguments Command arguments passed without string parsing.
     * @param {?FFmpegSessionCompleteCallback} completeCallback Optional completion callback.
     * @param {?LogCallback} logCallback Optional per-session log callback.
     * @param {?StatisticsCallback} statisticsCallback Optional per-session statistics callback.
     * @returns {Promise<FFmpegSession>} the session after the request is posted to the worker.
     */
    static async executeWithArgumentsAsync(
        commandArguments,
        completeCallback = null,
        logCallback = null,
        statisticsCallback = null
    ) {
        if (FFmpegKitConfig.isShutdown()) {
            return getFactory().rejectedSession(SessionType.FFMPEG, commandArguments, {completeCallback, logCallback, statisticsCallback}, true);
        }
        const session = await FFmpegSession.create(
            commandArguments,
            completeCallback,
            logCallback,
            statisticsCallback
        );
        await FFmpegKitConfig.asyncFFmpegExecute(session);
        return session;
    }

    /**
     * Cancels a running session, or all ongoing sessions when no session id is
     * provided.
     *
     * On web, cancel requests are processed by the worker event loop and can
     * target both execute/executeWithArguments and executeAsync/executeWithArgumentsAsync
     * while native FFmpeg is still running.
     *
     * @param {?number} sessionId Optional session id to cancel.
     * @returns {Promise<void>} resolves after the cancel request is sent.
     */
    static async cancel(sessionId = null) {
        return getFactory().cancel(sessionId);
    }

    /**
     * @returns {Promise<FFmpegSession[]>} all FFmpeg sessions still in the native session history.
     */
    static async listSessions() {
        return getFactory().listFFmpegSessions();
    }
}
