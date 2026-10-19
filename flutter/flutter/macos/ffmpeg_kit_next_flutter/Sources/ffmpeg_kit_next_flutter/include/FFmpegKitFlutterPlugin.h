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

#import <FlutterMacOS/FlutterMacOS.h>

@interface FFmpegKitFlutterPlugin : NSObject<FlutterPlugin,FlutterStreamHandler>
/**
 * Stop this engine's plugin before calling FlutterEngine.shutDownEngine.
 * macOS Flutter does not notify plugins when its engine shuts down, and pending
 * replies can retain the engine. This method settles those replies, closes
 * admission, and cancels owned work without shutting down the shared library.
 * Call on the main thread; repeated calls are harmless.
 */
+ (void)detachFromEngine:(NSObject<FlutterPluginRegistry> *)engine NS_SWIFT_NAME(detach(from:));
@end
