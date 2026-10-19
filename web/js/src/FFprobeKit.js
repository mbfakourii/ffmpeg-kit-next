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
import {FFprobeSession} from './FFprobeSession.js';
import {MediaInformationSession} from './MediaInformationSession.js';

function mediaInformationCommandArguments(path) {
    return [
        '-v',
        'error',
        '-hide_banner',
        '-print_format',
        'json',
        '-show_format',
        '-show_streams',
        '-show_chapters',
        '-i',
        path,
    ];
}

/**
 * Main entry point for running FFprobe commands and extracting media information.
 *
 * Commands execute inside the FFmpegKit worker. Methods that accept a command
 * string parse it into arguments using FFmpegKitConfig.parseArguments(); use
 * the argument-array methods when arguments must be passed losslessly.
 *
 * After FFmpegKitConfig.shutdown() executions fail with "FFmpegKit has been shut down"
 * without running or invoking completion callbacks. Sessions created on the page for
 * rejection have no native session id and are not added to history.
 */
export class FFprobeKit {
    /**
     * Runs an FFprobe command to completion.
     *
     * @param {string} command Command string to parse and execute.
     * @returns {Promise<FFprobeSession>} a populated session after the command reaches COMPLETED or FAILED.
     */
    static async execute(command) {
        return FFprobeKit.executeWithArguments(parseArguments(command));
    }

    /**
     * Runs an FFprobe command to completion using pre-split arguments.
     *
     * @param {string[]} commandArguments Command arguments passed without string parsing.
     * @returns {Promise<FFprobeSession>} a populated session after the command reaches COMPLETED or FAILED.
     */
    static async executeWithArguments(commandArguments) {
        if (FFmpegKitConfig.isShutdown()) {
            return getFactory().rejectedSession(SessionType.FFPROBE, commandArguments);
        }
        const session = await FFprobeSession.create(commandArguments);
        await FFmpegKitConfig.ffprobeExecute(session);
        return session;
    }

    /**
     * Starts an asynchronous FFprobe command.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion. Use completeCallback to observe the final
     * populated session. logCallback receives live log events while the command
     * runs.
     *
     * @param {string} command Command string to parse and execute.
     * @param {?FFprobeSessionCompleteCallback} completeCallback Optional completion callback.
     * @param {?LogCallback} logCallback Optional per-session log callback.
     * @returns {Promise<FFprobeSession>} the session after the request is posted to the worker.
     */
    static async executeAsync(command, completeCallback = null, logCallback = null) {
        return FFprobeKit.executeWithArgumentsAsync(
            parseArguments(command),
            completeCallback,
            logCallback
        );
    }

    /**
     * Starts an asynchronous FFprobe command using pre-split arguments.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion.
     *
     * @param {string[]} commandArguments Command arguments passed without string parsing.
     * @param {?FFprobeSessionCompleteCallback} completeCallback Optional completion callback.
     * @param {?LogCallback} logCallback Optional per-session log callback.
     * @returns {Promise<FFprobeSession>} the session after the request is posted to the worker.
     */
    static async executeWithArgumentsAsync(commandArguments, completeCallback = null, logCallback = null) {
        if (FFmpegKitConfig.isShutdown()) {
            return getFactory().rejectedSession(SessionType.FFPROBE, commandArguments, {completeCallback, logCallback}, true);
        }
        const session = await FFprobeSession.create(
            commandArguments,
            completeCallback,
            logCallback
        );
        await FFmpegKitConfig.asyncFFprobeExecute(session);
        return session;
    }

    /**
     * Extracts media information for a path already present in the virtual filesystem.
     *
     * @param {string} path Input path in MEMFS, WORKERFS, or another path visible to FFprobe.
     * @param {?number} waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns {Promise<MediaInformationSession>} a session populated with parsed media information when available.
     */
    static async getMediaInformation(path, waitTimeout = null) {
        return FFprobeKit.getMediaInformationFromCommandArguments(
            mediaInformationCommandArguments(path),
            waitTimeout
        );
    }

    /**
     * Starts asynchronous media-information extraction for a path.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion.
     *
     * @param {string} path Input path in MEMFS, WORKERFS, or another path visible to FFprobe.
     * @param {?MediaInformationSessionCompleteCallback} completeCallback Optional completion callback.
     * @param {?LogCallback} logCallback Optional per-session log callback.
     * @param {?number} waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns {Promise<MediaInformationSession>} the session after the request is posted to the worker.
     */
    static async getMediaInformationAsync(
        path,
        completeCallback = null,
        logCallback = null,
        waitTimeout = null
    ) {
        return FFprobeKit.getMediaInformationFromCommandArgumentsAsync(
            mediaInformationCommandArguments(path),
            completeCallback,
            logCallback,
            waitTimeout
        );
    }

    /**
     * Extracts media information using a custom command that must emit JSON.
     *
     * @param {string} command FFprobe command string that emits media information JSON.
     * @param {?number} waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns {Promise<MediaInformationSession>} a session populated with parsed media information when available.
     */
    static async getMediaInformationFromCommand(command, waitTimeout = null) {
        return FFprobeKit.getMediaInformationFromCommandArguments(
            parseArguments(command),
            waitTimeout
        );
    }

    /**
     * Extracts media information using pre-split command arguments that must emit JSON.
     *
     * @param {string[]} commandArguments FFprobe command arguments passed without string parsing.
     * @param {?number} waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns {Promise<MediaInformationSession>} a session populated with parsed media information when available.
     */
    static async getMediaInformationFromCommandArguments(commandArguments, waitTimeout = null) {
        if (FFmpegKitConfig.isShutdown()) {
            return getFactory().rejectedSession(SessionType.MEDIA_INFORMATION, commandArguments);
        }
        const session = await MediaInformationSession.create(commandArguments);
        await FFmpegKitConfig.getMediaInformationExecute(session, waitTimeout);
        return session;
    }

    /**
     * Starts asynchronous media-information extraction using a custom JSON command.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion.
     *
     * @param {string} command FFprobe command string that emits media information JSON.
     * @param {?MediaInformationSessionCompleteCallback} completeCallback Optional completion callback.
     * @param {?LogCallback} logCallback Optional per-session log callback.
     * @param {?number} waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns {Promise<MediaInformationSession>} the session after the request is posted to the worker.
     */
    static async getMediaInformationFromCommandAsync(
        command,
        completeCallback = null,
        logCallback = null,
        waitTimeout = null
    ) {
        return FFprobeKit.getMediaInformationFromCommandArgumentsAsync(
            parseArguments(command),
            completeCallback,
            logCallback,
            waitTimeout
        );
    }

    /**
     * Starts asynchronous media-information extraction using pre-split JSON command arguments.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion.
     *
     * @param {string[]} commandArguments FFprobe command arguments passed without string parsing.
     * @param {?MediaInformationSessionCompleteCallback} completeCallback Optional completion callback.
     * @param {?LogCallback} logCallback Optional per-session log callback.
     * @param {?number} waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns {Promise<MediaInformationSession>} the session after the request is posted to the worker.
     */
    static async getMediaInformationFromCommandArgumentsAsync(
        commandArguments,
        completeCallback = null,
        logCallback = null,
        waitTimeout = null
    ) {
        if (FFmpegKitConfig.isShutdown()) {
            return getFactory().rejectedSession(SessionType.MEDIA_INFORMATION, commandArguments, {completeCallback, logCallback}, true);
        }
        const session = await MediaInformationSession.create(
            commandArguments,
            completeCallback,
            logCallback
        );
        await FFmpegKitConfig.asyncGetMediaInformationExecute(session, waitTimeout);
        return session;
    }

    /**
     * @returns {Promise<FFprobeSession[]>} all FFprobe sessions still in the native session history.
     */
    static async listFFprobeSessions() {
        return getFactory().listFFprobeSessions();
    }

    /**
     * @returns {Promise<MediaInformationSession[]>} all media-information sessions still in the native session history.
     */
    static async listMediaInformationSessions() {
        return getFactory().listMediaInformationSessions();
    }
}
