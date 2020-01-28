#include "ofMain.h"
#include "ofxZeroMQ.h"

class ofApp : public ofBaseApp {
    ofxZeroMQSubscriber sub;
    ofxZeroMQPublisher pub;
    ofVideoGrabber grabber;
    ofImage image;
public:
    inline virtual void setup() override {
        sub.connect("tcp://localhost:26666");
        pub.bind("tcp://*:26666");
        grabber.setup(640, 480);
        ofSetWindowPosition(640, 480);
        ofSetWindowTitle("multipartmessage-example");
    }
    inline virtual void update() override {
        grabber.update();
        if(grabber.isFrameNew()) {
            pub.sendMultipart("video", grabber);
        }
        while(sub.hasWaitingMessage()) {
            ofxZeroMQMultipartMessage m;
            sub.getNextMessage(m);
            std::string type = m[0].get<std::string>();
            if(type == "space") {
                int x = m[1];
                ofColor c1 = m[2];
                ofShortColor c2 = m[3];
                ofLogNotice("space[1]: int          ") << x;
                ofLogNotice("space[2]: ofColor      ") << c1;
                ofLogNotice("space[3]: ofShortColor ") << c2;
            } else if(type == "return") {
                ofLogNotice("return[1]: long        ") << m[1].get<long>();
                ofLogNotice("return[2]: ofRectangle ") << m[2].get<ofRectangle>();
                ofLogNotice("return[3]: glm::vec4   ") << m[3].get<glm::vec4>();
            } else if(type == "video") {
                image = m[1];
                image.update();
            }
        }
    }
    inline virtual void draw() override {
        if(image.isAllocated()) {
            image.draw(0, 0);
        }
    }
    inline virtual void keyPressed(int key) override {
        if(key == ' ') {
            // use ofxZeroMQMultipartMessage
            ofxZeroMQMultipartMessage m;
            m.addArgument("space");
            m.addArgument(1);
            m.addArgument(ofColor::hotPink);
            m.addArgument(ofShortColor::hotPink);
            pub.send(m);
        } else if(key == OF_KEY_RETURN) {
            // use variadic arguments
            pub.sendMultipart("return", 1l, ofRectangle{10, 20, 30, 40}, glm::vec4{0.1f, 0.2f, 0.3f, 0.4f});
        }
    }
};

int main() {
    ofSetupOpenGL(400, 400, OF_WINDOW);
    ofRunApp(new ofApp());
}
