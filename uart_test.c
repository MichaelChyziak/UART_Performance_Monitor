/*
 * Placeholder PetaLinux user application.
 *
 * Replace this with your application code
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "../../user-modules/vga_driver/vga_ioctl.h"
#include <termios.h>

#include "UART_Interface.h"

#define SCREEN_W 640
#define SCREEN_H 480

#define IMG_W   192
#define IMG_H   368

#define CHAR_W  11
#define CHAR_H  22

#define BUFFER_SIZE SCREEN_W*SCREEN_H*4

//pixel struct -- holds 4bytes -- RGBA values each one byte
struct pixel{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
};

struct point{
	int x;
	int y;
    int origx;
    int origy;
    int finx;
    int finy;
};

//rectangle -- holds starting x and y offsets, with width and height
struct rect{
	int x;
	int y;
	int w;
	int h;
};

//image -- memory location with width and height
struct image{
	int *mem_loc;
	int w;
	int h;
};

struct sub_image{
	int row;
	int col;
	int w;
	int h;
};
//-----------------function declarations-----------------//

void clean_screen(int *buffer) {
    int row, col;
	for (row = 0; row < SCREEN_H; row++) {
        for (col = 0; col < SCREEN_W; col++) {
            *(buffer + col + row*SCREEN_W) = 0x00000000;
        }
    }
	return;
}

void color_init(struct pixel *color, int r,int g, int b, int a) {
    color->r = r;
    color->g = g;
    color->b = b;
    color->a = a;
    return;
}

void draw_rectangle(int *screen_buffer, struct rect *r, struct pixel *color) {
	int xstart, xend, ystart, yend;


	/*Invalid screen buffer*/
	if(!screen_buffer)
		return;

	/*invalid rectangle*/
	if (r->w <= 0 || r->h <= 0) //negative width and height
		return;

	/*Starting offsets if image is partially offscreen*/
	//check for clipping left and top
	xstart = (r->x < 0) ? -1*r->x : 0;
	ystart = (r->y < 0) ? -1*r->y : 0;
	//check for clipping right and bottom
	xend = (r->x + r->w) > SCREEN_W ? (SCREEN_W - r->x): r->w;
	yend = (r->y + r->h) > SCREEN_H ? (SCREEN_H - r->y): r->h;

	/*Image is entirely offscreen*/
	if (xstart == xend || ystart == yend)
		return;

	/*Copy image*/
	int row, col;
	for (row = ystart; row < yend; row++) {
		for (col = xstart; col < xend; col++) {
			struct pixel src_p, dest_p;

			int *screenP;

			screenP = screen_buffer + (r->x + col) + (r->y + row)*SCREEN_W;

			src_p.a = color->a;
			src_p.r = color->r;
			src_p.g = color->g;
			src_p.b = color->b;
			//destination initialization using byte masks
			dest_p.b = 0xFF & (*screenP >> 16);
			dest_p.g = 0xFF & (*screenP >> 8);
			dest_p.r = 0xFF & (*screenP >> 0);
			//alpha blending
			dest_p.b = (((int)src_p.b)*src_p.a + ((int)dest_p.b)*(255 - src_p.a))/256;
			dest_p.g = (((int)src_p.g)*src_p.a + ((int)dest_p.g)*(255 - src_p.a))/256;
			dest_p.r = (((int)src_p.r)*src_p.a + ((int)dest_p.r)*(255 - src_p.a))/256;
			//put destination pixel values into the screen buffer
			*screenP = (dest_p.b << 16) | (dest_p.g << 8) | (dest_p.r << 0);
		}
	}

	return;

}

void clear_monitor(int *buffer, struct point *p, struct rect *r, struct pixel *colorbox) {
    struct pixel color;
    color_init(&color, 0,0,0,255);
    p->x = p->origx;
    p->y = p->origy;
    //draw_rectangle(buffer, r, &color);
    draw_rectangle(buffer, r, colorbox);

    return;
}

void draw_image(int *screen_buffer, struct image *i, struct point *p, struct pixel *color) {

	int imgXstart, imgXend, imgYstart, imgYend;

	/*Invalid screen buffer*/
	if(!screen_buffer)
		return;

	/*invalid image*/
	if (!i->mem_loc || i->w <= 0 || i->h <= 0)
		return;

	/*Starting offsets if image is partially offscreen*/
	//check if clipping left and top
	imgXstart = (p->x < 0) ? -1*p->x : 0;
	imgYstart = (p->y < 0) ? -1*p->y : 0;
	//check if clipping right and bottom
	imgXend = (p->x + i->w) > SCREEN_W ? (SCREEN_W - p->x): i->w;
	imgYend = (p->y + i->h) > SCREEN_H ? (SCREEN_H - p->y): i->h;

	/*Image is entirely offscreen*/
	if (imgXstart == imgXend || imgYstart == imgYend)
		return;

	/*Copy image*/
	int row, col;
	for (row = imgYstart; row < imgYend; row++) {
		for (col = imgXstart; col < imgXend; col++) {
			struct pixel src_p, dest_p;

			int *imgP;
			int *screenP;

			imgP    = i->mem_loc + col + (i->w * row);
			screenP = screen_buffer + (p->x + col) + (p->y + row)*SCREEN_W;

			src_p.a = 0xFF & (*imgP >> 24);

			if(color) {
				src_p.r = color->r;
				src_p.g = color->g;
				src_p.b = color->b;
			}
			else {
				src_p.b = 0xFF & (*imgP >> 16);
				src_p.g = 0xFF & (*imgP >> 8);
				src_p.r = 0xFF & (*imgP >> 0);
			}

			dest_p.b = 0xFF & (*screenP >> 16);
			dest_p.g = 0xFF & (*screenP >> 8);
			dest_p.r = 0xFF & (*screenP >> 0);


			dest_p.b = (((int)src_p.b)*src_p.a + ((int)dest_p.b)*(255 - src_p.a))/256;
			dest_p.g = (((int)src_p.g)*src_p.a + ((int)dest_p.g)*(255 - src_p.a))/256;
			dest_p.r = (((int)src_p.r)*src_p.a + ((int)dest_p.r)*(255 - src_p.a))/256;

			*screenP = (dest_p.b << 16) | (dest_p.g << 8) | (dest_p.r << 0);
		}
	}

	return;

}

void draw_sub_image(int *screen_buffer, struct image *i, struct point *p, struct sub_image *sub_i, struct pixel *color) {

	int imgXstart, imgXend, imgYstart, imgYend;

	/*Invalid screen buffer*/
	if(!screen_buffer)
		return;

	/*invalid image*/
	if (!i->mem_loc || sub_i->w <= 0 || sub_i->h <= 0)
		return;

	/*Starting offsets if image is partially offscreen*/
	imgXstart = (p->x < 0) ? -1*p->x : 0;
	imgYstart = (p->y < 0) ? -1*p->y : 0;

	imgXend = (p->x + sub_i->w) > SCREEN_W ? (SCREEN_W - p->x): sub_i->w;
	imgYend = (p->y + sub_i->h) > SCREEN_H ? (SCREEN_H - p->y): sub_i->h;

	/*Image is entirely offscreen*/
	if (imgXstart == imgXend || imgYstart == imgXend)
		return;

	/*Copy image*/
	int row, col;
	for (row = imgYstart; row < imgYend; row++) {
		for (col = imgXstart; col < imgXend; col++) {
			struct pixel src_p, dest_p;

			int *imgP;
			int *screenP;

			imgP    = i->mem_loc + col + sub_i->col + (i->w * (row + sub_i->row));
			screenP = screen_buffer + (p->x + col) + (p->y + row)*SCREEN_W;

			src_p.a = 0xFF & (*imgP >> 24);

			if(color) {
				src_p.r = color->r;
				src_p.g = color->g;
				src_p.b = color->b;
			}
			else {
				src_p.b = 0xFF & (*imgP >> 16);
				src_p.g = 0xFF & (*imgP >> 8);
				src_p.r = 0xFF & (*imgP >> 0);
			}

			dest_p.b = 0xFF & (*screenP >> 16);
			dest_p.g = 0xFF & (*screenP >> 8);
			dest_p.r = 0xFF & (*screenP >> 0);


			dest_p.b = (((int)src_p.b)*src_p.a + ((int)dest_p.b)*(255 - src_p.a))/256;
			dest_p.g = (((int)src_p.g)*src_p.a + ((int)dest_p.g)*(255 - src_p.a))/256;
			dest_p.r = (((int)src_p.r)*src_p.a + ((int)dest_p.r)*(255 - src_p.a))/256;

			*screenP = (dest_p.b << 16) | (dest_p.g << 8) | (dest_p.r << 0);
		}
	}


}

void draw_letter(char a, int *screen_buffer, struct image *i, struct point *p, struct pixel *color) {

    struct sub_image sub_i;

    sub_i.row = 23*((0x0F & (a >> 4))%16) + 1;
    sub_i.col = 12*((0x0F & (a >> 0))%16) + 1;
    sub_i.w = CHAR_W;
    sub_i.h = CHAR_H;
    draw_sub_image(screen_buffer, i, p, &sub_i, color);


}

void tile_image(int *screen_buffer, struct image *i, struct point *p, struct pixel *color) {

	int full_x, full_y;
	int col, row;

	/*Invalid screen buffer*/
	if(!screen_buffer)
		return;

	/*invalid image*/
	if (!i->mem_loc || i->w <= 0 || i->h <= 0)
		return;
	
	full_x = SCREEN_W / i->w;
	full_y = SCREEN_H / i->h;
	
	p->x = 0;
	p->y = 0;

	for (row = 0; row <= full_y; row++) {		
		p->y = row * i->h;
		for (col = 0; col <= full_x; col++) {			
			p->x = col * i->w;
			draw_image(screen_buffer, i, p, NULL);
		}
	}
}

void next_char(int *buffer, struct point *p, struct rect *r, struct pixel *colorbox, int c_xinc, int c_yinc) {
    p->x += c_xinc;
    if (p->x + c_xinc > p->finx) {
        p->x = p->origx;
        p->y += c_yinc;
        if (p->y + c_yinc > p->finy) {
            p->y = p->origy;
            clear_monitor(buffer, p, r, colorbox);
        }
    }
    return;
}

//function to output a new line to monitor -- checks whether at the end of the 
void next_line(int *buffer, struct point *p, struct rect *r, struct pixel *colorbox, int c_yinc) {
    p->x = p->origx;
    p->y += c_yinc;
    if (p->y + c_yinc > p->finy) {
        p->y = p->origy;
        clear_monitor(buffer, p, r, colorbox);
    }
    return;
}

void draw_string(char *s, int *buffer, struct image *i, struct point *p, struct rect *r, struct pixel *color, struct pixel *colorbox) {
    int index = 0;
    int c_xinc = CHAR_W;
    int c_yinc = CHAR_H;
    while(s[index]) {
        if (s[index] == '\\') { //check for escape character \n
            if (index < strlen(s)) { //check if character is at the end of the char array
                if (s[index+1] == 'n') { //if 'n', then escape character
                    next_line(buffer, p, r, colorbox, c_yinc);
                    index += 2; //don't want to print the '\n'
                    continue;
                }
            }
        }
    	draw_letter(s[index], buffer, i, p, color);
        next_char(buffer, p, r, colorbox, c_xinc, c_yinc);
        
		index++;
    }
}

void backspace(int *buffer, struct point *p, struct pixel *colorbox, int c_xinc, int c_yinc) {
    struct pixel color;  
    struct rect blank;
    blank.w = c_xinc;
    blank.h = c_yinc; 
    color_init(&color, 0,0,0,255);

    if (p->x == p->origx) { //go back to previous line
        if (p->y == p->origy) {
            return; //can't go up anymore
        }
        p->x = p->finx - c_xinc;
        p->y -= c_yinc;
        blank.x = p->x;
        blank.y = p->y;
        draw_rectangle(buffer, &blank, &color);
        draw_rectangle(buffer, &blank, colorbox);
        return;
    }
    p->x -= c_xinc;
    blank.x = p->x;
    blank.y = p->y;
    draw_rectangle(buffer, &blank, &color);
    draw_rectangle(buffer, &blank, colorbox);
    
    return;
}

// temp buffer to hold all of the data of the frame that will be displayed, so we can memcpy it
// when we receive our sigio interrupts at whatever framerate we want to run at.
// Memcopy is faster than a for loop writing directly into VGA memory, which reduces flickering.
int * buffer;
int temp_buffer[SCREEN_W*SCREEN_H];
struct image img; // this had to be made global because display_metrics needs it


/*
 *  METRICS
 */

// here are the global structs and values for typing metrics.
struct rect gui_r; // this rect is for the gui window
struct point gui_p; // this point is for the gui window

struct pixel gui_color, gui_colorbox;

struct timespec console_start;
struct timespec previous_frame_time;

struct timespec typing_start;

int read_chars = 0;
int write_chars = 0; //currently not printed

//counting and boolean for word count
int read_words = 0;
//0 = no whitespaces before this character, 1 = previous character was whitespace.
// used by update_metrics to update word count.
int previous_is_space = 1;

int typing_has_started = 0; //bool val (0 false, otherwise true)

void reset_metrics() {
    read_chars = 0;
    write_chars = 0;
    read_words = 0;
    previous_is_space = 1;
    typing_has_started = 0;
}

// this function updates global variables for the typing metrics.
void update_metrics( char * c, int num_chars_read ) {
    int i = 0;
    if (num_chars_read > 0 && !typing_has_started) {
        clock_gettime(CLOCK_MONOTONIC, &typing_start);
        typing_has_started = 1;
    }
    read_chars += num_chars_read;
    //updating word count
    for (i = 0; i < num_chars_read; i++) {
        if (isspace(c[i])) {
            if (!previous_is_space) {
                previous_is_space = 1;
            }
        } else {
            if (previous_is_space) {
                previous_is_space = 0;
                read_words += 1;
            }
        }
    }
}


//Display metrics using printf statements on metric global variables
void display_metrics() {

    struct timespec current_time;
    int index = 0;
    char str[7][500];
    
    unsigned long long time_since_console_start = 0ULL;
    unsigned long long time_since_system_start = 0ULL;
    unsigned long long time_since_typing_start = 0ULL;
    unsigned long long time_since_last_frame = 0ULL;
    
    unsigned long long average_read_time_s = 0LL;
    unsigned long long average_read_time_ns = 0LL;
    //unsigned long long average_write_time_s = 0LL;
    //unsigned long long average_write_time_ns = 0LL;
    long double chars_read_per_s = 0.0L;
    //long double chars_written_per_s = 0.0L;
    long double words_per_min = 0.0L;
    long double fps = 0.0L;

    struct rect fps_bar;
    struct pixel fps_bar_color;
    struct rect key_bar;
    struct pixel key_bar_color;
    struct rect wpm_bar;
    struct pixel wpm_bar_color;
    
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    time_since_system_start = (current_time.tv_sec * BILLION) + current_time.tv_nsec;
    time_since_console_start = time_spec_diff(&current_time, &console_start);
    time_since_typing_start = time_spec_diff(&current_time, &typing_start);
    time_since_last_frame = time_spec_diff(&current_time, &previous_frame_time);

    //calculating metrics (always shown metrics)
    int time_since_system_start_total_s = (int)(time_since_system_start/BILLION);
    int time_since_system_start_h = (int) (time_since_system_start_total_s / 3600);
    int time_since_system_start_m = ((int) (time_since_system_start_total_s / 60)) % 60;
    int time_since_system_start_s = time_since_system_start_total_s % 60;

    int time_since_console_start_total_s = (int)(time_since_console_start/BILLION);
    int time_since_console_start_h = (int) (time_since_console_start_total_s / 3600);
    int time_since_console_start_m = ((int) (time_since_console_start_total_s / 60)) % 60;
    int time_since_console_start_s = time_since_console_start_total_s % 60;

    fps = 1.0L / ((long double) time_since_last_frame / (long double) BILLION );
    // creating FPS bar
    fps_bar.x = gui_p.origx + 5; // width of a PIPE character.
    fps_bar.w = 19*CHAR_W - 10; // this is actually a full bar.
    fps_bar.y = gui_p.origy + CHAR_H + 6; // char_h is 22.
    fps_bar.h = CHAR_H - 12; // this will leave two pixels of the pipe distance indicators
    long double fps_percent = (fps/30.0L >= 1.0L ? 1.0L : fps/30.0L );
    // width is a % of fps/30
    fps_bar.w = (int) ( ( (long double) fps_bar.w ) * fps_percent );
    if ( fps < 10.0L ) {
        color_init(&fps_bar_color,255, 0, 0, 255);
    } else if ( fps < 19.9L ) {
        color_init(&fps_bar_color, 255, 255, 0, 255);

    } else {
        color_init(&fps_bar_color, 0, 255, 0, 255);
    }


    //Calculating metrics
    if (typing_has_started) {
        average_read_time_s = read_chars == 0 ? 0 : (time_since_typing_start/(BILLION * (unsigned long long)read_chars));
        average_read_time_ns = read_chars == 0 ? 0 : ((time_since_typing_start/(unsigned long long)read_chars)) % BILLION;
        //average_write_time_s = write_chars == 0 ? 0 : (time_since_typing_start/(BILLION * (unsigned long long)write_chars));
        //average_write_time_ns = write_chars == 0 ? 0 : ((time_since_typing_start/(unsigned long long)write_chars)) % BILLION;
        chars_read_per_s = 1.0L/((((long double)average_read_time_s * (long double)BILLION) + (long double)average_read_time_ns)/(long double)BILLION);
        //chars_written_per_s = 1.0L/((((long double)average_write_time_s * (long double)BILLION) + (long double)average_write_time_ns)/(long double)BILLION);
        words_per_min = ((long double) read_words) / ( (long double)time_since_typing_start / ((long double) BILLION * 60.0L) );
    }



    // creating key bar
    key_bar.x = gui_p.origx + 5; // width of a PIPE character.
    key_bar.w = 19*CHAR_W - 10; // this is actually a full bar.
    key_bar.y = gui_p.origy + CHAR_H*10 + 6; // char_h is 22.
    key_bar.h = CHAR_H - 12; // this will leave two pixels of the pipe distance indicators
    long double key_percent = (chars_read_per_s/30.0L >= 1.0L ? 1.0L : chars_read_per_s/30.0L );
    // width is a % of fps/30
    key_bar.w = (int) ( ( (long double) key_bar.w ) * key_percent );
    if ( chars_read_per_s < 10.0L ) {
        color_init(&key_bar_color,255, 0, 0, 255);
    } else if ( chars_read_per_s < 20.0L ) {
        color_init(&key_bar_color, 255, 255, 0, 255);

    } else if ( chars_read_per_s < 30.0L ) {
        color_init(&key_bar_color, 0, 255, 0, 255);
    
    } else {
        color_init(&key_bar_color, 0, 255, 0, 255);
    }


    // creating wpm bar
    wpm_bar.x = gui_p.origx + 5; // width of a PIPE character.
    wpm_bar.w = 19*CHAR_W - 10; // this is actually a full bar.
    wpm_bar.y = gui_p.origy + CHAR_H*15 + 6; // char_h is 22.
    wpm_bar.h = CHAR_H - 12; // this will leave two pixels of the pipe distance indicators
    long double wpm_percent = (words_per_min/150.0L >= 1.0L ? 1.0L : words_per_min/150.0L );
    // width is a % of fps/30
    wpm_bar.w = (int) ( ( (long double) wpm_bar.w ) * wpm_percent );
    if ( words_per_min < 50.0L ) {
        color_init(&wpm_bar_color,255, 0, 0, 255);
    } else if ( words_per_min < 100.0L ) {
        color_init(&wpm_bar_color, 255, 255, 0, 255);

    } else if ( words_per_min < 150.0L ) {
        color_init(&wpm_bar_color, 0, 255, 0, 255);
    
    } else {
        color_init(&wpm_bar_color, 0, 255, 0, 255);
    }


    
    //draw_string("Hello!\\nHere are some commands you can use:\\n\\n", buffer, &img, &p, &r, &color, &colorbox);
    //printf("METRICS UPDATE:\n");
    sprintf(str[0], "FPS\\n|     |     |     |%7.2lf\\n0     10    20    30\\n", fps);
    sprintf(str[1], "Time Since System Start\\n%d:%02d:%02d\\n", time_since_system_start_h, time_since_system_start_m, time_since_system_start_s);
    sprintf(str[2], "Time Since Console Start\\n%d:%02d:%02d\\n", time_since_console_start_h, time_since_console_start_m, time_since_console_start_s);
    sprintf(str[3], "Keys Pressed\\n%d\\n", read_chars);
    if (typing_has_started) {
        //printf("\tAverage Read Time(s/char): %llu.%09llu\n", average_read_time_s, average_read_time_ns);
        //printf("\tAverage Write Time(s/char): %llu.%09llu\n", average_write_time_s, average_write_time_ns);
        sprintf(str[4], "Keys Pressed Per Second\\n|     |     |     |%7.2lf\\n0     10    20    30\\n", chars_read_per_s);
        // 
        //printf("\tCharacters Written Per Second: %.9lf\n", chars_written_per_s);
        sprintf(str[5], "Word Count\\n%d\\n", read_words);
        sprintf(str[6], "Words Per Minute\\n|     |     |     |%7.2lf\\n0     50    100   150\\n", words_per_min);
    } else {
        sprintf(str[4], "No typing statistics to\\nanalyze.\\n");
        if (time_since_system_start_s % 2) {sprintf(str[5], "   _\\n  ( \\\\n   ) )\\n  ( (  .-''-.  A.-.A\\n   \\ \\/      \\/ , , \\\\n    \\   \\    =;  t  /=\\n     \\   |''.  ',--'\\n      / //  | ||\\n     /_,))  |_,))");}
        else {sprintf(str[5], "    _\\n   / )\\n  ( (\\n   ) ) .-''-.  A.-.A\\n   \\ \\/      \\/ _ , \\\\n    \\   \\    =;  t  /=\\n     \\   |''.  ',--'\\n      / //  | | \\\\n     /_,))  |_,)_,)");}
    }

    // update entire buffer with new metrics
    clear_monitor( temp_buffer, &gui_p, &gui_r, &gui_colorbox);
    if (typing_has_started) {
        for ( index = 0; index < 7; ++index ) {
            draw_string( str[index], temp_buffer, &img, &gui_p, &gui_r, &gui_color, &gui_colorbox);
        }
        draw_rectangle( temp_buffer, &key_bar, &key_bar_color ); 
        draw_rectangle( temp_buffer, &wpm_bar, &wpm_bar_color ); 
    } else {
        for ( index = 0; index < 6; ++index ) {
            draw_string( str[index], temp_buffer, &img, &gui_p, &gui_r, &gui_color, &gui_colorbox);
        }
    }
    draw_rectangle( temp_buffer, &fps_bar, &fps_bar_color ); 
    previous_frame_time = current_time;
}


/*
 * SIGIO Signal Handler
 */

int vsync_flag = 0;

void sigio_handler(int signum)
{
	//printf("Received Signal, signum=%d (%s)\n", signum, strsignal(signum));
    if ( vsync_flag == 1 ) {
        
        vsync_flag = 0;
	    display_metrics();
        memcpy( buffer, temp_buffer, BUFFER_SIZE );
    } else {
        printf("Frame Skipped\n");
        // we could do something with FPS here (reduce the FPS counter somehow?)
    }
}

//initalizes the sigio interrupt handler and opens the timer fd
int setup_timer_handler() {
    int oflags;

	// Register handler for SIGINT, sent when pressing CTRL+C at terminal
	signal(SIGINT, &sigint_handler);

	// Open device driver file
	if (!(timer_fd = open("/dev/timer_driver", O_RDWR))) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	signal(SIGIO, &sigio_handler);
	fcntl(timer_fd, F_SETOWN, getpid());
	oflags = fcntl(timer_fd, F_GETFL);
	fcntl(timer_fd, F_SETFL, oflags | FASYNC);
	return 0;
}

struct vga_ioctl_data data;
int fd;
int image_fd;

// this sets up the global vars fd and image_fd which are all file descriptors.
int setup_fds() {
    
    fd = open("/dev/vga_driver",O_RDWR);

    image_fd = open("/home/root/example2.raw",O_RDONLY);		
    if(image_fd  == -1){
	    printf("Failed to open image... :( \n");
        return -1;
    }
		
    if(init_serial() != 0) {
	    printf("Initialization failed.\n");
        close(image_fd);
	    return -1;
	}
    return 0;
}

// globals set up:
// buffer, temp_buffer, previous_frame_time, img.
// this function should be called after setup_fds()
// the malloc for img.mem_loc is to store all of the image
// data into userspace memory, for faster access.
int setup_globals() {
    struct stat sb;
    int * image_addr;

    buffer = (int*)mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("buffer addr: 0x%08x\n", buffer);

    fstat (image_fd, &sb);
    image_addr = (int*)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, image_fd, 0);
    printf("image addr: 0x%08x\n", image_addr );
    // allocating userspace memory to copy the image into.
    img.mem_loc = (int*) malloc( sb.st_size );
    // img.mem_loc = (int*)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, image_fd, 0);
    if ( img.mem_loc == NULL ) {
        perror ("malloc failed");
        return -1;
    }
    // copy the image into the allocated memory.
    memcpy( img.mem_loc, image_addr, sb.st_size );
    printf("userspace copy of image addr: 0x%08x\n", img.mem_loc );

    //initialize example image width and height
	img.w = IMG_W;
    img.h = IMG_H;

    clean_screen(temp_buffer);
    printf("screen cleared\n" );

    msync(buffer, BUFFER_SIZE, MS_SYNC);

    return 0;
}

// this function is to be used if both setup_fds and setup_globals have successfully been
// called already. This closes the image_fd, the vga fd, and frees the temp buffer
// local memory.
void exit_console( int status ) {
    close(image_fd);
    close(fd);
    free( img.mem_loc );
    exit_interface( status );
    // this should never return
    return;
}

// function initializes the typing window structures, p, r, color, and colorbox.
void setup_typing_window( struct point * p, struct rect * r, struct pixel * color, struct pixel * colorbox,
                          int c_xinc, int c_yinc ) {
    //initialize typing window background rectangle
    color_init(colorbox,80, 100, 255, 255);
    r->x = 0;
    r->y = 0;
    r->w = SCREEN_W/2;
    r->h = SCREEN_H;

    //initialize text color
    color_init(color,0,0,0,255);
    //initialize the starting pixel struct for typing window.
    p->x = r->x + 10;
    p->y = r->y + 10;
    p->origx = p->x;
    p->origy = p->y;
    p->finx = ((r->w-2*10)/c_xinc)*c_xinc + p->origx;
    p->finy = ((r->h-2*10)/c_yinc)*c_yinc + p->origy;
}

int main(int argc, char *argv[])
{
    struct point p; // this point is for the typing window
    struct pixel color, colorbox;
    struct rect r; // this rect is for the typing window
    char c[MAX_CHAR_INPUT_SIZE];
    char esc_c = 0;
    int c_xinc = CHAR_W;
    int c_yinc = CHAR_H;
    int num_chars_read = 0;
    
    // first thing program does in main loop is "start" the console.
    clock_gettime(CLOCK_MONOTONIC, &console_start);
    previous_frame_time = console_start;

    // Set up Asynchronous Notification
	setup_timer_handler();

    // setup global file descriptors
    if ( setup_fds() != 0 ) {
        exit_interface(EXIT_FAILURE);
    }

    // setup global variables in this file
    if ( setup_globals() != 0 ) {
        exit_console(EXIT_FAILURE);
    }

    // setup local structs for the typing window's 
    // point, rect, and the pixels color and colorbox.
    setup_typing_window( &p, &r, &color, &colorbox, c_xinc, c_yinc );
    draw_rectangle(temp_buffer, &r, &colorbox);

    reset_metrics();

    // initialize gui window background rectangle
    color_init(&gui_colorbox,0, 0, 0, 255);
    gui_r.x = SCREEN_W/2;
    gui_r.y = 0;
    gui_r.w = SCREEN_W/2;
    gui_r.h = SCREEN_H;

    // initialize the starting pixel struct for gui window
    color_init(&gui_color, 255, 255, 255, 255);
    gui_p.x = gui_r.x + 10;
    gui_p.y = gui_r.y + 10;
    gui_p.origx = gui_p.x;
    gui_p.origy = gui_p.y;
    gui_p.finx = ((gui_r.w-2*10)/c_xinc)*c_xinc + gui_p.origx;
    gui_p.finy = ((gui_r.h-2*10)/c_yinc)*c_yinc + gui_p.origy;

    // this starts the timer.
	setup_timer_interval(50); //50 ms (20 fps) interval of UI updates.
	// 1/60 is 16.66666 ms, so if using int, rounded to 16 is OKAY... (~62.5 fps)

    // disable update of screen by sigio handler while writing to temp buffer
    vsync_flag = 0;
    draw_string("Hello!\\nHere are some commands you can use:\\n\\n", temp_buffer, &img, &p, &r, &color, &colorbox);
    draw_string("\\ c -- clear the screen\\n\\ r -- reset metrics\\n\\ q -- quit program\\n\\n", temp_buffer, &img, &p, &r, &color, &colorbox);
    
    draw_letter('_', temp_buffer, &img, &p, &color);
    next_char(temp_buffer, &p, &r, &colorbox, c_xinc, c_yinc);
    vsync_flag = 1; // re-enable updates to screen.

    while(1) {
        num_chars_read = 0;
        while( num_chars_read < 1 ) {
            vsync_flag = 1;
            num_chars_read = read_from_kermit(c);
            if (num_chars_read == -1) {
                exit_interface(EXIT_FAILURE);
            }
        }
        update_metrics( c, num_chars_read );
        vsync_flag = 0; // enable updating of screen (by the sigio handler)

        if (num_chars_read == -1) {
            printf("WTF\n");
            exit_interface(EXIT_FAILURE);
        } else if (num_chars_read) {
            if (c[0] == '\\' && !esc_c) { //check for escape character \n -- set flag
                esc_c = 1;
                continue;
            }
            vsync_flag = 0; // disable updating of screen by sigio handler
            if (esc_c) {
                if (c[0] == 'c') { //clear screen
                    clear_monitor(temp_buffer, &p, &r, &colorbox);
                    draw_letter('_', temp_buffer, &img, &p, &color);
                    next_char(temp_buffer, &p, &r, &colorbox, c_xinc, c_yinc);
                    esc_c = 0;
                    continue;
                }
                if (c[0] == 'q') { //quit program
                    //clear_monitor(buffer, &p, &r, &colorbox);
                    msync(buffer, BUFFER_SIZE, MS_SYNC);
                    exit_console(EXIT_SUCCESS);
                    return 0; // this return shouldn't be reached.
                }
                if (c[0] == 'r' ) { // reset metrics
                    reset_metrics();
                    esc_c = 0;
                    continue;
                }
                backspace(temp_buffer, &p, &colorbox, c_xinc, c_yinc);
                draw_letter('\\', temp_buffer, &img, &p, &color); //output '\' since no escape character used
                next_char(temp_buffer, &p, &r, &colorbox, c_xinc, c_yinc);
                draw_letter('_', temp_buffer, &img, &p, &color);
                next_char(temp_buffer, &p, &r, &colorbox, c_xinc, c_yinc);
                write_to_kermit(c, num_chars_read);
                write_chars += num_chars_read;
                esc_c = 0;
            }
            if (c[0] == '\n') { //newline return key
                backspace(temp_buffer, &p, &colorbox, c_xinc, c_yinc);
                next_line(temp_buffer, &p, &r, &colorbox, c_yinc);
                draw_letter('_', temp_buffer, &img, &p, &color);
                next_char(temp_buffer, &p, &r, &colorbox, c_xinc, c_yinc);
                write_to_kermit(c, num_chars_read);
                write_chars += num_chars_read;
                continue;
            }
            if (c[0] == 127) { //backspace key
                backspace(temp_buffer, &p, &colorbox, c_xinc, c_yinc);
                backspace(temp_buffer, &p, &colorbox, c_xinc, c_yinc);
                draw_letter('_', temp_buffer, &img, &p, &color);
                next_char(temp_buffer, &p, &r, &colorbox, c_xinc, c_yinc);
                write_to_kermit(c, num_chars_read);
                write_chars += num_chars_read;
                continue;
            }

            backspace(temp_buffer, &p, &colorbox, c_xinc, c_yinc);
            draw_letter(c[0], temp_buffer, &img, &p, &color);
            next_char(temp_buffer, &p, &r, &colorbox, c_xinc, c_yinc);
            draw_letter('_', temp_buffer, &img, &p, &color);
            next_char(temp_buffer, &p, &r, &colorbox, c_xinc, c_yinc);
        }
        write_to_kermit(c, num_chars_read);
        write_chars += num_chars_read;
    }
    msync(buffer, BUFFER_SIZE, MS_SYNC);

    exit_console(EXIT_SUCCESS); // this exit should never be reached.

    return 0; //this return should never be reached.
}
