/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "accountstate.h"
#include "pushnotifications.h"
#include "pushnotificationstestutils.h"
#include "syncenginetestutils.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <memory>

using namespace OCC;
using namespace Qt::StringLiterals;

namespace
{

// Distinct from the default 12345 used by PushNotificationsTest, so both can run in parallel.
constexpr quint16 webSocketPort = 12346;

// Long enough for the connectivity check queued on creation to send its first request.
constexpr auto quietPeriodMs = 1000;

// Counts connectivity checks at the point where slotCheckConnection() decides to call them.
class ConnectivityCountingAccountState : public AccountState
{
public:
    explicit ConnectivityCountingAccountState(const AccountPtr &account)
        : AccountState(account)
    {
    }

    void checkConnectivity() override
    {
        ++checkConnectivityCount;
    }

    int checkConnectivityCount = 0;
};

AccountPtr createPushAccount()
{
    auto account = FakeWebSocketServer::createAccount(u"admin"_s, u"password"_s, QUrl(u"http://localhost"_s), QUrl(u"ws://localhost:%1"_s.arg(webSocketPort)));

    // Becoming Connected fetches navigation apps; answer every request without a server.
    auto qnam = new FakeQNAM({});
    qnam->setOverride([qnam](QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
        return new FakeErrorReply(operation, request, qnam, 404);
    });
    account->setCredentials(new FakeCredentials{qnam});
    return account;
}

// Already fetched credentials that create a new access manager each time and count all their requests.
class RequestCountingCredentials : public FakeCredentials
{
public:
    RequestCountingCredentials()
        : FakeCredentials{nullptr}
    {
        _wasFetched = true;
    }

    [[nodiscard]] QNetworkAccessManager *createQNAM() const override
    {
        auto *const qnam = new FakeQNAM({});
        // The access manager can outlive the credentials, so it shares the counter instead of using this.
        qnam->setOverride(
            [qnam, requestCount = requestCount](QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
                ++*requestCount;
                return new FakeErrorReply(operation, request, qnam, 404);
            });
        return qnam;
    }

    const std::shared_ptr<int> requestCount = std::make_shared<int>(0);
};
}

class TestAccountState : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // The account wizard sets capabilities before the account is added, so push can be ready first.
    void disconnectedAccountWithReadyPushChecksConnectivityOnCreation()
    {
        FakeWebSocketServer fakeServer(webSocketPort);
        const auto account = createPushAccount();
        QVERIFY(fakeServer.authenticateAccount(account));
        QVERIFY(account->pushNotifications()->isReady());

        ConnectivityCountingAccountState accountState(account);
        QCOMPARE(accountState.state(), AccountState::Disconnected);

        QTRY_COMPARE(accountState.checkConnectivityCount, 1);
    }

    void disconnectedAccountWithReadyPushChecksConnectivityOnPeriodicCheck()
    {
        FakeWebSocketServer fakeServer(webSocketPort);
        const auto account = createPushAccount();
        QVERIFY(fakeServer.authenticateAccount(account));

        ConnectivityCountingAccountState accountState(account);
        QCoreApplication::sendPostedEvents(&accountState, QEvent::MetaCall);
        const auto checksAfterCreation = accountState.checkConnectivityCount;

        // Queues slotCheckConnection(), the slot the periodic timer also drives.
        accountState.systemOnlineConfigurationChanged();
        QCoreApplication::sendPostedEvents(&accountState, QEvent::MetaCall);

        QCOMPARE(accountState.state(), AccountState::Disconnected);
        QCOMPARE(accountState.checkConnectivityCount, checksAfterCreation + 1);
    }

    void connectedAccountWithReadyPushSkipsPeriodicCheck()
    {
        FakeWebSocketServer fakeServer(webSocketPort);
        const auto account = createPushAccount();
        ConnectivityCountingAccountState accountState(account);

        // Push becoming ready after the account state exists marks it Connected.
        const auto socket = fakeServer.authenticateAccount(account);
        QVERIFY(socket);
        QCOMPARE(accountState.state(), AccountState::Connected);
        QCoreApplication::sendPostedEvents(&accountState, QEvent::MetaCall);
        const auto checksWhileConnected = accountState.checkConnectivityCount;

        accountState.systemOnlineConfigurationChanged();
        QCoreApplication::sendPostedEvents(&accountState, QEvent::MetaCall);
        QCOMPARE(accountState.checkConnectivityCount, checksWhileConnected);

        // Control: the same trigger checks again once push is lost.
        QSignalSpy pushDisabledSpy(account.data(), &Account::pushNotificationsDisabled);
        socket->abort();
        QVERIFY(pushDisabledSpy.wait());
        QVERIFY(!account->pushNotifications()->isReady());

        accountState.systemOnlineConfigurationChanged();
        QCoreApplication::sendPostedEvents(&accountState, QEvent::MetaCall);
        QCOMPARE(accountState.checkConnectivityCount, checksWhileConnected + 1);
    }

    void signedOutAccountWithReadyPushSkipsPeriodicCheck()
    {
        FakeWebSocketServer fakeServer(webSocketPort);
        const auto account = createPushAccount();
        ConnectivityCountingAccountState accountState(account);
        QVERIFY(fakeServer.authenticateAccount(account));
        QCOMPARE(accountState.state(), AccountState::Connected);
        QCoreApplication::sendPostedEvents(&accountState, QEvent::MetaCall);

        accountState.signOutByUi();
        QCOMPARE(accountState.state(), AccountState::SignedOut);
        const auto checksAfterSignOut = accountState.checkConnectivityCount;

        accountState.systemOnlineConfigurationChanged();
        QCoreApplication::sendPostedEvents(&accountState, QEvent::MetaCall);
        QCOMPARE(accountState.checkConnectivityCount, checksAfterSignOut);
    }

    void waitsForCustomRequestHeadersBeforeCheckingConnectivity()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(u"http://localhost"_s));
        auto *const credentials = new RequestCountingCredentials;
        const auto requestCount = credentials->requestCount;
        account->setCredentials(credentials);
        account->setCustomRequestHeaders({}, Account::CustomRequestHeadersStatus::Loading);

        // The connection validator holds a reference, so the state must be reference counted.
        const auto accountState = AccountStatePtr(new AccountState(account));

        QTest::qWait(quietPeriodMs);
        QCOMPARE(*requestCount, 0);

        account->setCustomRequestHeaders({});
        QTRY_VERIFY(*requestCount > 0);
    }

    void failedConnectionExplainsUnreadableCustomRequestHeaders()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(u"http://localhost"_s));
        account->setCredentials(new RequestCountingCredentials);
        account->setCustomRequestHeaders({}, Account::CustomRequestHeadersStatus::Unreadable);
        const auto accountState = AccountStatePtr(new AccountState(account));
        const auto explanation = u"The custom headers couldn't be read from the keychain. Enter them again in the connection settings."_s;

        QTRY_COMPARE(accountState->state(), AccountState::NetworkError);
        QCOMPARE(accountState->connectionErrors().size(), 2);
        QCOMPARE(accountState->connectionErrors().first(), explanation);

        // Once the user entered the headers, a failure is no longer about them.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        account->setCustomRequestHeaders({});
        QTRY_VERIFY(!accountState->connectionErrors().isEmpty());
        QVERIFY(!accountState->connectionErrors().contains(explanation));
    }

    void changedCustomRequestHeadersReconnectConnectedAccount()
    {
        FakeWebSocketServer fakeServer(webSocketPort);
        const auto account = createPushAccount();
        ConnectivityCountingAccountState accountState(account);
        QVERIFY(fakeServer.authenticateAccount(account));
        QCOMPARE(accountState.state(), AccountState::Connected);
        QCoreApplication::sendPostedEvents(&accountState, QEvent::MetaCall);
        const auto checksWhileConnected = accountState.checkConnectivityCount;

        // The connection, including the push notification socket, has to start over with the new headers.
        const auto headers = CustomRequestHeaders::fromText(u"P-Access-Token: secret"_s);
        QVERIFY(headers);
        account->setCustomRequestHeaders(*headers);
        QCOMPARE(accountState.state(), AccountState::Disconnected);
        QCOMPARE(accountState.checkConnectivityCount, checksWhileConnected + 1);

        account->setCustomRequestHeaders(*headers);
        QCOMPARE(accountState.checkConnectivityCount, checksWhileConnected + 1);
    }
};

QTEST_MAIN(TestAccountState)
#include "testaccountstate.moc"
