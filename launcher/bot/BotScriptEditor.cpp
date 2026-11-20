#include "BotScriptEditor.h"

#include "BotManagerDialog.h"
#include "bot/BotScript.h"
#include "ui_BotScriptEditor.h"

#include <QMenu>
#include <QFileDialog>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QMimeData>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTimer>

// Tree model over nested BotScriptStep lists. Structural edits reset
// the model; field edits emit dataChanged. No cross-level drops.
// 'if' steps show their then-branch rows first and their else-branch rows
// (prefixed "else:") after them, so both branches are editable in-tree.
class StepModel : public QAbstractItemModel {
   public:
    struct Node {
        QList<BotScriptStep>* list = nullptr;
        int row = -1;         // position within list
        int viewRow = -1;     // position among the parent's child rows
        bool elseBranch = false;
        Node* parent = nullptr;
    };

    // Maps a child view-row of a container step onto its branch list and
    // local row. Then-branch rows come first; else-branch rows follow.
    struct BranchRef {
        QList<BotScriptStep>* list = nullptr;
        int localRow = 0;
        bool elseBranch = false;
    };

    explicit StepModel(QObject* parent = nullptr) : QAbstractItemModel(parent) {}
    void setRoot(QList<BotScriptStep>* root)
    {
        beginResetModel();
        m_root = root;
        qDeleteAll(m_nodeCache);
        m_nodeCache.clear();
        endResetModel();
    }

    BotScriptStep* stepAt(const QModelIndex& index) const
    {
        if (!index.isValid() || !m_root)
            return nullptr;
        auto* node = static_cast<Node*>(index.internalPointer());
        if (!node || !node->list || node->row < 0 || node->row >= node->list->size())
            return nullptr;
        return &(*node->list)[node->row];
    }

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override
    {
        if (column != 0 || !m_root)
            return {};
        QList<BotScriptStep>* list = nullptr;
        int localRow = 0;
        bool elseBranch = false;
        Node* parentNode = nullptr;
        if (parent.isValid()) {
            parentNode = static_cast<Node*>(parent.internalPointer());
            if (!parentNode || parentNode->row < 0)
                return {};
            BotScriptStep& step = (*parentNode->list)[parentNode->row];
            if (!isContainer(step))
                return {};
            const BranchRef b = branchAt(step, row);
            if (!b.list)
                return {};
            list = b.list;
            localRow = b.localRow;
            elseBranch = b.elseBranch;
        } else {
            if (row < 0 || row >= m_root->size())
                return {};
            list = m_root;
            localRow = row;
        }
        const auto key = qMakePair(static_cast<const void*>(list), localRow);
        auto it = m_nodeCache.find(key);
        if (it != m_nodeCache.end()) {
            // refresh parent/position: a (list,row) pair can be reached
            // through different parents after structural edits
            it.value()->parent = parentNode;
            it.value()->viewRow = row;
            it.value()->elseBranch = elseBranch;
            return createIndex(row, column, it.value());
        }
        auto* node = new Node{ list, localRow, row, elseBranch, parentNode };
        m_nodeCache.insert(key, node);
        return createIndex(row, column, node);
    }

    QModelIndex parent(const QModelIndex& index) const override
    {
        if (!index.isValid())
            return {};
        auto* node = static_cast<Node*>(index.internalPointer());
        if (!node || !node->parent)
            return {};
        return createIndex(node->parent->viewRow, 0, node->parent);
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (!m_root)
            return 0;
        if (!parent.isValid())
            return m_root->size();
        auto* node = static_cast<Node*>(parent.internalPointer());
        if (!node || node->row < 0)
            return 0;
        const BotScriptStep& step = (*node->list)[node->row];
        if (step.type == "loop")
            return step.steps.size();
        if (step.type == "if")
            return step.steps.size() + step.elseSteps.size();
        return 0;
    }

    int columnCount(const QModelIndex& = QModelIndex()) const override { return 1; }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (role != Qt::DisplayRole)
            return {};
        const BotScriptStep* step = stepAt(index);
        if (!step)
            return {};
        auto* node = static_cast<Node*>(index.internalPointer());
        if (node && node->elseBranch)
            return QStringLiteral("else: ") + displayText(*step);
        return displayText(*step);
    }

    QVariant headerData(int, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
            return tr("Step");
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        auto f = QAbstractItemModel::flags(index);
        if (index.isValid())
            f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
        else
            f |= Qt::ItemIsDropEnabled;
        return f;
    }

    static QString displayText(const BotScriptStep& s)
    {
        if (s.type == "say" || s.type == "command" || s.type == "log")
            return s.type + ": " + s.text;
        if (s.type == "wait")
            return QString("wait %1s").arg(s.seconds);
        if (s.type == "wait_for_chat")
            return "wait for chat ~" + s.pattern;
        if (s.type == "wait_for_player")
            return "wait for " + s.player;
        if (s.type == "loop")
            return s.times == -1 ? "loop ∞" : QString("loop ×%1").arg(s.times);
        if (s.type == "set")
            return QString("set %1 = \"%2\"").arg(s.name, s.value);
        if (s.type == "inc")
            return QString("inc %1 %2%3").arg(s.name, s.value.startsWith('-') ? "" : "+", s.value);
        if (s.type == "if")
            return QString("if %1").arg(conditionSummary(s));
        return s.type;
    }

    static QString conditionSummary(const BotScriptStep& s)
    {
        if (s.condType == "var_gt" || s.condType == "var_gte")
            return QString("%1 %2 %3")
                .arg(s.condVar, s.condType == "var_gt" ? ">" : ">=", QString::number(s.condNumber));
        if (s.condType == "var_equals")
            return QString("%1 == \"%2\"").arg(s.condVar, s.condValue);
        if (s.condType == "var_not_equals")
            return QString("%1 != \"%2\"").arg(s.condVar, s.condValue);
        if (s.condType == "health_below")
            return QString("health < %1").arg(s.condNumber);
        return QString("\"%1\"").arg(s.condValue);
    }

    bool moveStep(const QModelIndex& parent, int from, int to)
    {
        if (!m_root)
            return false;
        QList<BotScriptStep>* list = nullptr;
        int fromLocal = -1;
        int toLocal = -1;
        if (parent.isValid()) {
            BotScriptStep* owner = stepAt(parent);
            if (!owner || !isContainer(*owner))
                return false;
            const BranchRef src = branchAt(*owner, from);
            const BranchRef dst = branchAt(*owner, to);
            // moves stay inside one branch; cross-branch edits are remove+add
            if (!src.list || !dst.list || src.list != dst.list)
                return false;
            list = src.list;
            fromLocal = src.localRow;
            toLocal = dst.localRow;
        } else {
            list = m_root;
            fromLocal = from;
            toLocal = to;
        }
        if (fromLocal < 0 || fromLocal >= list->size() || toLocal < 0 || toLocal >= list->size() || fromLocal == toLocal)
            return false;
        if (from < to) {
            beginMoveRows(parent, from, from, parent, to + 1);
            list->move(fromLocal, toLocal);
        } else {
            beginMoveRows(parent, from, from, parent, to);
            list->move(fromLocal, toLocal);
        }
        endMoveRows();
        return true;
    }

    void insertStep(const QModelIndex& parent, int row, const BotScriptStep& step)
    {
        if (!m_root)
            return;
        QList<BotScriptStep>* list = nullptr;
        int localRow = 0;
        if (parent.isValid()) {
            BotScriptStep* owner = stepAt(parent);
            if (!owner || !isContainer(*owner))
                return;
            const int thenCount = owner->steps.size();
            const int total = thenCount + (owner->type == "if" ? owner->elseSteps.size() : 0);
            row = qBound(0, row, total);
            // rows at or below the then-branch length land in the then
            // branch; rows past it land in the else branch (if steps only)
            if (row <= thenCount) {
                list = &owner->steps;
                localRow = row;
            } else {
                list = &owner->elseSteps;
                localRow = row - thenCount;
            }
        } else {
            list = m_root;
            row = qBound(0, row, list->size());
            localRow = row;
        }
        beginInsertRows(parent, row, row);
        list->insert(localRow, step);
        endInsertRows();
    }

    void removeStep(const QModelIndex& index)
    {
        if (!index.isValid())
            return;
        auto* node = static_cast<Node*>(index.internalPointer());
        beginRemoveRows(parent(index), node->viewRow, node->viewRow);
        node->list->removeAt(node->row);
        endRemoveRows();
    }

    QStringList mimeTypes() const override { return { "application/x-qabstractitemmodeldatalist" }; }
    Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }

    QMimeData* mimeData(const QModelIndexList& indexes) const override
    {
        m_dragSource.clear();
        for (const auto& idx : indexes) {
            if (idx.isValid() && idx.column() == 0)
                m_dragSource.append(idx);
        }
        auto* mime = new QMimeData();
        mime->setData("application/x-qabstractitemmodeldatalist", {});
        return mime;
    }

    bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int, int, const QModelIndex&) const override
    {
        return action == Qt::MoveAction && data->hasFormat("application/x-qabstractitemmodeldatalist") &&
               !m_dragSource.isEmpty();
    }

    bool dropMimeData(const QMimeData*, Qt::DropAction action, int row, int, const QModelIndex& parent) override
    {
        if (action != Qt::MoveAction || m_dragSource.isEmpty())
            return false;
        const QModelIndex src = m_dragSource.first();
        m_dragSource.clear();
        if (src.parent() != parent)
            return false;  // siblings only, no cross-level drops
        int dest = (row == -1) ? rowCount(parent) : row;
        if (dest == src.row() || dest == src.row() + 1)
            return false;
        return moveStep(parent, src.row(), dest > src.row() ? dest - 1 : dest);
    }

   private:
    static bool isContainer(const BotScriptStep& s) { return s.type == "loop" || s.type == "if"; }

    static BranchRef branchAt(BotScriptStep& step, int viewRow)
    {
        BranchRef r;
        if (viewRow < 0)
            return r;
        if (viewRow < step.steps.size()) {
            r.list = &step.steps;
            r.localRow = viewRow;
            return r;
        }
        if (step.type == "if") {
            const int elseRow = viewRow - step.steps.size();
            if (elseRow < step.elseSteps.size()) {
                r.list = &step.elseSteps;
                r.localRow = elseRow;
                r.elseBranch = true;
            }
        }
        return r;
    }

    QList<BotScriptStep>* m_root = nullptr;
    // nodes cached by (list, row); cleared on every structural reset
    mutable QHash<QPair<const void*, int>, Node*> m_nodeCache;
    mutable QModelIndexList m_dragSource;
};

static BotScriptStep defaultStep(const QString& type)
{
    BotScriptStep s;
    s.type = type;
    if (type == "say")
        s.text = "hello";
    else if (type == "command")
        s.text = "/help";
    else if (type == "log")
        s.text = "note";
    else if (type == "wait")
        s.seconds = 30;
    else if (type == "wait_for_chat")
        s.timeoutSeconds = 300;
    else if (type == "wait_for_player")
        s.timeoutSeconds = 600;
    else if (type == "loop")
        s.times = 10;
    else if (type == "set") {
        s.name = "count";
        s.value = "0";
    } else if (type == "inc") {
        s.name = "count";
        s.value = "1";
    } else if (type == "if") {
        s.condType = "chat_contains";
        s.condValue = "ready";
        s.timeoutSeconds = 30;
    }
    return s;
}

static const char* STEP_TYPES[] = { "say", "command", "wait", "wait_for_chat", "wait_for_player", "log", "loop", "set", "inc", "if" };

// Condition types of 'if' steps, kept in sync with BotScript.cpp's kConditionTypes.
static const char* CONDITION_TYPES[] = {
    "chat_contains", "chat_equals", "var_equals", "var_not_equals",
    "var_gt", "var_gte", "player_joined", "health_below"
};

BotScriptEditor::BotScriptEditor(BotManagerDialog* manager, QWidget* parent) : QWidget(parent), m_manager(manager), m_ui(new Ui::BotScriptEditor)
{
    m_ui->setupUi(this);

    auto* model = new StepModel(this);
    m_ui->stepsTree->setModel(model);
    m_ui->stepsTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_ui->stepsTree->setSelectionMode(QAbstractItemView::SingleSelection);

    for (auto* label : { m_ui->textError, m_ui->waitError, m_ui->chatError, m_ui->playerError, m_ui->loopError,
                         m_ui->setError, m_ui->incError, m_ui->ifError })
        label->setStyleSheet("color: #f87171;");

    for (const char* t : CONDITION_TYPES)
        m_ui->condTypeCombo->addItem(QString(t));
    m_ui->condNumberSpin->setRange(-BotScriptParser::kMaxCondNumber, BotScriptParser::kMaxCondNumber);
    m_ui->condTimeoutSpin->setRange(0, BotScriptParser::kMaxWaitSeconds);

    connect(m_ui->stepsTree->selectionModel(), &QItemSelectionModel::currentChanged, this, &BotScriptEditor::onSelectionChanged);
    connect(m_ui->addBtn, &QPushButton::clicked, this, &BotScriptEditor::onAddStep);
    connect(m_ui->removeBtn, &QPushButton::clicked, this, &BotScriptEditor::onRemoveStep);
    connect(m_ui->upBtn, &QPushButton::clicked, this, [this] { onMoveStep(-1); });
    connect(m_ui->downBtn, &QPushButton::clicked, this, [this] { onMoveStep(1); });
    connect(m_ui->nameEdit, &QLineEdit::textEdited, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->enabledCheck, &QCheckBox::toggled, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->textEdit, &QLineEdit::textEdited, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->secondsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->patternEdit, &QLineEdit::textEdited, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->caseCheck, &QCheckBox::toggled, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->playerEdit, &QLineEdit::textEdited, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->timeoutSpin2, QOverload<int>::of(&QSpinBox::valueChanged), this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->timesSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->setNameEdit, &QLineEdit::textEdited, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->setValueEdit, &QLineEdit::textEdited, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->incNameEdit, &QLineEdit::textEdited, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->incValueEdit, &QLineEdit::textEdited, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->condTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        updateCondFieldVisibility();
        onFieldEdited();
    });
    connect(m_ui->condValueEdit, &QLineEdit::textEdited, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->condVarEdit, &QLineEdit::textEdited, this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->condNumberSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->condTimeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BotScriptEditor::onFieldEdited);
    connect(m_ui->saveBtn, &QPushButton::clicked, this, &BotScriptEditor::onSave);
    connect(m_ui->revertBtn, &QPushButton::clicked, this, &BotScriptEditor::onRevert);
    connect(m_ui->importBtn, &QPushButton::clicked, this, &BotScriptEditor::onImport);
    connect(m_ui->exportBtn, &QPushButton::clicked, this, &BotScriptEditor::onExport);
    connect(m_ui->runBtn, &QPushButton::clicked, this, &BotScriptEditor::onRun);
    connect(m_ui->stopBtn, &QPushButton::clicked, this, &BotScriptEditor::onStop);
    connect(BotScriptStore::instance(), &BotScriptStore::changed, this, &BotScriptEditor::onStoreChanged);

    m_ui->propStack->setCurrentWidget(m_ui->pageEmpty);
    updateCondFieldVisibility();
    refreshButtons();
}

BotScriptEditor::~BotScriptEditor()
{
    delete m_ui;
}

static StepModel* treeModel(QTreeView* view)
{
    return static_cast<StepModel*>(view->model());
}

void BotScriptEditor::setBot(const QString& botName, const QString& scriptName, bool connected)
{
    m_botName = botName;
    m_connected = connected;
    auto* store = BotScriptStore::instance();
    if (!scriptName.isEmpty() && store->contains(scriptName)) {
        m_assignedScript = scriptName;
        rebuildFromScript(store->script(scriptName));
        m_saved = true;
    } else {
        m_assignedScript.clear();
        BotScript blank;
        blank.name = QString("script-%1").arg(m_nameCounter);
        while (store->contains(blank.name))
            blank.name = QString("script-%1").arg(++m_nameCounter);
        rebuildFromScript(blank);
        m_saved = false;
    }
    refreshButtons();
}

void BotScriptEditor::rebuildFromScript(const BotScript& script)
{
    QSignalBlocker block(m_ui->stepsTree);
    m_steps = script.steps;
    treeModel(m_ui->stepsTree)->setRoot(&m_steps);
    {
        QSignalBlocker b2(m_ui->nameEdit);
        m_ui->nameEdit->setText(script.name);
    }
    m_ui->propStack->setCurrentWidget(m_ui->pageEmpty);
    refreshButtons();
}

BotScript BotScriptEditor::currentScript() const
{
    BotScript script;
    script.version = 1;
    script.name = m_ui->nameEdit->text().trimmed();
    script.steps = m_steps;
    return script;
}

bool BotScriptEditor::validate(QString& error) const
{
    // reuse the exact parser rules by round-tripping
    BotScript built = currentScript();
    BotScript parsed;
    return BotScriptParser::parse(BotScriptParser::serialize(built), parsed, error);
}

void BotScriptEditor::onSelectionChanged()
{
    showPropertiesFor();
    refreshButtons();
}

void BotScriptEditor::showPropertiesFor()
{
    const QModelIndex current = m_ui->stepsTree->currentIndex();
    BotScriptStep* step = treeModel(m_ui->stepsTree)->stepAt(current);
    // clear all error labels first
    for (auto* label : { m_ui->textError, m_ui->waitError, m_ui->chatError, m_ui->playerError, m_ui->loopError,
                         m_ui->setError, m_ui->incError, m_ui->ifError })
        label->clear();
    if (!step) {
        m_ui->propStack->setCurrentWidget(m_ui->pageEmpty);
        return;
    }
    const QSignalBlocker b1(m_ui->textEdit), b2(m_ui->secondsSpin), b3(m_ui->patternEdit), b4(m_ui->caseCheck),
          b5(m_ui->timeoutSpin), b6(m_ui->playerEdit), b7(m_ui->timeoutSpin2), b8(m_ui->timesSpin),
          b9(m_ui->setNameEdit), b10(m_ui->setValueEdit), b11(m_ui->incNameEdit), b12(m_ui->incValueEdit),
          b13(m_ui->condTypeCombo), b14(m_ui->condValueEdit), b15(m_ui->condVarEdit),
          b16(m_ui->condNumberSpin), b17(m_ui->condTimeoutSpin);
    if (step->type == "say" || step->type == "command" || step->type == "log") {
        m_ui->propStack->setCurrentWidget(m_ui->pageText);
        m_ui->textEdit->setText(step->text);
    } else if (step->type == "wait") {
        m_ui->propStack->setCurrentWidget(m_ui->pageWait);
        m_ui->secondsSpin->setValue(step->seconds);
    } else if (step->type == "wait_for_chat") {
        m_ui->propStack->setCurrentWidget(m_ui->pageChat);
        m_ui->patternEdit->setText(step->pattern);
        m_ui->caseCheck->setChecked(step->caseSensitive);
        m_ui->timeoutSpin->setValue(step->timeoutSeconds);
    } else if (step->type == "wait_for_player") {
        m_ui->propStack->setCurrentWidget(m_ui->pagePlayer);
        m_ui->playerEdit->setText(step->player);
        m_ui->timeoutSpin2->setValue(step->timeoutSeconds);
    } else if (step->type == "loop") {
        m_ui->propStack->setCurrentWidget(m_ui->pageLoop);
        m_ui->timesSpin->setValue(step->times);
    } else if (step->type == "set") {
        m_ui->propStack->setCurrentWidget(m_ui->pageSet);
        m_ui->setNameEdit->setText(step->name);
        m_ui->setValueEdit->setText(step->value);
    } else if (step->type == "inc") {
        m_ui->propStack->setCurrentWidget(m_ui->pageInc);
        m_ui->incNameEdit->setText(step->name);
        m_ui->incValueEdit->setText(step->value);
    } else if (step->type == "if") {
        m_ui->propStack->setCurrentWidget(m_ui->pageIf);
        m_ui->condTypeCombo->setCurrentText(step->condType.isEmpty() ? "chat_contains" : step->condType);
        m_ui->condValueEdit->setText(step->condValue);
        m_ui->condVarEdit->setText(step->condVar);
        m_ui->condNumberSpin->setValue(step->condNumber);
        m_ui->condTimeoutSpin->setValue(step->timeoutSeconds);
        updateCondFieldVisibility();
    }
}

void BotScriptEditor::updateCondFieldVisibility()
{
    const QString t = m_ui->condTypeCombo->currentText();
    // which operand fields the chosen condition type actually uses
    const bool wantsValue = t == "chat_contains" || t == "chat_equals" || t == "player_joined" ||
                            t == "var_equals" || t == "var_not_equals";
    const bool wantsVar = t == "var_equals" || t == "var_not_equals" || t == "var_gt" || t == "var_gte";
    const bool wantsNumber = t == "var_gt" || t == "var_gte" || t == "health_below";
    m_ui->condValueLabel->setVisible(wantsValue);
    m_ui->condValueEdit->setVisible(wantsValue);
    m_ui->condVarLabel->setVisible(wantsVar);
    m_ui->condVarEdit->setVisible(wantsVar);
    m_ui->condNumberLabel->setVisible(wantsNumber);
    m_ui->condNumberSpin->setVisible(wantsNumber);
}

void BotScriptEditor::onFieldEdited()
{
    m_ui->saveStatusLabel->setVisible(false);
    BotScriptStep* step = treeModel(m_ui->stepsTree)->stepAt(m_ui->stepsTree->currentIndex());
    if (step) {
        if (step->type == "say" || step->type == "command" || step->type == "log")
            step->text = m_ui->textEdit->text();
        else if (step->type == "wait")
            step->seconds = m_ui->secondsSpin->value();
        else if (step->type == "wait_for_chat") {
            step->pattern = m_ui->patternEdit->text();
            step->caseSensitive = m_ui->caseCheck->isChecked();
            step->timeoutSeconds = m_ui->timeoutSpin->value();
        } else if (step->type == "wait_for_player") {
            step->player = m_ui->playerEdit->text();
            step->timeoutSeconds = m_ui->timeoutSpin2->value();
        } else if (step->type == "loop")
            step->times = m_ui->timesSpin->value();
        else if (step->type == "set") {
            step->name = m_ui->setNameEdit->text();
            step->value = m_ui->setValueEdit->text();
        } else if (step->type == "inc") {
            step->name = m_ui->incNameEdit->text();
            step->value = m_ui->incValueEdit->text();
        } else if (step->type == "if") {
            step->condType = m_ui->condTypeCombo->currentText();
            step->condValue = m_ui->condValueEdit->text();
            step->condVar = m_ui->condVarEdit->text();
            step->condNumber = m_ui->condNumberSpin->value();
            step->timeoutSeconds = m_ui->condTimeoutSpin->value();
        }
        const QModelIndex idx = m_ui->stepsTree->currentIndex();
        emit treeModel(m_ui->stepsTree)->dataChanged(idx, idx);
    }
    m_saved = false;
    refreshButtons();
}

void BotScriptEditor::onAddStep()
{
    QMenu menu(this);
    for (const char* t : STEP_TYPES)
        menu.addAction(QString(t));
    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen)
        return;
    auto* model = treeModel(m_ui->stepsTree);
    const QModelIndex current = m_ui->stepsTree->currentIndex();
    const QModelIndex parent = current.isValid() ? current.parent() : QModelIndex();
    int row = current.isValid() ? current.row() + 1 : model->rowCount(parent);
    // loops and ifs accept children when expanded; otherwise insert beside selection
    if (current.isValid()) {
        BotScriptStep* sel = model->stepAt(current);
        if (sel && sel->type == "loop" && m_ui->stepsTree->isExpanded(current)) {
            model->insertStep(current, model->rowCount(current), defaultStep(chosen->text()));
            m_ui->stepsTree->expand(current);
            m_saved = false;
            refreshButtons();
            return;
        }
        // an expanded 'if' takes new steps at the end of its then branch;
        // with an else row selected, the code below lands them in the else
        // branch instead (row past the then-branch length)
        if (sel && sel->type == "if" && m_ui->stepsTree->isExpanded(current)) {
            model->insertStep(current, sel->steps.size(), defaultStep(chosen->text()));
            m_ui->stepsTree->expand(current);
            m_saved = false;
            refreshButtons();
            return;
        }
    }
    model->insertStep(parent, row, defaultStep(chosen->text()));
    m_saved = false;
    refreshButtons();
}

void BotScriptEditor::onRemoveStep()
{
    const QModelIndex current = m_ui->stepsTree->currentIndex();
    if (!current.isValid())
        return;
    treeModel(m_ui->stepsTree)->removeStep(current);
    m_saved = false;
    refreshButtons();
}

void BotScriptEditor::onMoveStep(int direction)
{
    const QModelIndex current = m_ui->stepsTree->currentIndex();
    if (!current.isValid())
        return;
    auto* model = treeModel(m_ui->stepsTree);
    const int row = current.row();
    const int dest = row + direction;
    if (dest < 0 || dest >= model->rowCount(current.parent()))
        return;
    if (model->moveStep(current.parent(), row, dest)) {
        m_saved = false;
        refreshButtons();
    }
}

void BotScriptEditor::onSave()
{
    BotScript current = currentScript();
    QString error;
    BotScript parsed;
    if (!BotScriptParser::parse(BotScriptParser::serialize(current), parsed, error)) {
        refreshButtons();
        return;
    }
    auto* store = BotScriptStore::instance();
    if (!store->save(parsed, error)) {
        m_ui->saveStatusLabel->setText(tr("Save failed: %1").arg(error));
        m_ui->saveStatusLabel->setStyleSheet("color: #f87171;");
        m_ui->saveStatusLabel->setVisible(true);
        refreshButtons();
        return;
    }
    m_ui->saveStatusLabel->setText(tr("Saved."));
    m_ui->saveStatusLabel->setStyleSheet("color: #6ee7b7;");
    m_ui->saveStatusLabel->setVisible(true);
    QTimer::singleShot(3000, m_ui->saveStatusLabel, [w = m_ui->saveStatusLabel] { w->setVisible(false); });
    m_saved = true;
    m_assignedScript = parsed.name;
    m_manager->setBotScript(m_botName, parsed.name);

    // Only rebuild the tree when the parser changed something
    // (e.g. added a leading '/' to a command step). Otherwise leave
    // the tree alone so expanded loops and the current selection
    // survive the save.
    const QByteArray beforeJson =
        QJsonDocument(BotScriptParser::serialize(current)).toJson(QJsonDocument::Compact);
    const QByteArray afterJson =
        QJsonDocument(BotScriptParser::serialize(parsed)).toJson(QJsonDocument::Compact);
    if (beforeJson != afterJson)
        rebuildFromScript(parsed);
}

void BotScriptEditor::onRevert()
{
    auto* store = BotScriptStore::instance();
    if (!m_assignedScript.isEmpty() && store->contains(m_assignedScript)) {
        rebuildFromScript(store->script(m_assignedScript));
        m_saved = true;
    } else {
        setBot(m_botName, m_assignedScript, m_connected);
    }
    refreshButtons();
}

void BotScriptEditor::onRun()
{
    if (!m_connected || !m_saved)
        return;
    QString error;
    if (!validate(error))
        return;
    m_manager->runScriptFor(m_botName, currentScript());
}

void BotScriptEditor::onStop()
{
    if (m_botName.isEmpty())
        return;
    m_manager->stopScriptFor(m_botName);
}

void BotScriptEditor::onStoreChanged()
{
    // another window changed the store; reload only when showing saved state
    if (m_saved && !m_assignedScript.isEmpty()) {
        auto* store = BotScriptStore::instance();
        if (store->contains(m_assignedScript))
            rebuildFromScript(store->script(m_assignedScript));
    }
}

void BotScriptEditor::refreshButtons()
{
    QString error;
    const bool valid = validate(error);
    if (!valid) {
        // jump to the offending step so the error lands on its page
        QString stepError;
        const QModelIndex bad = findFirstInvalidStep(QModelIndex(), 0, stepError);
        if (bad.isValid() && bad != m_ui->stepsTree->currentIndex())
            m_ui->stepsTree->setCurrentIndex(bad);
        if (bad.isValid())
            error = stepError;
    }
    // surface the error on the visible property page (re-read: the
    // jump above may have switched it)
    QWidget* page = m_ui->propStack->currentWidget();
    QLabel* target = nullptr;
    if (page == m_ui->pageText)
        target = m_ui->textError;
    else if (page == m_ui->pageWait)
        target = m_ui->waitError;
    else if (page == m_ui->pageChat)
        target = m_ui->chatError;
    else if (page == m_ui->pagePlayer)
        target = m_ui->playerError;
    else if (page == m_ui->pageLoop)
        target = m_ui->loopError;
    else if (page == m_ui->pageSet)
        target = m_ui->setError;
    else if (page == m_ui->pageInc)
        target = m_ui->incError;
    else if (page == m_ui->pageIf)
        target = m_ui->ifError;
    if (target)
        target->setText(valid ? QString() : error);
    const bool hasBot = !m_botName.isEmpty();
    m_ui->saveBtn->setEnabled(valid && hasBot);
    m_ui->runBtn->setEnabled(valid && m_saved && m_connected && hasBot && m_ui->enabledCheck->isChecked());
    m_ui->stopBtn->setEnabled(m_connected && hasBot);
    const bool hasSelection = m_ui->stepsTree->currentIndex().isValid();
    m_ui->removeBtn->setEnabled(hasSelection);
    m_ui->upBtn->setEnabled(hasSelection);
    m_ui->downBtn->setEnabled(hasSelection);
}

void BotScriptEditor::setConnected(const QString& botName, bool connected)
{
    if (botName != m_botName)
        return;
    m_connected = connected;
    refreshButtons();
}

QModelIndex BotScriptEditor::findFirstInvalidStep(const QModelIndex& parent, int depth, QString& error) const
{
    auto* model = treeModel(m_ui->stepsTree);
    const int rows = model->rowCount(parent);
    for (int r = 0; r < rows; r++) {
        const QModelIndex idx = model->index(r, 0, parent);
        BotScriptStep* step = model->stepAt(idx);
        if (!step)
            continue;
        // both loops and conditionals occupy one nesting level
        if ((step->type == "loop" || step->type == "if") && depth >= BotScriptParser::kMaxLoopDepth) {
            error = step->type == "loop" ? "Loops nested deeper than 5 levels are not allowed."
                                         : "Conditionals nested deeper than 5 levels are not allowed.";
            return idx;
        }
        if (!BotScriptParser::validateStep(*step, error))
            return idx;
        if (step->type == "loop" || step->type == "if") {
            // child rows cover the then branch and (for ifs) the else branch
            const QModelIndex nested = findFirstInvalidStep(idx, depth + 1, error);
            if (nested.isValid())
                return nested;
        }
    }
    return {};
}

void BotScriptEditor::onImport()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Bot Script"), QString(),
                                                      tr("Bot scripts (*.json);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_ui->saveStatusLabel->setText(tr("Import failed: cannot open %1").arg(path));
        m_ui->saveStatusLabel->setStyleSheet("color: #f87171;");
        m_ui->saveStatusLabel->setVisible(true);
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        m_ui->saveStatusLabel->setText(tr("Import failed: %1 is not a JSON script").arg(path));
        m_ui->saveStatusLabel->setStyleSheet("color: #f87171;");
        m_ui->saveStatusLabel->setVisible(true);
        return;
    }
    BotScript script;
    QString error;
    if (!BotScriptParser::parse(doc.object(), script, error)) {
        m_ui->saveStatusLabel->setText(tr("Import failed: %1").arg(error));
        m_ui->saveStatusLabel->setStyleSheet("color: #f87171;");
        m_ui->saveStatusLabel->setVisible(true);
        return;
    }
    auto* store = BotScriptStore::instance();
    if (store->contains(script.name)) {
        // saving would overwrite the stored script of the same name; ask first
        const auto answer = QMessageBox::question(this, tr("Import Bot Script"),
            tr("A script named '%1' already exists. Importing it will overwrite that script when you save. Continue?")
                .arg(script.name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    // load it into the editor as unsaved work, then offer to store it
    rebuildFromScript(script);
    m_saved = false;
    refreshButtons();
    if (QMessageBox::question(this, tr("Import Bot Script"),
                              tr("Save the imported script to the script store now?"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
        onSave();
}

void BotScriptEditor::onExport()
{
    QString error;
    if (!validate(error)) {
        QMessageBox::warning(this, tr("Export Bot Script"), tr("The script is not valid: %1").arg(error));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Bot Script"), QString(),
                                                      tr("Bot scripts (*.json);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        m_ui->saveStatusLabel->setText(tr("Export failed: %1").arg(f.errorString()));
        m_ui->saveStatusLabel->setStyleSheet("color: #f87171;");
        m_ui->saveStatusLabel->setVisible(true);
        return;
    }
    const QByteArray data = QJsonDocument(BotScriptParser::serialize(currentScript())).toJson(QJsonDocument::Indented);
    if (f.write(data) < 0) {
        m_ui->saveStatusLabel->setText(tr("Export failed: %1").arg(f.errorString()));
        m_ui->saveStatusLabel->setStyleSheet("color: #f87171;");
        m_ui->saveStatusLabel->setVisible(true);
        return;
    }
    m_ui->saveStatusLabel->setText(tr("Exported to %1").arg(path));
    m_ui->saveStatusLabel->setStyleSheet("color: #6ee7b7;");
    m_ui->saveStatusLabel->setVisible(true);
    QTimer::singleShot(3000, m_ui->saveStatusLabel, [w = m_ui->saveStatusLabel] { w->setVisible(false); });
}
