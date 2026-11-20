#include <QtTest>

#include "bot/BotScript.h"

#include <QJsonArray>
#include <QJsonDocument>

static QJsonObject parseJson(const char* text)
{
    return QJsonDocument::fromJson(QByteArray(text)).object();
}
class TestBotScript : public QObject {
    Q_OBJECT

   private slots:
    void validFullScript()
    {
        BotScript script;
        QString error;
        const bool ok = BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"farm-1","steps":[
                {"type":"say","text":"hello"},
                {"type":"command","text":"/time query daytime"},
                {"type":"wait","seconds":30},
                {"type":"wait_for_chat","pattern":"day","case_sensitive":true,"timeout_seconds":60},
                {"type":"wait_for_player","player":"Alice","timeout_seconds":0},
                {"type":"log","text":"started"},
                {"type":"loop","times":-1,"steps":[{"type":"say","text":"hi"}]}
            ]})"),
            script, error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(script.steps.size(), 7);
        QCOMPARE(script.steps[1].text, QString("/time query daytime"));
        QCOMPARE(script.steps[6].steps.size(), 1);
    }

    void commandGetsSlash()
    {
        BotScript script;
        QString error;
        QVERIFY(BotScriptParser::parse(parseJson(R"({"version":1,"name":"a","steps":[{"type":"command","text":"time query"}]})"),
                                       script, error));
        QCOMPARE(script.steps[0].text, QString("/time query"));
    }

    void nestedLoopThreeLevels()
    {
        BotScript script;
        QString error;
        const bool ok = BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"n","steps":[
                {"type":"loop","times":2,"steps":[
                    {"type":"loop","times":2,"steps":[
                        {"type":"loop","times":2,"steps":[{"type":"say","text":"deep"}]}
                    ]}
                ]}
            ]})"),
            script, error);
        QVERIFY2(ok, qPrintable(error));
    }

    void rejectBadVersion()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(parseJson(R"({"version":2,"name":"a","steps":[{"type":"say","text":"x"}]})"),
                                        script, error));
    }

    void rejectBadName()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(parseJson(R"({"version":1,"name":"has space","steps":[{"type":"say","text":"x"}]})"),
                                        script, error));
    }

    void rejectEmptySteps()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(parseJson(R"({"version":1,"name":"a","steps":[]})"), script, error));
    }

    void rejectUnknownType()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(parseJson(R"({"version":1,"name":"a","steps":[{"type":"dance","text":"x"}]})"),
                                        script, error));
    }

    void rejectZeroWait()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(parseJson(R"({"version":1,"name":"a","steps":[{"type":"wait","seconds":0}]})"),
                                        script, error));
    }

    void rejectZeroLoop()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"a","steps":[{"type":"loop","times":0,"steps":[{"type":"say","text":"x"}]}]})"),
            script, error));
    }

    void rejectSixLevelLoop()
    {
        const char* inner = R"({"type":"say","text":"x"})";
        QString nested(inner);
        for (int i = 0; i < 6; i++)
            nested = QString(R"({"type":"loop","times":1,"steps":[%1]})").arg(nested);
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(parseJson(QString(R"({"version":1,"name":"a","steps":[%1]})").arg(nested).toUtf8().constData()),
                                        script, error));
    }

    void rejectEmptyCommand()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(parseJson(R"({"version":1,"name":"a","steps":[{"type":"command","text":""}]})"),
                                        script, error));
    }

    void rejectOversizeLoop()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"a","steps":[
                {"type":"loop","times":2000000000,"steps":[{"type":"say","text":"x"}]}
            ]})"),
            script, error));
        QVERIFY(error.contains("100000"));
    }

    void roundTrip()
    {
        const QByteArray raw = R"({"name":"rt","steps":[{"text":"hi","type":"say"},{"steps":[{"seconds":5,"type":"wait"}],"times":3,"type":"loop"}],"version":1})";
        BotScript script;
        QString error;
        QVERIFY2(BotScriptParser::parse(QJsonDocument::fromJson(raw).object(), script, error), qPrintable(error));
        const QByteArray out = QJsonDocument(BotScriptParser::serialize(script)).toJson(QJsonDocument::Compact);
        QCOMPARE(out, raw);
    }

    void validSteps()
    {
        QString error;
        BotScriptStep s;
        s.type = "say";
        s.text = "hi";
        QVERIFY(BotScriptParser::validateStep(s, error));
        s.type = "command";
        QVERIFY(BotScriptParser::validateStep(s, error));
        s.type = "log";
        QVERIFY(BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "wait";
        s.seconds = 5;
        QVERIFY(BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "wait_for_chat";
        s.pattern = "hi";
        QVERIFY(BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "wait_for_player";
        s.player = "Alice";
        QVERIFY(BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "loop";
        s.times = 2;
        s.steps.append(defaultSay());
        QVERIFY(BotScriptParser::validateStep(s, error));
    }

    void invalidSayEmpty()
    {
        QString error;
        BotScriptStep s;
        s.type = "say";
        QVERIFY(!BotScriptParser::validateStep(s, error));
    }

    void invalidWaitZero()
    {
        QString error;
        BotScriptStep s;
        s.type = "wait";
        QVERIFY(!BotScriptParser::validateStep(s, error));
    }

    void invalidLoopEmpty()
    {
        QString error;
        BotScriptStep s;
        s.type = "loop";
        s.times = 2;
        QVERIFY(!BotScriptParser::validateStep(s, error));
    }

    void invalidChatEmptyPattern()
    {
        QString error;
        BotScriptStep s;
        s.type = "wait_for_chat";
        QVERIFY(!BotScriptParser::validateStep(s, error));
    }

    void commandNormalized()
    {
        BotScript script;
        QString error;
        QVERIFY(BotScriptParser::parse(parseJson(R"({"version":1,"name":"a","steps":[{"type":"command","text":"time query"}]})"),
                                       script, error));
        QVERIFY(script.steps[0].text.startsWith('/'));
    }

    void roundTripNewStepTypes()
    {
        // keys in alphabetical order: QJsonDocument sorts object keys on output
        const QByteArray raw =
            R"({"name":"rt2","steps":[)"
            R"({"name":"count","type":"set","value":"0"},)"
            R"({"name":"count","type":"inc","value":"1"},)"
            R"({"condition":{"timeout_seconds":30,"type":"chat_contains","value":"ready"},)"
            R"("else_steps":[{"text":"nope","type":"say"}],)"
            R"("steps":[{"text":"go","type":"say"}],"type":"if"}],)"
            R"("version":1})";
        BotScript script;
        QString error;
        QVERIFY2(BotScriptParser::parse(QJsonDocument::fromJson(raw).object(), script, error), qPrintable(error));
        const QByteArray out = QJsonDocument(BotScriptParser::serialize(script)).toJson(QJsonDocument::Compact);
        QCOMPARE(out, raw);
    }

    void ifConditionFieldsParsed()
    {
        BotScript script;
        QString error;
        const bool ok = BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"c","steps":[
                {"type":"if",
                 "condition":{"type":"var_gt","var":"count","number":5,"timeout_seconds":120},
                 "steps":[{"type":"say","text":"big"}],
                 "else_steps":[{"type":"wait","seconds":5}]}
            ]})"),
            script, error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(script.steps[0].condType, QString("var_gt"));
        QCOMPARE(script.steps[0].condVar, QString("count"));
        QCOMPARE(script.steps[0].condNumber, 5);
        QCOMPARE(script.steps[0].timeoutSeconds, 120);
        QCOMPARE(script.steps[0].steps.size(), 1);
        QCOMPARE(script.steps[0].steps[0].text, QString("big"));
        QCOMPARE(script.steps[0].elseSteps.size(), 1);
        QCOMPARE(script.steps[0].elseSteps[0].seconds, 5);
    }

    void ifWithoutElseDefaultsEmpty()
    {
        BotScript script;
        QString error;
        const bool ok = BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"e","steps":[
                {"type":"if","condition":{"type":"health_below","number":10},
                 "steps":[{"type":"say","text":"ow"}]}
            ]})"),
            script, error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(script.steps[0].elseSteps.size(), 0);
        // serialization always writes the (empty) else_steps array
        const QByteArray out = QJsonDocument(BotScriptParser::serialize(script)).toJson(QJsonDocument::Compact);
        QVERIFY(out.contains("\"else_steps\":[]"));
    }

    void rejectUnknownConditionType()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"a","steps":[
                {"type":"if","condition":{"type":"chat_smells","value":"x"},"steps":[{"type":"say","text":"y"}]}
            ]})"),
            script, error));
    }

    void rejectIfWithoutSteps()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"a","steps":[
                {"type":"if","condition":{"type":"chat_contains","value":"x"}}
            ]})"),
            script, error));
    }

    void rejectDeepIfNesting()
    {
        const char* inner = R"({"type":"say","text":"x"})";
        QString nested(inner);
        for (int i = 0; i < 6; i++)
            nested = QString(R"({"type":"if","condition":{"type":"var_gt","var":"v","number":1},"steps":[%1]})").arg(nested);
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(parseJson(QString(R"({"version":1,"name":"a","steps":[%1]})").arg(nested).toUtf8().constData()),
                                        script, error));
    }

    void rejectBadVariableName()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"a","steps":[{"type":"set","name":"has space","value":"0"}]})"),
            script, error));
        // 33 chars: one over the 32-char variable name limit
        QVERIFY(!BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"a","steps":[{"type":"inc","name":"123456789012345678901234567890123","value":"1"}]})"),
            script, error));
    }

    void rejectBadIncValue()
    {
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"a","steps":[{"type":"inc","name":"count","value":"abc"}]})"),
            script, error));
        QVERIFY(!BotScriptParser::parse(
            parseJson(R"({"version":1,"name":"a","steps":[{"type":"inc","name":"count","value":""}]})"),
            script, error));
    }

    void rejectOversizeSetValue()
    {
        const QString longValue(513, 'a');
        const QString json = QString(R"({"version":1,"name":"a","steps":[{"type":"set","name":"count","value":"%1"}]})").arg(longValue);
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(parseJson(json.toUtf8().constData()), script, error));
    }

    void rejectTooManyVariables()
    {
        QStringList steps;
        for (int i = 0; i < 65; i++)
            steps << QString(R"({"type":"set","name":"v%1","value":"0"})").arg(i);
        const QString json = QString(R"({"version":1,"name":"a","steps":[%1]})").arg(steps.join(","));
        BotScript script;
        QString error;
        QVERIFY(!BotScriptParser::parse(parseJson(json.toUtf8().constData()), script, error));
        QVERIFY(error.contains("64"));
    }

    void exactly64VariablesOk()
    {
        QStringList steps;
        for (int i = 0; i < 64; i++)
            steps << QString(R"({"type":"set","name":"v%1","value":"0"})").arg(i);
        const QString json = QString(R"({"version":1,"name":"a","steps":[%1]})").arg(steps.join(","));
        BotScript script;
        QString error;
        QVERIFY2(BotScriptParser::parse(parseJson(json.toUtf8().constData()), script, error), qPrintable(error));
        QCOMPARE(script.steps.size(), 64);
    }

    void validNewStepTypes()
    {
        QString error;
        BotScriptStep s;
        s.type = "set";
        s.name = "count";
        s.value = "0";
        QVERIFY(BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "inc";
        s.name = "count";
        s.value = "-7";
        QVERIFY(BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "if";
        s.condType = "chat_contains";
        s.condValue = "ready";
        s.timeoutSeconds = 30;
        s.steps.append(defaultSay());
        QVERIFY(BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "if";
        s.condType = "var_gt";
        s.condVar = "count";
        s.condNumber = 5;
        QVERIFY(BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "if";
        s.condType = "health_below";
        s.condNumber = 10;
        QVERIFY(BotScriptParser::validateStep(s, error));
    }

    void invalidNewStepTypes()
    {
        QString error;
        BotScriptStep s;
        s.type = "set";
        s.name = "bad-name";
        QVERIFY(!BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "inc";
        s.name = "count";
        s.value = "1.5";
        QVERIFY(!BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "if";
        s.condType = "var_equals";
        s.condVar = "count";
        QVERIFY(BotScriptParser::validateStep(s, error));  // empty expected value is allowed
        s.condType = "var_gt";
        s.condNumber = 2000000;
        QVERIFY(!BotScriptParser::validateStep(s, error));
        s = BotScriptStep();
        s.type = "if";
        s.condType = "health_below";
        s.condNumber = -1;
        QVERIFY(!BotScriptParser::validateStep(s, error));
    }

   private:
    static BotScriptStep defaultSay()
    {
        BotScriptStep s;
        s.type = "say";
        s.text = "hi";
        return s;
    }
};

QTEST_MAIN(TestBotScript)
#include "TestBotScript.moc"
