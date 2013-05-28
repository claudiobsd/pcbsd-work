#ifndef VGA_EFFECTS_H

#define VGA_EFFECTS_H

#define peek(seg,off) (*(unsigned short *)( ((seg)<<4) + (off)))
#define peekb(seg,off) (*(unsigned char *)( ((seg)<<4) + (off)))
#define poke(seg,off,val) (*(unsigned short *)( ((seg)<<4) + (off)))=(unsigned short)(val)

/* These routines are provided by the loader */
extern int		isa_inb(int port);
extern void	isa_outb(int port, int value);

struct textmode_bitmap ;
struct bitmap_glyphs;

void vga_init();
void vga_showbanner();
void vga_print(int x,int y,unsigned int attr,char *string);
void vga_clrlin(int y,int nlines,unsigned char attr);
void vga_printlogo(int x,int y,struct textmode_bitmap *logo, struct bitmap_glyphs *glyphs);
void vga_unmapfonts();
void vga_mapfonts();
void vga_slidedown(int totallines);
void vga_slideupandexit();
void vga_setpalette();







#endif
