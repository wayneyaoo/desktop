/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScopeGuard>
#include <QWidget>

#include "account.h"
#include "accountmanager.h"
#include "configfile.h"
#include "creds/keychainchunk.h"
#include "foldermantestutils.h"
#include "logger.h"
#include "managedsettingstestutils.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

#include "networksettings.h"

using namespace OCC;
using namespace Qt::StringLiterals;

class TestNetworkSettings : public QObject
{
    Q_OBJECT

    FolderManTestHelper helper;

    //! @brief Registers @p account with AccountManager until the returned guard removes it and its saved headers.
    static auto addAccount(const AccountPtr &account)
    {
        const auto accountState = AccountManager::instance()->addAccount(account);
        return qScopeGuard([account, accountState] {
            // Saving custom request headers wrote them to the real keychain.
            account->setCustomRequestHeaders({});
            AccountManager::instance()->saveAccount(account);
            // Keychain jobs are children of the account manager until they finished.
            QTRY_VERIFY(AccountManager::instance()->findChildren<KeychainChunk::Job *>().isEmpty());
            AccountManager::instance()->removeAccountState(accountState);
        });
    }

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    // Keeps tests off the device policy of the machine running them.
    void init()
    {
        ConfigFile::setDeviceSourcesFactory([] {
            return std::vector<std::unique_ptr<SettingSource>>{};
        });
    }

    void cleanup()
    {
        ConfigFile::setDeviceSourcesFactory([] {
            return std::vector<std::unique_ptr<SettingSource>>{};
        });
    }

    void test_whenAccountIsLoggedOut_doesNotCrash()
    {
        // Create an account that is not registered in AccountManager
        // This simulates a logged-out account scenario
        auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://example.com")));
        account->setDavUser(QStringLiteral("testuser"));
        
        // Create NetworkSettings with this account
        // When proxy settings are changed, saveProxySettings() will try to
        // get accountState from AccountManager, which will return nullptr
        // This test ensures the app doesn't crash in that scenario
        NetworkSettings settings(account);
        
        // The test passes if we reach here without crashing
        QVERIFY(true);
    }

    void testEnforcedProxyFieldsDisableOnlyTheirControls_data()
    {
        QTest::addColumn<bool>("typeEnforced");
        QTest::addColumn<bool>("hostAndPortEnforced");

        QTest::newRow("host and port enforced") << false << true;
        QTest::newRow("only type enforced") << true << false;
    }

    void testEnforcedProxyFieldsDisableOnlyTheirControls()
    {
        QFETCH(bool, typeEnforced);
        QFETCH(bool, hostAndPortEnforced);

        auto account = Account::create();
        account->setUrl(QUrl(u"https://example.com"_s));
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setProxyType(QNetworkProxy::HttpProxy);
        account->applyManagedProxySettings(managedProxyFields(true,
                                                              typeEnforced ? std::optional<int>(QNetworkProxy::HttpProxy) : std::nullopt,
                                                              hostAndPortEnforced ? std::optional<QString>(u"proxy.example.com"_s) : std::nullopt,
                                                              hostAndPortEnforced ? std::optional<int>(8080) : std::nullopt));

        NetworkSettings settings(account);

        const auto typeComboBox = settings.findChild<QWidget *>(u"typeComboBox"_s);
        const auto manualProxyRadioButton = settings.findChild<QWidget *>(u"manualProxyRadioButton"_s);
        const auto hostLineEdit = settings.findChild<QWidget *>(u"hostLineEdit"_s);
        const auto portSpinBox = settings.findChild<QWidget *>(u"portSpinBox"_s);
        const auto authRequiredCheckBox = settings.findChild<QWidget *>(u"authRequiredcheckBox"_s);
        const auto proxyEnforcedLabel = settings.findChild<QWidget *>(u"proxyEnforcedLabel"_s);
        QVERIFY(typeComboBox && manualProxyRadioButton && hostLineEdit && portSpinBox && authRequiredCheckBox && proxyEnforcedLabel);

        QCOMPARE(typeComboBox->isEnabled(), !typeEnforced);
        QCOMPARE(manualProxyRadioButton->isEnabled(), !typeEnforced);
        QCOMPARE(hostLineEdit->isEnabled(), !hostAndPortEnforced);
        QCOMPARE(portSpinBox->isEnabled(), !hostAndPortEnforced);
        QVERIFY(authRequiredCheckBox->isEnabled());
        QVERIFY(!proxyEnforcedLabel->isHidden());
    }

    // Enforcing only the proxy type must still let the user provide the host and port.
    void testTypeOnlyEnforcementKeepsHostAndPortEditable()
    {
        auto account = Account::create();
        account->setUrl(QUrl(u"https://example.com"_s));
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setProxyType(QNetworkProxy::DefaultProxy);
        account->applyManagedProxySettings(managedProxyFields(true, QNetworkProxy::HttpProxy, std::nullopt, std::nullopt));

        NetworkSettings settings(account);

        const auto manualProxyRadioButton = settings.findChild<QRadioButton *>(u"manualProxyRadioButton"_s);
        const auto hostLineEdit = settings.findChild<QLineEdit *>(u"hostLineEdit"_s);
        const auto portSpinBox = settings.findChild<QWidget *>(u"portSpinBox"_s);
        QVERIFY(manualProxyRadioButton && hostLineEdit && portSpinBox);

        QVERIFY(manualProxyRadioButton->isChecked());
        QVERIFY(hostLineEdit->isVisibleTo(&settings));
        QVERIFY(portSpinBox->isVisibleTo(&settings));
        QVERIFY(hostLineEdit->isEnabled());
        QVERIFY(portSpinBox->isEnabled());
    }

    // A managed default must not surface as the account's own value in the editor.
    void testExistingAccountShowsItsOwnProxyNotManagedDefault()
    {
        auto account = Account::create();
        account->setUrl(QUrl(u"https://example.com"_s));
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setProxyType(QNetworkProxy::DefaultProxy);
        account->applyManagedProxySettings(managedProxyFields(false, QNetworkProxy::HttpProxy, u"proxy.example.com"_s, 8080));

        NetworkSettings settings(account);

        const auto systemProxyRadioButton = settings.findChild<QRadioButton *>(u"systemProxyRadioButton"_s);
        const auto manualProxyRadioButton = settings.findChild<QRadioButton *>(u"manualProxyRadioButton"_s);
        const auto hostLineEdit = settings.findChild<QLineEdit *>(u"hostLineEdit"_s);
        QVERIFY(systemProxyRadioButton && manualProxyRadioButton && hostLineEdit);

        QVERIFY(systemProxyRadioButton->isChecked());
        QVERIFY(!manualProxyRadioButton->isChecked());
        QVERIFY(hostLineEdit->text().isEmpty());
    }

    void testCustomRequestHeadersAreAppliedAndSaved()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(u"https://example.com"_s));
        account->setDavUser(u"testuser"_s);
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        const auto removeAccount = addAccount(account);

        NetworkSettings settings(account);
        const auto textEdit = settings.findChild<QPlainTextEdit *>(u"customRequestHeadersTextEdit"_s);
        const auto applyButton = settings.findChild<QPushButton *>(u"customRequestHeadersApplyButton"_s);
        const auto errorLabel = settings.findChild<QLabel *>(u"customRequestHeadersErrorLabel"_s);
        QVERIFY(textEdit && applyButton && errorLabel);
        QVERIFY(textEdit->isEnabled());
        QVERIFY(textEdit->toPlainText().isEmpty());

        textEdit->setPlainText(u"  P-Access-Token : secret \n"_s);
        applyButton->click();

        QCOMPARE(account->customRequestHeaders().toText(), u"P-Access-Token: secret"_s);
        QCOMPARE(textEdit->toPlainText(), u"P-Access-Token: secret"_s);
        QVERIFY(errorLabel->isHidden());

        const auto config = ConfigFile::settingsWithGroup(u"Accounts"_s);
        config->beginGroup(account->id());
        QCOMPARE(config->value(u"customRequestHeaderNames"_s).toStringList(), QStringList{u"P-Access-Token"_s});
    }

    void testInvalidCustomRequestHeadersKeepTheAccountHeaders()
    {
        const auto headers = CustomRequestHeaders::fromText(u"P-Access-Token: secret"_s);
        QVERIFY(headers);
        const auto account = Account::create();
        account->setUrl(QUrl(u"https://example.com"_s));
        account->setCustomRequestHeaders(*headers);

        NetworkSettings settings(account);
        const auto textEdit = settings.findChild<QPlainTextEdit *>(u"customRequestHeadersTextEdit"_s);
        const auto applyButton = settings.findChild<QPushButton *>(u"customRequestHeadersApplyButton"_s);
        const auto errorLabel = settings.findChild<QLabel *>(u"customRequestHeadersErrorLabel"_s);
        QVERIFY(textEdit && applyButton && errorLabel);
        QCOMPARE(textEdit->toPlainText(), u"P-Access-Token: secret"_s);

        textEdit->setPlainText(u"Authorization: Basic abc"_s);
        applyButton->click();

        QVERIFY(!errorLabel->isHidden());
        QCOMPARE(errorLabel->text(), u"Line 1: This header is set by the client and cannot be changed."_s);
        QVERIFY(account->customRequestHeaders() == *headers);
    }

    void testCustomRequestHeadersCanBeEditedOnlyAfterLoading()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(u"https://example.com"_s));
        account->setCustomRequestHeaders({}, Account::CustomRequestHeadersStatus::Loading);

        NetworkSettings settings(account);
        const auto groupBox = settings.findChild<QGroupBox *>(u"customRequestHeadersGroupBox"_s);
        const auto textEdit = settings.findChild<QPlainTextEdit *>(u"customRequestHeadersTextEdit"_s);
        QVERIFY(groupBox && textEdit);
        QVERIFY(!groupBox->isEnabled());

        const auto headers = CustomRequestHeaders::fromText(u"P-Access-Token: secret"_s);
        QVERIFY(headers);
        account->setCustomRequestHeaders(*headers);

        QVERIFY(groupBox->isEnabled());
        QCOMPARE(textEdit->toPlainText(), u"P-Access-Token: secret"_s);
    }

    void testUnreadableCustomRequestHeadersCanBeReplaced_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QStringList>("expectedStoredNames");

        QTest::newRow("entered again") << u"P-Access-Token: new"_s << QStringList{u"P-Access-Token"_s};
        QTest::newRow("removed") << QString() << QStringList();
    }

    void testUnreadableCustomRequestHeadersCanBeReplaced()
    {
        QFETCH(QString, text);
        QFETCH(QStringList, expectedStoredNames);

        const auto account = Account::create();
        account->setUrl(QUrl(u"https://example.com"_s));
        account->setDavUser(u"testuser"_s);
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        const auto removeAccount = addAccount(account);
        const auto storedHeaders = CustomRequestHeaders::fromText(u"P-Access-Token: old"_s);
        QVERIFY(storedHeaders);
        account->setCustomRequestHeaders(*storedHeaders);
        AccountManager::instance()->saveAccount(account);
        // As after starting without a keychain.
        account->setCustomRequestHeaders({}, Account::CustomRequestHeadersStatus::Unreadable);

        NetworkSettings settings(account);
        const auto textEdit = settings.findChild<QPlainTextEdit *>(u"customRequestHeadersTextEdit"_s);
        const auto applyButton = settings.findChild<QPushButton *>(u"customRequestHeadersApplyButton"_s);
        const auto errorLabel = settings.findChild<QLabel *>(u"customRequestHeadersErrorLabel"_s);
        QVERIFY(textEdit && applyButton && errorLabel);
        QVERIFY(textEdit->isEnabled());
        QVERIFY(textEdit->toPlainText().isEmpty());
        QVERIFY(!errorLabel->isHidden());
        QCOMPARE(errorLabel->text(), u"The stored headers couldn't be read from the keychain. Enter them again, or apply without headers to remove them."_s);

        textEdit->setPlainText(text);
        applyButton->click();

        QCOMPARE(account->customRequestHeadersStatus(), Account::CustomRequestHeadersStatus::Ready);
        QCOMPARE(account->customRequestHeaders().toText(), text);
        QVERIFY(errorLabel->isHidden());
        const auto config = ConfigFile::settingsWithGroup(u"Accounts"_s);
        config->beginGroup(account->id());
        QCOMPARE(config->value(u"customRequestHeaderNames"_s).toStringList(), expectedStoredNames);
    }
};

QTEST_MAIN(TestNetworkSettings)
#include "testnetworksettings.moc"
