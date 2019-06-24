// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/apptests.h>

#include <chainparams.h>
#include <config.h>
#include <httprpc.h>
#include <key.h>
#include <qt/bitcoin.h>
#include <qt/bitcoingui.h>
#include <qt/networkstyle.h>
#include <qt/rpcconsole.h>
#include <rpc/server.h>
#include <shutdown.h>
#include <validation.h>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif
#ifdef ENABLE_WALLET
#include <wallet/db.h>
#endif

#include <test/setup_common.h>

#include <QAction>
#include <QEventLoop>
#include <QLineEdit>
#include <QScopedPointer>
#include <QTest>
#include <QTextEdit>
#include <QtGlobal>
/*
 * FIXME: <QtTest/QtTestGui> requires QT_WIDGETS_LIB to be defined prior
 * inclusion to export the QtTest::keyClicks symbol.
 * On some older Qt versions the definition end up being set by the inclusion of
 * <QtTest/QtTestWidgets>.
 * This only occurs when building with autotools, as QMake and CMake define
 * QT_WIDGETS_LIB on the command line. As a workaround for autotools,
 * <QtTest/QtTestWidgets> should be included before <QtTest/QtTestGui>.
 * Also prevent the linter from sorting the includes.
 */
// clang-format off
#include <QtTest/QtTestWidgets>
#include <QtTest/QtTestGui>
// clang-format on

#include <univalue.h>

#include <string>

namespace {
//! Call getblockchaininfo RPC and check first field of JSON output.
void TestRpcCommand(RPCConsole *console) {
    QEventLoop loop;
    QTextEdit *messagesWidget =
        console->findChild<QTextEdit *>("messagesWidget");
    QObject::connect(messagesWidget, &QTextEdit::textChanged, &loop,
                     &QEventLoop::quit);
    QLineEdit *lineEdit = console->findChild<QLineEdit *>("lineEdit");
    QTest::keyClicks(lineEdit, "getblockchaininfo");
    QTest::keyClick(lineEdit, Qt::Key_Return);
    loop.exec();
    QString output = messagesWidget->toPlainText();
    UniValue value;
    value.read(
        output
            .right(output.size() -
                   output.lastIndexOf(QChar::ObjectReplacementCharacter) - 1)
            .toStdString());
    QCOMPARE(value["chain"].get_str(), std::string("regtest"));
}
} // namespace

//! Entry point for BitcoinApplication tests.
void AppTests::appTests() {
#ifdef Q_OS_MAC
    if (QApplication::platformName() == "minimal") {
        // Disable for mac on "minimal" platform to avoid crashes inside the Qt
        // framework when it tries to look up unimplemented cocoa functions,
        // and fails to handle returned nulls
        // (https://bugreports.qt.io/browse/QTBUG-49686).
        QWARN("Skipping AppTests on mac build with 'minimal' platform set due "
              "to Qt bugs. To run AppTests, invoke "
              "with 'test_bitcoin-qt -platform cocoa' on mac, or else use a "
              "linux or windows build.");
        return;
    }
#endif

    Config &config = const_cast<Config &>(GetConfig());

    // Create a temp data directory to backup the gui settings to
    BasicTestingSetup test{CBaseChainParams::REGTEST};
    // Already started by the common test setup, so stop it to avoid
    // interference
    ECC_Stop();
    LogInstance().DisconnectTestLogger();

    m_app.parameterSetup();
    m_app.createOptionsModel(true /* reset settings */);
    QScopedPointer<const NetworkStyle> style(NetworkStyle::instantiate(
        QString::fromStdString(Params().NetworkIDString())));
    m_app.setupPlatformStyle();
    m_app.createWindow(&config, style.data());
    connect(&m_app, &BitcoinApplication::windowShown, this,
            &AppTests::guiTests);
    expectCallback("guiTests");
    m_app.baseInitialize(config);

    RPCServer rpcServer;
    HTTPRPCRequestProcessor httpRPCRequestProcessor(config, rpcServer);
    m_app.requestInitialize(config, rpcServer, httpRPCRequestProcessor);
    m_app.exec();
    m_app.requestShutdown(config);
    m_app.exec();

    // Reset global state to avoid interfering with later tests.
    AbortShutdown();
    UnloadBlockIndex();
}

//! Entry point for BitcoinGUI tests.
void AppTests::guiTests(BitcoinGUI *window) {
    HandleCallback callback{"guiTests", *this};
    connect(window, &BitcoinGUI::consoleShown, this, &AppTests::consoleTests);
    expectCallback("consoleTests");
    QAction *action = window->findChild<QAction *>("openRPCConsoleAction");
    action->activate(QAction::Trigger);
}

//! Entry point for RPCConsole tests.
void AppTests::consoleTests(RPCConsole *console) {
    HandleCallback callback{"consoleTests", *this};
    TestRpcCommand(console);
}

//! Destructor to shut down after the last expected callback completes.
AppTests::HandleCallback::~HandleCallback() {
    auto &callbacks = m_app_tests.m_callbacks;
    auto it = callbacks.find(m_callback);
    assert(it != callbacks.end());
    callbacks.erase(it);
    if (callbacks.empty()) {
        m_app_tests.m_app.quit();
    }
}
