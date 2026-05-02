/****************************************************************************
** Meta object code from reading C++ file 'registry_client.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.6.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../registry_client.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'registry_client.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.6.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSRegistryClientENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSRegistryClientENDCLASS = QtMocHelpers::stringData(
    "RegistryClient",
    "connected",
    "",
    "disconnected",
    "registerAck",
    "std::vector<PeerInfo>",
    "peers",
    "queryAck",
    "unregisterAck",
    "errorOccurred",
    "message",
    "browseResult",
    "path",
    "std::vector<DirEntry>",
    "entries",
    "browseError",
    "onConnected",
    "onDisconnected",
    "onReadyRead",
    "onSocketError",
    "QAbstractSocket::SocketError",
    "error"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSRegistryClientENDCLASS_t {
    uint offsetsAndSizes[44];
    char stringdata0[15];
    char stringdata1[10];
    char stringdata2[1];
    char stringdata3[13];
    char stringdata4[12];
    char stringdata5[22];
    char stringdata6[6];
    char stringdata7[9];
    char stringdata8[14];
    char stringdata9[14];
    char stringdata10[8];
    char stringdata11[13];
    char stringdata12[5];
    char stringdata13[22];
    char stringdata14[8];
    char stringdata15[12];
    char stringdata16[12];
    char stringdata17[15];
    char stringdata18[12];
    char stringdata19[14];
    char stringdata20[29];
    char stringdata21[6];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSRegistryClientENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSRegistryClientENDCLASS_t qt_meta_stringdata_CLASSRegistryClientENDCLASS = {
    {
        QT_MOC_LITERAL(0, 14),  // "RegistryClient"
        QT_MOC_LITERAL(15, 9),  // "connected"
        QT_MOC_LITERAL(25, 0),  // ""
        QT_MOC_LITERAL(26, 12),  // "disconnected"
        QT_MOC_LITERAL(39, 11),  // "registerAck"
        QT_MOC_LITERAL(51, 21),  // "std::vector<PeerInfo>"
        QT_MOC_LITERAL(73, 5),  // "peers"
        QT_MOC_LITERAL(79, 8),  // "queryAck"
        QT_MOC_LITERAL(88, 13),  // "unregisterAck"
        QT_MOC_LITERAL(102, 13),  // "errorOccurred"
        QT_MOC_LITERAL(116, 7),  // "message"
        QT_MOC_LITERAL(124, 12),  // "browseResult"
        QT_MOC_LITERAL(137, 4),  // "path"
        QT_MOC_LITERAL(142, 21),  // "std::vector<DirEntry>"
        QT_MOC_LITERAL(164, 7),  // "entries"
        QT_MOC_LITERAL(172, 11),  // "browseError"
        QT_MOC_LITERAL(184, 11),  // "onConnected"
        QT_MOC_LITERAL(196, 14),  // "onDisconnected"
        QT_MOC_LITERAL(211, 11),  // "onReadyRead"
        QT_MOC_LITERAL(223, 13),  // "onSocketError"
        QT_MOC_LITERAL(237, 28),  // "QAbstractSocket::SocketError"
        QT_MOC_LITERAL(266, 5)   // "error"
    },
    "RegistryClient",
    "connected",
    "",
    "disconnected",
    "registerAck",
    "std::vector<PeerInfo>",
    "peers",
    "queryAck",
    "unregisterAck",
    "errorOccurred",
    "message",
    "browseResult",
    "path",
    "std::vector<DirEntry>",
    "entries",
    "browseError",
    "onConnected",
    "onDisconnected",
    "onReadyRead",
    "onSocketError",
    "QAbstractSocket::SocketError",
    "error"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSRegistryClientENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       8,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   86,    2, 0x06,    1 /* Public */,
       3,    0,   87,    2, 0x06,    2 /* Public */,
       4,    1,   88,    2, 0x06,    3 /* Public */,
       7,    1,   91,    2, 0x06,    5 /* Public */,
       8,    0,   94,    2, 0x06,    7 /* Public */,
       9,    1,   95,    2, 0x06,    8 /* Public */,
      11,    2,   98,    2, 0x06,   10 /* Public */,
      15,    1,  103,    2, 0x06,   13 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      16,    0,  106,    2, 0x08,   15 /* Private */,
      17,    0,  107,    2, 0x08,   16 /* Private */,
      18,    0,  108,    2, 0x08,   17 /* Private */,
      19,    1,  109,    2, 0x08,   18 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 13,   12,   14,
    QMetaType::Void, QMetaType::QString,   10,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 20,   21,

       0        // eod
};

Q_CONSTINIT const QMetaObject RegistryClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSRegistryClientENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSRegistryClientENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSRegistryClientENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<RegistryClient, std::true_type>,
        // method 'connected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'disconnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'registerAck'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const std::vector<PeerInfo> &, std::false_type>,
        // method 'queryAck'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const std::vector<PeerInfo> &, std::false_type>,
        // method 'unregisterAck'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'errorOccurred'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'browseResult'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const std::vector<DirEntry> &, std::false_type>,
        // method 'browseError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onConnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDisconnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onReadyRead'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSocketError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QAbstractSocket::SocketError, std::false_type>
    >,
    nullptr
} };

void RegistryClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<RegistryClient *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->connected(); break;
        case 1: _t->disconnected(); break;
        case 2: _t->registerAck((*reinterpret_cast< std::add_pointer_t<std::vector<PeerInfo>>>(_a[1]))); break;
        case 3: _t->queryAck((*reinterpret_cast< std::add_pointer_t<std::vector<PeerInfo>>>(_a[1]))); break;
        case 4: _t->unregisterAck(); break;
        case 5: _t->errorOccurred((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->browseResult((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<std::vector<DirEntry>>>(_a[2]))); break;
        case 7: _t->browseError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->onConnected(); break;
        case 9: _t->onDisconnected(); break;
        case 10: _t->onReadyRead(); break;
        case 11: _t->onSocketError((*reinterpret_cast< std::add_pointer_t<QAbstractSocket::SocketError>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 11:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (RegistryClient::*)();
            if (_t _q_method = &RegistryClient::connected; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (RegistryClient::*)();
            if (_t _q_method = &RegistryClient::disconnected; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (RegistryClient::*)(const std::vector<PeerInfo> & );
            if (_t _q_method = &RegistryClient::registerAck; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (RegistryClient::*)(const std::vector<PeerInfo> & );
            if (_t _q_method = &RegistryClient::queryAck; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (RegistryClient::*)();
            if (_t _q_method = &RegistryClient::unregisterAck; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (RegistryClient::*)(const QString & );
            if (_t _q_method = &RegistryClient::errorOccurred; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (RegistryClient::*)(const QString & , const std::vector<DirEntry> & );
            if (_t _q_method = &RegistryClient::browseResult; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (RegistryClient::*)(const QString & );
            if (_t _q_method = &RegistryClient::browseError; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
    }
}

const QMetaObject *RegistryClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RegistryClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSRegistryClientENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int RegistryClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void RegistryClient::connected()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void RegistryClient::disconnected()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void RegistryClient::registerAck(const std::vector<PeerInfo> & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void RegistryClient::queryAck(const std::vector<PeerInfo> & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void RegistryClient::unregisterAck()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void RegistryClient::errorOccurred(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void RegistryClient::browseResult(const QString & _t1, const std::vector<DirEntry> & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void RegistryClient::browseError(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}
QT_WARNING_POP
