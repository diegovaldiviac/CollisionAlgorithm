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




/********************************************************************************
* FPGA Board and VGA Monitor Class Set Up.
*******************************************************************************/
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


  void RegisterWrite(unsigned int reg_offset, int value) {
    * (volatile unsigned int *)(pBase + reg_offset) = value;
  }

  int RegisterRead(unsigned int reg_offset) {
    return * (volatile unsigned int *)(pBase + reg_offset);
  }

  void write_pixel(int x, int y, short color) {

    if (x < 0 || x >= 320) return;
    if (y < 0 || y >= 240) return;

    * (volatile unsigned int *)(pBase_Pixel + (y<<10) + (x<<1)) = color ;
  }

  /* use write_pixel to set entire screen to black (does not clear the character buffer) */
  void clear_screen() {
    int x, y;
    for (x = 0; x < 320; x++) {
      for (y = 0; y < 240; y++) {
        write_pixel(x, y, 0);
      }
    }
  }

  void video_box(float x, float y, float width, float height, float angle, short pixel_color) {

    int pixel_ptr, row, col;
    float h, a, b;

    // Vector to draw
    float vec_x, vec_y;
    vec_x = cos(angle*PI/ 180.0) / 2;
    vec_y = sin(angle*PI / 180.0) /2;

    // Perp Vector to draw
    float pvc_x, pvc_y;
    pvc_x = -vec_y;
    pvc_y = vec_x;

    a = x - ((width) * vec_x) - ((height) * pvc_x);
    b = y - ((width) * vec_y) - ((height) * pvc_y);
    //cout << a << ", " << b << endl;

    if (angle > 90 || angle < -90) {
      a += width * 2 * vec_x + height * 2 * pvc_x;
      b += width * 2 * vec_y + height * 2 * pvc_y;
      vec_x *= -1;
      vec_y *= -1;
      pvc_x *= -1;
      pvc_y *= -1;
    }

    /* assume that the box coordinates are valid */
    for (float i = 0; i < width * 2; i++) {
      for (float j = 0; j < height * 2; j++) {

        col = round(a + (i * vec_x) + (j * pvc_x));
        row = round(b + (i * vec_y) + (j * pvc_y));
        write_pixel(col, row, pixel_color);
      }
    }
  }


  void video_t(float x, float y, float width, float height, float angle, short pixel_color) {

    int pixel_ptr, row, col;
    float h, a, b;

    // Vector to draw
    float vec_x, vec_y;
    vec_x = cos(angle*PI/ 180.0) / 2;
    vec_y = sin(angle*PI / 180.0) / 2;

    // Perp Vector to draw
    float pvc_x, pvc_y, diag_x, diag_y;
    pvc_x = -vec_y;
    pvc_y = vec_x;

    h = sqrt((height*height) + ((width)*(width)));
    a = x - ((width) * vec_x) - ((height) * pvc_x);
    b = y - ((width) * vec_y) - ((height) * pvc_y);

    float xx = a + ((width)*vec_x) + (height*2*pvc_x);
    float yy = b + ((width)*vec_y) + (height*2*pvc_y);

    diag_x = (xx-a)/h;
    diag_y = (yy-b)/h;

    for (float i = 0; i <= width * 2; i++) {
      for (float j = 0; j <= height*2; j++) {
        col = round(a + (i*vec_x) - (j * diag_x/2));
        row = round(b + (i*vec_y) - (j * diag_y/2));

        if (j <= i) {
          write_pixel(col, row, pixel_color);
        }
      }
    }
  }
}


/********************************************************************************
* Collision for Polygons Implementation.
*******************************************************************************/
class Point {
  public:
    float x, y;

  Point(float x, float y) {
    this->x = x;
    this->y = y;
  }
};

class Edge {
  public:
    float x, y;

  Edge(float x, float y) {
    this->x = x;
    this->y = y;
  }
};

class Polygon {
  public:
    vector<Point> vertices;
    vector<Edge> edges;
    
  Polygon(vector<Point> vertices) {
    this->vertices = vertices;

    for (int i = 0; i < this->vertices.size(); i++) {
      int j = (i + 1) % this->vertices.size();
      Point a = this->vertices[i];
      Point b = this->vertices[j];
      Edge edge(b.x - a.x, b.y - a.y);
      edges.push_back(edge);
    }
  }

  /**
  * Collision Algorithm.
  * Itirates through this polygon against provided other polygon.
  * Returns false if no collision has taken place.
  **/
  bool satCollision(Polygon* &other) {

    vector<Edge> perpendicularStack;

    for (int i = 0; i < this->edges.size(); i++) {
      Edge perpendicularLine(-edges[i].y, edges[i].x);
      perpendicularStack.push_back(perpendicularLine);
    }

    for (int i = 0; i < other->edges.size(); i++) {
      Edge perpendicularLine(-other->edges[i].y, other->edges[i].x);
      perpendicularStack.push_back(perpendicularLine);
    }

    for (int i = 0; i < perpendicularStack.size(); i++) {

      float nothing = 50000000;
      float aLeft = -10000000, aRight = 50000000, bLeft = -10000000, bRight = 50000000;
      float dot;

      for (int j = 0; j < this->vertices.size(); j++) {
        dot = this->vertices[j].x * perpendicularStack[i].x +
        this->vertices[j].y * perpendicularStack[i].y;

        if (aLeft == nothing || dot > aLeft){
          aLeft = dot;
        }
        if (aRight == nothing || dot < aRight){
          aRight = dot;
        }
      }

      for (int j = 0; j < other->vertices.size(); j++) {
        dot = other->vertices[j].x * perpendicularStack[i].x +
        other->vertices[j].y * perpendicularStack[i].y;

        if (bLeft == nothing || dot > bLeft){
          bLeft = dot;
        }
        if (bRight == nothing || dot < bRight){
          bRight = dot;
        }
      }

    if ((aRight < bLeft && aRight > bRight) || (bRight < aLeft && bRight > aRight)) { continue; }
    else { return false; }

    }
    return true;
  }
}



/********************************************************************************
* Shapes
*******************************************************************************/
class Shape {
  public:
    float x, y, xv, yv, width, height, angle, angular_vel;
    short pixel_color;
    Polygon *p;
    DE1SoCfpga *mem;

  Shape(float x, float y, float xv, float yv, float width, float height, float angle, float angular_vel, short pixel_color, DE1SoCfpga *mem) {
    this->x = x;
    this->y = y;
    this->xv = xv;
    this->yv = yv;
    this->width = width;
    this->height = height;
    this->angle = angle;
    this->angular_vel = angular_vel;
    this->pixel_color = pixel_color;
    this->mem = mem;
  }

  // Actual implementations in children class overide the inherited method.
  virtual void draw() = 0;
  virtual vector<Point> corners() = 0;

  void move() {
    x = x + xv;
    y = y + yv;
    angle = (angle + angular_vel);
    // Corner cases
    if (angle > 180) { angle -= 360.0; }
    if (angle < -180) { angle += 360.0; }

    p = new Polygon(corners());
  }

  void bounceHorizontal() { xv = -xv;}

  void bounceVertical() {yv = -yv;}

  void horizontalcollision(Shape* other) {
    if (p->satCollision(other->p) == 1) {
      bounceHorizontal();
      angular_vel = -angular_vel;
      move();
    }
  }

  void verticalcollision(Shape* other) {
    if (p->satCollision(other->p) == 1) {
      bounceVertical();
      angular_vel = -angular_vel;
      move();
    }
  }

  void checkcollision(Shape* other) {

    if (p->satCollision(other->p) == 1) {

      float diffX = x - other->x;
      float diffY = y - other->y;
      float diffM = sqrt(diffX*diffX + diffY*diffY);
      diffX /= diffM;
      diffY /= diffM;

      float thisM = sqrt(xv*xv + yv*yv);
      this->yv = diffY * thisM;
      float otherM = sqrt(other->xv*other->xv + other->yv*other->yv);
      other->xv = -diffX * otherM;
      other->yv = -diffY * otherM;

      move();
      other->move();
    }
  }
};


class Square : public Shape {
  public:

  Square(float x, float y, float xv, float yv, float width, float height, float angle, float angular_vel, short pixel_color, DE1SoCfpga *mem) 
    : Shape(x, y, xv, yv, width, height, angle, angular_vel, pixel_color, mem) {
      p = new Polygon(corners());
  }

  void draw() {
    mem->video_box(x, y, width, height, angle, pixel_color);
  }

  vector<Point> corners() {

    vector<Point> corner;
    float a, b;
    float vec_x, vec_y;
    float cox, coy, c1x, c1y, c3x, c3y;

    vec_x = cos(angle*PI/ 180.0) / 2;
    vec_y = sin(angle*PI / 180.0) / 2;
    // Perp Vector to draw
    float pvc_x, pvc_y;
    pvc_x = -vec_y;
    pvc_y = vec_x;

    a = x - ((width) * vec_x) - ((height) * pvc_x);
    b = y - ((width) * vec_y) - ((height) * pvc_y);
    cox = (a + ((width*2)*vec_x) + ((height*2)*pvc_x));
    coy = (b + ((width*2)*vec_y) + ((height*2)*pvc_y));
    c1x = a + (width*2*vec_x);
    c1y = b + (width*2*vec_y);
    c3x = a + (height*2*pvc_x);
    c3y = b + (height*2*pvc_y);

    Point a1(a, b);
    Point a2(c1x, c1y);
    Point a3(cox, coy);
    Point a4(c3x, c3y);

    corner.push_back(a1);
    corner.push_back(a2);
    corner.push_back(a3);
    corner.push_back(a4);

    return corner;
  }
};


class Triangle : public Shape {
  public:

  Triangle(float x, float y, float xv, float yv, float width, float height, float angle, float angular_vel, short pixel_color, DE1SoCfpga *mem) 
    : Shape(x, y, xv, yv, width, height, angle, angular_vel, pixel_color, mem) {
      p = new Polygon(corners());
  }

  void draw() {
    mem->video_t(x, y, width, height, angle, pixel_color);
  }

  vector<Point> corners() {

    vector<Point> corner;
    float h, a, b;
    float vec_x, vec_y;
    float cox, coy, c1x, c1y;

    vec_x = cos(angle*PI/ 180.0) / 2;
    vec_y = sin(angle*PI / 180.0) / 2;
    // Perp Vector to draw
    float pvc_x, pvc_y;
    pvc_x = -vec_y;
    pvc_y = vec_x;

    a = x - ((width) * vec_x) - ((height) * pvc_x);
    b = y - ((width) * vec_y) - ((height) * pvc_y);
    cox = (a + ((width*2)*vec_x));
    coy = (b + ((width*2)*vec_y));
    c1x = a + ((width)*vec_x) - (height*2*pvc_x);
    c1y = b + ((width)*vec_y) - (height*2*pvc_y);

    Point a1(a, b);
    Point a2(cox, coy);
    Point a3(c1x, c1y);
    corner.push_back(a1);
    corner.push_back(a3);
    corner.push_back(a2);
    return corner;
  }
};





/* global variables */
int screen_x;
int screen_y;
int res_offset;
int col_offset;
/********************************************************************************
* Main Process
*******************************************************************************/
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

    for (int i = 0; i < n; i++) {
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