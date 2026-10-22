/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_ACCESS_MANAGER_H
#define MIRALL_ACCESS_MANAGER_H

#include "accountfwd.h"
#include "owncloudlib.h"
#include <QNetworkAccessManager>
#include <QWeakPointer>

class QByteArray;
class QUrl;

namespace OCC {

/**
 * @brief The AccessManager class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT AccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    static QByteArray generateRequestId();

    AccessManager(QObject *parent = nullptr);

    /**
     * @brief Sends the custom request headers of @p account with requests to its server.
     *
     * The account is held weakly because the access manager can outlive it.
     */
    void setAccount(const QWeakPointer<Account> &account);

protected:
    QNetworkReply *createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData = nullptr) override;

private:
    QWeakPointer<Account> _account;
};

} // namespace OCC

#endif
