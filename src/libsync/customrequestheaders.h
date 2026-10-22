/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CUSTOMREQUESTHEADERS_H
#define CUSTOMREQUESTHEADERS_H

#include "owncloudlib.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>
#include <utility>

class QNetworkRequest;
class QUrl;

namespace OCC
{

/**
 * @brief HTTP headers a user configured to send with every request to an account's server.
 *
 * A typical use is a token for an authenticating reverse proxy in front of the server.
 * Instances only hold headers that passed validation in fromText().
 */
class OWNCLOUDSYNC_EXPORT CustomRequestHeaders
{
public:
    using Header = std::pair<QByteArray, QByteArray>;

    /** @brief Why fromText() rejected its input. */
    enum class ParseError {
        NoError,
        MissingSeparator, //!< A line has no colon between name and value.
        InvalidName, //!< A name is empty or not an HTTP token.
        InvalidValue, //!< A value is empty or contains characters other than printable ASCII, space and tab.
        ReservedName, //!< A name is managed by the client itself, for example Authorization.
        DuplicateName, //!< A name appears more than once, ignoring case.
        TooLarge, //!< The headers as text would exceed MaximumSize bytes.
    };

    /**
     * @brief Upper limit for the size of toText() in bytes.
     *
     * It keeps the stored headers within a single keychain entry on Windows.
     */
    static constexpr qsizetype MaximumSize = 2048;

    CustomRequestHeaders() = default;

    /**
     * @brief Parses one "Name: value" header per line.
     *
     * Blank lines are ignored and whitespace around names and values is removed.
     *
     * @param error Receives the reason when parsing fails.
     * @param errorLine Receives the 1-based line number when parsing fails.
     * @return The headers, or no value if any line is invalid.
     */
    [[nodiscard]] static std::optional<CustomRequestHeaders> fromText(const QString &text, ParseError *error = nullptr, qsizetype *errorLine = nullptr);

    /** @brief The headers as "Name: value" lines, which fromText() reads back unchanged. */
    [[nodiscard]] QString toText() const;

    [[nodiscard]] QList<QByteArray> names() const;
    [[nodiscard]] bool isEmpty() const;

    /**
     * @brief Adds the headers to @p request if it goes to the same server as @p serverUrl.
     *
     * The scheme, host and port must match, with ws and wss treated as http and https.
     * Headers the request already has are left unchanged. Unless the request handles redirects
     * manually, Qt is only allowed to follow redirects within the same origin, because it would
     * send the headers to the redirect target.
     *
     * A request to another server loses the headers it carries with the configured name and value,
     * as a redirected request is a copy of the original one.
     */
    void applyTo(QNetworkRequest &request, const QUrl &serverUrl) const;

    bool operator==(const CustomRequestHeaders &other) const = default;

private:
    explicit CustomRequestHeaders(QList<Header> headers);

    QList<Header> _headers;
};

} // namespace OCC

#endif // CUSTOMREQUESTHEADERS_H
