/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "customrequestheaders.h"

#include <QNetworkRequest>
#include <QStringList>
#include <QStringView>
#include <QUrl>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace
{

constexpr auto defaultHttpPort = 80;
constexpr auto defaultHttpsPort = 443;

// Text form of the headers, also used to compute their stored size.
constexpr auto nameValueSeparator = ": "_L1;
constexpr auto lineSeparator = u'\n';

// Headers the client always sets itself, and headers that control the connection or HTTP message framing.
constexpr std::array reservedNames = {
    "Accept"_L1,
    "Accept-Encoding"_L1,
    "Authorization"_L1,
    "Connection"_L1,
    "Content-Length"_L1,
    "Content-Type"_L1,
    "Cookie"_L1,
    "Expect"_L1,
    "Host"_L1,
    "Keep-Alive"_L1,
    "Proxy-Authorization"_L1,
    "Proxy-Connection"_L1,
    "TE"_L1,
    "Trailer"_L1,
    "Transfer-Encoding"_L1,
    "Upgrade"_L1,
    "User-Agent"_L1,
    "X-Request-ID"_L1,
};

// Token characters from RFC 9110, section 5.6.2.
bool isTokenCharacter(const QChar character)
{
    constexpr auto tokenSymbols = "!#$%&'*+-.^_`|~"_L1;
    const auto code = character.unicode();
    return (code >= u'0' && code <= u'9') || (code >= u'a' && code <= u'z') || (code >= u'A' && code <= u'Z') || tokenSymbols.contains(character);
}

// Printable ASCII, space and tab, so values convert to Latin-1 without loss.
bool isValueCharacter(const QChar character)
{
    const auto code = character.unicode();
    return code == u'\t' || (code >= u' ' && code <= u'~');
}

bool isReservedName(const QStringView name)
{
    return std::ranges::any_of(reservedNames, [name](const QLatin1StringView reservedName) {
        return name.compare(reservedName, Qt::CaseInsensitive) == 0;
    });
}

// WebSocket schemes map to their HTTP counterparts so the push notification socket counts as the same server.
QString normalizedScheme(const QUrl &url)
{
    const auto scheme = url.scheme().toLower();
    if (scheme == "wss"_L1) {
        return u"https"_s;
    }
    if (scheme == "ws"_L1) {
        return u"http"_s;
    }
    return scheme;
}

bool isSameServer(const QUrl &requestUrl, const QUrl &serverUrl)
{
    const auto scheme = normalizedScheme(requestUrl);
    if ((scheme != "https"_L1 && scheme != "http"_L1) || scheme != normalizedScheme(serverUrl)) {
        return false;
    }

    // The ASCII form tells apart internationalized names that only look alike when decoded.
    const auto host = requestUrl.host(QUrl::EncodeUnicode);
    if (host.isEmpty() || host != serverUrl.host(QUrl::EncodeUnicode)) {
        return false;
    }

    const auto defaultPort = scheme == "https"_L1 ? defaultHttpsPort : defaultHttpPort;
    return requestUrl.port(defaultPort) == serverUrl.port(defaultPort);
}

} // namespace

namespace OCC
{

CustomRequestHeaders::CustomRequestHeaders(QList<Header> headers)
    : _headers(std::move(headers))
{
}

std::optional<CustomRequestHeaders> CustomRequestHeaders::fromText(const QString &text, ParseError *error, qsizetype *errorLine)
{
    const auto reportResult = [error, errorLine](const ParseError reason, const qsizetype line) {
        if (error) {
            *error = reason;
        }
        if (errorLine) {
            *errorLine = line;
        }
    };

    QList<Header> headers;
    qsizetype totalSize = 0;
    const auto lines = QStringView{text}.split(lineSeparator);
    for (qsizetype index = 0; index < lines.size(); ++index) {
        const auto lineNumber = index + 1;
        const auto line = lines.at(index).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const auto separatorIndex = line.indexOf(u':');
        if (separatorIndex < 0) {
            reportResult(ParseError::MissingSeparator, lineNumber);
            return std::nullopt;
        }

        const auto name = line.first(separatorIndex).trimmed();
        if (name.isEmpty() || !std::ranges::all_of(name, isTokenCharacter)) {
            reportResult(ParseError::InvalidName, lineNumber);
            return std::nullopt;
        }

        const auto value = line.sliced(separatorIndex + 1).trimmed();
        if (value.isEmpty() || !std::ranges::all_of(value, isValueCharacter)) {
            reportResult(ParseError::InvalidValue, lineNumber);
            return std::nullopt;
        }

        if (isReservedName(name)) {
            reportResult(ParseError::ReservedName, lineNumber);
            return std::nullopt;
        }

        auto latin1Name = name.toLatin1();
        const auto isDuplicate = std::ranges::any_of(headers, [&latin1Name](const Header &header) {
            return header.first.compare(latin1Name, Qt::CaseInsensitive) == 0;
        });
        if (isDuplicate) {
            reportResult(ParseError::DuplicateName, lineNumber);
            return std::nullopt;
        }

        const auto lineSeparatorSize = headers.isEmpty() ? 0 : 1;
        totalSize += lineSeparatorSize + name.size() + nameValueSeparator.size() + value.size();
        if (totalSize > MaximumSize) {
            reportResult(ParseError::TooLarge, lineNumber);
            return std::nullopt;
        }

        headers.emplaceBack(std::move(latin1Name), value.toLatin1());
    }

    reportResult(ParseError::NoError, 0);
    return CustomRequestHeaders{std::move(headers)};
}

QString CustomRequestHeaders::toText() const
{
    QStringList lines;
    lines.reserve(_headers.size());
    for (const auto &[name, value] : _headers) {
        lines.append(QString::fromLatin1(name) + nameValueSeparator + QString::fromLatin1(value));
    }
    return lines.join(lineSeparator);
}

QList<QByteArray> CustomRequestHeaders::names() const
{
    QList<QByteArray> result;
    result.reserve(_headers.size());
    for (const auto &header : _headers) {
        result.append(header.first);
    }
    return result;
}

bool CustomRequestHeaders::isEmpty() const
{
    return _headers.isEmpty();
}

void CustomRequestHeaders::applyTo(QNetworkRequest &request, const QUrl &serverUrl) const
{
    if (_headers.isEmpty()) {
        return;
    }

    if (!isSameServer(request.url(), serverUrl)) {
        auto headers = request.headers();
        for (const auto &[name, value] : _headers) {
            if (request.rawHeader(name) == value) {
                headers.removeAll(name);
            }
        }
        request.setHeaders(headers);
        return;
    }

    for (const auto &[name, value] : _headers) {
        if (!request.hasRawHeader(name)) {
            request.setRawHeader(name, value);
        }
    }

    const auto redirectPolicy = request.attribute(QNetworkRequest::RedirectPolicyAttribute);
    if (!redirectPolicy.isValid() || redirectPolicy.toInt() != QNetworkRequest::ManualRedirectPolicy) {
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    }
}

} // namespace OCC
