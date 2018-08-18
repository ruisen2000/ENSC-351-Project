/*

///////////////////////////////////////////////////////////////////////////////////////////////////////////
ENSC 351 - Final Project
December 1st, 2016

Authors:
Bradley Dalrymple
Alastair Scott
Greyson Wang
Ricardo Dupouy

Description:
To allow system performance statistics to be displayed to the screen as well as typed characters.

///////////////////////////////////////////////////////////////////////////////////////////////////////////
*/

#include <stdio.h>
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
//#include "../../user-modules/vga_test/vga_ioctl.h"
#include <termios.h>


//copied over
#include <stdlib.h>
#include <time.h>
#include <sys/sysinfo.h>
#include "../../user-modules/timer_driver/timer_ioctl.h"



#define SCREEN_W 640
#define SCREEN_H 480


#define CHARACTER_W 11
#define CHARACTER_H 22

#define BUFFER_SIZE SCREEN_W*SCREEN_H*4




/*
 * Global variables copied over
 */
struct timer_ioctl_data data; // data structure for ioctl calls

int fd1; // file descriptor for timer driver

struct pixel{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
};

struct point{
	int x;
	int y;
};


struct rect{
	int x;
	int y;
	int w;
	int h;
};


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

    



/* 
 * Function to read the value of the timer
 */
__u32 read_timer()
{
	data.offset = TIMER_REG;
	ioctl(fd1, TIMER_READ_REG, &data);
	return data.data;
}


/*
 * SIGIO Signal Handler
 */

void sigio_handler(int signum) 
{

}


/*
 * SIGINT Signal Handler
 */
void sigint_handler(int signum)
{
	printf("Received Signal, signum=%d (%s)\n", signum, strsignal(signum));

	if (fd1) {
		// Turn off timer and reset device
		data.offset = CONTROL_REG;
		data.data = 0x0;
		ioctl(fd1, TIMER_WRITE_REG, &data);

		// Close device file
		close(fd1);
	}

	exit(EXIT_SUCCESS);
}



//Start of drawing funcitons to draw a rectangle with passed in RGB values to the screen
void draw_rectangle(int *screen_buffer, struct rect *r, struct pixel *color) {
	int xstart, xend, ystart, yend;


	/*Invalid screen buffer*/
	if(!screen_buffer)
		return;

	/*invalid rectangle*/
	if (r->w <= 0 || r->h <= 0)
		return;

	/*Starting offsets if image is partially offscreen*/
	xstart = (r->x < 0) ? (-1*r->x) : 0;
	ystart = (r->y < 0) ? (-1*r->y) : 0;

	xend = ((r->x + r->w) > SCREEN_W) ? (SCREEN_W - r->x): r->w;
	yend = ((r->y + r->h) > SCREEN_H) ? (SCREEN_H - r->y): r->h;

	/*Image is entirely offscreen*/
	if (ystart == yend || xstart == xend)                                       //rectangle does not exist. just a line of pixels
		return;

	/*Copy image*/
	int row, col;
	for (row = ystart; row < yend; row++) {
	  for (col = xstart; col < xend; col++) {                                   //increment in x
			struct pixel src_p, dest_p;

			int *screenP;

			screenP = screen_buffer + (r->x + col) + (r->y + row)*SCREEN_W;    //puts the correct image with the size

			src_p.a = color->a;
			src_p.r = color->r;
			src_p.g = color->g;
			src_p.b = color->b;

			dest_p.b = 0xFF & (*screenP >> 16);
			dest_p.g = 0xFF & (*screenP >> 8);
			dest_p.r = 0xFF & (*screenP >> 0);

			dest_p.b = (((int)src_p.b)*src_p.a + ((int)dest_p.b)*(255 - src_p.a))/256;   //combine colour and pixel location for Blue. 
			dest_p.g = (((int)src_p.g)*src_p.a + ((int)dest_p.g)*(255 - src_p.a))/256;   //combine colour and pixel location for Green. 
			dest_p.r = (((int)src_p.r)*src_p.a + ((int)dest_p.r)*(255 - src_p.a))/256;   //combine colour and pixel location for Red.

			*screenP = (dest_p.b << 16) | (dest_p.g << 8) | (dest_p.r << 0);     //printing to screen with shift
		}
	}

	return;

}

void draw_image(int *screen_buffer, struct image *i, struct point *p, struct pixel *color) {

	int imgXstart, imgXend, imgYstart, imgYend;

	/*Invalid screen buffer*/
	if(!screen_buffer)
		return;

	/*invalid image*/
	if (!i->mem_loc || i->w <= 0 || i->h <= 0)    //making sue that it has a real width and height
		return;

	/*Starting offsets if image is partially offscreen*/
	imgXstart = (p->x < 0) ? -1*p->x : 0;                      //puts the start at the top left corner. The buffer is accounted for in the for loop
	imgYstart = (p->y < 0) ? -1*p->y : 0;                      //puts the start at the top left corner. The buffer is accounted for in the for loop

	imgXend = (p->x + i->w) > SCREEN_W ? (SCREEN_W - p->x): i->w;   //added the width to get the end of the x value. Same as above but adds the offset
	imgYend = (p->y + i->h) > SCREEN_H ? (SCREEN_H - p->y): i->h;   //added the hieght to get the end of the y value. Same as above but adds the offset

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
			screenP = screen_buffer + (p->x + col) + (p->y + row)*SCREEN_W;   //Same as above for offsetting on the screen

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

			//This code takes into account the alpha value for RGB
			dest_p.b = (((int)src_p.b)*src_p.a + ((int)dest_p.b)*(255 - src_p.a))/256;
			dest_p.g = (((int)src_p.g)*src_p.a + ((int)dest_p.g)*(255 - src_p.a))/256;
			dest_p.r = (((int)src_p.r)*src_p.a + ((int)dest_p.r)*(255 - src_p.a))/256;

			*screenP = (dest_p.b << 16) | (dest_p.g << 8) | (dest_p.r << 0);   //Setting pixels to the screen
		}
	}

	return;

}


/*

This function draws a clipped image at specified starting and ending points.
It is called from the draw letter function, see draw image for additional comments

*/
void draw_sub_image(int *screen_buffer, struct image *i, struct point *p, struct sub_image *sub_i, struct pixel *color) {

        	//printf("draw Subimage\n" );

	int imgXstart, imgXend, imgYstart, imgYend;

	//Invalid screen buffer
	if(!screen_buffer)
		return;

	//invalid image
	if (!i || sub_i->w <= 0 || sub_i->h <= 0)
		return;

	//Starting offsets if image is partially offscreen
	imgXstart = (p->x < 0) ? -1*p->x : 0;
	imgYstart = (p->y < 0) ? -1*p->y : 0;

	imgXend = (p->x + sub_i->w) > SCREEN_W ? (SCREEN_W - p->x): sub_i->w;
	imgYend = (p->y + sub_i->h) > SCREEN_H ? (SCREEN_H - p->y): sub_i->h;

	//Image is entirely offscreen
	if (imgXstart == imgXend || imgYstart == imgYend)
		return;

	//Copy image
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

//This funciton draws the letter "a" passed in by reading the ascii value and finding the coresponding value of the location of the image on the ascii table 
void draw_letter(char a, int *screen_buffer, struct image *i, struct point *p, struct pixel *color) {

        	//printf("draw letter\n" );
    struct sub_image sub_i;    //chosen sub image

    sub_i.row = 23*((0x0F & (a >> 4))%16) + 1;
    sub_i.col = 12*((0x0F & (a >> 0))%16) + 1;
    sub_i.w = 11;
    sub_i.h = 22;


    draw_sub_image(screen_buffer, i, p, &sub_i, color);



}

//This funciton is a loop that calls draw_letter for each character in the string
void draw_string(char s[], int *screen_buffer, struct image *i, struct point *p, struct pixel *color) {
        	//printf("draw string\n" );
    int index = 0;
    while(s[index]) {
    	draw_letter(s[index], screen_buffer, i, p, color);
		p->x += 11;
		index++;
    }


}


//struct vga_ioctl_data data;
int fd;
int image_fd;
int serial_fd;

int main(int argc, char *argv[])
{


time_t startTime = 0;  // start time of application
time_t oneSecStart = 0;  // used to count intervals of 1s
time_t oneSecEnd = 0; // determine if one second ended

struct sysinfo info;
time_t currentTime = 0;
	
	int oflags;
	//int serial_fd1;
//	struct termios tio; 
	char towrite[] = "testaaaaaaaaaaaaaaaaaaaaaaaaaaa";  //write buffer
	char c[100] = {'\0'};	//read buffer
	char charsSentStr[50];
	char charsSentStr1[50];
	int pages=0;
	clock_t programTime;		
	int initialTime = 3;

	struct image img;
    	struct point p;
    	struct point p1;
    	struct point p2;
	
	
	int charsReceived = 0;  //total number of characters received by Kermit since program started
	int charsSent = 0;	//total number of characters sent to Kermit since program started
	unsigned int numCycles = 0; // number of cycles in 1s	
	double readTime = 0; //number of cycles it took to complete read operation
	double writeTime = 0; //number of cycles it took to complete write operation
	double avgReadTime = 0;
	double avgWriteTime = 0;
	double totalReadTime = 0;
	double totalWriteTime = 0;

	int * buffer;

    	int i, j;
    	struct pixel color;
    	struct pixel color1;

    	struct rect r;
    	int row, col;
    	struct stat sb;
	struct termios tio;

	
	//return values of read/write
	int charsWritten = 0;
	int charsRead = 0;
	
	startTime = time(NULL);
	// Register handler for SIGINT, sent when pressing CTRL+C at terminal
	signal(SIGINT, &sigint_handler);

	// Open device driver file
	if (!(fd1 = open("/dev/timer_driver", O_RDWR))) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	
	
	 
	// Rune the timer for 1 second to get the number of clock ticks per second

	// Inititalize the control register bits
	data.offset = CONTROL_REG;
	data.data = T0INT;
	ioctl(fd1, TIMER_WRITE_REG, &data);

	// Reset the counter to 0 (load register)
	data.offset = LOAD_REG;
	data.data = 0x0;
	ioctl(fd1, TIMER_WRITE_REG, &data);

	// Set control bits to load value in load register into counter
	// move value from load register into timer register
	data.offset = CONTROL_REG;
	data.data = LOAD0;
	ioctl(fd1, TIMER_WRITE_REG, &data);

	sleep(1);

	// Set control bits to enable timer, count up
	data.offset = CONTROL_REG;
	data.data = ENT0;
	ioctl(fd1, TIMER_WRITE_REG, &data);

	sleep(1);

	// Clear control bits to disable timer
	data.offset = CONTROL_REG;
	data.data = 0x0;
	ioctl(fd1, TIMER_WRITE_REG, &data);

	// Read value from timer
	numCycles = read_timer();

	//printf("timer value after 1 seconds  = %u\n", numCycles);
	//printf("there are this many clock cycles in 1ms\n");

		


	memset(&tio, 0, sizeof(tio));
    
	fd = open("/dev/vga_driver",O_RDWR);
	image_fd = open("/home/root/example2.raw",O_RDONLY);  
		
	if(image_fd  == -1){
			printf("Failed to open image... :( \n");
	}
		
	//serial_fd = open("/dev/ttyS1",O_RDWR);

	serial_fd = open("/dev/ttyPS0",O_RDWR|O_NONBLOCK);


	if(serial_fd  == -1){
			printf("Failed to open serial port... :( \n");
	}

	tcgetattr(serial_fd, &tio);
	cfsetospeed(&tio, B115200);
	cfsetispeed(&tio, B115200);
	tio.c_lflag = tio.c_lflag & ~(ICANON);
	tcsetattr(serial_fd, TCSANOW, &tio);

	if(fd != -1){
		
		buffer = (int*)mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		
		//printf(    	struct pixel color1;"buffer addr: 0x%08x\n", buffer);

        fstat (image_fd, &sb);
        img.mem_loc = (int*)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, image_fd, 0);
        if ( img.mem_loc == MAP_FAILED) {
                  perror ("mmap");
          } 
    	printf("image addr: 0x%08x\n", img.mem_loc );
    	

        for (row = 0; row < 480; row++) {
            for (col = 0; col < 640; col++) {
                    	*(buffer + col + row*640) = 0x00000000;
    	    }
    	}
    	printf("screen cleared\n" );
    	  	
    	  p.x = 20;
    	  p.y = 20;


    	    	
    	color.r = 128;
        color.g = 128;
        color.b = 128;
        color.a = 255;
        
    	img.w = 192;                                                
        img.h = 368;


        r.x = 0;
        r.y = 230;
        r.w = 680;
        r.h = 20;
//Draw the rectangle that devides the two parts of the screen
        draw_rectangle(buffer, &r, &color);                               
            msync(buffer, BUFFER_SIZE, MS_SYNC);


    	color.r = 255;
        color.g = 255;
        color.b = 255;


    	color1.r = 0;
        color1.g = 0;
        color1.b = 0;
        color1.a = 255;

	oneSecStart = time(NULL); // start one second count
            
	    char c;
            char previous=' ';
       while(1) {

        	//fflush(stdout);
            

		charsSent++;
            
		data.offset = LOAD_REG;
		data.data = 0x0;
		ioctl(fd1, TIMER_WRITE_REG, &data);
		
		// move value from load register into timer register
		data.offset = CONTROL_REG;
		data.data = LOAD0;
		ioctl(fd1, TIMER_WRITE_REG, &data);
		initialTime = read_timer(); 
	
		// Set control bits to enable timer, count up
		data.offset = CONTROL_REG;
		data.data = ENT0;
		ioctl(fd1, TIMER_WRITE_REG, &data);

	    charsRead = read(serial_fd, &c, 1);

		// Clear control bits to disable timer
		data.offset = CONTROL_REG;
		data.data = 0x0;
		ioctl(fd1, TIMER_WRITE_REG, &data);

		if(charsRead == 1)
		{
			//This allows the user to clear the screen
			if((c=='c')&&(previous=='/')){
				//Clear screen
				for (row = 0; row < 230; row++) {
				        for (col = 0; col < 640; col++) {
				                	*(buffer + col + row*640) = 0x00000000;
					    }
					}
    	  			p.x = 20;
    	  			p.y = 20;
				continue;
			}

			//This allows the user to reset the screen
			if((c=='r')&&(previous=='/')){
				//Clear screen
				for (row = 0; row < 230; row++) {
				        for (col = 0; col < 640; col++) {
				                	*(buffer + col + row*640) = 0x00000000;
					    }
					}
    	  			p.x = 20;
    	  			p.y = 20;
			charsReceived = 0;	
			avgReadTime = 0;
			totalReadTime = 0;
			pages = 0;
			startTime = time(NULL);
				continue;
			}
			previous=c;

			readTime = read_timer(); 
			totalReadTime = totalReadTime + readTime;
			charsReceived++;	
			avgReadTime = totalReadTime / charsReceived;



//Setting the colour to allow characters to by typed to the screen
	color.r = 12;
        color.g = 216;
        color.b = 100;
            draw_letter(c, buffer, &img, &p, &color);


//Printing a string to the screen
    	  p1.x = 20;
    	  p1.y = 270;
	    draw_string("============= System Preformance: ==============", buffer,  &img, &p1, NULL);
     	  p1.x = 20;
    	  p1.y = p1.y+22;



        
//Printing a string to the screen, one line down

	r.x = 20+21*11;
        r.y = p1.y;
        r.w = 300;
        r.h = 44;

        draw_rectangle(buffer, &r, &color1);                               //fixed
	sprintf(charsSentStr1, "initial timer value: %d", initialTime);
	draw_string(charsSentStr1, buffer,  &img, &p1, NULL);
    	  



	  p1.x = 20;
    	  p1.y = p1.y+22;



	  sprintf(charsSentStr, "Characters received: %d", charsReceived);
//Printing a string to the screen, one line down

	    draw_string(charsSentStr, buffer,  &img, &p1, NULL);  	  	    
            p1.x = 20;
    	  p1.y = p1.y+22;

		  
//Printing a string to the screen, one line down

	r.x = 20+17*11;
        r.y = p1.y;
        r.w = 300;
        r.h = 22;
        draw_rectangle(buffer, &r, &color1);                               //fixed
	  sprintf(charsSentStr1, "Number of pages: %i", pages);
	  draw_string(charsSentStr1, buffer,  &img, &p1, NULL);
    	  p1.x = 20;
    	  p1.y = p1.y+22;
	  
	  
	  //Printing a string to the screen, one line down

	r.x = 20+30*11;
        r.y = p1.y;
        r.w = 300;
        r.h = 22;
        draw_rectangle(buffer, &r, &color1);
	  sprintf(charsSentStr1, "Average read time in seconds: %f", avgReadTime/numCycles);
	  draw_string(charsSentStr1, buffer,  &img, &p1, NULL);
	p1.x = 20;
    	  p1.y = p1.y+22;
	
 	draw_string("~~~ Statistics for Time ~~~", buffer,  &img, &p1, NULL);
	p1.x = 20;
    	  p1.y = p1.y+22+22+22;



//Printing a string to the screen, one line down

 	draw_string("Commands For Program: /c (Clear letters), /r (reset)", buffer,  &img, &p1, NULL);

           if( p.x >= SCREEN_W - CHARACTER_W || c == 10)
           {
               //line_end[y/CHARACTER_H] = x;
               p.x = 10;
               p.y = p.y + CHARACTER_H;
           }
           
          // int base = (CHARACTER_W+1)*(c%16) + (16*(CHARACTER_W+1)*(CHARACTER_H+1)*(c/16)) + 193;
           p.x+=11;
    	   if (p.x > 640-12*2 ||p.y > 230-23) {
                p.x = 20;
                p.y += 22; //new line
                if (p.y > 230-23) {
                    p.y = 20;

                    for (row = 0; row < 230; row++) {
                        for (col = 0; col < 640; col++) {
                                	*(buffer + col + row*640) = 0x00000000;
                	    }
                	}

pages++; //increment the number of times the pages has had to automatically clear itself

		}
           }

		
		}
		else {
		printf("no characters to be read\n");
		}

    	oneSecEnd = time(NULL);

	if(oneSecEnd - oneSecStart >= 1) // if 1 second is up
	{
		oneSecStart = time(NULL); // get new starting time of new 1 second

	//The below two print functions will be printed every second and not when a character is entered
	p2.x = 20;
    	  p2.y = 270+6*22;
	r.x = 20+28*11;
        r.y = 270+6*22;
        r.w = 300;
        r.h = 22;
        draw_rectangle(buffer, &r, &color1);
	sysinfo(&info);
	sprintf(charsSentStr1, "Time since boot in seconds: %ld", info.uptime);	
	draw_string(charsSentStr1, buffer,  &img, &p2, NULL);
	p2.x = 20;
    	  p2.y = 270+7*22;


	r.x = 20+32*11;
        r.y = 270+7*22;
        r.w = 300;
        r.h = 22;
        draw_rectangle(buffer, &r, &color1);
	currentTime = time(NULL);
	sprintf(charsSentStr1, "Time of application in seconds: %ld", currentTime - startTime);	
	draw_string(charsSentStr1, buffer,  &img, &p2, NULL);

	}

	
}
	
/////////////////////////////////////////////////////////////////////////////////////////////////
//The below source code is never entered
/////////////////////////////////////////////////////////////////////////////////////////////////
		msync(buffer, BUFFER_SIZE, MS_SYNC);
    	
    	printf("buffer synced\n" );
	
    	color.r = 10;
        color.g = 156;               
        color.b = 114;
        color.a = 255;

        r.x = 100;
        r.y = 50;
        r.w = 100;
        r.h = 100;

        draw_rectangle(buffer, &r, &color);                               //fixed
            msync(buffer, BUFFER_SIZE, MS_SYNC);
	    
        img.w = 192;
        img.h = 368;
	
        p.x = 0;
        p.y = 0;
		
        draw_image(buffer, &img, &p, NULL);


    	color.r = 10;
        color.g = 156;               
        color.b = 114;
        color.a = 255;

        r.x = 100;
        r.y = 50;
        r.w = 100;
        r.h = 100;

        draw_rectangle(buffer, &r, &color);                               //fixed
            msync(buffer, BUFFER_SIZE, MS_SYNC);
	
        p.x = 50;
        p.y = 10;
	
        draw_string("Test String A", buffer,  &img, &p, NULL);
	
        p.x = 362;
        p.y = 256;

        color.r = 128;
        color.g = 64;
        color.b = 10;

        draw_string("Blah Blah", buffer,  &img, &p, &color);


    msync(buffer, BUFFER_SIZE, MS_SYNC);
	
   // close(?);                                                  
   // close(?);                                                    

	}else{
		printf("Failed to open driver... :( \n");
	}
	return 0;
}


