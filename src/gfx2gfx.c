/* pdf2swf.c
   main routine for pdf2swf(1)

   Part of the swftools package.
   
   Copyright (c) 2001,2002,2003 Matthias Kramm <kramm@quiss.org> 
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "../../swftools/config.h"
#include "../../swftools/lib/args.h"
#include "../../swftools/lib/os.h"
#include "../../swftools/lib/gfxsource.h"
#include "../../swftools/lib/gfxdevice.h"
#include "../../swftools/lib/gfxpoly.h"
#include "../../swftools/lib/devices/bbox.h"
#include "../../swftools/lib/devices/lrf.h"
#include "../../swftools/lib/devices/ocr.h"
#include "../../swftools/lib/devices/rescale.h"
#include "../../swftools/lib/devices/record.h"
#include "../../swftools/lib/readers/image.h"
#include "../../swftools/lib/readers/swf.h"
#include "../../swftools/lib/pdf/pdf.h"
#include "../../swftools/lib/log.h"

gfxsource_t*driver;

static char * outputname = 0;
static int loglevel = 3;
static char * pagerange = 0;
static char * filename = 0;

int args_callback_option(char*name,char*val) {
    if (!strcmp(name, "o"))
    {
	outputname = val;
	return 1;
    }
    else if (!strcmp(name, "v"))
    {
	loglevel ++;
        setConsoleLogging(loglevel);
	return 0;
    }
    else if (!strcmp(name, "q"))
    {
	loglevel --;
        setConsoleLogging(loglevel);
	return 0;
    }
    else if (name[0]=='p')
    {
	do {
	    name++;
	} while(*name == 32 || *name == 13 || *name == 10 || *name == '\t');

	if(*name) {
	    pagerange = name;
	    return 0;
	} 
	pagerange = val;	
	return 1;
    }
    else if (!strcmp(name, "s"))
    {
	const char*s = strdup(val);
	char*c = strchr(s, '=');
	if(c && *c && c[1])  {
	    *c = 0;
	    c++;
	    driver->set_parameter(driver, s,c);
	}
	else
	    driver->set_parameter(driver, s,"1");
	return 1;
    }
    else if (!strcmp(name, "V"))
    {	
	printf("pdf2swf - part of %s %s\n", PACKAGE, VERSION);
	exit(0);
    }
    else 
    {
	fprintf(stderr, "Unknown option: -%s\n", name);
	exit(1);
    }
    return 0;
}

struct options_t options[] =
{{"o","output"},
 {"q","quiet"},
 {"V","version"},
 {"s","set"},
 {"p","pages"},
 {0,0}
};

int args_callback_longoption(char*name,char*val) {
    return args_long2shortoption(options, name, val);
}

int args_callback_command(char*name, char*val) {
    if (!filename) 
        filename = name;
    else {
	if(outputname)
	{
	     fprintf(stderr, "Error: Do you want the output to go to %s or to %s?", 
		     outputname, name);
	     exit(1);
	}
	outputname = name;
    }
    return 0;
}

void args_callback_usage(char*name)
{
}

int main(int argn, char *argv[])
{
    processargs(argn, argv);
    initLog(0,-1,0,0,-1,loglevel);
    
    gfxsource_t*source = 0;
    
    if(!filename) {
	fprintf(stderr, "Please specify an input file\n");
	exit(1);
    }
    
    if(strstr(filename, ".pdf") || strstr(filename, ".PDF")) {
        msg("<notice> Treating file as PDF");
        source = gfxsource_pdf_create();
    } else if(strstr(filename, ".swf") || strstr(filename, ".SWF")) {
        msg("<notice> Treating file as SWF");
        source = gfxsource_swf_create();
    } else if(strstr(filename, ".jpg") || strstr(filename, ".JPG") ||
              strstr(filename, ".png") || strstr(filename, ".PNG")) {
        msg("<notice> Treating file as Image");
        source = gfxsource_image_create();
    }

    if(!outputname)
    {
	if(filename) {
	    outputname = stripFilename(filename, ".out");
	    msg("<notice> Output filename not given. Writing to %s", outputname);
	} 
    }
    if(!outputname)
    {
	fprintf(stderr, "Please use -o to specify an output file\n");
	exit(1);
    }
    printf("%s\n", filename);
    is_in_range(0x7fffffff, pagerange);
    if(pagerange)
	source->set_parameter(source, "pages", pagerange);

    if(!filename) {
	args_callback_usage(argv[0]);
	exit(0);
    }

    gfxdocument_t* doc = source->open(source, filename);
    if(!doc) {
        msg("<error> Couldn't open %s", filename);
        exit(1);
    }

    gfxdevice_t lrf;
    gfxdevice_lrf_init(&lrf);

    gfxdevice_t rescale;
    gfxdevice_rescale_init(&rescale, &lrf, 592, 732, 0);

    gfxdevice_t*out = &rescale;
    out->setparameter(out, "keepratio", "1");
    out->setparameter(out, "pagepattern", outputname);

    gfxdevice_t bbox2,*bbox=&bbox2;
    gfxdevice_bbox_init(bbox);
    bbox->setparameter(bbox, "graphics", "0");

    int pagenr;

    for(pagenr = 1; pagenr <= doc->num_pages; pagenr++) 
    {
	if(is_in_range(pagenr, pagerange)) {
	    gfxpage_t* page = doc->getpage(doc, pagenr);
	    bbox->startpage(bbox,-1,-1);
	    page->render(page, bbox);
	    gfxbbox_t b = gfxdevice_bbox_getbbox(bbox);

	    out->startpage(out, b.xmax-b.xmin, b.ymax-b.ymin);
	    page->rendersection(page, out, -b.xmin, -b.ymin, 0,0,b.xmax-b.xmin,b.ymax-b.ymin);
	    out->endpage(out);

	    page->destroy(page);
	}
    }

    gfxresult_t*result = out->finish(out);

    if(result) {
	if(result->save(result, outputname) < 0) {
	    exit(1);
	}
	result->destroy(result);
    }

    doc->destroy(doc);

    return 0;
}
