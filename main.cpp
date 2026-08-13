// Minimal reproducer: QV4 writes through a null MemberData pointer after an
// internal class is rebuilt, because Object::insertMember reuses a property
// index that InternalClass::changeMember captured before the rebuild.
//
// Crashes in QV4::Object::insertMember -> setProperty -> WriteBarrier::write.

#include <QCoreApplication>
#include <QJSEngine>
#include <QJSValue>
#include <QDebug>

static const char *script = R"JS(
(function () {
    var o = {};

    // Claim slots 0..9, then leave only the highest-indexed one live.
    // After the rebuild the class keeps <= Object::NInlineProperties
    // properties, so QV4 sets memberData to null - but the entry index
    // handed to insertMember is still 9.
    for (var i = 0; i < 10; ++i)
        o["k" + i] = i;
    for (var i = 0; i < 9; ++i)
        delete o["k" + i];

    // Every delete/re-add pair creates two new internal classes and burns
    // two redundant transitions. The rebuild fires at MaxRedundantTransitions
    // (255), i.e. at around iteration 128.
    for (var i = 0; i < 200; ++i) {
        delete o.k9;
        o.k9 = i;
    }
    return "SURVIVED";
})()
)JS";

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QJSEngine engine;
    const QJSValue result = engine.evaluate(QString::fromUtf8(script));

    if (result.isError()) {
        qWarning() << "uncaught exception:" << result.toString();
        return 2;
    }

    qInfo() << result.toString() << "- no crash on this Qt build";
    return 0;
}
