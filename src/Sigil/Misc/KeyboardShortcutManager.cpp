/************************************************************************
**
**  Copyright (C) 2011  John Schember <john@nachtimwald.com>
**
**  This file is part of Sigil.
**
**  Sigil is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Sigil is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Sigil.  If not, see <http://www.gnu.org/licenses/>.
**
*************************************************************************/

#include <stdafx.h>
#include <QAction>
#include <QHashIterator>
#include <QRegExp>
#include <QShortcut>
#include <QSettings>
#include "KeyboardShortcutManager.h"

static const QString SETTINGS_GROUP = "shortcuts";
KeyboardShortcutManager *KeyboardShortcutManager::m_instance = 0;

KeyboardShortcutManager *KeyboardShortcutManager::instance()
{
    if (m_instance == 0) {
        m_instance = new KeyboardShortcutManager();
    }

    return m_instance;
}

KeyboardShortcutManager::~KeyboardShortcutManager()
{
    writeSettings();
}

void KeyboardShortcutManager::registerAction(QAction *action, const QString &id, const QString &description)
{
    KeyboardShortcut s;
    QKeySequence keySequence = action->shortcut();
    QString desc = description;

    if (m_shortcuts.contains(id)) {
        s = m_shortcuts.value(id);
    }
    else {
        s = createShortcut(keySequence);
        m_shortcuts.insert(id, s);
    }
    // Use the actions tool tip (falls back to text) if no description
    // was given.
    if (desc.isEmpty()) {
        desc = action->toolTip();
        desc.remove(QRegExp("<[^>]*>"));
    }
    s.setDescription(desc.simplified());
    s.setAction(action);

    // If we are registering with a KeyboardShortcut that was previously created
    // we don't want to over write the key sequence.
    if (s.keySequence().isEmpty() && !keySequenceInUse(keySequence)) {
        s.setKeySequence(keySequence);
    }
    if (s.defaultKeySequence().isEmpty() && !defaultKeySequenceInUse(keySequence)) {
        s.setDefaultKeySequence(keySequence);
    }

    // Set the keyboard shortcut that is associated with this id.
    s.action()->setShortcut(s.keySequence());
}

void KeyboardShortcutManager::registerShortcut(QShortcut *shortcut, const QString &id, const QString &description)
{
    KeyboardShortcut s;
    QKeySequence keySequence = shortcut->key();
    QString desc = description;

    if (m_shortcuts.contains(id)) {
        s = m_shortcuts.value(id);
    }
    else {
        s = createShortcut(keySequence);
        m_shortcuts.insert(id, s);
    }
    // Use the actions tool tip (falls back to text) if no description
    // was given.
    if (!desc.isEmpty()) {
        desc.remove(QRegExp("<[^>]*>"));
        s.setDescription(desc.simplified());
    }
    s.setShortcut(shortcut);

    // If we are registering with a KeyboardShortcut that was previously created
    // we don't want to over write the key sequence.
    if (s.keySequence().isEmpty() && !keySequenceInUse(keySequence)) {
        s.setKeySequence(keySequence);
    }
    if (s.defaultKeySequence().isEmpty() && !defaultKeySequenceInUse(keySequence)) {
        s.setDefaultKeySequence(keySequence);
    }

    // Set the keyboard shortcut that is associated with this id.
    s.shortcut()->setKey(s.keySequence());
}

bool KeyboardShortcutManager::setKeySequence(const QString &id, const QKeySequence &keySequence, bool isDefault)
{
    if (!keySequence.isEmpty() && keySequenceInUse(keySequence)) {
        // Resettings to a keysequence's default value will always succeed.
        if (isDefault) {
            // Find the shortcut that is using this default sequence.
            QString id;
            foreach (QString tid, m_shortcuts.keys()) {
                if (m_shortcuts[tid].keySequence() == keySequence) {
                    id = tid;
                    break;
                }
            }
            if (!id.isEmpty()) {
                // Set the shortcut that is using this default to it's default
                // only if it's default is not in use otherwise clear it. We
                // don't want to end up in an infinite loop if multiple shortcuts
                // have the same default set so we try once instead of following
                // the shortcut chain.
                if (!keySequenceInUse(m_shortcuts[id].defaultKeySequence())) {
                    setKeySequence(id, m_shortcuts[id].defaultKeySequence());
                }
                else {
                    setKeySequence(id, QKeySequence());
                }
            }
        }
        else {
            // The keysequence for a shortcut will be in use if it was set from
            // a saved state but we still need to regeister it. Only stop here
            // if this keysequence doens't match the one for the shortcut we're
            // looking at.
            if (m_shortcuts[id].keySequence() != keySequence) {
                return false;
            }
        }
    }

    KeyboardShortcut s;

    if (!m_shortcuts.contains(id)) {
        s = createShortcut(keySequence, keySequence);
        m_shortcuts.insert(id, s);
    }
    else {
        s = m_shortcuts.value(id);
    }

    s.setKeySequence(keySequence);
    if (s.action() != 0) {
        s.action()->setShortcut(keySequence);
    }
    if (s.shortcut() != 0) {
        s.shortcut()->setKey(keySequence);
    }

    return true;
}

bool KeyboardShortcutManager::setDefaultKeySequence(const QString &id, const QKeySequence &keySequence)
{
    if (defaultKeySequenceInUse(keySequence)) {
        return false;
    }

    KeyboardShortcut s = m_shortcuts.value(id);
    s.setDefaultKeySequence(keySequence);

    return true;
}

void KeyboardShortcutManager::resetKeySequence(const QString &id)
{
    KeyboardShortcut s = m_shortcuts.value(id);
    setKeySequence(id, s.defaultKeySequence(), true);
}

void KeyboardShortcutManager::setDescription(const QString &id, const QString &description)
{
    KeyboardShortcut s = m_shortcuts.value(id);
    s.setDescription(description);
}

void KeyboardShortcutManager::unregisterId(const QString &id)
{
    m_shortcuts.remove(id);
}

void KeyboardShortcutManager::unregisterAction(QAction *action)
{
    QStringList ids;

    // Gather a list of ids. There could be more than one and we don't
    // want to modify the list as we search.
    foreach (QString key, m_shortcuts.keys()) {
        KeyboardShortcut s = m_shortcuts.value(key);
        if (s.action() == action) {
            ids.append(key);
        }
    }

    foreach (QString id, ids) {
        unregisterId(id);
    }
}

void KeyboardShortcutManager::unregisterShortcut(QShortcut *shortcut)
{
    QStringList ids;

    // Gather a list of ids. There could be more than one and we don't
    // want to modify the list as we search.
    foreach (QString key, m_shortcuts.keys()) {
        KeyboardShortcut s = m_shortcuts.value(key);
        if (s.shortcut() == shortcut) {
            ids.append(key);
        }
    }

    foreach (QString id, ids) {
        unregisterId(id);
    }
}

void KeyboardShortcutManager::unregisterAll()
{
    m_shortcuts.clear();
}

KeyboardShortcut KeyboardShortcutManager::keyboardShortcut(const QString &id)
{
    if (!m_shortcuts.contains(id)) {
        KeyboardShortcut s = createShortcut(QKeySequence(), QKeySequence());
        m_shortcuts.insert(id, s);
    }
    return m_shortcuts.value(id);
}

QStringList KeyboardShortcutManager::ids()
{
    return m_shortcuts.keys();
}

bool KeyboardShortcutManager::keySequenceInUse(const QKeySequence &keySequence)
{
    foreach (KeyboardShortcut s, m_shortcuts) {
        if (s.keySequence() == keySequence) {
            return true;
        }
    }
    return false;
}

bool KeyboardShortcutManager::defaultKeySequenceInUse(const QKeySequence &keySequence)
{
    foreach (KeyboardShortcut s, m_shortcuts) {
        if (s.defaultKeySequence() == keySequence) {
            return true;
        }
    }
    return false;
}

void KeyboardShortcutManager::writeSettings()
{
    QSettings settings;
    settings.beginWriteArray(SETTINGS_GROUP);
    int i = 0;

    QHashIterator<QString, KeyboardShortcut> it(m_shortcuts);
    while (it.hasNext()) {
        it.next();
        KeyboardShortcut s = it.value();

        if (s.action() == 0 && s.shortcut() == 0) {
            continue;
        }
        settings.setArrayIndex(i);
        settings.setValue("id", it.key());
        settings.setValue("keySequence", s.keySequence().toString());

        i++;
    }

    settings.endArray();
}

KeyboardShortcutManager::KeyboardShortcutManager()
{
    readSettings();
}

KeyboardShortcut KeyboardShortcutManager::createShortcut(const QKeySequence &keySequence, const QKeySequence &defaultKeySequence, const QString &description)
{
    KeyboardShortcut s;

    s.setKeySequence(keySequence);
    s.setDefaultKeySequence(defaultKeySequence);
    s.setDescription(description);

    return s;
}

void KeyboardShortcutManager::readSettings()
{
    QSettings settings;
    int size = settings.beginReadArray(SETTINGS_GROUP);

    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        const QString id = settings.value("id").toString();
        const QKeySequence keySequence(settings.value("keySequence").toString());

        //KeyboardShortcut s = createShortcut(keySequence, defaultKeySequence, description);
        KeyboardShortcut s = createShortcut(keySequence);
        m_shortcuts.insert(id, s);
    }
}
