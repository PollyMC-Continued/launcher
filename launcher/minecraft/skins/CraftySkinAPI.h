// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (c) 2026 PollyMC-Continued Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <functional>

#include <QImage>
#include <QJsonArray>
#include <QList>
#include <QSharedPointer>
#include <QString>
#include <QUrl>

#include "net/NetJob.h"
#include "tasks/Task.h"

namespace Crafty {

// one entry of the crafty.gg skin catalog
struct Skin {
    QString hash;
    QString username;
    QString createdAt;
    bool slim = false;
    int playersCount = 0;
    int upvotes = 0;
    int views = 0;
    QString texturePath;
    QImage preview;
    QImage texture;
};
using SkinPtr = QSharedPointer<Skin>;

// one page of catalog results
struct SkinPage {
    QList<SkinPtr> skins;
    QString playerName;
    int page = 1;
    bool hasMore = false;
};

// one cape offered by the cape lookup. crafty.gg tracks the Mojang and OptiFine capes a player
// owns and shows them on the player's page, but its public API does not document a cape
// catalog, so capes are scraped defensively out of the player profile (or imported directly
// from a texture URL).
struct CapeEntry {
    QString name;      // display name of the cape, may be empty when nothing named it
    QString username;  // crafty.gg player the cape was found for, may be empty for URL imports
    QString source;    // "mojang"/"optifine"/"crafty"/"url" depending on where the PNG is hosted
    QUrl textureUrl;   // where the PNG came from, may be empty for purely local capes
    QImage texture;    // the decoded cape PNG
    QImage preview;    // small aspect-preserving preview of the texture
    QString cachePath; // <dataRoot>/cache/capes/<sha1 of the PNG>.png
};
using CapePtr = QSharedPointer<CapeEntry>;

// small async client for the public crafty.gg skin catalog
class API : public QObject {
    Q_OBJECT
   public:
    using PageCallback = std::function<void(const SkinPage&, const QString&)>;
    using EachCallback = std::function<void(const SkinPtr&)>;
    using DoneCallback = std::function<void(const QString&)>;
    // capes come back as a list; the message is an error, or the informational
    // noCapesMessage() when the player simply has no capes on record
    using CapeListCallback = std::function<void(const QList<CapePtr>&, const QString&)>;

    explicit API(QObject* parent = nullptr);

    // an empty query browses the catalog, a query matches player names, tags and styles and adds the matching player's own skins
    NetJob::Ptr fetchSkins(int page, const QString& query, PageCallback callback);
    // fills texture/preview/texturePath on every entry, cached textures are not downloaded again
    NetJob::Ptr fetchTextures(const QList<SkinPtr>& skins, EachCallback each, DoneCallback done);

    // looks up the capes crafty.gg tracks for a player name. The player profile is walked
    // defensively for cape-looking texture URLs, so the call degrades gracefully when the
    // API shape changes: an empty list with noCapesMessage() means the player has no capes
    // on record, which is not an error, any other message is one.
    NetJob::Ptr fetchPlayerCapes(const QString& username, CapeListCallback callback);
    // downloads and validates any cape PNG offered by a direct URL
    NetJob::Ptr fetchCapeFromUrl(const QUrl& url, CapeListCallback callback);

    // stops any request still in flight, used when the dialog goes away
    void abortAll();

    static QString pageUrl(const QString& hash);
    // writes the texture to the local cache so the skin can be previewed and installed later
    static bool cacheTexture(const SkinPtr& skin);
    // the informational "nothing found" message handed to CapeListCallback implementations
    static QString noCapesMessage();

   private:
    NetJob::Ptr catalogPage(int page, const QString& search, PageCallback callback);
    NetJob::Ptr playerPage(int page, const QString& playerId, const QString& playerName, PageCallback callback);
    static bool fillTexture(const SkinPtr& skin, const QByteArray& json);
    static bool loadCached(const SkinPtr& skin);

    NetJob::Ptr m_pageJob;
    NetJob::Ptr m_searchJob;
    NetJob::Ptr m_nestedJob;
    NetJob::Ptr m_capeJob;
    QList<NetJob::Ptr> m_textureJobs;
    QList<NetJob::Ptr> m_capeTextureJobs;
};

}  // namespace Crafty
