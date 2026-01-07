#pragma once

#include <glib.h>
#include <glib/gstdio.h>
#include <config.h>
#include <errno.h>

G_BEGIN_DECLS

#undef MOO_BUILTIN_MOO_GLIB
#define MOO_BUILTIN_MOO_GLIB 1

typedef struct mgw_errno_t mgw_errno_t;
typedef struct MGW_FILE MGW_FILE;
typedef struct MgwFd MgwFd;
typedef struct mgw_access_mode_t mgw_access_mode_t;

typedef int mgw_errno_value_t;

#define MGW_ENOERROR 0
#define MGW_EACCES EACCESS
#define MGW_EPERM EPERM
#define MGW_EEXIST EEXIST
#define MGW_ELOOP ELOOP
#define MGW_ENAMETOOLONG ENAMETOOLONG
#define MGW_ENOENT ENOENT
#define MGW_ENOTDIR ENOTDIR
#define MGW_EROFS EROFS
#define MGW_EXDEV EXDEV

struct mgw_errno_t
{
    mgw_errno_value_t value;
};

struct MgwFd
{
    int value;
};

#define MOO_GLIB_VAR extern

MOO_GLIB_VAR const mgw_errno_t MGW_E_NOERROR;
MOO_GLIB_VAR const mgw_errno_t MGW_E_EXIST;

inline static gboolean mgw_errno_is_set (mgw_errno_t err) { return err.value != MGW_ENOERROR; }
const char *mgw_strerror (mgw_errno_t err);
GFileError mgw_file_error_from_errno (mgw_errno_t err);

guint64 mgw_ascii_strtoull (const gchar *nptr, gchar **endptr, guint base, mgw_errno_t *err);
gdouble mgw_ascii_strtod (const gchar *nptr, gchar **endptr, mgw_errno_t *err);

MGW_FILE *mgw_fopen (const char *filename, const char *mode, mgw_errno_t *err);
int mgw_fclose (MGW_FILE *file);
gsize mgw_fread(void *ptr, gsize size, gsize nmemb, MGW_FILE *stream, mgw_errno_t *err);
gsize mgw_fwrite(const void *ptr, gsize size, gsize nmemb, MGW_FILE *stream);
int mgw_ferror (MGW_FILE *file);
char *mgw_fgets(char *s, int size, MGW_FILE *stream);

MgwFd mgw_open (const char *filename, int flags, int mode);
int mgw_close (MgwFd fd);

int mgw_unlink (const char *path, mgw_errno_t *err);
int mgw_remove (const char *path, mgw_errno_t *err);
int mgw_rename (const char *oldpath, const char *newpath, mgw_errno_t *err);
int mgw_mkdir (const gchar *filename, int mode, mgw_errno_t *err);
int mgw_mkdir_with_parents (const gchar *pathname, gint mode, mgw_errno_t *err);

gboolean
mgw_spawn_async_with_pipes (const gchar *working_directory,
                            gchar **argv,
                            gchar **envp,
                            GSpawnFlags flags,
                            GSpawnChildSetupFunc child_setup,
                            gpointer user_data,
                            GPid *child_pid,
                            MgwFd *standard_input,
                            MgwFd *standard_output,
                            MgwFd *standard_error,
                            GError **error);
GIOChannel *mgw_io_channel_unix_new (MgwFd fd);

enum mgw_access_mode_value_t
{
    MGW_F_OK = 0,
    MGW_R_OK = 1,
    MGW_W_OK = 2,
    MGW_X_OK = 4,
};

struct mgw_access_mode_t
{
    enum mgw_access_mode_value_t value;
};

int mgw_access (const char *path, mgw_access_mode_t mode);

G_END_DECLS
