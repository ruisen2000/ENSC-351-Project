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
int charsReceived = 0;  //total number of characters received by Kermit since program started
int charsSent = 0;	//total number of characters sent to Kermit since program started
unsigned int numCycles = 0; // number of cycles in 1s	
double readTime = 0; //number of cycles it took to complete read operation
double writeTime = 0; //number of cycles it took to complete write operation
double avgReadTime = 0;
double avgWriteTime = 0;
double totalReadTime = 0;
double totalWriteTime = 0;
time_t startTime = 0;  // start time of application

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




///Copied functions

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
int *setup_vga()
{
	fd = open("/dev/vga_driver",O_RDWR);
	if (fd < 0) {
		printf("FAILED TO OPEN DRIVER");
	}
	return (int*)mmap(NULL,BUFFER_SIZE,PROT_READ |
	PROT_WRITE, MAP_SHARED, fd, 0);
} 
*/

/*
 * SIGIO Signal Handler
 */

void sigio_handler(int signum) 
{
	printf("Received Signal, signum=%d (%s)\n", signum, strsignal(signum));

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


//End of copied functions



//Start of drawing funcitons
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

			dest_p.b = (((int)src_p.b)*src_p.a + ((int)dest_p.b)*(255 - src_p.a))/256;   //combine colour and pixel location for Blue. Taken from code below
			dest_p.g = (((int)src_p.g)*src_p.a + ((int)dest_p.g)*(255 - src_p.a))/256;   //combine colour and pixel location for Green. Taken from code below
			dest_p.r = (((int)src_p.r)*src_p.a + ((int)dest_p.r)*(255 - src_p.a))/256;   //combine colour and pixel location for Red.Taken from code below

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


			dest_p.b = (((int)src_p.b)*src_p.a + ((int)dest_p.b)*(255 - src_p.a))/256;
			dest_p.g = (((int)src_p.g)*src_p.a + ((int)dest_p.g)*(255 - src_p.a))/256;
			dest_p.r = (((int)src_p.r)*src_p.a + ((int)dest_p.r)*(255 - src_p.a))/256;

			*screenP = (dest_p.b << 16) | (dest_p.g << 8) | (dest_p.r << 0);   //Same as above
		}
	}

	return;

}



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

void draw_letter(char a, int *screen_buffer, struct image *i, struct point *p, struct pixel *color) {

        	//printf("draw letter\n" );
    struct sub_image sub_i;    //chosen sub image

    sub_i.row = 23*((0x0F & (a >> 4))%16) + 1;
    sub_i.col = 12*((0x0F & (a >> 0))%16) + 1;
    sub_i.w = 11;
    sub_i.h = 22;


    draw_sub_image(screen_buffer, i, p, &sub_i, color);



}

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



	
	int oflags;
	//int serial_fd1;
//	struct termios tio; 
	char towrite[] = "testaaaaaaaaaaaaaaaaaaaaaaaaaaa";  //write buffer
	char c[100] = {'\0'};	//read buffer
	char charsSentStr[50];
	char charsSentStr1[50];
	int pages=0;
	clock_t programTime;
	struct sysinfo info;
	time_t currentTime = 0;


	int * buffer;
    	struct image img;
    	struct point p;
    	struct point p1;
    	int i, j;
    	struct pixel color;
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

		

	// Set up Asynchronous Notification
	signal(SIGIO, &sigio_handler);
	fcntl(fd1, F_SETOWN, getpid());
	oflags = fcntl(fd1, F_GETFL);
	fcntl(fd1, F_SETFL, oflags | FASYNC);


	// Set one of the timers to keep counting down from 1 second and generating interupts and the signal every second
	/*
	// Load countdown timer initial value for ~1 sec
	data.offset = LOAD_REG;
	data.data = 100e6; // timer runs on 100 MHz clock
	ioctl(fd1, TIMER_WRITE_REG, &data);
	sleep(1);

	// Set control bits to load value in load register into counter
	data.offset = CONTROL_REG;
	data.data = LOAD0;
	ioctl(fd1, TIMER_WRITE_REG, &data);

	// Set control bits to enable timer, enable interrupt, auto reload mode,
	//  and count down
	data.data = ENT0 | ENIT0 | ARHT0 | UDT0;
	ioctl(fd1, TIMER_WRITE_REG, &data);
*/
	/*/ setup connection to kermit
	memset(&tio, 0, sizeof(tio));
	serial_fd1 = open("/dev/ttyPS0",O_RDWR);
	if(serial_fd1 == -1)printf("Failed to open serial port... :( \n");
	tcgetattr(serial_fd1, &tio);
	cfsetospeed(&tio, B115200);
	cfsetispeed(&tio, B115200);
	tcsetattr(serial_fd1, TCSANOW, &tio);*/ 	

	//programTime = clock();
	//programTime = (double)clock()/CLOCKS_PER_SEC; //time since application start
	//printf("Time since application start in seconds: %ld \n", programTime);
	//printf("\n\n=================== System Preformance: ===================== \n\n");







	memset(&tio, 0, sizeof(tio));
    
	fd = open("/dev/vga_driver",O_RDWR);
	image_fd = open("/home/root/example2.raw",O_RDONLY);  
		
	if(image_fd  == -1){
			printf("Failed to open image... :( \n");
	}
		
	//serial_fd = open("/dev/ttyS1",O_RDWR);

	serial_fd = open("/dev/ttyPS0",O_RDWR);


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
		printf("buffer addr: 0x%08x\n", buffer);

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


    	    	
    	color.r = 0;
        color.g = 0;
        color.b = 255;
        color.a = 255;
        
    	img.w = 192;                                                
        img.h = 368;


        r.x = 0;
        r.y = 230;
        r.w = 680;
        r.h = 20;

        draw_rectangle(buffer, &r, &color);                               //fixed
            msync(buffer, BUFFER_SIZE, MS_SYNC);


       while(1) {
            char c;
        	//fflush(stdout);
            

		charsSent++;
            
		data.offset = LOAD_REG1;
		data.data = 0x0;
		ioctl(fd1, TIMER_WRITE_REG, &data);
	
		// move value from load register into timer register
		data.offset = CONTROL_REG1;
		data.data = LOAD0;
		ioctl(fd1, TIMER_WRITE_REG, &data);
			
		// Set control bits to enable timer, count up
		data.offset = CONTROL_REG1;
		data.data = ENT0;
		ioctl(fd1, TIMER_WRITE_REG, &data);

	    charsRead = read(serial_fd, &c, 1);

		// Clear control bits to disable timer
		data.offset = CONTROL_REG1;
		data.data = 0x0;
		ioctl(fd1, TIMER_WRITE_REG, &data);

		if(charsRead == 1)
		{
		readTime = read_timer(); 
		totalReadTime = totalReadTime + readTime;
		charsReceived++;	
		avgReadTime = totalReadTime / charsReceived;
		
		}
		else {
		printf("reading failed\n");
		}

            draw_letter(c, buffer, &img, &p, &color);


		for (row = 250; row < 480; row++) {
                        for (col = 0; col < 640; col++) {
                                	*(buffer + col + row*640) = 0x00000000;
                	    }
                	}
    	  p1.x = 20;
    	  p1.y = 270;
	    draw_string("============= System Preformance: ==============", buffer,  &img, &p1, NULL);
     	  p1.x = 20;
    	  p1.y = p1.y+22;
	    draw_string("Statistics for Writing:", buffer,  &img, &p1, NULL);
    	  p1.x = 20;
    	  p1.y = p1.y+22;
	  sprintf(charsSentStr, "Characters received: %d", charsReceived);
	    draw_string(charsSentStr, buffer,  &img, &p1, NULL);  	  	    
            p1.x = 20;
    	  p1.y = p1.y+22;

	  sprintf(charsSentStr1, "Number of pages: %i", pages);
	  draw_string(charsSentStr1, buffer,  &img, &p1, NULL);
    	  p1.x = 20;
    	  p1.y = p1.y+22;
 	  
	sprintf(charsSentStr1, "read time in cycles: %f", readTime);
	  draw_string(charsSentStr1, buffer,  &img, &p1, NULL);
    	  p1.x = 20;
    	  p1.y = p1.y+22;

	  sprintf(charsSentStr1, "Average read time in seconds: %f", avgReadTime);
	  draw_string(charsSentStr1, buffer,  &img, &p1, NULL);
	p1.x = 20;
    	  p1.y = p1.y+22;
	
 	draw_string("Statistics for Time:", buffer,  &img, &p1, NULL);
	p1.x = 20;
    	  p1.y = p1.y+22;

	sysinfo(&info);
	sprintf(charsSentStr1, "Time since boot in seconds: %ld", info.uptime);	
	draw_string(charsSentStr1, buffer,  &img, &p1, NULL);
	p1.x = 20;
    	  p1.y = p1.y+22;

	currentTime = time(NULL);
	sprintf(charsSentStr1, "Time of application in seconds: %ld", currentTime - startTime);	
	draw_string(charsSentStr1, buffer,  &img, &p1, NULL);
	



          	//color.r = rand() % 255;
            	//color.g = rand() % 255;
           	//color.b = rand() % 255;



		if (c=='a'){// Reset the counter to 0 (load register)
/*
		data.offset = LOAD_REG1;
		data.data = 0x0;
		ioctl(fd1, TIMER_WRITE_REG, &data);
	
		// move value from load register into timer register
		data.offset = CONTROL_REG1;
		data.data = LOAD0;
		ioctl(fd1, TIMER_WRITE_REG, &data);
			
		// Set control bits to enable timer, count up
		data.offset = CONTROL_REG1;
		data.data = ENT0;
		ioctl(fd1, TIMER_WRITE_REG, &data);
		
		// disable timer
		data.offset = CONTROL_REG1;
		data.data = 0x0;
		ioctl(fd1, TIMER_WRITE_REG, &data);	
	
		if(charsWritten == 1) {
		writeTime = read_timer();
		totalWriteTime = totalWriteTime + writeTime;
		charsSent++;	
		avgWriteTime = totalWriteTime / charsSent;	
		
		}
		else {
		printf("writing failed\n");
		}
*/
		/* 
			===  Reset timer for testing reading ===

		*/

		// Reset the counter to 0 (load register)
		data.offset = LOAD_REG1;
		data.data = 0x0;
		ioctl(fd1, TIMER_WRITE_REG, &data);
	
		// move value from load register into timer register
		data.offset = CONTROL_REG1;
		data.data = LOAD0;
		ioctl(fd1, TIMER_WRITE_REG, &data);
			
		// Set control bits to enable timer, count up
		data.offset = CONTROL_REG1;
		data.data = ENT0;
		ioctl(fd1, TIMER_WRITE_REG, &data);
		charsRead = read(serial_fd, &c, 1); // read 1 byte from /dev/ttyPS0, and store it in buffer pointed to by c
		// Note: Read function sleeps if no input is available, so read time includes time waiting for input
			
		// Clear control bits to disable timer
		data.offset = CONTROL_REG1;
		data.data = 0x0;
		ioctl(fd1, TIMER_WRITE_REG, &data);

		if(charsRead == 1)
		{
		readTime = read_timer(); 
		totalReadTime = totalReadTime + readTime;
		charsReceived++;	
		avgReadTime = totalReadTime / charsReceived;
		
		}
		else {
		printf("reading failed\n");
		}
		

	}




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

pages++;

		}
           }
	   }
	

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

