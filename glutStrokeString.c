/*
 *  glutStrokeString.c
 *  proj
 *
 *  Created by bolson on Fri Mar 08 2002.
 *  Copyright (c) 2001 Brian Olson. All rights reserved.
 *
 */

#include <Carbon/Carbon.h>
#include <GLUT/glut.h>

#define CHAR_MAXHEIGHT (119.05+33.33)
#define CHAR_DESCALE (1/(CHAR_MAXHEIGHT))

// align -1 left, 0 center, 1 right
void glutStrokeString( char* str, int align ) {
	float tw = 0;
	int i;
	// float textscale;
	for ( i = 0; str[i] != '\0'; i++ ) {
		tw += glutStrokeWidth( GLUT_STROKE_ROMAN, str[i] );
	}
	// textscale = width / tw;
	glPushMatrix();
	// scale chars to be 1 unit high
	glScalef(CHAR_DESCALE,CHAR_DESCALE,1);
	// glScalef(textscale,textscale,1);
	switch ( align ) {
		case 0: // center
		default:
			glTranslatef( -1.0*tw/(2),-(119.05-33.33)/2,0);
			break;
		case -1: // pos is left edge
			glTranslatef( 0,-(119.05-33.33)/2,0);
			break;
		case 1: // pos is right right
			glTranslatef( -1.0*tw,-(119.05-33.33)/2,0);
			break;
	}
	for ( i = 0; str[i] != '\0'; i++ ) {
		glutStrokeCharacter( GLUT_STROKE_ROMAN, str[i] );
		// includes implicit translate to position for next char
		// glTranslatef( 1.0 * glutStrokeWidth( GLUT_STROKE_ROMAN, str[i] ), 0, 0 );
	}
	glPopMatrix();
}

void glutStrokeWrap( char* str, double width, int align ) {
	double tw = 0;
	double pos = 0;
	int i;
	int needswrap = 0;
	//float textscale;
	for ( i = 0; str[i] != '\0'; i++ ) {
		tw += glutStrokeWidth( GLUT_STROKE_ROMAN, str[i] );
	}
	if ( tw > width ) {
		tw = width;
		needswrap = 1;
	}
	//textscale = width / tw;
	glPushMatrix();
	// scale chars to be 1 unit high
	glScalef(CHAR_DESCALE,CHAR_DESCALE,1);
	//glScalef(textscale,textscale,1);
	switch ( align ) {
		case 0: // center
		default:
			glTranslatef( -1.0*tw/(2),-(119.05-33.33)/2,0);
			break;
		case -1: // pos is left edge
			glTranslatef( 0,-(119.05-33.33)/2,0);
			break;
		case 1: // pos is right right
			glTranslatef( -1.0*tw,-(119.05-33.33)/2,0);
			break;
	}
	if ( needswrap ) {
		glPushMatrix();
	}
	for ( i = 0; str[i] != '\0'; i++ ) {
		double ncw;
		ncw = glutStrokeWidth( GLUT_STROKE_ROMAN, str[i] );
		if ( pos + ncw > width ) {
			glPopMatrix();
			glTranslated( 0, CHAR_MAXHEIGHT, 0 );
			glPushMatrix();
			pos = 0;
		}
		glutStrokeCharacter( GLUT_STROKE_ROMAN, str[i] );
		pos += ncw;
		// includes implicit translate to position for next char
		//glTranslatef( 1.0 * glutStrokeWidth( GLUT_STROKE_ROMAN, str[i] ), 0, 0 );
	}
	if ( needswrap ) {
		glPopMatrix();
	}
	glPopMatrix();
}

