#include "ofMain.h"
#include "ofxZeroMQ.h"

class ofApp : public ofBaseApp {
    ofxZeroMQPublisher pub;
    ofVideoGrabber grabber;
public:
    inline virtual void setup() override {
        pub.bind("ipc:///tmp/ofxZeroMQPubSubTest");
        grabber.setup(1280, 720);
        
        ofSetWindowPosition(0, 0);
        ofSetWindowTitle("pub-example");
    }
    inline virtual void update() override {
        pub.sendMultipart("mouse", ofGetMouseX(), ofGetMouseY());
        grabber.update();
        if(grabber.isFrameNew()) {
            pub.sendMultipart("grabber", grabber);
        }
    }
    inline virtual void draw() override {
        grabber.draw(0, 0);
        ofDrawCircle(ofGetMouseX(), ofGetMouseY(), 10);
    }
    
    inline virtual void keyPressed(int key) override {
        if(key == ' ') {
            std::vector<std::uint8_t> data1{{0, 1, 2}};
            std::vector<std::uint8_t> data2{{3, 4, 5}};
            pub.sendMultipart("data", data1, data2);
        }
    }
};

int main() {
    ofSetupOpenGL(400, 400, OF_WINDOW);
    ofRunApp(new ofApp());
}
