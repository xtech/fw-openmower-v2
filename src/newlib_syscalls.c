//
// newlib syscall stubs - symbol-provenance override, not a reimplementation.
// Each libnosys.a stub does exactly `errno = ENOSYS; return -1;` (verified by
// disassembly) and so do these; the only difference is that the linker now
// resolves _close & co. here instead of pulling the libnosys members, whose
// .gnu.warning.* sections print "_close is not implemented and will always
// fail" on every link. (No ld flag suppresses those individually, and
// dropping --specs=nosys.specs would also cost _exit and _sbrk.) Nothing
// performs real file I/O on newlib file descriptors, so behavior is unchanged.
// Referenced by the exit-time stdio cleanup (fd vtable) and by abort() ->
// raise() (_kill/_getpid via the C++ terminate path).
//

#include <errno.h>

struct stat;

int _close(int fd) {
  (void)fd;
  errno = ENOSYS;
  return -1;
}

int _fstat(int fd, struct stat* st) {
  (void)fd;
  (void)st;
  errno = ENOSYS;
  return -1;
}

int _getpid(void) {
  errno = ENOSYS;
  return -1;
}

int _isatty(int fd) {
  (void)fd;
  errno = ENOSYS;
  return -1;
}

int _kill(int pid, int sig) {
  (void)pid;
  (void)sig;
  errno = ENOSYS;
  return -1;
}

int _lseek(int fd, int ptr, int dir) {
  (void)fd;
  (void)ptr;
  (void)dir;
  errno = ENOSYS;
  return -1;
}

int _read(int fd, char* buf, int len) {
  (void)fd;
  (void)buf;
  (void)len;
  errno = ENOSYS;
  return -1;
}

int _write(int fd, const char* buf, int len) {
  (void)fd;
  (void)buf;
  (void)len;
  errno = ENOSYS;
  return -1;
}
