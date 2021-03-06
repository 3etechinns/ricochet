/* Ricochet - https://ricochet.im/
 * Copyright (C) 2014, John Brooks <john.brooks@dereferenced.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 *    * Neither the names of the copyright owners nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ContactsModel.h"
#include "core/IdentityManager.h"
#include "core/ContactsManager.h"
#include <QDebug>

inline bool contactSort(const ContactUser *c1, const ContactUser *c2)
{
    if (c1->status() != c2->status())
        return c1->status() < c2->status();
    return c1->nickname().localeAwareCompare(c2->nickname()) < 0;
}

ContactsModel::ContactsModel(QObject *parent)
    : QAbstractListModel(parent), m_identity(0)
{
}

void ContactsModel::setIdentity(UserIdentity *identity)
{
    if (identity == m_identity)
        return;

    beginResetModel();

    foreach (ContactUser *user, contacts)
        user->disconnect(this);
    contacts.clear();

    if (m_identity) {
        disconnect(m_identity, 0, this, 0);
        disconnect(&m_identity->contacts, 0, this, 0);
    }

    m_identity = identity;

    if (m_identity) {
        connect(&identity->contacts, SIGNAL(contactAdded(ContactUser*)), SLOT(contactAdded(ContactUser*)));

        contacts = identity->contacts.contacts();
        std::sort(contacts.begin(), contacts.end(), contactSort);

        foreach (ContactUser *user, contacts)
            connectSignals(user);
    }

    endResetModel();
    emit identityChanged();
}

QModelIndex ContactsModel::indexOfContact(ContactUser *user) const
{
    int row = contacts.indexOf(user);
    if (row < 0)
        return QModelIndex();
    return index(row, 0);
}

ContactUser *ContactsModel::contact(int row) const
{
    return contacts.value(row);
}

void ContactsModel::updateUser(ContactUser *user)
{
    if (!user)
    {
        user = qobject_cast<ContactUser*>(sender());
        if (!user)
            return;
    }

    int row = contacts.indexOf(user);
    if (row < 0)
    {
        user->disconnect(this);
        return;
    }

    QList<ContactUser*> sorted = contacts;
    std::sort(sorted.begin(), sorted.end(), contactSort);
    int newRow = sorted.indexOf(user);

    if (row != newRow)
    {
        beginMoveRows(QModelIndex(), row, row, QModelIndex(), (newRow > row) ? (newRow+1) : newRow);
        contacts = sorted;
        endMoveRows();
    }
    emit dataChanged(index(newRow, 0), index(newRow, 0));
}

void ContactsModel::connectSignals(ContactUser *user)
{
    connect(user, SIGNAL(statusChanged()), SLOT(updateUser()));
    connect(user, SIGNAL(nicknameChanged()), SLOT(updateUser()));
    connect(user, SIGNAL(contactDeleted(ContactUser*)), SLOT(contactRemoved(ContactUser*)));
}

void ContactsModel::contactAdded(ContactUser *user)
{
    Q_ASSERT(!indexOfContact(user).isValid());

    connectSignals(user);

    QList<ContactUser*>::Iterator lp = qLowerBound(contacts.begin(), contacts.end(), user, contactSort);
    int row = lp - contacts.begin();

    beginInsertRows(QModelIndex(), row, row);
    contacts.insert(lp, user);
    endInsertRows();
}

void ContactsModel::contactRemoved(ContactUser *user)
{
    if (!user && !(user = qobject_cast<ContactUser*>(sender())))
        return;

    int row = contacts.indexOf(user);
    beginRemoveRows(QModelIndex(), row, row);
    contacts.removeAt(row);
    endRemoveRows();

    disconnect(user, 0, this, 0);
}

QHash<int,QByteArray> ContactsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "name";
    roles[PointerRole] = "contact";
    roles[StatusRole] = "status";
    return roles;
}

int ContactsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return contacts.size();
}

QVariant ContactsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= contacts.size())
        return QVariant();

    ContactUser *user = contacts[index.row()];

    switch (role)
    {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return user->nickname();
    case PointerRole:
        return QVariant::fromValue(user);
    case StatusRole:
        return user->status();
    }

    return QVariant();
}

