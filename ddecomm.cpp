// References
//     http://support.microsoft.com/kb/279721/ko
//     http://www.angelfire.com/biz/rhaminisys/ddeinfo.html
//     http://msdn.microsoft.com/en-us/library/windows/desktop/ms648711(v=vs.85).aspx
//     http://msdn.microsoft.com/en-us/library/windows/desktop/ms648713(v=vs.85).aspx
//     http://stackoverflow.com/questions/3306216/how-to-be-notified-of-any-update-from-dynamic-data-exchange-dde

#include "ddecomm.h"


#define Log(X)   log(QString("%1: %2").arg(__FUNCTION__).arg((X)))  //这个信号如果与槽连接可以输出程序详细的调试信息

//将Clipboard类型的文本转为QString类型
static QString getStringFromCFTEXT(char *buff, ulong size)
{
    // CF_TEXT type: Each line ends with a carriage return/linefeed (CR-LF) combination.
    //               A null character signals the end of the data.
    QTextCodec *codec = QTextCodec::codecForLocale();
    QString text = (size >= 3) ? codec->toUnicode(buff, size - 3) : 0;
    //    QString text = (size >= 3) ? QString::fromLatin1(buff, size - 3) : 0;
    return text;
}
//回调函数
static inline HDDEDATA CALLBACK DdeCallbackLocal(
        DdeComm *comm, ulong *pIdInst,
        UINT uType,     // Transaction type. 传输数据类型
        UINT uFmt,      // Clipboard data format. 数据格式
        HCONV hconv,    // Handle to the conversation. 会话的句柄
        HSZ hsz1,       // Handle to a string depends on the type of the current transaction.
        HSZ hsz2,       // Handle to a string depends on the type of the current transaction.
        HDDEDATA hdata, // Handle to a global memory object depends on the type of the current transaction.
        DWORD dwData1,  // Transaction-specific data.
        DWORD dwData2)  // Transaction-specific data.
{
    HDDEDATA ret = DDE_FNOTPROCESSED;
    ulong idInst = *pIdInst;

    // debug
    QString msg = QString("Callback: uType=%1h, uFmt=%2h, hconv=%3h, "
                          "hsz1=%4h, hsz2=%5h, hdata=%6h, dwData1=%7h, dwData2=%8h")
            .arg((ulong)uType, 0, 16)
            .arg((ulong)uFmt, 0, 16)
            .arg((UINT64)hconv, 0, 16)
            .arg((UINT64)hsz1, 0, 16)
            .arg((UINT64)hsz2, 0, 16)
            .arg((UINT64)hdata, 0, 16)
            .arg((ulong)dwData1, 0, 16)
            .arg((ulong)dwData2, 0, 16);
    comm->log(msg);  //把收到的消息作信号的参数发射出去

    if(idInst && uType == XTYP_ADVDATA && uFmt == CF_TEXT)
    {
        wchar_t strBuff[256]; //16位Unicode字符
        char szResult[256];   //8位Windows字符（ANSI）
        DWORD size;

        // conversation  为什么要强制转换类型????? 答:可能是为了方便将其当作log信号的发射参数
        UINT64 conversation = (UINT64)hconv;
        //qDebug() << "Conversation is" << conversation << endl;
        // topic  将与字符串句柄hsz1关联的文本复制到缓冲区strBuff中。wchar_t类型长度指的是字符数不是字节数
        size = DdeQueryString(idInst, hsz1, strBuff, sizeof(strBuff) / sizeof(wchar_t), CP_WINUNICODE);
        QString topic = QString::fromWCharArray(strBuff, size);  //返回经转换后的字符串
        DdeFreeStringHandle(idInst, hsz1);                       //释放hsz1

        // item  以下代码作用与上面相似
        size = DdeQueryString(idInst, hsz2, strBuff, sizeof(strBuff) / sizeof(wchar_t), CP_WINUNICODE);
        QString item = QString::fromWCharArray(strBuff, size);
        DdeFreeStringHandle(idInst, hsz2);

        // data  把数据取出到本地缓冲 返回值是数据是长度
        size = DdeGetData(hdata, (unsigned char *)szResult, sizeof(szResult), 0);
        QString text = getStringFromCFTEXT(szResult, size);  //调用getStringFromCFTEXT函数将数据转为QString类型

        // fire event
        emit comm->advised(conversation, topic, item, text);  //发射advised信号

        comm->log(QString("Advised: conv=%1h, topic=%2, item=%3, text=%4")
                  .arg((quintptr)hconv, 0, 16).arg(topic).arg(item).arg(text));  //将解析后的消息作为log的参数发射出去
        ret = (HDDEDATA)DDE_FACK;
    }
    return ret;
}
//构造函数
DdeComm::DdeComm(QObject *parent)
    : QObject(parent),
      mSync(QMutex::Recursive), mDdeInstance(0)
{    
    //指定函数符为lambda的匿名函数
    auto lambda = [&] (UINT uType, UINT uFmt, HCONV hconv, HSZ hsz1, HSZ hsz2,
            HDDEDATA hdata, DWORD dwData1,DWORD dwData2 ) -> HDDEDATA
    {
        return DdeCallbackLocal(this, &mDdeInstance, uType, uFmt, hconv, hsz1, hsz2, hdata, dwData1, dwData2);
    };
    singleton<decltype(lambda)>::set_instance(lambda);  //从匿名函数获得一个回调函数
    PFNCALLBACK callback = &lambda_caller<decltype(lambda)>;  //取得回调函数的指针

    DWORD inst = 0;
    UINT result = DdeInitialize(&inst, callback, APPCLASS_STANDARD | APPCMD_CLIENTONLY ,0);

    // HOWTO
    //    UINT result = DdeInitialize(
    //                (LPDWORD) &inst,  // instance identifier
    //                &DdeCallback,        // pointer to callback function
    //                APPCLASS_MONITOR |  // this is a monitoring application
    //                MF_CALLBACKS     |  // monitor callback functions
    //                MF_CONV          |  // monitor conversation data
    //                MF_ERRORS        |  // monitor DDEML errors
    //                MF_HSZ_INFO      |  // monitor data handle activity
    //                MF_LINKS         |  // monitor advise loops
    //                MF_POSTMSGS      |  // monitor posted DDE messages
    //                MF_SENDMSGS,        // monitor sent DDE messages
    //                0);                 // reserved

    if (result == DMLERR_NO_ERROR) {
        mDdeInstance = inst;
        Log(tr("DDE initialization succeeded: instance=%1h").arg((ulong)inst, 0, 16)); //发射信号出去
    }
    else {
        Log(tr("DDE initialization failed: result=%1h").arg((ulong)result, 0, 16));
    }
}

DdeComm::~DdeComm()
{
    DdeUninitialize(mDdeInstance);
    mDdeInstance = 0;

    Log(tr("DDE released"));  //发射信号说已析构
}

//内联函数 执行连接应用服务器作用 并返回会话句柄  多次重复的操作适合用内联函数
static inline HCONV _openConv(DWORD idInst, QString application, QString topic)
{
    //DDE Connect to Server using given AppName and topic.
    HSZ hszApp = DdeCreateStringHandle(idInst, application.toStdWString().c_str(), 0);
    HSZ hszTopic = DdeCreateStringHandle(idInst, topic.toStdWString().c_str(), 0);
    HCONV hConv = DdeConnect(idInst, hszApp, hszTopic, NULL); // If the function fails, the return value is 0L.
    DdeFreeStringHandle(idInst, hszApp);
    DdeFreeStringHandle(idInst, hszTopic);

    //    If the function succeeds, the return value is the handle to the established conversation.
    //    If the function fails, the return value is 0L.
    return hConv;
}

static inline BOOL _closeConv(HCONV hConv)
{
    //    If the function succeeds, the return value is nonzero.
    //    If the function fails, the return value is zero.
    return DdeDisconnect(hConv);
}

QString DdeComm::request(QString application, QString topic, QString item, ulong timeoutInMs)
{
    QString text = 0;
    DWORD idInst = mDdeInstance;
    HCONV hConv = _openConv(idInst, application, topic);
    UINT64 conversation = (UINT64)hConv;
    if (hConv) {
        HSZ hszItem = DdeCreateStringHandle(idInst, item.toStdWString().c_str(), CP_WINUNICODE);
        HDDEDATA hData = DdeClientTransaction(NULL, 0, hConv, hszItem, CF_TEXT, XTYP_REQUEST, timeoutInMs, NULL);
        DdeFreeStringHandle(idInst, hszItem);

        if (hData)
        {
            char szResult[255];
            DWORD size = DdeGetData(hData, (unsigned char *)szResult, sizeof(szResult), 0);
            text = getStringFromCFTEXT(szResult, size);
            emit requested(conversation, topic, item, text);  //发射requested信号
        }
        else {
            UINT errCode = DdeGetLastError(mDdeInstance);
            Log(tr("No request transaction: app=%1, topic=%2, item=%3, errCode=%4h")
                .arg(application).arg(topic).arg(item).arg(errCode, 0, 16));
        }
        _closeConv(hConv);
    }
    else {
        UINT errCode = DdeGetLastError(mDdeInstance);
        Log(tr("No conversation opened: errCode=%1h").arg(errCode, 0, 16));
    }
    return text;
}
//向服务器发送数据
bool DdeComm::poke(QString application, QString topic, QString item, QString text, ulong timeoutInMs)
{
    bool ret = false;
    DWORD idInst = mDdeInstance;
    HCONV hConv = _openConv(idInst, application, topic);
    UINT64 conversation = (UINT64)hConv;
    if (hConv) {
        HSZ hszItem = DdeCreateStringHandle(idInst, item.toStdWString().c_str(), CP_WINUNICODE);
        QByteArray byteArray = text.toLocal8Bit();
        byteArray.append((char)0);  // zero terminated string
        if (DdeClientTransaction((LPBYTE)byteArray.data(), byteArray.size(),
                                 hConv, hszItem, CF_TEXT, XTYP_POKE, timeoutInMs, NULL)) {
            ret = true;
        }
        DdeFreeStringHandle(idInst, hszItem);
        _closeConv(hConv);
        emit poked(conversation, topic, item, text); //发射poked信号
    }
    else {
        UINT errCode = DdeGetLastError(mDdeInstance);
        Log(tr("No conversation opened: errCode=%1h").arg(errCode, 0, 16));
    }
    return ret;
}
//向服务器发送命令  命令怎么执行由服务器的相应代码决定
bool DdeComm::execute(QString application, QString topic, QString command, ulong timeoutInMs)
{
    bool ret = false;
    DWORD idInst = mDdeInstance;
    HCONV hConv = _openConv(idInst, application, topic);
    UINT64 conversation = (UINT64)hConv;
    if (hConv) {
        QByteArray byteArray = command.toLocal8Bit();
        byteArray.append((char)0);  // zero terminated string
        HDDEDATA hData = DdeCreateDataHandle(idInst, (LPBYTE)byteArray.data(), byteArray.size(), 0, NULL, CF_TEXT, 0);
        if (hData)   {
            DdeClientTransaction((LPBYTE)hData, 0xFFFFFFFF, hConv, 0L, 0, XTYP_EXECUTE, timeoutInMs, NULL);
            DdeFreeDataHandle(hData);
            emit executed(conversation, topic, command);  //发射executed信号
            ret = true;
        }
        else {
            UINT errCode = DdeGetLastError(mDdeInstance);
            Log(tr("Command failed: app=%1, topic=%2, cmd=%3, errCode=%4h")
                .arg(application).arg(topic).arg(command).arg(errCode, 0, 16));
        }
        _closeConv(hConv);
    }
    else {
        UINT errCode = DdeGetLastError(mDdeInstance);
        Log(tr("No conversation opened: errCode=%1h").arg(errCode, 0, 16));
    }
    return ret;
}
//连接DDE服务器
UINT64 DdeComm::open(QString application, QString topic)
{
    QMutexLocker lock(&mSync);
    DWORD idInst = mDdeInstance;
    HCONV hConv = _openConv(idInst, application, topic);
    UINT64 conversation = (UINT64)hConv;
    if (hConv) {
        Log(tr("New conversation opened: application=%1, topic=%2, conv=%3h")
            .arg(application).arg(topic).arg(conversation, 0, 16));
        emit opened(conversation, application, topic);  //发射opened信号
    }
    else {
        UINT errCode = DdeGetLastError(mDdeInstance);
        Log(tr("No conversation opened: application=%1, topic=%2, errCode=%3h")
            .arg(application).arg(topic).arg(errCode, 0, 16));
    }
    return conversation;
}
//关闭与DDE服务器的连接
bool DdeComm::close(UINT64 conversation)
{
    bool ret = false;
    QMutexLocker lock(&mSync);
    HCONV hConv = (HCONV)conversation;  //conversation类型如为ulong 会提示数据类型长度太短
    BOOL retConv = _closeConv(hConv);
    if (retConv) {
        Log(tr("Conversation closed: conv=%1h").arg(conversation, 0, 16));
        emit closed(conversation);   //发身closed信号
        ret = true;
    }
    else {
        UINT errCode = DdeGetLastError(mDdeInstance);
        Log(tr("No conversation closed: conv=%1h, errCode=%2h")
            .arg(conversation, 0, 16).arg(errCode, 0, 16));
    }
    return ret;
}
//订阅数据项
bool DdeComm::advise(UINT64 conversation, QString item, ulong timeoutInMs)
{
    bool ret = false;
    QMutexLocker lock(&mSync);
    DWORD idInst = mDdeInstance;
    HCONV hConv = (HCONV) conversation;    
    if (hConv) {
        HSZ hszItem = DdeCreateStringHandle(idInst, item.toStdWString().c_str(), CP_WINUNICODE);
        HDDEDATA hData = DdeClientTransaction(NULL, 0, hConv, hszItem, CF_TEXT, XTYP_ADVSTART, timeoutInMs, NULL);
        DdeFreeStringHandle(idInst, hszItem);
        if (hData) {
            log(tr("New advice started: conv=%1h, item=%2")
                .arg(conversation, 0, 16).arg(item));
            emit adviceUpdated(conversation, item, true);  //发射adviceUpdated更新信号
            ret = true;
        }
        else {
            UINT errCode = DdeGetLastError(mDdeInstance);
            Log(tr("No advice transaction: conv=%1h, item=%2, errCode=%3h")
                .arg(conversation, 0, 16).arg(item).arg(errCode, 0, 16));
        }
    }
    else
        Log(tr("No conversation opened"));
    return ret;
}
//退订数据项
bool DdeComm::unadvise(UINT64 conversation, QString item, ulong timeoutInMs)
{
    bool ret = false;
    QMutexLocker lock(&mSync);
    DWORD idInst = mDdeInstance;
    HCONV hConv = (HCONV)conversation;
    if (hConv) {
        HSZ hszItem = DdeCreateStringHandle(idInst, item.toStdWString().c_str(), CP_WINUNICODE);
        HDDEDATA hData = DdeClientTransaction(NULL, 0, hConv, hszItem, CF_TEXT, XTYP_ADVSTOP, timeoutInMs, NULL);
        DdeFreeStringHandle(idInst, hszItem);
        if (hData) {
            Log(tr("Advice canceled: conv=%1h, item=%2").arg(conversation, 0, 16).arg(item));
            emit adviceUpdated(conversation, item, false);  //订阅发生变生的信号
            ret = true;
        }
        else {
            UINT errCode = DdeGetLastError(mDdeInstance);
            Log(tr("No advice transaction: conv=%1h, item=%2, errCode=%3h")
                .arg(conversation, 0, 16).arg(item).arg(errCode, 0, 16));
        }
        _closeConv(hConv);
    }
    else
        Log(tr("No conversation opened"));
    return ret;
}
