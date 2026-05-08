#include "util/defs.h"
#include <bits/time.h>
#include <bits/types/clockid_t.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

typedef uint32_t wasi_exitcode_t;
typedef uint64_t wasi_timestamp_t;


/**
 * Identifiers for clocks.
 */
typedef enum wasi_clockid : uint32_t {
    WASI_CLOCKID_REALTIME = 0,
    WASI_CLOCKID_MONOTONIC = 1,
    WASI_CLOCKID_PROCESS_CPUTIME_ID = 2,
    WASI_CLOCKID_THREAD_CPUTIME_ID = 3,
} wasi_clockid_t;

typedef enum wasi_errno : uint16_t {
    WASI_ERRNO_SUCCESS = 0,
    WASI_ERRNO_2BIG = 1,
    WASI_ERRNO_ACCES = 2,
    WASI_ERRNO_ADDRINUSE = 3,
    WASI_ERRNO_ADDRNOTAVAIL = 4,
    WASI_ERRNO_AFNOSUPPORT = 5,
    WASI_ERRNO_AGAIN = 6,
    WASI_ERRNO_ALREADY = 7,
    WASI_ERRNO_BADF = 8,
    WASI_ERRNO_BADMSG = 9,
    WASI_ERRNO_BUSY = 10,
    WASI_ERRNO_CANCELED = 11,
    WASI_ERRNO_CHILD = 12,
    WASI_ERRNO_CONNABORTED = 13,
    WASI_ERRNO_CONNREFUSED = 14,
    WASI_ERRNO_CONNRESET = 15,
    WASI_ERRNO_DEADLK = 16,
    WASI_ERRNO_DESTADDRREQ = 17,
    WASI_ERRNO_DOM = 18,
    WASI_ERRNO_DQUOT = 19,
    WASI_ERRNO_EXIST = 20,
    WASI_ERRNO_FAULT = 21,
    WASI_ERRNO_FBIG = 22,
    WASI_ERRNO_HOSTUNREACH = 23,
    WASI_ERRNO_IDRM = 24,
    WASI_ERRNO_ILSEQ = 25,
    WASI_ERRNO_INPROGRESS = 26,
    WASI_ERRNO_INTR = 27,
    WASI_ERRNO_INVAL = 28,
    WASI_ERRNO_IO = 29,
    WASI_ERRNO_ISCONN = 30,
    WASI_ERRNO_ISDIR = 31,
    WASI_ERRNO_LOOP = 32,
    WASI_ERRNO_MFILE = 33,
    WASI_ERRNO_MLINK = 34,
    WASI_ERRNO_MSGSIZE = 35,
    WASI_ERRNO_MULTIHOP = 36,
    WASI_ERRNO_NAMETOOLONG = 37,
    WASI_ERRNO_NETDOWN = 38,
    WASI_ERRNO_NETRESET = 39,
    WASI_ERRNO_NETUNREACH = 40,
    WASI_ERRNO_NFILE = 41,
    WASI_ERRNO_NOBUFS = 42,
    WASI_ERRNO_NODEV = 43,
    WASI_ERRNO_NOENT = 44,
    WASI_ERRNO_NOEXEC = 45,
    WASI_ERRNO_NOLCK = 46,
    WASI_ERRNO_NOLINK = 47,
    WASI_ERRNO_NOMEM = 48,
    WASI_ERRNO_NOMSG = 49,
    WASI_ERRNO_NOPROTOOPT = 50,
    WASI_ERRNO_NOSPC = 51,
    WASI_ERRNO_NOSYS = 52,
    WASI_ERRNO_NOTCONN = 53,
    WASI_ERRNO_NOTDIR = 54,
    WASI_ERRNO_NOTEMPTY = 55,
    WASI_ERRNO_NOTRECOVERABLE = 56,
    WASI_ERRNO_NOTSOCK = 57,
    WASI_ERRNO_NOTSUP = 58,
    WASI_ERRNO_NOTTY = 59,
    WASI_ERRNO_NXIO = 60,
    WASI_ERRNO_OVERFLOW = 61,
    WASI_ERRNO_OWNERDEAD = 62,
    WASI_ERRNO_PERM = 63,
    WASI_ERRNO_PIPE = 64,
    WASI_ERRNO_PROTO = 65,
    WASI_ERRNO_PROTONOSUPPORT = 66,
    WASI_ERRNO_PROTOTYPE = 67,
    WASI_ERRNO_RANGE = 68,
    WASI_ERRNO_ROFS = 69,
    WASI_ERRNO_SPIPE = 70,
    WASI_ERRNO_SRCH = 71,
    WASI_ERRNO_STALE = 72,
    WASI_ERRNO_TIMEDOUT = 73,
    WASI_ERRNO_TXTBSY = 74,
    WASI_ERRNO_XDEV = 75,
    WASI_ERRNO_NOTCAPABLE = 76,
} wasi_errno_t;

static wasi_errno_t errno_to_wasi(int errno_val) {
    switch (errno) {
        case 0: return WASI_ERRNO_SUCCESS;
        case E2BIG: return WASI_ERRNO_2BIG;
        case EACCES: return WASI_ERRNO_ACCES;
        case EADDRINUSE: return WASI_ERRNO_ADDRINUSE;
        case EADDRNOTAVAIL: return WASI_ERRNO_ADDRNOTAVAIL;
        case EAFNOSUPPORT: return WASI_ERRNO_AFNOSUPPORT;
        case EAGAIN: return WASI_ERRNO_AGAIN;
        case EALREADY: return WASI_ERRNO_ALREADY;
        case EBADF: return WASI_ERRNO_BADF;
        case EBADMSG: return WASI_ERRNO_BADMSG;
        case EBUSY: return WASI_ERRNO_BUSY;
        case ECANCELED: return WASI_ERRNO_CANCELED;
        case ECHILD: return WASI_ERRNO_CHILD;
        case ECONNABORTED: return WASI_ERRNO_CONNABORTED;
        case ECONNREFUSED: return WASI_ERRNO_CONNREFUSED;
        case ECONNRESET: return WASI_ERRNO_CONNRESET;
        case EDEADLK: return WASI_ERRNO_DEADLK;
        case EDESTADDRREQ: return WASI_ERRNO_DESTADDRREQ;
        case EDOM: return WASI_ERRNO_DOM;
        case EDQUOT: return WASI_ERRNO_DQUOT;
        case EEXIST: return WASI_ERRNO_EXIST;
        case EFAULT: return WASI_ERRNO_FAULT;
        case EFBIG: return WASI_ERRNO_FBIG;
        case EHOSTUNREACH: return WASI_ERRNO_HOSTUNREACH;
        case EIDRM: return WASI_ERRNO_IDRM;
        case EILSEQ: return WASI_ERRNO_ILSEQ;
        case EINPROGRESS: return WASI_ERRNO_INPROGRESS;
        case EINTR: return WASI_ERRNO_INTR;
        case EINVAL: return WASI_ERRNO_INVAL;
        case EIO: return WASI_ERRNO_IO;
        case EISCONN: return WASI_ERRNO_ISCONN;
        case EISDIR: return WASI_ERRNO_ISDIR;
        case ELOOP: return WASI_ERRNO_LOOP;
        case EMFILE: return WASI_ERRNO_MFILE;
        case EMLINK: return WASI_ERRNO_MLINK;
        case EMSGSIZE: return WASI_ERRNO_MSGSIZE;
        case EMULTIHOP: return WASI_ERRNO_MULTIHOP;
        case ENAMETOOLONG: return WASI_ERRNO_NAMETOOLONG;
        case ENETDOWN: return WASI_ERRNO_NETDOWN;
        case ENETRESET: return WASI_ERRNO_NETRESET;
        case ENETUNREACH: return WASI_ERRNO_NETUNREACH;
        case ENFILE: return WASI_ERRNO_NFILE;
        case ENOBUFS: return WASI_ERRNO_NOBUFS;
        case ENODEV: return WASI_ERRNO_NODEV;
        case ENOENT: return WASI_ERRNO_NOENT;
        case ENOEXEC: return WASI_ERRNO_NOEXEC;
        case ENOLCK: return WASI_ERRNO_NOLCK;
        case ENOLINK: return WASI_ERRNO_NOLINK;
        case ENOMEM: return WASI_ERRNO_NOMEM;
        case ENOMSG: return WASI_ERRNO_NOMSG;
        case ENOPROTOOPT: return WASI_ERRNO_NOPROTOOPT;
        case ENOSPC: return WASI_ERRNO_NOSPC;
        case ENOSYS: return WASI_ERRNO_NOSYS;
        case ENOTCONN: return WASI_ERRNO_NOTCONN;
        case ENOTDIR: return WASI_ERRNO_NOTDIR;
        case ENOTEMPTY: return WASI_ERRNO_NOTEMPTY;
        case ENOTRECOVERABLE: return WASI_ERRNO_NOTRECOVERABLE;
        case ENOTSOCK: return WASI_ERRNO_NOTSOCK;
        case ENOTSUP: return WASI_ERRNO_NOTSUP;
        case ENOTTY: return WASI_ERRNO_NOTTY;
        case ENXIO: return WASI_ERRNO_NXIO;
        case EOVERFLOW: return WASI_ERRNO_OVERFLOW;
        case EOWNERDEAD: return WASI_ERRNO_OWNERDEAD;
        case EPERM: return WASI_ERRNO_PERM;
        case EPIPE: return WASI_ERRNO_PIPE;
        case EPROTO: return WASI_ERRNO_PROTO;
        case EPROTONOSUPPORT: return WASI_ERRNO_PROTONOSUPPORT;
        case EPROTOTYPE: return WASI_ERRNO_PROTOTYPE;
        case ERANGE: return WASI_ERRNO_RANGE;
        case EROFS: return WASI_ERRNO_ROFS;
        case ESPIPE: return WASI_ERRNO_SPIPE;
        case ESRCH: return WASI_ERRNO_SRCH;
        case ESTALE: return WASI_ERRNO_STALE;
        case ETIMEDOUT: return WASI_ERRNO_TIMEDOUT;
        case ETXTBSY: return WASI_ERRNO_TXTBSY;
        case EXDEV: return WASI_ERRNO_XDEV;
        default: return WASI_ERRNO_INVAL;
    }
}

static void wasi_proc_exit(void* memory_base, void* state_base, wasi_exitcode_t rval) {
    exit(rval);
}

static wasi_errno_t wasi_clock_time_get(void* memory_base, void* state_base, wasi_clockid_t id, wasi_timestamp_t precision, uint32_t _retptr0) {
    wasi_timestamp_t* retptr0 = memory_base + _retptr0;

    clockid_t linux_clockid;
    switch (id) {
        case WASI_CLOCKID_REALTIME: linux_clockid = CLOCK_REALTIME; break;
        case WASI_CLOCKID_MONOTONIC: linux_clockid = CLOCK_MONOTONIC; break;
        case WASI_CLOCKID_PROCESS_CPUTIME_ID: linux_clockid = CLOCK_PROCESS_CPUTIME_ID; break;
        case WASI_CLOCKID_THREAD_CPUTIME_ID: linux_clockid = CLOCK_THREAD_CPUTIME_ID; break;
        default: return WASI_ERRNO_INVAL;
    }

    struct timespec ts = {};
    if (clock_gettime(linux_clockid, &ts) != 0) {
        return errno_to_wasi(errno);
    }

    *retptr0 = ts.tv_sec * 1000000000 + ts.tv_nsec;

    return 0;
}

typedef struct wasi_syscall {
    const char* name;
    void* addr;
} wasi_syscall_t;

static const wasi_syscall_t m_wasip1_syscalls[] = {
    { "proc_exit", wasi_proc_exit },
    { "clock_time_get", wasi_clock_time_get },
};

void* wasip1_resolve_import(const char* name) {
    for (int i = 0; i < ARRAY_LENGTH(m_wasip1_syscalls); i++) {
        if (strcmp(m_wasip1_syscalls[i].name, name) == 0) {
            return m_wasip1_syscalls[i].addr;
        }
    }
    return nullptr;
}
