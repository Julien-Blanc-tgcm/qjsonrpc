#include <QTimer>
#include <QEventLoop>
#include <QDebug>

#include "qjsonrpcservice.h"
#include "qjsonrpcservicereply_p.h"
#include "qjsonrpcservicereply.h"
#include "qjsonrpcsocket_p.h"
#include "qjsonrpcsocket.h"

int QJsonRpcSocketPrivate::findJsonDocumentEnd(const QByteArray &jsonData)
{
    const char* pos = jsonData.constData();
    const char* end = pos + jsonData.length();

    char blockStart = 0;
    char blockEnd = 0;
    int index = 0;

    // Find the beginning of the JSON document and determine if it is an object or an array
    while (true) {
        if (pos == end) {
            return -1;
        } else if (*pos == '{') {
            blockStart = '{';
            blockEnd = '}';
            break;
        } else if(*pos == '[') {
            blockStart = '[';
            blockEnd = ']';
            break;
        }

        pos++;
        index++;
    }

    // Find the end of the JSON document
    pos++;
    index++;
    int depth = 1;
    bool inString = false;
    while (depth > 0 && pos <= end) {
        if (*pos == '\\') {
            pos += 2;
            index += 2;
            continue;
        } else if (*pos == '"') {
            inString = !inString;
        } else if (!inString) {
            if (*pos == blockStart)
                depth++;
            else if (*pos == blockEnd)
                depth--;
        }

        pos++;
        index++;
    }

    // index-1 because we are one position ahead
    return depth == 0 ? index-1 : -1;
}

void QJsonRpcSocketPrivate::writeData(const QJsonRpcMessage &message)
{
    QJsonDocument doc = QJsonDocument(message.toObject());

#if QT_VERSION >= 0x050100 || QT_VERSION <= 0x050000
    QByteArray data = doc.toJson(format);
#else
    QByteArray data = doc.toJson();
#endif

    device.data()->write(data);
    qJsonRpcDebug() << "sending: " << data;
}

QJsonRpcAbstractSocket::QJsonRpcAbstractSocket(QObject *parent)
#if defined(USE_QT_PRIVATE_HEADERS)
    : QObject(*new QJsonRpcAbstractSocketPrivate, parent)
#else
    : QObject(parent),
      d_ptr(new QJsonRpcAbstractSocketPrivate)
#endif
{
}

QJsonRpcAbstractSocket::~QJsonRpcAbstractSocket()
{
}

QJsonRpcAbstractSocket::QJsonRpcAbstractSocket(QJsonRpcAbstractSocketPrivate &dd, QObject *parent)
#if defined(USE_QT_PRIVATE_HEADERS)
    : QObject(dd, parent)
#else
    : QObject(parent),
      d_ptr(&dd)
#endif
{
}

#if QT_VERSION >= 0x050100 || QT_VERSION <= 0x050000
QJsonDocument::JsonFormat QJsonRpcAbstractSocket::wireFormat() const
{
    Q_D(const QJsonRpcAbstractSocket);
    return d->format;
}

void QJsonRpcAbstractSocket::setWireFormat(QJsonDocument::JsonFormat format)
{
    Q_D(QJsonRpcAbstractSocket);
    d->format = format;
}
#endif

bool QJsonRpcAbstractSocket::isValid() const
{
    return false;
}

QJsonRpcSocket::QJsonRpcSocket(QIODevice *device, QObject *parent)
#if defined(USE_QT_PRIVATE_HEADERS)
    : QJsonRpcAbstractSocket(*new QJsonRpcSocketPrivate(this), parent)
#else
    : QJsonRpcAbstractSocket(parent),
      d_ptr(new QJsonRpcSocketPrivate(this))
#endif
{
    Q_D(QJsonRpcSocket);
    connect(device, SIGNAL(readyRead()), this, SLOT(_q_processIncomingData()));
    d->device = device;
}

QJsonRpcSocket::QJsonRpcSocket(QJsonRpcSocketPrivate &dd, QObject *parent)
#if defined(USE_QT_PRIVATE_HEADERS)
    : QJsonRpcAbstractSocket(dd, parent)
#else
    : QJsonRpcAbstractSocket(parent),
      d_ptr(&dd)
#endif
{
    Q_D(QJsonRpcSocket);
    connect(d->device, SIGNAL(readyRead()), this, SLOT(_q_processIncomingData()));
}

QJsonRpcSocket::~QJsonRpcSocket()
{
}

bool QJsonRpcSocket::isValid() const
{
    Q_D(const QJsonRpcSocket);
    return d->device && d->device.data()->isOpen();
}

/*
void QJsonRpcSocket::sendMessage(const QList<QJsonRpcMessage> &messages)
{
    QJsonArray array;
    foreach (QJsonRpcMessage message, messages) {
        array.append(message.toObject());
    }

    QJsonDocument doc = QJsonDocument(array);
    m_device.data()->write(doc.toBinaryData());
}
*/

QJsonRpcMessage QJsonRpcSocket::sendMessageBlocking(const QJsonRpcMessage &message, int msecs)
{
    Q_D(QJsonRpcSocket);
    QJsonRpcServiceReply *reply = sendMessage(message);
    QScopedPointer<QJsonRpcServiceReply> replyPtr(reply);

    QEventLoop responseLoop;
    connect(reply, SIGNAL(finished()), &responseLoop, SLOT(quit()));
    QTimer::singleShot(msecs, &responseLoop, SLOT(quit()));
    responseLoop.exec();

    if (!reply->response().isValid()) {
        d->replies.remove(message.id());
        return message.createErrorResponse(QJsonRpc::TimeoutError, "request timed out");
    }

    return reply->response();
}

QJsonRpcServiceReply *QJsonRpcSocket::sendMessage(const QJsonRpcMessage &message)
{
    Q_D(QJsonRpcSocket);
    if (!d->device) {
        qJsonRpcDebug() << Q_FUNC_INFO << "trying to send message without device";
        return 0;
    }

    notify(message);
    QPointer<QJsonRpcServiceReply> reply(new QJsonRpcServiceReply);
    d->replies.insert(message.id(), reply);
    return reply;
}

void QJsonRpcSocket::notify(const QJsonRpcMessage &message)
{
    Q_D(QJsonRpcSocket);
    if (!d->device) {
        qJsonRpcDebug() << Q_FUNC_INFO << "trying to send message without device";
        return;
    }

    // disconnect the result message if we need to
    QJsonRpcService *service = qobject_cast<QJsonRpcService*>(sender());
    if (service)
        disconnect(service, SIGNAL(result(QJsonRpcMessage)), this, SLOT(notify(QJsonRpcMessage)));

    d->writeData(message);
}

QJsonRpcMessage QJsonRpcSocket::invokeRemoteMethodBlocking(const QString &method, const QVariant &param1,
                                                           const QVariant &param2, const QVariant &param3,
                                                           const QVariant &param4, const QVariant &param5,
                                                           const QVariant &param6, const QVariant &param7,
                                                           const QVariant &param8, const QVariant &param9,
                                                           const QVariant &param10)
{
    QVariantList params;
    if (param1.isValid()) params.append(param1);
    if (param2.isValid()) params.append(param2);
    if (param3.isValid()) params.append(param3);
    if (param4.isValid()) params.append(param4);
    if (param5.isValid()) params.append(param5);
    if (param6.isValid()) params.append(param6);
    if (param7.isValid()) params.append(param7);
    if (param8.isValid()) params.append(param8);
    if (param9.isValid()) params.append(param9);
    if (param10.isValid()) params.append(param10);

    QJsonRpcMessage request =
        QJsonRpcMessage::createRequest(method, QJsonArray::fromVariantList(params));
    return sendMessageBlocking(request);
}

QJsonRpcServiceReply *QJsonRpcSocket::invokeRemoteMethod(const QString &method, const QVariant &param1,
                                                         const QVariant &param2, const QVariant &param3,
                                                         const QVariant &param4, const QVariant &param5,
                                                         const QVariant &param6, const QVariant &param7,
                                                         const QVariant &param8, const QVariant &param9,
                                                         const QVariant &param10)
{
    QVariantList params;
    if (param1.isValid()) params.append(param1);
    if (param2.isValid()) params.append(param2);
    if (param3.isValid()) params.append(param3);
    if (param4.isValid()) params.append(param4);
    if (param5.isValid()) params.append(param5);
    if (param6.isValid()) params.append(param6);
    if (param7.isValid()) params.append(param7);
    if (param8.isValid()) params.append(param8);
    if (param9.isValid()) params.append(param9);
    if (param10.isValid()) params.append(param10);

    QJsonRpcMessage request =
        QJsonRpcMessage::createRequest(method, QJsonArray::fromVariantList(params));
    return sendMessage(request);
}

void QJsonRpcSocketPrivate::_q_processIncomingData()
{
    Q_Q(QJsonRpcSocket);
    if (!device) {
        qJsonRpcDebug() << Q_FUNC_INFO << "called without device";
        return;
    }

    buffer.append(device.data()->readAll());
    while (!buffer.isEmpty()) {
        int dataSize = findJsonDocumentEnd(buffer);
        if (dataSize == -1) {
            // incomplete data, wait for more
            return;
        }

        QJsonDocument document = QJsonDocument::fromJson(buffer);
        if (document.isEmpty())
            break;

        buffer = buffer.mid(dataSize + 1);
        if (document.isArray()) {
            qJsonRpcDebug() << Q_FUNC_INFO << "bulk support is current disabled";
            /*
            for (int i = 0; i < document.array().size(); ++i) {
                QJsonObject messageObject = document.array().at(i).toObject();
                if (!messageObject.isEmpty()) {
                    QJsonRpcMessage message(messageObject);
                    Q_EMIT messageReceived(message);
                }
            }
            */
        } else if (document.isObject()){
            qJsonRpcDebug() << "received: " << document.toJson();
            QJsonRpcMessage message(document.object());
            Q_EMIT q->messageReceived(message);

            if (message.type() == QJsonRpcMessage::Response ||
                message.type() == QJsonRpcMessage::Error) {
                if (replies.contains(message.id())) {
                    QPointer<QJsonRpcServiceReply> reply = replies.take(message.id());
                    if (!reply.isNull()) {
                        reply->d_func()->response = message;
                        reply->finished();
                    }
                }
            } else {
                q->processRequestMessage(message);
            }
        }
    }
}

void QJsonRpcSocket::processRequestMessage(const QJsonRpcMessage &message)
{
    Q_UNUSED(message)
    // we don't do anything the default case with requests and notifications,
    // these are only handled by the provider
}

QJsonRpcServiceSocket::QJsonRpcServiceSocket(QIODevice *device, QObject *parent)
    : QJsonRpcSocket(device, parent)
{
}

QJsonRpcServiceSocket::~QJsonRpcServiceSocket()
{
}

void QJsonRpcServiceSocket::processRequestMessage(const QJsonRpcMessage &message)
{
    QJsonRpcServiceProvider::processMessage(this, message);
}

#include "moc_qjsonrpcsocket.cpp"
