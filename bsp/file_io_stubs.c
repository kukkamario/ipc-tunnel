int _open(const char *filename, int flags, int mode)
{
    (void)filename;
    (void)flags;
    (void)mode;
    return -1;
}

int _read(int fd, char *buffer, int buflen)
{
    (void)fd;
    (void)buffer;
    (void)buflen;
    return -1;
}

int _write(int fd, const char *ptr, int len)
{
    (void)fd;
    (void)ptr;
    (void)len;
    return -1;
}

int _close(int fd)
{
    (void)fd;
    return -1;
}
