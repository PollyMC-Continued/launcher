#pragma once

#include "BaseInstance.h"
#include "ResourceDownloadTask.h"
#include "ReviewMessageBox.h"

#include "minecraft/mod/ModFolderModel.h"

#include "modplatform/CheckUpdateTask.h"

class Mod;
class ModrinthCheckUpdate;
class FlameCheckUpdate;
class ConcurrentTask;

class ResourceUpdateDialog final : public ReviewMessageBox {
    Q_OBJECT
   public:
    explicit ResourceUpdateDialog(QWidget* parent,
                                  BaseInstance* instance,
                                  ResourceFolderModel* resourceModel,
                                  QList<Resource*>& searchFor,
                                  bool includeDeps,
                                  QList<ModPlatform::ModLoaderType> loadersList = {});

    void checkCandidates();

    void appendResource(const CheckUpdateTask::Update& info, QStringList requiredBy = {});

    const QList<ResourceDownloadTask::Ptr> getTasks();
    auto indexDir() const -> QDir { return m_resourceModel->indexDir(); }

    auto noUpdates() const -> bool { return m_noUpdates; };
    auto aborted() const -> bool { return m_aborted; };

    /// Resources that could not be checked (no metadata found, or the provider failed): "up to date" must not claim more than we know.
    auto failedCount() const -> int { return static_cast<int>(m_failedMetadata.size() + m_failedCheckUpdate.size()); }

   private:
    auto ensureMetadata() -> bool;

   private slots:
    void onMetadataEnsured(Resource* resource);
    void onMetadataFailed(Resource* resource,
                          bool try_others = false,
                          ModPlatform::ResourceProvider firstChoice = ModPlatform::ResourceProvider::MODRINTH);

   private:
    QWidget* m_parent;

    shared_qobject_ptr<ModrinthCheckUpdate> m_modrinthCheckTask;
    shared_qobject_ptr<FlameCheckUpdate> m_flameCheckTask;

    ResourceFolderModel* m_resourceModel;

    QList<Resource*>& m_candidates;
    QList<Resource*> m_modrinthToUpdate;
    QList<Resource*> m_flameToUpdate;

    ConcurrentTask::Ptr m_secondTryMetadata;
    QList<std::tuple<Resource*, QString>> m_failedMetadata;
    QList<std::tuple<Resource*, QString, QUrl>> m_failedCheckUpdate;

    // keyed by a unique number, not by the resource name: two files (or a mod and a dependency) can share a name
    QHash<QString, ResourceDownloadTask::Ptr> m_tasks;
    int m_nextTaskKey = 0;
    BaseInstance* m_instance;

    bool m_noUpdates = false;
    bool m_aborted = false;
    bool m_includeDeps = false;
    QList<ModPlatform::ModLoaderType> m_loadersList;
};
