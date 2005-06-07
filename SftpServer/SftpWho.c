/*
MySecureShell permit to add restriction to modified sftp-server
when using MySecureShell as shell.
Copyright (C) 2004 Sebastien Tardif

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation (version 2)

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <sys/types.h>
#include <sys/shm.h>
#include "SftpWho.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct		s_shm
{
	t_sftpglobal	global;
	t_sftpwho	who[SFTPWHO_MAXCLIENT];
}			t_shm;

static t_sftpwho	*_sftpwho_ptr = 0;
t_sftpglobal		*_sftpglobal = 0;

t_sftpwho	*SftWhoGetAllStructs()
{
  return	(_sftpwho_ptr);
}
#include <errno.h>
t_sftpwho		*SftpWhoGetStruct(int create)
{
  struct shmid_ds	shst;
  t_sftpwho		*who = 0;
  void			*ptr;
  key_t			key;
  int			shmid, shmkey = 0x0782;
  int			eraze = 0;
  int			i, try;

 try_shm:
  if ((key = ftok("/dev/null", shmkey)) != -1)
    {
      //try to join to existing shm
      if ((shmid = shmget(key, sizeof(t_shm), IPC_EXCL)) == -1)
	{
	  if (create == 1) //check if we are not in sftp-who
	    //doesn't exist so create it
	    shmid = shmget(key, sizeof(t_shm), IPC_CREAT | 0666);
	  eraze = 1;
	}
      if (shmid != -1 && create == 1 && !shmctl(shmid, IPC_STAT, &shst))
	{
	  if (shst.shm_segsz != sizeof(t_shm)) //huho we have a old shm memory
	    {
	      shmkey++;
	      goto try_shm;
	    }
	}
      if (shmid != -1 && (ptr = shmat(shmid, 0, 0)))
	{
		t_shm	*shm = ptr;
		
	  _sftpglobal = &shm->global;
	  who = shm->who;
	  _sftpwho_ptr = who;
	  if (eraze)
	    memset(shm, 0, sizeof(t_shm));
	  else
	    //clean all sessions of bugged client (abnormally quit)
	    SftpWhoCleanBuggedClient();
	  if (create == -1)
	    return (who);
	  //search a empty place :)
	  //try to search 3 times to prevent infinite loop
	  for (try = 0; try < 3; try++)
	      for (i = 0; i < SFTPWHO_MAXCLIENT; i++)
		if (who[i].status == SFTPWHO_EMPTY)
		  {
		    usleep(100);
		    if (who[i].status == SFTPWHO_EMPTY)
		      {
			//clean all old infos
			memset(&who[i], 0, sizeof(*who));
			//marked structure as occuped :)
			who[i].status = SFTPWHO_IDLE;
			return (&who[i]);
		      }
		  }
	}
    }
  if (create == 1)
    who = calloc(1, sizeof(*who));
  return (who);
}

//return number of connected clients
int		SftpWhoCleanBuggedClient()
{
  unsigned int	t;
  int		i, nb, nbdown, nbup;

  t = time(0);
  nb = 0;
  nbdown = 0;
  nbup = 0;
  for (i = 0; i < SFTPWHO_MAXCLIENT; i++)
    if ((_sftpwho_ptr[i].status & SFTPWHO_STATUS_MASK) != SFTPWHO_EMPTY)
      {
	//add 10s to make sure that the session is definitively dead
	if ((_sftpwho_ptr[i].time_begin + _sftpwho_ptr[i].time_total + 10) < t)
	  _sftpwho_ptr[i].status = SFTPWHO_EMPTY;
	else
	  {
	    nb++;
	    if ((_sftpwho_ptr[i].status & SFTPWHO_STATUS_MASK) == SFTPWHO_GET)
	      nbdown++;
	    else if ((_sftpwho_ptr[i].status & SFTPWHO_STATUS_MASK) == SFTPWHO_PUT)
	      nbup++;
	  }
      }
  if (nbdown > 0)
    _sftpglobal->download_by_client = _sftpglobal->download_max / nbdown;
  else
    _sftpglobal->download_by_client = _sftpglobal->download_max;
  if (nbup > 0)
    _sftpglobal->upload_by_client = _sftpglobal->upload_max / nbup;
  else
    _sftpglobal->upload_by_client = _sftpglobal->upload_max;
  return (nb);
}

void	SftpWhoRelaseStruct()
{
  if (_sftpwho_ptr)
    {
      shmdt(_sftpwho_ptr);
      _sftpwho_ptr = 0;
    }
}
