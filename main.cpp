#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include <string>
#include <cmath>
#include <vector>
#include <utility>

using namespace std;


/* Memory */
#define FPGA_ONCHIP_BASE 0xC8000000
#define FPGA_ONCHIP_SPAN 0x0003FFFF
/* Cyclone V FPGA devices */
#define LW_BRIDGE_BASE 0xFF200000
#define PIXEL_BUF_CTRL_BASE 0x00003020
//#define CHAR_BUF_CTRL_BASE 0x00003030
#define LW_BRIDGE_SPAN 0x00DEC700
#define FPGA_BRIDGE 0x0060501C
#define HPS_BRIDGE_SPAN 0x006FFFFF // span

int resample_rgb(int, int);
int get_data_bits(int);

#define STANDARD_X 320
#define STANDARD_Y 240
#define INTEL_BLUE 0x0071C5
#define VALDIVIA_PURPLE 0x990099
#define LA_COTE_ENOJADA 0xFF8000
#define RGB_RESAMPLER_BASE 0xFF203010
#define PI 3.14159265


class DE1SoCfpga {

  public:
    char *pBase ;
    char *pBase_Pixel ;
    char *pBase_Pixel_Back;
    char *pBase_Current;
    int fd ;

  DE1SoCfpga() {
    // Open /dev/mem to give access to physical addresses
    fd = open( "/dev/mem", (O_RDWR | O_SYNC));
    if (fd == -1) { // check for errors in openning /dev/mem
      cout << "ERROR: could not open /dev/mem..." << endl;
      exit(1);
    }

    // Get a mapping from physical addresses to virtual addresses
    char *virtual_base = (char *)mmap (NULL, LW_BRIDGE_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, LW_BRIDGE_BASE);

    if (virtual_base == MAP_FAILED) { // check for errors
      cout << "ERROR: mmap1() failed..." << endl;
      close (fd); // close memory before exiting
      exit(1); // Returns 1 to the operating system;
    } 
    
    // === get VGA pixel addr ====================
    // get virtual addr that maps to physical
    char *vga_pixel_virtual_base = (char *)mmap( NULL, FPGA_ONCHIP_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, FPGA_ONCHIP_BASE);

    if (vga_pixel_virtual_base == MAP_FAILED) {
      printf( "ERROR: mmap3() failed...\n" );
      close(fd);
      exit(1);
    }

    pBase = virtual_base;
    pBase_Pixel = vga_pixel_virtual_base ;

  } // end costructor


  ~DE1SoCfpga() {
    if (munmap (pBase, LW_BRIDGE_SPAN) != 0) {
      cout << "ERROR: munmap() failed..." << endl;
      exit(1);
    }

    if (munmap (pBase_Pixel, FPGA_ONCHIP_SPAN) != 0) {
      cout << "ERROR: munmap() failed..." << endl;
      exit(1);
    }

    close (fd); // close memory
  } // end destructor

}










/********************************************************************************
* Resamples 24-bit color to 16-bit or 8-bit color
*******************************************************************************/

int resample_rgb(int num_bits, int color) {

  if (num_bits == 8) {
    color = (((color >> 16) & 0x000000E0) | ((color >> 11) & 0x0000001C) | ((color >> 6) & 0x00000003));
    color = (color << 8) | color;
  } else if (num_bits == 16) {
    color = (((color >> 8) & 0x0000F800) | ((color >> 5) & 0x000007E0) | ((color >> 3) & 0x0000001F));
  }

  return color;
}

int get_data_bits(int mode) {

  switch (mode) {
    case 0x0:
      return 1;
    case 0x7:
      return 8;
    case 0x11:
      return 8;
    case 0x12:
      return 9;
    case 0x14:
      return 16;
    case 0x17:
      return 24;
    case 0x19:
      return 30;
    case 0x31:
      return 8;
    case 0x32:
      return 12;
    case 0x33:
      return 16;
    case 0x37:
      return 32;
    case 0x39:
      return 40;
  }
}


int main(void) {

  DE1SoCfpga *main = new DE1SoCfpga;
  int video_resolution = main->RegisterRead(PIXEL_BUF_CTRL_BASE + 0x8) ;
  screen_x = video_resolution & 0xFFFF;
  screen_y = (video_resolution >> 16) & 0xFFFF;

  //cout << screen_x << endl;
  //cout << screen_y << endl;

  int rgb_status = main->RegisterRead(RGB_RESAMPLER_BASE - LW_BRIDGE_BASE);
  int db = get_data_bits(rgb_status & 0x3F);

  /* check if resolution is smaller than the standard 320 x 240 */
  res_offset = (screen_x == 160) ? 1 : 0;

  /* check if number of data bits is less than the standard 16-bits */
  col_offset = (db == 8) ? 1 : 0;

  /* update color */
  short background_color1 = resample_rgb(db, VALDIVIA_PURPLE);
  short background_color2 = resample_rgb(db, LA_COTE_ENOJADA);

  float xv;// = rand() % 5 - 5;
  float yv;//= rand() % 5 - 5;
  float xx;// = rand() % 320;
  float yy;// = rand() % 240;

  float width;
  float height;
  int n;

  cout << "How many shapes do you want? ";
  cin >> n;
  //n = 1;
  //cout << n << endl;

  Shape *shape[20];
  Square *leftscreen = new Square(-120, 120, 0, 0, 240, 240, 0, 0, 0, main);
  Square *rightscreen = new Square(440, 120, 0, 0, 240, 240, 0, 0, 0, main);
  Square *topscreen = new Square(160, -160, 0, 0, 320, 320, 0, 0, 0, main);
  Square *bottomscreen = new Square(160, 400, 0, 0, 320, 320, 0, 0, 0, main);

  for (int i = 0; i < n; i++) {

    int type;
    cout << "(0) Triangle \n(1) Square: ";
    cin >> type;
    xv = rand() % 5;
    yv = rand() % 5;
    cout << "Width? ";
    cin >> width;
    cout << "Height? ";
    cin >> height;

    xx = (rand() % (320 - (int)width*2)) + width;
    yy = (rand() % (160 - (int)height*2)) + height;
    cout << "coordinates: "<< xx << ", " << yy << endl;

    if (type == 0) {
      shape[i] = new Triangle(xx, yy, 3, 3, width, height, 0, -2.0, background_color1, main);
      cout << "new triangle" << endl;

    } else if (type == 1) {
      shape[i] = new Square(xx, yy, 3, 3, width, height, 0, -2.0, background_color2, main);
      cout << "new square" << endl;
    }
  }

  while (1) {

    usleep(16*1000);
    main->clear_screen();

    for (int i = 0; i < n; i++){
      shape[i]->horizontalcollision(leftscreen);
      shape[i]->horizontalcollision(rightscreen);
      shape[i]->verticalcollision(topscreen);
      shape[i]->verticalcollision(bottomscreen);

      for (int j = i + 1; j < n; j++) {
        shape[i]->checkcollision(shape[j]);
      }

      shape[i]->move();
      shape[i]->draw();

      } // for loop

  } // while loop

} // main loop