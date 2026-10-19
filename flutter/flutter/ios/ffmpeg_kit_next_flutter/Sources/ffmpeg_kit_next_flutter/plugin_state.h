/* Copyright (c) 2026 Taner Sener. LGPL version 3 or later. */

// This state belongs to one engine. Queues retain it instead of the plugin, so
// engine delegate release can begin teardown while native work is still active.
@interface PluginReply : NSObject
@property(nonatomic, copy) FlutterResult callback;
@end
@implementation PluginReply
@end

@interface PluginWork : NSObject
@property(nonatomic, copy) void (^cancel)(void);
@property(nonatomic) BOOL active;
@end
@implementation PluginWork
@end

@interface PluginState : NSObject
- (FlutterResult)registerReply:(FlutterResult)result;
- (PluginWork *)queueWork:(void (^)(void))cancel;
- (BOOL)startWork:(PluginWork *)work;
- (void)finishWork:(PluginWork *)work;
- (void)setEventSink:(FlutterEventSink)sink;
- (void)emit:(id)value;
- (void)beginShutdownAfterDraining:(void (^)(void))cleanup;
- (BOOL)isStopping;
- (BOOL)trackAsyncSession:(id<Session>)session;
- (void)finishAsyncSession:(long)identifier;
- (void)pruneAsyncSessions;
@end

@implementation PluginState {
    NSCondition *condition;
    BOOL stopping;
    NSMutableArray<PluginReply *> *replies;
    NSMutableArray<PluginWork *> *workItems;
    FlutterEventSink eventSink;
    NSMutableDictionary<NSNumber *, id<Session>> *asyncSessions;
}
- (instancetype)init {
    self = [super init];
    if (self) {
        condition = [NSCondition new];
        replies = [NSMutableArray new];
        workItems = [NSMutableArray new];
        asyncSessions = [NSMutableDictionary new];
    }
    return self;
}
- (FlutterResult)registerReply:(FlutterResult)result {
    PluginReply *reply = [PluginReply new];
    reply.callback = result;
    [condition lock];
    BOOL accepted = !stopping;
    if (accepted)
        [replies addObject:reply];
    [condition unlock];
    if (!accepted) {
        result([FlutterError errorWithCode:@"PLUGIN_SHUTDOWN" message:@"The plugin is detached." details:nil]);
        return nil;
    }
    return ^(id value) {
      dispatch_async(dispatch_get_main_queue(), ^{
        [self->condition lock];
        FlutterResult callback = reply.callback;
        reply.callback = nil;
        [self->replies removeObjectIdenticalTo:reply];
        [self->condition unlock];
        if (callback != nil)
            callback(value);
      });
    };
}
- (BOOL)isStopping {
    [condition lock];
    BOOL value = stopping;
    [condition unlock];
    return value;
}
- (void)pruneAsyncSessionsLocked {
    for (NSNumber *identifier in [asyncSessions.allKeys copy]) {
        SessionState status = [asyncSessions[identifier] getState];
        if (status == SessionStateCompleted || status == SessionStateFailed)
            [asyncSessions removeObjectForKey:identifier];
    }
}
- (void)pruneAsyncSessions {
    [condition lock];
    [self pruneAsyncSessionsLocked];
    [condition unlock];
}
- (BOOL)trackAsyncSession:(id<Session>)session {
    [condition lock];
    BOOL accepted = !stopping;
    if (accepted) {
        [self pruneAsyncSessionsLocked];
        asyncSessions[@([session getSessionId])] = session;
    }
    [condition unlock];
    return accepted;
}
- (void)finishAsyncSession:(long)identifier {
    [condition lock];
    [asyncSessions removeObjectForKey:@(identifier)];
    [condition unlock];
}
- (PluginWork *)queueWork:(void (^)(void))cancel {
    PluginWork *work = [PluginWork new];
    work.cancel = cancel;
    [condition lock];
    BOOL accepted = !stopping;
    if (accepted)
        [workItems addObject:work];
    [condition unlock];
    return accepted ? work : nil;
}
- (BOOL)startWork:(PluginWork *)work {
    [condition lock];
    BOOL accepted = work != nil && !stopping;
    if (accepted)
        work.active = YES;
    [condition unlock];
    return accepted;
}
- (void)finishWork:(PluginWork *)work {
    if (work == nil)
        return;
    __attribute__((objc_precise_lifetime)) void (^retiredCancel)(void);
    [condition lock];
    retiredCancel = work.cancel;
    [workItems removeObjectIdenticalTo:work];
    work.cancel = nil;
    [condition broadcast];
    [condition unlock];
}
- (void)setEventSink:(FlutterEventSink)sink {
    [condition lock];
    if (!stopping)
        eventSink = sink;
    [condition unlock];
}
- (void)emit:(id)value {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self->condition lock];
      FlutterEventSink sink = self->stopping ? nil : self->eventSink;
      [self->condition unlock];
      if (sink != nil)
          sink(value);
    });
}
- (void)beginShutdownAfterDraining:(void (^)(void))cleanup {
    [condition lock];
    if (stopping) {
        [condition unlock];
        return;
    }
    stopping = YES;
    eventSink = nil;
    [self pruneAsyncSessionsLocked];
    NSArray *ownedAsync = asyncSessions.allValues;
    [asyncSessions removeAllObjects];
    NSArray *pending = [replies copy];
    [replies removeAllObjects];
    NSMutableArray *callbacks = [NSMutableArray new];
    for (PluginReply *reply in pending) {
        if (reply.callback != nil)
            [callbacks addObject:reply.callback];
        reply.callback = nil;
    }
    // Plugin workers still queued at teardown cannot enter native execution.
    for (PluginWork *work in [workItems copy]) {
        if (!work.active) {
            work.cancel = nil;
            [workItems removeObjectIdenticalTo:work];
        }
    }
    [condition unlock];
    for (id<Session> session in ownedAsync) {
        @try {
            [session cancel];
        } @catch (NSException *exception) { /* Continue cancelling owned sessions. */
        }
    }
    void (^settleReplies)(void) = ^{
      for (FlutterResult callback in callbacks) {
          callback([FlutterError errorWithCode:@"PLUGIN_SHUTDOWN" message:@"The plugin is detached." details:nil]);
      }
    };
    // macOS may release its last delegate reference on a native callback thread.
    // Channel delivery still belongs to the main queue in that case.
    if ([NSThread isMainThread])
        settleReplies();
    else
        dispatch_async(dispatch_get_main_queue(), settleReplies);
    // Never block the engine/platform thread while native workers finish. State
    // and native handles stay alive independently until the last worker returns.
    [NSThread detachNewThreadWithBlock:^{
      @autoreleasepool {
          [self->condition lock];
          while (self->workItems.count != 0) {
              @autoreleasepool {
                  NSMutableArray *cancellations = [NSMutableArray new];
                  for (PluginWork *work in self->workItems) {
                      if (work.cancel != nil)
                          [cancellations addObject:[work.cancel copy]];
                  }
                  [self->condition unlock];
                  for (void (^cancel)(void) in cancellations) {
                      @try {
                          cancel();
                      } @catch (NSException *exception) { /* Continue draining other work. */
                      }
                  }
                  [self->condition lock];
                  if (self->workItems.count != 0) {
                      // Native registration can overwrite an early cancel request.
                      [self->condition waitUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
                  }
              }
          }
          [self->condition unlock];
          @try {
              if (cleanup != nil)
                  cleanup();
          } @catch (NSException *exception) { /* Teardown must not escape its worker. */
          }
      }
    }];
}
@end
