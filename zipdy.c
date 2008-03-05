/* zipdy.c
 * Copyright (C) 2000, 2001 V. Alex Brennen
 *
 * This file is part of Zipdy.
 *
 * Zipdy is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Zipdy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/*
	Version:        2.0.0
	Description:	This is a simple CGI program to calculate
			the distance between two zip codes.
	Project:	Zipdy: Zip code distance calculator
        Home:           http://www.cryptnet.net/fsp/zipdy/
        Author:         V. Alex Brennen <vab@cryptnet.net>
                        http://www.cryptnet.net/people/vab/
*/ 

#include <stdio.h>
#include <math.h>
#include <gdbm.h>
#include <string.h>

/* Radius of the earth in miles */
#define EARTH_RADIUS 3956

float great_circle_distance(float lat1,float long1,float lat2,float long2);
float deg_to_rad(float deg);
void  do_error_page(char *error);

int main(void)
{
        GDBM_FILE       dbf;
        int             result;

        datum           data;
        datum           info;
	datum		data2;
	datum		info2;

	float		lat1;
	float		lon1;
	float		lat2;
	float		lon2;

	float		distance;

	/* For parsing post */
	char		*method;
	char		*content;
	int		content_length = 0;
	char		*nvpair1;
	char		*nvpair2;
	char		*zip_1;
	char		*zip_2;

	method = (char *)getenv("REQUEST_METHOD");

	if(strcmp(method,"POST") != 0)
	{
		do_error_page("Only method POST is supported.");
	}
	
	content_length = atoi(getenv("CONTENT_LENGTH"));

	if(content_length > 100)
	{
		do_error_page("Content Length expectation exceeded.");
	}

	content = (char *)malloc(content_length+1);
	if(content == NULL)
	{	
		do_error_page("Server was unable to malloc memory.  Server out of memory.");
	}

	fread(content,1,content_length,stdin);

	nvpair1 = strtok(content,"&");
	nvpair2 = strtok('\0',"&");
        zip_1 = strtok(nvpair1,"=");
        zip_1 = strtok('\0',"="); 
	zip_2 = strtok(nvpair2,"=");
	zip_2 = strtok('\0',"=");

	if(strlen(zip_1) != 5)
	{
		do_error_page("Zip code #1 does not appear to be a valid US zip code.");
	}
	
	if(strlen(zip_2) != 5)
	{
		do_error_page("Zip code #2 does not appear to be a valid US zip code.");
	}

        if((dbf = gdbm_open("zips_gdbm",1024,GDBM_READER, 0755, 0)) ==NULL)
	{
		fprintf(stderr, "Unable to open gdbm data file.\n");
		exit(1);
	}

        data.dptr = zip_1;
        data.dsize = 5;
        info = gdbm_fetch(dbf,data);
	if(info.dptr == NULL)
	{
		do_error_page("Zip code #1 was not found in the data base.\n");
	}
	sscanf(info.dptr,"%f%f",&lon1, &lat1);

        free(info.dptr);

	data2.dptr = zip_2;
	data2.dsize = 5;
	info2 = gdbm_fetch(dbf,data2);
	if(info2.dptr == NULL)
	{
		do_error_page("Zip code #2 was not found in the data base.\n");
	}
        sscanf(info2.dptr,"%f%f",&lon2,&lat2);

        free(info2.dptr);

	gdbm_close(dbf);

	distance = great_circle_distance(lat1,lon1,lat2,lon2);	
	
	printf("Content-Type: text/html\n\n");
	printf("<HTML><HEAD><TITLE>Zipdy Results</TITLE>\n");
	printf("<BODY BGCOLOR=#FFFFFF>\n");
	printf("The distance between %s and %s is: %f.\n", zip_1, zip_2, distance);
	printf("</BODY></HTML>\n");

	free(content);

        return 0;
}

void do_error_page(char *error)
{
	printf("Content-Type: text/html\n\n");
	printf("<HTML><HEAD><TITLE>Zipdy Results</TITLE></HEAD>\n");
	printf("<BODY BGCOLOR=#FFFFFF>\n");
	printf("<H3>Error.</H3>\n");
	printf("%s\n",error);
	printf("</BODY></HTML>\n");
	
	exit(0);
}

float great_circle_distance(float lat1,float long1,float lat2,float long2)
{
        float delta_long = 0;
        float delta_lat = 0;
        float temp = 0;
        float distance = 0;

        /* Convert all the degrees to radians */
        lat1 = deg_to_rad(lat1);
        long1 = deg_to_rad(long1);
        lat2 = deg_to_rad(lat2);
        long2 = deg_to_rad(long2);

        /* Find the deltas */
        delta_lat = lat2 - lat1;
        delta_long = long2 - long1;

        /* Find the GC distance */
        temp = pow(sin(delta_lat / 2.0), 2) + cos(lat1) * cos(lat2) * pow(sin(delta_long / 2.0), 2);

        distance = EARTH_RADIUS * 2 * atan2(sqrt(temp),sqrt(1 - temp));

        return (distance);
}

float deg_to_rad(float deg)
{
        double radians = 0;

        radians = deg * M_PI / 180.0;

        return(radians);
}
