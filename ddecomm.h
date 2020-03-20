/// @brief      DDE communicator
/// @author     Sanghoon Kim <wizest@gmail.com>
/// @date       2014-10-19
/// @note       Only support CF_TEXT type

#ifndef DDECOMM_H
#define DDECOMM_H

#include <QObject>
#include <QMutex>
#include <QByteArray>
#include <Windows.h>

#include <Ddeml.h>
#include <string>
#include <iostream>
#include <QTextCodec>
#include "callback.h"


#define DDECOMM_TIMEOUT_MS       5000

class DdeComm : public QObject
{
    Q_OBJECT
private:
    QMutex mSync;
    ulong mDdeInstance;

public:
    explicit DdeComm(QObject *parent = 0);
    ~DdeComm();

public:
    /// @return     message if succeed, otherwise null string (.isNull() == true)
    QString request(QString application, QString topic, QString item, ulong timeoutInMs = DDECOMM_TIMEOUT_MS);
    bool poke(QString application, QString topic, QString item, QString text, ulong timeoutInMs = DDECOMM_TIMEOUT_MS);
    bool execute(QString application, QString topic, QString command, ulong timeoutInMs = DDECOMM_TIMEOUT_MS);

    // functions based on conversation
    /// @return     conversation to advise, number as conversation Id
    ///             if the function fails, the return value is 0L.
    UINT64 open(QString application, QString topic);
    /// @param      conversation    number as conversation Id
    bool close(UINT64 conversation);

    bool advise(UINT64 conversation, QString item, ulong timeoutInMs = DDECOMM_TIMEOUT_MS);
    bool unadvise(UINT64 conversation, QString item, ulong timeoutInMs = DDECOMM_TIMEOUT_MS);

signals:
    void requested(UINT64 conversation, QString topic, QString item, QString text);
    void poked(UINT64 conversation, QString topic, QString item, QString text);
    void executed(UINT64 conversation, QString topic, QString command);

    void opened(UINT64 conversation, QString application, QString topic);
    void closed(UINT64 conversation);
    void advised(UINT64 conversation, QString topic, QString item, QString text);
    void adviceUpdated(UINT64 conversation, QString item, bool started);

    void log(QString msg);
};

#endif // DDECOMM_H
