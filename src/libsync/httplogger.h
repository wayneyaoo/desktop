/*
 * SPDX-FileCopyrightText: 2020 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "owncloudlib.h"

#include <QNetworkReply>
#include <QUrl>

namespace OCC {
namespace HttpLogger {
/**
 * @brief Logs the request and, once finished, the response of @p reply.
 *
 * @param redactedHeaderNames Headers whose values must not appear in the log, in addition to Authorization.
 */
void OWNCLOUDSYNC_EXPORT logRequest(QNetworkReply *reply,
                                    QNetworkAccessManager::Operation operation,
                                    QIODevice *device,
                                    const QList<QByteArray> &redactedHeaderNames = {});

    /**
    * Helper to construct the HTTP verb used in the request
    */
    QByteArray OWNCLOUDSYNC_EXPORT requestVerb(QNetworkAccessManager::Operation operation, const QNetworkRequest &request);
    inline QByteArray requestVerb(const QNetworkReply &reply)
    {
        return requestVerb(reply.operation(), reply.request());
    }
}
}
