/*
  Copyright (C) 2016      Roman Y. Dayneko, <dayneko3000@gmail.com>
                2017-2018 Nikita Yu. Lovyagin, <lovyagin@mail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is based on bbfs fuse-tutorial code
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
*/
#include <config.h>
#include "fgacops.h"
#include <fgacfs.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <sys/time.h>
#include <stdio.h>

#define FGACFS_INIT                                                                      \
    int rc;                                                                              \
    fgac_prc fgacprc, *prc = &fgacprc;                                                   \
    fgac_state *state = ((fgac_state *) fuse_get_context()->private_data);               \
    fgac_path fgacpath = fgac_path_init((char *)rpath), *path = &fgacpath;               \
    char pex [FGAC_LIMIT_PATH];                                                          \
    char *hostpath;                                                                      \
    (void) hostpath;                                                                     \
    (void) rc;                                                                           \
                                                                                         \
    fgacprc.uid = fuse_get_context()->uid;                                               \
    fgacprc.gid = fuse_get_context()->gid;                                               \
    fgacprc.cmd = NULL;                                                                  \
                                                                                         \
    if (state->check_prexec)                                                             \
    {                                                                                    \
        char exe [FGAC_LIMIT_PATH];                                                      \
        ssize_t rs;                                                                      \
        snprintf (exe, FGAC_LIMIT_PATH, "/proc/%u/exe", fuse_get_context()->pid);        \
        if ((rs = readlink(exe, pex, FGAC_LIMIT_PATH)) > 0 && rs < FGAC_LIMIT_PATH -1)   \
        {                                                                                \
            pex[rs] = '\0';                                                              \
            fgacprc.cmd = pex;                                                           \
        }                                                                                \
        FGAC_DEBUG_REQUEST                                                               \
    }                                                                                    \
    fgacprc.ngroups = fuse_getgroups(FGAC_LIMIT_GROUPS, fgacprc.groups);                 \
    if (fgacprc.ngroups < 0 || fgacprc.ngroups > FGAC_LIMIT_GROUPS) fgacprc.ngroups = 0;

#ifndef NDEBUG                                                                     
#define FGAC_DEBUG_REQUEST                                                               \
    printf("REQUEST FROM: uid=%u, gid=%u cmd='%s' exe='%s' rs=%i\n",                     \
                (unsigned) fgacprc.uid,                                                  \
                (unsigned) fgacprc.gid,                                                  \
                fgacprc.cmd,                                                             \
                exe,                                                                     \
                rs                                                                       \
          );                                                                       
#else
#define FGAC_DEBUG_REQUEST
#endif                                                                             


#define FGACFS_INIT_PARENT                                                               \
    char pbuffer[FGAC_LIMIT_PATH];                                                       \
    fgac_path fgacparent = fgac_path_init(pbuffer), *parent = &fgacparent;               \
    FGACFS_INIT                                                                          \
    if (!fgac_parent(path, parent)) parent = NULL;                                       \
    if (!fgac_is_dir(state, parent)) return -ENOTDIR;


#define FGACFS_EXISTS if (!fgac_exists(state, path)) return -ENOENT;
#define FGACFS_PARENT_PRM(prm) if (!fgac_check_prm(state, parent, prc, FGAC_PRM_##prm)) return -EACCES;
#define FGACFS_PRM(prm) if (!fgac_check_prm(state, path, prc, FGAC_PRM_##prm)) return -EACCES;
#define FGACFS_PRM2(prm) if (!fgac_check_prm2(state, path, prc, FGAC_PRM_F##prm, FGAC_PRM_D##prm)) return -EACCES;

#define FGACFS_CALL(call) if ((rc = call) < 0) rc = -errno;

#define FGACFS_RETCALL(call) return (call) < 0 ? -errno : 0;

#define FGACFS_FAILCALL(call) if ((rc = call) < 0) return -errno;

#define FGACFS_HOSTPATH hostpath = FGAC_HOSTPATH; if (!hostpath) return -EFAULT;

int fgacfs_getattr (const char *rpath, struct stat *statbuf)
{
    struct stat *stat;

    FGACFS_INIT
    FGACFS_EXISTS
    FGACFS_PRM2(RA)

    stat = fgac_stat(state, path, prc);

    if (!stat) return -errno;
    *statbuf = *stat;

    return 0;
}

int fgacfs_readlink (const char *rpath, char *buffer, size_t s)
{
    FGACFS_INIT
    FGACFS_EXISTS
    FGACFS_PRM(FSL)
    FGACFS_HOSTPATH

    FGACFS_RETCALL(readlink(hostpath, buffer, s - 1));
}


#define CHECK_OWN(mk,ad)                     \
    if      (pprm & FGAC_PRM_D##mk) own = 1; \
    else if (pprm & FGAC_PRM_D##ad) own = 0; \
    else return -EACCES;

#define ADD(addf,rmf)                            \
    if (own)                                     \
    {                                            \
        uid = prc->uid;                          \
        gid = prc->gid;                          \
    }                                            \
    else                                         \
    {                                            \
        if (fgac_get_owner(state, parent, &uid)) \
        {                                        \
            if (!source) rmf (hostpath);         \
            return -EACCES;                      \
        }                                        \
        if (fgac_get_group(state, parent, &gid)) \
        {                                        \
            if (!source) rmf (hostpath);         \
            return -EACCES;                      \
        }                                        \
    }                                            \
                                                 \
    if (fgac_add (state, path, uid, gid))        \
    {                                            \
printf("1\n");\
        if (!source) rmf (hostpath);             \
        return -EACCES;                          \
    }                                            \
                                                 \
    if (fgac_set_##addf##_prm (state, path))     \
    {                                            \
printf("2\n");\
        if (!source) rmf (hostpath);             \
        fgac_delete (state, path);               \
        return -EACCES;                          \
    }                                            \

#define ADD_FILE ADD(mkfile, unlink)
#define ADD_DIR  ADD(mkdir,  rmdir )

#define ADD_ORD_PRM_SINGLE(type,df)                                   \
    prm->allow = prm->deny = 0;                                       \
    if (S_IR##type & stat->st_mode) prm->allow |= FGAC_PRM_##df##REA; \
    if (S_IW##type & stat->st_mode) prm->allow |= FGAC_PRM_##df##WRI; \
    if (S_IX##type & stat->st_mode) prm->allow |= FGAC_PRM_##df##EXE; \
    fgac_set_prm(state, path, prm);                                   \


#define ADD_ORD_PRM(df)                                  \
    if (!fgac_check_inh(state, parent, FGAC_INH_SPI) &&  \
        !fgac_check_inh(state, parent, FGAC_INH_CPR)     \
       )                                                 \
    {                                                    \
        fgac_prm fgacprm, *prm = &fgacprm;               \
                                                         \
        prm->cat   = FGAC_CAT_UID;                       \
        prm->prc.uid = prc->uid;                         \
        ADD_ORD_PRM_SINGLE(USR,df)                       \
                                                         \
        prm->cat   = FGAC_CAT_GID;                       \
        prm->prc.gid = prc->gid;                         \
        ADD_ORD_PRM_SINGLE(GRP,df)                       \
                                                         \
        prm->cat   = FGAC_CAT_OUS;                       \
        prm->allow = prm->deny = 0;                      \
        ADD_ORD_PRM_SINGLE(USR,df)                       \
                                                         \
        prm->cat   = FGAC_CAT_OGR;                       \
        ADD_ORD_PRM_SINGLE(GRP,df)                       \
                                                         \
        prm->cat   = FGAC_CAT_OTH;                       \
        ADD_ORD_PRM_SINGLE(OTH,df)                       \
    }

#define ADD_ORD_PRM_SL                                   \
    if (!fgac_check_inh(state, parent, FGAC_INH_SPI) &&  \
        !fgac_check_inh(state, parent, FGAC_INH_CPR)     \
       )                                                 \
    {                                                    \
        fgac_prm fgacprm, *prm = &fgacprm;               \
                                                         \
        prm->cat   = FGAC_CAT_UID;                       \
        prm->prc.uid = prc->uid;                         \
        prm->deny = 0;                                   \
        prm->allow = FGAC_PRM_FILE;                      \
                                                         \
        prm->cat   = FGAC_CAT_GID;                       \
        prm->prc.gid = prc->gid;                         \
        prm->deny = 0;                                   \
        prm->allow = FGAC_PRM_FILE;                      \
                                                         \
        prm->cat   = FGAC_CAT_OUS;                       \
        prm->allow = prm->deny = 0;                      \
        prm->deny = 0;                                   \
        prm->allow = FGAC_PRM_FILE;                      \
                                                         \
        prm->cat   = FGAC_CAT_OGR;                       \
        prm->deny = 0;                                   \
        prm->allow = FGAC_PRM_FILE;                      \
                                                         \
        prm->cat   = FGAC_CAT_OTH;                       \
        prm->deny = 0;                                   \
        prm->allow = FGAC_PRM_FILE;                      \
    }

#define CHOWNMOD                                                           \
    if (source)                                                            \
    {                                                                      \
        uint64_t inh, newinh;                                              \
        fgac_prms prms = fgac_get_prms (state, source);                    \
        fgacfs_chown (rpath, uid, gid);                                    \
        if (fgac_prms_is_error(&prms)) return -EFAULT;                     \
        if (fgac_get_inh (state, path,   &newinh)) return -EFAULT;         \
        if (fgac_get_inh (state, source, &inh)) return -EFAULT;            \
                                                                           \
        if (inh != newinh)                                                 \
        {                                                                  \
            int ci = fgac_check_prm2(state, path, prc,                     \
                                   FGAC_PRM_FCI, FGAC_PRM_DCI              \
                                  ),                                       \
                ct = fgac_is_dir(state, path) &&                           \
                     fgac_check_prm(state, path, prc, FGAC_PRM_DCT);       \
            if (!ci)                                                       \
                inh = (inh & ~FGAC_INH_INHS) | (newinh & FGAC_INH_INHS);   \
            if (!ct)                                                       \
                inh = (inh & ~FGAC_INH_TRMS) | (newinh & FGAC_INH_TRMS);   \
                                                                           \
            if (inh != newinh)                                             \
            {                                                              \
                fgac_set_inh(state, path, inh);                            \
                                                                           \
                if (!(inh & FGAC_INH_INH) && (newinh & FGAC_INH_INH))      \
                    fgac_unset_inh (state, path);                          \
                if (!(inh & FGAC_INH_IFP) && (newinh & FGAC_INH_IFP))      \
                    fgac_unset_ifp (state, path);                          \
            }                                                              \
        }                                                                  \
        if (fgac_check_prm2(state, path, prc, FGAC_PRM_FCP, FGAC_PRM_DCP)) \
        {                                                                  \
            size_t i, s = fgac_prms_size(&prms);                           \
            for (i = 0; i < s; ++i)                                        \
            {                                                              \
                fgac_prm prm = *fgac_prms_get(&prms, i);                   \
                fgac_get_prm(state, path, &prm);                           \
                prm.allow |= fgac_prms_get(&prms, i)->allow;               \
                prm.deny  |= fgac_prms_get(&prms, i)->deny;                \
                fgac_set_prm(state, path, &prm);                           \
            }                                                              \
        }                                                                  \
    }

int fgacfs_add(const char  *rpath,
               struct stat *stat,
               fgac_path     *source,
               const char  *target
              )
{
    int own;
    uint64_t pprm;
    uid_t uid;
    gid_t gid;
    mode_t hostmode = stat->st_mode;
    FGACFS_INIT_PARENT
    FGACFS_HOSTPATH
    if (!parent) return -EACCES;
    pprm = fgac_check_prms(state, parent, prc, 1);

    hostmode &= ~(S_IRWXG | S_IRWXO);
    hostmode |= S_IRWXU;

    if (S_ISREG(stat->st_mode))
    {
        int fd;
        CHECK_OWN(MK,AF)

        if (!source)
        {
            fd = open(hostpath, O_CREAT | O_EXCL | O_WRONLY, hostmode);
            if (fd < 0) return -errno;
            close(fd);
        }

        ADD_FILE
        ADD_ORD_PRM(F)
        CHOWNMOD
    }
    else if (S_ISCHR(stat->st_mode))
    {
        CHECK_OWN(CH,AC)
        if (!source) FGACFS_FAILCALL(mknod(hostpath, hostmode, stat->st_dev))
        ADD_FILE
        ADD_ORD_PRM(F)
        CHOWNMOD
    }
    else if (S_ISBLK(stat->st_mode))
    {
        CHECK_OWN(BL,AB)
        if (!source) FGACFS_FAILCALL(mknod(hostpath, hostmode, stat->st_dev))
        ADD_FILE
        ADD_ORD_PRM(F)
        CHOWNMOD
    }
    else if (S_ISFIFO(stat->st_mode))
    {
        CHECK_OWN(FF,AP)
        if (!source) FGACFS_FAILCALL(mkfifo(hostpath, hostmode))
        ADD_FILE
        ADD_ORD_PRM(F)
        CHOWNMOD
    }
    else if (S_ISSOCK(stat->st_mode))
    {
        CHECK_OWN(SC,AO)
        if (!source) FGACFS_FAILCALL(mknod(hostpath, hostmode, stat->st_dev))
        ADD_FILE
        ADD_ORD_PRM(F)
        CHOWNMOD
    }
    else if (S_ISDIR(stat->st_mode))
    {
        CHECK_OWN(MD,AD)
        if (!source) FGACFS_FAILCALL(mkdir(hostpath, hostmode));
        ADD_DIR
        ADD_ORD_PRM(F)
        CHOWNMOD
    }
    else if (S_ISLNK(stat->st_mode))
    {
        CHECK_OWN(SL,AL)
        if (!source) FGACFS_FAILCALL(symlink(target, hostpath))
        ADD_FILE
        ADD_ORD_PRM_SL
        CHOWNMOD
    }
    else return -EACCES;

    return 0;
}

int fgacfs_mknod (const char *rpath, mode_t mode, dev_t dev)
{
    struct stat stat;

    stat.st_mode = mode;
    stat.st_dev  = dev;

    return fgacfs_add(rpath, &stat, NULL, NULL);
}

int fgacfs_mkdir (const char *rpath, mode_t mode)
{
    struct stat stat;

    stat.st_mode = mode | S_IFDIR;

    return fgacfs_add(rpath, &stat, NULL, NULL);
}

int fgacfs_unlink (const char *rpath)
{
    FGACFS_INIT
    FGACFS_EXISTS
    FGACFS_PRM(FRM)
    FGACFS_HOSTPATH

    FGACFS_FAILCALL(unlink(hostpath));
    fgac_delete(state, path);

    return 0;
}

int fgacfs_rmdir (const char *rpath)
{
    FGACFS_INIT
    FGACFS_EXISTS
    FGACFS_PRM(DRM)
    FGACFS_HOSTPATH

    FGACFS_FAILCALL(rmdir(hostpath));
    fgac_delete(state, path);

    return 0;
}

int fgacfs_symlink (const char *target, const char *rpath)
{
    struct stat stat;

    stat.st_mode = S_IFLNK;
    return fgacfs_add(rpath, &stat, NULL, target);
}

int fgacfs_rename (const char *rpath, const char *newrpath)
{
    struct stat *stat;
    fgac_path fgacnewpath = fgac_path_init((char *) newrpath), *newpath = &fgacnewpath;
    char newpbuffer[FGAC_LIMIT_PATH];
    fgac_path fgacnewparent = fgac_path_init(newpbuffer), *newparent = &fgacnewparent;
    char *newhostpath;
    FGACFS_INIT_PARENT
    FGACFS_HOSTPATH
    FGACFS_EXISTS
    if (!fgac_parent(path, newparent)) parent = NULL;
    if (!fgac_is_dir(state, newparent)) return -ENOTDIR;
    newhostpath = fgac_get_hostpath(state, newpath);

    if (!strcmp(fgac_get_hostpath(state,parent), fgac_get_hostpath(state,newparent)))
    {
        FGACFS_PRM2(MV);

        FGACFS_FAILCALL(rename(hostpath, newhostpath))
        if (fgac_rename (state, path, newrpath))
        {
            rename(newhostpath, hostpath);
            return -EACCES;
        }

        return 0;
    }

    stat = fgac_stat(state, path, prc);

    FGACFS_FAILCALL(fgacfs_add(newrpath, stat, path, NULL))

    if (rename(hostpath, newhostpath) < 0)
    {
        fgac_delete(state, newpath);
        return -EACCES;
    }
    if (fgac_rename (state, path, newrpath))
    {
        rename(newhostpath, hostpath);
        fgac_delete(state, newpath);
        return -EACCES;
    }

    fgac_delete(state, path);
    return 0;
}

#define CHMOD_CATCH(t,T)     \
    if (d##t##T)             \
    {                        \
        if (s##t##T)         \
        {                    \
            prm.allow |=  T; \
            prm.deny  &= ~T; \
        }                    \
        else                 \
        {                    \
            prm.allow &= ~T; \
            prm.deny  |=  T; \
        }                    \
    }

int fgacfs_chmod (const char *rpath, mode_t mode)
{
    struct stat *stat;
    fgac_prc prc2;
    fgac_prm prm;
    uint64_t r, w, x;
    int isdir;
    unsigned sur, suw, sux, sgr, sgw, sgx, sor, sow, sox,
             dur, duw, dux, dgr, dgw, dgx, dor, dow, dox;
    FGACFS_INIT
    FGACFS_EXISTS

    prc2 = *prc;
    prc2.cmd = NULL;

    stat = fgac_stat(state, path, &prc2);
    if (!stat) return -EACCES;

    isdir = fgac_is_dir(state, path);

    r = isdir ? FGAC_PRM_DREA : FGAC_PRM_FREA;
    w = isdir ? FGAC_PRM_DWRI : FGAC_PRM_FWRI;
    x = isdir ? FGAC_PRM_DEXE : FGAC_PRM_FEXE;

    sur = mode & S_IRUSR;
    suw = mode & S_IWUSR;
    sux = mode & S_IXUSR;

    sgr = mode & S_IRGRP;
    sgw = mode & S_IWGRP;
    sgx = mode & S_IXGRP;

    sor = mode & S_IROTH;
    sow = mode & S_IWOTH;
    sox = mode & S_IXOTH;


    dur = (stat->st_mode & S_IRUSR) != sur;
    duw = (stat->st_mode & S_IWUSR) != suw;
    dux = (stat->st_mode & S_IXUSR) != sux;

    dgr = (stat->st_mode & S_IRGRP) != sgr;
    dgw = (stat->st_mode & S_IWGRP) != sgw;
    dgx = (stat->st_mode & S_IXGRP) != sgx;

    dor = (stat->st_mode & S_IROTH) != sor;
    dow = (stat->st_mode & S_IWOTH) != sow;
    dox = (stat->st_mode & S_IXOTH) != sox;

    if ((isdir && fgac_check_prm(state, path, prc, FGAC_PRM_DCP)) || (!isdir && fgac_check_prm(state, path, prc, FGAC_PRM_FCP)))
    {
        if (dur || duw || dux)
        {
            prm.cat = FGAC_CAT_OUS;
            if (fgac_get_prm(state, path, &prm)) return -EACCES;
            CHMOD_CATCH(u,r);
            CHMOD_CATCH(u,w);
            CHMOD_CATCH(u,x);
            if (fgac_set_prm(state, path, &prm)) return -EACCES;

            prm.cat = FGAC_CAT_UID;
            prm.prc.uid = stat->st_uid;
            if (fgac_get_prm(state, path, &prm)) return -EACCES;
            CHMOD_CATCH(u,r);
            CHMOD_CATCH(u,w);
            CHMOD_CATCH(u,x);
            if (fgac_set_prm(state, path, &prm)) return -EACCES;
        }

        if (dgr || dgw || dgx)
        {
            prm.cat = FGAC_CAT_OGR;
            if (fgac_get_prm(state, path, &prm)) return -EACCES;
            CHMOD_CATCH(g,r);
            CHMOD_CATCH(g,w);
            CHMOD_CATCH(g,x);
            if (fgac_set_prm(state, path, &prm)) return -EACCES;

            prm.cat = FGAC_CAT_GID;
            prm.prc.gid = stat->st_gid;
            if (fgac_get_prm(state, path, &prm)) return -EACCES;
            CHMOD_CATCH(g,r);
            CHMOD_CATCH(g,w);
            CHMOD_CATCH(g,x);
            if (fgac_set_prm(state, path, &prm)) return -EACCES;
        }

        if (dor || dow || dox)
        {
            prm.cat = FGAC_CAT_OTH;
            if (fgac_get_prm(state, path, &prm)) return -EACCES;
            CHMOD_CATCH(o,r);
            CHMOD_CATCH(o,w);
            CHMOD_CATCH(o,x);
            if (fgac_set_prm(state, path, &prm)) return -EACCES;
        }
    }
    else if (!isdir && fgac_check_prm(state, path, prc, FGAC_PRM_FSX))
    {

        if (dur || duw || dgr || dgw || dor || dow) return -EACCES;

        if (dux)
        {
            prm.cat = FGAC_CAT_OUS;
            if (fgac_get_prm(state, path, &prm)) return -EACCES;
            CHMOD_CATCH(u,x);
            if (fgac_set_prm(state, path, &prm)) return -EACCES;

            prm.cat = FGAC_CAT_UID;
            prm.prc.uid = stat->st_uid;
            if (fgac_get_prm(state, path, &prm)) return -EACCES;
            CHMOD_CATCH(u,x);
            if (fgac_set_prm(state, path, &prm)) return -EACCES;
        }
        if (dgx)
        {
            prm.cat = FGAC_CAT_OGR;
            if (fgac_get_prm(state, path, &prm)) return -EACCES;
            CHMOD_CATCH(g,x);
            if (fgac_set_prm(state, path, &prm)) return -EACCES;

            prm.cat = FGAC_CAT_GID;
            prm.prc.gid = stat->st_gid;
            if (fgac_get_prm(state, path, &prm)) return -EACCES;
            CHMOD_CATCH(u,x);
            if (fgac_set_prm(state, path, &prm)) return -EACCES;
        }
        if (dox)
        {
            prm.cat = FGAC_CAT_OTH;
            if (fgac_get_prm(state, path, &prm)) return -EACCES;
            CHMOD_CATCH(o,x);
            if (fgac_set_prm(state, path, &prm)) return -EACCES;
        }
    }
    else return -EACCES;
    return 0;
}

int fgacfs_chown (const char *rpath, uid_t uid, gid_t gid)
{
    uid_t ouid;
    gid_t ogid;

    FGACFS_INIT_PARENT
    FGACFS_EXISTS

    if (fgac_get_owner (state, path, &ouid)) return -EACCES;
    if (fgac_get_group (state, path, &ogid)) return -EACCES;

    if (ouid != uid)
    {
        if (!fgac_check_prm2(state, path, prc, FGAC_PRM_FCO, FGAC_PRM_DCO))
        {
            uid_t puid;
            if (!parent) return -EACCES;

            if (fgac_get_owner (state, parent, &puid)) return -EACCES;
            if (puid != uid || !fgac_check_prm2(state, path, prc, FGAC_PRM_FOP, FGAC_PRM_DOP)) return -EACCES;
        }
    }

    if (ogid != gid)
    {
        if (!fgac_check_prm2(state, path, prc, FGAC_PRM_FCG, FGAC_PRM_DCG))
        {
            gid_t pgid;
            if (!parent) return -EACCES;

            if (fgac_get_group (state, parent, &pgid)) return -EACCES;
            if (pgid != gid || !fgac_check_prm2(state, path, prc, FGAC_PRM_FOP, FGAC_PRM_DOP)) return -EACCES;
        }
    }

    if (ouid != uid) fgac_set_owner(state, path, uid);
    if (ogid != gid) fgac_set_group(state, path, gid);

    return 0;
}

int fgacfs_truncate (const char *rpath, off_t newsize)
{
    FGACFS_INIT
    FGACFS_EXISTS
    FGACFS_PRM(FWR)
    FGACFS_HOSTPATH

    FGACFS_RETCALL(truncate(hostpath, newsize))
}

int fgacfs_utime (const char *rpath, struct utimbuf *ubuf)
{
    FGACFS_INIT
    FGACFS_EXISTS
    FGACFS_PRM2(CA)
    FGACFS_HOSTPATH

    FGACFS_RETCALL(utime(hostpath, ubuf))
}

int fgacfs_open (const char *rpath, struct fuse_file_info *fi)
{
    FGACFS_INIT
    FGACFS_EXISTS

    if (fi->flags & O_RDONLY || fi->flags & O_RDWR) FGACFS_PRM(FRD);
    if (fi->flags & O_WRONLY || fi->flags & O_RDWR) FGACFS_PRM(FWR);

    FGACFS_HOSTPATH

    FGACFS_FAILCALL(open(hostpath, fi->flags))
    
    fi->fh        = rc;
    fi->direct_io = 1;    
    return 0;
}

int fgacfs_read (const char *rpath, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void) rpath;
    return pread(fi->fh, buf, size, offset);
}

int fgacfs_write (const char *rpath, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void) rpath;
    return pwrite(fi->fh, buf, size, offset);
}

int fgacfs_statfs (const char *rpath, struct statvfs *statv)
{
    FGACFS_INIT
    FGACFS_HOSTPATH
    (void) prc;
    FGACFS_RETCALL (statvfs(hostpath, statv));
}

int fgacfs_flush (const char *rpath, struct fuse_file_info *fi)
{
    (void) rpath;
    (void) fi;
    return 0;
}

int fgacfs_release (const char *rpath, struct fuse_file_info *fi)
{
    (void) rpath;
    FGACFS_RETCALL(close(fi->fh));
}

int fgacfs_fsync (const char *rpath, int datasync, struct fuse_file_info *fi)
{
    (void) rpath;
    (void) datasync;
#ifdef HAVE_FDATASYNC
    if (datasync)
        FGACFS_RETCALL(fdatasync(fi->fh))
    else
#endif
	FGACFS_RETCALL(fsync(fi->fh))

}

int fgacfs_setxattr (const char *rpath, const char *name, const char *value, size_t size, int flags)
{
    FGACFS_INIT
    FGACFS_EXISTS

    if (!memcmp(name, "user.fgacfs.", 12)) return -EACCES;

    FGACFS_PRM2(MX);
    FGACFS_HOSTPATH

    FGACFS_RETCALL(lsetxattr(hostpath, name, value, size, flags))
}

int fgacfs_getxattr (const char *rpath, const char *name, char *value, size_t size)
{
    FGACFS_INIT
    FGACFS_EXISTS

    if (!memcmp(name, "user.fgacfs.", 12)) return -EACCES;

    FGACFS_PRM2(XA);
    FGACFS_HOSTPATH

    FGACFS_RETCALL(lgetxattr(hostpath, name, value, size))
}

int fgacfs_listxattr (const char *rpath, char *list, size_t size)
{
    ssize_t s = size, r = 0;
    char *list2, *i, *e;
    size_t length;

    FGACFS_INIT
    FGACFS_EXISTS

    FGACFS_PRM2(XA);
    FGACFS_HOSTPATH

    if (size == 0)
        if ((s = llistxattr(hostpath, list, 0)) < 0) return -errno;

    list2 = malloc(s);
    if (!list2) return -ENOMEM;

    s = llistxattr(hostpath, list2, s);
    if (s < 0) {free(list2); return -errno; }

    for (i = list2, e = list2 + s; i < e; i += length)
    {
        length = strlen (i) + 1;
        if (length < 10 || memcmp(i, "user.fgacfs.", 12))
        {
            r += length;
            if (size)
            {
                if (r >= (ssize_t) size) {free (list2); return -ERANGE; }
                memcpy(i, list, length);
                list += length;
            }
        }
    }

    free(list2); return r;
}

int fgacfs_removexattr (const char *rpath, const char *name)
{
    FGACFS_INIT
    FGACFS_EXISTS

    if (!memcmp(name, "user.fgacfs.", 12)) return -EACCES;

    FGACFS_PRM2(MX);
    FGACFS_HOSTPATH

    FGACFS_RETCALL(lremovexattr(hostpath, name))
}

int fgacfs_opendir (const char *rpath, struct fuse_file_info *fi)
{
    FGACFS_INIT
    FGACFS_EXISTS
    FGACFS_PRM(DRD)
    FGACFS_HOSTPATH
    rc = (intptr_t) opendir(hostpath);
    if (!rc) return -errno;
    fi->fh = rc;
    return 0;
}

int fgacfs_readdir (const char *rpath, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    (void)rpath;
    (void)offset;

    DIR *dp = (DIR *) (uintptr_t) fi->fh;
    struct dirent *de;

    errno = 0;
    while ((de = readdir(dp)))
        if (filler(buf, de->d_name, NULL, 0)) return -ENOMEM;

    return -errno;
}

int fgacfs_releasedir (const char *rpath, struct fuse_file_info *fi)
{
    (void)rpath;
    closedir((DIR *) (uintptr_t) fi->fh);
    return 0;
}

/*
int fgacfs_access (const char *rpath, int mode)
{
    int r = 1;

    FGACFS_INIT
    FGACFS_HOSTPATH

    if (mode & F_OK) r = access(hostpath, );

    if (mode != F_OK)
    {
        struct stat statbuf;

        int R, W, X;

        if (fgac_stat(state, path, prc, &statbuf) < 0) return -errno;

        if      (statbuf.st_uid == prc->uid)
        {
            R = S_IRUSR;
            W = S_IWUSR;
            X = S_IXUSR;
        }
        else if (statbuf.st_gid == prc->gid)
        {
            R = S_IRGRP;
            W = S_IWGRP;
            X = S_IXGRP;
        }
        else
        {
            R = S_IROTH;
            W = S_IWOTH;
            X = S_IXOTH;
        }

        if (mode & R_OK) r = r && statbuf.st_mode & R;
        if (mode & W_OK) r = r && statbuf.st_mode & W;
        if (mode & X_OK) r = r && statbuf.st_mode & X;

    }

    return r ? 0 : -1;
}
*/


int fgacfs_ftruncate (const char *rpath, off_t offset, struct fuse_file_info *fi)
{
    (void) rpath;
    FGACFS_RETCALL(ftruncate(fi->fh, offset));
}

int fgacfs_fgetattr (const char *rpath, struct stat *statbuf, struct fuse_file_info *fi)
{
    (void) fi;
    return fgacfs_getattr(rpath, statbuf);
}
