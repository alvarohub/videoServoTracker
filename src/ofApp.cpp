#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    
    ofBackground(0,0,0);
    ofSetFrameRate(30);
    
    camWidth 		= 320;//640;	// try to grab at this size.
    camHeight 		= 240;//480;
    camFrameRate=30;
    cameraCenter.set(camWidth/2, camHeight/2);
    
    // auxiliary framebuffer to create rotations using graphic card:
    myFbo.allocate(camWidth, camHeight, GL_RGBA);
    myFbo.begin();
    ofClear(255,255,255, 0);
    myFbo.end();
    
    finderFrontal.setup("haarcascade_frontalface_default.xml");
    // The default value is 1.2. For accuracy, bring it closer but not equal to 1.0. To make it faster, use a larger value.
    finderFrontal.setScaleHaar(1.4);
    // How many neighbors can be grouped into a face? Default value is 2. If set to 0, no grouping will be done.
    finderFrontal.setNeighbors(2);
    
    finderSide.setup("haarcascade_profileface.xml"); // note: this checks only faces looking to the left
    finderSide.setScaleHaar(1.5);
    // How many neighbors can be grouped into a face? Default value is 2. If set to 0, no grouping will be done.
    finderSide.setNeighbors(2);
    
    
    //we can now get back a list of devices.
    std::vector<ofVideoDevice> devices = vidGrabber.listDevices();
    for(std::size_t i = 0; i < devices.size(); ++i)
    {
        std::stringstream ss;
        ss << devices[i].id << ": " << devices[i].deviceName;
        if(!devices[i].bAvailable) ss << " - unavailable ";
        ofLogNotice("ofApp::setup") << ss.str();
    }
    
    vidGrabber.setDeviceID(0);
    //vidGrabber.setPixelFormat(OF_PIXELS_RGBA); // not necessary: the modified PS3 grabber does not work with any other mode.
    vidGrabber.setDesiredFrameRate(camFrameRate);
    vidGrabber.initGrabber(camWidth, camHeight);
    
#ifdef USE_PS3_CAMERA
    vidGrabber.setAutogain(false);
    vidGrabber.setAutoWhiteBalance(false);
#endif
    
    img.allocate(camWidth,camHeight, OF_IMAGE_COLOR);
    imgR1.allocate(camWidth,camHeight,OF_IMAGE_COLOR);
    imgR2.allocate(camWidth,camHeight, OF_IMAGE_COLOR);
    
    grayCvImage.allocate(camWidth,camHeight);
    grayCvImageR1.allocate(camWidth,camHeight);
    grayCvImageR2.allocate(camWidth,camHeight);
    
    alpha=.6;
    firstTimeTrack=true;
    trackingMode=false;
    
    // SERIAL COMMUNICATION:
    mySerial.listDevices();
    vector <ofSerialDeviceInfo> deviceList = mySerial.getDeviceList();
    mySerial.setup(0, 115200); //open the first device
    //mySerial.setup("COM4", baud); // windows example
    //mySerial.setup("/dev/tty.usbserial-A4001JEC", baud); // mac osx example
    //mySerial.setup("/dev/ttyUSB0", baud); //linux example
    
    
    lastTimeSend=lastAvoidLook=lastTimeFace=ofGetElapsedTimeMillis();
    
    testMode=0b001;//0b111;
    senseMove=1.0; // if -1, then the camera will AVOID the face... ;)
}

void ofApp::rotateFBO(float angle, ofImage& from, ofImage& to) {
    myFbo.begin();
    ofPushMatrix();
    ofTranslate(camWidth/2, camHeight/2);
    ofRotateZ(angle);
    ofTranslate(-camWidth/2, -camHeight/2);
    from.draw(0,0);
    ofPopMatrix();
    myFbo.end();
    // Get the rotated image on grayImageR1 and grayImageR2:
    ofPixels pixels;
    myFbo.readToPixels(pixels);
    to.setFromPixels(pixels);
    to.update();
}

//--------------------------------------------------------------
void ofApp::update(){
    ofBackground(100,100,100);
    vidGrabber.update();
    
    if (vidGrabber.isFrameNew()){
        
        
        // ****This is a hack***
        // It seems that the ps3 video grabber does not work properly and I cannot DIRECTLY generate the cv image from it.
        img.setFromPixels(vidGrabber.getPixelsRef());
        ofxCv::convertColor(img.getPixelsRef(), grayCvImage.getPixelsRef(), CV_RGB2GRAY);
        grayCvImage.flagImageChanged();
    
        
        // 1) DETECT FACES
        // NOTE: use a ROI from the last detected zone? better not, just track what we find on the whole image.
        // We will rotate/flip the image, draw on a fbo and detect, or use different cascade filters for tilt and side..
        detectedFaces.clear();
        
        if (testMode&0b001) {
            // Check frontal face:
            finderFrontal.findHaarObjects(grayCvImage, 50,50);
            if (finderFrontal.blobs.size()>0) // track only first blob:
                detectedFaces.push_back(finderFrontal.blobs[0].boundingRect);
        }
        if (testMode&0b010) {
            // Check faces to the left:
            finderSide.findHaarObjects(grayCvImage, 50,50);
            if (finderSide.blobs.size()>0) // track only first blob:
                detectedFaces.push_back(finderSide.blobs[0].boundingRect);
            // Check faces to the right:
            grayCvImage.mirror( false, true ); // flip vertical, horizonal
            finderSide.findHaarObjects(grayCvImage);
            if (finderSide.blobs.size()>0) {// track only first blob
                ofRectangle toFlip=finderSide.blobs[0].boundingRect;
                detectedFaces.push_back(ofRectangle(camWidth-toFlip.x-toFlip.width, toFlip.y, toFlip.width, toFlip.height)); // note that here the blob coordinates are also flipped!
            }
        }
        if (testMode&0b100) {
            // Also check frontal faces with slight tilts:
            // Rotate (ofImage) image a little towards both sides using a fbo:
            rotateFBO(15, img, imgR1);
            ofxCv::convertColor(imgR1.getPixelsRef(), grayCvImageR1.getPixelsRef(), CV_RGB2GRAY);
            grayCvImageR1.flagImageChanged();
            
            rotateFBO(-15, img, imgR2);
            ofxCv::convertColor(imgR2.getPixelsRef(), grayCvImageR2.getPixelsRef(), CV_RGB2GRAY);
            grayCvImageR2.flagImageChanged();
            
            finderFrontal.findHaarObjects(grayCvImageR1, 50,50);
            if (finderFrontal.blobs.size()>0) { // track only first blob. Attention! blobs are rotated! but I won't care about de-rotating them here because the tilt is small...
                detectedFaces.push_back(finderFrontal.blobs[0].boundingRect);
            }
            finderFrontal.findHaarObjects(grayCvImageR2, 50,50);
            if (finderFrontal.blobs.size()>0) { // track only first blob. Attention! blobs are rotated! but I won't care about de-rotating them here because the tilt is small...
                detectedFaces.push_back(finderFrontal.blobs[0].boundingRect);
            }
        }
        
        // Compute the final rectangle by averaging the detected rectangles if they are not too far from each other:
        // For the time being, just average:
        
        ofPoint deltaPos(0,0);
        
        if (detectedFaces.size()>0) {
            finalCenter.set(0,0,0,0);
            
            for (int i=0; i<detectedFaces.size(); i++) {
                finalCenter.x+=detectedFaces[i].x;
                finalCenter.y+=detectedFaces[i].y;
                finalCenter.width=MAX(detectedFaces[i].width, finalCenter.width);
                finalCenter.height=MAX(detectedFaces[i].height, finalCenter.height);
            }
            finalCenter.x/=detectedFaces.size();
            finalCenter.y/=detectedFaces.size();
            
            // Use a recursive filter to smooth the detection (better Kalman in the future):
            if (firstTimeTrack) {smoothedCenter=finalCenter; firstTimeTrack=false;}
            smoothedCenter.x=alpha*smoothedCenter.x+(1-alpha)*finalCenter.x;
            smoothedCenter.y=alpha*smoothedCenter.y+(1-alpha)*finalCenter.y;
            smoothedCenter.width=alpha*smoothedCenter.width+(1-alpha)*finalCenter.width;
            smoothedCenter.height=alpha*smoothedCenter.height+(1-alpha)*finalCenter.height;
            
            // If there was a detected face, send the recentering error (that will control the SPEED of the servos through the PID controller in the Arduino).
            deltaPos.set(smoothedCenter.getCenter().x-cameraCenter.x,smoothedCenter.getCenter().y-cameraCenter.y) ;
            
            if (senseMove==-1) {
                lastAvoidLook=ofGetElapsedTimeMillis();
                shyTime=(unsigned long)ofRandom(MIN_SHY_TIME, MAX_SHY_TIME);
            }
            
            // Send data serially if necessary:
            if (trackingMode) sendSerialCommand(deltaPos);
            
            lastTimeFace=ofGetElapsedTimeMillis();
        }
        else { // this means no face was detected:
            
            // reset position after a while if there was no face detected:
            if (ofGetElapsedTimeMillis()-lastTimeFace>RESET_POSITION_PERIOD) {
                mySerial.writeByte('R'); // reset position
            }
            else  if (trackingMode) sendSerialCommand(deltaPos); // if no face was found, error is 0
            
        }
    }
}

void ofApp::sendSerialCommand(ofPoint& error) {
    if (ofGetElapsedTimeMillis()-lastTimeSend>PERIOD_SEND) {
        
        int dx=ofMap(error.x, -camWidth, camWidth, -100, 100, true)*senseMove;
        int dy=ofMap(error.y, -camHeight, camHeight, -100, 100, true)*senseMove;
        
        if (senseMove==-1) {
            if (ofGetElapsedTimeMillis()-lastAvoidLook>shyTime) {
                mySerial.writeByte('R'); // reset position
            }
            dx=dx*2;dy=dy*2;
        }
        
        string msg;
        msg = ofToString(dx)+"#"+ofToString(dy)+"#";
        mySerial.writeBytes((unsigned char*)msg.c_str(), msg.size());
        
        lastTimeSend=ofGetElapsedTimeMillis();
    }
    
}

//--------------------------------------------------------------
void ofApp::draw(){
    
    
    ofNoFill();
    
    ofPushMatrix();
    
    ofScale(2.0, 2.0);
    img.draw(0,0);//, 640, 480);
    
    if (detectedFaces.size()>0) {
        ofCircle(smoothedCenter.getCenter().x, smoothedCenter.getCenter().y, 20);
        ofRect(smoothedCenter.x, smoothedCenter.y, smoothedCenter.width, smoothedCenter.height);
    }
    
    // grayCvImageR1.draw(camWidth,0);
    // grayCvImageR2.draw(2*camWidth,0);
    
    
    ofPopMatrix();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (key == 's' || key == 'S'){
        vidGrabber.videoSettings();
    }
    
    if (key=='+') alpha*=1.1;
    if (key=='-') alpha/=1.1;
    
    if (key=='1') testMode^=0b1;
    if (key=='2') testMode^=0b10;
    if (key=='3') testMode^=0b100;
    
    if (key=='b') senseMove=-1*senseMove;
    
    // Modes and control:
    if (key==' ') {
        trackingMode=!trackingMode;
        if (trackingMode==false) {
            mySerial.writeByte('S'); // stop tracking
            mySerial.writeByte('R'); // reset position
            firstTimeTrack=true; // prepare to START tracking again
        } else {
            mySerial.writeByte('T'); // resume tracking
        }
    }
}


//--------------------------------------------------------------
void ofApp::keyReleased(int key){
    
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
    
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
    
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){
    
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){
    
}
