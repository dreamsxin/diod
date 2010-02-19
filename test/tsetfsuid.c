/* tsetfsuid.c - check that pthreads can independently setfsuid */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "diod_log.h"

#include "test.h"

#define TEST_UID 100
#define TEST_GID 100

typedef enum { S0, S1, S2, S3, S4, S5 } state_t;

static state_t         state = S0;
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  state_cond = PTHREAD_COND_INITIALIZER;

static int
check_fsid (char *msg, uid_t uid, gid_t gid)
{
    int fd;
    char path[] = "/tmp/testfsuid.XXXXXX";

    struct stat sb;

    fd = _mkstemp (path);
    _fstat (fd, &sb);
    _unlink (path);

    printf ("%s: %d:%d\n", msg, sb.st_uid, sb.st_gid);

    return (sb.st_uid == uid  && sb.st_gid == gid);
}

static void
change_fsid (char *msg, uid_t uid, gid_t gid)
{
    printf ("%s: changing to %d:%d\n", msg, uid, gid);
    setfsuid (uid);
    setfsgid (gid);
}

static void
change_state (state_t s)
{
    _lock (&state_lock);
    state = s;
    _condsig (&state_cond);
    _unlock (&state_lock);
}

static void
wait_state (state_t s)
{
    _lock (&state_lock);
    while ((state != s))
        _condwait (&state_cond, &state_lock);
    _unlock (&state_lock);
}

static void *proc1 (void *a)
{
    assert (check_fsid ("task1", 0, 0));
    change_state (S1);
    wait_state (S2);
    assert (check_fsid ("task1", 0, 0));
    change_fsid ("task1", TEST_UID, TEST_GID);
    assert (check_fsid ("task1", TEST_UID, TEST_GID));
    change_state (S3);
    wait_state (S4);
    assert (check_fsid ("task1", TEST_UID, TEST_GID));
}

static void *proc2 (void *a)
{
    assert (check_fsid ("task2", 0, 0));
    wait_state (S1);
    change_fsid ("task2", TEST_UID, TEST_GID);
    assert (check_fsid ("task2", TEST_UID, TEST_GID));
    change_state (S2);
    wait_state (S3);
    change_fsid ("task2", 0, 0);
    assert (check_fsid ("task2", 0, 0));
    change_state (S4);
}

int main(int argc, char *argv[])
{
    pthread_t t1, t2;

    diod_log_init (argv[0]);

    assert (geteuid () == 0);

    assert (check_fsid ("task0", 0, 0));

    _create (&t1, proc1, NULL);
    _create (&t2, proc2, NULL);

    _join (t2, NULL);
    _join (t1, NULL);

    assert (check_fsid ("task0", 0, 0));
    
    exit (0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */