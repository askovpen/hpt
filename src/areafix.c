/*****************************************************************************
 * AreaFix for HPT (FTN NetMail/EchoMail Tosser)
 *****************************************************************************
 * Copyright (C) 1998-2002
 *
 * Max Levenkov
 *
 * Fido:     2:5000/117
 * Internet: sackett@mail.ru
 * Novosibirsk, West Siberia, Russia
 *
 * Big thanks to:
 *
 * Fedor Lizunkov
 *
 * Fido:     2:5020/960
 * Moscow, Russia
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
 * $Id$
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>


#include <huskylib/compiler.h>
#include <huskylib/huskylib.h>

#ifdef HAS_IO_H
#include <io.h>
#endif

#ifdef HAS_UNISTD_H
#include <unistd.h>
#endif

#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#include <huskylib/xstr.h>
#include <fidoconf/areatree.h>
#include <fidoconf/afixcmd.h>
#include <fidoconf/arealist.h>
#include <huskylib/recode.h>

#include <fcommon.h>
#include <global.h>
#include <pkt.h>
#include <version.h>
#include <toss.h>
#include <ctype.h>
#include <seenby.h>
#include <scan.h>
#include <areafix.h>
#include <scanarea.h>
#include <hpt.h>
#include <dupe.h>
#include <query.h>

#ifdef DO_PERL
#include <hptperl.h>
#endif

unsigned char RetFix;
static int rescanMode = 0;
static int rulesCount = 0;
static char **rulesList = NULL;

char *print_ch(int len, char ch)
{
    static char tmp[256];

    if (len <= 0 || len > 255) return "";

    memset(tmp, ch, len);
    tmp[len]=0;
    return tmp;
}

char *getPatternFromLine(char *line, int *reversed)
{

    *reversed = 0;
    if (!line) return NULL;
    /* original string is like "%list ! *.citycat.*" or withut '!' sign*/
    if (line[0] == '%') line++; /* exclude '%' sign */
    while((strlen(line) > 0) && isspace(line[0])) line++; /* exclude spaces between '%' and command */
    while((strlen(line) > 0) && !isspace(line[0])) line++; /* exclude command */
    while((strlen(line) > 0) && isspace(line[0])) line++; /* exclude spaces between command and pattern */

    if ((strlen(line) > 2) && (line[0] == '!') && (isspace(line[1])))
    {
        *reversed = 1;
        line++;     /* exclude '!' sign */
        while(isspace(line[0])) line++; /* exclude spaces between '!' and pattern */
    }

    if (strlen(line) > 0)
        return line;
    else
        return NULL;
}

char *list(s_listype type, s_link *link, char *cmdline) {
    unsigned int i, j, export, import, mandatory, active, avail, rc = 0;
    char *report = NULL;
    char *list = NULL;
    char *pattern = NULL;
    int reversed;
    ps_arealist al;
    ps_area area;
    int grps = (config->listEcho == lemGroup) || (config->listEcho == lemGroupName);

    if (cmdline) pattern = getPatternFromLine(cmdline, &reversed);
    if ((pattern) && (strlen(pattern)>60 || !isValidConference(pattern))) {
        w_log(LL_FUNC, "areafix::list() FAILED (error request line)");
        return errorRQ(cmdline);
    }

    switch (type) {
      case lt_all:
        xscatprintf(&report, "Available areas for %s\r\r", aka2str(link->hisAka));
        break;
      case lt_linked:
        xscatprintf(&report, "%s areas for %s\r\r",
                    ((link->Pause & ECHOAREA) == ECHOAREA) ? "Passive" : "Active", aka2str(link->hisAka));
        break;
      case lt_unlinked:
        xscatprintf(&report, "Unlinked areas for %s\r\r", aka2str(link->hisAka));
        break;
    }

    al = newAreaList(config);
    for (i=active=avail=0; i< config->echoAreaCount; i++) {
		
	area = &(config->echoAreas[i]);
	rc = subscribeCheck(area, link);

        if ( (type == lt_all && rc < 2)
             || (type == lt_linked && rc == 0)
             || (type == lt_unlinked && rc == 1)
           ) { /*  add line */

            import = export = 1; mandatory = 0;
            for (j = 0; j < area->downlinkCount; j++) {
               if (addrComp(link->hisAka, area->downlinks[j]->link->hisAka) == 0) {
                  import = area->downlinks[j]->import;
                  export = area->downlinks[j]->export;
                  mandatory = area->downlinks[j]->mandatory;
               }
            }

            if (pattern)
            {
                /* if matches pattern and not reversed (or vise versa) */
                if (patimat(area->areaName, pattern)!=reversed)
                {
                    addAreaListItem(al,rc==0, area->msgbType!=MSGTYPE_PASSTHROUGH, import, export, mandatory, area->areaName,area->description,area->group);
                    if (rc==0) active++; avail++;
                }
            } else
            {
                addAreaListItem(al,rc==0, area->msgbType!=MSGTYPE_PASSTHROUGH, import, export, mandatory, area->areaName,area->description,area->group);
                if (rc==0) active++; avail++;
            }
	} /* end add line */

    } /* end for */
#ifdef DO_PERL
    if ( !perl_echolist(&report, type, al, aka2str(link->hisAka)) ) {
#endif
    sortAreaList(al);
    switch (type) {
      case lt_linked:
      case lt_all:      list = formatAreaList(al,78," *SRW M", grps); break;
      case lt_unlinked: list = formatAreaList(al,78,"  S    ", grps); break;
    }
    if (list) xstrcat(&report,list);
    nfree(list);
    freeAreaList(al);

    if (type != lt_unlinked) 
        xstrcat(&report, "\r'*' = area is active");
        xstrcat(&report, "\r'R' = area is readonly for you");
        xstrcat(&report, "\r'W' = area is writeonly for you");
        xstrcat(&report, "\r'M' = area is mandatory for you");
    xstrcat(&report, "\r'S' = area is rescanable");

    if (type == lt_linked) {
    }
    switch (type) {
      case lt_all:
        xscatprintf(&report, "\r\r %i area(s) available, %i area(s) active\r", avail, active);
        break;
      case lt_linked:
        xscatprintf(&report, "\r\r %i area(s) linked\r", active);
        break;
      case lt_unlinked:
        xscatprintf(&report, "\r\r %i area(s) available\r", avail);
        break;
    }
/*    xscatprintf(&report,  "\r for link %s\r", aka2str(link->hisAka));*/

    if (link->afixEchoLimit) xscatprintf(&report, "\rYour limit is %u areas for subscribe\r", link->afixEchoLimit);
#ifdef DO_PERL
    }
#endif
    switch (type) {
      case lt_all:
        w_log(LL_AREAFIX, "areafix: list sent to %s", aka2str(link->hisAka));
        break;
      case lt_linked:
        w_log(LL_AREAFIX, "areafix: linked areas list sent to %s", aka2str(link->hisAka));
        break;
      case lt_unlinked:
        w_log(LL_AREAFIX, "areafix: unlinked areas list sent to %s", aka2str(link->hisAka));
        break;
    }

    return report;
}

char *help(s_link *link) {
    FILE *f;
    int i=1;
    char *help;
    long endpos;

    if (config->areafixhelp!=NULL) {
	if ((f=fopen(config->areafixhelp,"r")) == NULL) {
	    w_log (LL_ERR, "areafix: cannot open help file \"%s\": %s",
	           config->areafixhelp, strerror(errno));
	    if (!quiet)
		fprintf(stderr,"areafix: cannot open help file \"%s\": %s\n",
		        config->areafixhelp, strerror(errno));
	    return NULL;
	}
		
	fseek(f,0L,SEEK_END);
	endpos=ftell(f);

	help=(char*) safe_malloc((size_t) endpos+1);

	fseek(f,0L,SEEK_SET);
	endpos = fread(help,1,(size_t) endpos,f);

	for (i=0; i<endpos; i++) if (help[i]=='\n') help[i]='\r';
	help[endpos]='\0';

	fclose(f);

	w_log(LL_AREAFIX, "areafix: help sent to %s",link->name);

	return help;
    }
    return NULL;
}

int tag_mask(char *tag, char **mask, unsigned num) {
    unsigned int i;

    for (i = 0; i < num; i++) {
	if (patimat(tag,mask[i])) return 1;
    }

    return 0;
}

/* Process %avail command.
 *
 */
char *available(s_link *link, char *cmdline)
{
    FILE *f;
    unsigned int j=0, found;
    unsigned int k, rc;
    char *report = NULL, *line, *token, *running, linkAka[SIZE_aka2str];
    char *pattern;
    int reversed;
    s_link *uplink=NULL;
    ps_arealist al=NULL, *hal=NULL;
    unsigned int halcnt=0, isuplink;

    pattern = getPatternFromLine(cmdline, &reversed);
    if ((pattern) && (strlen(pattern)>60 || !isValidConference(pattern))) {
        w_log(LL_FUNC, "areafix::avail() FAILED (error request line)");
        return errorRQ(cmdline);
    }

    for (j = 0; j < config->linkCount; j++)
    {
	uplink = config->links[j];

	found = 0;
	isuplink = 0;
	for (k = 0; k < link->numAccessGrp && uplink->LinkGrp; k++)
	    if (strcmp(link->AccessGrp[k], uplink->LinkGrp) == 0)
		found = 1;

	if ((uplink->forwardRequests && uplink->forwardRequestFile) &&
	    ((uplink->LinkGrp == NULL) || (found != 0)))
	{
	    if ((f=fopen(uplink->forwardRequestFile,"r")) == NULL) {
		w_log(LL_ERR, "areafix: cannot open forwardRequestFile \"%s\": %s",
		      uplink->forwardRequestFile, strerror(errno));
 		continue;
	    }

	    isuplink = 1;

            if ((!hal)&&(link->availlist == AVAILLIST_UNIQUEONE))
                xscatprintf(&report, "Available Area List from all uplinks:\r");

            if ((!halcnt)||(link->availlist != AVAILLIST_UNIQUEONE))
            {
              halcnt++;
              hal = realloc(hal, sizeof(ps_arealist)*halcnt);
              hal[halcnt-1] = newAreaList(config);
              al = hal[halcnt-1];
              w_log(LL_DEBUGW,  __FILE__ ":%u: New item added to hal, halcnt = %u", __LINE__, halcnt);
            }

            while ((line = readLine(f)) != NULL)
            {
                line = trimLine(line);
                if (line[0] != '\0')
                {
                    running = line;
                    token = strseparate(&running, " \t\r\n");
                    rc = 0;

                    if (uplink->numDfMask)
                      rc |= tag_mask(token, uplink->dfMask, uplink->numDfMask);

                    if (uplink->denyFwdFile)
                      rc |= IsAreaAvailable(token,uplink->denyFwdFile,NULL,0);

                    if (pattern)
                    {
                        /* if matches pattern and not reversed (or vise versa) */
                        if ((rc==0) &&(patimat(token, pattern)!=reversed))
                            addAreaListItem(al,0,0,1,1,0,token,running,uplink->LinkGrp);
                    } else
                    {
                        if (rc==0) addAreaListItem(al,0,0,1,1,0,token,running,uplink->LinkGrp);
                    }

    	        }
    	        nfree(line);
            }
            fclose(f);


            /*  warning! do not ever use aka2str twice at once! */
            sprintf(linkAka, "%s", aka2str(link->hisAka));
            w_log( LL_AREAFIX, "areafix: Available Area List from %s %s to %s",
                  aka2str(uplink->hisAka),
                  (link->availlist == AVAILLIST_UNIQUEONE) ? "prepared": "sent",
                  linkAka );
        }


 	if ((link->availlist != AVAILLIST_UNIQUEONE)||(j==(config->linkCount-1)))
 	{
 		if((hal)&&((hal[halcnt-1])->count))
 		    if ((link->availlist != AVAILLIST_UNIQUE)||(isuplink))
 		    {
 		        sortAreaListNoDupes(halcnt, hal, link->availlist != AVAILLIST_FULL);
 		        if ((hal[halcnt-1])->count)
 		        {
 			    line = formatAreaList(hal[halcnt-1],78,NULL,(config->listEcho==lemGroup)||(config->listEcho==lemGroupName));
 			    if (link->availlist != AVAILLIST_UNIQUEONE)
 			        xscatprintf(&report, "\rAvailable Area List from %s:\r", aka2str(uplink->hisAka));
			    if (line)
 			        xstrscat(&report, "\r", line,print_ch(77,'-'), "\r", NULL);
 			    nfree(line);
	 	        }
 		    }

            if ((link->availlist != AVAILLIST_UNIQUE)||(j==(config->linkCount-1)))
              if (hal)
              {
  	        w_log(LL_DEBUGW,  __FILE__ ":%u: hal freed, (%u items)", __LINE__, halcnt);
                for(;halcnt>0;halcnt--)
                  freeAreaList(hal[halcnt-1]);
                nfree(hal);
              }
 	}
    }	

    if (report==NULL) {
	xstrcat(&report, "\r  no links for creating Available Area List\r");
	w_log(LL_AREAFIX, "areafix: no links for creating Available Area List");
    }
    return report;
}


/*  subscribe if (act==0),  unsubscribe if (act==1), delete if (act==2) */
int forwardRequestToLink (char *areatag, s_link *uplink, s_link *dwlink, int act) {
    s_message *msg;
    char *base, pass[]="passthrough";

    if (!uplink) return -1;

    if (uplink->msg == NULL) {
	msg = makeMessage(uplink->ourAka, &(uplink->hisAka), config->sysop,
        uplink->RemoteRobotName ? uplink->RemoteRobotName : "areafix",
        uplink->areaFixPwd ? uplink->areaFixPwd : "\x00", 1,
        uplink->areafixReportsAttr ? uplink->areafixReportsAttr : config->areafixReportsAttr);
	msg->text = createKludges(config, NULL, uplink->ourAka, &(uplink->hisAka),
                              versionStr);
        if (uplink->areafixReportsFlags)
            xstrscat(&(msg->text), "\001FLAGS ", uplink->areafixReportsFlags, "\r",NULL);
        else if (config->areafixReportsFlags)
	    xstrscat(&(msg->text), "\001FLAGS ", config->areafixReportsFlags, "\r",NULL);
	uplink->msg = msg;
    } else msg = uplink->msg;
	
    if (act==0) {
    if (getArea(config, areatag) == &(config->badArea)) {
        if(config->areafixQueueFile) {
            af_CheckAreaInQuery(areatag, &(uplink->hisAka), &(dwlink->hisAka), ADDFREQ);
        }
        else {
            base = uplink->msgBaseDir;
            if (config->createFwdNonPass==0) uplink->msgBaseDir = pass;
            /*  create from own address */
            if (isOurAka(config,dwlink->hisAka)) {
                uplink->msgBaseDir = base;
            }
            strUpper(areatag);
            autoCreate(areatag, uplink->hisAka, &(dwlink->hisAka));
            uplink->msgBaseDir = base;
        }
    }
    xstrscat(&msg->text, "+", areatag, "\r", NULL);
    } else if (act==1) {
        xscatprintf(&(msg->text), "-%s\r", areatag);
    } else {
        /*  delete area */
        if (uplink->advancedAreafix)
            xscatprintf(&(msg->text), "~%s\r", areatag);
        else
            xscatprintf(&(msg->text), "-%s\r", areatag);
    }
    return 0;
}

char* GetWordByPos(char* str, UINT pos)
{
    UINT i = 0;
    while( i < pos)
    {
        while( *str &&  isspace(*str)) str++; /* skip spaces */
        i++;
        if( i == pos ) break;
        while( *str && !isspace(*str)) str++; /* skip word   */
        if( *str == '\0' ) return NULL;
    }
    return str; 
}

int changeconfig(char *fileName, s_area *area, s_link *link, int action) {
    char *cfgline=NULL, *token=NULL, *tmpPtr=NULL, *line=NULL, *buff=NULL;
    char *strbegfileName = fileName;
    long strbeg = 0, strend = -1;
    int rc=0;

    e_changeConfigRet nRet = I_ERR;
    char *areaName = area->areaName;

    w_log(LL_FUNC, __FILE__ "::changeconfig(%s,...)", fileName);

    if (init_conf(fileName))
		return -1;

    w_log(LL_SRCLINE, __FILE__ ":%u:changeconfig() action=%i",__LINE__,action);

    while ((cfgline = configline()) != NULL) {
        line = sstrdup(cfgline);
        line = trimLine(line);
        line = stripComment(line);
        if (line[0] != 0) {
            line = shell_expand(line);
            line = tmpPtr = vars_expand(line);
            token = strseparate(&tmpPtr, " \t");
            if (stricmp(token, "echoarea")==0) {
                token = strseparate(&tmpPtr, " \t");
                if (*token=='\"' && token[strlen(token)-1]=='\"' && token[1]) {
                    token++;
                    token[strlen(token)-1]='\0';
                }
                if (stricmp(token, areaName)==0) {
                    fileName = safe_strdup(getCurConfName());
                    strend = get_hcfgPos();
                    if (strcmp(strbegfileName, fileName) != 0) strbeg = 0;
                    break;
                }
            }
        }
        strbeg = get_hcfgPos();
        strbegfileName = safe_strdup(getCurConfName());
        w_log(LL_DEBUGF, __FILE__ ":%u:changeconfig() strbeg=%ld", __LINE__, strbeg);
        nfree(line);
        nfree(cfgline);
    }
    close_conf();
    nfree(line);
    if (strend == -1) { /*  impossible   error occurred */
        nfree(cfgline);
        nfree(fileName);
        return -1;
    }

    switch (action) {
    case 0: /*  forward Request To Link */
        if ((area->msgbType==MSGTYPE_PASSTHROUGH) &&
            (!config->areafixQueueFile) &&
            (area->downlinkCount==1) &&
            (area->downlinks[0]->link->hisAka.point == 0))
        {
            forwardRequestToLink(areaName, area->downlinks[0]->link, NULL, 0);
        }
    case 3: /*  add link to existing area */
        xscatprintf(&cfgline, " %s", aka2str(link->hisAka));
        nRet = ADD_OK;
        break;
    case 1: /*  remove link from area */
        if ((area->msgbType==MSGTYPE_PASSTHROUGH)
            && (area->downlinkCount==1) &&
            (area->downlinks[0]->link->hisAka.point == 0)) {
            forwardRequestToLink(areaName, area->downlinks[0]->link, NULL, 1);
        }
    case 7:
        if ((rc = DelLinkFromString(cfgline, link->hisAka)) == 1) {
            w_log(LL_ERR,"areafix: Unlink is not possible for %s from echo area %s",
                aka2str(link->hisAka), areaName);
            nRet = O_ERR;
        } else {
            nRet = DEL_OK;
        }
        break;
    case 2:
        /* makepass(f, fileName, areaName); */
    case 4: /*  delete area */
        nfree(cfgline);
        nRet = DEL_OK;
        break;
    case 5: /*  subscribe us to  passthrough */
        w_log(LL_SRCLINE, __FILE__ "::changeconfig():%u",__LINE__);

        tmpPtr = fc_stristr(cfgline,"passthrough");
        if ( tmpPtr )  {
            char* msgbFileName = makeMsgbFileName(config, area->areaName);
            /*  translating filename of the area to lowercase/uppercase */
            if (config->areasFileNameCase == eUpper) strUpper(msgbFileName);
            else strLower(msgbFileName);

            *(--tmpPtr) = '\0';
            xstrscat(&buff, cfgline, " ", config->msgBaseDir,msgbFileName, tmpPtr+12, NULL);
            nfree(cfgline);
            cfgline = buff;

            nfree(msgbFileName);
        } else  {
            tmpPtr = fc_stristr(cfgline,"-pass");
            *(--tmpPtr) = '\0';
            xstrscat(&buff, cfgline, tmpPtr+6 , NULL);
            nfree(cfgline);
            cfgline = buff;
        }
        nRet = ADD_OK;
        break;
    case 6: /*  make area pass. */
        tmpPtr = GetWordByPos(cfgline,4);
        if ( tmpPtr ) {
            *(--tmpPtr) = '\0';
            xstrscat(&buff, cfgline, " -pass ", ++tmpPtr , NULL);
            nfree(cfgline);
            cfgline = buff;
        } else {
            xstrcat(&cfgline, " -pass");
        }
        nRet = DEL_OK;
        break;
    case 8: /*  make area paused. */
        tmpPtr = GetWordByPos(cfgline,4);
        if ( tmpPtr ) {
            *(--tmpPtr) = '\0';
            xstrscat(&buff, cfgline, " -paused ", ++tmpPtr , NULL);
            nfree(cfgline);
            cfgline = buff;
        } else {
            xstrcat(&cfgline, " -paused");
        }
        nRet = ADD_OK;
        break;
    case 9: /*  make area not paused */
        tmpPtr = fc_stristr(cfgline,"-paused");
        *(--tmpPtr) = '\0';
        xstrscat(&buff, cfgline, tmpPtr+8 , NULL);
        nfree(cfgline);
        cfgline = buff;
        nRet = ADD_OK;
        break;
    default: break;
    } /*  switch (action) */

    w_log(LL_DEBUGF, __FILE__ ":%u:changeconfig() call InsertCfgLine(\"%s\",<cfgline>,%ld,%ld)", __LINE__, fileName, strbeg, strend);
    InsertCfgLine(fileName, cfgline, strbeg, strend);
    nfree(cfgline);
    nfree(fileName);
#ifdef DO_PERL
    /* val: update perl structures */
    perl_setvars();
#endif
    w_log(LL_FUNC, __FILE__ "::changeconfig() rc=%i", nRet);
    return nRet;
}

static int compare_links_priority(const void *a, const void *b) {
    int ia = *((int*)a);
    int ib = *((int*)b);
    if(config->links[ia]->forwardAreaPriority < config->links[ib]->forwardAreaPriority) return -1;
    else if(config->links[ia]->forwardAreaPriority > config->links[ib]->forwardAreaPriority) return 1;
    else return 0;
}

int forwardRequest(char *areatag, s_link *dwlink, s_link **lastRlink) {
    unsigned int i=0, rc = 1;
    s_link *uplink=NULL;
    int *Indexes=NULL;
    unsigned int Requestable = 0;

    /* From Lev Serebryakov -- sort Links by priority */
    Indexes = safe_malloc(sizeof(int)*config->linkCount);
    for (i = 0; i < config->linkCount; i++) {
	if (config->links[i]->forwardRequests) Indexes[Requestable++] = i;
    }
    qsort(Indexes,Requestable,sizeof(Indexes[0]),compare_links_priority);
    i = 0;
    if(lastRlink) { /*  try to find next requestable uplink */
        for (; i < Requestable; i++) {
            uplink = config->links[Indexes[i]];
            if( addrComp(uplink->hisAka, (*lastRlink)->hisAka) == 0)
            {   /*  we found lastRequestedlink */
                i++;   /*  let's try next link */
                break;
            }
        }
    }

    for (; i < Requestable; i++) {
        uplink = config->links[Indexes[i]];

        if(lastRlink) *lastRlink = uplink;

        if (uplink->forwardRequests && (uplink->LinkGrp) ?
            grpInArray(uplink->LinkGrp,dwlink->AccessGrp,dwlink->numAccessGrp) : 1)
        {
            /* skip downlink from list of uplinks */
            if(addrComp(uplink->hisAka, dwlink->hisAka) == 0)
            {
                rc = 2;
                continue;
            }
            if ( (uplink->numDfMask) &&
                 (tag_mask(areatag, uplink->dfMask, uplink->numDfMask)))
            {
                rc = 2;
                continue;
            }
            if ( (uplink->denyFwdFile!=NULL) &&
                 (IsAreaAvailable(areatag,uplink->denyFwdFile,NULL,0)))
            {
                rc = 2;
                continue;
            }
            rc = 0;
            if (uplink->forwardRequestFile!=NULL) {
                /*  first try to find the areatag in forwardRequestFile */
                if (tag_mask(areatag, uplink->frMask, uplink->numFrMask) ||
                    IsAreaAvailable(areatag,uplink->forwardRequestFile,NULL,0))
                {
                    break;
                }
                else
                { rc = 2; }/*  found link with freqfile, but there is no areatag */
            } else {
                if (uplink->numFrMask) /*  found mask */
                {
                    if (tag_mask(areatag, uplink->frMask, uplink->numFrMask))
                        break;
                    else rc = 2;
                } else { /*  unconditional forward request */
                    if (dwlink->denyUFRA==0)
                        break;
                    else rc = 2;
                }
            }/* (uplink->forwardRequestFile!=NULL) */

        }/*  if (uplink->forwardRequests && (uplink->LinkGrp) ? */
    }/*  for (i = 0; i < Requestable; i++) { */

    if(rc == 0)
        forwardRequestToLink(areatag, uplink, dwlink, 0);

    nfree(Indexes);
    return rc;
}

int isPatternLine(char *s) {
    if (strchr(s,'*') || strchr(s,'?')) return 1;
    return 0;
}

void fixRules (s_link *link, char *area) {
    char *fileName = NULL;

    if (!config->rulesDir) return;
    if (link->noRules) return;

    xscatprintf(&fileName, "%s%s.rul", config->rulesDir, strLower(makeMsgbFileName(config, area)));

    if (fexist(fileName)) {
        rulesCount++;
        rulesList = safe_realloc (rulesList, rulesCount * sizeof (char*));
        rulesList[rulesCount-1] = safe_strdup (area);
        /*  don't simply copy pointer because area may be */
        /*  removed while processing other commands */
    }
    nfree (fileName);
}

char *subscribe(s_link *link, char *cmd) {
    unsigned int i, rc=4, found=0, matched=0;
    char *line, *an=NULL, *report = NULL;
    s_area *area=NULL;

    w_log(LL_FUNC, "%s::subscribe(...,%s)", __FILE__, cmd);

    line = cmd;
	
    if (line[0]=='+') line++;
    while (*line==' ') line++;

    if (*line=='+') line++; while (*line==' ') line++;
	
    if (strlen(line)>60 || !isValidConference(line)) {
      report = errorRQ(line);
      w_log(LL_FUNC, "%s::subscribe() FAILED (error request line) rc=%s", __FILE__, report);
      return report;
    }

    for (i=0; !found && rc!=6 && i<config->echoAreaCount; i++) {
	area = &(config->echoAreas[i]);
	an = area->areaName;

	rc=subscribeAreaCheck(area, line, link);
	if (rc==4) continue;        /* not match areatag, try next */
	if (rc==1 && manualCheck(area, link)) rc = 5; /* manual area/group/link */

	if (rc!=0 && limitCheck(link)) rc = 6; /* areas limit exceed for link */

        switch (rc) {
	case 0:         /* already linked */
	    if (isPatternLine(line)) {
		matched = 1;
	    } else {
		xscatprintf(&report, " %s %s  already linked\r",
			    an, print_ch(49-strlen(an), '.'));
		w_log(LL_AREAFIX, "areafix: %s already linked to %s",
		      aka2str(link->hisAka), an);
		i = config->echoAreaCount;
	    }
	    break;
	case 1:         /* not linked */
        if( isOurAka(config,link->hisAka)) {
           if(area->msgbType==MSGTYPE_PASSTHROUGH) {
              int state =
                  changeconfig(cfgFile?cfgFile:getConfigFileName(),area,link,5);
              if( state == ADD_OK) {
                  af_CheckAreaInQuery(an, NULL, NULL, DELIDLE);
                  xscatprintf(&report," %s %s  added\r",an,print_ch(49-strlen(an),'.'));
                  w_log(LL_AREAFIX, "areafix: %s subscribed to %s",aka2str(link->hisAka),an);
                  if (config->autoAreaPause && area->paused)
                      pauseAreas(1, NULL, area);
              } else {
                  xscatprintf(&report, " %s %s  not subscribed\r",an,print_ch(49-strlen(an), '.'));
                  w_log(LL_AREAFIX, "areafix: %s not subscribed to %s , cause uplink",aka2str(link->hisAka),an);
                  w_log(LL_AREAFIX, "areafix: %s has \"passthrough\" in \"autoAreaCreateDefaults\" for %s",
                                    an, aka2str(area->downlinks[0]->link->hisAka));
              }
           } else {  /* ??? (not passthrou echo) */
                     /*   non-passthrough area for our aka means */
                     /*   that we already linked to this area */
               xscatprintf(&report, " %s %s  already linked\r",an, print_ch(49-strlen(an), '.'));
               w_log(LL_AREAFIX, "areafix: %s already linked to %s",aka2str(link->hisAka), an);
           }
        } else {
            if (changeconfig(cfgFile?cfgFile:getConfigFileName(),area,link,0)==ADD_OK) {
                Addlink(config, link, area);
                fixRules (link, area->areaName);
                xscatprintf(&report," %s %s  added\r",an,print_ch(49-strlen(an),'.'));
                w_log(LL_AREAFIX, "areafix: %s subscribed to %s",aka2str(link->hisAka),an);
                if (config->autoAreaPause && area->paused && (link->Pause & ECHOAREA)!=ECHOAREA)
                    pauseAreas(1, link, area);
                af_CheckAreaInQuery(an, NULL, NULL, DELIDLE);
                if(cmNotifyLink)
                forwardRequestToLink(area->areaName,link, NULL, 0);
            } else {
                xscatprintf(&report," %s %s  error. report to sysop!\r",an,print_ch(49-strlen(an),'.'));
                w_log(LL_AREAFIX, "areafix: %s not subscribed to %s",aka2str(link->hisAka),an);
                w_log(LL_ERR, "areafix: can't write to config file: %s!", strerror(errno));
            }/* if (changeconfig(cfgFile?cfgFile:getConfigFileName(),area,link,3)==0) */
        }
	    if (!isPatternLine(line)) i = config->echoAreaCount;
	    break;
	case 6:         /* areas limit exceed for link */
            break;
	default : /*  rc = 2  not access */
	    if (!area->hide && !isPatternLine(line)) {
		w_log(LL_AREAFIX, "areafix: area %s -- no access for %s",
		      an, aka2str(link->hisAka));
		xscatprintf(&report," %s %s  no access\r", an,
			    print_ch(49-strlen(an), '.'));
		found=1;
	    }
	    if (area->hide && !isPatternLine(line)) found=1;
	    break;
	}
    }

    if (rc!=0 && limitCheck(link)) rc = 6; /*double!*/ /* areas limit exceed for link */

    if (rc==4 && !isPatternLine(line) && !found) { /* rc not equal 4 there! */
        if (checkRefuse(line))
        {
            xscatprintf(&report, " %s %s  forwarding refused\r",
			    line, print_ch(49-strlen(line), '.'));
            w_log(LL_WARN, "Can't forward request for area %s : refused by NewAreaRefuseFile\n", line);
        } else
        if (link->denyFRA==0) {
	    /*  try to forward request */
	    if ((rc=forwardRequest(line, link, NULL))==2) {
		xscatprintf(&report, " %s %s  no uplinks to forward\r",
			    line, print_ch(49-strlen(line), '.'));
		w_log( LL_AREAFIX, "areafix: %s - no uplinks to forward", line);
	    }
	    else if (rc==0) {
		xscatprintf(&report, " %s %s  request forwarded\r",
			    line, print_ch(49-strlen(line), '.'));
		w_log( LL_AREAFIX, "areafix: %s - request forwarded", line);
        if( !config->areafixQueueFile && isOurAka(config,link->hisAka)==0)
        {
            area = getArea(config, line);
            if ( !isLinkOfArea(link, area) ) {
                if(changeconfig(cfgFile?cfgFile:getConfigFileName(),area,link,3)==ADD_OK) {
                    Addlink(config, link, area);
                    fixRules (link, area->areaName);
                    w_log( LL_AREAFIX, "areafix: %s subscribed to area %s",
                        aka2str(link->hisAka),line);
                } else {
                    xscatprintf( &report," %s %s  error. report to sysop!\r",
                        an, print_ch(49-strlen(an),'.') );
                    w_log( LL_AREAFIX, "areafix: %s not subscribed to %s",
                        aka2str(link->hisAka),an);
                    w_log(LL_ERR, "areafix: can't change config file: %s!", strerror(errno));
                }
            } else w_log( LL_AREAFIX, "areafix: %s already subscribed to area %s",
                aka2str(link->hisAka), line );

        } else {
            fixRules (link, line);
        }
        }
	}
    }

    if (rc == 6) {   /* areas limit exceed for link */
	w_log( LL_AREAFIX,"areafix: area %s -- no access (full limit) for %s",
	      line, aka2str(link->hisAka));
	xscatprintf(&report," %s %s  no access (full limit)\r",
		    line, print_ch(49-strlen(line), '.'));
    }

    if (matched) {
	if (report == NULL)
	    w_log (LL_AREAFIX, "areafix: all areas matching %s are already linked", line);
	xscatprintf(&report, "All %sareas matching %s are already linked\r", report ? "other " : "", line);
    }
    else if ((report == NULL && found==0) || (found && area->hide)) {
	xscatprintf(&report," %s %s  not found\r",line,print_ch(49-strlen(line),'.'));
	w_log( LL_AREAFIX, "areafix: area %s is not found",line);
    }
    w_log(LL_FUNC, "areafix::subscribe() OK");
    return report;
}

char *errorRQ(char *line) {
    char *report = NULL;

    if (strlen(line)>48) {
	xstrscat(&report, " ", line, " .......... error line\r", NULL);
    }
    else xscatprintf(&report, " %s %s  error line\r",
		    line, print_ch(49-strlen(line),'.'));
    return report;
}

char *do_delete(s_link *link, s_area *area) {
    char *report = NULL, *an = area->areaName;
    unsigned int i=0;

    if(!link)
    {
        link = getLinkFromAddr(config, *area->useAka);
        while( !link && i < config->addrCount )
        {
            link = getLinkFromAddr( config, config->addr[i] );
            i++;
        }
        if(!link) return NULL;
    }
    /* unsubscribe from downlinks */
    xscatprintf(&report, " %s %s  deleted\r", an, print_ch(49-strlen(an), '.'));
    for (i=0; i<area->downlinkCount; i++) {
	if (addrComp(area->downlinks[i]->link->hisAka, link->hisAka))
	    forwardRequestToLink(an, area->downlinks[i]->link, NULL, 2);
    }
    /* remove area from config-file */
    if( changeconfig ((cfgFile) ? cfgFile : getConfigFileName(),  area, link, 4) != DEL_OK) {
       w_log( LL_AREAFIX, "areafix: can't remove area from config: %s", strerror(errno));
    }

    /* delete msgbase and dupebase for the area */

    /*
    if (area->msgbType!=MSGTYPE_PASSTHROUGH)
	MsgDeleteBase(area->fileName, (word) area->msgbType);
    */

    if (area->dupeCheck != dcOff && config->typeDupeBase != commonDupeBase) {
	char *dupename = createDupeFileName(area);
	if (dupename) {
	    unlink(dupename);
	    nfree(dupename);
	}
    }

    w_log( LL_AREAFIX, "areafix: area %s deleted by %s",
                  an, aka2str(link->hisAka));

    /* delete the area from in-core config */
    for (i=0; i<config->echoAreaCount; i++)
    {
        if (stricmp(config->echoAreas[i].areaName, an)==0)
            break;
    }
    if (i<config->echoAreaCount && area==&(config->echoAreas[i])) {
        fc_freeEchoArea(area);
        for (; i<config->echoAreaCount-1; i++)
            memcpy(&(config->echoAreas[i]), &(config->echoAreas[i+1]),
            sizeof(s_area));
        config->echoAreaCount--;
        RebuildEchoAreaTree(config);
    }
    return report;
}

char *delete(s_link *link, char *cmd) {
    int rc;
    char *line, *report = NULL, *an;
    s_area *area;

    for (line = cmd + 1; *line == ' ' || *line == '\t'; line++);

    if (*line == 0) return errorRQ(cmd);

    area = getArea(config, line);
    if (area == &(config->badArea)) {
	xscatprintf(&report, " %s %s  not found\r", line, print_ch(49-strlen(line), '.'));
	w_log(LL_AREAFIX, "areafix: area %s is not found", line);
	return report;
    }
    rc = subscribeCheck(area, link);
    an = area->areaName;

    switch (rc) {
    case 0:
	break;
    case 1:
	xscatprintf(&report, " %s %s  not linked\r", an, print_ch(49-strlen(an), '.'));
	w_log(LL_AREAFIX, "areafix: area %s is not linked to %s",
	      an, aka2str(link->hisAka));
	return report;
    case 2:
	xscatprintf(&report, " %s %s  no access\r", an, print_ch(49-strlen(an), '.'));
	w_log(LL_AREAFIX, "areafix: area %s -- no access for %s", an, aka2str(link->hisAka));
	return report;
    }
    if (link->LinkGrp == NULL || (area->group && strcmp(link->LinkGrp, area->group))) {
	xscatprintf(&report, " %s %s  delete not allowed\r",
		    an, print_ch(49-strlen(an), '.'));
	w_log(LL_AREAFIX, "areafix: area %s delete not allowed for %s",
	      an, aka2str(link->hisAka));
	return report;
    }
    return do_delete(link, area);
}

char *unsubscribe(s_link *link, char *cmd) {
    unsigned int i, rc = 2, j=(unsigned int)I_ERR, from_us=0, matched = 0;
    char *line, *an, *report = NULL;
    s_area *area;

    w_log(LL_FUNC,__FILE__ ":%u:unsubscribe() begin", __LINE__);
    line = cmd;
	
    if (line[1]=='-') return NULL;
    line++;
    while (*line==' ') line++;
	
    for (i = 0; i< config->echoAreaCount; i++) {
        area = &(config->echoAreas[i]);
        an = area->areaName;

        rc = subscribeAreaCheck(area, line, link);
        if (rc==4) continue;
        if (rc==0 && mandatoryCheck(area,link)) rc = 5;

        if (isOurAka(config,link->hisAka))
        {
            from_us = 1;
            rc = area->msgbType == MSGTYPE_PASSTHROUGH ? 1 : 0 ;
        }

        switch (rc) {
        case 0:
            if (from_us == 0) {
                unsigned int k;
                for (k=0; k<area->downlinkCount; k++)
                    if (addrComp(link->hisAka, area->downlinks[k]->link->hisAka)==0 &&
                        area->downlinks[k]->defLink)
                        return do_delete(link, area);
                    RemoveLink(link, area);
                    if ((area->msgbType == MSGTYPE_PASSTHROUGH) &&
                        (area->downlinkCount == 1) &&
                        (area->downlinks[0]->link->hisAka.point == 0))
                    {
                        if(config->areafixQueueFile)
                        {
                            af_CheckAreaInQuery(an, &(area->downlinks[0]->link->hisAka), NULL, ADDIDLE);
                            j = changeconfig(cfgFile?cfgFile:getConfigFileName(),area,link,7);
                        }
                        else
                        {
                            j = changeconfig(cfgFile?cfgFile:getConfigFileName(),area,link,1);
                        }
                    }
                    else
                    {
                        j = changeconfig(cfgFile?cfgFile:getConfigFileName(),area,link,7);
                        if (j == DEL_OK && config->autoAreaPause && !area->paused && (link->Pause & ECHOAREA) != ECHOAREA)
                            pauseAreas(0,NULL,area);
                    }
                    if (j != DEL_OK) {
                        w_log(LL_AREAFIX, "areafix: %s doesn't unlinked from %s",
                            aka2str(link->hisAka), an);
                    } else {
                        w_log(LL_AREAFIX,"areafix: %s unlinked from %s",aka2str(link->hisAka),an);
                        if(cmNotifyLink)
                        forwardRequestToLink(area->areaName,link, NULL, 1);
                    }
            } else { /*  unsubscribing from own address - set area passtrough */
                if (area->downlinkCount==0)
                {
                    return do_delete(getLinkFromAddr(config,*(area->useAka)), area);
                }
                else if ( (area->downlinkCount==1) &&
                          (area->downlinks[0]->link->hisAka.point == 0 ||
                           area->downlinks[0]->defLink) ) {
                    if(config->areafixQueueFile) {
                        af_CheckAreaInQuery(an, &(area->downlinks[0]->link->hisAka), NULL, ADDIDLE);
                    } else {
                        forwardRequestToLink(area->areaName,
                            area->downlinks[0]->link, NULL, 1);
                    }
                } else if (config->autoAreaPause && !area->paused) {
                       area->msgbType = MSGTYPE_PASSTHROUGH;
                       pauseAreas(0,NULL,area);
                }
                j = changeconfig(cfgFile?cfgFile:getConfigFileName(),area,link,6);
/*                if ( (j == DEL_OK) && area->msgbType!=MSGTYPE_PASSTHROUGH ) */
                if (j == DEL_OK && area->fileName && area->killMsgBase)
                       MsgDeleteBase(area->fileName, (word) area->msgbType);
            }
            if (j == DEL_OK){
                xscatprintf(&report," %s %s  unlinked\r",an,print_ch(49-strlen(an),'.'));
            }else
                xscatprintf(&report," %s %s  error. report to sysop!\r",
                                         an, print_ch(49-strlen(an),'.') );
            break;
        case 1:
            if (isPatternLine(line)) {
                matched = 1;
                continue;
            }
            if (area->hide) {
                i = config->echoAreaCount;
                break;
            }
            xscatprintf(&report, " %s %s  not linked\r",
                an, print_ch(49-strlen(an), '.'));
            w_log(LL_AREAFIX, "areafix: area %s is not linked to %s",
                area->areaName, aka2str(link->hisAka));
            break;
        case 5:
            xscatprintf(&report, " %s %s  unlink is not possible\r",
                an, print_ch(49-strlen(an), '.'));
            w_log(LL_AREAFIX, "areafix: area %s -- unlink is not possible for %s",
                area->areaName, aka2str(link->hisAka));
            break;
        default:
            break;
        }
    }
    if(config->areafixQueueFile)
        report = af_Req2Idle(line, report, link->hisAka);
    if (report == NULL) {
        if (matched) {
            xscatprintf(&report, " %s %s  no areas to unlink\r",
                line, print_ch(49-strlen(line), '.'));
            w_log(LL_AREAFIX, "areafix: no areas to unlink");
        } else {
            xscatprintf(&report, " %s %s  not found\r",
                line, print_ch(49-strlen(line), '.'));
            w_log(LL_AREAFIX, "areafix: area %s is not found", line);
        }
    }
    w_log(LL_FUNC,__FILE__ ":%u:unsubscribe() end", __LINE__);
    return report;
}

/* if act==0 pause area, if act==1 unpause area */
void pauseAreas(int act, s_link *searchLink, s_area *searchArea) {
  unsigned int i, j, k, linkCount;

  if (!searchLink && !searchArea) return;

  for (i=0; i < config->echoAreaCount; i++) {
    s_link *uplink;
    s_area *area;
    s_message *msg;

    area = &(config->echoAreas[i]);
    if (act==0 && (area->paused || area->noautoareapause || area->msgbType!=MSGTYPE_PASSTHROUGH)) continue;
    if (act==1 && (!area->paused)) continue;
    if (searchArea && searchArea != area) continue;
    if (searchLink && isLinkOfArea(searchLink, area)!=1) continue;

    linkCount = 0;
    for (j=0; j < area->downlinkCount; j++) { /* try to find uplink */
      if ( (area->downlinks[j]->link->Pause & ECHOAREA) != ECHOAREA &&
           (!searchLink || addrComp(searchLink->hisAka, area->downlinks[j]->link->hisAka)!=0) ) {
        linkCount++;
        if (area->downlinks[j]->defLink) uplink = area->downlinks[j]->link;
      }
    }

    if (linkCount!=1 || !uplink) continue; /* uplink not found */

    /* change config */
    if (act==0) { /* make area paused */
      if (changeconfig(cfgFile?cfgFile:getConfigFileName(),area,NULL,8)==ADD_OK) {
        w_log(LL_AREAFIX, "areafix: area %s is paused (uplink: %s)",
                 area->areaName, aka2str(uplink->hisAka));
      } else {
        w_log(LL_AREAFIX, "areafix: error pausing area %s", area->areaName);
        continue;
      }
    } else if (act==1) { /* make area non paused */
      if (changeconfig(cfgFile?cfgFile:getConfigFileName(),area,NULL,9)==ADD_OK) {
        w_log(LL_AREAFIX, "areafix: area %s is not paused any more (uplink: %s)",
                 area->areaName, aka2str(uplink->hisAka));
      } else {
        w_log(LL_AREAFIX, "areafix: error unpausing area %s", area->areaName);
        continue;
      }
    }

    /* write messages */
    if (uplink->msg == NULL) {
      msg = makeMessage(uplink->ourAka, &(uplink->hisAka), config->sysop,
      uplink->RemoteRobotName ? uplink->RemoteRobotName : "areafix",
      uplink->areaFixPwd ? uplink->areaFixPwd : "\x00", 1,
      uplink->areafixReportsAttr ? uplink->areafixReportsAttr : config->areafixReportsAttr);
      msg->text = createKludges(config, NULL, uplink->ourAka, &(uplink->hisAka),
                      versionStr);
      if (uplink->areafixReportsFlags)
        xstrscat(&(msg->text), "\001FLAGS ", uplink->areafixReportsFlags, "\r",NULL);
      else if (config->areafixReportsFlags)
        xstrscat(&(msg->text), "\001FLAGS ", config->areafixReportsFlags, "\r",NULL);
      uplink->msg = msg;
    } else msg = uplink->msg;

    if (act==0)
      xscatprintf(&(msg->text), "-%s\r", area->areaName);
    else if (act==1)
      xscatprintf(&(msg->text), "+%s\r", area->areaName);

  }
}

char *pause_link(s_link *link)
{
   char *tmp, *report = NULL;

   if ((link->Pause & ECHOAREA) != ECHOAREA) {
      if (Changepause((cfgFile) ? cfgFile : getConfigFileName(), link, 0,ECHOAREA) == 0)
         return NULL;
   }
   xstrcat(&report, " System switched to passive\r");
   tmp = list (lt_linked, link, NULL);/*linked (link);*/
   xstrcat(&report, tmp);
   nfree(tmp);

   /* check for areas with one link alive and others paused */
   if (config->autoAreaPause)
       pauseAreas(0, link, NULL);

   return report;
}
char *resume_link(s_link *link)
{
    char *tmp, *report = NULL;

    if ((link->Pause & ECHOAREA) == ECHOAREA) {
	if (Changepause((cfgFile) ? cfgFile : getConfigFileName(), link,0,ECHOAREA) == 0)
	    return NULL;
    }

    xstrcat(&report, " System switched to active\r");
    tmp = list (lt_linked, link, NULL);/*linked (link);*/
    xstrcat(&report, tmp);
    nfree(tmp);

    /* check for paused areas with this link */
    if (config->autoAreaPause)
        pauseAreas(1, link, NULL);

    return report;
}

char *info_link(s_link *link)
{
    char *report=NULL, *ptr, linkAka[SIZE_aka2str];
    unsigned int i;

    sprintf(linkAka,aka2str(link->hisAka));
    xscatprintf(&report, "Here is some information about our link:\r\r");
    xscatprintf(&report, "            Your address: %s\r", linkAka);
    xscatprintf(&report, "           AKA used here: %s\r", aka2str(*link->ourAka));
    xscatprintf(&report, "         Reduced SEEN-BY: %s\r", link->reducedSeenBy ? "on" : "off");
    xscatprintf(&report, " Send rules on subscribe: %s\r", link->noRules ? "off" : "on");
    if (link->pktSize)
    xscatprintf(&report, "             Packet size: %u kbytes\r", link->pktSize);
    else
    xscatprintf(&report, "             Packet size: unlimited\r");
    xscatprintf(&report, "     Arcmail bundle size: %u kbytes\r", link->arcmailSize!=0 ? link->arcmailSize :
                      (config->defarcmailSize ? config->defarcmailSize : 500));
    xscatprintf(&report, " Forward requests access: %s\r", link->denyFRA ? "off" : "on");
    xscatprintf(&report, "Compression: ");

    if (link->packerDef==NULL)
	xscatprintf(&report, "No packer (");
    else
	xscatprintf(&report, "%s (", link->packerDef->packer);

    for (i=0; i < config->packCount; i++)
	xscatprintf(&report, "%s%s", config->pack[i].packer,
		    (i+1 == config->packCount) ? "" : ", ");
    xscatprintf(&report, ")\r\r");
    xscatprintf(&report, "Your system is %s\r", ((link->Pause & ECHOAREA) == ECHOAREA)?"passive":"active");
    ptr = list (lt_linked, link, NULL);/*linked (link);*/
    xstrcat(&report, ptr);
    nfree(ptr);
    w_log(LL_AREAFIX, "areafix: link information sent to %s", aka2str(link->hisAka));
    return report;
}

char *rescan(s_link *link, char *cmd) {
    unsigned int i, c, rc = 0;
    long rescanCount = -1, rcc;
    char *report = NULL, *line, *countstr, *an, *end;
    s_area *area;
    s_arealink *arealink;

    line = cmd;
    if (strncasecmp(cmd, "%rescan", 7)==0) line += strlen("%rescan");

    if (*line == 0) return errorRQ(cmd);

    while (*line && (*line == ' ' || *line == '\t')) line++;

    if (*line == 0) return errorRQ(cmd);

    countstr = line;
    while (*countstr && (!isspace(*countstr))) countstr++; /*  skip areatag */
    while (*countstr && (*countstr == ' ' || *countstr == '\t')) countstr++;
    if (strncasecmp(countstr, "/R",2)==0) {
	countstr += 2;
	if (*countstr == '=') countstr++;
    }
    if (strncasecmp(countstr, "R=",2)==0) {
	countstr += 2;
    }
	
    if (*countstr != '\0') {
	rescanCount = strtol(countstr, NULL, 10);
    }

    end = strpbrk(line, " \t");
    if (end) *end = 0;

    if (*line == 0) return errorRQ(cmd);

    for (i=c=0; i<config->echoAreaCount; i++) {
	rc=subscribeAreaCheck(&(config->echoAreas[i]), line, link);
	if (rc == 4) continue;

	area = &(config->echoAreas[i]);
	an = area->areaName;

	switch (rc) {
	case 0:
	    if (area->msgbType == MSGTYPE_PASSTHROUGH) {
		xscatprintf(&report," %s %s  no rescan possible\r",
			    an, print_ch(49-strlen(an), '.'));
		w_log(LL_AREAFIX, "areafix: %s area no rescan possible to %s",
		      an, aka2str(link->hisAka));
	    } else {

		arealink = getAreaLink(area, link->hisAka);
		if (arealink->export) {
		    rcc = rescanEMArea(area, arealink, rescanCount);
		    tossTempOutbound(config->tempOutbound);
		} else {
		    rcc = 0;
		    xscatprintf(&report," %s %s  no access to export\r",
				an, print_ch(49-strlen(an), '.'));
		    w_log(LL_AREAFIX, "areafix: %s -- no access to export for %s",
			  an, aka2str(link->hisAka));
		}
		xscatprintf(&report," %s %s  rescanned %lu mails\r",
			    an, print_ch(49-strlen(an), '.'), rcc);
		w_log(LL_AREAFIX,"areafix: %s rescanned %lu mails to %s",
		      an, rcc, aka2str(link->hisAka));
	    }
	    if (!isPatternLine(line)) i = config->echoAreaCount;
	    break;
	case 1: if (isPatternLine(line)) continue;
	    w_log(LL_AREAFIX, "areafix: %s area not linked for rescan to %s",
		  area->areaName, aka2str(link->hisAka));
	    xscatprintf(&report, " %s %s  not linked for rescan\r",
			an, print_ch(49-strlen(an), '.'));
	    break;
	default: w_log(LL_AREAFIX, "areafix: %s area not access for %s",
		       area->areaName, aka2str(link->hisAka));
	    break;
	}
    }
    if (report == NULL) {
	xscatprintf(&report," %s %s  not linked for rescan\r",
		    line, print_ch(49-strlen(line), '.'));
	w_log(LL_AREAFIX, "areafix: %s area not linked for rescan", line);
    }
    return report;
}

char *add_rescan(s_link *link, char *line) {
    char *report=NULL, *line2=NULL, *p;

    if (*line=='+') line++; while (*line==' ') line++;

    p = strchr(line, ' '); /* select only areaname */
    if (p) *p = '\0';

    report = subscribe (link, line);
    if (p) *p = ' '; /* resume original string */

    xstrscat(&line2,"%rescan ", line, NULL);
    xstrcat(&report, rescan(link, line2));
    nfree(line2);
    if (p) *p = '\0';

    return report;
}

char *pktsize (s_link *link, char *cmdline) {

    char *report = NULL;
    char *pattern = NULL;
    int reversed;
    char *error = NULL;
    unsigned long num = 0;

    pattern = safe_strdup(getPatternFromLine(cmdline, &reversed));

    if (pattern == NULL) {
        xscatprintf(&report, "Invalid request :%s\rPlease, read help!\r\r", cmdline);
        return report;
    }

    pattern = trimLine(pattern);

    num = strtoul(pattern, &error, 10);
    if ( ((error != NULL) && (*error != '\0')) || num == unsigned_long_max ) {
        xscatprintf(&report, "'%s' is not a valid number!\r", pattern);
        nfree(error);
        return report;
    } else {

        char *confName = NULL;
        char *pktSizeString = NULL;
        long strbeg = 0;
        long strend = 0;

        if (link->pktSize == num) {
            xscatprintf(&report, "Pkt size is already set to %u kbytes. No changes were made.\r", num);
            return report;
        }

        xstrcat(&confName,(cfgFile) ? cfgFile : getConfigFileName());
        FindTokenPos4Link(&confName, "pktSize", link, &strbeg, &strend);
        xscatprintf(&pktSizeString,"pktSize %u",num);
        if( InsertCfgLine(confName, pktSizeString, strbeg, strend) ) {
            link->pktSize = num;
            xscatprintf(&report, "Pkt size is set to %u kbytes.\r", num);
        }

        nfree(confName);
        nfree(pktSizeString);
        return report;
    }
}

char *arcmailsize (s_link *link, char *cmdline) {

    char *report = NULL;
    char *pattern = NULL;
    int reversed;
    char *error = NULL;
    unsigned long num = 0;

    pattern = safe_strdup(getPatternFromLine(cmdline, &reversed));

    if (pattern == NULL) {
        xscatprintf(&report, "Invalid request :%s\rPlease, read help!\r\r", cmdline);
        return report;
    }

    pattern = trimLine(pattern);

    num = strtoul(pattern, &error, 10);
    if ( ((error != NULL) && (*error != '\0')) || num == unsigned_long_max ) {
        xscatprintf(&report, "'%s' is not a valid number!\r", pattern);
        nfree(error);
        return report;
    } else {

        char *confName = NULL;
        char *arcmailSizeString = NULL;
        long strbeg = 0;
        long strend = 0;

        if (link->arcmailSize == num) {
            xscatprintf(&report, "Arcmail size is already set to %u kbytes. No changes were made.\r", num);
            return report;
        }

        xstrcat(&confName,(cfgFile) ? cfgFile : getConfigFileName());
        FindTokenPos4Link(&confName, "arcmailSize", link, &strbeg, &strend);
        xscatprintf(&arcmailSizeString,"arcmailSize %u",num);
        if( InsertCfgLine(confName, arcmailSizeString, strbeg, strend) ) {
            link->arcmailSize = num;
            xscatprintf(&report, "Arcmail size is set to %u kbytes.\r", num);
        }

        nfree(confName);
        nfree(arcmailSizeString);
        return report;
    }
}

char *packer(s_link *link, char *cmdline) {
    char *report=NULL;
    char *was=NULL;
    char *pattern = NULL;
    int reversed;
    UINT i;
    pattern = getPatternFromLine(cmdline, &reversed);
    if(pattern)
    {
        char *packerString=NULL;
        ps_pack packerDef = NULL;
        char *confName = NULL;
        long  strbeg=0;
        long  strend=0;

        for (i=0; i < config->packCount; i++)
        {
            if (stricmp(config->pack[i].packer,pattern) == 0)
            {
                packerDef = &(config->pack[i]);
                break;
            }
        }
        if( (i == config->packCount) && (stricmp("none",pattern) != 0) )
        {
            xscatprintf(&report, "Packer '%s' was not found\r", pattern);
            return report;
        }
        if (link->packerDef==NULL)
            xstrcat(&was, "none");
        else
            xstrcat(&was, link->packerDef->packer);

        xstrcat(&confName,(cfgFile) ? cfgFile : getConfigFileName());
        FindTokenPos4Link(&confName, "Packer", link, &strbeg, &strend);
        xscatprintf(&packerString,"Packer %s",pattern);
        if( InsertCfgLine(confName, packerString, strbeg, strend) )
        {
           link->packerDef = packerDef;
        }
        nfree(confName);
        nfree(packerString);
    }

    xstrcat(  &report, "Here is some information about current & available packers:\r\r");
    xstrcat(  &report,       "Compression: ");
    if (link->packerDef==NULL)
        xscatprintf(&report, "none (");
    else
        xscatprintf(&report, "%s (", link->packerDef->packer);

    for (i=0; i < config->packCount; i++)
        xscatprintf(&report, "%s%s", config->pack[i].packer,(i+1 == config->packCount) ? "" : ", ");

    xscatprintf(&report, "%snone)\r", (i == 0) ? "" : ", ");
    if(was)
    {
        xscatprintf(&report, "        was: %s\r", was);
    }
    return report;
}

char *rsb(s_link *link, char *cmdline)
{
    int mode; /*  1 = RSB on, 0 - RSB off. */
    char *param=NULL; /*  RSB value. */
    char *report=NULL;
    char *confName = NULL;
    long  strbeg=0;
    long  strend=0;

    param = getPatternFromLine(cmdline, &mode); /*  extract rsb value (on or off) */
    if (param == NULL)
    {
        xscatprintf(&report, "Invalid request: %s\rPlease read help.\r\r", cmdline);
        return report;
    }

    param = trimLine(param);

    if ((!strcmp(param, "0")) || (!strcasecmp(param, "off")))
        mode = 0;
    else
    {
        if ((!strcmp(param, "1")) || (!strcasecmp(param, "on")))
            mode = 1;
        else
        {
            xscatprintf(&report, "Unknown parameter for areafix %rsb command: %s\r. Please read help.\r\r",
                        param);
            nfree(param);
            return report;
        }
    }
    nfree(param);
    if (link->reducedSeenBy == (UINT)mode)
    {
        xscatprintf(&report, "Redused SEEN-BYs had not been changed.\rCurrent value is '%s'\r\r",
                    mode?"on":"off");
        return report;
    }
    xstrcat(&confName,(cfgFile) ? cfgFile : getConfigFileName());
    FindTokenPos4Link(&confName, "reducedSeenBy", link, &strbeg, &strend);
    xscatprintf(&param, "reducedSeenBy %s", mode?"on":"off");
    if( InsertCfgLine(confName, param, strbeg, strend) )
    {
        xscatprintf(&report, "Redused SEEN-BYs is turned %s now\r\r", mode?"on":"off");
        link->reducedSeenBy = mode;
    }
    nfree(param);
    nfree(confName);
    return report;
}

char *rules(s_link *link, char *cmdline)
{
    int mode; /*  1 = RULES on (noRules off), 0 - RULES off (noRules on). */
              /*  !!! Use inversed values for noRules keyword. !!! */

    char *param=NULL; /*  RULES value. */
    char *report=NULL;
    char *confName = NULL;
    long  strbeg=0;
    long  strend=0;

    param = safe_strdup(getPatternFromLine(cmdline, &mode)); /*  extract rules value (on or off) */
    param = trimLine(param);
    if (*param == '\0')
    {
        xscatprintf(&report, "Invalid request: %s\rPlease read help.\r\r", cmdline);
        return report;
    }

    if ((!strcmp(param, "0")) || (!strcasecmp(param, "off")))
        mode = 0;
    else
    {
        if ((!strcmp(param, "1")) || (!strcasecmp(param, "on")))
            mode = 1;
        else
        {
            xscatprintf(&report, "Unknown parameter for areafix %rules command: %s\r. Please read help.\r\r",
                        param);
            nfree(param);
            return report;
        }
    }
    nfree(param);
    if (link->noRules != (UINT)mode)
    {
        xscatprintf(&report, "Send rules mode had not been changed.\rCurrent value is '%s'\r\r",
                    mode?"on":"off");
        return report;
    }
    xstrcat(&confName,(cfgFile) ? cfgFile : getConfigFileName());
    FindTokenPos4Link(&confName, "noRules", link, &strbeg, &strend);
    xscatprintf(&param, "noRules %s", mode?"off":"on");
    if( InsertCfgLine(confName, param, strbeg, strend) )
    {
        xscatprintf(&report, "Send rules mode is turned %s now\r\r", mode?"on":"off");
        link->noRules = (mode ? 0 : 1);
    }
    nfree(param);
    nfree(confName);
    return report;
}

int tellcmd(char *cmd) {
    char *line;

    if (strncmp(cmd, "* Origin:", 9) == 0) return NOTHING;

    line = cmd;
    if (line && *line && (line[1]==' ' || line[1]=='\t')) return AFERROR;

    switch (line[0]) {
    case '%':
        line++;
        if (*line == '\000') return AFERROR;
        if (strncasecmp(line,"list",4)==0) return LIST;
        if (strncasecmp(line,"help",4)==0) return HELP;
        if (strncasecmp(line,"avail",5)==0) return AVAIL;
        if (strncasecmp(line,"all",3)==0) return AVAIL;
        if (strncasecmp(line,"unlinked",8)==0) return UNLINKED;
        if (strncasecmp(line,"linked",6)==0) return QUERY;
        if (strncasecmp(line,"query",5)==0) return QUERY;
        if (strncasecmp(line,"pause",5)==0) return PAUSE;
        if (strncasecmp(line,"resume",6)==0) return RESUME;
        if (strncasecmp(line,"info",4)==0) return INFO;
        if (strncasecmp(line,"packer",6)==0) return PACKER;
        if (strncasecmp(line,"compress",8)==0) return PACKER;
        if (strncasecmp(line,"rsb",3)==0) return RSB;
        if (strncasecmp(line,"rules",5)==0) return RULES;
        if (strncasecmp(line,"pktsize",7)==0) return PKTSIZE;
        if (strncasecmp(line,"arcmailsize",11)==0) return ARCMAILSIZE;
        if (strncasecmp(line,"rescan", 6)==0) {
            if (line[6] == '\0') {
                rescanMode=1;
                return NOTHING;
            } else {
                return RESCAN;
            }
        }
        return AFERROR;
    case '\001': return NOTHING;
    case '\000': return NOTHING;
    case '-'  :
        if (line[1]=='-' && line[2]=='-') return DONE;
        if (line[1]=='\000') return AFERROR;
        if (strchr(line,' ') || strchr(line,'\t')) return AFERROR;
        return DEL;
    case '~'  : return REMOVE;
    case '+':
        if (line[1]=='\000') return AFERROR;
    default:
        if (fc_stristr(line, " /R")!=NULL) return ADD_RSC; /*  add & rescan */
        if (fc_stristr(line, " R=")!=NULL) return ADD_RSC; /*  add & rescan */
        return ADD;
    }
    return 0;/*  - Unreachable */
}

char *processcmd(s_link *link, char *line, int cmd) {

    char *report = NULL;

    w_log(LL_FUNC, __FILE__ "::processcmd()");
#ifdef DO_PERL
    if (cmd != NOTHING && cmd != DONE && cmd != AFERROR) {
      int rc = perl_afixcmd(&report, cmd, aka2str(link->hisAka), line);
      if (cmd == DEL || cmd == REMOVE || cmd == RESCAN || cmd == ADD_RSC)
        RetFix = STAT;
        else RetFix = cmd;
      if (rc) return report;
    }
#endif
    switch (cmd) {

    case NOTHING: return NULL;

    case DONE: RetFix=DONE;
        return NULL;

    case LIST: report = list (lt_all, link, line);
        RetFix=LIST;
        break;
    case HELP: report = help (link);
        RetFix=HELP;
        break;
    case ADD: report = subscribe (link, line);
        RetFix=ADD;
        break;
    case DEL: report = unsubscribe (link, line);
        RetFix=STAT;
        break;
    case REMOVE: report = delete (link, line);
        RetFix=STAT;
        break;
    case AVAIL: report = available (link, line);
        RetFix=AVAIL;
        break;
    case UNLINKED: report = list (lt_unlinked, link, line);/*report = unlinked (link);*/
        RetFix=UNLINKED;
        break;
    case QUERY: report = list (lt_linked, link, line);/*report = linked (link);*/
        RetFix=QUERY;
        break;
    case PAUSE: report = pause_link (link);
        RetFix=PAUSE;
        break;
    case RESUME: report = resume_link (link);
        RetFix=RESUME;
        break;
    case PACKER: report = packer (link, line);
        RetFix=PACKER;
        break;
    case RSB: report = rsb (link, line);
        RetFix=RSB;
        break;
    case RULES: report = rules (link, line);
        RetFix=RULES;
        break;
    case PKTSIZE: report = pktsize (link, line);
        RetFix=PKTSIZE;
        break;
    case ARCMAILSIZE: report = arcmailsize (link, line);
        RetFix=ARCMAILSIZE;
        break;
    case INFO: report = info_link(link);
        RetFix=INFO;
        break;
    case RESCAN: report = rescan(link, line);
        RetFix=STAT;
        break;
    case ADD_RSC: report = add_rescan(link, line);
        RetFix=STAT;
        break;
    case AFERROR: report = errorRQ(line);
        RetFix=STAT;
        break;
    default: return NULL;
    }
    w_log(LL_FUNC, __FILE__ "::processcmd() OK");
    return report;
}

void preprocText(char *split, s_message *msg, char *reply, s_link *link)
{
    char *orig = config->areafixOrigin;

    msg->text = createKludges(config, NULL, &msg->origAddr,
        &msg->destAddr, versionStr);
    if (reply) {
        reply = strchr(reply, ' ');
        if (reply) reply++;
        if (reply[0])
            xscatprintf(&(msg->text), "\001REPLY: %s\r", reply);
    }
    /* xstrcat(&(msg->text), "\001FLAGS NPD DIR\r"); */
    if (link->areafixReportsFlags)
        xstrscat(&(msg->text), "\001FLAGS ", link->areafixReportsFlags, "\r",NULL);
    else if (config->areafixReportsFlags)
        xstrscat(&(msg->text), "\001FLAGS ", config->areafixReportsFlags, "\r",NULL);
    xscatprintf(&split, "\r--- %s areafix\r", versionStr);
    if (orig && orig[0]) {
        xscatprintf(&split, " * Origin: %s (%s)\r", orig, aka2str(msg->origAddr));
    }
    xstrcat(&(msg->text), split);
    msg->textLength=(int)strlen(msg->text);
    nfree(split);
}

char *textHead(void)
{
    char *text_head = NULL;

    xscatprintf(&text_head, " Area%sStatus\r",	print_ch(48,' '));
    xscatprintf(&text_head, " %s  -------------------------\r",print_ch(50, '-'));
    return text_head;
}

char *areaStatus(char *report, char *preport)
{
    if (report == NULL) report = textHead();
    xstrcat(&report, preport);
    nfree(preport);
    return report;
}

/* report already nfree() after this function */
void RetMsg(s_message *msg, s_link *link, char *report, char *subj)
{
    char *text, *split, *p, *newsubj = NULL;
    char splitted[]=" > message splitted...";
    char *splitStr = config->areafixSplitStr;
    int len, msgsize = config->areafixMsgSize * 1024, partnum=0;
    s_message *tmpmsg;
    char *reply = NULL;

    /* val: silent mode - don't write messages */
    if (silent_mode) return;

    text = report;
    reply = GetCtrlToken(msg->ctl, "MSGID");

    while (text) {

        len = strlen(text);
        if (msgsize == 0 || len <= msgsize) {
            split = text;
            text = NULL;
            if (partnum) { /* last part of splitted msg */
                partnum++;
                xstrcat(&text,split);
                split = text;
                text = NULL;
                nfree(report);
            }
            if (msg->text)
                xstrscat(&split,"\rFollowing is the original message text\r--------------------------------------\r",msg->text,"\r--------------------------------------\r",NULL);
            else
                xstrscat(&split,"\r",NULL);
        } else {
            p = text + msgsize;
            while (*p != '\r') p--;
            *p = '\000';
            len = p - text;
            split = (char*)safe_malloc(len+strlen(splitStr ? splitStr : splitted)+3+1);
            memcpy(split,text,len);
            strcpy(split+len,"\r\r");
            strcat(split, (splitStr) ? splitStr : splitted);
            strcat(split,"\r");
            text = p+1;
            partnum++;
        }

        if (partnum) xscatprintf(&newsubj, "%s (%d)", subj, partnum);
        else newsubj = subj;

        tmpmsg = makeMessage(link->ourAka, &(link->hisAka),
            config->areafixFromName ? config->areafixFromName : msg->toUserName,
            msg->fromUserName, newsubj, 1,
            link->areafixReportsAttr ? link->areafixReportsAttr : config->areafixReportsAttr);
  
        preprocText(split, tmpmsg, reply, link);

        if (config->outtab != NULL) {
            recodeToTransportCharset((CHAR*)tmpmsg->subjectLine);
            recodeToTransportCharset((CHAR*)tmpmsg->fromUserName);
            recodeToTransportCharset((CHAR*)tmpmsg->toUserName);
            recodeToTransportCharset((CHAR*)tmpmsg->text);
            tmpmsg->recode &= ~(REC_HDR|REC_TXT);
        }

        nfree(reply);
        processNMMsg(tmpmsg, NULL, getRobotsArea(config),
            0, MSGLOCAL);
        writeEchoTossLogEntry(getRobotsArea(config)->areaName);
        closeOpenedPkt();
        freeMsgBuffers(tmpmsg);
        nfree(tmpmsg);
        if (partnum) nfree(newsubj);
    }

/*    config->intab = tab; */
}

void RetRules (s_message *msg, s_link *link, char *areaName)
{
    FILE *f=NULL;
    char *fileName = NULL;
    char *text=NULL, *subj=NULL;
    char *msg_text;
    long len=0;
    int nrul=0;

    xscatprintf(&fileName, "%s%s.rul", config->rulesDir, strLower(makeMsgbFileName(config, areaName)));

    for (nrul=0; nrul<=9 && (f = fopen (fileName, "rb")); nrul++) {

	len = fsize (fileName);
	text = safe_malloc (len+1);
	fread (text, len, 1, f);
	fclose (f);

	text[len] = '\0';

	if (nrul==0) {
	    xscatprintf(&subj, "Rules of %s", areaName);
	    w_log(LL_AREAFIX, "areafix: send '%s' as rules for area '%s'",
		  fileName, areaName);
	} else {
	    xscatprintf(&subj, "Echo related text #%d of %s", nrul, areaName);
	    w_log(LL_AREAFIX, "areafix: send '%s' as text %d for area '%s'",
		  fileName, nrul, areaName);
	}

        /* prevent "Following original message text" in rules msgs */
        msg_text = msg->text;
        msg->text= NULL;
        RetMsg(msg, link, text, subj);
        /* preserve original message text */
        msg->text= msg_text;

	nfree (subj);
	/* nfree (text); don't free text because RetMsg() free it */

	fileName[strlen(fileName)-1] = nrul+'1';
    }

    if (nrul==0) { /*  couldn't open any rules file while first one exists! */
	w_log(LL_ERR, "areafix: can't open file '%s' for reading: %s", fileName, strerror(errno));
    }
    nfree (fileName);

}

void sendAreafixMessages()
{
    s_link *link = NULL;
    s_message *linkmsg;
    unsigned int i;

    for (i = 0; i < config->linkCount; i++) {
        if (config->links[i]->msg == NULL) continue;
        link = config->links[i];
        linkmsg = link->msg;

        xscatprintf(&(linkmsg->text), " \r--- %s areafix\r", versionStr);
        linkmsg->textLength = strlen(linkmsg->text);

        w_log(LL_AREAFIX, "areafix: write netmail msg for %s", aka2str(link->hisAka));

        processNMMsg(linkmsg, NULL, getRobotsArea(config),
            0, MSGLOCAL);
        writeEchoTossLogEntry(getRobotsArea(config)->areaName);
        closeOpenedPkt();
        freeMsgBuffers(linkmsg);
        nfree(linkmsg);
        link->msg = NULL;
    }
}

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

int processAreaFix(s_message *msg, s_pktHeader *pktHeader, unsigned force_pwd)
{
    unsigned int security=1, notforme = 0;
    s_link *link = NULL;
    s_link *tmplink = NULL;
    /* s_message *linkmsg; */
    s_pktHeader header;
    char *token = NULL, *report = NULL, *preport = NULL;
    char *textBuff = NULL,*tmp;
    int nr;

    w_log(LL_FUNC, __FILE__ "::processAreaFix()");

    RetFix = NOTHING;

    /*  1st security check */
    if (pktHeader) security=addrComp(msg->origAddr, pktHeader->origAddr);
    else {
	makePktHeader(NULL, &header);
	pktHeader = &header;
	pktHeader->origAddr = msg->origAddr;
	pktHeader->destAddr = msg->destAddr;
	security = 0;
    }

    if (security) security=1; /* different pkt and msg addresses */

    /*  find link */
    link=getLinkFromAddr(config, msg->origAddr);

    /*  if keyword allowPktAddrDiffer for this link is on, */
    /*  we allow the addresses in PKT and MSG header differ */
    if (link!=NULL)
	if (link->allowPktAddrDiffer == pdOn)
	    security = 0;  /* OK */

    /*  is this for me? */
    if (link!=NULL)	notforme=addrComp(msg->destAddr, *link->ourAka);
    else if (!security) security=4; /*  link == NULL; unknown system */
	
    if (notforme && !security) security=5; /*  message to wrong AKA */

#if 0 /*  we're process only our messages here */
    /*  ignore msg for other link (maybe this is transit...) */
    if (notforme || (link==NULL && security==1)) {
        w_log(LL_FUNC, __FILE__ "::processAreaFix() call processNMMsg() and return");
	nr = processNMMsg(msg, pktHeader, NULL, 0, 0);
	closeOpenedPkt();
	return nr;
    }
#endif

    /*  2nd security check. link, areafixing & password. */
    if (!security && !force_pwd) {
        if (link->AreaFix==1) {
            if (link->areaFixPwd!=NULL) {
                if (stricmp(link->areaFixPwd,msg->subjectLine)==0) security=0;
                else security=3; /* password error */
            }
        } else security=2; /* areafix is turned off */
    }

    /*  remove kluges */
    tmp = msg->text;
    token = strseparate (&tmp,"\n\r");

    while(token != NULL) {
        if( !strcmp(token,"---") || !strncmp(token,"--- ",4) )
            /*  stop on tearline ("---" or "--- text") */
            break;
        if( token[0] != '\001' )
            xstrscat(&textBuff,token,"\r",NULL);
        token = strseparate (&tmp,"\n\r");
    }
    nfree(msg->text);
    msg->text = textBuff;
#ifdef DO_PERL
    if ( perl_afixreq(msg, pktHeader->origAddr) )
        link = getLinkFromAddr(config, msg->origAddr);
#endif
    if (!security) {
	textBuff = safe_strdup(msg->text);
        tmp = textBuff;
	token = strseparate (&tmp, "\n\r");
	while(token != NULL) {
	    while ((*token == ' ') || (*token == '\t')) token++;
	    while(isspace(token[strlen(token)-1])) token[strlen(token)-1]='\0';
            w_log(LL_AREAFIX, "Process command: %s", token);
	    preport = processcmd( link, token, tellcmd (token) );
	    if (preport != NULL) {
		switch (RetFix) {
		case LIST:
		    RetMsg(msg, link, preport, "Areafix reply: list request");
		    break;
		case HELP:
		    RetMsg(msg, link, preport, "Areafix reply: help request");
		    break;
		case ADD:
		    report = areaStatus(report, preport);
		    if (rescanMode) {
			preport = processcmd( link, token, RESCAN );
			if (preport != NULL)
			    report = areaStatus(report, preport);
		    }
		    break;
		case AVAIL:
		    RetMsg(msg, link, preport, "Areafix reply: available areas");
		    break;
		case UNLINKED:
		    RetMsg(msg, link, preport, "Areafix reply: unlinked request");
		    break;
		case QUERY:
		    RetMsg(msg, link, preport, "Areafix reply: linked request");
		    break;
		case PAUSE:
		    RetMsg(msg, link, preport, "Areafix reply: pause request");
		    break;
		case RESUME:
		    RetMsg(msg, link, preport, "Areafix reply: resume request");
		    break;
		case INFO:
		    RetMsg(msg, link, preport, "Areafix reply: link information");
		    break;
		case PACKER:
		    RetMsg(msg, link, preport, "Areafix reply: packer change request");
		    break;
		case RSB:
		    RetMsg(msg, link, preport, "Areafix reply: reduced seen-by change request");
		    break;
		case RULES:
		    RetMsg(msg, link, preport, "Areafix reply: send rules change request");
		    break;
		case PKTSIZE:
		    RetMsg(msg, link, preport, "Areafix reply: pkt size change request");
		    break;
		case ARCMAILSIZE:
		    RetMsg(msg, link, preport, "Areafix reply: arcmail size change request");
		    break;
		case STAT:
		    report = areaStatus(report, preport);
		    break;
		default:
		    w_log(LL_ERR,"Unknown areafix command:%s", token);
		    break;
		}
	    } /* end if (preport != NULL) */
	    token = strseparate (&tmp, "\n\r");
	    if (RetFix==DONE) token=NULL;
	} /* end while (token != NULL) */
    nfree(textBuff);
    } else {
	if (link == NULL) {
	    tmplink = (s_link*) safe_malloc(sizeof(s_link));
	    memset(tmplink, '\0', sizeof(s_link));
	    tmplink->ourAka = &(msg->destAddr);
	    tmplink->hisAka.zone = msg->origAddr.zone;
	    tmplink->hisAka.net = msg->origAddr.net;
	    tmplink->hisAka.node = msg->origAddr.node;
	    tmplink->hisAka.point = msg->origAddr.point;
	    link = tmplink;
	}
	/*  security problem */
		
	switch (security) {
	case 1:
	    xscatprintf(&report, " \r different pkt and msg addresses\r");
	    break;
	case 2:
	    xscatprintf(&report, " \r areafix is turned off\r");
	    break;
	case 3:
	    xscatprintf(&report, " \r password error\r");
	    break;
	case 4:
	    xscatprintf(&report, " \r your system is unknown\r");
	    break;
	case 5:
	    xscatprintf(&report, " \r message sent to wrong AKA\r");
	    break;
	default:
	    xscatprintf(&report, " \r unknown error. mail to sysop.\r");
	    break;
	}

	RetMsg(msg, link, report, "Areafix reply: security violation");
	w_log(LL_AREAFIX, "areafix: security violation from %s", aka2str(link->hisAka));
	nfree(tmplink);
        w_log(LL_FUNC, __FILE__ ":%u:processAreaFix() rc=1", __LINE__);
	return 1;
    }

    if ( report != NULL ) {
        if (config->areafixQueryReports) {
            preport = list (lt_linked, link, NULL);/*linked (link);*/
            xstrcat(&report, preport);
            nfree(preport);
        }
        RetMsg(msg, link, report, "Areafix reply: node change request");
    }

    if (rulesCount) {
        for (nr=0; nr < rulesCount; nr++) {
            if (rulesList && rulesList[nr]) {
                RetRules (msg, link, rulesList[nr]);
                nfree (rulesList[nr]);
            }
        }
        nfree (rulesList);
        rulesCount=0;
    }

    w_log(LL_AREAFIX, "Areafix: successfully done for %s",aka2str(link->hisAka));

    /*  send msg to the links (forward requests to areafix) */
    sendAreafixMessages();
#ifdef DO_PERL
    /* val: update perl structures */
    perl_setvars();
#endif
    w_log(LL_FUNC, __FILE__ "::processAreaFix() end (rc=1)");
    return 1;
}

void MsgToStruct(HMSG SQmsg, XMSG xmsg, s_message *msg)
{
    /*  convert header */
    msg->attributes  = xmsg.attr;

    msg->origAddr.zone  = xmsg.orig.zone;
    msg->origAddr.net   = xmsg.orig.net;
    msg->origAddr.node  = xmsg.orig.node;
    msg->origAddr.point = xmsg.orig.point;

    msg->destAddr.zone  = xmsg.dest.zone;
    msg->destAddr.net   = xmsg.dest.net;
    msg->destAddr.node  = xmsg.dest.node;
    msg->destAddr.point = xmsg.dest.point;

    strcpy((char *)msg->datetime, (char *) xmsg.__ftsc_date);
    xstrcat(&(msg->subjectLine), (char *) xmsg.subj);
    xstrcat(&(msg->toUserName), (char *) xmsg.to);
    xstrcat(&(msg->fromUserName), (char *) xmsg.from);

    msg->textLength = MsgGetTextLen(SQmsg);
    msg->ctlLength = MsgGetCtrlLen(SQmsg);
    xstralloc(&(msg->text),msg->textLength+1);
    xstralloc(&(msg->ctl),msg->ctlLength+1);
    MsgReadMsg(SQmsg, NULL, 0, msg->textLength, (unsigned char *) msg->text, msg->ctlLength, msg->ctl);
    msg->text[msg->textLength] = '\0';
    msg->ctl[msg->ctlLength] = '\0';

}

void afix(hs_addr addr, char *cmd)
{
    HAREA           netmail;
    HMSG            SQmsg;
    unsigned long   highmsg, i;
    XMSG            xmsg;
    hs_addr         orig, dest;
    s_message	    msg, *tmpmsg;
    int             k, startarea = 0, endarea = config->netMailAreaCount;
    s_area          *area;
    char            *name = config->robotsArea;
    s_link          *link;

    w_log(LL_FUNC, __FILE__ "::afix() begin");
    w_log(LL_INFO, "Start AreaFix...");

    if ((area = getNetMailArea(config, name)) != NULL) {
        startarea = area - config->netMailAreas;
        endarea = startarea + 1;
    }

    if (cmd) {
        link = getLinkFromAddr(config, addr);
        if (link) {
          if (cmd && strlen(cmd)) {
            tmpmsg = makeMessage(&addr, link->ourAka, link->name,
                link->RemoteRobotName ?
                link->RemoteRobotName : "Areafix",
                link->areaFixPwd ?
                link->areaFixPwd : "", 1,
                link->areafixReportsAttr ? link->areafixReportsAttr : config->areafixReportsAttr);
            tmpmsg->text = safe_strdup(cmd);
            processAreaFix(tmpmsg, NULL, 1);
            freeMsgBuffers(tmpmsg);
	  } else w_log(LL_WARN, "areafix: empty areafix command from %s", aka2str(addr));
        } else w_log(LL_ERR, "areafix: no such link in config: %s!", aka2str(addr));
    }
    else for (k = startarea; k < endarea; k++) {

        netmail = MsgOpenArea((unsigned char *) config->netMailAreas[k].fileName,
            MSGAREA_NORMAL,
            /*config -> netMailArea.fperm,
            config -> netMailArea.uid,
            config -> netMailArea.gid,*/
            (word)config -> netMailAreas[k].msgbType);

        if (netmail != NULL) {

            highmsg = MsgGetHighMsg(netmail);
            w_log(LL_INFO,"Scanning %s",config->netMailAreas[k].areaName);

            /*  scan all Messages and test if they are already sent. */
            for (i=1; i<= highmsg; i++) {
                SQmsg = MsgOpenMsg(netmail, MOPEN_RW, i);

                /*  msg does not exist */
                if (SQmsg == NULL) continue;

                MsgReadMsg(SQmsg, &xmsg, 0, 0, NULL, 0, NULL);
                cvtAddr(xmsg.orig, &orig);
                cvtAddr(xmsg.dest, &dest);
                w_log(LL_DEBUG, "Reading msg %lu from %s -> %s", i,
                      aka2str(orig), aka2str(dest));

                /*  if not read and for us -> process AreaFix */
                striptwhite((char*)xmsg.to);
                if ((xmsg.attr & MSGREAD) == MSGREAD) {
                    w_log(LL_DEBUG, "Message is already read, skipping");
                    MsgCloseMsg(SQmsg);
                    continue;
                }
                if (!isOurAka(config,dest)) {
                    w_log(LL_DEBUG, "Message is not to us, skipping");
                    MsgCloseMsg(SQmsg);
                    continue;
                }
                if ((strlen((char*)xmsg.to)>0) &&
                    fc_stristr(config->areafixNames,(char*)xmsg.to))
                {
                    memset(&msg,'\0',sizeof(s_message));
                    MsgToStruct(SQmsg, xmsg, &msg);
                    processAreaFix(&msg, NULL, 0);
                    if (config->areafixKillRequests) {
                        MsgCloseMsg(SQmsg);
                        MsgKillMsg(netmail, i--);
                    } else {
                        xmsg.attr |= MSGREAD;
                        MsgWriteMsg(SQmsg, 0, &xmsg, NULL, 0, 0, 0, NULL);
                        MsgCloseMsg(SQmsg);
                    }
                    freeMsgBuffers(&msg);
                }
                else
                {
                    w_log(LL_DEBUG, "Message is not to AreaFix, skipping");
                    MsgCloseMsg(SQmsg);
                }
            }

            MsgCloseArea(netmail);
        } else {
            w_log(LL_ERR, "Could not open %s", config->netMailAreas[k].areaName);
        }
    }
    w_log(LL_FUNC, __FILE__ "::afix() end");
}

void autoPassive()
{
  time_t   time_cur, time_test;
  struct   stat stat_file;
  s_message *msg;
  FILE *f;
  char *line, *path;
  unsigned int i;

  for (i = 0; i < config->linkCount; i++) {

      if (config->links[i]->autoPause==0 || (config->links[i]->Pause == (ECHOAREA|FILEAREA))
         ) continue;

      if (createOutboundFileName(config->links[i],
				 config->links[i]->echoMailFlavour,
				 FLOFILE) == 0) {
	  f = fopen(config->links[i]->floFile, "rt");
	  if (f) {
	      while ((line = readLine(f)) != NULL) {
		  line = trimLine(line);
		  path = line;
		  if (!isArcMail(path)) {
		      nfree(line);
		      continue;
		  }
		  if (*path && (*path == '^' || *path == '#')) {
		      path++;
		      /*  set Pause if files stored only in outbound */
		      if (*path && strncmp(config->outbound,path,strlen(config->outbound)-1)==0 && stat(path, &stat_file) != -1) {

			  time_cur = time(NULL);
			  if (time_cur > stat_file.st_mtime) {
			      time_test = (time_cur - stat_file.st_mtime)/3600;
			  } else { /*  buggly time on file, anyway don't autopause on it */
			      time_test = 0;
			  }

			  if (time_test >= (time_t)(config->links[i]->autoPause*24)) {
			      w_log(LL_AREAFIX, "autopause: the file %s is %d days old", path, time_test/24);
			      if (Changepause((cfgFile) ? cfgFile :
					      getConfigFileName(),
					      config->links[i], 1,
					      config->links[i]->Pause^(ECHOAREA|FILEAREA))) {
				  int mask = config->links[i]->areafixReportsAttr ? config->links[i]->areafixReportsAttr : config->areafixReportsAttr;
				  msg = makeMessage(config->links[i]->ourAka,
					    &(config->links[i]->hisAka),
					    config->areafixFromName ? config->areafixFromName : versionStr,
					    config->links[i]->name,
					    "AutoPassive", 1,
                                            MSGPRIVATE | MSGLOCAL | (mask & (MSGKILL|MSGCPT)) );
				  msg->text = createKludges(config, NULL,
					    config->links[i]->ourAka,
					    &(config->links[i]->hisAka),
					    versionStr);
				  xstrcat(&msg->text, "\r System switched to passive, your subscription are paused.\r\r"
					" You are being unsubscribed from echo areas with no downlinks besides you!\r\r"
					" When you wish to continue receiving echomail, please send requests\r"
					" to AreaFix containing the %RESUME command.");
				  xscatprintf(&msg->text, "\r\r--- %s autopause\r", versionStr);
				  msg->textLength = strlen(msg->text);
				  processNMMsg(msg, NULL,
					       getRobotsArea(config),
                               0, MSGLOCAL);
                  writeEchoTossLogEntry(getRobotsArea(config)->areaName);
				  closeOpenedPkt();
				  freeMsgBuffers(msg);
				  nfree(msg);

				  /* pause areas with one link alive while others are paused */
				  if (config->autoAreaPause)
                                      pauseAreas(0,config->links[i],NULL);

			      } /*  end changepause */
			      nfree(line);
			      /* fclose(f); file closed after endwhile */
			      break;
			  }
		      } /* endif */
		  } /* endif ^# */
		  nfree(line);
	      } /* endwhile */
	      fclose(f);
	  } /* endif */
	  nfree(config->links[i]->floFile);
	  remove(config->links[i]->bsyFile);
	  nfree(config->links[i]->bsyFile);
      }
      nfree(config->links[i]->pktFile);
      nfree(config->links[i]->packFile);
  } /* endfor */
}

int relink (char *straddr) {
    s_link          *researchLink = NULL;
    unsigned int    count, areasArraySize;
    s_area          **areasIndexArray = NULL;
    struct _minf m;

    /*  parse config */
    if (config==NULL) processConfig();
    if ( initSMAPI == -1 ) {
	/*  init SMAPI */
	initSMAPI = 0;
	m.req_version = 0;
	m.def_zone = (UINT16) config->addr[0].zone;
	if (MsgOpenApi(&m) != 0) {
	    exit_hpt("MsgApiOpen Error",1);
	}
    }

    w_log(LL_START, "Start relink...");

    if (straddr) researchLink = getLink(config, straddr);
    else {
	w_log(LL_ERR, "No address");
	return 1;
    }

    if ( researchLink == NULL ) {
	w_log(LL_ERR, "Unknown link address %s", straddr);
	return 1;
    }

    areasArraySize = 0;
    areasIndexArray = (s_area **) safe_malloc
	(sizeof(s_area *) * (config->echoAreaCount + config->localAreaCount + 1));

    for (count = 0; count < config->echoAreaCount; count++)
	if ( isLinkOfArea(researchLink, &config->echoAreas[count])) {
	    areasIndexArray[areasArraySize] = &config->echoAreas[count];
	    areasArraySize++;
	    w_log(LL_AREAFIX, "EchoArea %s from link %s is relinked",
		  config->echoAreas[count].areaName, aka2str(researchLink->hisAka));
	}

    if ( areasArraySize > 0 ) {
	s_message *msg;

	msg = makeMessage(researchLink->ourAka,
			  &researchLink->hisAka,
			  config->sysop,
			  researchLink->RemoteRobotName ?
			  researchLink->RemoteRobotName : "areafix",
			  researchLink->areaFixPwd ? researchLink->areaFixPwd : "", 1,
                          researchLink->areafixReportsAttr ? researchLink->areafixReportsAttr : config->areafixReportsAttr);

	msg->text = createKludges(config,NULL,researchLink->ourAka,
                              &researchLink->hisAka,versionStr);
        if (researchLink->areafixReportsFlags)
            xstrscat(&(msg->text), "\001FLAGS ", researchLink->areafixReportsFlags, "\r",NULL);
        else if (config->areafixReportsFlags)
	    xstrscat(&(msg->text), "\001FLAGS ", config->areafixReportsFlags, "\r",NULL);

	for ( count = 0 ; count < areasArraySize; count++ ) {
	    if ((areasIndexArray[count]->downlinkCount  <= 1) &&
		(areasIndexArray[count]->msgbType & MSGTYPE_PASSTHROUGH))
		xscatprintf(&(msg->text), "-%s\r",areasIndexArray[count]->areaName);
	    else
		xscatprintf(&(msg->text), "+%s\r",areasIndexArray[count]->areaName);
	}

	xscatprintf(&(msg->text), " \r--- %s areafix\r", versionStr);
	msg->textLength = strlen(msg->text);
	w_log(LL_AREAFIX, "'Relink' message created to `%s`",
	      researchLink->RemoteRobotName ?
	      researchLink->RemoteRobotName : "areafix");
	processNMMsg(msg, NULL,
		     getRobotsArea(config),
                 1, MSGLOCAL|MSGKILL);
    writeEchoTossLogEntry(getRobotsArea(config)->areaName);
	closeOpenedPkt();
	freeMsgBuffers(msg);
	nfree(msg);
	w_log(LL_AREAFIX, "Relinked %i area(s)",areasArraySize);
    }

    nfree(areasIndexArray);

    /* deinit SMAPI */
    MsgCloseApi();

    return 0;
}

int resubscribe (char *pattern, char *strFromAddr, char *strToAddr) {
    s_link          *fromLink = NULL;
    s_link          *toLink = NULL;
    unsigned int    count, fromArraySize, toArraySize;
    s_area          **fromIndexArray = NULL;
    s_area          **toIndexArray = NULL;
    struct _minf    m;
    char            *fromAddr, *toAddr;

    /*  parse config */
    if (config==NULL) processConfig();
    if ( initSMAPI == -1 ) {
        /*  init SMAPI */
        initSMAPI = 0;
        m.req_version = 0;
        m.def_zone = (UINT16) config->addr[0].zone;
        if (MsgOpenApi(&m) != 0) {
            exit_hpt("MsgApiOpen Error",1);
        }
    }

    w_log(LL_START, "Start resubscribe...");

    if ( pattern == NULL ) {
        w_log(LL_ERR, "Areas pattern is not defined");
        return 1;
    }

    if (strFromAddr) fromLink = getLink(config, strFromAddr);
    else {
        w_log(LL_ERR, "No address to resubscribe from");
        return 1;
    }

    if ( fromLink == NULL ) {
        w_log(LL_ERR, "Unknown link address %s", strFromAddr);
        return 1;
    }

    fromArraySize = 0;
    fromIndexArray = (s_area **) safe_malloc
        (sizeof(s_area *) * (config->echoAreaCount + config->localAreaCount + 1));

    if (strToAddr) toLink = getLink(config, strToAddr);
    else {
        w_log(LL_ERR, "No address to resubscribe to");
        return 1;
    }

    if ( toLink == NULL ) {
        w_log(LL_ERR, "Unknown link address %s", strToAddr);
        return 1;
    }

    toArraySize = 0;
    toIndexArray = (s_area **) safe_malloc
        (sizeof(s_area *) * (config->echoAreaCount + config->localAreaCount + 1));

    for (count = 0; count < config->echoAreaCount; count++)
        if (isLinkOfArea(fromLink, &config->echoAreas[count])) {
            int rc;

            if(patimat(config->echoAreas[count].areaName, pattern)==0)
                continue;

            rc = changeconfig(cfgFile?cfgFile:getConfigFileName(),
                              &config->echoAreas[count],fromLink,1);

            if (rc != DEL_OK) {
                w_log(LL_AREAFIX, "areafix: %s can't unlink %s from area ",
                      aka2str(fromLink->hisAka), config->echoAreas[count].areaName);
                continue;
            }

            fromIndexArray[fromArraySize] = &config->echoAreas[count];
            fromArraySize++;
            RemoveLink(fromLink, &config->echoAreas[count]);

            if (isLinkOfArea(toLink, &config->echoAreas[count])) {
                w_log(LL_AREAFIX, "Link %s is already subscribed to area %s",
                      aka2str(toLink->hisAka), config->echoAreas[count].areaName);
                continue;
            }

            rc = changeconfig(cfgFile?cfgFile:getConfigFileName(),
                              &config->echoAreas[count],toLink,0);

            if (rc != ADD_OK) {
                w_log(LL_AREAFIX, "areafix: %s is not subscribed to %s",
                      aka2str(toLink->hisAka), config->echoAreas[count].areaName);
                continue;
            }

            Addlink(config, toLink, &config->echoAreas[count]);
            toIndexArray[toArraySize] = &config->echoAreas[count];
            toArraySize++;

            fromAddr = safe_strdup(aka2str(fromLink->hisAka));
            toAddr   = safe_strdup(aka2str(toLink->hisAka));
            w_log(LL_AREAFIX, "EchoArea %s resubscribed from link %s (old link was %s)",
                  config->echoAreas[count].areaName, fromAddr, toAddr);
            nfree(fromAddr);
            nfree(toAddr);
        }

    if ( fromArraySize > 0 ) {
        s_message *msg;

        msg = makeMessage(fromLink->ourAka,
                          &fromLink->hisAka,
                          config->sysop,
                          fromLink->RemoteRobotName ?
                          fromLink->RemoteRobotName : "areafix",
                          fromLink->areaFixPwd ? fromLink->areaFixPwd : "", 1,
                          fromLink->areafixReportsAttr ? fromLink->areafixReportsAttr : config->areafixReportsAttr);

        msg->text = createKludges(config,NULL,fromLink->ourAka,
                                  &fromLink->hisAka,versionStr);
        if (fromLink->areafixReportsFlags)
            xstrscat(&(msg->text), "\001FLAGS ", fromLink->areafixReportsFlags, "\r",NULL);
        else if (config->areafixReportsFlags)
            xstrscat(&(msg->text), "\001FLAGS ", config->areafixReportsFlags, "\r",NULL);

        for ( count = 0 ; count < fromArraySize; count++ ) {
            if (toLink!=NULL) {
                xscatprintf(&(msg->text), "-%s\r",fromIndexArray[count]->areaName);
            } else {
                xscatprintf(&(msg->text), "+%s\r",fromIndexArray[count]->areaName);
            }
        }

        xscatprintf(&(msg->text), " \r--- %s areafix\r", versionStr);
        msg->textLength = strlen(msg->text);
        processNMMsg(msg, NULL, getRobotsArea(config), 1, MSGLOCAL|MSGKILL);
        writeEchoTossLogEntry(getRobotsArea(config)->areaName);
        closeOpenedPkt();
        freeMsgBuffers(msg);
        nfree(msg);

        w_log(LL_AREAFIX, "Unlinked %i area(s) from %s",
              fromArraySize, aka2str(fromLink->hisAka));
    }

    nfree(fromIndexArray);

    if (toArraySize > 0) {
        s_message *msg;

        msg = makeMessage(toLink->ourAka,
                          &toLink->hisAka,
                          config->sysop,
                          toLink->RemoteRobotName ?
                          toLink->RemoteRobotName : "areafix",
                          toLink->areaFixPwd ? toLink->areaFixPwd : "", 1,
                          toLink->areafixReportsAttr ? toLink->areafixReportsAttr : config->areafixReportsAttr);

        msg->text = createKludges(config,NULL,toLink->ourAka,
                                  &toLink->hisAka,versionStr);
        if (toLink->areafixReportsFlags)
            xstrscat(&(msg->text), "\001FLAGS ", toLink->areafixReportsFlags, "\r",NULL);
        else if (config->areafixReportsFlags)
            xstrscat(&(msg->text), "\001FLAGS ", config->areafixReportsFlags, "\r",NULL);

        for ( count = 0 ; count < toArraySize; count++ ) {
            xscatprintf(&(msg->text), "+%s\r",toIndexArray[count]->areaName);
        }

        xscatprintf(&(msg->text), " \r--- %s areafix\r", versionStr);
        msg->textLength = strlen(msg->text);
        processNMMsg(msg, NULL, getRobotsArea(config), 1, MSGLOCAL|MSGKILL);
        writeEchoTossLogEntry(getRobotsArea(config)->areaName);
        closeOpenedPkt();
        freeMsgBuffers(msg);
        nfree(msg);
        w_log(LL_AREAFIX, "Linked %i area(s) to %s",
              toArraySize, aka2str(toLink->hisAka));
    }

    nfree(toIndexArray);

    /* deinit SMAPI */
    MsgCloseApi();

    return 0;
}
