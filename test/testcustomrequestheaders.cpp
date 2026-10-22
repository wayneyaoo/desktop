/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "customrequestheaders.h"

#include <QNetworkRequest>
#include <QTest>
#include <QUrl>

using namespace OCC;
using namespace Qt::StringLiterals;

using ParseError = CustomRequestHeaders::ParseError;

class TestCustomRequestHeaders : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesValidText_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expectedText");

        QTest::newRow("empty") << QString() << QString();
        QTest::newRow("only blank lines") << u"\n  \n\t\n"_s << QString();
        QTest::newRow("single header") << u"P-Access-Token: secret"_s << u"P-Access-Token: secret"_s;
        QTest::newRow("several headers") << u"P-Access-Token-Id: id\nP-Access-Token: secret"_s << u"P-Access-Token-Id: id\nP-Access-Token: secret"_s;
        QTest::newRow("whitespace and blank lines around headers") << u"\n  X-Token :  value  \n\n"_s << u"X-Token: value"_s;
        QTest::newRow("windows line endings") << u"A: 1\r\nB: 2\r\n"_s << u"A: 1\nB: 2"_s;
        QTest::newRow("colon and tab inside value") << u"X-Token: a:b\tc"_s << u"X-Token: a:b\tc"_s;
        QTest::newRow("all token characters in name") << u"!#$%&'*+-.^_`|~09azAZ: v"_s << u"!#$%&'*+-.^_`|~09azAZ: v"_s;
    }

    void parsesValidText()
    {
        QFETCH(QString, text);
        QFETCH(QString, expectedText);

        auto error = ParseError::DuplicateName;
        auto errorLine = qsizetype{-1};
        const auto headers = CustomRequestHeaders::fromText(text, &error, &errorLine);

        QVERIFY(headers);
        QCOMPARE(error, ParseError::NoError);
        QCOMPARE(errorLine, 0);
        QCOMPARE(headers->toText(), expectedText);
        QCOMPARE(headers->isEmpty(), expectedText.isEmpty());
        // The keychain stores the text form, so it must read back to the same headers.
        QVERIFY(CustomRequestHeaders::fromText(headers->toText()) == headers);
    }

    void rejectsInvalidText_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<ParseError>("expectedError");
        QTest::addColumn<qsizetype>("expectedLine");

        QTest::newRow("missing colon") << u"X-Token value"_s << ParseError::MissingSeparator << qsizetype{1};
        QTest::newRow("line number counts blank lines") << u"A: 1\n\nB"_s << ParseError::MissingSeparator << qsizetype{3};
        QTest::newRow("empty name") << u": value"_s << ParseError::InvalidName << qsizetype{1};
        QTest::newRow("space inside name") << u"X Token: value"_s << ParseError::InvalidName << qsizetype{1};
        QTest::newRow("non-ASCII name") << u"X-T\u00f6ken: value"_s << ParseError::InvalidName << qsizetype{1};
        QTest::newRow("empty value") << u"X-Token:   "_s << ParseError::InvalidValue << qsizetype{1};
        QTest::newRow("non-ASCII value") << u"X-Token: v\u00e4lue"_s << ParseError::InvalidValue << qsizetype{1};
        QTest::newRow("control character in value") << u"X-Token: a\u0001b"_s << ParseError::InvalidValue << qsizetype{1};
        QTest::newRow("reserved name") << u"Authorization: Basic abc"_s << ParseError::ReservedName << qsizetype{1};
        QTest::newRow("reserved name ignoring case") << u"cOOKIE: a=b"_s << ParseError::ReservedName << qsizetype{1};
        QTest::newRow("name the client always sets") << u"X-Request-ID: abc"_s << ParseError::ReservedName << qsizetype{1};
        QTest::newRow("name that changes how responses are decoded") << u"Accept-Encoding: gzip"_s << ParseError::ReservedName << qsizetype{1};
        QTest::newRow("duplicate name ignoring case") << u"X-Token: a\nx-token: b"_s << ParseError::DuplicateName << qsizetype{2};
    }

    void rejectsInvalidText()
    {
        QFETCH(QString, text);
        QFETCH(ParseError, expectedError);
        QFETCH(qsizetype, expectedLine);

        auto error = ParseError::NoError;
        auto errorLine = qsizetype{0};
        QVERIFY(!CustomRequestHeaders::fromText(text, &error, &errorLine));
        QCOMPARE(error, expectedError);
        QCOMPARE(errorLine, expectedLine);

        // Callers may omit the error details.
        QVERIFY(!CustomRequestHeaders::fromText(text));
    }

    void limitsSizeOfText()
    {
        // The limit applies to the text form including separators and line breaks.
        const auto firstLine = u"A: a"_s;
        const auto secondLinePrefix = u"\nB: "_s;
        const auto valueSize = CustomRequestHeaders::MaximumSize - firstLine.size() - secondLinePrefix.size();
        const auto textAtLimit = firstLine + secondLinePrefix + QString(valueSize, u'b');

        const auto headers = CustomRequestHeaders::fromText(textAtLimit);
        QVERIFY(headers);
        QCOMPARE(headers->toText().size(), CustomRequestHeaders::MaximumSize);

        auto error = ParseError::NoError;
        auto errorLine = qsizetype{0};
        QVERIFY(!CustomRequestHeaders::fromText(textAtLimit + u'b', &error, &errorLine));
        QCOMPARE(error, ParseError::TooLarge);
        QCOMPARE(errorLine, 2);
    }

    void keepsNamesInOrder()
    {
        const auto headers = CustomRequestHeaders::fromText(u"B-Header: 1\na-header: 2"_s);
        QVERIFY(headers);
        QCOMPARE(headers->names(), (QList<QByteArray>{"B-Header"_ba, "a-header"_ba}));
    }

    void appliesOnlyToSameServer_data()
    {
        QTest::addColumn<QUrl>("serverUrl");
        QTest::addColumn<QUrl>("requestUrl");
        QTest::addColumn<bool>("expectHeaders");

        const auto server = QUrl(u"https://cloud.example/nextcloud"_s);
        QTest::newRow("same server, other path") << server << QUrl(u"https://cloud.example/remote.php/dav"_s) << true;
        QTest::newRow("explicit default port") << server << QUrl(u"https://cloud.example:443/status.php"_s) << true;
        QTest::newRow("host differs in case") << QUrl(u"https://Cloud.Example"_s) << QUrl(u"https://cloud.example/"_s) << true;
        QTest::newRow("internationalized host in ASCII form") << QUrl(u"https://b\u00fccher.com"_s) << QUrl(u"https://xn--bcher-kva.com/"_s) << true;
        // Final sigma and sigma are equal ignoring case, but the hosts differ.
        QTest::newRow("internationalized host equal only ignoring case") << QUrl(u"https://\u03c3.com"_s) << QUrl(u"https://\u03c2.com/"_s) << false;
        QTest::newRow("secure websocket") << server << QUrl(u"wss://cloud.example/push/ws"_s) << true;
        QTest::newRow("plain websocket on plain server") << QUrl(u"http://cloud.example"_s) << QUrl(u"ws://cloud.example/push/ws"_s) << true;
        QTest::newRow("other host") << server << QUrl(u"https://other.example/"_s) << false;
        QTest::newRow("subdomain") << server << QUrl(u"https://sub.cloud.example/"_s) << false;
        QTest::newRow("other port") << server << QUrl(u"https://cloud.example:8443/"_s) << false;
        QTest::newRow("plain http to secure server") << server << QUrl(u"http://cloud.example/"_s) << false;
        QTest::newRow("plain websocket to secure server") << server << QUrl(u"ws://cloud.example/push/ws"_s) << false;
        QTest::newRow("other scheme") << server << QUrl(u"ftp://cloud.example/"_s) << false;
        QTest::newRow("relative url") << server << QUrl(u"/status.php"_s) << false;
    }

    void appliesOnlyToSameServer()
    {
        QFETCH(QUrl, serverUrl);
        QFETCH(QUrl, requestUrl);
        QFETCH(bool, expectHeaders);

        const auto headers = CustomRequestHeaders::fromText(u"P-Access-Token-Id: id\nP-Access-Token: secret"_s);
        QVERIFY(headers);

        auto request = QNetworkRequest{requestUrl};
        headers->applyTo(request, serverUrl);

        QCOMPARE(request.hasRawHeader("P-Access-Token-Id"_ba), expectHeaders);
        QCOMPARE(request.rawHeader("P-Access-Token"_ba), expectHeaders ? "secret"_ba : QByteArray{});
    }

    void keepsHeadersTheRequestAlreadyHas()
    {
        const auto headers = CustomRequestHeaders::fromText(u"X-Token: custom\nX-Other: other"_s);
        QVERIFY(headers);

        auto request = QNetworkRequest{QUrl(u"https://cloud.example/"_s)};
        request.setRawHeader("x-token"_ba, "set by client"_ba);
        headers->applyTo(request, QUrl(u"https://cloud.example"_s));

        QCOMPARE(request.rawHeader("X-Token"_ba), "set by client"_ba);
        QCOMPARE(request.rawHeader("X-Other"_ba), "other"_ba);
    }

    void removesHeadersFromRequestsToOtherServers()
    {
        const auto headers = CustomRequestHeaders::fromText(u"P-Access-Token: secret"_s);
        QVERIFY(headers);

        // A redirect to another server copies the request that already carries the headers.
        auto request = QNetworkRequest{QUrl(u"https://other.example/"_s)};
        request.setRawHeader("p-access-token"_ba, "secret"_ba);
        request.setRawHeader("X-Request-ID"_ba, "id"_ba);
        headers->applyTo(request, QUrl(u"https://cloud.example"_s));

        QVERIFY(!request.hasRawHeader("P-Access-Token"_ba));
        QCOMPARE(request.rawHeader("X-Request-ID"_ba), "id"_ba);

        // A header the client set for the other server keeps its value.
        auto ownRequest = QNetworkRequest{QUrl(u"https://other.example/"_s)};
        ownRequest.setRawHeader("P-Access-Token"_ba, "for other server"_ba);
        headers->applyTo(ownRequest, QUrl(u"https://cloud.example"_s));

        QCOMPARE(ownRequest.rawHeader("P-Access-Token"_ba), "for other server"_ba);
    }

    void limitsRedirectsToSameOrigin_data()
    {
        QTest::addColumn<QVariant>("redirectPolicy");
        QTest::addColumn<int>("expectedRedirectPolicy");

        QTest::newRow("default") << QVariant{} << int{QNetworkRequest::SameOriginRedirectPolicy};
        QTest::newRow("followed, as by IconJob") << QVariant{true} << int{QNetworkRequest::SameOriginRedirectPolicy};
        QTest::newRow("no less safe") << QVariant{QNetworkRequest::NoLessSafeRedirectPolicy} << int{QNetworkRequest::SameOriginRedirectPolicy};
        QTest::newRow("manual") << QVariant{QNetworkRequest::ManualRedirectPolicy} << int{QNetworkRequest::ManualRedirectPolicy};
    }

    void limitsRedirectsToSameOrigin()
    {
        QFETCH(QVariant, redirectPolicy);
        QFETCH(int, expectedRedirectPolicy);

        const auto headers = CustomRequestHeaders::fromText(u"P-Access-Token: secret"_s);
        QVERIFY(headers);

        auto request = QNetworkRequest{QUrl(u"https://cloud.example/"_s)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, redirectPolicy);
        headers->applyTo(request, QUrl(u"https://cloud.example"_s));

        QCOMPARE(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), expectedRedirectPolicy);
    }
};

QTEST_GUILESS_MAIN(TestCustomRequestHeaders)
#include "testcustomrequestheaders.moc"
