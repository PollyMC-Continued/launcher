#pragma once

// Bot script model, parser/serializer, and on-disk store.
// QtCore only, so the unit test links without the launcher.

#include <QList>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

struct BotScriptStep {
    QString type;
    QString text;            // say / command / log
    int seconds = 0;         // wait
    QString pattern;         // wait_for_chat
    bool caseSensitive = false;
    QString player;          // wait_for_player
    int timeoutSeconds = 0;  // waits, 0 = forever; if: condition timeout
    int times = 1;           // loop, -1 = forever
    QList<BotScriptStep> steps;      // loop / if then-branch
    QString name;            // set / inc: variable name
    QString value;           // set / inc: literal value (inc: integer string)
    QString condType;        // if: condition type
    QString condValue;       // if: string operand (chat text, player, expected var value)
    QString condVar;         // if: variable name for var_* conditions
    int condNumber = 0;      // if: numeric operand (var_gt, var_gte, health_below)
    QList<BotScriptStep> elseSteps;  // if: else-branch
};

struct BotScript {
    int version = 1;
    QString name;
    QList<BotScriptStep> steps;
};

class BotScriptParser {
   public:
    static bool parse(const QJsonObject& obj, BotScript& out, QString& error);
    static QJsonObject serialize(const BotScript& script);
    static QJsonArray serializeSteps(const QList<BotScriptStep>& steps);
    static bool isValidName(const QString& name);
    // Variable names for set/inc/var_* conditions: shared by parser and editor.
    static bool isValidVarName(const QString& name);
    // Single-step field rules, shared by parse() and the editor.
    static bool validateStep(const BotScriptStep& step, QString& error);

    static const int kMaxLoopDepth = 5;
    static const int kMaxWaitSeconds = 86400;
    static const int kMaxLoopTimes = 100000;
    static const int kMaxValueLength = 512;  // set value / condition string operands
    static const int kMaxVarNameLength = 32;
    static const int kMaxVariables = 64;      // distinct variables per script
    static const int kMaxIncValue = 1000000;  // inc delta magnitude
    static const int kMaxCondNumber = 1000000;  // var_gt / var_gte threshold magnitude
    static const int kMaxHealthBelow = 1000;    // health_below threshold (absorption-safe)

   private:
    static bool parseSteps(const QJsonArray& arr, QList<BotScriptStep>& out, QString& error, int depth, QStringList& vars);
};

class BotScriptStore : public QObject {
    Q_OBJECT
   public:
    static BotScriptStore* instance();

    QList<BotScript> scripts() const;
    BotScript script(const QString& name) const;
    bool contains(const QString& name) const;

    bool save(const BotScript& script, QString& error);
    bool remove(const QString& name, QString& error);

    // Test seam: explicit path. Production sets the data-dir path once.
    void setFilePath(const QString& path);

    // Missing file is not an error; it results in an empty store.
    void load();
    bool saveNow();

   signals:
    void changed();

   private:
    explicit BotScriptStore(QObject* parent = nullptr);

    QString m_path;
    QMap<QString, BotScript> m_scripts;
};
