/*
 *      hpttree - area tree builder for High Portable Tosser (hpt)
 *      by Serguei Revtov 2:5021/11.10 || 2:5021/19.1
 *
 * This file is part of HPT.
 *
 * HPT is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * HPT is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HPT; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************
*/

#include <stdio.h>
#include <ctype.h>

#ifdef UNIX
#include <unistd.h>
#include <strings.h>
#else
#include <io.h>
#endif

#ifdef __EMX__
#include <share.h>
#include <sys/types.h>
#else
#include <fcntl.h>
#endif
#include <sys/stat.h>

#include <smapi/msgapi.h>
#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>

#include <string.h>

#if defined ( __WATCOMC__ )
#include <smapi/prog.h>
#include <share.h>
#endif

#include <stdlib.h>
#include <time.h>
#include <smapi/progprot.h>

s_fidoconfig *cfg;

struct nodepath {
   unsigned int zone, net, node;
   int    exportto;
   int    printed;
};

typedef struct nodepath s_nodepath;

s_nodepath *allNodes = NULL;
int nodeCount = 0;

FILE *outlog;
char *version = "1.03";

int *linksOnLevel = NULL;
int linksInArray = 0;

#ifdef UNIX
int charsPG = 0;
#else
int charsPG = 1;
#endif

#define LLCORNER allCharsPG[charsPG][0]
#define VLINE    allCharsPG[charsPG][1]
#define LTEE     allCharsPG[charsPG][2]

char *allCharsPG[2] = { "L|+", "\300\263\303" };

int  tperiod;

void  printTree (int level, int nodeNum)
{
   int i;
   s_nodepath *cnode;
   int linech;

   cnode = &allNodes [nodeNum];

   if ( linksInArray <= level ) { // Allocate place for link counter in current level
      linksInArray = level + 1;
      linksOnLevel = realloc ( linksOnLevel, sizeof(int) * linksInArray);
   }

   linksOnLevel[level] = 0;
   for (i=0; i < nodeCount; i++) {
      if ((allNodes[i]).exportto == nodeNum) {
	 linksOnLevel[level]++;
      }
   }

   if ( level > 0 ) linksOnLevel[level-1]--; // current node is already printed

   for (i=0; i<level; i++) {
      linech = ' ';
      if ( linksOnLevel[i] ) linech = VLINE;
      if ( i == (level-1) ) {
	 linech = LTEE;
	 if (linksOnLevel[level-1] == 0) linech = LLCORNER;
      }
      printf ( "  %c ", linech);
   }
   printf ("%d:%d/%d\n", cnode->zone, cnode->net, cnode->node);
   if (cnode->printed) {
     printf("WARNING: Loop at %d:%d/%d, escaping thread\n", cnode->zone, cnode->net, cnode->node);
     return;
   }
   cnode->printed = 1; // for checking for lost nodes

   for (i=0; i < nodeCount; i++) {
      if ((allNodes[i]).exportto == nodeNum) {
	 printTree (level+1, i);
      }
   }
}



void buildAreaTree(s_area *area)
{
   HAREA harea;
   dword highMsg;
   HMSG  hmsg;
   XMSG  xmsg;
   int   i = -1;
   int nmsg;
   char *text;
   dword  textLength;
   s_nodepath node;
   s_nodepath *cnode;
   int prevNode;
   char *token;
   char *start;
   char *endptr;
   unsigned long temp;
   int found;
   int done;
   int root = -1;

   /* for time period */
   struct tm tmTime;
   time_t ttime, actualTime = time(NULL);

   fprintf(outlog, "Distribution tree of area %s", area->areaName);

     if (tperiod) {
        fprintf(outlog, " for last %d %s\n\n", tperiod, tperiod ==1 ? "day" : "days");
     }
     else fprintf(outlog, "\n\n");

   if ((area->msgbType & MSGTYPE_PASSTHROUGH) == MSGTYPE_PASSTHROUGH) {
     fprintf(outlog, "PASSTHROUGH, ignoring\n");
     return;
   }

#ifndef __NT__
   if ((area->msgbType & MSGTYPE_JAM) == MSGTYPE_JAM) {
     fprintf(outlog, "JAM, ignoring\n");
     return;
   }
#endif

   if((area->useAka)!=NULL)
      node.zone = (area->useAka)->zone;
   else
      node.zone = 2;

   nodeCount = linksInArray = 0;
   linksOnLevel = NULL;
   allNodes = NULL;

   harea = MsgOpenArea((byte *) area->fileName, MSGAREA_NORMAL, (word)area->msgbType);

   if (harea)
   {
	   highMsg = MsgGetHighMsg(harea);

	   if ( highMsg < 1 ) {
	      fprintf(outlog, "nothing to build (%ld messages)\n", (long)highMsg);
	      MsgCloseArea(harea);
	      return;
	   }

	   /* Pass 1: fill nodes array and buld links */

	   for (nmsg = 1; nmsg <= highMsg; nmsg++) {
	      hmsg  = MsgOpenMsg(harea, MOPEN_READ, nmsg);
	      if (hmsg){
		   /*
		    * NOTE: text would be destructed by strtoc() !!!
		    * Do not use it for anything else
		    */


		   textLength = MsgGetTextLen(hmsg);
		   text = (char *) calloc(textLength+1,sizeof(char));
		   if (text == NULL) return;

                   ttime = mktime(&tmTime);

		   MsgReadMsg(hmsg, &xmsg, 0, textLength, (unsigned char *) text, 0, NULL);

		   /* check time period */
                   if (xmsg.attr & MSGLOCAL) {
                      DosDate_to_TmDate((SCOMBO*)&(xmsg.date_written), &tmTime);
                   } else {
                      DosDate_to_TmDate((SCOMBO*)&(xmsg.date_arrived), &tmTime);
                   }

		   MsgCloseMsg(hmsg);

		   /* check time period */
                   if ( (tperiod) && ( abs(actualTime - ttime) >= ( tperiod * 24 *60 * 60)) )
                     continue;

		   start = text;
		   prevNode = -1;
		   node.exportto = -1;
		   node.printed = 0;
		   done = 0;

		   // find beginning of path lines
		   do {
		      start = strstr(start, "\001PATH: ");
		      if (start == NULL) done++;

		      if (!done) {
			 start += 7; // jump over PATH:
			 while (*start == ' ') start++; // find first word after PATH:
		      }
		   } while (!done && !isdigit( (int) *start));
		   if (!done) {
		      token = strtok(start, " \r\t\376");
		      while (token != NULL) {
			 if (isdigit( (int) *token)) {
			    // parse token
			    temp = strtoul(token, &endptr, 10);

			    if ((*endptr) == ':') { // zone
			       node.zone = temp;
			       endptr++;
			       temp = strtoul(endptr, &endptr, 10);
			    }

			    if ((*endptr) == '/') { // net
			       node.net = temp;
			       endptr++;
			       temp = strtoul(endptr, &endptr, 10);
			    }
			    if (*endptr) fprintf (outlog, "POINT or bad address in PATH: in message %d\n", nmsg);

			    // only node aka
			    node.node = temp;

			    // find if there where that node in array
			    for ( found=0, i=0, cnode=allNodes; i < nodeCount && !found; i++, cnode++) {
			      if ( cnode->node == node.node &&
				   cnode->net  == node.net  &&
				   cnode->zone == node.zone
				 ) found++;
			    }

			    if (!found) {
			       nodeCount++;
			       i = nodeCount;
			       allNodes = (s_nodepath *) realloc (allNodes, sizeof(s_nodepath) * nodeCount);
			       cnode = &allNodes[i-1];
			       *cnode = node;
			    }

			    i--; // was incremented in 'for' & in prev. 'if'

			    if (prevNode >= 0 && prevNode != i) (allNodes[prevNode]).exportto = i;
			    prevNode = i;
			 }
			 token = strtok(NULL, " \r\t\376");
		      }
		      if (root < 0) root = i;
		   }
		   nfree(text);
	      }
	   }


	   MsgCloseArea(harea);


	   // printing tree
	   if (nodeCount > 0)
	      printTree (0, root);
	   else
	      printf("Not distributed\n");


	   for (i=0; i < nodeCount; i++) {
	      if (!((allNodes[i]).printed)) {
		 fprintf(outlog, "Lost Node: %d:%d/%d\n", (allNodes[i]).zone, (allNodes[i]).net, (allNodes[i]).node);
	      }
	   }

	   nfree(allNodes);
	   nfree(linksOnLevel);

	   fprintf(outlog, "\nGenerated by hptTree %s\n", version);
   } else {
	   fprintf(outlog,"\nCould not open area %s\n", area->areaName);
   }
}

void usage(void) {

   fprintf(outlog, "hptTree %s\n", version);
   fprintf(outlog, "Usage:\n hpttree [-p] [-d NUM] [areaname ...]\n");
   fprintf(outlog, "         -p - toggle pseudographics mode\n");
   fprintf(outlog, "         -d NUM - for last NUM days\n");


}

int main(int argc, char **argv) {

   int i, j;
   struct _minf m;
   char **argareas=NULL;
   int nareas=0;
   int found;

   outlog=stdout;

   setbuf(outlog, NULL);

   for (i=1; i<argc; i++) {
     if ( argv[i][0] == '-' ) {
	switch (argv[i][1])
	  {
	     case 'p': /* Toggle pseudographics */
	     case 'P':
		charsPG = (charsPG) ? 0 : 1;
		break;

	     case 'd': /* Last NUM days */
	     case 'D':
                i++;
                if ( !argv[i] ) {
                   usage();
                   exit(-1);
                }
		tperiod = atoi(argv[i]);
                if ( tperiod <=0 ) {
                   usage();
                   exit(-1);
                }
		break;

	     default:
		usage();
		exit(-1);
	  }
     } else {
       // AreaName(s) specified by args
       nareas++;
       argareas = (char **)realloc ( argareas, nareas*sizeof(char *));
       argareas[nareas-1] = argv[i];
     }
   }

   cfg = readConfig(NULL);

   if (!cfg) {
      fprintf(outlog, "Could not read fido config\n");
      return (1);
   }

   m.req_version = 0;
   m.def_zone = (UINT16) cfg->addr[0].zone;
   if (MsgOpenApi(&m)!= 0) {
      fprintf(outlog, "MsgOpenApi Error.\n");
      exit(1);
   }

   if ( argareas )
   {
     // link only specified areas

     for ( j=0; j<nareas; j++) {

	found=0;

	// EchoAreas
	for (i=0; i < cfg->echoAreaCount && !found; i++) {
	   if (stricmp(cfg->echoAreas[i].areaName, argareas[j])==0){
	      buildAreaTree(&(cfg->echoAreas[i]));
	      found++;
	   }
	}

	if (!found) fprintf(outlog, "Couldn't find area \"%s\"\n", argareas[j]);
     }

   } else {

	 // EchoAreas
	 for (i=0; i < cfg->echoAreaCount; i++) buildAreaTree(&(cfg->echoAreas[i]));

   }

   disposeConfig(cfg);

   return (0);
}
