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

#ifndef FFMPEG_KIT_CONFIG_H
#define FFMPEG_KIT_CONFIG_H

#import "FFmpegSession.h"
#import "FFprobeSession.h"
#import "LogCallback.h"
#import "MediaInformationSession.h"
#import "StatisticsCallback.h"
#import <Foundation/Foundation.h>
#import <pthread.h>
#import <stdio.h>
#import <unistd.h>

/** Global library version */
extern NSString *const FFmpegKitVersion;

typedef NS_ENUM(NSUInteger, Signal) {
    SignalInt = 2,
    SignalQuit = 3,
    SignalPipe = 13,
    SignalTerm = 15,
    SignalXcpu = 24
};

/**
 * Listener notified when a session is deleted from the native session history.
 */
@protocol SessionDeleteListener <NSObject>

/**
 * Called after the session identified by sessionId has been deleted from
 * session history.
 *
 * @param sessionId session identifier
 */
- (void)sessionDeleted:(long)sessionId;

@end

/**
 * <p>Configuration class of <code>FFmpegKit</code> library. Allows customizing
 * the global library options. Provides helper methods to support additional
 * resources.
 */
@interface FFmpegKitConfig : NSObject

/**
 * <p>Enables log and statistics redirection.
 *
 * <p>When redirection is enabled FFmpeg/FFprobe logs are redirected to NSLog
 * and sessions collect log and statistics entries for the executions. It is
 * possible to define global or session specific log/statistics callbacks as
 * well.
 *
 * <p>Note that redirection is enabled by default. If you do not want to use its
 * functionality please use disableRedirection method to disable it.
 *
 * <p>Does nothing after shutdownWithTimeout:.
 */
+ (void)enableRedirection;

/**
 * <p>Disables log and statistics redirection.
 *
 * <p>When redirection is disabled logs are printed to stderr, all logs and
 * statistics callbacks are disabled and <code>FFprobe</code>'s
 * <code>getMediaInformation</code> methods do not work.
 */
+ (void)disableRedirection;

/**
 * <p>Permanently shuts down FFmpegKit: requests FFmpeg cancellation, closes
 * streams, disables redirection and drops pending messages and delete
 * notifications. New executions fail without completion callbacks. Running
 * FFprobe and media information executions cannot be cancelled.
 *
 * <p>Within waitTimeout, the first call allows up to two seconds of completion
 * callback grace, then up to one second before clearing history. Queued blocks
 * stay on their queues and fail without execution when served; their callbacks
 * may run during grace and are dropped afterwards. Admitted callbacks may continue.
 * Call again after NO to wait longer; retries never restart grace or callbacks.
 *
 * <p>In-flight results, full output and callbacks are not guaranteed. For
 * predictable results, cancel and wait for execution and output delivery first.
 * Even after YES, queued blocks and handles may retain sessions and invoke
 * cleanup functions later; keep their code and state valid until released.
 * Stop submissions and stream writers first. Do not wait while holding a lock
 * or thread that active work needs.
 *
 * @param waitTimeout total wait budget in milliseconds; zero does not wait,
 * a negative value waits indefinitely for counted work and history cleanup
 * @return YES when running executions, admitted callbacks, message delivery
 * and history cleanup have finished; NO at the deadline or from counted library
 * work that cannot wait for itself. Queued blocks are excluded after grace.
 */
+ (BOOL)shutdownWithTimeout:(int)waitTimeout;

/**
 * <p>Returns whether shutdownWithTimeout: was called.
 *
 * @return YES after shutdown, NO before
 */
+ (BOOL)isShutdown;

/**
 * <p>Sets and overrides <code>fontconfig</code> configuration directory.
 *
 * @param path directory that contains fontconfig configuration (fonts.conf)
 * @return zero on success, non-zero on error
 */
+ (int)setFontconfigConfigurationPath:(NSString *)path;

/**
 * <p>Registers the fonts inside the given path, so they become available to use
 * in FFmpeg filters.
 *
 * <p>Note that you need to build <code>FFmpegKit</code> with
 * <code>fontconfig</code> enabled or use a prebuilt package with
 * <code>fontconfig</code> inside to be able to use fonts in
 * <code>FFmpeg</code>.
 *
 * @param fontDirectoryPath directory that contains fonts (.ttf and .otf files)
 * @param fontNameMapping   custom font name mappings, useful to access your
 * fonts with more friendly names
 */
+ (void)setFontDirectory:(NSString *)fontDirectoryPath with:(NSDictionary *)fontNameMapping;

/**
 * <p>Registers the fonts inside the given array of font directories, so they
 * become available to use in FFmpeg filters.
 *
 * <p>Note that you need to build <code>FFmpegKit</code> with
 * <code>fontconfig</code> enabled or use a prebuilt package with
 * <code>fontconfig</code> inside to be able to use fonts in
 * <code>FFmpeg</code>.
 *
 * @param fontDirectoryList array of directories that contain fonts (.ttf and
 * .otf files)
 * @param fontNameMapping   custom font name mappings, useful to access your
 * fonts with more friendly names
 */
+ (void)setFontDirectoryList:(NSArray *)fontDirectoryList with:(NSDictionary *)fontNameMapping;

/**
 * <p>Creates a new named pipe to use in <code>FFmpeg</code> operations.
 *
 * <p>Please note that creator is responsible of closing created pipes.
 *
 * @return the full path of the named pipe
 * @deprecated Named pipes are deprecated. Use FFmpegKit input/output buffers and streams instead.
 */
+ (NSString *)registerNewFFmpegPipe __attribute__((deprecated("Named pipes are deprecated. Use FFmpegKit input/output buffers and streams instead.")));

/**
 * <p>Closes a previously created <code>FFmpeg</code> pipe.
 *
 * @param ffmpegPipePath full path of the FFmpeg pipe
 * @deprecated Named pipes are deprecated. Use FFmpegKit input/output buffers and streams instead.
 */
+ (void)closeFFmpegPipe:(NSString *)ffmpegPipePath __attribute__((deprecated("Named pipes are deprecated. Use FFmpegKit input/output buffers and streams instead.")));

/**
 * <p>Returns the version of FFmpeg bundled within <code>FFmpegKit</code>
 * library.
 *
 * @return the version of FFmpeg
 */
+ (NSString *)getFFmpegVersion;

/**
 * Returns FFmpegKit library version.
 *
 * @return FFmpegKit version
 */
+ (NSString *)getVersion;

/**
 * <p>Returns whether FFmpegKit release is a Long Term Release or not.
 *
 * @return true/yes or false/no
 */
+ (int)isLTSBuild __attribute__((deprecated("Apple platform builds do not have an LTS build concept.")));

/**
 * Returns FFmpegKit library build date.
 *
 * @return FFmpegKit library build date
 */
+ (NSString *)getBuildDate;

/**
 * <p>Sets an environment variable.
 *
 * @param variableName  environment variable name
 * @param variableValue environment variable value
 * @return zero on success, non-zero on error
 */
+ (int)setEnvironmentVariable:(NSString *)variableName value:(NSString *)variableValue;

/**
 * <p>Registers a new ignored signal. Ignored signals are not handled by
 * <code>FFmpegKit</code> library.
 *
 * @param signal signal to be ignored
 */
+ (void)ignoreSignal:(Signal)signal;

/**
 * <p>Synchronously executes the FFmpeg session provided.
 *
 * <p>After shutdownWithTimeout: the session fails with the reason "FFmpegKit
 * has been shut down" and does not run.
 *
 * @param ffmpegSession FFmpeg session which includes command options/arguments
 */
+ (void)ffmpegExecute:(FFmpegSession *)ffmpegSession;

/**
 * <p>Synchronously executes the FFprobe session provided.
 *
 * <p>The session completes when FFprobe returns, without waiting for its output,
 * which arrives as logs, to be delivered. An asynchronous execution calls its
 * complete callbacks at that point. Use the session's getOutput or getAllLogs
 * methods to wait for pending log delivery, subject to their timeouts.
 *
 * <p>After shutdownWithTimeout: the session fails with the reason "FFmpegKit
 * has been shut down" and does not run.
 *
 * @param ffprobeSession FFprobe session which includes command
 * options/arguments
 */
+ (void)ffprobeExecute:(FFprobeSession *)ffprobeSession;

/**
 * <p>Synchronously executes the media information session provided.
 *
 * <p>After shutdownWithTimeout: the session fails with the reason "FFmpegKit
 * has been shut down" and does not run.
 *
 * @param mediaInformationSession media information session which includes
 * command options/arguments
 * @param waitTimeout             max time to wait until media information is
 * transmitted
 */
+ (void)getMediaInformationExecute:(MediaInformationSession *)mediaInformationSession withTimeout:(int)waitTimeout;

/**
 * <p>Starts an asynchronous FFmpeg execution for the given session.
 *
 * <p>Note that this method returns immediately and does not wait the execution
 * to complete. You must use an FFmpegSessionCompleteCallback if you want to be
 * notified about the result.
 *
 * <p>After shutdownWithTimeout: the session fails without running or invoking
 * completion callbacks. A previously queued session fails on its dispatch
 * queue when its block runs; its callbacks may run during shutdown's grace
 * period and are dropped afterwards. See shutdownWithTimeout:.
 *
 * @param ffmpegSession FFmpeg session which includes command options/arguments
 */
+ (void)asyncFFmpegExecute:(FFmpegSession *)ffmpegSession;

/**
 * <p>Starts an asynchronous FFmpeg execution for the given session.
 *
 * <p>Note that this method returns immediately and does not wait the execution
 * to complete. You must use an FFmpegSessionCompleteCallback if you want to be
 * notified about the result.
 *
 * <p>After shutdownWithTimeout: the session fails without running or invoking
 * completion callbacks. A previously queued session fails on its dispatch
 * queue when its block runs; its callbacks may run during shutdown's grace
 * period and are dropped afterwards. See shutdownWithTimeout:.
 *
 * @param ffmpegSession   FFmpeg session which includes command
 * options/arguments
 * @param queue           dispatch queue that will be used to run this
 * asynchronous operation
 */
+ (void)asyncFFmpegExecute:(FFmpegSession *)ffmpegSession onDispatchQueue:(dispatch_queue_t)queue;

/**
 * <p>Starts an asynchronous FFprobe execution for the given session.
 *
 * <p>Note that this method returns immediately and does not wait the execution
 * to complete. You must use an FFprobeSessionCompleteCallback if you want to be
 * notified about the result.
 *
 * <p>After shutdownWithTimeout: the session fails without running or invoking
 * completion callbacks. A previously queued session fails on its dispatch
 * queue when its block runs; its callbacks may run during shutdown's grace
 * period and are dropped afterwards. See shutdownWithTimeout:.
 *
 * @param ffprobeSession FFprobe session which includes command
 * options/arguments
 */
+ (void)asyncFFprobeExecute:(FFprobeSession *)ffprobeSession;

/**
 * <p>Starts an asynchronous FFprobe execution for the given session.
 *
 * <p>Note that this method returns immediately and does not wait the execution
 * to complete. You must use an FFprobeSessionCompleteCallback if you want to be
 * notified about the result.
 *
 * <p>After shutdownWithTimeout: the session fails without running or invoking
 * completion callbacks. A previously queued session fails on its dispatch
 * queue when its block runs; its callbacks may run during shutdown's grace
 * period and are dropped afterwards. See shutdownWithTimeout:.
 *
 * @param ffprobeSession  FFprobe session which includes command
 * options/arguments
 * @param queue           dispatch queue that will be used to run this
 * asynchronous operation
 */
+ (void)asyncFFprobeExecute:(FFprobeSession *)ffprobeSession onDispatchQueue:(dispatch_queue_t)queue;

/**
 * <p>Starts an asynchronous FFprobe execution for the given media information
 * session.
 *
 * <p>Note that this method returns immediately and does not wait the execution
 * to complete. You must use an MediaInformationSessionCompleteCallback if you
 * want to be notified about the result.
 *
 * <p>After shutdownWithTimeout: the session fails without running or invoking
 * completion callbacks. A previously queued session fails on its dispatch
 * queue when its block runs; its callbacks may run during shutdown's grace
 * period and are dropped afterwards. See shutdownWithTimeout:.
 *
 * @param mediaInformationSession media information session which includes
 * command options/arguments
 * @param waitTimeout             max time to wait until media information is
 * transmitted
 */
+ (void)asyncGetMediaInformationExecute:(MediaInformationSession *)mediaInformationSession withTimeout:(int)waitTimeout;

/**
 * <p>Starts an asynchronous FFprobe execution for the given media information
 * session.
 *
 * <p>Note that this method returns immediately and does not wait the execution
 * to complete. You must use an MediaInformationSessionCompleteCallback if you
 * want to be notified about the result.
 *
 * <p>After shutdownWithTimeout: the session fails without running or invoking
 * completion callbacks. A previously queued session fails on its dispatch
 * queue when its block runs; its callbacks may run during shutdown's grace
 * period and are dropped afterwards. See shutdownWithTimeout:.
 *
 * @param mediaInformationSession media information session which includes
 * command options/arguments
 * @param queue           dispatch queue that will be used to run this
 * asynchronous operation
 * @param waitTimeout             max time to wait until media information is
 * transmitted
 */
+ (void)asyncGetMediaInformationExecute:(MediaInformationSession *)mediaInformationSession onDispatchQueue:(dispatch_queue_t)queue withTimeout:(int)waitTimeout;

/**
 * <p>Sets a global log callback to redirect FFmpeg/FFprobe logs.
 *
 * @param logCallback log callback or nil to disable a previously defined log
 * callback
 */
+ (void)enableLogCallback:(LogCallback)logCallback;

/**
 * <p>Sets a global statistics callback to redirect FFmpeg statistics.
 *
 * @param statisticsCallback statistics callback or nil to disable a previously
 * defined statistics callback
 */
+ (void)enableStatisticsCallback:(StatisticsCallback)statisticsCallback;

/**
 * <p>Sets a global FFmpegSessionCompleteCallback to receive execution results
 * for FFmpeg sessions.
 *
 * @param ffmpegSessionCompleteCallback complete callback or nil to disable a
 * previously defined callback
 */
+ (void)enableFFmpegSessionCompleteCallback:(FFmpegSessionCompleteCallback)ffmpegSessionCompleteCallback;

/**
 * <p>Returns the global FFmpegSessionCompleteCallback set.
 *
 * @return global FFmpegSessionCompleteCallback or nil if it is not set
 */
+ (FFmpegSessionCompleteCallback)getFFmpegSessionCompleteCallback;

/**
 * <p>Sets a global FFprobeSessionCompleteCallback to receive execution results
 * for FFprobe sessions.
 *
 * @param ffprobeSessionCompleteCallback complete callback or nil to disable a
 * previously defined callback
 */
+ (void)enableFFprobeSessionCompleteCallback:(FFprobeSessionCompleteCallback)ffprobeSessionCompleteCallback;

/**
 * <p>Returns the global FFprobeSessionCompleteCallback set.
 *
 * @return global FFprobeSessionCompleteCallback or nil if it is not set
 */
+ (FFprobeSessionCompleteCallback)getFFprobeSessionCompleteCallback;

/**
 * <p>Sets a global MediaInformationSessionCompleteCallback to receive execution
 * results for MediaInformation sessions.
 *
 * @param mediaInformationSessionCompleteCallback complete callback or nil to
 * disable a previously defined callback
 */
+ (void)enableMediaInformationSessionCompleteCallback:(MediaInformationSessionCompleteCallback)mediaInformationSessionCompleteCallback;

/**
 * <p>Returns the global MediaInformationSessionCompleteCallback set.
 *
 * @return global MediaInformationSessionCompleteCallback or nil if it is not
 * set
 */
+ (MediaInformationSessionCompleteCallback)getMediaInformationSessionCompleteCallback;

/**
 * Returns the current log level.
 *
 * @return current log level
 */
+ (int)getLogLevel;

/**
 * Sets the log level.
 *
 * @param level new log level
 */
+ (void)setLogLevel:(int)level;

/**
 * Converts int log level to string.
 *
 * @param level value
 * @return string value
 */
+ (NSString *)logLevelToString:(int)level;

/**
 * Returns the session history size.
 *
 * @return session history size
 */
+ (int)getSessionHistorySize;

/**
 * Sets the session history size.
 *
 * @param sessionHistorySize session history size, should be smaller than 1000
 */
+ (void)setSessionHistorySize:(int)sessionHistorySize;

/**
 * Returns the session specified with <code>sessionId</code> from the session
 * history.
 *
 * @param sessionId session identifier
 * @return session specified with sessionId or nil if it is not found in the
 * history
 */
+ (id<Session>)getSession:(long)sessionId;

/**
 * Deletes the session specified with <code>sessionId</code> from the session
 * history.
 *
 * @param sessionId session identifier
 */
+ (void)deleteSession:(long)sessionId;

/**
 * Adds a listener that is notified when sessions are deleted from session
 * history. Listeners are held weakly.
 *
 * @param listener listener to add
 */
+ (void)addSessionDeleteListener:(id<SessionDeleteListener>)listener;

/**
 * Removes a session delete listener.
 *
 * @param listener listener to remove
 */
+ (void)removeSessionDeleteListener:(id<SessionDeleteListener>)listener;

/**
 * Returns the last session created from the session history.
 *
 * @return the last session created or nil if session history is empty
 */
+ (id<Session>)getLastSession;

/**
 * Returns the last session completed from the session history.
 *
 * @return the last session completed. If there are no completed sessions in the
 * history this method will return nil
 */
+ (id<Session>)getLastCompletedSession;

/**
 * <p>Returns all sessions in the session history.
 *
 * @return all sessions in the session history
 */
+ (NSArray *)getSessions;

/**
 * <p>Clears all, including ongoing, sessions in the session history.
 * <p>Note that callbacks cannot be triggered for deleted sessions.
 */
+ (void)clearSessions;

/**
 * <p>Returns all FFmpeg sessions in the session history.
 *
 * @return all FFmpeg sessions in the session history
 */
+ (NSArray *)getFFmpegSessions;

/**
 * <p>Returns all FFprobe sessions in the session history.
 *
 * @return all FFprobe sessions in the session history
 */
+ (NSArray *)getFFprobeSessions;

/**
 * <p>Returns all MediaInformation sessions in the session history.
 *
 * @return all MediaInformation sessions in the session history
 */
+ (NSArray *)getMediaInformationSessions;

/**
 * <p>Returns sessions that have the given state.
 *
 * @return sessions that have the given state from the session history
 */
+ (NSArray *)getSessionsByState:(SessionState)state;

/**
 * Returns the active log redirection strategy.
 *
 * @return log redirection strategy
 */
+ (LogRedirectionStrategy)getLogRedirectionStrategy;

/**
 * <p>Sets the log redirection strategy
 *
 * @param logRedirectionStrategy log redirection strategy
 */
+ (void)setLogRedirectionStrategy:(LogRedirectionStrategy)logRedirectionStrategy;

/**
 * <p>Returns the number of async messages that are not transmitted to the
 * callbacks for this session.
 *
 * @param sessionId id of the session
 * @return number of async messages that are not transmitted to the callbacks
 * for this session
 */
+ (int)messagesInTransmit:(long)sessionId;

/**
 * Converts session state to string.
 *
 * @param state session state
 * @return string value
 */
+ (NSString *)sessionStateToString:(SessionState)state;

/**
 * <p>Parses the given command into arguments. Uses space character to split the
 * arguments. Supports single and double quote characters.
 *
 * @param command string command
 * @return array of arguments
 */
+ (NSArray *)parseArguments:(NSString *)command;

/**
 * <p>Concatenates arguments into a string adding a space character between two
 * arguments.
 *
 * @param arguments arguments
 * @return concatenated string containing all arguments
 */
+ (NSString *)argumentsToString:(NSArray *)arguments;

@end

#endif // FFMPEG_KIT_CONFIG_H
