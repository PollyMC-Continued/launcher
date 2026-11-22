// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <utility>

#include <QFutureWatcher>
#include <QObject>
#include <QtConcurrent>

namespace BedrockAsync {

/*
 * Runs blocking work (PowerShell, package queries, file operations) on a
 * worker thread and delivers the result on the owner's thread, so the launcher
 * UI never freezes while it waits. `done` runs in the owner's thread; if the
 * owner is destroyed first, the callback is dropped.
 */
template <typename R, typename Work, typename Done>
void runOffThread(QObject* owner, Work work, Done done)
{
    auto* watcher = new QFutureWatcher<R>(owner);
    QObject::connect(watcher, &QFutureWatcher<R>::finished, owner, [watcher, done]() {
        R result = watcher->result();
        watcher->deleteLater();
        done(std::move(result));
    });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}

}  // namespace BedrockAsync
