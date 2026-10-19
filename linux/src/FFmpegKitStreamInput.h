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

#ifndef FFMPEG_KIT_STREAM_INPUT_H
#define FFMPEG_KIT_STREAM_INPUT_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ffmpegkit {

/**
 * Non-seekable streaming input that can be used in FFmpeg commands with an
 * ffkitstream: URL.
 */
class FFmpegKitStreamInput {
  public:
    static std::shared_ptr<ffmpegkit::FFmpegKitStreamInput> create(const std::string &extension);

    static std::shared_ptr<ffmpegkit::FFmpegKitStreamInput> create(const std::string &extension, const long capacity);

    ~FFmpegKitStreamInput();

    std::string getUrl();

    int write(const std::vector<uint8_t> &data);

    int write(const std::vector<uint8_t> &data, const int timeoutMs);

    int write(const std::vector<uint8_t> &data, const size_t offset, const size_t length, const int timeoutMs);

    int write(const uint8_t *data, const size_t length, const int timeoutMs);

    void closeInput();

    void close();

  private:
    FFmpegKitStreamInput(const long id, const std::string &extension);

    void ensureOpen();

    long _id;
    std::string _url;
    // Do not lock across I/O: native close must be able to wake blocked writes.
    // Operations past these checks are synchronized by the native registry.
    std::atomic<bool> _closed;
    std::atomic<bool> _inputClosed;
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_STREAM_INPUT_H
