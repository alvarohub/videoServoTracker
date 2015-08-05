/* 
   Name: servoVideoTraker (aka "shermanTracker").
   Description: Face tracker using haar cascade filters (front and side), and two rotations and flips of the images (using fbo) to
                make the tracking more robust to poses.
   Ver. 5.8.2015 for OF 0.8.4
   Author: Alvaro Cassinelli
 
   NOTES: 1) This program send data to the Arduino running sketch servoVideoTrackerArduinoPID.ino
             PID control is implemented ON THE ARDUINO, not here.
          2) The program uses ofxPS3EyeGrabber.h library, and is designed to work with Sony PS3 camera but could easily work with another type of camera by UNCOMMENTING the #define USE_PS3_CAMERA. There seems to be a problem with the modified grabber for sony PS3, so I needed to do some hacking to get the relevant data for the tracker.
*/


#pragma once

#include "ofMain.h"
#include "ofxCvHaarFinder.h"
#include "ofxPS3EyeGrabber.h"
#include "ofxCv.h"

//#define USE_PS3_CAMERA // comment this to use other camera (e.g., the laptop camera)

// COMMANDS for serial control of the Arduino:
#define NUMBER_END '#'
#define DIRECT_POS 'A'
#define TARGET_POS 'B'
#define SET_PROPORTIONAL_MODE 'C'
#define SET_DIRECT_MODE 'D'
#define STAND_BY 'E'
#define RESUME 'F'

#define PERIOD_SEND 0 //50
// NOTE: in milliseconds. Putting 0 means it will send every time we compute a new face. This is ok, because it cannot be faster than the camera frame rate which is not so fast.

#define RESET_POSITION_PERIOD 2000

// Experimental "shy" mode (avoid the camera)
#define MIN_SHY_TIME 500
#define MAX_SHY_TIME 3000

class ofApp : public ofBaseApp{
public:
    void setup();
    void update();
    void draw();
    
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y );
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);
    
#ifdef USE_PS3_CAMERA
    ofxPS3EyeGrabber vidGrabber;
#else
    ofVideoGrabber vidGrabber;
#endif
    
    int camWidth, camHeight, camFrameRate;
    ofPoint cameraCenter;
    
    ofxCvHaarFinder finderFrontal, finderSide;
    
    ofImage img, imgR1, imgR2;
    ofxCvGrayscaleImage grayCvImage, grayCvImageR1, grayCvImageR2;
    
    ofFbo myFbo;
    void rotateFBO(float angle, ofImage& from, ofImage& to);
    
    vector<ofRectangle> detectedFaces; // this is to keep track of the detected centers from all the cascades (it will be cleared during the update)
    ofRectangle finalCenter;// this is the final computed rectangle.
    ofRectangle smoothedCenter;// this is the final smoothed tracking rectangle.
    float alpha;
    bool firstTimeTrack;
    bool trackingMode;
    
    ofSerial	mySerial;
    void sendSerialCommand(ofPoint&);
 
    unsigned long lastTimeSend, lastAvoidLook, shyTime, lastTimeFace;
    
    int senseMove;
    
    uint8_t testMode;

};
