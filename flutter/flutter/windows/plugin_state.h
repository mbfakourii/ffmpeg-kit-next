/*
 * Copyright (c) 2026 Taner Sener
 *
 * This file is part of FFmpegKitNext and is licensed under the GNU Lesser
 * General Public License, version 3 or later.
 */

#ifndef FFMPEG_KIT_FLUTTER_WINDOWS_PLUGIN_STATE_H
#define FFMPEG_KIT_FLUTTER_WINDOWS_PLUGIN_STATE_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <flutter/encodable_value.h>
#include <flutter/method_result.h>

#include <Session.h>
#include <SessionState.h>

namespace ffmpeg_kit_windows {

// A method call reply. Exactly one of its worker or teardown answers it.
struct PendingResponse {
    explicit PendingResponse(std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>> value) : result(std::move(value)) {}

    std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>> result;
    std::atomic<bool> completed{false};
};

// Lifecycle of one plugin instance, like PluginState on Android and Apple:
// admission, admitted executions, pending replies and async sessions. An
// admitted execution keeps its lease until the native call returns, even when
// that call throws.
class PluginState {
    struct Execution {
        std::function<void()> cancel;
    };

  public:
    class Lease {
      public:
        ~Lease() { owner_.Finish(execution_); }
        Lease(const Lease &) = delete;
        Lease &operator=(const Lease &) = delete;

      private:
        friend class PluginState;
        Lease(PluginState &owner, std::shared_ptr<Execution> execution) : owner_(owner), execution_(std::move(execution)) {}

        PluginState &owner_;
        std::shared_ptr<Execution> execution_;
    };

    std::unique_ptr<Lease> TryStart(std::function<void()> cancel) {
        auto execution = std::make_shared<Execution>(Execution{std::move(cancel)});
        // Allocate the lease before registering it, so allocation failure cannot
        // leave teardown waiting for a worker that never started.
        auto lease = std::unique_ptr<Lease>(new Lease(*this, execution));
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_)
                return nullptr;
            executions_.push_back(execution);
        }
        return lease;
    }

    bool IsStopping() const { return stopping_.load(); }

    bool BeginShutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        return !stopping_.exchange(true);
    }

    void CancelAndWait() {
        BeginShutdown();
        std::unique_lock<std::mutex> lock(mutex_);
        while (!executions_.empty()) {
            const auto executions = executions_;
            lock.unlock();
            for (const auto &execution : executions) {
                try {
                    if (execution->cancel)
                        execution->cancel();
                } catch (...) {
                    // Keep draining other workers and retry cancellation on the next
                    // pass; a native error must not escape the plugin destructor.
                }
            }
            lock.lock();
            // Native registration can overwrite a cancellation sent before it.
            // Repeat until every admitted native call has returned. No lifecycle
            // lock is held while calling native code or waiting for stream I/O.
            finished_.wait_for(lock, std::chrono::milliseconds(10), [this] { return executions_.empty(); });
        }
    }

    // Admission and registration share the lock BeginShutdown takes, so a
    // reply is either registered before teardown takes the list or refused.
    bool AddPendingResponse(const std::shared_ptr<PendingResponse> &response) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_)
            return false;
        pending_responses_.push_back(response);
        return true;
    }

    bool ClaimPendingResponse(const std::shared_ptr<PendingResponse> &response) {
        if (response->completed.exchange(true))
            return false;
        std::lock_guard<std::mutex> lock(mutex_);
        pending_responses_.erase(std::remove(pending_responses_.begin(), pending_responses_.end(), response), pending_responses_.end());
        return true;
    }

    // Answers every unclaimed reply. Call after BeginShutdown.
    void CompletePendingResponses(const std::string &code, const std::string &message) {
        std::vector<std::shared_ptr<PendingResponse>> responses;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            responses.swap(pending_responses_);
        }
        for (const auto &response : responses) {
            if (!response->completed.exchange(true))
                response->result->Error(code, message);
        }
    }

    // Only asynchronous submissions need ownership beyond a worker's lease.
    // Keep their handles across history eviction, then release on completion.
    bool TrackAsync(const std::shared_ptr<ffmpegkit::Session> &session) {
        std::lock_guard<std::mutex> lock(async_mutex_);
        if (stopping_)
            return false;
        PruneAsyncLocked();
        async_sessions_[session->getSessionId()] = session;
        return true;
    }

    void FinishAsync(long id) {
        std::lock_guard<std::mutex> lock(async_mutex_);
        async_sessions_.erase(id);
    }

    void PruneAsync() {
        std::lock_guard<std::mutex> lock(async_mutex_);
        PruneAsyncLocked();
    }

    // Cancels outside the lock that completion callbacks take.
    void CancelAsync() {
        std::map<long, std::shared_ptr<ffmpegkit::Session>> owned;
        {
            std::lock_guard<std::mutex> lock(async_mutex_);
            PruneAsyncLocked();
            owned.swap(async_sessions_);
        }
        for (const auto &item : owned) {
            try {
                item.second->cancel();
            } catch (...) {
            }
        }
    }

  private:
    void Finish(const std::shared_ptr<Execution> &execution) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            executions_.remove(execution);
        }
        finished_.notify_all();
    }

    void PruneAsyncLocked() {
        for (auto item = async_sessions_.begin(); item != async_sessions_.end();) {
            const auto status = item->second->getState();
            if (status == ffmpegkit::SessionStateCompleted || status == ffmpegkit::SessionStateFailed)
                item = async_sessions_.erase(item);
            else
                ++item;
        }
    }

    std::mutex mutex_;
    std::condition_variable finished_;
    std::atomic<bool> stopping_{false};
    std::list<std::shared_ptr<Execution>> executions_;
    std::vector<std::shared_ptr<PendingResponse>> pending_responses_;
    std::mutex async_mutex_;
    std::map<long, std::shared_ptr<ffmpegkit::Session>> async_sessions_;
};

} // namespace ffmpeg_kit_windows

#endif
