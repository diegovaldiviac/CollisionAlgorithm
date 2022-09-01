



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