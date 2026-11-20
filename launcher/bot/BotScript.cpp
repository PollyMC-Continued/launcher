#include "BotScript.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <cmath>

static const QStringList kStepTypes = { "say", "command", "wait", "wait_for_chat", "wait_for_player", "log", "loop", "set", "inc", "if" };

// Condition types accepted by 'if' steps (kept in sync with bot-server/script-runner.js).
static const QStringList kConditionTypes = { "chat_contains", "chat_equals", "var_equals", "var_not_equals",
                                             "var_gt", "var_gte", "player_joined", "health_below" };

bool BotScriptParser::isValidName(const QString& name)
{
    static const QRegularExpression re("^[a-zA-Z0-9_-]{1,64}$");
    return re.match(name).hasMatch();
}

bool BotScriptParser::isValidVarName(const QString& name)
{
    static const QRegularExpression re("^[a-zA-Z0-9_]{1,32}$");
    return re.match(name).hasMatch();
}

bool BotScriptParser::parse(const QJsonObject& obj, BotScript& out, QString& error)
{
    if (obj["version"].toInt(-1) != 1) {
        error = "Unsupported script version (want 1).";
        return false;
    }
    const QString name = obj["name"].toString();
    if (!isValidName(name)) {
        error = "Script name must be 1-64 chars of letters, digits, _ or -.";
        return false;
    }
    if (!obj["steps"].isArray() || obj["steps"].toArray().isEmpty()) {
        error = "Script must contain at least one step.";
        return false;
    }
    QList<BotScriptStep> steps;
    QStringList vars;
    if (!parseSteps(obj["steps"].toArray(), steps, error, 0, vars))
        return false;
    if (vars.size() > kMaxVariables) {
        error = QString("Scripts may use at most %1 distinct variables.").arg(kMaxVariables);
        return false;
    }
    out.version = 1;
    out.name = name;
    out.steps = steps;
    return true;
}

static bool rejectKeys(const QJsonObject& obj, const QStringList& allowed, QString& error)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!allowed.contains(it.key())) {
            error = QString("Unknown key '%1'.").arg(it.key());
            return false;
        }
    }
    return true;
}

bool BotScriptParser::validateStep(const BotScriptStep& s, QString& error)
{
    if (!kStepTypes.contains(s.type)) {
        error = QString("Unknown step type '%1'.").arg(s.type);
        return false;
    }
    if (s.type == "say" || s.type == "log") {
        if (s.text.isEmpty()) {
            error = QString("Step '%1' needs non-empty text.").arg(s.type);
            return false;
        }
    } else if (s.type == "command") {
        if (s.text.isEmpty()) {
            error = "Step 'command' needs non-empty text.";
            return false;
        }
    } else if (s.type == "wait") {
        if (s.seconds <= 0 || s.seconds > kMaxWaitSeconds) {
            error = "Step 'wait' needs seconds between 1 and 86400.";
            return false;
        }
    } else if (s.type == "wait_for_chat") {
        if (s.pattern.isEmpty()) {
            error = "Step 'wait_for_chat' needs a non-empty pattern.";
            return false;
        }
        if (s.timeoutSeconds < 0) {
            error = "Step 'wait_for_chat' needs timeout_seconds >= 0.";
            return false;
        }
    } else if (s.type == "wait_for_player") {
        if (s.player.isEmpty()) {
            error = "Step 'wait_for_player' needs a non-empty player.";
            return false;
        }
        if (s.timeoutSeconds < 0) {
            error = "Step 'wait_for_player' needs timeout_seconds >= 0.";
            return false;
        }
    } else if (s.type == "loop") {
        if (s.times != -1 && s.times <= 0) {
            error = "Step 'loop' needs times -1 or > 0.";
            return false;
        }
        if (s.times != -1 && s.times > kMaxLoopTimes) {
            error = QString("Step 'loop' times cannot exceed %1 (use -1 for forever).")
                        .arg(kMaxLoopTimes);
            return false;
        }
        if (s.steps.isEmpty()) {
            error = "Step 'loop' needs a non-empty steps array.";
            return false;
        }
    } else if (s.type == "set") {
        if (!isValidVarName(s.name)) {
            error = "Step 'set' needs a variable name of 1-32 letters, digits or '_' only.";
            return false;
        }
        if (s.value.size() > kMaxValueLength) {
            error = QString("Step 'set' value cannot exceed %1 characters.").arg(kMaxValueLength);
            return false;
        }
    } else if (s.type == "inc") {
        if (!isValidVarName(s.name)) {
            error = "Step 'inc' needs a variable name of 1-32 letters, digits or '_' only.";
            return false;
        }
        static const QRegularExpression intRe("^-?[0-9]{1,7}$");
        if (!intRe.match(s.value).hasMatch()) {
            error = QString("Step 'inc' needs an integer value between -%1 and %1.").arg(kMaxIncValue);
            return false;
        }
        bool ok = false;
        const int delta = s.value.toInt(&ok);
        if (!ok || qAbs(delta) > kMaxIncValue) {
            error = QString("Step 'inc' needs an integer value between -%1 and %1.").arg(kMaxIncValue);
            return false;
        }
    } else if (s.type == "if") {
        if (!kConditionTypes.contains(s.condType)) {
            error = QString("Step 'if' condition type '%1' is not known.").arg(s.condType);
            return false;
        }
        if (s.timeoutSeconds < 0 || s.timeoutSeconds > kMaxWaitSeconds) {
            error = "Step 'if' needs timeout_seconds between 0 and 86400.";
            return false;
        }
        if (s.condType == "chat_contains" || s.condType == "chat_equals" || s.condType == "player_joined") {
            if (s.condValue.isEmpty()) {
                error = QString("Condition '%1' needs a non-empty value.").arg(s.condType);
                return false;
            }
            if (s.condValue.size() > kMaxValueLength) {
                error = QString("Condition '%1' value cannot exceed %2 characters.").arg(s.condType).arg(kMaxValueLength);
                return false;
            }
        } else if (s.condType == "var_equals" || s.condType == "var_not_equals") {
            if (!isValidVarName(s.condVar)) {
                error = QString("Condition '%1' needs a variable name of 1-32 letters, digits or '_' only.")
                            .arg(s.condType);
                return false;
            }
            if (s.condValue.size() > kMaxValueLength) {
                error = QString("Condition '%1' value cannot exceed %2 characters.").arg(s.condType).arg(kMaxValueLength);
                return false;
            }
        } else if (s.condType == "var_gt" || s.condType == "var_gte") {
            if (!isValidVarName(s.condVar)) {
                error = QString("Condition '%1' needs a variable name of 1-32 letters, digits or '_' only.")
                            .arg(s.condType);
                return false;
            }
            if (qAbs(s.condNumber) > kMaxCondNumber) {
                error = QString("Condition '%1' number cannot exceed %2 in magnitude.").arg(s.condType).arg(kMaxCondNumber);
                return false;
            }
        } else {  // health_below
            if (s.condNumber < 0 || s.condNumber > kMaxHealthBelow) {
                error = QString("Condition 'health_below' needs a number between 0 and %1.").arg(kMaxHealthBelow);
                return false;
            }
        }
    }
    return true;
}

bool BotScriptParser::parseSteps(const QJsonArray& arr, QList<BotScriptStep>& out, QString& error, int depth, QStringList& vars)
{
    for (const auto& v : arr) {
        if (!v.isObject()) {
            error = "Every step must be an object.";
            return false;
        }
        const QJsonObject o = v.toObject();
        const QString type = o["type"].toString();
        if (!kStepTypes.contains(type)) {
            error = QString("Unknown step type '%1'.").arg(type);
            return false;
        }
        BotScriptStep s;
        s.type = type;
        if (type == "say" || type == "log") {
            if (!rejectKeys(o, { "type", "text" }, error)) return false;
            s.text = o["text"].toString();
        } else if (type == "command") {
            if (!rejectKeys(o, { "type", "text" }, error)) return false;
            s.text = o["text"].toString();
            if (!s.text.isEmpty() && !s.text.startsWith('/'))
                s.text.prepend('/');
        } else if (type == "wait") {
            if (!rejectKeys(o, { "type", "seconds" }, error)) return false;
            s.seconds = o["seconds"].toInt(-1);
        } else if (type == "wait_for_chat") {
            if (!rejectKeys(o, { "type", "pattern", "case_sensitive", "timeout_seconds" }, error)) return false;
            s.pattern = o["pattern"].toString();
            s.caseSensitive = o["case_sensitive"].toBool(false);
            s.timeoutSeconds = o["timeout_seconds"].toInt(0);
        } else if (type == "wait_for_player") {
            if (!rejectKeys(o, { "type", "player", "timeout_seconds" }, error)) return false;
            s.player = o["player"].toString();
            s.timeoutSeconds = o["timeout_seconds"].toInt(0);
        } else if (type == "loop") {
            if (!rejectKeys(o, { "type", "times", "steps" }, error)) return false;
            if (depth >= kMaxLoopDepth) {
                error = "Loops nested deeper than 5 levels are not allowed.";
                return false;
            }
            s.times = o["times"].toInt(0);
            if (!o["steps"].isArray()) {
                error = "Step 'loop' needs a non-empty steps array.";
                return false;
            }
            if (!parseSteps(o["steps"].toArray(), s.steps, error, depth + 1, vars))
                return false;
        } else if (type == "set") {
            if (!rejectKeys(o, { "type", "name", "value" }, error)) return false;
            if (o.contains("name") && !o["name"].isString()) {
                error = "Step 'set' name must be a string.";
                return false;
            }
            if (o.contains("value") && !o["value"].isString()) {
                error = "Step 'set' value must be a string.";
                return false;
            }
            s.name = o["name"].toString();
            s.value = o["value"].toString();
        } else if (type == "inc") {
            if (!rejectKeys(o, { "type", "name", "value" }, error)) return false;
            if (o.contains("name") && !o["name"].isString()) {
                error = "Step 'inc' name must be a string.";
                return false;
            }
            if (o.contains("value") && !o["value"].isString()) {
                error = "Step 'inc' value must be a string.";
                return false;
            }
            s.name = o["name"].toString();
            s.value = o["value"].toString();
        } else if (type == "if") {
            if (!rejectKeys(o, { "type", "condition", "steps", "else_steps" }, error)) return false;
            if (depth >= kMaxLoopDepth) {
                error = "Conditionals nested deeper than 5 levels are not allowed.";
                return false;
            }
            if (!o["condition"].isObject()) {
                error = "Step 'if' needs a condition object.";
                return false;
            }
            const QJsonObject cond = o["condition"].toObject();
            if (!rejectKeys(cond, { "type", "value", "var", "number", "timeout_seconds" }, error))
                return false;
            if (!cond["type"].isString()) {
                error = "Step 'if' condition type must be a string.";
                return false;
            }
            if (cond.contains("value") && !cond["value"].isString()) {
                error = "Step 'if' condition value must be a string.";
                return false;
            }
            if (cond.contains("var") && !cond["var"].isString()) {
                error = "Step 'if' condition var must be a string.";
                return false;
            }
            // numeric fields must be JSON numbers, and whole ones: toInt()
            // silently returns the default for 4.5 or a string otherwise
            for (const char* key : { "number", "timeout_seconds" }) {
                if (cond.contains(key)) {
                    const QJsonValue v = cond[key];
                    if (!v.isDouble() || std::floor(v.toDouble()) != v.toDouble()) {
                        error = QString("Step 'if' condition field '%1' must be a whole number.").arg(key);
                        return false;
                    }
                }
            }
            s.condType = cond["type"].toString();
            s.condValue = cond["value"].toString();
            s.condVar = cond["var"].toString();
            s.condNumber = cond["number"].toInt(0);
            s.timeoutSeconds = cond["timeout_seconds"].toInt(0);
            // the then-branch may be empty (a pure not-block); the key must exist
            if (!o["steps"].isArray()) {
                error = "Step 'if' needs a steps array.";
                return false;
            }
            if (!parseSteps(o["steps"].toArray(), s.steps, error, depth + 1, vars))
                return false;
            if (o.contains("else_steps")) {
                if (!o["else_steps"].isArray()) {
                    error = "Step 'if' else_steps must be an array.";
                    return false;
                }
                if (!parseSteps(o["else_steps"].toArray(), s.elseSteps, error, depth + 1, vars))
                    return false;
            }
        }
        if (!validateStep(s, error))
            return false;
        // track variable usage for the distinct-variable cap
        if (type == "set" || type == "inc") {
            if (!vars.contains(s.name))
                vars.append(s.name);
        } else if (type == "if" &&
                   (s.condType == "var_equals" || s.condType == "var_not_equals" ||
                    s.condType == "var_gt" || s.condType == "var_gte")) {
            if (!vars.contains(s.condVar))
                vars.append(s.condVar);
        }
        out.append(s);
    }
    return true;
}

static QJsonObject serializeStep(const BotScriptStep& s)
{
    QJsonObject o;
    o["type"] = s.type;
    if (s.type == "say" || s.type == "command" || s.type == "log") {
        o["text"] = s.text;
    } else if (s.type == "wait") {
        o["seconds"] = s.seconds;
    } else if (s.type == "wait_for_chat") {
        o["pattern"] = s.pattern;
        o["case_sensitive"] = s.caseSensitive;
        o["timeout_seconds"] = s.timeoutSeconds;
    } else if (s.type == "wait_for_player") {
        o["player"] = s.player;
        o["timeout_seconds"] = s.timeoutSeconds;
    } else if (s.type == "loop") {
        o["times"] = s.times;
        o["steps"] = BotScriptParser::serializeSteps(s.steps);
    } else if (s.type == "set" || s.type == "inc") {
        o["name"] = s.name;
        o["value"] = s.value;
    } else if (s.type == "if") {
        QJsonObject cond;
        cond["type"] = s.condType;
        // only the operand fields the condition type uses are written
        if (s.condType == "chat_contains" || s.condType == "chat_equals" || s.condType == "player_joined" ||
            s.condType == "var_equals" || s.condType == "var_not_equals") {
            cond["value"] = s.condValue;
        }
        if (s.condType == "var_equals" || s.condType == "var_not_equals" ||
            s.condType == "var_gt" || s.condType == "var_gte") {
            cond["var"] = s.condVar;
        }
        if (s.condType == "var_gt" || s.condType == "var_gte" || s.condType == "health_below") {
            cond["number"] = s.condNumber;
        }
        cond["timeout_seconds"] = s.timeoutSeconds;
        o["condition"] = cond;
        o["steps"] = BotScriptParser::serializeSteps(s.steps);
        o["else_steps"] = BotScriptParser::serializeSteps(s.elseSteps);
    }
    return o;
}

QJsonObject BotScriptParser::serialize(const BotScript& script)
{
    QJsonObject o;
    o["version"] = 1;
    o["name"] = script.name;
    o["steps"] = serializeSteps(script.steps);
    return o;
}

QJsonArray BotScriptParser::serializeSteps(const QList<BotScriptStep>& steps)
{
    QJsonArray arr;
    for (const auto& s : steps)
        arr.append(serializeStep(s));
    return arr;
}

BotScriptStore::BotScriptStore(QObject* parent) : QObject(parent) {}

BotScriptStore* BotScriptStore::instance()
{
    static BotScriptStore* s_instance = new BotScriptStore();
    return s_instance;
}

void BotScriptStore::setFilePath(const QString& path)
{
    m_path = path;
}

QList<BotScript> BotScriptStore::scripts() const
{
    return m_scripts.values();
}

BotScript BotScriptStore::script(const QString& name) const
{
    return m_scripts.value(name);
}

bool BotScriptStore::contains(const QString& name) const
{
    return m_scripts.contains(name);
}

bool BotScriptStore::save(const BotScript& script, QString& error)
{
    BotScript parsed;
    QString parseError;
    if (!BotScriptParser::parse(BotScriptParser::serialize(script), parsed, parseError)) {
        error = parseError;
        return false;
    }
    m_scripts[parsed.name] = parsed;
    if (!saveNow()) {
        error = "Failed to write script store.";
        return false;
    }
    emit changed();
    return true;
}

bool BotScriptStore::remove(const QString& name, QString& error)
{
    if (!m_scripts.contains(name)) {
        error = QString("No script named '%1'.").arg(name);
        return false;
    }
    m_scripts.remove(name);
    if (!saveNow()) {
        error = "Failed to write script store.";
        return false;
    }
    emit changed();
    return true;
}

void BotScriptStore::load()
{
    m_scripts.clear();
    if (m_path.isEmpty())
        return;
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    if (root["version"].toInt(-1) != 1)
        return;
    const QJsonObject all = root["scripts"].toObject();
    for (auto it = all.begin(); it != all.end(); ++it) {
        BotScript script;
        QString error;
        if (BotScriptParser::parse(it.value().toObject(), script, error))
            m_scripts[script.name] = script;
        else
            qWarning() << "BotScript: dropping script" << it.key() << "during load:" << error;
    }
    emit changed();
}

bool BotScriptStore::saveNow()
{
    if (m_path.isEmpty())
        return true;
    QJsonObject all;
    for (auto it = m_scripts.begin(); it != m_scripts.end(); ++it)
        all[it.key()] = BotScriptParser::serialize(it.value());
    QJsonObject root;
    root["version"] = 1;
    root["scripts"] = all;
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    if (f.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0)
        return false;
    return f.commit();
}
