#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../pcbsd-boot/vga_effects/pcbsd_logo.c"

/* GIMP typedefs */
typedef int guint;
typedef unsigned char gchar;
typedef unsigned char guint8;


#include "pcbsd.c"

typedef struct {
    int idx;
    unsigned char bitmap[32];
} bmpchar;

bmpchar font[256];

unsigned short int *charmap;



int main(int argc, char *argv[])
{
int f;
int xcnt,ycnt,scan,pixel;
int currentchar,currentidx;
bool isequal;
    /* Initialize the font */

for(f=0;f<256;++f)
{
    font[f].idx=-1;
}


int font_height=16;     // CHANGE TO 8 IF USING 8x8 FONT



int widthchars=(gimp_image.width+7)>>3;
int heightchars=(gimp_image.height+font_height-1)/font_height;

charmap=(unsigned short int *)calloc(1,widthchars*heightchars*sizeof(short int));

if(!charmap) return -1;



currentchar=0;
currentidx=0xff;

unsigned char newchar[32];


for(ycnt=0;ycnt<heightchars;++ycnt)
{
for(xcnt=0;xcnt<widthchars;++xcnt)
{
    /* Get one character glyph from bitmap */

    memset(newchar,0,32);

    for(scan=0;scan<font_height;++scan)
    {
        for(pixel=0;pixel<8;++pixel)
        {
            newchar[scan]|=(gimp_image.pixel_data[ ((ycnt*font_height+scan)*gimp_image.width + (xcnt*8+pixel))<<1]==0)? (1<<(7-pixel)):0;
        }
    }

    /* Find if glyph is repeated and reuse */
    isequal=false;
    for(f=0;f<currentchar;++f)
    {
        isequal=true;
        for(scan=0;scan<font_height;++scan)
        {
            if(newchar[scan]!=font[f].bitmap[scan]) { isequal=false; break; }
        }
        if(isequal) break;
    }

    if(!isequal) {
        /* Glyph is new and unique */
        font[currentchar].idx=currentidx;
        --currentidx;
        memcpy(font[currentchar].bitmap,newchar,32);
        f=currentchar;
        ++currentchar;
    }

    /* Store the current char map */
    /* By default use whit background and grey color (intensity bit must be 1) */

    charmap[xcnt+ycnt*widthchars]=0xf800 | font[f].idx;
    }
}

/* Output a file suitable for loader.c */

printf("/* Text mode bitmap image */\n\n");
printf("struct textmode_bitmap {\nint width,height;\nunsigned short charmap[%d];\n} pcbsd_logo = {\n",widthchars*heightchars);
printf("%d,%d, {\n",widthchars,heightchars);
for(f=0;f<widthchars*heightchars;++f)
{
    printf("0x%04x",charmap[f]);
    if(f!=widthchars*heightchars-1) printf(",");
    if(f!=0 && (f%widthchars==0)) printf("\n");
}
printf("}\n};\n\n");

/* Output the glyphs */

printf("struct bitmap_glyphs {\nint start, count;\nunsigned char glyphs[%d];\n} pcbsd_logo_font = {\n",32*(0xff-(currentidx+1)));
printf("%d,%d, {\n",currentidx+1,0xff-(currentidx+1));
for(f=255-(currentidx+1);f>=0;--f)
{
    for(scan=0;scan<32;++scan) {
    printf("0x%02x",font[f].bitmap[scan]);
    if(scan!=31) printf(",");
    }
    if(f!=0) printf(",");
    printf("\n");
}

printf("}\n};\n\n");

printf("/* End of Text mode Bitmap */");
}
