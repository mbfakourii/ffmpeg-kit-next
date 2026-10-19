/* Copyright (c) 2026 Taner Sener. LGPL version 3 or later. */
#ifndef FFMPEG_KIT_FLUTTER_LINUX_PLUGIN_STATE_H
#define FFMPEG_KIT_FLUTTER_LINUX_PLUGIN_STATE_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <flutter_linux/flutter_linux.h>

#include <Session.h>
#include <SessionState.h>

namespace ffmpeg_kit_linux {

// A deferred method call reply. Completed only on the platform thread,
// including channel teardown. Workers enqueue a response; they never call
// Flutter or release its call object.
class PendingResponse {
  public:
    explicit PendingResponse(FlMethodCall *call) : call_(static_cast<FlMethodCall *>(g_object_ref(call))) {}
    ~PendingResponse() {
        if (call_)
            g_object_unref(call_);
    }
    PendingResponse(const PendingResponse &) = delete;
    PendingResponse &operator=(const PendingResponse &) = delete;

    // Takes ownership of response.
    void Complete(FlMethodResponse *response) {
        if (call_) {
            fl_method_call_respond(call_, response, nullptr);
            g_object_unref(call_);
            call_ = nullptr;
        }
        g_object_unref(response);
    }
    bool IsComplete() const { return call_ == nullptr; }

  private:
    FlMethodCall *call_;
};

// Lifecycle of one engine's plugin, like PluginState on Android and Apple:
// admission, admitted executions, pending replies and async sessions. Native
// work holds the state, never the plugin GObject, so retaining a worker cannot
// delay channel-detachment notification.
class PluginState : public std::enable_shared_from_this<PluginState> {
    struct Execution {
        std::function<void()> cancel;
    };

  public:
    class Lease {
      public:
        ~Lease() { owner_->Finish(execution_); }

      private:
        friend class PluginState;
        Lease(std::shared_ptr<PluginState> owner, std::shared_ptr<Execution> execution) : owner_(std::move(owner)), execution_(std::move(execution)) {}
        std::shared_ptr<PluginState> owner_;
        std::shared_ptr<Execution> execution_;
    };

    bool IsStopping() const { return stopping_.load(); }
    bool BeginShutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        return !stopping_.exchange(true);
    }
    std::shared_ptr<Lease> TryStart(std::function<void()> cancel) {
        auto execution = std::make_shared<Execution>(Execution{std::move(cancel)});
        auto lease = std::shared_ptr<Lease>(new Lease(shared_from_this(), execution));
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_)
            return nullptr;
        executions_.push_back(execution);
        return lease;
    }
    void CancelAndWait() {
        BeginShutdown();
        std::unique_lock<std::mutex> lock(mutex_);
        while (!executions_.empty()) {
            const auto snapshot = executions_;
            lock.unlock();
            for (const auto &execution : snapshot) {
                try {
                    if (execution->cancel)
                        execution->cancel();
                } catch (...) {
                }
            }
            lock.lock();
            // Cancellation before native registration can be overwritten. Retry
            // until every active native call returns; never hold this lock in native.
            finished_.wait_for(lock, std::chrono::milliseconds(10), [this] { return executions_.empty(); });
        }
    }

    // Pending replies are platform-thread owned, as is teardown, so they need
    // no lock. Workers keep their own reference to the reply they complete.
    bool AddPendingResponse(const std::shared_ptr<PendingResponse> &response) {
        if (IsStopping())
            return false;
        pending_responses_.push_back(response);
        return true;
    }
    void PrunePendingResponses() {
        pending_responses_.erase(std::remove_if(pending_responses_.begin(), pending_responses_.end(), [](const std::shared_ptr<PendingResponse> &response) { return response->IsComplete(); }), pending_responses_.end());
    }
    // Answers every outstanding reply. Call after BeginShutdown.
    void CompletePendingResponses(const char *code, const char *message) {
        std::vector<std::shared_ptr<PendingResponse>> responses;
        responses.swap(pending_responses_);
        for (const auto &response : responses)
            response->Complete(FL_METHOD_RESPONSE(fl_method_error_response_new(code, message, nullptr)));
    }

    // Only asynchronous submissions need ownership beyond a worker's lease.
    // Keep their handles across history eviction, then release on completion.
    bool TrackAsync(const std::shared_ptr<ffmpegkit::Session> &session) {
        std::lock_guard<std::mutex> lock(async_mutex_);
        if (IsStopping())
            return false;
        PruneAsyncLocked();
        async_sessions_[session->getSessionId()] = session;
        return true;
    }
    void FinishAsync(long id) {
        std::lock_guard<std::mutex> lock(async_mutex_);
        async_sessions_.erase(id);
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
    std::atomic<bool> stopping_{false};
    std::mutex mutex_;
    std::condition_variable finished_;
    std::list<std::shared_ptr<Execution>> executions_;
    std::vector<std::shared_ptr<PendingResponse>> pending_responses_;
    std::mutex async_mutex_;
    std::map<long, std::shared_ptr<ffmpegkit::Session>> async_sessions_;
};
} // namespace ffmpeg_kit_linux
#endif
